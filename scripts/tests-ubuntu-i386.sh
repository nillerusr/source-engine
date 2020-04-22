install#!/bin/sh

git submodule init && git submodule update
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y g++-multilib gcc-multilib libbz2-dev:i386

PKG_CONFIG_PATH=/usr/lib/i386-linux-gnu/pkgconfig ./waf configure -T release --32bits --sanitize=address,undefined --disable-warns --tests --prefix=out/ $* &&
./waf install &&
cd out &&
LD_LIBRARY_PATH=bin/ ./unittest
