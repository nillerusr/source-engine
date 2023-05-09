#!/bin/sh

git submodule init && git submodule update
./waf configure -T release --sanitize=address,undefined --no-warns -lt -8 --prefix=out/ $* &&
./waf install &&
cd out &&
DYLD_LIBRARY_PATH=bin/ ./unittest || exit 1
