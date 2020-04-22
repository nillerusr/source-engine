#!/usr/bin/env python

import p4helpers
import sys
import os
import shutil
import pickle
import zipfile
import zlib
import datetime
import subprocess
import getopt
import glob
import struct
try:
	import wx
except:
	pass

# 
# Set program defaults.
#
g_ChangelistInfoFilename = '__CHANGELISTINFO__'

RUNMODE_BACKUP = 0
RUNMODE_RESTORE = 1
RUNMODE_EMPTY = 2
RUNMODE_NONE = 3

g_RunMode = RUNMODE_NONE
g_ZipFilename = ''
g_bAutoRevert = False
g_bBinsOnly = False
g_Changelist = []
g_bShowUI = False
g_bPickleAllChanges = False
g_bVerbose = True
g_IncludeGlobPatterns = []
g_ExcludeGlobPatterns = []
g_ChangelistComment = ''
g_DefaultFilename = None

g_bCreateDiff = False
g_DiffDir = ''

g_SettingsFilename = 'vcodepickle.cfg'
g_Settings = {}

g_UISaveDirKey = 'uisavedir'

# These are used with -changetree if the user wants to restore the changelist to
# a different tree than it originally came from.
g_FromTree = None
g_ToTree = None

g_QueuedWarnings = []


#
# Pickle data for various file versions supported by vcodepickle.
#
class CV2Data:
	changelistComment = None

class CV3Data:
	relativeToDepotFileMap = None


def ShowUsage():
	print "vcodepickle.py [options] <--backup [options] or --restore or --diff or --empty> [--file filename] [--changelist <changelist> | --bins]"
	print ""
	print "-q/--quiet               sshhhhhh."
	print "-b/--backup              pickle local changes to file"
	print "-r/--restore             apply pickled edits to client"
	print "--empty            	 	create a valid, but empty, pickle, used by buildbot"
	print "-d/--diff                produce a diff of the changes in a vcodepickled zipfile"
	print "-f/--file filename       pickle file - optional for backup, required for restore"
	print "-c/--changelist <#|all>  in backup mode, pickle specified changelist(s)"
	print "                         in restore mode, reapply changes to the specified changelist"
	print "                         defaults to 'default'"
	print "   --prompt				if using --backup, this asks where to backup (and makes suggestions)"
	print "                         if using --restore, this brings up a file open dialog"
	print ""
	print "backup options:"
	print ""
	print "  --bins                 pickle binaries from the auto-checkout changelist"
	print "  --revert               in backup mode, revert open files after pickling"
	print "  -e/--exclude <globpat> a glob pattern of files to exclude from pickling"
	print "  -i/--include <globpat> a glob pattern of files to include in the code pickle"
	print ""
	print "restore options:"
	print ""
	print "  --changetree <src> <dst> allows you to restore to a Perforce tree different from the original one"
	print ""
	print "diff options:"
	print ""
	print "  --diffdir <directory>    specify where the diff files will go"
	print ""
	print 'ex: vcodepickle.py --backup --changelist all --exclude "bin/*"'
	print '    creates a zip file of all files not matching "bin/*" in all changelists'	
	print ""
	print "ex: vcodepickle.py --restore --file vcodepickle_2009_01_20.zip -c 342421"
	print "    restore from the specified file into changelist 342421"
	print ""
	print "ex: vcodepickle.py --diff --file VCP_2009-03-26__17h-01m-25s.zip"
	print "    creates a diff of the pickled changes in the specified file"
	sys.exit( 1 )


def DefaultPickleDirName( username ):
	return '\\\\fileserver\\user\\' + username + '\\vcodepickles'


def DefaultPickleDiffName( username ):
	return '\\\\fileserver\\user\\' + username + '\\vcodepicklediff'

def CreateChangelist( comment ):
	po = subprocess.Popen( 'p4 change -i', shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE )
	po.stdin.write( 'Change: new\n\nDescription:\n\t' + comment.replace('\n','\n\t') + '\n\n' )
	po.stdin.close()
	theOutput = po.stdout.readline()
	ret = po.wait()
	if ret == 0:
		return int( theOutput.split( ' ' )[1] )
	else:
		print "Creating a changelist failed."
		print theOutput
		sys.exit( 1 )

def GetChangelistComment( nChangelist ):
	po = subprocess.Popen( 'p4 change -o ' + nChangelist, shell=True, stdout=subprocess.PIPE )
	lines = po.stdout.readlines()
	ret = po.wait()
	if ret != 0:
		print "Couldn't get changelist description for change " + nChangelist
		sys.exit( 1 )
	
	comment = ''
	bFound = False
	for line in lines:
		if bFound == True:
			if line[0] == '\t':
				comment += line[1:]
			else:
				break
		else:
			if line[:12] == 'Description:':
				bFound = True
	
	return comment	


