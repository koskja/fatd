#include "internal.h"
#include "cache.h"
#include <memory.h>
#include <endian.h>
#include <stdio.h>

#define CACHE_ENTRIES 1024 * 1024

uint8_t get_fat_type(struct fat_table *self);
uint32_t root_dir_start_sector(struct fat_table *self);
uint32_t root_dir_cluster(struct fat_device *self);
uint32_t fat_start(struct fat_table *self);

uint64_t fat_init(struct fat_device **self_dptr,
				  struct blk_device *dev)
{
	struct fat_device *self;

	// Allocate self and io buffer
	if (!MALLOC_ASSIGN(*self_dptr)) {
		return ENOMEM;
	}
	self = *self_dptr;

	PROPAGATE_ERRNO(cache_init(&self->cache, dev, CACHE_ENTRIES));

	char *bpb_buf;
	// Load BPB
	PROPAGATE_ERRNO(cache_get(&self->cache, 0, 1, &bpb_buf));

	memcpy(&self->table, bpb_buf, sizeof(struct fat_table));

	self->fat_type = get_fat_type(&self->table);
	if (self->fat_type == FATEX)
		return ENOTSUP;

	return 0;
}

uint64_t next_cluster_raw(struct fat_device *self, uint32_t *cluster)
{
	if (self->fat_type == FAT12)
		return EXIT_FAILURE;
	uint32_t fat_start_sector = fat_start(&self->table);

	uint32_t entry_width;
	if (self->fat_type == FAT16)
		entry_width = 2;
	else if (self->fat_type == FAT32 || self->fat_type == FATEX)
		entry_width = 4;
	else
		return EXIT_FAILURE;

	uint32_t offset_bytes = *cluster * entry_width;
	uint32_t target_sector =
		(offset_bytes / self->table.bytes_per_sector) +
		fat_start_sector;
	uint32_t sector_offset =
		offset_bytes % self->table.bytes_per_sector;
	char *fat_sector;
	PROPAGATE_ERRNO(
		cache_get(&self->cache, target_sector, 1, &fat_sector));

	uint32_t raw_next = 0;
	memcpy(&raw_next, fat_sector + sector_offset, entry_width);
	*cluster = le32toh(raw_next);
	return 0;
}

static uint8_t FAT_ENTRY_BITS_LOOKUP[4] = { 12, 16, 28, 32 };
uint64_t next_cluster(struct fat_device *self, uint32_t *cluster)
{
	uint32_t raw = *cluster;
	PROPAGATE_ERRNO(next_cluster_raw(self, &raw));
	uint32_t entry_bits = FAT_ENTRY_BITS_LOOKUP[self->fat_type];
	uint32_t end_cutoff = (1UL << entry_bits) - 0x8;

	if (raw == end_cutoff - 1)
		*cluster = 0; // bad cluster, doesn't happen on modern drives
	else if (raw >= end_cutoff)
		*cluster = 0; // end of cluster chain
	else
		*cluster = raw;
	return 0;
}
uint64_t set_next_cluster(struct fat_device *self, uint32_t cluster,
						  uint32_t next)
{
	// TODO
	return 0;
}

uint64_t read_data_cluster(struct fat_device *self, uint32_t cluster,
						   char **cluster_buf)
{
	return cache_get(&self->cache,
					 (cluster - 2) * self->table.sectors_per_cluster +
						 first_data_sector(&self->table),
					 self->table.sectors_per_cluster, cluster_buf);
}

uint64_t fat_treewalk(struct fat_device *self, uint64_t entry,
					  char **prefix)
{
	uint64_t start = entry;
	struct entry_node e;
	char **_p = prefix;
	do {
		PROPAGATE_ERRNO(next_node(self, &entry, &e));
		if(!entry)
			return 0;
		PROPAGATE_ERRNO(delete_file(self, entry, e._last));
		prefix = _p;
		do {
			printf("%s/", *prefix);
		} while (*(++prefix));
		printf("%s @ %u\n", e.name, e._last);
		if (e.attr & DIRECTORY &&
			(strcmp(e.name, ".") != 0 && strcmp(e.name, "..") != 0)) {
			*prefix = malloc(strlen(e.name));
			strcpy(*prefix, e.name);
			PROPAGATE_ERRNO(
				fat_treewalk(self, dir_entries(self, &e), _p));
			free(*prefix);
			*prefix = 0;
		}
		PROPAGATE_ERRNO(delete_file(self, start, e._last));
		prefix = _p;
	} while (1);
	return 0;
}

