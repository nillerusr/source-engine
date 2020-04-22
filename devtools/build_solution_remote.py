#!/usr/bin/env python

#
# build "my current enlistment" remotely 
# does a handful of steps:
# 1. makes sure the users has ssh key auth setup to the target host (as automatic as possible)
# 2. creates a p4 client on the remote host that's a mirror of their local client
# 3. sync's the remote client to the same (submitted) change as the local client
# 4. pickles up local changes, scp's them to the remote host, and applies them to the remote client
# 5. runs vpc and make, reporting status/errors to the user
#

import getopt
import getpass
import marshal
import os
import p4helpers
import stat
import sys
import subprocess
import tempfile

##### Don't merge this block to another branch, this section needs to be altered to match the requirements
#####   of the p4 branch you are merging into!!
g_Branch = "//ValveGames/staging"
# branchspec needs the leading \t to make the p4 formatting happy when making a changelist
g_BranchSpec = "\t//ValveGames/staging/src/... //%s/src/..."
g_VPCCommand = { 'linux' : "dedicated /dedicated /f /tf /dod /cstrike /hl2mp" , 'osx' : "port /tf /dod /portal /hl2 /episodic /hl2mp /cstrike" }  
#####
#### merge block ends
####

g_bVerbose = True
g_bDebugVerbose = False
g_bBatchMode = False
g_bSimulateOnly = False

g_lRemotePlatforms = [ 'linux', 'osx' ]
g_lBuildConfigs = [ 'debug', 'release' ]

g_szSSHBin = 'ssh'
g_szSCPBin = 'scp'
g_szVCodePickle = os.path.join( "src", "devtools", "vcodepickle.py" )
g_szMoreBin = 'less'
g_szSSHKeyDir = '.ssh'
g_szSSHAgentAddKeyCmd = 'ssh-add'
g_szCleanTarget = ''

if sys.platform == 'win32':
  # relative to the top of the enlistment
	g_szSSHBin = 'src\\devtools\\bin\\putty\\plink.exe'
	g_szSCPBin = 'src\\devtools\\bin\\putty\\pscp.exe'
	g_szMoreBin = '%WINDIR%\\notepad.exe'
	g_szSSHKeyDir = 'ssh'
	g_szSSHKeyConvertCmd = 'start /wait src\\devtools\\bin\\putty\\puttygen.exe'
	g_szSSHAgentAddKeyCmd = 'src\\devtools\\bin\\putty\\pageant.exe'
										  