def chomp( str ):
	while 1:
		str2 = str.lstrip( '\n' ).lstrip( '\r' ).rstrip( '\n' ).rstrip( '\r' )
		if len( str2 ) == len( str ):
			return str
		else:
			str = str2


def AddQueuedWarning( s ):
	global g_QueuedWarnings
	g_QueuedWarnings.append( s )
	
def PrintQueuedWarnings():
	global g_QueuedWarnings
	if len( g_QueuedWarnings ) > 0:
		print "\n\n===== WARNINGS =====\n"
		nWarning = 1
		for warning in g_QueuedWarnings:
			print "%d:\n%s\n" % (nWarning, warning)
			nWarning += 1


def DoesFileExistInPerforce( filename ):
	x = p4helpers.ReadPerforceOutput( 'p4 -G fstat ' + filename )
	return (x[1][0]['code'] != 'error')

#
# The settings file is just a bunch of key/value pairs on different lines separated by =
#
def LoadSettingsFile( filename ):
	ret = {}
	try:
		f = open( filename, 'rt' )
	except IOError:
		return {}
		
	for line in f.readlines():
		line = chomp( line )
		parts = line.split( '=' )
		if len( parts ) > 0:
			ret[parts[0]] = parts[1]
	f.close()
	return ret;
	
def SaveSettingsFile( filename, thedict ):
	f = open( filename, 'wt' )
	for (k,v) in thedict.items():
		f.write( '%s=%s\n' % (k, v) )
	f.close()


def SetZipFilename( filename ):
	global g_ZipFilename
	g_ZipFilename = filename
	if g_ZipFilename[-4:].lower() != '.zip':
		g_ZipFilename += '.zip'


def GetReadableChangelistComment( str ):
	alphabet = 'abcdefghijklmnopqrstuvwxyz'

	# Strip out all lists of repeating nonalphabetic characters
	tempstr = ''
	prevchar = ''
	for i in range(0,len(str)):
		testchar = str[i:i+1]
		if testchar != prevchar or alphabet.find(testchar) != -1:
			tempstr += testchar
		prevchar = testchar
	
	str = tempstr
	str = str[:100]
	
	outstr = ''
	validchars = alphabet + alphabet.upper() + '0123456789_'
	prevchar = ''
	for i in range(0,len(str)):
		testchar = str[i:i+1]
		if validchars.find( testchar ) == -1:
			if prevchar != '-':
				outstr += '-'
		else:
			outstr += testchar
		prevchar = outstr[-1:]
	
	return outstr


def ShowUIForFilename():
	global g_Settings
	global g_SettingsFilename
	global g_ZipFilename
	global g_DefaultFilename
	
	#
	# Get the directory to save to.
	#
	username = os.environ['username']
	defaultdirname = DefaultPickleDirName(username)

	if not g_Settings.has_key( g_UISaveDirKey ):
		g_Settings[g_UISaveDirKey] = defaultdirname
	
	if g_Settings[g_UISaveDirKey] == 'default':
		theinput = raw_input( '\nDirectory\n=========\n(Leave blank for "%s")\n> ' % ( defaultdirname ) )
	else:
		theinput = raw_input( '\nDirectory\n=========\n(Leave blank    for "%s")\n(Type "default" for "%s"\n--> ' % ( g_Settings[g_UISaveDirKey], defaultdirname ) )

	if theinput == '':
		theDirName = g_Settings[g_UISaveDirKey]
	else:
		g_Settings[g_UISaveDirKey] = theinput
		theDirName =  theinput
	
	if theDirName.lower() == 'default':
		theDirName = defaultdirname

	#
	# Make sure it exists.. create it if necessary.
	#
	if os.access( theDirName, os.F_OK ) != True:
		theinput = raw_input( "Directory %s doesn't exist. Create? (Y/n) " % theDirName )
		if theinput == '' or theinput.lower() == 'y':
			os.makedirs( theDirName )
		else:
			print "Exiting."
			sys.exit( 1 )
	
	#
	# Get the filename to save to.
	#
	theinput = raw_input( '\nFilename\n========\n(Leave blank for "%s")\n> ' % ( g_DefaultFilename ) )
	
	if theinput == '':
		theZipFilename = g_DefaultFilename
	else:
		theZipFilename = theinput

	#
	# Construct the actual zip filename we'll use.
	#		
	SetZipFilename( os.path.join( theDirName, theZipFilename ) )			

	
	#
	# Check if it already exists.
	#
	if os.access( g_ZipFilename, os.F_OK ) == True:
		theinput = raw_input( "%s already exists. Overwrite? (y/N)" % g_ZipFilename )
		if theinput == '' or theinput.lower() == 'n':
			print "Exiting."
			sys.exit(1)

	# 
	# Save the settings back out.
	#
	SaveSettingsFile( g_SettingsFilename, g_Settings )



