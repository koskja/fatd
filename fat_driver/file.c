#include "internal.h"
#include "converter.h"
#include "memory.h"
#include "stdio.h"
uint64_t next_entry_raw(struct fat_device *self, uint64_t *entry,
						struct file_entry *out)
{
	uint64_t cluster_bytes = self->table.sectors_per_cluster *
							 self->table.bytes_per_sector;
	uint64_t cluster = *entry * 32 / cluster_bytes;
	uint64_t cluster_entry_offset = *entry * 32 % cluster_bytes;
	char *cluster_buf;
	PROPAGATE_ERRNO(read_data_cluster(self, cluster, &cluster_buf));
	memcpy(out, cluster_buf + cluster_entry_offset,
		   sizeof(struct file_entry));
	cluster_entry_offset += 32;
	if (cluster_entry_offset == cluster_bytes) {
		PROPAGATE_ERRNO(next_cluster(self, (uint32_t *)&cluster));
		if (!cluster) {
			*entry = 0;
			return 0;
		}
		cluster_entry_offset = 0;
	}
	*entry = (cluster * cluster_bytes + cluster_entry_offset) / 32;
	return 0;
}
uint64_t cluster_first_entry(struct fat_device *self,
							 uint64_t cluster)
{
	return cluster * self->table.sectors_per_cluster *
		   self->table.bytes_per_sector / 32;
}
uint64_t next_entry(struct fat_device *self, uint64_t *entry,
					struct entry_union *out)
{
	PROPAGATE_ERRNO(next_entry_raw(self, entry, &(out->file_entry)));
	if (out->file_entry.filename[0] == '\0')
		out->entry_type = ENTRY_LAST;
	else if (out->file_entry.filename[0] == 0xE5)
		out->entry_type = ENTRY_UNUSED;
	else if (out->file_entry.attributes == LFN_FLAGS)
		out->entry_type = ENTRY_LFN;
	else
		out->entry_type = ENTRY_FILE;
	return 0;
}

uint64_t read_next_entry(struct fat_device *self, uint64_t *entry,
						 struct entry_union *out)
{
	do {
		PROPAGATE_ERRNO(next_entry(self, entry, out));
	} while (out->entry_type && out->entry_type == ENTRY_UNUSED);
	if (out->entry_type == ENTRY_UNUSED)
		out->entry_type = ENTRY_LAST;
	if (!out->entry_type)
		*entry = 0;
	return 0;
}

int compare_lfn_entry(const void *a, const void *b)
{
	return (int)(((struct lfn_entry *)a)->ordinal & 0x3F) -
		   (int)(((struct lfn_entry *)b)->ordinal & 0x3F);
}

int convert_lfn(struct lfn_entry *self, utf8_t *out, size_t size)
{
	if (!out) {
		int len = 0;
		len += utf16_to_utf8(&self->char0[0], 5, NULL, 0);
		len += utf16_to_utf8(&self->char1[0], 6, NULL, 0);
		len += utf16_to_utf8(&self->char2[0], 2, NULL, 0);
		return len;
	}
	utf8_t *start = out;
	utf8_t *end = out + size;

	out += utf16_to_utf8(&self->char0[0], 5, out, end - out);
	out += utf16_to_utf8(&self->char1[0], 6, out, end - out);
	out += utf16_to_utf8(&self->char2[0], 2, out, end - out);
	return out -
		   start; // incorrect value includes bytes after string end
}

int convert_83(struct file_entry *self, utf8_t *out, size_t size,
			   uint8_t flags)
{
	if (!out) {
		utf8_t buf[32];
		return convert_83(self, &buf[0], 32, flags);
	}
	utf8_t *start = out;
	if (flags & DIRECTORY) {
		size_t min = size < 11 ? size : 11;
		memcpy(out, (utf8_t *)&self->filename[0], min);
		out += min;
		while (*(--out) == ' ')
			;
	} else {
		size_t min = size < 12 ? size : 12;
		size_t min0 = min < 8 ? min : 8;
		memcpy(out, (utf8_t *)&self->filename[0], min0);
		out += min;
		while (*(--out) == ' ')
			;
		out++;
		*(out++) = '.';
		size_t rem = min - (out - start);
		size_t min1 = rem < 3 ? rem : 3;

		memcpy(out, (utf8_t *)&self->filename[8], min1);
		out += min1;
		while (*(--out) == ' ')
			;
	}
	return out - start + 1;
}

