#!/bin/sh

# example: ./build.sh everything togl

make MAKE_VERBOSE=1 NO_CHROOT=1 -f $1.mak $2 -j$(nproc --all)
