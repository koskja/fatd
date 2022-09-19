#ifndef FAT_TABLES_H
#define FAT_TABLES_H

#include "fat.h"

// A FAT partition consists of the following parts:
// Sector 0: BIOS Parameter Block (BPB)

// ReservedRegion      =	1			   		.. reserved_sector_count
// 							not included in the fat system; `reserved_sector_count` includes the BPB

// FATRegion           =	ReservedRegion      .. ReservedRegion + (table_count * table_size)
//							`table_count` many File Allocation Tables, each `table_size` big

// RootDirectoryRegion =	FATRegion           .. FATRegion + (root_entry_count * 32) / bytes_per_cluster
// 							`root_entry_count` many root entries, each 32 bytes long

// DataRegion          =	RootDirectoryRegion ...
//							data clusters managed by the File Allocation Table(s)

struct fat_table {
	uint8_t bootjmp[3];
	uint8_t oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t table_count;
	uint16_t root_entry_count;
	uint16_t total_sectors_16;
	uint8_t media_type;
	uint16_t table_size;
	uint16_t sectors_per_track;
	uint16_t head_side_count;
	uint32_t hidden_sector_count;
	uint32_t total_sectors_32;

	uint8_t extended_section[54];
} __attribute__((packed));

struct fat_ext16 {
	uint8_t bios_drive_num;
	uint8_t reserved1;
	uint8_t boot_signature;
	uint32_t volume_id;
	uint8_t volume_label[11];
	uint8_t fat_type_label[8];
} __attribute__((packed));

struct fat_ext32 {
	uint32_t table_size_32;
	uint16_t extended_flags;
	uint16_t fat_version;
	uint32_t root_cluster;
	uint16_t fat_info;
	uint16_t backup_BS_sector;
	uint8_t reserved_0[12];
	uint8_t drive_number;
	uint8_t reserved_1;
	uint8_t boot_signature;
	uint32_t volume_id;
	uint8_t volume_label[11];
	uint8_t fat_type_label[8];
} __attribute__((packed));

struct fat_device {
	struct fat_table table; // stored main table
	struct blk_device *device; // handle to the underlying device
	uint8_t fat_type; // FAT12, FAT16, ...
	char *blk_buf; // buffer for clusters
	char *fat_buf; // buffer for FAT entries
};

uint8_t get_fat_type(struct fat_table *self);
uint32_t root_dir_start_sector(struct fat_table *self);
uint32_t root_dir_cluster(struct fat_device *self);
uint32_t fat_start(struct fat_table *self);
uint32_t total_sectors(struct fat_table *self);
uint32_t root_directory_sectors(struct fat_table *self);
uint32_t fat_start(struct fat_table *self);
uint32_t root_dir_start_sector(struct fat_table *self);
uint32_t first_data_sector(struct fat_table *self);
uint32_t data_sectors(struct fat_table *self);
uint32_t data_clusters(struct fat_table *self);

#endif
