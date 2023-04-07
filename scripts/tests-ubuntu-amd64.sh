#!/bin/sh

git submodule init && git submodule update
sudo apt-get update
sudo apt-get install -y libbz2-dev

./waf configure -T release --sanitize=address,undefined --disable-warns --tests --prefix=out/ --64bits $* &&
./waf install &&
cd out &&
LD_LIBRARY_PATH=bin/ ./unittest
