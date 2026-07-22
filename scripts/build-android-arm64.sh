#!/bin/sh

git submodule init && git submodule update
if [ ! -d "android-ndk-r10e" ]; then
    wget https://dl.google.com/android/repository/android-ndk-r10e-linux-x86_64.zip -o /dev/null
    unzip android-ndk-r10e-linux-x86_64.zip
fi
export ANDROID_NDK_HOME=$PWD/android-ndk-r10e/
export NDK_HOME=$PWD/android-ndk-r10e/
LINKFLAGS="-Wl,--strip-all" CXXFLAGS="-fexceptions" CFLAGS="-fexceptions" ./waf configure -T release --android=aarch64,4.9,21 --togles --disable-warns && ./waf build
