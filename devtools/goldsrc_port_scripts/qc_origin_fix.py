
#
# This script is used to take a goldsrc-style character model with the origin at 
# its centerpoint and put its origin at its feet.
#
# It parses a .qc file, takes all $origin commands, and subtracts that amount
# from all 'origin' blocks inside a sequence command.
#
# Redirect its output to a file (or the input file).
#

import sys
import re

f = open( sys.argv[1], "rt" )
lines = f.readlines()
f.close()

re1 = re.compile( r'\$origin (?P<x>-?\d+)\s+(?P<y>-?\d+)\s+(?P<z>-?\d+)' )
re2 = re.compile( r'(?P<ws>\s)origin (?P<x>-?\d+)\s+(?P<y>-?\d+)\s+(?P<z>-?\d+)' )

curOffset = [0,0,0]

def OriginOffset( m ):
	testOffset = [ int(m.group('x')), int(m.group('y')), int(m.group('z')) ]
	return '%sorigin %d %d %d' % (m.group('ws'), testOffset[0]-curOffset[0], testOffset[1]-curOffset[1], testOffset[2]-curOffset[2])

for line in lines:
	m = re1.search( line )
	if m:
		#print 'match: %s %s %s' % ( m.group('x'), m.group('y'), m.group('z') )
		curOffset = [ int(m.group('x')), int(m.group('y')), int(m.group('z')) ]

	sys.stdout.write( re2.sub( OriginOffset, line ) )

