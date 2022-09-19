#include <stddef.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include "fat_driver/internal.h"
#include "errno.h"

struct blk_device {
	FILE *f;
};

int main_(char *blk_dev_path)
{
	struct fat_device *fd;
	struct blk_device *bd = malloc(sizeof(*bd));
	if (!bd)
		return 1;
	bd->f = fopen(blk_dev_path, "r+");
	if (!bd->f)
		return errno;
	PROPAGATE_ERRNO(fat_init(&fd, bd));
	printf("Initialized\n");
	fat_test(fd);
	fat_dispose(fd);
	return 0;
}

uint64_t blk_read(struct blk_device *dev, char *dst, uint64_t blk,
				  uint64_t count)
{
	if (fseek(dev->f, blk * blk_size(dev), SEEK_SET) == -1)
		return errno;
	if (fread(dst, blk_size(dev), count, dev->f) == 0) {
		if (feof(dev->f))
			return EOF;
		else
			return errno;
	}
	return 0;
}

// Seek to targeted block in file, read required amount of bytes
uint64_t blk_write(struct blk_device *dev, uint64_t blk, char *src,
				   size_t count)
{
	if (fseek(dev->f, blk * blk_size(dev), SEEK_SET) == -1)
		return errno;
	if (fwrite(src, blk_size(dev), count, dev->f) == 0) {
		if (feof(dev->f))
			return EOF;
		else
			return errno;
	}
	return 0;
}

// The artifical drive uses 512 byte blocks.
uint64_t blk_size(struct blk_device *_dev)
{
	return 512;
}

void blk_dispose(struct blk_device *self)
{
	if (self && self->f)
		fclose(self->f);
	free(self);
}

int main(void)
{
	uint32_t x = main_("/dev/loop0");
	return x;
}
