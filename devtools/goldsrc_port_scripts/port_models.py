
import msvcrt, os, sys, re, WildcardSearch, VisitFiles


def GetArg( argName, ifAtEnd='' ):
	for i in range( 1, len( sys.argv ) ):
		if argName.upper() == sys.argv[i].upper():
			if i+1 >= len( sys.argv ):
				return ifAtEnd
			else:
				return sys.argv[i+1]
	return None


def PrintUsage():
	print r'port_models.py -srcdir <directory with the QCs> -vproject <game directory>'
	print r'Note: -srcdir represents the ROOT of the models directory, but it points at the model sources'
	print r'      instead of the model content.'
	print r'ex: port_models.py -srcdir c:\hl2\models\tfc -vproject c:\hl2\tfc'
	sys.exit( 1 )


def FixFilename( filename ):
	filename = filename.replace( '/', '\\' )
	if filename == '.\\':
		return filename
	else:
		return re.compile( r'[^\.]?\.\\' ).sub( '', filename )


def GetDirName( fullFilename ):
	a = fullFilename.rfind( '/' )
	b = fullFilename.rfind( '\\' )
	if a == -1 and b == -1:
		return FixFilename( fullFilename )
	else:
		return FixFilename( fullFilename[0 : max(a,b)] )


g_bAlwaysContinue = 0
def CheckContinue():
	global g_bAlwaysContinue
	if g_bAlwaysContinue:
		return 1
	else:
		sys.stdout.write( 'Continue [(Y)es/(N)o/(A)lways]? ' )
		r = str( msvcrt.getch() ).upper()
		if r == 'Y':
			return 1
		elif r == 'A':
			g_bAlwaysContinue = 1
			return 1
		else:
			return 0


#
# Get command line args.
#
modelSourceDir = GetArg( '-srcdir' )
vprojectDir = GetArg( '-vproject' )
if not modelSourceDir or not vprojectDir:
	PrintUsage()

binDir = vprojectDir + '\\..\\bin'

studiomdlFilename = binDir + '\\studiomdl.exe'
vtexFilename = binDir + '\\vtex.exe'
xwadFilename = binDir + '\\xwad.exe'
if not os.access( studiomdlFilename, os.F_OK ) or not os.access( vtexFilename, os.F_OK ) or not os.access( xwadFilename, os.F_OK ):
	print 'Can\'t find vtex.exe, xwad.exe, or studiomdl.exe in bin directory %s.' % (binDir)
	sys.exit( 1 )



#
#
# REGEX's and code to convert the QC file.
#
#
reModelName = re.compile( r'(?P<start>\$modelname\s+)(?P<stripout>.*models(\\|\/))(?P<keep>.+)', re.IGNORECASE )
reCDTexture = re.compile( r'(?P<start>\$(cdTexture|cdmaterials)\s+)(?P<stripout>.*models(\\|\/))(?P<keep>.+)', re.IGNORECASE )
reCD = re.compile( r'\$cd\s', re.IGNORECASE )

def ident( name=None ):
	if name:
		return r'(?P<' + name + r'>\s+((\".*?\")|([^\"]\S*)))'
	else:
		return r'\s+((\".*?\")|([^\"]\S*))'

reAttachmentLine = re.compile( r'(?P<keep>.*\$attachment'+ident()+ident()+ident()+ident()+ident() + ')' + ident()+ident(), re.IGNORECASE )

g_DirToMaterialsMap = {}

