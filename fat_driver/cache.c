#include "cache.h"
#include "internal.h"
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
	if (!p->val)
		return 0;
	PROPAGATE_ERRNO(entry_flush(self->dev, p));
	free(p->val);

	if (!p->next)
		self->lru = p->prev;
	else
		p->next->prev = p->prev;
	if (!p->prev)
		self->mru = p->next;
	else
		p->prev->next = p->next;

	memset(p, 0, sizeof(*p));
	return 0;
}

uint64_t cache_evict_lru(struct blk_cache *self)
{
	if (!self->lru)
		return ENOMEM;
	return cache_evict(self, self->lru);
}

uint64_t cache_insert(struct blk_cache *self, uint64_t blk,
					  uint64_t blk_count, char *val)
{
	size_t index = blk % self->entry_count;
	struct entry *e = self->entries + index;
	if (e->val)
		PROPAGATE_ERRNO(cache_evict(self, e));

	e->blk = blk;
	e->count = blk_count;
	e->dirty = 0;
	e->next = self->mru;
	e->prev = NULL;
	e->val = val;

	self->mru = e;
	return 0;
}
/*while (!MALLOC_ASSIGN_SIZE(e->val,
							   blk_count * blk_size(self->dev))) {
	}
	PROPAGATE_ERRNO(blk_read(self->dev, e->val, blk, blk_count));*/

void cache_use_entry(struct blk_cache *self, struct entry *p)
{
	if (!p->prev)
		return;
	p->prev->next = p->next;
	if (!p->next)
		self->lru = p->prev;
	else
		p->next->prev = p->prev;
	p->prev = 0;
	p->next = self->mru;
	self->mru = p;
}

uint64_t cache_resize(struct blk_cache *self, size_t entry_count)
{
	struct blk_cache new;
	PROPAGATE_ERRNO(cache_init(&new, self->dev, entry_count));
	struct entry *p = self->lru;
	if (!p)
		goto end;
	do {
		cache_insert(&new, p->blk, p->count, p->val);
	} while ((p = p->prev));

end:
	*self = new;
	return 0;
}

void cache_mark(struct blk_cache *self, uint64_t blk)
{
	self->entries[blk % self->entry_count].dirty = 1;
	return;
}

uint64_t cache_get(struct blk_cache *self, uint64_t blk,
				   uint64_t count, char **ptr)
{
	struct entry *e = self->entries + blk % self->entry_count;
	if (!e->val || e->blk != blk) {
		PROPAGATE_ERRNO(cache_evict(self, e));
		char *buf;
		while (
			!MALLOC_ASSIGN_SIZE(buf, count * blk_size(self->dev))) {
			PROPAGATE_ERRNO(cache_evict_lru(self));
		}
		PROPAGATE_ERRNO(cache_insert(self, blk, count, buf));
        PROPAGATE_ERRNO(blk_read(self->dev, buf, blk, count));
		*ptr = buf;
	} else {
		if (e->count < count) {
			PROPAGATE_ERRNO(entry_flush(self->dev, e));
			while (!(e->val = realloc(e->val,
									  count * blk_size(self->dev)))) {
				PROPAGATE_ERRNO(cache_evict_lru(self));
			}
			e->count = count;
			PROPAGATE_ERRNO(blk_read(self->dev, e->val, blk, count));
			cache_use_entry(self, e);
		}
		*ptr = e->val;
	}
	return 0;
}
uint64_t cache_flush(struct blk_cache *self)
{
	for (size_t i = 0; i < self->entry_count; ++i)
		PROPAGATE_ERRNO(entry_flush(self->dev, self->entries + i));
	return 0;
}
uint64_t cache_dispose(struct blk_cache *self)
{
	if (!self)
		return 0;
	PROPAGATE_ERRNO(cache_flush(self));
    blk_dispose(self->dev);
	free(self->entries);
	return 0;
}
