

import os, sys


class PySSException:
	pass


g_bVerbose = 0


class PySourceSafe:
	"""
	PySourceSafe represents a connection to a SS database.
	Pass the directory to the database in the constructor, or \\hl2vss\hl2vss is assumed.
	"""

	def __init__( self, dbPath=r"\\hl2vss\hl2vss" ):
		# Store this off so we can put it in the environment when we run ss.
		self.m_DBPath = dbPath
		self.bUseExceptions = 0

		self.m_VSSCommand = None

		# Since we can't use spawnvpe to have it find ss.exe for us, we have to look in the path for it manually.
		paths = os.environ['path'].split( ';' )
		for path in paths:
			if path[-1:] == '/' or path[-1:] == '\\':
				testFilename = path + 'ss.exe'
			else:
				testFilename = path + '\\' + 'ss.exe'

			testFilename = testFilename.replace( '\"', '' )
			if os.access( testFilename, os.F_OK ):
				self.m_VSSCommand = testFilename
				break

		# If we can't find the vss command, they we're screwed so throw an exception.
		if not self.m_VSSCommand:
			raise PySSException

		for_cmd = 'for %I in ("' + self.m_VSSCommand + '") do echo %~sI'
		p = os.popen(for_cmd)
		self.m_VSSCommand = p.readlines()[-1]    # last line from for command
		p.close()

		self.m_VSSCommand = self.m_VSSCommand.replace( '\n', '' )
		self.lastExitStatus = 0
		self.m_LastCommandOutput = ''

			
	# Throw an exception on error? Default is no.
	def EnableExceptions( self, bEnable ):
		self.bUseExceptions = 1


	# Get the output from the last command.
	# This returns a list of the lines of text with the output.
	def GetLastCommandOutput():
		return self.m_LastCommandOutput


	# Return a list of the filenames in a directory.
	def ListFiles( self, rootDir ):
		outlines = self.__RunCommand( ['dir',rootDir] )
		if not outlines:
			return None

		returnList = []
		for i in range( 1, len( outlines ) ):
			if len( outlines[i] ) == 0:
				break
			elif outlines[i][0:1] != '$':
				returnList.append( outlines[i] )

		return returnList
		
	
	def ListDirectories( self, rootDir ):
		outlines = self.__RunCommand( ['dir',rootDir] )
		if not outlines:
			return None

		returnList = []
		for i in range( 1, len( outlines ) ):
			if len( outlines[i] ) == 0:
				break
			elif outlines[i][0:1] == '$':
				returnList.append( outlines[i] )

		return returnList


	# Example: p.AddFile( "$/hl2/release/dev/hl2/scripts", "c:\\test.txt", "some comment here" )
	def AddFile( self, vssDir, localFilename, comment=None ):
		if not self.__RunCommand( ['cp', vssDir] ):
			return None

		args = ['add', localFilename, '-I-']
		if comment:
			args.append( '-C%s' % comment )

		return self.__RunCommand( args )


	# Create a new directory (or 'subproject').
	# Example: p.CreateDirectory( '$/tfc/models/test' )
	# Note: this WILL create the subdirectories leading up to the final one if they don't exist.
	def CreateDirectory( self, vssDir, comment=None ):
		if not comment:
			comment = ''

		lastRet = []

		# Create all directories leading up to this one/
		dirs = vssDir.split( '/' )
		curDir = ''
		for dir in dirs:
			if len( curDir ) == 0:
				curDir = dir
			else:
				curDir = curDir + '/' + dir

				# Does it exist already?
				self.__RunCommand( ['properties', curDir], 0 )
				if self.lastExitStatus != 0 and self.lastExitStatus != None:
					lastRet = self.__RunCommand( ['create', curDir, '-C%s' % comment, '-I-'] )
					if lastRet == None:
						break

		return lastRet


	# Remove a file. Returns:
	# 0 if the file doesn't exist
	# 1 if it existed and was removed
	# None if the file existed but there was an error.
	def DeleteFile( self, vssFilename ):
		self.__RunCommand( ['properties', vssFilename], 0 )
		if self.lastExitStatus:
			return 0
		else:
			# Now try to remove it.			
			self.__RunCommand( ['delete', vssFilename, '-I-'] )
			if self.lastExitStatus:
				return None
			else:
				return 1


	# Get the checkout status of a file (and find out if the file even exists).
	def GetFileStatus( self, ssFilename ):
		pass


	# Get the local working directory for the specified SS directory.
	def GetWorkingDirectory( self, ssDir ):
		pass


	# Get the local filename for the specified file.
	def GetLocalFilename( self, ssFilename ):
		pass


	# For all the CheckOut and Get commands, if localFilename is not None, then it'll 
	# treat that as the local file. Otherwise, it'll use the default working directory for that directory in SS.
	# Returns 1 if successful, 0 if there was an error.

	# Check out a file.
	# Use GetLastOutput() to get the output string.
	def CheckOutFile( self, ssFilename, localFilename=None ):
		pass


	# Check out the whole directory.
	def CheckOutDir( self, ssDirName, localDirName=None, bRecursive=0 ):
		pass


	# Get a file.
	def GetFile( self, ssFilename, localFilename=None ):
		pass

	
	# Get a whole directory.
	def GetFile( self, ssFilename, localFilename=None ):
		pass

	# Check in a file. Optionally specify a comment.
	# Returns 1 if successful, 0 otherwise.
	def CheckInFile( self, ssFilename, localFilename=None, comment=None ):
		pass

	
	# The big master function to run a vss command and get the results back in a list.
	def __RunCommand( self, args, bHandleErrors=1 ):
		# First, set the environment up.
		tempEnviron = os.environ
		os.environ['ssdir'] = self.m_DBPath
		
		# Now build the command.
		cmd = self.m_VSSCommand
		for i in args:
			cmd = cmd + ' \"%s\"' % i

		if g_bVerbose:		
			print "VSS: " + cmd

		# Run the command and capture its output.
		f = os.popen( cmd, 'r' )
		lines = f.readlines()
		self.lastExitStatus = f.close()

		self.m_LastCommandOutput = lines
		lines = [i.strip() for i in lines]

		# Restore the environment.
		os.environ = tempEnviron

		if self.lastExitStatus != 0 and self.lastExitStatus != None:
			if bHandleErrors:
				print 'VSS Error (status: %d): ' % self.lastExitStatus
				print 'cmd: %s' % cmd
				print 'output: '
				for i in lines:
					print '\t%s' % i
				
				if self.bUseExceptions:
					raise PySSException
				else:
					return None

		return lines



p = PySourceSafe()


# TEST CODE
"""
print "Files"
files = p.ListFiles( '$/hl2/release/dev' )
for i in files:
	print i

print "\n\nDirectories"
files = p.ListDirectories( '$/hl2/release/dev' )
for i in files:
	print i


vssRoot = '$/tfc/models'
p.AddFile( "$/tfc", "c:\\test.txt", "test comment blah blah" )

p.CreateDirectory( '$/tfc/aa/bb/cc/dd', 'here is hte comment' )
"""
