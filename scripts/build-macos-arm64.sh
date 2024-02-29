#!/bin/sh

git submodule init && git submodule update

brew install sdl2 freetype2 fontconfig pkg-config opus libpng libedit

python3 ./waf configure -T debug --disable-warns $* &&
./waf build
