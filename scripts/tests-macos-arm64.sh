#!/bin/sh

git submodule init && git submodule update
python3 ./waf configure -T release --sanitize=address,undefined --disable-warns --tests --prefix=out/ $* &&
python3 ./waf install &&
cd out &&
DYLD_LIBRARY_PATH=bin/ ./unittest || exit 1