g_mapPlatformToBuildHostInfo = { "linux" : { "hostname" : "linuxpiston.valvesoftware.com",
											 "sshkey"   : { "win32" : { "regkey" : "Software\\SimonTatham\\Putty\\SshHostKeys",
																		"valueName" : "rsa2@22:linuxpiston.valvesoftware.com",
																		"value" : "0x23,0xb9a05c086f2b8fd5ffcb5270f4d4479e2462459cf6716e297bcf15aef2696f29ac28661f45b8a427e1e9e224eba3a96ceb88f821fddfba950722bcd1bcb46fca3a1065f5e17270d1de605091cc77ee839acbcb8ac09e6c36d10f517f294e9b033bc52f969511cfc2157a2c711de8d4a54ef8eab46d6aed7b38b0ddf3a35dd2412b48ab91378415d9a01c5d0ff2423ea059d8c5d38b342c106cc860989c65607887b552d599a018baee98f382d03a733ad0c26a91d9366df3b43bab72ea030856a85721379e71521f38280aa9aad36455810e3ac1c9ea53a829a92a92c3d6dbb7ee075618c1931ff075798e284e6c7871588e64abc9719d159819cfb729fb517d" },
															"posix": "linuxpiston.valvesoftware.com ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAQEAuaBcCG8rj9X/y1Jw9NRHniRiRZz2cW4pe88VrvJpbymsKGYfRbikJ+Hp4iTro6ls64j4If3fupUHIrzRvLRvyjoQZfXhcnDR3mBQkcx37oOay8uKwJ5sNtEPUX8pTpsDO8UvlpURz8IVeixxHejUpU746rRtau17OLDd86Nd0kErSKuRN4QV2aAcXQ/yQj6gWdjF04s0LBBsyGCYnGVgeIe1UtWZoBi67pjzgtA6czrQwmqR2TZt87Q7q3LqAwhWqFchN55xUh84KAqpqtNkVYEOOsHJ6lOoKakqksPW27fuB1YYwZMf8HV5jihObHhxWI5kq8lxnRWYGc+3KftRfQ==" } },
								 "osx"   : { "hostname" : "macslave01.valvesoftware.com", 
											 "sshkey"   : { "win32" : { "regkey" : "Software\\SimonTatham\\Putty\\SshHostKeys",
																		"valueName" : "rsa2@22:macslave01.valvesoftware.com",
																		"value" : "0x23,0xc50ecfe57fef329f442a652566fc8734fcf42f7c20beb2593ee55ce05c432a5047887ee28d6f24f3c60c5d6daf582fcd8d074f20de467893ad858ffa6809cc59427cde2c86536bf6c98810dc374b7afceb49a69b607fb29ce7eaa28751fcd78d45b59b409e665800e91b09a0615b16118bfbfebb638ef63d9bdd23a66a1cda493e17d529b4d9fcf96a8c73c947e86a0a41936b236c798a35620af65ae50459f4602ef1434f06f596cffa8dbebf985e588335e00a9f67e52c97e9d9b0cf420a04e92f56c124f430be4dc72e701244bbf005d361da048b9e8d3d56ce5d3fc45b083ded2315a7692bcd1ab7c197dd588fd695993442371aa6db863f17a3d0fbb1f9" },
																"posix": "macslave01.valvesoftware.com ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAQEAxQ7P5X/vMp9EKmUlZvyHNPz0L3wgvrJZPuVc4FxDKlBHiH7ijW8k88YMXW2vWC/NjQdPIN5GeJOthY/6aAnMWUJ83iyGU2v2yYgQ3DdLevzrSaabYH+ynOfqoodR/NeNRbWbQJ5mWADpGwmgYVsWEYv7/rtjjvY9m90jpmoc2kk+F9UptNn8+WqMc8lH6GoKQZNrI2x5ijViCvZa5QRZ9GAu8UNPBvWWz/qNvr+YXliDNeAKn2flLJfp2bDPQgoE6S9WwST0ML5Nxy5wEkS78AXTYdoEi56NPVbOXT/EWwg97SMVp2krzRq3wZfdWI/WlZk0QjcaptuGPxej0Pux+Q==" } } }

g_szFetchSSHPrivateKeyScriptlet = '''
export LANG="C";
KEYFILE=~/.ssh/id_rsa;
if [ -f ${KEYFILE} -a -f ${KEYFILE}.pub ]; then
   echo "using existing ssh private/public key" >&2;
else
   echo "generating ssh keypair" >&2;
   mkdir -p ~/.ssh;
   ssh-keygen -t rsa -b 1024 -f ${KEYFILE} -q -N "";
fi;
grep -q "`head -2 ${KEYFILE}.pub | tail -1`" ~/.ssh/authorized_keys;
if [ $? -ne 0 ]; then
   echo "adding key to authorized keys file" >&2;
   cat ${KEYFILE}.pub >> ~/.ssh/authorized_keys;
fi;
cat ${KEYFILE};
exit $?;'''

g_szCreateClientScriptletTemplate = '''
export LANG="C";
export P4CLIENT=%(P4CLIENT)s;
export P4PORT=perforce.valvesoftware.com:1666;

mkdir -p P4Clients/${P4CLIENT};
cd P4Clients/${P4CLIENT};

echo "creating p4 client ${P4CLIENT}" >&2;
p4 client -i << EOF

Client: ${P4CLIENT}

Owner: ${USER}

Root: ${PWD}

Options: noallwrite clobber nocompress unlocked nomodtime rmdir

SubmitOptions: revertunchanged

LineEnd: local

View:
%(VIEWSPEC)s

EOF

RET=$?;
if [ $RET -ne 0 ]; then
   echo "client creation failed" >&2;
   exit $RET;
fi;
'''

g_szRemoteP4ScriptletTemplate = '''
export LANG="C";
export P4CLIENT=%(P4CLIENT)s;
export P4PORT=perforce.valvesoftware.com:1666;

mkdir -p P4Clients/${P4CLIENT};
cd P4Clients/${P4CLIENT};

p4 %(P4COMMAND)s;
exit $?;
'''

g_szRemoteUnpickleScriptletTemplate = '''
export LANG="C";
export P4CLIENT=%(P4CLIENT)s;
export P4PORT=perforce.valvesoftware.com:1666;

mkdir -p P4Clients/${P4CLIENT};
cd P4Clients/${P4CLIENT};

/usr/bin/env python ./src/devtools/vcodepickle.py --restore --file %(PICKLEFILE)s;
exit $?;
'''

