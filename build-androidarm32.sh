#!/bin/sh

# example: ./build.sh everything togl

export VALVE_NO_AUTO_P4=1
export NDK_TOOLCHAIN_VERSION=4.9

make NDK=1 NDK_ABI=armeabi-v7a NDK_PATH=/mnt/f/soft/android-ndk-r10e APP_API_LEVEL=21 MAKE_VERBOSE=1 NO_CHROOT=1 -f $1.mak $2 -j$(nproc --all)
