#!/bin/sh
# Native build on Linux riscv64 (RV64GC), e.g. Debian 13 (trixie).

git submodule init && git submodule update
sudo apt-get update
sudo apt-get install -y g++ python-is-python3 pkg-config libsimde-dev libopenal-dev libpng-dev libjpeg-dev libfreetype-dev libfontconfig-dev libcurl4-gnutls-dev libsdl2-dev zlib1g-dev libbz2-dev libedit-dev libgl-dev

./waf configure -T debug --disable-warns $* &&
./waf build