g_szRemoteBuildScriptletTemplate = '''
export LANG="C";
export P4CLIENT=%(P4CLIENT)s;
export P4PORT=perforce.valvesoftware.com:1666;
export BUILDLOG=remote_build.log
export CRITWARNLOG=critical_warnings.log
export SOLUTION=remote_build

mkdir -p P4Clients/${P4CLIENT};
cd P4Clients/${P4CLIENT}/src;
cp /dev/null ${BUILDLOG}

./devtools/bin/vpc /mksln ${SOLUTION} +%(VPCGROUP)s >> ${BUILDLOG} 2>> ${BUILDLOG};

echo -e "\n\n\n=================== Starting %(CONFIG)s Build ===================\n" >> ${BUILDLOG};
make -f ${SOLUTION}.mak CFG=%(CONFIG)s -k %(TARGET)s >> ${BUILDLOG} 2>> ${BUILDLOG};
BUILDFAIL=$?

# look for warnings we promote to errors - needs to be kept
# in sync with the buildbot rules
grep "call will abort" ${BUILDLOG} > ${CRITWARNLOG};
let "NOCRITICALWARNINGS=$?";
grep "is used uninitialized" ${BUILDLOG} >> ${CRITWARNLOG};
let "NOCRITICALWARNINGS=$NOCRITICALWARNINGS & $?";

RET=1;
if [ $BUILDFAIL -eq 0 -a $NOCRITICALWARNINGS -eq 1 ]; then
   RET=0;
   echo "BUILD SUCCESS";
else
   echo "BUILD FAILED";
fi;

if [ $NOCRITICALWARNINGS -eq 0 ]; then
   echo "!!! The following warnings will be promoted to errors !!!" >&2
   echo >&2;
   cat ${CRITWARNLOG} >&2
   echo >&2
fi;

cat ${BUILDLOG} >&2
exit $RET
'''

g_szRemoteVsignTestTemplate = '''
export P4CLIENT=%(P4CLIENT)s;
cd P4Clients/${P4CLIENT}/src;
cp ./devtools/bin/vsign ./devtools/bin/vsign_test;
chmod +w ./devtools/bin/vsign_test
./devtools/bin/vsign -signvalve ./devtools/bin/vsign_test;
exit $?;
'''

g_szRemoteVsignSetPassphraseTemplate = '''
export P4CLIENT=%(P4CLIENT)s;
cd P4Clients/${P4CLIENT}/src;
mkdir -p ~/Library/Application\ Support/Steam;
./devtools/bin/vsign -set_passphrase CodeSignature %(PASSPHRASE)s;
exit $?;
'''

def usage():
	global g_mapPlatformToBuildHostInfo
	print "usage: ", sys.argv[0], " [options]"
	print ""
	print "-p,--platform=   	%s" % g_mapPlatformToBuildHostInfo.keys()
	print "-c,--config=     	[ 'debug', 'release' ] (default builds both )"
	print "-e,--exhaustive  	build all build configurations on all platforms"
	print "--rebuild        	do a clean build"
	print "-b,--batch       	supress user prompts"
	print "-r,--root=       	if you have a strange p4 client spec (cough, JoeR, cough)"
	print "                 	specify the directory *containing* src"
	print "output related options:"
	print "-q,--quiet       	reduce spew"
	print "-d,--debug       	debug level spew"
	print "-s,--simulate    	dry-run"

def runLocalCommand( szCommand, bWait=True ):
	global g_bSimulateOnly
	global g_bDebugVerbose

	if g_bDebugVerbose:
		print ">>> runLocalCommand( %s )\n" % szCommand
	if g_bSimulateOnly:
		return( 0, None, None )

	po = subprocess.Popen( szCommand, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=sys.stdin )
	if bWait:
		( stdout, stderr ) = po.communicate()
		if g_bDebugVerbose:
			print stdout
			print stderr
		return ( po.returncode, stdout, stderr )
	else:
		return ( 0, "", "" )

