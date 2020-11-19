#!/bin/sh

# example: ./build.sh everything togl

export VALVE_NO_AUTO_P4=1

make MAKE_VERBOSE=1 NO_CHROOT=1 -f $1.mak $2 -j$(nproc --all)
