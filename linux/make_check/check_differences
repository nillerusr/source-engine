#!/bin/bash

##############################################################
#
# Check to see if two MSVC dsp files differ and if they
# do then print out the changes
#
##############################################################

cd make_check
mkdir cache > /dev/null

IN_DSP=../$1
OUT_DSP=cache/`basename $1`.check

p4 edit $OUT_DSP

if [ ! -f "$OUT_DSP" ]; then
	cp $IN_DSP $OUT_DSP
	chmod +w $OUT_DSP
fi

IN_SUM=`md5sum $IN_DSP | cut -f1 -d" "`
OUT_SUM=`md5sum $OUT_DSP| cut -f1 -d" "`

if test "$IN_SUM" != "$OUT_SUM" ; then
	diff $IN_DSP $OUT_DSP > $OUT_DSP.diff.`date +%H:%M:%S-%d%m%y`
	less $OUT_DSP.diff.`date +%H:%M:%S-%d%m%y`
fi

cp $IN_DSP $OUT_DSP
#chmod +w $OUT_DSP
p4 revert -a $OUT_DSP
cd ..