def runRemoteCommand( szRemoteUser, szRemoteHost, szCommand, bPromptForPasswd = False, bCheckReturn = True ):
	global g_bSimulateOnly
	global g_bDebugVerbose
	global g_bBatchMode
	global g_szSSHBin
	global g_mapPlatformToBuildHostInfo
	szSSHCmd = g_szSSHBin

	if bPromptForPasswd and g_bBatchMode:
		raise RuntimeError( "can't prompt for password in batch mode" )

	if sys.platform == 'win32':
		szSSHCmd += " -agent -batch"
		if bPromptForPasswd:
			szPasswd = getpass.getpass( "password for %s@%s: " % (szRemoteUser, szRemoteHost) )
			szSSHCmd += ' -pw "%s"' % szPasswd

	cmd = '%s %s@%s' % ( szSSHCmd,
						 szRemoteUser,
						 szRemoteHost )

	fd = fname = None
	if sys.platform == 'win32':
		# windows shell FTW
		(fd, fname) = tempfile.mkstemp()
		os.write( fd, szCommand )
		os.close( fd )
		cmd += " -m %s" % fname
		if g_bDebugVerbose: print "tmpfile %s contains %s" % ( fname, szCommand )
	else:
		cmd += " '%s'" % szCommand

	if g_bDebugVerbose:
		print ">>> runRemoteCommand( %s )\n" % cmd
	if g_bSimulateOnly:
		return ( 0, "simulation mode", "" )

	po = subprocess.Popen( cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=sys.stdin )
	( stdout, stderr ) = po.communicate()

	if fd is not None:
		os.unlink( fname )

	if po.returncode != 0 and bCheckReturn:
		print "looks like something went wrong with a remote command"
		print ">>> %s " % cmd
		print "returned %d" % po.returncode
		if sys.platform == 'win32':
			print "tempfile contains %s" % szCommand
		print "produced stdout:"
		print stdout
		print ""
		print "produced stderr:"
		print stderr
		raise RuntimeError( "runRemoteCommand returned non-zero result" )

	if g_bDebugVerbose: print "return: %d\nstdout: %s\nstderr: %s" % ( po.returncode, stdout, stderr )

	return ( po.returncode, stdout, stderr )

def addHostKeyToKnownHostKeys( szPlatformName ):
	global g_mapPlatformToBuildHostInfo
	global g_bDebugVerbose

	if sys.platform == 'win32':
		mapRegKeyInfo = g_mapPlatformToBuildHostInfo[ szPlatformName ][ 'sshkey' ][ 'win32' ]
		import _winreg
		try:
			knownHostsKey = _winreg.OpenKey( _winreg.HKEY_CURRENT_USER,
											 mapRegKeyInfo[ 'regkey' ],
											 sam = _winreg.KEY_SET_VALUE )
			if g_bDebugVerbose: print "OpenKey %s succeeded" % mapRegKeyInfo[ 'regkey' ]
		except:
			knownHostsKey = _winreg.CreateKey( _winreg.HKEY_CURRENT_USER,
											   mapRegKeyInfo[ 'regkey' ] )
			if g_bDebugVerbose: print "CreateKey %s succeeded" % mapRegKeyInfo[ 'regkey' ]
		try:
			value, type = _winreg.QueryValueEx( knownHostsKey, mapRegKeyInfo[ 'valueName' ] )
			assert( type == _winreg.REG_SZ and value == mapRegKeyInfo[ 'value' ] )
			if g_bDebugVerbose: print "QueryValueEx %s succeeded" % mapRegKeyInfo[ 'valueName' ]
		except:
			_winreg.SetValueEx( knownHostsKey, 
								mapRegKeyInfo[ 'valueName' ], False,
								_winreg.REG_SZ, mapRegKeyInfo[ 'value' ] )
			if g_bDebugVerbose: print "SetValueEx %s=%s succeeded" % ( mapRegKeyInfo[ 'valueName' ],
																	   mapRegKeyInfo[ 'value' ] )
		try:
			_winreg.CloseKey( knownHostsKey )
			if g_bDebugVerbose: print "CloseKey succeeded" 
		except:
			pass
	else:
		assert( "implement addHostKeyToKnownKeys" )


def calculateRemoteView( mapClientInfo, szRemoteClientName, szBranchName ):
	global g_BranchSpec
	szRemoteView = g_BranchSpec % ( szRemoteClientName )
	return szRemoteView


