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
On Linux:
1. Clone repo ( ```git clone https://github.com/nillerusr/source-engine```)
2. Run ```git submodule init && git submodule update```
3. Build
```
./waf configure -T debug
./waf build
```
