
import WildcardSearch
import sys
import re
import os


curProps = {}


modelsContentFallbackDir = "c:\\hl2\\hl2\\models"
modelsContentDir = "c:\\hl2\\cstrike\\models"

materialsFallbackDir = "c:\\hl2\\hl2\\materials"
materialsDir = "c:\\hl2\\cstrike\\materials"
mapsDir = "c:\\hl2\\cstrike\\maps"
exeDir = "c:\\hl2\\bin"


# RE to look for '$surfaceProp blah'
surfacePropRE = re.compile( r'\"?\$surfaceprop\"?\s+\"?(?P<propname>[^\"]+)\"?', re.IGNORECASE )



# ------------------------------------------------------------------------------------------- #
# Helper functions.
# ------------------------------------------------------------------------------------------- #

def FileExists(f):
	try:
		file = open(f)
	except IOError:
		exists = 0
	else:
		exists = 1
		file.close()
	return exists


def PrintFilename( filename ):
	print filename


def SearchFile( filename ):
		f = open( filename, "rt" )
			
		PrintFilename( filename )

		fileData = f.read()
		match = surfacePropRE.search( fileData )
		if match:
			propName = match.group( 1 ).upper()
			curProps[propName] = 1

		f.close()



# ------------------------------------------------------------------------------------------- #
# Search all the map files for texture names and model files.
# ------------------------------------------------------------------------------------------- #

usedVMTFiles = {}
modelFiles = {}


# RE to look for 'material blah'
materialRE = re.compile( r'\"?\material\"?\s+\"?(?P<matname>[^\"]+)\"?', re.IGNORECASE )

# Look for a model name referenced in the VMF file.
modelRE = re.compile( r'\"models\/(?P<modelname>.+)\.mdl\"', re.IGNORECASE )


files = WildcardSearch.WildcardSearch( mapsDir + "\\*.vmf", 1 )
for filename in files:
	f = open( filename, "rt" )
	fileData = f.read()
	f.close()

	PrintFilename( filename )

	# Get all the model names.
	allMatches = modelRE.findall( fileData )
	for match in allMatches:
		modelFiles[match.upper()] = 1

	# Get all the texture names.
	allMatches = materialRE.findall( fileData )
	for match in allMatches:
		vmtName = match
		usedVMTFiles[vmtName] = 1


# ------------------------------------------------------------------------------------------- #
# Search all the model files for surface props.
# ------------------------------------------------------------------------------------------- #

# Make sure we look at ALL models in the CStrike folder.
for filename in WildcardSearch.WildcardSearch( modelsContentDir + "\\*.mdl", 1 ):
	modelFiles[filename.upper()] = 1

for iModel in modelFiles.keys():
	iModel = iModel.replace( "/", "\\" )
	filename = modelsContentDir + "\\" + iModel + ".mdl"
	if not FileExists( filename ):
		filename = modelsContentFallbackDir + "\\" + iModel + ".mdl"

	if FileExists( filename ):
		PrintFilename( filename )

		cmd = exeDir + "\\studiomdl.exe -PrintSurfaceProps " + filename
		f = os.popen( cmd )
		if f:
			output = f.readlines()
			returnValue = f.close()

			if returnValue == None:
				for line in output:
					curProps[line.upper().strip()] = 1



# ------------------------------------------------------------------------------------------- #
# Search all the texture files for surface props.
# ------------------------------------------------------------------------------------------- #

for iFile in usedVMTFiles.keys():
	filename = materialsDir + "\\" + iFile + ".vmt"
	if not FileExists( filename ):
		filename = materialsFallbackDir + "\\" + iFile + ".vmt"
	
	if FileExists( filename ):
		SearchFile( filename )


			

# ------------------------------------------------------------------------------------------- #
# Output the results.
# ------------------------------------------------------------------------------------------- #

print "\n"
print "---------------------------------"
print "- Surface types found"
print "---------------------------------\n"

sortedList = [x for x in curProps.keys()]
sortedList.sort()

for x in sortedList:
	print x


