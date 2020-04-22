
import os
import sys
import stat


g_NumSoundsAdded = 0


def ScanSounds_R( baseDir, relativeDir, outFile ):
	files = os.listdir( baseDir )
	for filename in files:
		fullFilename = baseDir + "\\" + filename
		if len( relativeDir ) > 0:
			newRelativeDir = relativeDir + "/" + filename
		else:
			newRelativeDir = filename

		mode = os.stat( fullFilename )[stat.ST_MODE]
		
		if stat.S_ISREG( mode ):
			if filename[-4:].upper() == ".WAV":
				outFile.write( "\"%s\"\n" % ( newRelativeDir[0:-4] ) )
				outFile.write( "{\n" )
				outFile.write( "\t\"channel\"\t\t\"CHAN_ITEM\"\n" )
				outFile.write( "\t\"volume\"\t\t\"VOL_NORM\"\n" )
				outFile.write( "\t\"soundlevel\"\t\"SNDLVL_NONE\"\n" )
				outFile.write( "\t\"pitch\"\t\t\t\"PITCH_NORM\"\n" )
				outFile.write( "\t\"wave\"\t\t\t\"%s\"\n" % ( newRelativeDir ) )
				outFile.write( "}\n\n" )
				
				global g_NumSoundsAdded
				g_NumSoundsAdded += 1
		
		if stat.S_ISDIR( mode ):
			ScanSounds_R( fullFilename, newRelativeDir, outFile )


# Make sure we've got a valid base directory.
if len( sys.argv ) < 2:
	print "Error: Must specify the root sound directory."
	sys.exit( 1 )

baseDir = sys.argv[1]
if os.access( baseDir, os.R_OK ) != 1:
	print "Error: Can't access %s." % ( baseDir )
	sys.exit( 1 )


# Now scan all the .cpp files for sound function calls.
outFile = open( "game_sounds.txt", "wt" )

ScanSounds_R( baseDir, "", outFile )

outFile.close()


print "Added %d sounds to game_sounds.txt" % ( g_NumSoundsAdded )

