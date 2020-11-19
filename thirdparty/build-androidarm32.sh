#!/bin/sh

INSTALLDIR=../../lib/public/androidarm32

export ANDROID_HOST=arm-linux-androideabi
export ANDROID_BUILD=linux-x86_64
export ANDROID_ARCH=arm
export ANDROID_NDK=/mnt/f/soft/android-ndk-r10e
export ANDROID_VERSION=21
export ANDROID_TOOLCHAIN_VERSION=4.9
export ANDROID_SYSROOT=$ANDROID_NDK/platforms/android-$ANDROID_VERSION/arch-$ANDROID_ARCH
export CFLAGS=--sysroot=$ANDROID_SYSROOT
export CPPFLAGS=--sysroot=$ANDROID_SYSROOT
export AR=$ANDROID_HOST-ar
export RANLIB=$ANDROID_HOST-ranlib
export PATH=$ANDROID_NDK/toolchains/$ANDROID_HOST-$ANDROID_TOOLCHAIN_VERSION/prebuilt/$ANDROID_BUILD/bin:$PATH

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
make "$@" -j$(nproc --all) NDK=1 NDK_ABI=armeabi-v7a NDK_PATH=$ANDROID_NDK
}

inst() {
cp $1 ../../lib/public/androidarm32
}

mkdir -p ../lib/public/androidarm32
mkdir -p ../lib/common/androidarm32/
mkdir -p ../lib/common/androidarm32

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

#cd SDL2-src
#conf ./configure --host=$ANDROID_HOST --with-sysroot=$ANDROID_SYSROOT
#mk
#inst build/.libs/libSDL2.so
#cd ../

#cd protobuf-2.6.1
#conf ./configure --build=i686-pc-linux-gnu "CFLAGS=-m32 -Wno-narrowing" "CXXFLAGS=-m32 -Wno-narrowing -fpermissive" "LDFLAGS=-m32"
#mk
#cd ../

cd StubSteamAPI/
mk
inst libsteam_api.so 
cd ../

cd libiconv-1.15/
./configure --host=$ANDROID_HOST --with-sysroot=$ANDROID_SYSROOT --enable-static
mk
inst lib/.libs/libiconv.a
cd ../

#cd cryptopp
#mk IS_X86=1 IS_X64=0 CC='gcc -m32 -msse4 -fPIC' CXX='g++ -m32 -msse4 -D_GLIBCXX_USE_CXX11_ABI=0'
#cp libcryptopp.a ../../lib/common/ubuntu12_32/
#cd ../

cd libjpeg
conf ./configure --host=$ANDROID_HOST --with-sysroot=$ANDROID_SYSROOT
mk
cp .libs/libjpeg.a ../../lib/common/androidarm32
inst .libs/libjpeg.a
cd ../

#cd libpng
#conf ./configure --host=$ANDROID_HOST --with-sysroot=$ANDROID_SYSROOT
#mk
#cp .libs/libpng16.a ../../lib/public/androidarm32/libpng.a
#cd ../

#cd zlib
#CFLAGS="-m32" LDFLAGS="-m32" conf ./configure
#conf ./configure --build=i686-pc-linux-gnu "CFLAGS=-m32 -Wno-narrowing" "CXXFLAGS=-m32 -Wno-narrowing -fpermissive" "LDFLAGS=-m32"
#mk
#inst libz.a
#cd ../


#cd libedit-3.1
#conf ./configure --build=i686-pc-linux-gnu "CFLAGS=-m32 -Wno-narrowing" "CXXFLAGS=-m32 -Wno-narrowing -fpermissive" "LDFLAGS=-m32"
#mk
#cd ../
