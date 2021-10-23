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
On Linux:

dependencies:
fontconfig, freetype2, OpenAL, SDL2, libbz2, libcurl, libjpeg, libpng, zlib
```
./waf configure -T debug
./waf build
```
On Linux for Android(**Note: only Android NDK r10e is supported**):
```
export ANDROID_NDK=/path/to/ndk
./waf configure -T debug --android=armeabi-v7a,4.9,21
./waf build
```
On Windows/MacOS:
**TODO(WAF is not configured for Windows/MacOS. Use VPC as temporary solution)**