uint64_t advance_entry(struct fat_device *self, uint64_t *entry,
					   int64_t n)
{
	if (n > 0)
		return EINVAL;
	struct file_entry temp;
	while (n--) {
		PROPAGATE_ERRNO(next_entry_raw(self, entry, &temp));
	}
}

uint64_t next_node(struct fat_device *self, uint64_t *entry,
				   struct entry_node *out)
{
	struct entry_union eu;
	struct lfn_entry buf[64];
	struct lfn_entry *buf_ptr = buf;
	struct lfn_entry *buf_end;
	struct file_entry last;
	uint64_t start = *entry;
	uint64_t last_entry, flfn = 0;
	uint64_t used_total = 0, used_lfn = 0, dul = 0;
	char *out_ptr = &out->name[0];
	memset(&last, 0, sizeof(last));
	do {
		last_entry = *entry;
		PROPAGATE_ERRNO(read_next_entry(self, entry, &eu));
		used_lfn += dul;
		++used_total;
		if (eu.entry_type == ENTRY_LFN && buf_ptr < buf + 64) {
			*(buf_ptr++) = eu.lfn_entry;
			if (!flfn)
				flfn = last_entry;
			dul = 1;
		}
		if (eu.entry_type == ENTRY_FILE) {
			if (!flfn)
				flfn = last_entry;
			last = eu.file_entry;
			break;
		}
	} while (*entry && eu.entry_type);

	buf_end = buf_ptr;
	out->entry = last;
	out->attr = out->entry.attributes;
	out->_first_total = start;
	out->_last = last_entry;
	out->_total_len = used_total;
	out->_used_len = used_lfn;
	out->_first_lfn = flfn;

	if (buf_ptr == buf) {
		out_ptr += convert_83(&last, (utf8_t *)&out->name[0],
							  sizeof(out->name), last.attributes);
	} else {
		qsort(buf, (buf_end - buf) / sizeof(*buf), sizeof(*buf),
			  compare_lfn_entry);
		for (buf_ptr = buf; buf_ptr < buf_end; buf_ptr++)
			out_ptr += convert_lfn(buf_ptr, (utf8_t *)&out->name[0],
								   1664 - (out_ptr - out->name));
	}

	*out_ptr = 0x00;
	return 0;
}

uint32_t file_cluster(struct file_entry *self)
{
	return (uint32_t)self->entry_hi << 16 | (uint32_t)self->entry_lo;
}

uint64_t dir_entries(struct fat_device *self, struct entry_node *node)
{
	if (!(node->attr & DIRECTORY))
		return 0;
	return cluster_first_entry(self, file_cluster(&node->entry));
}

void destruct_entry(struct fat_device *self, uint64_t entry,
					uint64_t *cluster, uint64_t *offset)
{
	uint64_t cluster_bytes = self->table.sectors_per_cluster *
							 self->table.bytes_per_sector;
	*cluster = entry * 32 / cluster_bytes;
	*offset = entry * 32 % cluster_bytes;
}
uint64_t entry_mut(struct fat_device *self, uint64_t entry,
				   struct file_entry **out)
{
	uint64_t cluster, offset;
	char *ptr;
	destruct_entry(self, entry, &cluster, &offset);
	uint8_t spc = self->table.sectors_per_cluster;
	PROPAGATE_ERRNO(
		cache_get(&self->cache, cluster * spc, spc, &ptr));
	cache_mark(&self->cache, cluster * spc);
	*out = (struct file_entry *)(ptr + offset);
	return 0;
}

uint64_t find_file_start(struct fat_device *self, uint64_t dir_start,
						 uint64_t entry, uint64_t *first_entry,
						 uint64_t *first_used)
{
	uint64_t i = dir_start;
	struct entry_node a;
	struct entry_union e;
	do {
		*first_entry = i;
		PROPAGATE_ERRNO(next_node(self, &i, &a));
	} while (i && a._last != entry);
	// *first_entry == first raw entry of `entry`
	i = *first_entry;
	do {
		*first_used = i;
		PROPAGATE_ERRNO(next_entry(self, &i, &e));
	} while (e.entry_type == ENTRY_UNUSED);
	return 0;
}