class VCodePickle:
	theZipFile = None
	filesDeleted = {}
	filesAdded = {}
	filesEdited = {}
	changelistComment = ''
	relativeToDepotFileMap = {}	# Maps src\engine\cl_main.cpp to //valvegames/main/src/engine/cl_main.cpp (obviously //valvegames/main/src changes based on the root)
	
	def Load( self, zipFilename ):
		self.theZipFile = zipfile.ZipFile( zipFilename, 'r', zipfile.ZIP_DEFLATED )

		# Load the added and deleted lists.
		changelistinfo = self.theZipFile.read( g_ChangelistInfoFilename + '.TXT' )
		(self.filesDeleted, self.filesAdded, self.filesEdited) = pickle.loads( changelistinfo )
		
		self.changelistComment = 'vcodepickle: ' + os.path.basename(g_ZipFilename)
		
		# Load version2 data.
		try:
			ci2 = pickle.loads( self.theZipFile.read( g_ChangelistInfoFilename + 'v2.TXT' ) )
			self.changelistComment = ci2.changelistComment
		except KeyError:
			pass
		
		# Load version3 data
		try:
			ci3 = pickle.loads( self.theZipFile.read( g_ChangelistInfoFilename + 'v3.TXT' ) )
			self.relativeToDepotFileMap = ci3.relativeToDepotFileMap
		except KeyError:
			pass



##
# Given a 'zip' instance, copy data from the 'name' to the
# 'out' stream.

def explode( out, zip, name ):
	zinfo = zip.getinfo(name)

	stringFileHeader = "PK\003\004"   # magic number for file header
	structFileHeader = "<4s2B4HlLL2H"  # 12 items, file header record, 30 bytes
	_FH_FILENAME_LENGTH = 10
	_FH_EXTRA_FIELD_LENGTH = 11

	filepos = zip.fp.tell()
	zip.fp.seek(zinfo.header_offset, 0)

	# Skip the file header:
	fheader = zip.fp.read(30)
	if fheader[0:4] != stringFileHeader:
		raise zipfile.BadZipfile, "Bad magic number for file header"
		
	fheader = struct.unpack(structFileHeader, fheader)
	fname = zip.fp.read(fheader[_FH_FILENAME_LENGTH])
	if fheader[_FH_EXTRA_FIELD_LENGTH]:
		zip.fp.read(fheader[_FH_EXTRA_FIELD_LENGTH])

	if fname != zinfo.orig_filename:
		raise zipfile.BadZipfile, 'File name in directory "%s" and header "%s" differ.' % ( zinfo.orig_filename, fname)

	if zinfo.compress_type == zipfile.ZIP_STORED:
		decoder = None
	elif zinfo.compress_type == zipfile.ZIP_DEFLATED:
		if not zlib:
			raise RuntimeError,  "De-compression requires the (missing) zlib module"
		decoder = zlib.decompressobj(-zlib.MAX_WBITS)
	else:
		raise zipfile.BadZipfile,"Unsupported compression method %d for file %s" % (zinfo.compress_type, name)

	size = zinfo.compress_size
	
	while 1:
		data = zip.fp.read(min(size, 8192))
		if not data:
			break
		size -= len(data)
		if decoder:
			data = decoder.decompress(data)
		out.write(data)
	
	if decoder:
		out.write(decoder.decompress('Z'))
		out.write(decoder.flush())

	zip.fp.seek(filepos, 0)



def WriteFileFromZip( theZipFile, srcFilename, sDestFilename ):
#	thebytes = theZipFile.read( srcFilename )
	
	try:
		f = open( sDestFilename, 'wb')
	except:
		os.makedirs( os.path.dirname( sDestFilename ) )
		f = open( sDestFilename, 'wb' )

	explode( f, theZipFile, srcFilename );
#	f.write( thebytes )
	f.close()


def PerforceToZipFilename( sP4Filename ):
	if sP4Filename[0] != '/' or sP4Filename[1] != '/':
		print "PerforceToZipFilename: invalid filename (%s)" % (sP4Filename)
	
	return sP4Filename[2:]


def CheckFilesOpenedForEdit( thePickle, theDict ):
	ret = []
	for k in theDict.keys():
		sDepotFilename = thePickle.relativeToDepotFileMap[k]
		if p4helpers.GetClientFileAction( sDepotFilename ) != 'none':
			ret.append( sDepotFilename )
	
	return ret
	


