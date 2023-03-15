#!/bin/sh

tail -c +4097 $1 > /run/initramfs/image-$2
cd /run/initramfs
./update
exit $?

