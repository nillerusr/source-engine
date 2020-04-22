#!/bin/sh

git submodule init && git submodule update

brew install sdl2

./waf configure -T debug --disable-warns $* &&
./waf build