def HandleNewTree( thePickle ):
	global g_FromTree, g_ToTree

	if g_FromTree == None:
		return
		
	print "-changetree is translating these files:\n"
	
	for k in thePickle.relativeToDepotFileMap.keys():
		sFromFilename = thePickle.relativeToDepotFileMap[k]
		
		if sFromFilename[:len(g_FromTree)].lower() == g_FromTree.lower():
			sToFilename = g_ToTree + sFromFilename[len(g_FromTree):]
			
			print sFromFilename + "\n\t-> " + sToFilename + "\n"
			
			thePickle.relativeToDepotFileMap[k] = sToFilename
		else:
			print "Used -changetree [%s] [%s], but file %s does not come from the source tree." % (g_FromTree, g_ToTree, sFilename)
			sys.exit( 1 )

	# Translate the revision number to a changelist number for edited files.
	newEdited = {}
	for k in thePickle.filesEdited.keys():
		thePickle.filesEdited[k] = "head"
		
		sDepotFilename = thePickle.relativeToDepotFileMap[k]
		if DoesFileExistInPerforce( sDepotFilename ):
			newEdited[k] = thePickle.filesEdited[k]
		else:
			AddQueuedWarning( "%s was edited in the old tree, but doesn't exist in the new tree. This was treated as an add instead." % sDepotFilename )
			thePickle.filesAdded[k] = thePickle.filesEdited[k]
	
	thePickle.filesEdited = newEdited
	
	print "\n"
	
	
def PromptForOpenZipFilename( sClientRoot, promptDir ):
	if not sys.modules.has_key( "wx" ):
		print >>sys.stderr, "Couldn't load wx, no UI available."
		sys.exit( 1 )
		
	temp_app = wx.PySimpleApp()
	dialog = wx.FileDialog ( None, style = wx.OPEN )
	dialog.SetMessage( "Select code pickle" );
	dialog.SetDirectory(promptDir)
	dialog.SetWildcard( "VCodePickle Zips (*.zip)|*.zip|All Files (*.*)|*.*" );
	
	if dialog.ShowModal() == wx.ID_OK:
		print 'Selected: %s\n' % (dialog.GetPath())
		return dialog.GetPath()
	else:
		print 'Cancelled.'
		return None
   
   
def DoRestore( sClientRoot, sZipFilename ):
	global g_bBinsOnly
	thePickle = VCodePickle()
	thePickle.Load( sZipFilename )
	
	# First make sure they don't have any files open for edit that we want to mess with.	
	print "Checking the code pickle...\n"
	
	openedForEdit = CheckFilesOpenedForEdit( thePickle, thePickle.filesAdded ) + CheckFilesOpenedForEdit( thePickle, thePickle.filesEdited )
	if len( openedForEdit ) > 0 and g_bBinsOnly==False:
		print "You have one or more files open locally that are in the vcodepickle you are trying to restore:\n"
		for f in openedForEdit:
			print "\t" + f
		print "\nPlease revert the specified file(s) and retry the vcodepickle command you were doing."
		return

	changelistNum = None
	if ( g_bBinsOnly == False ):
		changelistNum = CreateChangelist( thePickle.changelistComment )
	else:
		changelistNum = 'default'
	
	HandleNewTree( thePickle )

	if g_bVerbose: print "Adding %d files..." % len( thePickle.filesAdded )
	for k in thePickle.filesAdded.keys():
		sDepotFilename = thePickle.relativeToDepotFileMap[k]

		if DoesFileExistInPerforce( sDepotFilename ):
			sLocalClientFilename = p4helpers.GetClientFileInfo( sDepotFilename )[0]
			AddQueuedWarning( "%s was added in the code pickle, but already exists in Perforce. We'll treat it as an edit." % sLocalClientFilename );
			
			p4helpers.CheckPerforceReturn( 'p4 sync %s' % (sDepotFilename) )
			p4helpers.CheckPerforceReturn( 'p4 edit -c %s %s' % (changelistNum, sDepotFilename) )
			WriteFileFromZip( thePickle.theZipFile, k.replace('\\','/'), sLocalClientFilename )
		else:
			# Note: We do the "p4 add" first because we don't yet know the local filename for this depot filename,
			# and there's no way to get it if the file doesn't already exist.
			#
			# Perforce will let you do a "p4 add" on a file that doesn't exist in your local tree yet, then you can
			# do a "p4 fstat" (p4helpers.GetClientFileInfo) on it to find its local filename.
			sCmd = 'p4 add -c %s %s' % (changelistNum, sDepotFilename)
			p4helpers.CheckPerforceReturn( sCmd )
			
			# Get the local filename and copy from the zip file to that.
			sLocalClientFilename = p4helpers.GetClientFileInfo( sDepotFilename )
			if sLocalClientFilename == None:
				AddQueuedWarning( "Unable to add %s. It probably isn't in your client spec." % sDepotFilename )
			else:
				sLocalClientFilename = sLocalClientFilename[0]
				WriteFileFromZip( thePickle.theZipFile, k.replace('\\','/'), sLocalClientFilename )
				
			if g_bVerbose: print "\t" + k

	if g_bVerbose: print "Deleting %d files..." % len( thePickle.filesDeleted )
	for k in thePickle.filesDeleted.keys():
		sDepotFilename = thePickle.relativeToDepotFileMap[k]
		p4helpers.CheckPerforceReturn( 'p4 delete -c %s %s ' % (changelistNum, sDepotFilename) )
		if g_bVerbose: print "\t" + k

	if g_bVerbose: print "Editing %d files..." % len( thePickle.filesEdited )
	for k in thePickle.filesEdited.keys():
		revisionNumber = thePickle.filesEdited[k]
		sDepotFilename = thePickle.relativeToDepotFileMap[k]
		sLocalClientFilename = p4helpers.GetClientFileInfo( sDepotFilename )[0]

		# the zipfiles like forward slashes
		internalfilename = k.replace('\\','/')
		
		# Sync to the revision they had it at, edit, copy the file over, and sync to the latest so Perforce forces a resolve.
		p4helpers.CheckPerforceReturn( 'p4 sync %s#%s' % (sDepotFilename, revisionNumber) )
		p4helpers.CheckPerforceReturn( 'p4 edit -c %s %s' % (changelistNum, sDepotFilename) )
		WriteFileFromZip( thePickle.theZipFile, internalfilename, sLocalClientFilename )
		p4helpers.CheckPerforceReturn( 'p4 sync %s' % sDepotFilename )
		if g_bVerbose: print "\t%s#%s" % (k, revisionNumber)
		
	thePickle.theZipFile.close()

	PrintQueuedWarnings()
			
	if g_FromTree != None:
		print "\n"
		print "    WARNING!!!!! -changetree edited the HEAD revision of the files in new tree."
		print "    So if they have been edited since you checked them out for your"
		print "    code pickle, then you must MANUALLY merge those changes into the new version."


