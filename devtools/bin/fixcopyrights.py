#!/usr/bin/python
# ========= Copyright Valve Corporation, All rights reserved. ============

import subprocess
import re
import os
import sys

reValve = re.compile( "valve", flags = re.IGNORECASE )
reTurtleRock = re.compile( "turtle rock", flags = re.IGNORECASE )
reCopyright = re.compile( "copyright", flags = re.IGNORECASE )
sOutputCopyright = "//========= Copyright Valve Corporation, All rights reserved. ============//\n"

def IsOldCopyrightLine( line ):
	if( len( reCopyright.findall( line ) ) == 0 ):
		return False
	if( len( reValve.findall( line ) ) == 0 
		and len( reTurtleRock.findall( line ) ) == 0 ):
		return False
	
	return True


rFilesWithNoCopyrightNotice = []

def FixCopyrightNotice( sFullPath ):
	nLine = 0

	f = open( sFullPath, "r" )
	if( not f ):
		print( "Unable to open file " + sFullPath + "\n" )
		return

	rFileContents = f.readlines()
	f.close()
	nOldCopyright = -1
	for line in rFileContents:
		if( nLine < 10 ):
			if( line == sOutputCopyright ):
				# File already has the right notice
				return
			if( IsOldCopyrightLine( line ) ):
				nOldCopyright = nLine
				break

		nLine += 1
	if( nOldCopyright == -1 ):
		rFilesWithNoCopyrightNotice.append( sFullPath )
		rFileContents.insert( 0, sOutputCopyright )
	else:
		rFileContents[ nOldCopyright ] = sOutputCopyright

	# open the file for edit
	subprocess.call( [ "p4", "edit", sFullPath ], stdout = subprocess.PIPE )

	# open the file for writing
	f = open( sFullPath, "w" )
	f.writelines( rFileContents )
	f.close()

rDirsToSkip = [ 
	'thirdparty', 
	'external', 
	'BinkSDK',
	'bink',
	'bink_x360',
	'freetype',
	'GL',
	'maya',
	'miles',
	'curl',
	'ihfx',
	'lxma',
	'modo',
	'openal',
	'opengl',
	'p4api',
	'python',
	'quicktime_win32',
	'xsi',
	'speex',
	'ocaml',
	'perl5',
	'dx10sdk',
	'dx11sdk',
	'dx9sdk',
	'haptics',
	'ajb',
	'stb',
	'havok',
	'hk_physics',
	'lua',
	'maxsdk',
	'x360xdk',
	'swigwin-1.3.34',
	'sapi51',
	'WMPSDK10',
	'FontMaker',
	'mxtk',
	'nvtristriplib',
	'g15',
	'lzma',
	'libparsifal-0.8.3',
	'parsifal',
	'libpng',
	'mysql',
	'zip',
	'zlib',
	'Zlib',
	'windowssdk',
	'bzip2',
	'jpeglib',
	'MakeGameData',
	'toollib',
	]

rFileExtensionsToSkip = [
	'.pb.h',
	'.pb.cpp',
	'.spa.h',
	'ATI_Compress.h',
	'luaxlib.h',
	'lua.h',
	'luaconf.h',
	'lualib.h',
	'eax.h',
	'IceKey.cpp',
	'nvtc.h',
	'amd3dx.h',
	'halton.h',
	'snappy',
	'extendedtrace',
	]

def FixCopyrightNoticeWalk( sPath ):
	for root, dirs, files in os.walk( sPath ):
		print "Walking directory", root
		#print root, dirs
		for sDir in rDirsToSkip:
			if sDir in dirs:
				print "Skipping dir ", os.path.join( root, sDir )
				dirs.remove( sDir )

		for sFilename in files:
			sShortFilename, sFileExt = os.path.splitext( sFilename )

			if( sFileExt in [ '.cpp', '.h' ] ):
				bSkip = False
				for sExt in rFileExtensionsToSkip:
					if sExt in sFilename:
						bSkip = True

				#print "filename=", sFilename 
				if( bSkip ):
					print "Skipping ", sFilename, "because of its extension"
				else:
					FixCopyrightNotice( os.path.join( root, sFilename ) )


#FixCopyrightNotice( os.path.join( "..", "..", "bitmap", "bitmap.cpp" ) )
#FixCopyrightNoticeWalk( os.path.join( "..", "..", "bitmap" ) )

if( len( sys.argv ) != 2 ):
	print "Usage: fixcopyrights.py <path>"
	sys.exit(1)

FixCopyrightNoticeWalk( sys.argv[1] )

if( len( rFilesWithNoCopyrightNotice ) ):

	f = open( "newcopyrights.txt", "w" )
	for file in rFilesWithNoCopyrightNotice:
		f.write( file + "\n" )
	f.close()

	print "Copyright notices added to", len( rFilesWithNoCopyrightNotice ), "files. See newcopyrights.txt for a list\n"

