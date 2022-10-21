#!/bin/sh
umount /dev/loop0 
losetup -d /dev/loop0 
umount ./tmpfs
