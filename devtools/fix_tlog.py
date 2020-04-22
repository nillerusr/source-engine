"""
This script is designed to fix dependency problems with PCH files and tlog files.
VS 2010 puts dependency information in cl.read.1.tlog files, and sometimes (for
unknown reasons that may be related to a PS3 Addin) the list of dependencies for
the source file that generates a PCH file are corrupt. In this situation there are
only four or five files listed. Those four (the fifth file is optional) files are
always:

    C:\PROGRAM FILES (X86)\MICROSOFT VISUAL STUDIO 10.0\VC\BIN\1033\CLUI.DLL
    C:\WINDOWS\GLOBALIZATION\SORTING\SORTDEFAULT.NLS
    C:\WINDOWS\SYSTEM32\RSAENH.DLL
    C:\WINDOWS\SYSTEM32\TZRES.DLL
    C:\PROGRAM FILES (X86)\MICROSOFT VISUAL STUDIO 10.0\COMMON7\IDE\MSPDBSRV.EXE

The only known reliable fix appears to be to delete all files from the directory
containing the tlog file. This forces a full rebuild of that project, which may
be happening earlier than necessary, but it is better than having a build not
happen when it should.

For more (not necessarily accurate) information see this post:
http://social.msdn.microsoft.com/Forums/is/msbuild/thread/df873f5d-9d9f-4e8a-80ec-01042a9a7022

The problem is probably at least partially due to a bug in VC++ where it will
incrementally update a PCH file, without reading all of the header files, thus
messing up the MSBuild dependency tracker. This is Microsoft bug TFS # 412600.
"""

import os
import codecs
import sys
import glob
import re


# Determine by pattern matching whether a source file name is a PCH generator.
# These are heuristics, but as long as we do sane naming they should work fine.
def IsPCHSourceFile(filepath):
	filepath = filepath.lower()
	filepart = os.path.split(filepath)[1]
	# Common PCH source file names
	if filepart == "stdafx.cpp":
		return True
	if filepart == "cbase.cpp":
		return True

	# Common PCH source file patterns
	if filepart.count("pch.cpp") > 0:
		return True
	if filepart.count("pch_") > 0:
		return True

	# Broken accidental PCH source file names
	if filepart == "ivp_compact_ledge_gen.cxx":
		return True
	if filepart == "ivp_physic.cxx":
		return True


# Check to see if dependencies for currentSource are plausible
# If not then delete all the files from the diretory that the .tlog file is
# in.
def CheckDependencies(currentSource, currentDependencies, tlogPath):
	if IsPCHSourceFile(currentSource):
		headersFound = False
		for dependency in currentDependencies:
			if dependency.lower()[-2:] == ".h":
				headersFound = True
		# Across dozens of samples this bug always happens with four or
		# five dependencies. There are some legitimate cases where there is just a single
		# dependency, which is why I don't check for <= 4.
		if not headersFound and (len(currentDependencies) == 4 or len(currentDependencies) == 5):
			buildDir = os.path.split(tlogPath)[0]
			objectName = os.path.split(currentSource)[1]
			objectName = os.path.splitext(objectName)[0] + ".obj"
			objectPath = os.path.join(buildDir, objectName)
			print ""
			print "%s(1): warning : Bogus dependencies for %s." % (tlogPath, currentSource)
			print "Only %d dependencies found in %s." % (len(currentDependencies), tlogPath)
			print "Dependencies are:"
			for dependency in currentDependencies:
				print "    %s" % dependency

			# Nuking the object file associated with the currentSource file forces a rebuild,
			# but it does not seem to reliably update the .tlog file.
			# Similarly, nuking the tlog file(s) does not seem to reliably force a rebuild.
			# Also, nuking all of the object files does not seem to reliably update the .tlog files
			# These techniques are all supposed to work but due to an incremental PCH rebuild
			# 'feature' in Visual Studio they do not. The VS compiler will detect that no header
			# files have changed and will rebuild stdafx.cpp *using* the .pch file. This means
			# that MSBuild determines that there are *no* header file dependencies, so the .tlog
			# file gets permanently corrupted.

			# This must (and does) force a rebuild that updates the .tlog file.
			files = glob.glob(os.path.join(buildDir, "*"))
			print "Removing all %d files from %s to force a rebuild." % (len(files), buildDir)
			for anyFile in files:
				os.remove(anyFile)
		elif not headersFound:
			print ""
			print "No headers found in %d dependencies for %s" % (len(currentDependencies), tlogPath)
			for dependency in currentDependencies:
				print "    %s" % dependency


# This is called for each cl.read.1.tlog file in the tree
def ParseTLogFile(tlogPath):
	# Read all of the lines in the tlog file:
	lines = codecs.open(tlogPath, encoding="utf-16").readlines()

	# Iterate through all the lines. Lines that start with ^ are source files.
	# Lines that follow are the dependencies for that file. That's it.
	currentSource = ""
	for line in lines:
		# Strip off the carriage-return and line-feed character
		line = line.strip()
		#print line
		if line[0] == "^":
			if len(currentSource) > 0:
				CheckDependencies(currentSource, currentDependencies, tlogPath)
			currentSource = line[1:]
			currentDependencies = []
		else:
			currentDependencies.append(line)

	if len(currentSource) > 0:
		CheckDependencies(currentSource, currentDependencies, tlogPath)


tlogDroppingParser = re.compile(r"cl\.\d+\..+\.1\.tlog")

deletedDroppingsCount = 0

# Two-byte files of the form cl.1234.*.1.tlog (where * is either 'read' or
# 'write') build up without limit in the intermediate directories. The number
# is the PID of the compiler. Some builders had over 300,000 of these files and
# they noticeably slow down directory scanning. As long as we're iterating we
# might as well delete them -- they can be deleted anytime a compile is not in progress.
def DeleteExcessTLogFiles(dirname, filesindir):
	global deletedDroppingsCount
	for fname in filesindir:
		fname = fname.lower()
		if tlogDroppingParser.match(fname):
			tlogDroppingPath = os.path.join(dirname, fname)
			os.remove(tlogDroppingPath)
			deletedDroppingsCount += 1


# This is called for each directory in the tree.
def lister(dummy, dirname, filesindir):
	for fname in filesindir:
		if fname.lower() == "cl.read.1.tlog":
			# Run this first because ParseTLogFile might delete them which
			# would cause exceptions on this step.
			# We only need to run this on directories that contain
			# a cl.read.1.tlog file since these are the intermediate
			# directories.
			DeleteExcessTLogFiles(dirname, filesindir)
			tlogPath = os.path.join(dirname, fname)
			ParseTLogFile(tlogPath)


os.path.walk(".", lister, None)

# MSBuild accumulates these files without limit. Other files also accumulate but these
# ones account for 99% of the problem.
print "%d tlog droppings (cl.*.*.1.tlog temporary files) deleted." % deletedDroppingsCount
