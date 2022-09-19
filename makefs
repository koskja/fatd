#!/bin/sh
umount /dev/loop0 
losetup -d /dev/loop0 
umount ./tmpfs

# create a 10GB ram resident tmpfs
mount -t tmpfs -o size=10g tmpfs ./tmpfs && \
# create a raw disk image
dd if=/dev/zero of=./tmpfs/floppy.img bs=512 count=10000000 && \
# create a loopback block device to the disk image
losetup /dev/loop0 ./tmpfs/floppy.img && \
# format it as FAT
mkfs.fat /dev/loop0 && \
# mount it to `fs`
mount /dev/loop0 ./fs
