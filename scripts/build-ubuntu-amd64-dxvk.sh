#!/bin/sh
# Linux amd64 build with DXVK-native (D3D9 on Vulkan) instead of ToGL.
# DXVK (submodule dxvk/, d3d9 only, SDL2 WSI) is built first and installed into
# dxvk-x86_64/, the default --dxvk-prefix; copy dxvk-x86_64/lib/libdxvk_d3d9.so*
# next to the engine libraries (bin/) to run it.

git submodule init && git submodule update
# dxvk has "update = none" in .gitmodules, so only this script checks it out
git submodule update --init --recursive --checkout dxvk || exit 1
sudo apt-get update
sudo apt-get install -f -y libopenal-dev g++-multilib gcc-multilib libpng-dev libjpeg-dev libfreetype6-dev libfontconfig1-dev libcurl4-gnutls-dev libsdl2-dev zlib1g-dev libbz2-dev libedit-dev meson ninja-build glslang-tools

if [ ! -f build-dxvk/build.ninja ]; then
	meson setup --buildtype release --prefix "$PWD/dxvk-x86_64" --libdir lib \
		-Denable_dxgi=false -Denable_d3d8=false -Denable_d3d10=false -Denable_d3d11=false \
		-Dnative_sdl2=enabled -Dnative_sdl3=disabled -Dnative_glfw=disabled \
		build-dxvk dxvk || exit 1
fi

ninja -C build-dxvk install &&
./waf configure -T debug --disable-warns --use-dxvk $* &&
./waf build
