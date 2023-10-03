# GM GameMonkey LANGUAGE SPECIFICATION FILE FOR CRIMSON EDITOR
# FIRST EDITED BY Greg

$CASESENSITIVE=YES
$DELIMITERS=~`!@#$%^&*()-+=|\{}[]:;"',.<>/?

# There are currently no preprocessor commands in GM, but there maybe sometime
# $KEYWORDPREFIX=#

# Bit of a hack to highlight binary constants
# Will need to set Variable color to match constants
#$BINARYMARK=0x  Be nice if this was available
# The below lines don't behave as expected
#$VARIABLEPREFIX=0b
#$SPECIALVARIABLECHARS=0b

$HEXADECIMALMARK=0x

$ESCAPECHAR=\
$QUOTATIONMARK1="
$QUOTATIONMARK2='

# In the current version of CrimsonEditor (3.51), the $QUOTATIONMARK3 
# tag isn't supported, but maybe it will be in the future...
#$QUOTATIONMARK3=`

$LINECOMMENT=//
$BLOCKCOMMENTON=/*
$BLOCKCOMMENTOFF=*/
$INDENTATIONON={
$INDENTATIONOFF=}
$PAIRS1=()
$PAIRS2=[]
$PAIRS3={}
