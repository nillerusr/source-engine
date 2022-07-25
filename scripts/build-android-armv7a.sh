#!/bin/sh

git submodule init && git submodule update
wget https://dl.google.com/android/repository/android-ndk-r10e-linux-x86_64.zip
unzip android-ndk-r10e-linux-x86_64.zip
export ANDROID_NDK=$PWD/android-ndk-r10e/
./waf configure -T debug --android=armeabi-v7a-hard,4.9,21 --disable-warns &&
./waf build