def SetupBinChangelists():
	global g_Changelist	
	clientInfo = p4helpers.GetClientInfo()
	pendingChanges = p4helpers.GetPendingChanges( clientInfo[ 'Client' ] )
	for changeList in pendingChanges:
		if ( changeList[ 'desc' ].find( 'Visual Studio Auto Checkout' ) <> -1 ):
			g_Changelist.append( changeList[ 'change' ] )

def AutoFillChangelistArray():
	global g_bBinsOnly
	global g_RunMode
	global g_bPickleAllChanges
	global g_Changelist

	if ( g_bBinsOnly and g_RunMode == RUNMODE_BACKUP ):
		SetupBinChangelists()
	
	elif g_bPickleAllChanges:
		clientInfo = p4helpers.GetClientInfo()
		pendingChanges = p4helpers.GetPendingChanges( clientInfo[ 'Client' ] )
		for changeList in pendingChanges:
			g_Changelist.append( changeList['change'] )
			
		g_Changelist.append( 'default' )
		
	elif len( g_Changelist ) == 0 and g_RunMode == RUNMODE_BACKUP:
		g_Changelist.append( 'default' )


def CheckFileExcludedByPatterns( sFilename ):
	global g_IncludeGlobPatterns
	global g_ExcludeGlobPatterns

	bInclude = ( len( g_IncludeGlobPatterns ) == 0 ) # include by default
	for includePattern in g_IncludeGlobPatterns:
		if glob.fnmatch.fnmatch( sFilename, includePattern ):
			bInclude = True
			break
	if not bInclude:
		if g_bVerbose: print "skipping %s, doesn't match any include pattern" % sFilename
		return True

	for excludePattern in g_ExcludeGlobPatterns:
		if glob.fnmatch.fnmatch( sFilename, excludePattern ):
			if g_bVerbose: print "skipping %s matches exclude pattern %s" % ( sFilename, excludePattern )
			return True
			
	return False



