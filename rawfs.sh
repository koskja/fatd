#!/bin/sh
umount /dev/loop0 
losetup -d /dev/loop0 
umount ./tmpfs

# create a 10GB ram resident tmpfs
mount -t tmpfs -o size=10g tmpfs ./tmpfs && \
# create a raw disk image
dd if=/dev/zero of=./tmpfs/floppy.img bs=512 count=10000000 