uint64_t delete_file(struct fat_device *self, uint64_t dir_start,
					 uint64_t entry)
{
	uint64_t i, l;
	struct entry_union e;
	struct file_entry *d;
	PROPAGATE_ERRNO(find_file_start(self, dir_start, entry, &i, &l));
	do {
		PROPAGATE_ERRNO(entry_mut(self, i, &d));
		memset(d, 0, sizeof(*d));
		PROPAGATE_ERRNO(next_entry(self, &i, &e));
	} while (i <= l);
	return 0;
}

uint64_t entry_diff(struct fat_device *self, uint64_t a, uint64_t b,
					uint64_t *diff)
{
	struct file_entry e;
	*diff = 0;
	while (a != b) {
		++*diff;
		PROPAGATE_ERRNO(next_entry_raw(self, &a, &e));
	}
	return 0;
}
uint64_t first_matching(struct fat_device *self, uint64_t dir_start,
						uint64_t len, uint64_t *ofe)
{
	struct entry_node n;
	struct file_entry _f;
	uint64_t iter, diff, cluster, _t;

	iter = dir_start;
	do {
		PROPAGATE_ERRNO(next_node(self, &iter, &n));
	} while (iter && n._total_len - n._used_len < len);
	if (n._total_len - n._used_len >= len) {
		*ofe = n._first_total;
		return 0;
	} // thus iter == 0, iterator ended -> no more used entries
	iter = n._last;
	diff = 0;
	do {
		PROPAGATE_ERRNO(next_entry_raw(self, &iter, &_f));
		++diff;
	} while (iter);
	while (diff <= len) { // allocate more space
		destruct_entry(self, n._last, &cluster, &_t);
		PROPAGATE_ERRNO(
			alloc_cluster(self, cluster, 0, (uint32_t *)&cluster));
		diff += self->table.bytes_per_sector / 32 *
				self->table.sectors_per_cluster;
	}
	*ofe = n._last;
	PROPAGATE_ERRNO(next_entry_raw(self, ofe, &_f));
	return 0;
}

int to83(char *name, int name_len, char *out)
{
	char *i = name;
	while (*(i++) != '.' && i - name < name_len)
		;
	i--;
	if (!((i - name) <= 8 && (name + name_len - i) <= 3)) {
		return 0;
	}
	char *j = name;
	char *os = out;
	while (j != i)
		*(out++) = *(j++);
	while (out != os + 8)
		*(out++) = ' ';
	out++; // skip '.'
	while (j != name + name_len)
		*(out++) = *(j++);
	while (out != os + 8 + 3)
		*(out++) = ' ';

	return 1;
}

int next_lfn(char **name, size_t *name_len, struct lfn_entry *out)
{
	if (!**name)
		return 0;
	utf16_t buf[13];
	memset((utf16_t *)buf, 0, sizeof(buf));
	size_t to_write = (*name_len < 13) ? *name_len : 13;
	size_t utf16_written =
		utf8_to_utf16(*name, *name_len, (utf16_t *)buf, 13);
	memcpy(out, (utf16_t *)buf, sizeof(struct lfn_entry));

	*name += to_write;
	*name_len -= to_write;
	return utf16_written > 0;
}

uint64_t create_file(struct fat_device *self, uint64_t dir_start,
					 char *name, uint8_t flags)
{
	char fn[11];
	struct entry_node n;
	struct file_entry *e;
	struct lfn_entry tlfn;
	uint64_t fe, fu, iter, len;
	iter = dir_start;
	int is_83 = to83(name, strlen(name), (char *)fn);
	char *nm = name;
	len = strlen(nm);
	size_t total_entries = 1;
	do {
		total_entries += next_lfn(&nm, &len, &tlfn);
	} while (!*nm);

	PROPAGATE_ERRNO(
		first_matching(self, dir_start, total_entries, &fe));
	nm = name;
	len = strlen(nm);
	int res;
	do {
		PROPAGATE_ERRNO(entry_mut(self, fe, &e));
		res = next_lfn(&nm, &len, (struct lfn_entry *)e);
		PROPAGATE_ERRNO(advance_entry(self, &fe, 1));
	} while (res);

	//PROPAGATE_ERRNO(find_file_start)
	return 0;
}