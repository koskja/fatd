#ifndef FAT_CACHE_H
#define FAT_CACHE_H
#include "fat.h"
#include <errno.h>
#include <string.h>
#define __ATTR_PUBLIC __attribute__((visibility("default")))

struct entry {
	uint64_t blk : 48;
	uint8_t dirty : 1;
	uint16_t count : 15;
	struct entry *next;
	struct entry *prev;
	char *val;
};

struct blk_cache {
	struct blk_device *dev;
	struct entry *entries;
	size_t entry_count;
	struct entry *mru;
	struct entry *lru;
};

uint64_t cache_init(struct blk_cache *self, struct blk_device *dev,
					size_t entry_count) __ATTR_PUBLIC;
uint64_t entry_flush(struct blk_device *dev, struct entry *p);
uint64_t cache_evict(struct blk_cache *self, struct entry *p);
uint64_t cache_evict_lru(struct blk_cache *self);
uint64_t cache_insert(struct blk_cache *self, uint64_t blk,
					  uint64_t blk_count, char *val);
void cache_use_entry(struct blk_cache *self, struct entry *p);
uint64_t cache_resize(struct blk_cache *self, size_t entry_count) __ATTR_PUBLIC;
void cache_mark(struct blk_cache *self, uint64_t blk) __ATTR_PUBLIC;
uint64_t cache_get(struct blk_cache *self, uint64_t blk,
				   uint64_t count, char **ptr) __ATTR_PUBLIC;
uint64_t cache_flush(struct blk_cache *self)  __ATTR_PUBLIC;
uint64_t cache_dispose(struct blk_cache *self)  __ATTR_PUBLIC;
#undef __ATTR_PUBLIC
#endif
