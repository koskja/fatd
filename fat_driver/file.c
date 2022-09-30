#include "internal.h"
#include "converter.h"
#include "memory.h"
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

uint64_t next_node(struct fat_device *self, uint64_t *entry,
				   struct entry_node *out)
{
	struct entry_union eu;
	struct lfn_entry buf[64];
	struct lfn_entry *buf_ptr = buf;
	struct lfn_entry *buf_end;
	struct file_entry last;
	char *out_ptr = &out->name[0];
	memset(&last, 0, sizeof(last));
	do {
		PROPAGATE_ERRNO(read_next_entry(self, entry, &eu));
		if (eu.entry_type == ENTRY_LFN && buf_ptr < buf + 64)
			*(buf_ptr++) = eu.lfn_entry;
		if (eu.entry_type == ENTRY_FILE) {
			last = eu.file_entry;
			break;
		}
	} while (*entry && eu.entry_type);

	buf_end = buf_ptr;
	out->entry = last;
	out->attr = out->entry.attributes;

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
