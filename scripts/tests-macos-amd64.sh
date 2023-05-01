#!/bin/sh

git submodule init && git submodule update
./waf configure -T release --sanitize=address,undefined --disable-warns --tests -8 --prefix=out/ $* &&
./waf install &&
cd out &&
./bin/unittest || exit 1
