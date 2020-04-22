# Make the 32-bit and 64-bit versions of crypto++
ISX86=1 ISX64=0 make clean
ISX86=1 ISX64=0 make -j12
ISX86=1 ISX64=1 make clean
ISX86=1 ISX64=1 make -j12

# Make the steamcmd version of crypto++
STEAMCMD=1 ISX86=1 ISX64=0 make clean
STEAMCMD=1 ISX86=1 ISX64=0 make -j12