def DoBackup( sClientRoot, sZipFilename, bAutoRevert ):
	global g_Changelist
	global g_ChangelistInfoFilename
	global g_ChangelistComment

	filesDeleted = {}
	filesAdded = {}
	filesEdited = {}

	#
	# Filter out all the files that aren't of the changelist we want.
	#
	openedFiles = p4helpers.ReadPerforceOutput( 'p4 -G opened' )[1]
	changelistfiles = [x for x in openedFiles if ( x['change'] in g_Changelist ) ]

	# Ok, there's something to do. Open the zip file.
	theZipFile = zipfile.ZipFile( sZipFilename, 'w', zipfile.ZIP_DEFLATED )

	#
	#
	#
	relativeToDepotFileMap = {}
	
	for openedFile in changelistfiles:
		# Get the local client filename.
		depotFile = openedFile['depotFile']		# (//valvegames/main/src/etc....)
		x = p4helpers.GetClientFileInfo( depotFile )
		
		localFilename = x[0]
		sFriendlyName = localFilename[ len(sClientRoot)+1: ]	# ONLY for printing messages
		
		# Get the filename in the zipfile. We're just going to use the depot filename and make it palatable to the zipfile.
		# You could easily restore the depot filename from this filename, but for now we're just looking it up in relativeToDepotFileMap on the receiving end.
		sFilenameInZip = PerforceToZipFilename( depotFile )

		# Check include and exclude patterns and skip this file if necessary.
		if CheckFileExcludedByPatterns( sFilenameInZip ):
			continue
		
		relativeToDepotFileMap[sFilenameInZip] = depotFile

		# Don't do anything if it's a delete.
		action = x[2]
		
		bCopy = True
		if action == 'delete':
			filesDeleted[sFilenameInZip] = 1
			bCopy = False
			print "%s (DELETE)" % (sFriendlyName)
		else:
			if os.access( localFilename, os.F_OK ) == True:
				theZipFile.write( localFilename, sFilenameInZip )
			else:
				print "\n**\n** WARNING: File '%s' is in the changelist but not on disk. It will not be in the zipfile.\n**\n" % sFriendlyName
			
			if action == 'add':
				filesAdded[sFilenameInZip] = 1
				print "%s (ADD)" % (sFriendlyName)
			else:
				filesEdited[sFilenameInZip] = openedFile['rev']
				print "%s#%s" % (sFriendlyName, openedFile['rev'])
	
		# Revert the file..
		if bAutoRevert == True:
			p4helpers.CheckPerforceReturn( 'p4 revert ' + localFilename )


	# Save the metadata about add/delete/edit and file revisions.
	data = pickle.dumps( [filesDeleted, filesAdded, filesEdited] )
	theZipFile.writestr( g_ChangelistInfoFilename + '.TXT', data )
	
	# Save v2 data.
	ci2 = CV2Data()
	ci2.changelistComment = g_ChangelistComment
	data = pickle.dumps( ci2 )
	theZipFile.writestr( g_ChangelistInfoFilename + 'v2.TXT', data )
	
	# Save v3 data.
	ci3 = CV3Data()
	ci3.relativeToDepotFileMap = relativeToDepotFileMap
	data = pickle.dumps( ci3 )
	theZipFile.writestr( g_ChangelistInfoFilename + 'v3.TXT', data )
	

	# 
	# Write a human-readable changelist file.
	#
	theZipFile.writestr( 'ReadableChangelist.txt', g_ChangelistComment )
	
	theZipFile.close()


def DoEmptyPickle( sZipFilename ):
	global g_ChangelistComment

	filesDeleted = {}
	filesAdded = {}
	filesEdited = {}

	relativeToDepotFileMap = {}

	# Ok, there's something to do. Open the zip file.
	theZipFile = zipfile.ZipFile( sZipFilename, 'w', zipfile.ZIP_DEFLATED )

	# Save the metadata about add/delete/edit and file revisions.
	data = pickle.dumps( [filesDeleted, filesAdded, filesEdited] )
	theZipFile.writestr( g_ChangelistInfoFilename + '.TXT', data )
	
	# Save v2 data.
	ci2 = CV2Data()
	ci2.changelistComment = g_ChangelistComment
	data = pickle.dumps( ci2 )
	theZipFile.writestr( g_ChangelistInfoFilename + 'v2.TXT', data )
	
	# Save v3 data.
	ci3 = CV3Data()
	ci3.relativeToDepotFileMap = relativeToDepotFileMap
	data = pickle.dumps( ci3 )
	theZipFile.writestr( g_ChangelistInfoFilename + 'v3.TXT', data )
	

	# 
	# Write a human-readable changelist file.
	#
	theZipFile.writestr( 'ReadableChangelist.txt', g_ChangelistComment )
	
	theZipFile.close()