def main():
	global g_bVerbose
	global g_bDebugVerbose
	global g_bSimulateOnly
	global g_bBatchMode
	global g_lRemotePlatforms
	global g_lBuildConfigs
	global g_szSSHKeyDir
	global g_szSSHAddKeyCmd
	global g_szCleanTarget
	global g_szSSHKeyConvertCmd
	global g_szVCodePickle
	global g_szMoreBin
	global g_szFetchSSHPrivateKeyScriptlet
	global g_szCreateClientScriptletTemplate
	global g_szRemoteP4ScriptletTemplate
	global g_szRemoteBuildScriptletTemplate
	global g_mapPlatformToBuildHostInfo
	global g_VPCCommand
	global g_Branch

	bSetBuildConfigs = False
	bSetBuildPlatform = False
	szBranchName = ""
	szUserName = getpass.getuser()
	szP4ChangeToSync = "all"

	clientInfo = p4helpers.GetClientInfo()
	szLocalClientName = clientInfo[ 'Client' ]
	szLocalClientRoot = clientInfo[ 'Root' ]

	try:
		opts, args = getopt.getopt( sys.argv[1:], "bdsvqp:c:r:e?l", [ "change=", "batch", "config=", "exhaustive", "debug", "simulate", "verbose", "quiet", "platform=", "root=", "rebuild", "limited-sync" ] )
	except getopt.GetoptError, e:
		print ""
		print "Argument error: ", e
		print ""
		usage()
		sys.exit(1)
	for o, a in opts:
		if o in ( "-?" ):
			usage()
			sys.exit(1)
		if o in ( "-b", "--batch" ):
			g_bBatchMode = True
		if o in ( "--rebuild" ):
			g_szCleanTarget = "clean"
		if o in ( "-c", "--config" ):
			if a not in [ 'debug', 'release' ]:
				raise RuntimeError( "configuration '%s' not understood" % a )
			g_lBuildConfigs = [ a ]
			bSetBuildConfigs = True;
		if o in ( "-r", "--root" ):
			szLocalClientRoot = a
		if o in ( "-e", "--exhaustive" ):
			g_lRemotePlatforms = g_mapPlatformToBuildHostInfo.keys()
		if o in ( "-p", "--platform" ):
			if a not in g_mapPlatformToBuildHostInfo.keys():
				raise RuntimeError( "platform '%s' not supported" % a )
			g_lRemotePlatforms = [ a ]
			bSetBuildPlatform = True
		if o in ( "-v", "--verbose" ):
			g_bVerbose = True
		if o in ( "-q", "--quiet" ):
			g_bVerbose = False
			g_bDebugVerbose = False
		if o in ( "-d", "--debug" ):
			g_bDebugVerbose = True
		if o in ( "-s", "--simulate" ):
			g_bSimulateOnly = True
		if o in ( "--change" ):
			szP4ChangeToSync = a

	if not os.getcwd().lower().startswith( clientInfo[ 'Root' ].lower() ):
		print ""
		print "I need to be run from the root (%s)" % szLocalClientRoot
		print "or any subdirectory of your p4 client (%s)" % szLocalClientName
		print ""
		print "If these values look wrong, make sure you have p4 configured"
		print "correctly for command-line use." 
		sys.exit(1)

	try:
		p4path = p4helpers.P4Where( os.path.join( szLocalClientRoot, "src\..." )  )
	except:
		try:
			# try the cwd as the place src/ is to support "interesting" client specs
			szLocalClientRoot =  os.path.split( os.getcwd() )[0]
			p4path = p4helpers.P4Where( os.path.join( szLocalClientRoot, "src\..." )  )
		except:
			print "couldn't find src where I expected it, perhaps you need the -r option?"
			usage()
			sys.exit(1)

	# on source only build release by default
	if ( bSetBuildConfigs == False  ):
		g_lBuildConfigs = ['release' ]

	if not p4path.lower().startswith( g_Branch ):
		raise RunTimeError( "couldn't map local enlistment to staging branch" )

	# do everything relative to the client root directory
	os.chdir( szLocalClientRoot )

	if g_bVerbose: print "building configuration(s) %s on platform(s) %s" % ( g_lBuildConfigs, g_lRemotePlatforms )

	szLocallySyncedRevision = p4helpers.GetSyncedRevision( szLocalClientRoot )

	szPickleFile = 'vcodepickle_%s.zip' % szLocalClientName
	if g_bVerbose: print "pickling local changes to %s..." % szPickleFile, 
	( ret, stdout, stderr ) = runLocalCommand( '%s --backup --file %s -c %s --exclude "*.exe" --exclude "*.dll" --exclude "*.pdb"  --include "*/src/*"' % ( g_szVCodePickle, szPickleFile, szP4ChangeToSync ) )
	if g_bVerbose: sys.stdout.flush(); print "done." 

	for platform in g_lRemotePlatforms:
		if g_bVerbose: print "\nbuilding on platform %s\n" % ( platform )		
		szRemoteHost = g_mapPlatformToBuildHostInfo[ platform ][ 'hostname' ]

		szRemoteClientName = "%s_%s_remote_build_%s" % ( szUserName, g_Branch.replace( "//", "" ).replace( "/", "_" ), szRemoteHost[ : szRemoteHost.find(".") ] )

		# figure out where the private key for this host would be, if we were to have one
		absKeyDirPath = os.path.join( os.path.expanduser("~"), g_szSSHKeyDir )
		szSSHPrivKeyFile = os.path.join( absKeyDirPath, 'id_rsa_%s@%s' % ( szUserName, szRemoteHost ) )
		if sys.platform == 'win32':
				szSSHPrivKeyFile += '.ppk'

		if g_bDebugVerbose: print "adding remote host key for %s to known keys..." % platform
		addHostKeyToKnownHostKeys( platform )

		# if we don't have a key for this host, run the scriptlet to create one
		if not os.path.exists( szSSHPrivKeyFile ):
			if not os.path.exists( absKeyDirPath ):
				os.makedirs( absKeyDirPath )

			if g_bVerbose: print "creating ssh public/private keypair..."
			( ret, stdout, stderr ) = runRemoteCommand( szUserName, szRemoteHost, g_szFetchSSHPrivateKeyScriptlet, bPromptForPasswd = True )
		
			if not g_bSimulateOnly:
				assert( len( stdout ) )
				assert( not g_bBatchMode )
				f = open( szSSHPrivKeyFile, "w" )
				f.write( stdout )
				f.close()

				if sys.platform == 'win32':
					os.rename( szSSHPrivKeyFile, szSSHPrivKeyFile + ".tmp" )
					# let's assume we need to convert their key into putty format
					print ""
					print "			!!!!! ACHTUNG !!!!!"
					print ""
					print "your private key doesn't seem to be in putty format (yet)."
					print ""
					print "I'm going to launch puttygen, which should present a dialog"
					print "saying it's successfully imported the key, and that you need"
					print "to save it in putty format before use."
					print ""
					print "1. click OK to dismiss this dialog"
					print "2. Choose File->Save Private Key"
					print "3. Confirm saving the key without a passphrase"
					print "4. paste this path into the save dialog:"
					print "   %s" % szSSHPrivKeyFile.replace( "/", "\\" )
					print "5. quit puttygen"
					print ""
					raw_input( "Press return to continue: " )
					runLocalCommand( "%s %s" % ( g_szSSHKeyConvertCmd, szSSHPrivKeyFile + ".tmp" ) )
					os.remove( szSSHPrivKeyFile + ".tmp" )
				else:
					# on posix, ssh wants the keyfile to be tightly controlled
					os.chmod( szSSHPrivKeyFile, stat.S_IRUSR | stat.S_IWUSR )
			
		# try loading the ssh key into their agent
		if g_bDebugVerbose: print "adding private key to ssh authentication agent..."
		( ret, stdout, stderr ) = runLocalCommand( "%s %s" % ( g_szSSHAgentAddKeyCmd, szSSHPrivKeyFile ), bWait = False )

		# figure out what the remote view should look like
		if g_bDebugVerbose: print "calculating remote view..."
		szRemoteView = calculateRemoteView( clientInfo, szRemoteClientName, szBranchName )
		if g_bDebugVerbose: print "remote view: %s" % szRemoteView
		
		# do the magic substituion on the client create scriptlet
		if g_bVerbose: print "syncing remote p4 client spec...",
		szClientCreateScriptlet = g_szCreateClientScriptletTemplate % { "P4CLIENT" : szRemoteClientName,
																		"VIEWSPEC" : szRemoteView,
																		"REVISION" : szLocallySyncedRevision }
		( ret, stdout, stderr ) = runRemoteCommand( szUserName, szRemoteHost, szClientCreateScriptlet )
		if g_bVerbose: sys.stdout.flush(); print "done." 

		if platform == "osx":
			if g_bVerbose: print "checking if vsign is set up...",
			szRemoteCheckVsignScriptlet = g_szRemoteVsignTestTemplate % {	"P4CLIENT" : szRemoteClientName }
			( ret, stdout, stderr ) = runRemoteCommand( szUserName, szRemoteHost, szRemoteCheckVsignScriptlet, False, False )
			if g_bVerbose: sys.stdout.flush(); print "done."	
		
			if( ret != 0 ):	
				passphrase = raw_input("Please enter code signature passphrase for public universe: ")	
				if g_bVerbose: print "setting vsign passphrase...",
				szremoteSetVsignPassphraseScriptlet = g_szRemoteVsignSetPassphraseTemplate % {  "P4CLIENT" : szRemoteClientName,
																								"PASSPHRASE": passphrase}
				( ret, stdout, stderr ) = runRemoteCommand( szUserName, szRemoteHost, szremoteSetVsignPassphraseScriptlet, False, False )
				if g_bVerbose: sys.stdout.flush(); print "done."
			
		if g_bVerbose: print "reverting any open files on remote p4client...",
		szClientSyncScriptlet = g_szRemoteP4ScriptletTemplate % { "P4CLIENT" : szRemoteClientName,
																  "P4COMMAND": 'revert ...' }
		( ret, stdout, stderr ) = runRemoteCommand( szUserName, szRemoteHost, szClientSyncScriptlet )
		if g_bVerbose: sys.stdout.flush(); print "done." 
		
		if g_bVerbose: print "syncing remote p4client to changelist %s (this could take a while)..." % szLocallySyncedRevision,
		szClientSyncScriptlet = g_szRemoteP4ScriptletTemplate % { "P4CLIENT" : szRemoteClientName,
																  "P4COMMAND": 'sync @%s' % szLocallySyncedRevision }
		( ret, stdout, stderr ) = runRemoteCommand( szUserName, szRemoteHost, szClientSyncScriptlet )
		if g_bVerbose: sys.stdout.flush(); print "done." 
		
		if g_bVerbose: print "transferring pickled changes...",
		( ret, stdout, stderr ) = runLocalCommand( "%s %s %s@%s:P4Clients/%s/" % ( g_szSCPBin, szPickleFile, szUserName, szRemoteHost, szRemoteClientName ) )
		if g_bVerbose: sys.stdout.flush(); print "done." 
		
		if g_bVerbose: print "applying changes to remote client...",
		szUnpickleScriptlet = g_szRemoteUnpickleScriptletTemplate % { "P4CLIENT"  : szRemoteClientName,
																	  "PICKLEFILE": szPickleFile }
		( ret, stdout, stderr ) = runRemoteCommand( szUserName, szRemoteHost, szUnpickleScriptlet )
		if g_bVerbose: sys.stdout.flush(); print "done." 
		
		for buildConfig in g_lBuildConfigs:
			if g_bVerbose: print "running remote %s build..." % buildConfig,
			szBuildScriptlet = g_szRemoteBuildScriptletTemplate % { "P4CLIENT" : szRemoteClientName,
																	"VPCGROUP" : g_VPCCommand[ platform ],
																	"CONFIG"   : buildConfig,
																	"TARGET"   : "%s all" % g_szCleanTarget }
			( ret, stdout, stderr ) = runRemoteCommand( szUserName, szRemoteHost, szBuildScriptlet, bCheckReturn = False )
			if g_bVerbose: sys.stdout.flush(); print "done." 
		
			if len(stderr):
				# figure out the correct newline character - can't we all just get along?
				NL = "\n"
				if sys.platform == "win32":
					NL = "\r\n"

				# look for a couple common cases
				errlines = stderr.split( "\n" )

				(fd, name) = tempfile.mkstemp( suffix=".txt" )
				os.write( fd, NL.join( errlines ) )
				os.close( fd )
				print "build output in file://%s" % name

				bFoundErrors = False
				for errline in errlines:
					if errline.lower().find( "error:" ) >= 0:
						bFoundErrors = True
						print "  " + errline

				if ret !=0 and not bFoundErrors:
					iLines = 5
					print "first %d lines of build log:" % iLines
					print NL.join( errlines[ :iLines ] )
					if len(errlines) > iLines:
						print "..."
			
				if ret != 0:
					if not g_bBatchMode:
						resp = raw_input( "display full build log (Y/N)? " )
						if resp.lower() == "y":
							cmd = "%s %s" % ( g_szMoreBin, name )
							runLocalCommand( cmd )

			print stdout

			if ret != 0:
				break;

if __name__ == '__main__':
	main()
	
