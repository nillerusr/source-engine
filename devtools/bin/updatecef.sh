#!/bin/bash
# Bash script to update libcef from Steam/main to this project
# brings over all the dll's and import libraries and support libraries plus updates the header files
# run this, then resolve the changelist in perforce and submit

SRCDIR=//Steam/main
PUBLISH_SCRIPT=//thirdpartycode/nonredist/chromeemb/dev/publish_cef_to_steam.py
DSTDIR="$(cd $(dirname "$0")/../../.. && echo $PWD)"

p4 print -q $PUBLISH_SCRIPT | python - --game=tf2 "$DSTDIR" "$SRCDIR"
p4 merge $SRCDIR/client/bin/icudt.dll $DSTDIR/game/bin/icudt.dll
