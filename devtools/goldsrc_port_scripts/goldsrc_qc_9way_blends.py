
# This script converts old 9 way blends that look like this:
#
# $sequence crouch_tommy_shoot "crouch_tommy_shootupL" "crouch_tommy_shootup" "crouch_tommy_shootupR" "crouch_tommy_shootmidL" "crouch_tommy_shootmid" "crouch_tommy_shootmidR" "crouch_tommy_shootdownL" "crouch_tommy_shootdown" "crouch_tommy_shootdownL" blend XR 45 -45 fps 30 origin 0 0 17 { event 7002 1 "1" }
#
# to this:
#
# $sequence crouch_tommy_shoot {
#	"crouch_tommy_shootupL" "crouch_tommy_shootup" "crouch_tommy_shootupR" 
#	"crouch_tommy_shootmidL" "crouch_tommy_shootmid" "crouch_tommy_shootmidR" 
#	"crouch_tommy_shootdownL" "crouch_tommy_shootdown" "crouch_tommy_shootdownL" 
#	blendwidth 3 blend body_yaw -45 45 blend body_pitch -65 65
#	fps 30 origin 0 0 17 { event 7002 1 "1" }
# }

import sys
import re
import string

if len( sys.argv ) < 3:
	print "Requires 2 parameters: <input file> <output file>"
	sys.exit( 1 )


# Read in the file.
f = open( sys.argv[1], "rt" )
lines = f.readlines()
f.close()


# Do the search/replace.
def named( re, name ):
	return r'(?P<' + name + r'>' + re + ')'

def ident( animName ):
	if animName:
		return named( r'\"?\S+\"?', animName )
	else:
		return r'\"?\S+\"?'


ws = r'\s*'

# THIS IS THE FIRST PASS - ADDS THE 9-WAY BLENDS
if 1:
	nineNames = [ws+ident('a'+str(i)) for i in range(1,10)]
	sequenceAndAnims = named( r'\s*', 'prefix' ) + r'\$sequence' + ws + ident('animName') + string.join( nineNames, '' )

	# NOTE THERE IS A KNOWN BUG HERE - extra NEEDS TO INCLUDE THE 'blend xr -45 45' STUFF SO WE CAN STRIP IT OUT
	extra = ws + ident('blend') + named( '.+', 'extra' )
	myRE = re.compile( sequenceAndAnims + extra )

	#lines = ['$sequence crouch_pistol_aim "crouch_pistol_aimupL" "crouch_pistol_aimup" "crouch_pistol_aimupR" "crouch_pistol_aimmidL" "crouch_pistol_aimmid" "crouch_pistol_aimmidR" "crouch_pistol_aimdownL" "crouch_pistol_aimdown" "crouch_pistol_aimdownR" loop blend XR 45 -45 fps 30 origin 0 0 17']

	outLines = []
	for curLine in lines:
		m = myRE.match( curLine )
		if m and (m.group('blend') == 'blend' or m.group('blend') == 'loop'):
			prefix = m.group('prefix')
			curLine =  prefix + '$sequence %s {\n' % m.group('animName')
			curLine += prefix + '\t%s %s %s\n' % ( m.group('a1'), m.group('a2'), m.group('a3') ) 
			curLine += prefix + '\t%s %s %s\n' % ( m.group('a4'), m.group('a5'), m.group('a6') ) 
			curLine += prefix + '\t%s %s %s\n' % ( m.group('a7'), m.group('a8'), m.group('a9') ) 
			curLine += prefix + '\tblendwidth 3 blend body_yaw -45 45 blend body_pitch -65 65\n'
			curLine += prefix + '\t' + m.group( 'extra' ) + '\n'
			curLine += prefix + '}\n\n'
		
		outLines.append( curLine )

# THIS IS THE SECOND PASS - IT ADDS THE UPPER-BODY WEIGHTLISTS
if 0:
	lookFor = named( r'\s*', 'start' ) + named( r'blendwidth 3 blend body_yaw -45 45 blend body_pitch -65 65', 'mid' ) + named( r'\s*', 'end' )
	myRE = re.compile( lookFor )

	outLines = []
	for curLine in lines:
		m = myRE.match( curLine )
		if m:
			curLine = curLine + m.group('start') + 'weightlist upperbody\n'

		outLines.append( curLine )


# Save the output file.
f = open( sys.argv[2], "wt" )
f.writelines( outLines )
f.close()