def DoDiff( sZipFilename, sDiffDir ):
	# Load the zipfile.
	thePickle = VCodePickle()
	thePickle.Load( sZipFilename )

	sTempFilesDir = os.path.join( sDiffDir, "tempfiles" )

	# Get rid of the old directory.
	if os.access( sDiffDir, os.F_OK ) == True:
		theinput = raw_input( "Warning: This will clear out %s. Proceed (y/N)? " % sDiffDir )
		if theinput == '' or theinput.lower() == 'n':
			print "Exiting."
			sys.exit( 0 )
		
		# Delete all the files.
		# This method is lame but if we use unlink() on a network path, you won't be able to delete them
		# next time around or even from that command line. You can delete them from an explorer window, but 
		# not from the command prompt anymore. Strange.
		os.system( 'rd /s /q "%s"' % sDiffDir )
				
	# Create the directories.
	os.makedirs( sTempFilesDir )
	
	# Dump out the changelist description.
	f = open( os.path.join( sDiffDir, "changelist.txt" ), 'wt' )
	f.write( thePickle.changelistComment )
	f.close()	
	
	# Dump out all the files.
	for sFilename in thePickle.filesEdited.keys():
		# The zipfile utils like forward slashes.
		sInternalFilename = sFilename.replace('\\','/')
		
		# Write the file to tempfiles\basefilename
		sEditedFilename = os.path.join( sTempFilesDir, os.path.basename( sInternalFilename ) )
		WriteFileFromZip( thePickle.theZipFile, sInternalFilename, sEditedFilename )
		
		# Have Perforce write out the prior version.
		nFileRevision = int( thePickle.filesEdited[sFilename] )
		sPriorFilename = os.path.join( sTempFilesDir, os.path.basename( sInternalFilename ) ) + '.prev'
		p4helpers.ReadPerforceOutput( 'p4 -G print -o %s %s#%d' % (sPriorFilename, thePickle.relativeToDepotFileMap[sFilename], nFileRevision) )
		
		# Write out a batch file with the diff command.
		sBatchFilename = os.path.join( sDiffDir, sInternalFilename.replace( '/', '-' ) ) + '.bat'
		fp = open( sBatchFilename, 'wt' )
		sPercent = '%'
		fp.write( """		
			@echo off
			setlocal
			""" )
		fp.write( "set sPriorFilename=%s\n" % sPriorFilename )
		fp.write( "set sEditedFilename=%s\n" % sEditedFilename )

		fp.write( """		
			rem // ------------------------------------------------------------------------------------------------------------------------ //
			rem // First, check if a P4DIFF environment variable exists.
			rem // ------------------------------------------------------------------------------------------------------------------------ //
			set diffProg=%P4DIFF%
			if "%diffprog%" == "" (
				goto CheckRegistry
			)

			:DoDiff
			"%diffProg%" %sPriorFilename% %sEditedFilename%
			goto End

			rem // ------------------------------------------------------------------------------------------------------------------------ //
			rem // No P4DIFF? Ok, check for what P4Win has setup as its diff program.
			rem // The nasty for command there basically runs the reg command, does away with stderr output, pipes the output
			rem // to FINDSTR to strip out everything but the second line which contains the result we want. Then it takes the relevant
			rem // part of the end of that line, which is what we care about.
			rem // ------------------------------------------------------------------------------------------------------------------------ //
			:CheckRegistry
			for /F "tokens=2* delims=REG_SZ" %%a in ('reg query HKEY_CURRENT_USER\Software\Perforce\P4win\Options /v DiffApp 2^>nul ^| FINDSTR DiffApp') do set initialValue=%%a
			if "%initialValue%" == "" (
				set diffProg=p4diff.exe
			) else (
				for /F "tokens=*" %%s in ("%initialValue%") do set diffProg=%%s
			)
			goto DoDiff
			
			:End

			""" )
			
		# Now the batch file references the diff program. Just add the arguments we care about.
		fp.close()
		
		print "Wrote %s" % sBatchFilename
	
	thePickle.theZipFile.close()
	
	# Open up that directory.
	os.system( 'start %s' % sDiffDir )

	


