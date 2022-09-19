#ifndef FATD_INTERFACE_H
#define FATD_INTERFACE_H

#include <stddef.h>
#include <stdint.h>

#define __ATTR_PUBLIC __attribute__((visibility("default")))

struct blk_device;
struct fat_device;

uint64_t blk_read(struct blk_device *dev, char *dst, uint64_t blk,
				  uint64_t count) __ATTR_PUBLIC;
uint64_t blk_write(struct blk_device *dev, uint64_t blk, char *src,
				  uint64_t count) __ATTR_PUBLIC;
uint64_t blk_size(struct blk_device *dev) __ATTR_PUBLIC;
void blk_dispose(struct blk_device *dev) __ATTR_PUBLIC;

uint64_t fat_init(struct fat_device **self,
				  struct blk_device *dev) __ATTR_PUBLIC;
void fat_test(struct fat_device *self) __ATTR_PUBLIC;
void fat_dispose(struct fat_device *self) __ATTR_PUBLIC;

#undef __ATTR_PUBLIC

#endif
