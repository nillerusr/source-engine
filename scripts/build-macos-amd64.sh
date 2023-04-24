#!/bin/sh

git submodule init && git submodule update

brew install sdl2

./waf configure -T debug --64bits --disable-warns $* &&
./waf build
