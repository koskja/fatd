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
	char* val;
};

struct blk_cache {
	struct blk_device *dev;
	struct entry *entries;
	size_t entry_count;
	struct entry *head;
	struct entry *tail;
};

uint64_t cache_init(struct blk_cache *self, struct blk_device *dev,
					size_t entry_count)
{
	memset(self, 0, sizeof(*self));
	self->dev = dev;
	self->entry_count = entry_count;
	if (!MALLOC_ASSIGN_SIZE(self->entries,
							entry_count * sizeof(*self->entries)))
		return ENOMEM;
	return 0;
}

uint64_t entry_flush(struct blk_device *dev, struct entry *p)
{
	if (!p->dirty)
		return 0;
	PROPAGATE_ERRNO(blk_write(dev, p->blk, p->val, p->count));
	p->dirty = 0;
	return 0;
}

uint64_t cache_evict(struct blk_cache *self, struct entry *p)
{
	PROPAGATE_ERRNO(entry_flush(self->dev, p));
	free(p->val);

	if(!p->next) 
		self->tail = p->prev;
	else
		p->next->prev = p->prev;
	if(!p->prev)
		self->head = p->next;
	else
		p->prev->next= p->next;
	
	memset(p, 0, sizeof(*p));
	return 0;
}

uint64_t cache_evict_lru(struct blk_cache *self) {
	if(!self->head)
		return ENOMEM;
	return cache_evict(self, self->head);
}

uint64_t cache_insert(struct blk_cache *self, uint64_t blk,
					  uint64_t blk_count, char *val)
{
	size_t index = blk % self->entry_count;
	struct entry *e = self->entries + index;
	if(e->val)
		PROPAGATE_ERRNO(cache_evict(self, e));

	e->blk = blk;
	e->count = blk_count;
	e->dirty = 0;
	e->next = NULL;
	e->prev = self->tail;

	while(!MALLOC_ASSIGN_SIZE(e->val, blk_count * blk_size(self->dev))) {
		PROPAGATE_ERRNO(cache_evict_lru(self));
	}
	PROPAGATE_ERRNO(blk_read(self->dev, e->val, blk, blk_count));

	self->tail = e;
	return 0;
}

uint64_t cache_resize(struct blk_cache *self, size_t entry_count
					  )
{
	struct blk_cache new;
	PROPAGATE_ERRNO(
		cache_init(&new, self->dev, entry_count));

	*self = new;
	return 0;
}

uint64_t cache_mark(struct blk_device *dev, uint64_t blk,
					uint64_t count) __ATTR_PUBLIC;

uint64_t cache_get(struct blk_cache *self, uint64_t blk,
				   uint64_t count, char **ptr) __ATTR_PUBLIC
{
	return 0;
}
void blk_dispose(struct blk_device *dev) __ATTR_PUBLIC;
#undef __ATTR_PUBLIC
#endif
