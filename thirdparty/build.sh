#!/bin/sh

INSTALLDIR=../../lib/public/linux32

clean() {
cd $1
echo cleaning $1
make clean &> /dev/null || echo clean failed for $1
[ -f confdone ] && rm confdone
cd ../
}

conf() {
[ -f confdone ] || ( "$@" && touch confdone )
}

mk() {
make "$@" -j$(nproc --all)
}

inst() {
cp $1 ../../lib/public/linux32
}

mkdir -p ../lib/public/linux32
mkdir -p ../lib/common/ubuntu12_32/
mkdir -p ../lib/common/linux32

if [ "$1" = "clean" ]
then
#	clean gperftools-2.0
	clean SDL2-src
	clean protobuf-2.6.1/
	clean StubSteamAPI/
#	clean openssl
#	clean cryptopp
	clean libjpeg
	clean libpng
	clean zlib
	clean libedit-3.1
	exit
fi

#cd gperftools-2.0
#conf ./configure CFLAGS="-m32 -Wno-narrowing" "CXXFLAGS=-m32 -Wno-narrowing -fpermissive" "LDFLAGS=-m32"
#mk
#inst .libs/libtcmalloc_minimal.so.4
#cd ../

cd SDL2-src
conf ./configure --build=i686-pc-linux-gnu "CFLAGS=-m32 -Wno-narrowing" "CXXFLAGS=-m32 -Wno-narrowing -fpermissive" "LDFLAGS=-m32" --enable-input-tslib=no
mk
inst build/.libs/libSDL2.so
cd ../

cd protobuf-2.6.1
conf ./configure --build=i686-pc-linux-gnu "CFLAGS=-m32 -Wno-narrowing" "CXXFLAGS=-m32 -Wno-narrowing -fpermissive" "LDFLAGS=-m32"
mk
cd ../

cd StubSteamAPI/
mk
inst libsteam_api.so 
cd ../

#cd openssl
#conf ./Configure -m32 linux-generic32
#mk
#cp libcrypto.a ../../lib/common/ubuntu12_32/
#cd ../

#cd cryptopp
#mk IS_X86=1 IS_X64=0 CC='gcc -m32 -msse4 -fPIC' CXX='g++ -m32 -msse4 -D_GLIBCXX_USE_CXX11_ABI=0'
#cp libcryptopp.a ../../lib/common/ubuntu12_32/
#cd ../

cd libjpeg
conf ./configure --build=i686-pc-linux-gnu "CFLAGS=-m32 -Wno-narrowing" "CXXFLAGS=-m32 -Wno-narrowing -fpermissive" "LDFLAGS=-m32"
mk
cp .libs/libjpeg.a ../../lib/common/linux32

inst .libs/libjpeg.a
cd ../

cd libpng
conf ./configure --build=i686-pc-linux-gnu "CFLAGS=-m32 -Wno-narrowing" "CXXFLAGS=-m32 -Wno-narrowing -fpermissive" "LDFLAGS=-m32"
mk
cp .libs/libpng16.a ../../lib/public/linux32/libpng.a
cd ../

cd zlib
CFLAGS="-m32" LDFLAGS="-m32" conf ./configure
#conf ./configure --build=i686-pc-linux-gnu "CFLAGS=-m32 -Wno-narrowing" "CXXFLAGS=-m32 -Wno-narrowing -fpermissive" "LDFLAGS=-m32"
mk
inst libz.a
cd ../


cd libedit-3.1
conf ./configure --build=i686-pc-linux-gnu "CFLAGS=-m32 -Wno-narrowing" "CXXFLAGS=-m32 -Wno-narrowing -fpermissive" "LDFLAGS=-m32"
mk
cd ../
