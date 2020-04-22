
import subprocess, marshal, os, stat, sys


g_bPerforceVerbose = False


# Make all lowercase and forward slashes.
def FixFilename( f ):
	return f.replace( '\\', '/' ).lower()



def SetPerforceVerbose( bVerbose ):
	global g_bPerforceVerbose
	g_bPerforceVerbose = bVerbose


def CheckPerforceReturn( cmd ):
	po = subprocess.Popen( cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE )
	sStdout = po.stdout.read()
	sStderr = po.stderr.read()
	ret = po.wait()
	if ret != 0:
		print >>sys.stderr, "A command returned %d: %s\nstdout = %s\nstderr = %s" % (ret, cmd, sStdout, sStderr)
		sys.exit( ret )
	


def ReadPerforceOutput( cmd, bCheckReturn=True ):
	if g_bPerforceVerbose:
		print "Running: " + cmd
		
	po = subprocess.Popen( cmd, shell=True, stdout=subprocess.PIPE )
	
	results = []
	try:
		while True:
			try:
				entry = marshal.load(po.stdout)
			except ValueError:
				print '----------------------------------------------'
				print 'Marshal.load(po.stdout) got ValueError'
				print 'Next data:'
				print po.stdout.readline()
				print po.stdout.readline()
				print po.stdout.readline()
				print '----------------------------------------------'
				raise

			results.append(entry)
	except EOFError:
		pass

	ret = ( po.wait(), results )
	
	# Check the return value?
	if bCheckReturn and ret[0] != 0:
		print >>sys.stderr, "A command returned %d: %s" % (ret[0], cmd)
		sys.exit( 1 )

	return ( ret )
	


# Get the list of files. These are returned in a dictionary where the keys are the
# FixFilename'd filenames (lowercase and forward-slashes-only) and the values are 
# the non-lowercased version (which you need to send to Linux).
def GetP4OpenedFiles( perforceRoot, cmd ):
	if perforceRoot != None:
		perforceRoot = FixFilename( perforceRoot )
		
	(x,files) = ReadPerforceOutput( cmd, bCheckReturn=True )

	srcfiles = {}
	depotFiles = [ (x['depotFile'], x['action']) for x in files]
	for (perforceFilename,action) in depotFiles:
		# For now, just ignore delete commands. This means they won't get mirrored over 
		# to the remote end.
		if action == 'delete':
			continue
			
		fixed = FixFilename( perforceFilename )
		if len(fixed) == 0:
			break
		
		if perforceRoot == None or fixed.startswith( perforceRoot ):
			srcfiles[fixed] = perforceFilename
	
	return srcfiles


# Returns 'add', 'edit', 'remove', or 'none' (which either means nothing's happening to it or that depot file doesn't exist).
def GetClientFileAction( sDepotFilename ):
	kv = ReadPerforceOutput( 'p4 -G fstat \"%s\"' % sDepotFilename )[1][0]
	if kv.has_key( 'action' ):
		return kv[ 'action' ]
	else:
		return 'none'
	

# Returns ( client filename, perforce filename, action [edit/add/remove] )
def GetClientFileInfo( perforceFilename ):
	kv = ReadPerforceOutput( 'p4 -G fstat \"%s\"' % perforceFilename )[1][0]
	
	try:
		ret = [ kv['clientFile'], kv['depotFile'] ]
	except KeyError:
		print >>sys.stderr, "\nGetClientFileInfo( %s ) failed.\nPerhaps your clientspec doesn't include this file?" % perforceFilename
		sys.exit( 1 )
		
	if kv.has_key( 'action' ):
		ret.append( kv['action'] )
	else:
		ret.append( 'none' )
		
	return ret


# Returns a dictionary with info from p4 client.
# Particularly interesting are 'Root' (client's root folder), 'Client' (client name)
def GetClientInfo():
	return ReadPerforceOutput( 'p4 -G client -o' )[1][0]


# Scan the directory tree and get filenames relative to the specified dir.
def GetFilenamesRelativeTo_R( dirname ):
	ret = []
	
	for f in os.listdir( os.path.join(dirname) ):
		if f[0] == '.':
			continue
		
		fullname = os.path.join( dirname, f )
		s = os.stat( fullname )
		if stat.S_ISDIR( s[stat.ST_MODE] ):
			names = GetFilenamesRelativeTo_R( fullname )
			ret.extend( names )
		else:
			ret.append( fullname )
	
	return ret

def GetFilenamesRelativeTo( dirname ):
	ret = GetFilenamesRelativeTo_R( dirname )
	return [ x[len(dirname)+1:] for x in ret ]
	
	
def GetPendingChanges( p4client, fileFilter = "" ):
	cmd = 'p4 -G changes -s pending -c ' + p4client
	if ( len(fileFilter) > 0 ):
		cmd += ' ' + fileFilter
	return ReadPerforceOutput( cmd )[1]

def P4Where( file ):
	cmd = 'p4 -G where %s' % file
	return ReadPerforceOutput( cmd )[1][0][ "depotFile" ]

def GetSyncedRevision( p4ClientRoot ):
	cmd = 'p4 -G changes -s submitted -m 1 %s/...' % p4ClientRoot
	return ReadPerforceOutput( cmd )[1][0]['change']
