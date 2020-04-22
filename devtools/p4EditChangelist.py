
import sys
import os
import re



def PrintUsage():
	print "p4EditChangelist.py [changelist #]"
	print "    - Checks out all the files in the specified changelist."


if len( sys.argv ) < 2:
	PrintUsage()
	sys.exit( 1 )


sChangelist = sys.argv[1]
f = os.popen2( 'p4 describe -s %s' % sChangelist )
allText = f[1].read()
#f.close()
#print allText
	
# Now match an RE to get each filename.
testRE = re.compile( r'\.\.\. (?P<fn>//.+)#\d+ ', re.IGNORECASE )
startPos = 0
while 1:
	m = testRE.search( allText, startPos )
	if not m:
		break
		
	filename = m.group('fn')
	startPos = m.end()

	os.system( 'p4 edit "%s"' % filename )
