#!/bin/sh

# example: ./build.sh everything togl

make NO_CHROOT=1 -f $1.mak $2 -j$(nproc --all)
