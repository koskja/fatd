#ifndef FATD_H
#define FATD_H

#include "fat.h"
#include "util.h"
#include "tables.h"
#include "file.h"
#include <errno.h>
#include <stdlib.h>

#define FAT12 0
#define FAT16 1
#define FAT32 2
#define FATEX 3

#define EXT_FAT16(variable) \
	((struct fat_ext16 *)(&(variable.extended_section)))
#define EXT_FAT32(variable) \
	((struct fat_ext32 *)(&(variable.extended_section)))

uint64_t next_cluster_raw(struct fat_device *self, uint32_t *cluster);
uint64_t next_cluster(struct fat_device *self, uint32_t *cluster);

uint64_t read_data_cluster(struct fat_device *self, uint32_t cluster, char **cluster_buf);

#endif
