#include "internal.h"
#include "memory.h"
uint64_t next_entry_raw(struct fat_device *self, uint64_t *entry,
						struct file_entry *out)
{
	uint64_t cluster_bytes = self->table.sectors_per_cluster *
							 self->table.bytes_per_sector;
	uint64_t cluster = *entry * 32 / cluster_bytes;
	uint64_t cluster_entry_offset = *entry * 32 % cluster_bytes;
	PROPAGATE_ERRNO(read_data_cluster(self, cluster));
	memcpy(out, self->blk_buf + cluster_entry_offset,
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
	PROPAGATE_ERRNO(
		next_entry_raw(self, entry, &(out->file_entry))
	);
	if(out->file_entry.filename[0] == '\0')
		out->entry_type = ENTRY_LAST;
	else if(out->file_entry.filename[0] == 0xE5)
		out->entry_type = ENTRY_UNUSED;
	else if(out->file_entry.attributes == LFN_FLAGS)
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
	} while(out->entry_type && out->entry_type == ENTRY_UNUSED);
	if(out->entry_type == ENTRY_UNUSED)
		out->entry_type = ENTRY_LAST;
	return 0;
}
