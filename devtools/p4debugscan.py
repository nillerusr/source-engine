
import sys, re, os, stat
from ctypes import *


if len( sys.argv ) < 3:
	print 'p4debugscan.py scans for debug DLLs in the last N revisions of a file in Perforce'
	print 'usage: p4debugscan.py filename N'
	print 'alt  : p4debugscan.py filename -1   (will only look at filename on disk)'
	print 'ex   : p4debugscan.py //valvegames/rel/hl2/game/bin/engine.dll 10'
	print '       (looks for debug versions in the past 10 revisions of engine.dll)'
	sys.exit( 1 )

filename = sys.argv[1]
nRevisions = sys.argv[2]


if nRevisions == '-1':
	hdll = windll.kernel32.LoadLibraryA( filename )
	fn_addr = windll.kernel32.GetProcAddress( hdll, "BuiltDebug" )
	windll.kernel32.FreeLibrary( hdll )

	if fn_addr == 0:
		print '%s: RELEASE' % (filename)
	else:
		print '%s: DEBUG' % (filename)
else:
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

	testDLLFilename = 'p4debugscan_test.dll'
			
	for x in changelistNumbers:
		os.system( 'p4 print -q -o %s %s@%s' % (testDLLFilename, filename, x[0]) )
		
		hdll = windll.kernel32.LoadLibraryA( testDLLFilename )
		fn_addr = windll.kernel32.GetProcAddress( hdll, "BuiltDebug" )
		windll.kernel32.FreeLibrary( hdll )

		if fn_addr == 0:
			print '%s: %s@%s - RELEASE' % (x[1], filename, x[0])
		else:
			print '%s: %s@%s - DEBUG' % (x[1], filename, x[0])

	os.chmod( testDLLFilename, stat.S_IWRITE | stat.S_IREAD )
	os.unlink( testDLLFilename )