def EditQCFile( fullFilename ):
	f = open( fullFilename, 'rt' )
	lines = f.readlines()
	f.close()

	bDidMaterials = 0

	outLines = []
	for line in lines:
		if not reCD.search( line ):
			# Strip out stuff before /models in the $modelname line.
			m = reModelName.search( line )
			if m:
				line = m.group('start') + m.group('keep' ) + '\n'
			else:
				m = reCDTexture.search( line )
				if m:
					materialsDirName = 'models/' + m.group('keep')
					bmpDir = m.group('keep')
					g_DirToMaterialsMap[FixFilename(bmpDir.upper())] = materialsDirName
					bDidMaterials = 1
					line = '$cdmaterials %s\n' % materialsDirName

			# Strip out the last 2 args on the $attachment line if they're using too many.
			# Goldsrc studiomdl allowed 2 extra args but it ignored them.
			m = reAttachmentLine.search( line )
			if m:
				line = m.group('keep')

			outLines.append( line )

	if not bDidMaterials:
		print 'No $cdtexture line in %s so we don\'t know where to convert the BMPs into.' % (fullFilename)
		sys.exit( 1 )

	#
	# Write the QC back out.
	#
	try:
		f = open( fullFilename, 'wt' )
	except IOError:
		print 'Can\'t open %s for writing.\nMake sure it is not read-only.' % fullFilename
		sys.exit( 1 )
	f.writelines( outLines )
	f.close()



#
# Convert all the QC files and backup the old ones.
#
print '--- Converting QC files and running studiomdl ---'

def QCFileCallback( filename, relativeName, fullFilename ):
	print '\t' + relativeName

	# First, backup the QC.
	os.system( 'copy \"%s\" \"%s\" > out' % (fullFilename, fullFilename + ".OLD") )
		
	#
	# Now, make some edits.
	#
	EditQCFile( fullFilename );

	#
	# Run studiomdl on it.
	#
	cmd = '%s -vproject \"%s\" \"%s\" > out' % (studiomdlFilename, vprojectDir, fullFilename)
	if os.system( cmd ) != 0:
		lines = open( 'out', 'rt' ).readlines()
		sys.stdout.writelines( lines )
		sys.exit( 1 )

VisitFiles.VisitFiles( modelSourceDir, WildcardSearch.GetRegExForDOSWildcard( '*.qc' ), QCFileCallback, None )


#
# Now convert all the BMP files.
#
print '\n--- Converting BMP files and running vtex ---'
def BMPFileCallback( filename, relativeName, fullFilename ):
	print '\t' + relativeName

	# Find out where we want to store this BMP's files under materials.
	dirName = GetDirName( relativeName )
	
	try:
		cdMaterials = g_DirToMaterialsMap[dirName.upper()]
	except:
		#print '\tWarning: couldn\'t convert %s (don\'t know where to put it).' % (relativeName)
		pass
	else:
		#
		# Run XWAD.
		#
		cmd = '%s -quiet -bmpfile \"%s\" -basedir \"%s\"' % (xwadFilename, fullFilename, vprojectDir)
		if os.system( cmd ) != 0:
			print '\tError running xwad on %s' % fullFilename
			if not CheckContinue():
				print '\tcmd: %s' % cmd
				sys.exit( 1 )
		
		#
		# Move the TGA it generated into the right directory.
		#
		materialsrcDir = '%s\\materialsrc\\%s' % (vprojectDir, cdMaterials)
		if not os.access( materialsrcDir, os.F_OK ) and os.system( 'md \"%s\"' % materialsrcDir ) != 0:
			print 'Can\'t create or access directory %s.' % materialsrcDir

		materialsDir = '%s\\materials\\%s' % (vprojectDir, cdMaterials)
		if not os.access( materialsDir, os.F_OK ) and os.system( 'md \"%s\"' % materialsDir ) != 0:
			print 'Can\'t create or access directory %s.' % materialsDir

		baseTGAFilename = filename[0:-4] + '.tga'
		generatedFilename = vprojectDir + '\\materialsrc\\' + baseTGAFilename
		cmd = 'move  \"%s\" \"%s\"' % (generatedFilename, materialsrcDir)
		if os.system( cmd ) != 0:
			print 'system( %s ) failed.' % cmd

		#
		# Lastly, run vtex on it.
		#
		cmd = '%s -shader VertexLitGeneric -quiet \"%s\\%s\"' % (vtexFilename, materialsrcDir, baseTGAFilename)
		if os.system( cmd ) != 0:
			sys.exit( 1 )
	

VisitFiles.VisitFiles( modelSourceDir, WildcardSearch.GetRegExForDOSWildcard( '*.bmp' ), BMPFileCallback, None )




