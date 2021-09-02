# source-engine
The main purpose of this repository is to port the engine for other platforms.
# Goals
* fixing bugs
* NEON support
* DXVK support
* remove unnecessary dependencies
* Elbrus port
* Arm(android) port
* improve performance
* replace current buildsystem with waf
* rewrite achivement system( to work without steam )
# How to Build?
1. Clone repo ( ```git clone https://github.com/nillerusr/source-engine```)
2. Run ```git submodule init && git submodule update```

On Linux:
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
On Windows:
**TODO(WAF is not configured for Windows. Use VPC as temporary solution)**
