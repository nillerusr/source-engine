
from __future__ import generators
import os
import re
import stat


# This takes a DOS filename wildcard like *abc.t?t and returns a regex string that will match it.
def GetRegExForDOSWildcard( wildcard ):
	# First find the base directory name.
	iLast = wildcard.rfind( "/" )
	if iLast == -1:
		iLast = wildcard.rfind( "\\" )

	if iLast == -1:
		dirName = "."
		dosStyleWildcard = wildcard
	else:
		dirName = wildcard[0:iLast]
		dosStyleWildcard = wildcard[iLast+1:]
		
	# Now generate a regular expression for the search.
	# DOS     -> RE
	# *       -> .*
	# .       -> \.
	# ?       -> .
	reString = dosStyleWildcard.replace( ".", r"\." ).replace( "*", ".*" ).replace( "?", "." ) + r'\Z'
	return reString


#
# Useful function to return a list of files in a directory based on a dos-style wildcard like "*.txt"
#
# for name in WildcardSearch( "d:/hl2/src4/dt*.cpp", 1 ):
#	print name
#
def WildcardSearch( wildcard, bRecurse=0 ):
	reString = GetRegExForDOSWildcard( wildcard )
	matcher = re.compile( reString, re.IGNORECASE )

	return __GetFiles_R( matcher, dirName, bRecurse )

def __GetFiles_R( matcher, dirName, bRecurse ):
	fileList = []
	# For each file, see if we can find the regular expression.
	files = os.listdir( dirName )
	for baseName in files:
		filename = dirName + "/" + baseName

		mode = os.stat( filename )[stat.ST_MODE]
		if stat.S_ISREG( mode ):
			# Make sure the file matches the search string.
			if matcher.match( baseName ):
				fileList.append( filename )

		elif bRecurse and stat.S_ISDIR( mode ):
			fileList += __GetFiles_R( matcher, filename, bRecurse )

	return fileList				
	

