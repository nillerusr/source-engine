# source-engine

# Goals
* fixing bugs
* ~~NEON support~~
* DXVK support
* remove unnecessary dependencies
* Elbrus port
* ~~Arm(android) port~~
* improve performance
* ~~replace current buildsystem with waf~~
* rewrite achivement system( to work without steam )
* 64-bit support

# How to Build?
Clone repo and change directory:
```
git clone https://github.com/nillerusr/source-engine --recursive --depth 1
cd source-engine
```
On Linux (**Note: Builds will currently only work on i386**):

dependencies:
fontconfig, freetype2, OpenAL, SDL2, libbz2, libcurl, libjpeg, libpng, zlib

In Ubuntu 16.04 these dependencies can be installed with the following commands from within the repo directory:

```
sudo apt-get install libsdl2-dev cmake libfreetype6-dev libfontconfig1-dev xclip libalut-dev libjpeg-dev libcurl4-gnutls-dev libopus-dev libbz2-dev
cd thirdparty/libpng
./configure
sudo make check
sudo make install
```

Then to build:

```
./waf configure -T debug
./waf build
```
On Linux for Android(**Note: only Android NDK r10e is supported**):

Follow same steps as building for Linux, but when building run:

```
export ANDROID_NDK=/path/to/ndk
./waf configure -T debug --android=armeabi-v7a,4.9,21
./waf build
```
On Windows/MacOS:
**TODO(WAF is not configured for Windows/MacOS. Use VPC as temporary solution)**
