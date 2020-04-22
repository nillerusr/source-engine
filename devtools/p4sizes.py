
import sys, re, os


if len( sys.argv ) < 3:
	print 'p4sizes.py shows the size of the last N revisions of a file in Perforce'
	print 'usage: p4sizes.py filename N'
	print 'ex   : p4size.py //valvegames/rel/hl2/game/bin/engine.dll 10'
	print '       (shows the sizes of the last 10 checkins to engine.dll)'
	sys.exit( 1 )

filename = sys.argv[1]
nRevisions = sys.argv[2]


# Get the revisions list.
f = os.popen( 'p4 changes -m %s %s' % (nRevisions, filename) )
str = f.read()
f.close()


changelistNumbers = []
myRE = re.compile( r'change (?P<num>\d+?) on (?P<date>.+?) ', re.IGNORECASE )
while 1:
	m = myRE.search( str )
	if m:
		str = str[m.end():]
		changelistNumbers.append( [m.group('num'), m.group('date')] )
	else:
		break
		
for x in changelistNumbers:
	sys.stdout.write( x[1] + ': ' )
	cmd = 'p4 sizes %s@%s' % (filename, x[1])
	os.system( cmd )