uint64_t fat_test(struct fat_device *self)
{
	printf("Root directory clusters: [ ");
	uint32_t cluster = root_dir_cluster(self);
	do {
		printf("%u ", cluster);
	} while (!next_cluster(self, &cluster) && cluster);
	printf("]\n");
	uint64_t entry =
		cluster_first_entry(self, root_dir_cluster(self));
	size_t c = 0;
	char **buf = calloc(8192, 1);
	PROPAGATE_ERRNO(fat_treewalk(self, entry, buf));
	/*PROPAGATE_ERRNO(cache_flush(&self->cache));
	while (!cache_evict_lru(&self->cache))
		;*/
	/*while (!next_node(self, &entry, &f) && entry && ++c) {
		printf("%s ", f.name);
		if(c % 10 == 0)
			printf("\n");
	}*/
	return 0;
}

uint32_t root_dir_cluster(struct fat_device *self)
{
	switch (self->fat_type) {
	case FAT12:
	case FAT16:
		return root_dir_start_sector(&self->table) /
			   self->table.sectors_per_cluster;

	case FAT32: {
		struct fat_ext32 *ext =
			((struct fat_ext32 *)(&((self->table).extended_section)));
		return ext->root_cluster;
	}

	case FATEX:
	default:
		exit(EXIT_FAILURE);
	}
}

uint32_t total_sectors(struct fat_table *self)
{
	return self->total_sectors_16 + self->total_sectors_32;
}

uint32_t root_directory_sectors(struct fat_table *self)
{
	uint32_t root_entries = self->root_entry_count;
	uint32_t bytes_per_sector = self->bytes_per_sector;
	// perform division with rounding up
	return (root_entries * 32U + bytes_per_sector - 1) /
		   bytes_per_sector;
}

uint32_t fat_start(struct fat_table *self)
{
	return self->reserved_sector_count;
}

uint32_t fat_table_size(struct fat_table *self)
{
	uint32_t type = get_fat_type(self);
	if (type == FAT32)
		return EXT_FAT32((*self))->table_size_32;
	else
		return self->table_size;
}

uint32_t root_dir_start_sector(struct fat_table *self)
{
	return fat_start(self) + self->table_count * fat_table_size(self);
}

uint32_t first_data_sector(struct fat_table *self)
{
	return root_dir_start_sector(self) + root_directory_sectors(self);
}

uint32_t data_sectors(struct fat_table *self)
{
	return total_sectors(self);
}

uint32_t data_clusters(struct fat_table *self)
{
	return data_sectors(self) / self->sectors_per_cluster;
}

uint8_t get_fat_type(struct fat_table *self)
{
	if (self->bytes_per_sector == 0)
		return FATEX;
	uint32_t clusters = data_clusters(self);
	if (clusters < 4085)
		return FAT12;
	else if (clusters < 65525)
		return FAT16;
	else
		return FAT32;
}

void fat_dispose(struct fat_device *self)
{
	if (self) {
		cache_dispose(&self->cache);
	}
	free(self);
}

uint64_t alloc_cluster(struct fat_device *self, uint32_t prev,
					   uint32_t next, uint32_t *out)
{
	uint64_t iter = search_start_hint(self) - 1;
	uint64_t c = 0;
	char *buf;
	do {
		c = ++iter;
		PROPAGATE_ERRNO(next_cluster(self, &c));
	} while (c != 0); // find unused cluster
	if (prev)
		PROPAGATE_ERRNO(set_next_cluster(self, prev, iter));
	if (next)
		PROPAGATE_ERRNO(set_next_cluster(self, iter, next));
	else
		PROPAGATE_ERRNO(set_next_cluster(self, iter, 0xFFFFFFF8));
	if (out)
		*out = iter;
	return 0;
}
uint64_t search_start_hint(struct fat_device *self)
{
	return 0;
}