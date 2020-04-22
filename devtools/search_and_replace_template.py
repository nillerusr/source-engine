
# This file can be used as a template for search-and-replace operations across a bunch of files.
# It's fairly slow, but it's still a lot faster than doing it by hand.
# You can use foreach.exe or batch commands to run the script on a bunch of files at once.
# It's a good idea to test your regular expressions beforehand though. To do this, just set g_bTest to 1.

import sys
import re


g_bTest = 0


# Read the file in.
f = open( sys.argv[1], "rt" )
lines = f.readlines()
f.close()


# First entry is the regular expression to look for.
# Second entry is the text to replace it with.
myREList = [
	[re.compile( r"m_vecRenderOrigin" ),						r"GetAbsOrigin()"],
	[re.compile( r"m_vecRenderAngles" ),						r"GetAbsAngles()"],
	[re.compile( r"GetAbsOrigin\(\)\s*\=\s*(?P<setTo>.+);" ),	r"SetAbsOrigin( \g<setTo> );" ],
	[re.compile( r"GetAbsAngles\(\)\s*\=\s*(?P<setTo>.+);" ),	r"SetAbsAngles( \g<setTo> );" ],
	[re.compile( r"SetAbsOrigin\( GetAbsOrigin\(\) \);" ),		r"" ],
	[re.compile( r"SetAbsAngles\( GetAbsAngles\(\) \);" ),		r"" ],
	[re.compile( r"GetAbsOrigin\(\)\.Init\(\)" ),				r"SetAbsOrigin( Vector( 0, 0, 0 ) )"],
	[re.compile( r"GetAbsAngles\(\)\.Init\(\)" ),				r"SetAbsAngles( Vector( 0, 0, 0 ) )"],
]


# Now replace occurrences of it.
i = 1
if g_bTest:
	# If g_bTest is set, just print out the matches.
	for x in lines:
		startX = x
		for curRE in myREList:
			x = curRE[0].sub( curRE[1], x )

		if x != startX:
			print "%d: %s" % ( i, x )
			i += 1
else:
	f = open( sys.argv[1], "wt" )

	for x in lines:
		for curRE in myREList:
			x = curRE[0].sub( curRE[1], x )
		
		f.write( x )

	f.close()


print sys.argv[1]
