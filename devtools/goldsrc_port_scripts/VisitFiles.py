
import os
import re


STOP_RECURSING = 23452


class VisitFiles:
	"""
		This is a helper class to visit a bunch of files in a tree given
		the regular expressions that will match the filenames you're interested in.

		Create this class like a function, and pass in these parameters:
			baseDir   - a string for the root directory in the search. This must not end in a slash.
			
			fileMasks - a list of strings that contain regular expressions for filenames you want to match.
			
			fileCB    - This function is called for each filename matched. It takes these parameters:
				- short filename ("blah.txt")
				- relative filename ("a/b/c/blah.txt")
				- long filename ("c:/basedir/a/b/c/blah.txt")
			
			dirCB     - a function called for each directory name. This can be None. If it returns STOP_RECURSING,
			            then it won't do callbacks for the files and directories under this one.
				- relative dirName ("./a/b")
				- full dirname (from the baseDir you pass into __init__) ("d:/a/b")

			bRecurse  - a boolean telling whether or not to recurse into other directories.
	
		Example: This would visit all files recursively (.+ matches any non-zero-length string).
			VisitFiles( ".", [".+"], MyCallback )
	"""

	def __init__( self, baseDir, fileMasks, fileCB, dirCB, bRecurse=1 ):
		# Handle it appropriately whether they pass 1 filemask or a list of them.
		if isinstance( fileMasks, list ):
			self.fileMasks = [re.compile( x, re.IGNORECASE ) for x in fileMasks]
		else:
			self.fileMasks = [re.compile( fileMasks, re.IGNORECASE )]

		self.fileCB = fileCB
		self.dirCB = dirCB
		self.bRecurse = bRecurse

		self.__VisitFiles_R( ".", baseDir )


	def __RemoveDotSlash( self, name ):
		if name[0:2] == '.\\':
			return name[2:]
		else:
			return name


	def __VisitFiles_R( self, relativeDirName, baseDirName ):
		if self.dirCB:
			if self.dirCB( relativeDirName, baseDirName ) == 0:
				return

		files = os.listdir( baseDirName )
		
		for filename in files:
			upperFilename = filename.upper()
			fullFilename = self.__RemoveDotSlash( baseDirName + "\\" + filename )
			relativeName = self.__RemoveDotSlash( relativeDirName + "\\" + filename )

			stats = os.stat( fullFilename )
			if stats[0] & (1<<15):
				
				matched = 0
				for curRE in self.fileMasks:
					if curRE.search( upperFilename ):
						matched = 1
						break

				if matched:
					self.fileCB( filename, relativeName, fullFilename )
			else:
				
				# It's a directory.
				if self.bRecurse:
					self.__VisitFiles_R( relativeName, fullFilename )
