#!/bin/sh

git submodule init && git submodule update
sudo apt-get update
sudo apt-get install -f -y gdb libopenal-dev g++-multilib gcc-multilib libpng-dev libjpeg-dev libfreetype6-dev libfontconfig1-dev libcurl4-gnutls-dev libsdl2-dev zlib1g-dev libbz2-dev libedit-dev

./waf configure -T release --disable-warns --tests --prefix=out/ --64bits $* &&
./waf install &&
cd out &&
LD_LIBRARY_PATH=. ./unittest