def main():
	global g_bVerbose
	global g_RunMode
	global g_ZipFilename
	global g_bAutoRevert
	global g_bBinsOnly
	global g_bPickleAllChanges
	global g_Changelist
	global g_IncludeGlobPatterns
	global g_ExcludeGlobPatterns
	global g_ChangelistInfoFilename
	global g_ChangelistComment
	global g_DiffDir
	global g_bShowUI
	global g_DefaultFilename
	global g_bCreateDiff
	
	
	#
	# Parse out the command line.
	#
	try:
		opts, args = getopt.getopt( sys.argv[1:], "bdrf:c:ri:e:?q",
									[ "prompt", "diff", "diffdir=", "backup", "restore", "file=", "bins", "changelist=", "revert", "include=", "exclude=", "quiet", "help", "empty" ] )
	except getopt.GetoptError, e:
		print ""
		print "Argument error: ", e
		print ""
		ShowUsage()
		sys.exit(1)

	bPrompt = False
	for o, a in opts:
		if o in ( "-?", "--help" ):
			ShowUsage()
			sys.exit(1)
		if o in ( "-q", "--quiet" ):
			g_bVerbose = False
		if o in ( "--prompt" ):
			bPrompt = True
		if o in ( "-b", "--backup" ):
			assert( g_RunMode == RUNMODE_NONE )
			g_RunMode = RUNMODE_BACKUP
		if o in ( "-r", "--restore" ):
			assert( g_RunMode == RUNMODE_NONE )
			g_RunMode = RUNMODE_RESTORE
		if o in ( "-e", "--empty" ):
			assert( g_RunMode == RUNMODE_NONE )
			g_RunMode = RUNMODE_EMPTY
		if o in ( "-d", "--diff" ):
			g_bCreateDiff = True
		if o.lower() == "--diffdir" :
			g_DiffDir = a
		if o in ( "-f", "--file" ):
			g_ZipFilename = a
			if g_ZipFilename[-4:].lower() != '.zip':
				g_ZipFilename += '.zip'
		if o in ( "-c", "--changelist" ):
			if a in ( 'all', '*' ):
				g_bPickleAllChanges = True
			else:
				g_Changelist.append( a )
		if o.lower() == "--bins":
			g_bBinsOnly = True
		if o in ( "--revert" ):
			g_bAutoRevert = True
		if o in ( "-i", "--include" ):
			g_IncludeGlobPatterns.append( a )
		if o in ( "-e", "--exclude" ):
			g_ExcludeGlobPatterns.append( a )

	if g_RunMode == RUNMODE_NONE and not g_bCreateDiff:
		ShowUsage()
		sys.exit( 1 )

	if ( g_RunMode == RUNMODE_RESTORE or g_RunMode == RUNMODE_EMPTY ) and len( g_ZipFilename ) == 0 and not bPrompt:
		print >>sys.stderr, "\n*** file parameter is required in restore/empty mode ***\n\n\n"
		ShowUsage()
		sys.exit( 1 )

	if len(g_Changelist) and g_bBinsOnly and g_RunMode == RUNMODE_BACKUP:
		print >>sys.stderr, "\n*** -c and --bins are mutally exclusive ***\n\n\n"
		ShowUsage()
		sys.exit( 1 )

	if ( len( g_IncludeGlobPatterns ) or len( g_ExcludeGlobPatterns ) ) and g_RunMode != RUNMODE_BACKUP:
		print >>sys.stderr, "\n*** -i/-e apply only in backup mode ***\n\n\n"
		ShowUsage()
		sys.exit(1)
			
	g_Settings = LoadSettingsFile( g_SettingsFilename )

	# Auto-fill g_Changelist if necessary.
	AutoFillChangelistArray()
		

	if g_RunMode == RUNMODE_BACKUP:
		#
		# Get the string for the changelist.
		#
		sDate = datetime.datetime.now().strftime("%Y-%m-%d__%Hh-%Mm-%Ss")
		if g_Changelist[0] == 'default':
			g_ChangelistComment = 'vcodepickle: ' + os.path.basename(g_ZipFilename)
			g_DefaultFilename = 'VCP_' + sDate + '.zip'
		else:
			g_ChangelistComment = GetChangelistComment( g_Changelist[0] )
			partOfChangelistComment = GetReadableChangelistComment( g_ChangelistComment )
			g_DefaultFilename = 'VCP_' + partOfChangelistComment + '_' + sDate + '.zip'

		#
		# Generate a default zip filename if they didn't specify one. 
		#
		# Either we'll use a mixture of the current date and their changelist text
		# and if it was launched from a Perforce tool (used the -ui) parameter,
		# we'll let them specify where to save it (and default to \\fileserver\user\username\[the date and the changelist comment])
		#
		if bPrompt == True:
			ShowUIForFilename()
		else:
			#
			# Non-ui mode. Just use what they entered on the command line or use the default if they didn't enter anything.
			#
			if len( g_ZipFilename ) == 0:
				SetZipFilename( g_DefaultFilename )

		
	g_ClientInfo = p4helpers.GetClientInfo()

	# Get the root directory without a trailing slash.
	g_ClientRoot = g_ClientInfo['Root']
	if g_ClientRoot[-1:] == '\\' or g_ClientRoot[-1:] == '/':
		g_ClientRoot = g_ClientRoot[:-1]

	if g_bVerbose:
		print "\n---- vcodepickle ----"
		print "zipfile        : " + g_ZipFilename
		print "p4 client root : " + g_ClientRoot
		if g_bAutoRevert == True:
			print "-revert specified"
		print "-----------------------"
		print ""



	if g_RunMode == RUNMODE_BACKUP:
		DoBackup( g_ClientRoot, g_ZipFilename, g_bAutoRevert )
	if g_RunMode == RUNMODE_EMPTY:
		DoEmptyPickle( g_ZipFilename )
	
	elif g_RunMode == RUNMODE_RESTORE:
		if bPrompt:
			username = os.environ['username']
			g_ZipFilename = PromptForOpenZipFilename( g_ClientRoot, DefaultPickleDirName(username) )
			if g_ZipFilename != None:
				DoRestore( g_ClientRoot, g_ZipFilename )
		else:
			DoRestore( g_ClientRoot, g_ZipFilename )
	
	if g_bCreateDiff:
		print "\n\nPreparing diff...\n"
		username = os.environ['username']
		if g_DiffDir == '':
			g_DiffDir = DefaultPickleDiffName(username)
				
		if bPrompt and ( g_RunMode != RUNMODE_BACKUP ):	# If RUNMODE_BACKUP, then g_ZipFilename is already set to a valid thing.
			g_ZipFilename = PromptForOpenZipFilename( g_ClientRoot, DefaultPickleDirName(username) )
			if g_ZipFilename != None:
				DoDiff( g_ZipFilename, g_DiffDir )
		else:
			DoDiff( g_ZipFilename, g_DiffDir )



if __name__ == '__main__':
	main()


