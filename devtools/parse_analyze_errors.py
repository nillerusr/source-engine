# This script is used to parse the results of the Visual C++ /analyze feature.
# See the 'usage' section for details.

# Regular expression experimentation was done at http://www.pythonregex.com/

# The buildbot warning parser that looks at this script uses the default compile warning
# parser which is documented at http://buildbot.net/buildbot/docs/0.8.4/Compile.html
# The regex used is '.*warning[: ].*'. This means that any instance of 'warning:' or
# 'warning ' will be flagged as a warning. The check is case sensitive so Warning will
# not be flagged as a warning. This script remaps warning to 'wrning' in some places so
# that lists of fixed warnings or old warnings will not trigger warning detection.
# Similarly it remaps error to 'eror'.

# Typical warning messages might look like this:
# 2>d:\dota\src\tier1\bitbuf.cpp(1336): warning C6001: Using uninitialized memory 'retval': Lines: 1327, 1328, 1331, 1332, 1333, 1334, 1336

import re
import sys
import os

# Grab per-project configuration information from the analyzeconfig package
import analyzeconfig
ignorePaths = analyzeconfig.ignorePaths
alwaysFatalWarnings = analyzeconfig.alwaysFatalWarnings.keys()
fatalWhenNewWarnings = analyzeconfig.fatalWhenNewWarnings.keys()
remaps = analyzeconfig.remaps
informationalWarnings = analyzeconfig.informationalWarnings

lkgFilename = "analyzelkg.txt"

# This matches 0-3 digits and an optional '>' character. Some builds prefix the output
# with '10>' or something equivalent, but some builds do not.
prefixRePattern = r"\d?\d?\d?>?"

warningWithLinesRe = re.compile(prefixRePattern + r"(.*)\((\d+)\): warning C(\d{4,5})(.*)(: Lines:.*)")
warningRe = re.compile(prefixRePattern + r"(.*)\((\d+)\): warning C(\d{4,5})(.*)")
errorRe = re.compile(prefixRePattern + r"(.*)\((\d+)\): error C(\d{4,5})(.*)")

# For reparsing the keys that we use to store the parsed log data:
# The format for keys is like this:
# key = "%s %s in %s" % (type, warningNumber, filename)
parseKeyRe = re.compile(r"(.*) (\d{4,5}) in (.*)")

warningsToText = {
	2719 : "Formal parameter with __declspec(align('n')) won't be aligned",
	4005 : "Macro redefinition",
	4100 : "Unreferenced formal parameter",
	4189 : "Local variable is initialized but not referenced",
	4245 : "Signed/unsigned mismatch",
	4505 : "Unreferenced local function has been removed",
	4611 : "interaction between '_setjmp' and C++ object destruction is non-portable",
	4703 : "Potentially uninitialized local pointer variable used",
	4789 : "Destination of memory copy is too small",
	6001 : "Using uninitialized memory",
	6029 : "Possible buffer overrun: use of unchecked value",
	6053 : "Call to <function> may not zero-terminate string",
	6054 : "String may not be zero-terminated",
	6057 : "Buffer overrun due to number of characters/number of bytes mismatch",
	6059 : "Incorrect length parameter",
	6063 : "Missing string argument",
	6064 : "Missing integer argument",
	6066 : "Non-pointer passed as parameter when pointer is required",
	6067 : "Parameter in call must be the address of the string",
	6200 : "Index is out of valid index range for non-stack buffer",
	6201 : "Out of range index",
	6202 : "Buffer overrun for stack allocated variable in call to function",
	6203 : "Buffer overrun for non-stack buffer",
	6204 : "Possible buffer overrun: use of unchecked parameter",
	6209 : "Using sizeof when a character count might be needed. Annotate with OUT_Z_CAP or its relatives",
	6216 : "Compiler-inserted cast between semantically different integral types: a Boolean type to HRESULT",
	6221 : "Implicit cast between semantically different integer types",
	6219 : "Implicit cast between semantically different integer types",
	6236 : "(<expression> || <non-zero constant>) is always a non-zero constant",
	6244 : "Local declaration shadows declaration of same name in global scope",
	6246 : "Local declaration shadows declaration of same name in outer scope",
	6248 : "Setting a SECURITY_DESCRIPTOR's DACL to NULL will result in an unprotected object",
	6258 : "Using TerminateThread does not allow proper thread clean up",
	6262 : "Excessive stack usage in function",
	6263 : "Using _alloca in a loop: this can quickly overflow stack",
	6269 : "Possible incorrect order of operations: dereference ignored",
	6270 : "Missing float argument to varargs function",
	6271 : "Extra argument passed: parameter is not used by the format string",
	6272 : "Non-float passed as argument <number> when float is required",
	6273 : "Non-integer passed as a parameter when integer is required",
	6277 : "NULL application name with an unquoted path results in a security vulnerability if the path contains spaces",
	6278 : "Buffer is allocated with array new [], but deleted with scalar delete. Destructors will not be called",
	6281 : "Incorrect order of operations: relational operators have higher precedence than bitwise operators",
	6282 : "Incorrect operator: assignment of constant in Boolean context",
	6283 : "Buffer is allocated with array new [], but deleted with scalar delete",
	6284 : "Object passed as a parameter when string is required",
	6286 : "(<non-zero constant> || <expression>) is always a non-zero constant.",
	6287 : "Redundant code: the left and right sub-expressions are identical",
	6290 : "Bitwise operation on logical result: ! has higher precedence than &. Use && or (!(x & y)) instead",
	6293 : "Ill-defined for-loop: counts down from minimum",
	6294 : "Ill-defined for-loop: initial condition does not satisfy test. Loop body not executed",
	6295 : "Ill-defined for-loop: Loop executed indefinitely",
	6297 : "Arithmetic overflow: 32-bit value is shifted, then cast to 64-bit value",
	6298 : "Using a read-only string <pointer> as a writable string argument",
	6302 : "Format string mismatch: character string passed as parameter when wide character string is required",
	6306 : "Incorrect call to 'fprintf*': consider using 'vfprintf*' which accepts a va_list as an argument",
	6313 : "Incorrect operator: zero-valued flag cannot be tested with bitwise-and. Use an equality test to check for zero-valued flags",
	6316 : "Incorrect operator: tested expression is constant and non-zero. Use bitwise-and to determine whether bits are set",
	6318 : "Ill-defined __try/__except: use of the constant EXCEPTION_CONTINUE_SEARCH ",
	6328 : "Wrong parameter type passed",
	6330 : "'const char' passed as a parameter when 'unsigned char' is required",
	6333 : "Invalid parameter: passing MEM_RELEASE and a non-zero dwSize parameter to 'VirtualFree' is not allowed",
	6334 : "Sizeof operator applied to an expression with an operator might yield unexpected results",
	6336 : "Arithmetic operator has precedence over question operator, use parentheses to clarify intent",
	6385 : "Out of range read",
	6386 : "Out of range write",
	6522 : "Invalid size specification: expression must be of integral type",
	6523 : "Invalid size specification: parameter 'size' not found",
	28199 : "Using possibly uninitialized: The variable has had its address taken but no assignment to it has been discovered.",
	}



def Cleanup(textline):
	for sourcePath in remaps.keys():
		if textline.startswith(sourcePath):
			return textline.replace(sourcePath, remaps[sourcePath])
	return textline



def ParseLog(logName):
	# Create a dictionary in which to store the results
	# The keys for the dictionary are "warning 6328 in c:\buildbot\..."
	# This means that the count of keys is not particularly meaningful. The
	# length of each data item tells you the total number of raw warnings, but
	# some of those are duplicates (from the same file being compiled multiple
	# times). The UniqueWarningCount function can be used to find the number of
	# unique warnings in each record.
	#
	# This probably could have been designed better, perhaps by having the key
	# include the line number. Probably not worth changing now.
	result = {}
	lines = open(logName).readlines()

	# First look for compiler crashes. Joy.
	if analyzeconfig.abortOnCompilerCrash:
		compilerCrashes = 0
		for line in lines:
			# Look for signs that the compiler crashed and if it did then abort.
			if line.count("Please choose the Technical Support command on the Visual C++") > 0:
				compilerCrashes += 1
				# Print a message in the warning format so that we can see how many times the
				# compiler crashed on the buildbot waterfall page.
				print "cl.exe(1): warning : internal compiler error, the compiler has crashed. Aborting code analysis."
		# If the compiler crashes one or more times then give up.
		if compilerCrashes > 0:
			sys.exit(0)

	warningCount = 0
	ignoredCount = 0
	namePrinted = False
	for line in lines:
		# Some of the paths in the output lines have slashes instead of backslashes.
		line = line.replace("/", "\\")
		ignored = False
		for path in ignorePaths:
			if line.count(path) > 0:
				ignored = True
				ignoredCount += 1
		if ignored:
			continue
		filename = ""
		type = "warning"
		# Look for warnings with filename and line number. The groups returned
		# are:
		#    file name
		#    line number
		#    warning number
		#    warning text
		#    optionally (warningWithLinesRe only) the lines implicated in the warning
		warningMatch = warningWithLinesRe.match(line)
		if not warningMatch:
			warningMatch = warningRe.match(line)
		if not warningMatch:
			warningMatch = errorRe.match(line)
			if warningMatch:
				type = "error"

		# We want to record how many errors of a particular type occur in a particular source
		# file so we create a dictionary with [file name, warning number, isError] as the key.
		if warningMatch:
			filename = warningMatch.groups()[0]
			lineNumber = warningMatch.groups()[1]
			warningNumber = warningMatch.groups()[2]
			warningText = warningMatch.groups()[3]
			key = "%s %s in %s" % (type, warningNumber, filename)
			data = "%s(%s): %s C%s%s" % (filename, lineNumber, type, warningNumber, warningText)
			warningCount += 1
			if key in result:
				result[key] += [data]
			else:
				result[key] = [data]
		elif line.find(": warning") >= 0:
			pass # Ignore these warnings for now
		elif line.find(": error ") >= 0:
			if not namePrinted:
				namePrinted = True
				print "  Unhandled errors found in '%s'" % logName
			print "    %s" % line.strip()

	uniqueWarningCount = 0
	uniqueInformationalCount = 0
	for key in result.keys():
		count = UniqueWarningCount(result[key])
		match = parseKeyRe.match(key)
		warningNumber = match.groups()[1]
		if warningNumber in informationalWarnings:
			uniqueInformationalCount += count
		else:
			uniqueWarningCount += count

	print "%d lines of output in %s, %d issues found, %d ignored, plus %d informational." % (len(lines), logName, uniqueWarningCount, ignoredCount, uniqueInformationalCount)
	print ""
	return result



# The output of this script is filtered by buildbot as described at
# http://buildbot.net/buildbot/docs/0.8.4/Compile.html which means that the
# warning text is generated by running it through re.match(".*warning[: ].*")
# The e-mails are generated by running them through BuildAnalyze.createSummary
# in //steam/main/tools/buildbot/shared_helpers.py. The two sets of regexes
# should be kept compatible.
# The matching is case sensitive so Warning is not matched.

def PrintEntries(newEntries, prefix, sanitize):
	printedAlready = {}
	for newEntry in newEntries:
		if not newEntry in printedAlready:
			printedAlready[newEntry] = True
			# When printing out the list of warnings that have been fixed
			# replace ": warning" with a string that will not be
			# recognized by the buildbot parser as a warning so that the
			# break e-mails will only include new warnings.
			# Yes, this is a hack. In the future a custom parser/filter
			# for the e-mails would be better.
			if sanitize:
				newEntry = newEntry.replace(": warning", ": wrning")
				newEntry = newEntry.replace(": error", ": eror")
			print "%s%s" % (prefix, Cleanup(newEntry))



def UniqueWarningCount(warningRecord):
	# Warnings may be encountered multiple times (header files included
	# from many places, or source files compiled multiple times) and these
	# are all added to the warning record. However, for determining
	# unique warnings we want to filter out these duplicates.
	alreadySeen = {}
	count = 0
	for warning in warningRecord:
		if not warning in alreadySeen:
			alreadySeen[warning] = True
			count += 1
	return count



def DumpNewWarnings(old, new, oldname, newname):
	newWarningsFound = False
	warningsFixed = False
	fatalWarningsFound = False

	warningCounts = {}
	oldWarningCounts = {}
	sampleWarnings = {}

	for key in new.keys():
		match = parseKeyRe.match(key)
		warningNumber = int(match.groups()[1])
		if warningNumber in alwaysFatalWarnings:
			fatalWarningsFound = True
		if warningNumber in warningCounts:
			warningCounts[warningNumber] += UniqueWarningCount(new[key])
		else:
			warningCounts[warningNumber] = UniqueWarningCount(new[key])
			sampleWarnings[warningNumber] = new[key][0]
		if not key in old:
			newWarningsFound = True
			if warningNumber in fatalWhenNewWarnings:
				fatalWarningsFound = True
	for key in old.keys():
		match = parseKeyRe.match(key)
		warningNumber = int(match.groups()[1])
		if warningNumber in oldWarningCounts:
			oldWarningCounts[warningNumber] += UniqueWarningCount(old[key])
		else:
			oldWarningCounts[warningNumber] = UniqueWarningCount(old[key])
			if not warningNumber in sampleWarnings:
				sampleWarnings[warningNumber] = old[key][0]
		if not key in new:
			warningsFixed = True

	if fatalWarningsFound:
		errorCode = 10
	elif newWarningsFound:
		errorCode = 10
	else:
		errorCode = 0

	# Make three passes through the warnings so that we group fatal, fatal-when-new, and
	# new warnings together, with the fatal warnings first.
	# The colons at the beginning of blank lines are so that buildbot's BuildAnalyze.createSummary
	# will retain those lines.
	for type in ["Fatal", "Fatal-when-new", "New"]:
		fixing = "required"
		if type == "New":
			fixing = "optional"
		message = "%s warning or warnings found. Fixing these is %s:\n:" % (type, fixing)
		for key in new.keys():
			newEntries = new[key]
			match = parseKeyRe.match(key)
			warningNumber = int(match.groups()[1])
			if warningNumber in alwaysFatalWarnings:
				if type == "Fatal":
					print message
					message = ":"
					PrintEntries(newEntries, "    ", False)
			elif not key in old:
				if warningNumber in fatalWhenNewWarnings:
					if type == "Fatal-when-new":
						print message
						message = ":"
						PrintEntries(newEntries, "    ", False)
				else:
					if type == "New":
						print message
						message = ":"
						PrintEntries(newEntries, "    ", False)

		# If message is short then that means it was printed and then assigned to a short
		# string, which means some warnings of this type were printed, which means we should
		# print a separator.
		if len(message) < 2:
			print ":\n:\n:\n:\n:"



	if warningsFixed:
		print "\n\n\n\n\nOld issues that have been fixed:"
		for key in old.keys():
			oldEntries = old[key]
			if not key in new:
				print "Warning fixed in %s:" % newname
				print "%d times:" % len(oldEntries)
				PrintEntries(oldEntries, "    ", True)
				print ""
			else:
				newEntries = new[key]
				# Disable printing decreased warning counts -- too much noise.
				if False and len(newEntries) < len(oldEntries):
					print "Decreased wrning count:"
					print "    Old (%s):" % oldname
					print "    %d times:" % len(oldEntries)
					PrintEntries(oldEntries, "        ", True)
					print "    New (%s):" % newname
					print "    %d times:" % len(newEntries)
					PrintEntries(newEntries, "        ", True)
					print ""

	print "\n\n\n"
	warningStats = []
	for warningNumber in warningCounts.keys():
		warningCount = warningCounts[warningNumber]
		if warningNumber in oldWarningCounts:
			warningDiff = warningCount - oldWarningCounts[warningNumber]
		else:
			warningDiff = warningCount
		warningStats.append((warningCount, warningNumber, warningDiff))
	for warningNumber in oldWarningCounts.keys():
		if not warningNumber in warningCounts:
			warningStats.append((0, warningNumber, -oldWarningCounts[warningNumber]))
	warningStats.sort()
	warningStats.reverse()
	for warningStat in warningStats:
		warningNumber = warningStat[1]
		description = ""
		if warningNumber in warningsToText:
			description = ", %s" % warningsToText[warningNumber]
		else:
			# Replace warning/error with wrning/eror so that these warning summaries don't trigger the
			# warning detection logic.
			description = ", example: %s" % sampleWarnings[warningNumber].replace("warning", "wrning").replace("error", "eror")
		print "%3d occurrences of C%d, changed %d%s" % (warningStat[0], warningStat[1], warningStat[2], description)

	# Print a summary of all stack related warnings in the new data, regardless of whether they were in the old.
	bigStackCulprits = {}
	allocaCulprits = {}
	# c:\src\simplify.cpp(1840): warning C6262: : Function uses '28708' bytes of stack: exceeds /analyze:stacksize'16384'. Consider moving some data to heap
	stackUsedRe = re.compile("(.*): warning C6262: Function uses '(\d*)' .*")
	print "\n\n\n"
	print "Stack related summary:"
	print "C6263: Using _alloca in a loop: this can quickly overflow stack"
	bigStackCulprits = []
	for key in new.keys():
		# warning C6262: Function uses '400352' bytes of stack
		# warning C6263: Using _alloca in a loop
		stackMatch = parseKeyRe.match(key)
		if stackMatch:
			warningNumber = stackMatch.groups()[1]
			if warningNumber == "6262":
				#print "Found warning %s in %s" % (warningNumber, stackMatch.groups()[2])
				entries = new[key]
				printed = {}
				for entry in entries:
					if not entry in printed:
						match = stackUsedRe.match(entry)
						if match:
							location = match.groups()[0]
							stackBytes = int(match.groups()[1])
							printed[entry] = True
							bigStackCulprits.append((stackBytes, location))
			elif warningNumber == "6263":
				#print "Found warning %s in %s" % (warningNumber, stackMatch.groups()[2])
				entries = new[key]
				printed = {}
				for entry in entries:
					if not entry in printed:
						print Cleanup(entry[:entry.find(": ")])
						printed[entry] = True

	print "\n\n"
	print "C6262: Functions that use many bytes of stack"
	bigStackCulprits.sort()
	bigStackCulprits.reverse()
	print "filename(linenumber): bytes"
	# Print a sorted summary of functions using excessive stack. It would be tidier
	# to print the size first (better alignment) but then the output can't be used
	# in the Visual Studio output window to jump to the code in question.

	# Get the lengths of all of the file names
	lengths = []
	for val in bigStackCulprits:
		lengths.append(len(Cleanup(val[1])))
	lengths.sort()
	if len(lengths) > 0:
		# Set the length at the 9xth percentile so that most of the sizes
		# are lined up.
		formatLength = lengths[int(len(lengths)*.97)]
		formatString = "%%-%ds: %%7d" % formatLength
		for val in bigStackCulprits:
			print formatString % (Cleanup(val[1]), val[0])

	# Print a list of all of the outstanding warnings
	print "\n\n\n"
	print "Outstanding warnings are:"
	DumpWarnings(new, True)
	return (errorCode, fatalWarningsFound)



def DumpWarnings(new, ignoreInformational):
	filePrinted = {}
	# If we just scan the dictionary then warnings will be grouped
	# by warning-number-in-file, but different warning numbers from the
	# same file will be scattered, and different files from the same
	# directory will also be scattered.
	# We really want warnings sorted by path name. To do that we scan
	# through the dictionary and add all of the entries to a dictionary
	# whose primary key is filename (path). Then we sort those keys.
	warningsByFile = {}
	for key in new.keys():
		match = parseKeyRe.match(key)
		type, warningNumber, filename = match.groups()
		if filename in warningsByFile:
			warningsByFile[filename].append(key)
		else:
			warningsByFile[filename] = [key]

	filenames = warningsByFile.keys()
	filenames.sort();

	for filename in filenames:
		for key in warningsByFile[filename]:
			match = parseKeyRe.match(key)
			warningNumber = match.groups()[1]
			if ignoreInformational and warningNumber in informationalWarnings:
				pass
			else:
				newEntries = new[key]
				print "%d times:" % len(newEntries)
				PrintEntries(newEntries, "    ", True)
				print ""

	if ignoreInformational:
		# Print the 6244 and 6246 warnings together in a group. We print
		# them here so that they are sorted by file name.
		print "\n\n\nVariable shadowing warnings"
		for filename in filenames:
			for key in warningsByFile[filename]:
				match = parseKeyRe.match(key)
				warningNumber = match.groups()[1]
				if warningNumber == "6244" or warningNumber == "6246":
					newEntries = new[key]
					PrintEntries(newEntries, "    ", True)
		print ""



def GetLogFileName(arg):
	# Special indicator for last-known-good. This means that the script
	# should look for analysislkg.txt and extract a file name from it.
	# Temporarily have "2" be equivalent to "lkg" to allow for a transition
	# to the lkg model.
	if arg == "lkg" or arg == "2":
		try:
			lines = open(lkgFilename).readlines()
			if len(lines) > 0:
				result = lines[0].strip()
				print "LKG analysis results are in '%s'" % result
				return result
			else:
				print "No data found in %s" % lkgFilename
		except IOError:
			print "Failed to open %s" % lkgFilename
			arg = 2

	try:
		x = int(arg)
	except:
		return arg

	if x <= 0:
		print "Numerical arguments must be from 1 to numlogs (%s)" % arg
		sys.exit(10)
	basedir = r"."
	dirEntries = os.listdir(basedir)
	logRe = re.compile(r"analyze(.*)_cl_(\d+).txt");
	logs = []
	for entry in dirEntries:
		if logRe.match(entry):
			logs.append(entry)
	# This will throw an exception if there aren't enough log files
	# available.
	newname = os.path.join(basedir, logs[-x])
	return newname



if len(sys.argv) < 2:
	print "Usage:"
	print "To get a comparison between two error log files:"
	print "    Syntax: parseerrors newlogfile oldlogfile"
	print "To get a summary of a single log file:"
	print "    Syntax: parseerrors logfile"
	print "To get a summary of the two most recent log files:"
	print "    Syntax: parseerrors 1 2"
	print "Log files can also be indicated by number where '1' is the"
	print "most recent, '2' is second oldest, etc."
	sys.exit(0)

newname = GetLogFileName(sys.argv[1])
resultnew = ParseLog(newname)
if len(sys.argv) >= 3:
	oldname = GetLogFileName(sys.argv[2])
	resultold = ParseLog(oldname)
	result = DumpNewWarnings(resultold, resultnew, oldname, newname)
	errorCode = result[0]
	fatalWarningsFound = result[1]
	if fatalWarningsFound == 0:
		if analyzeconfig.updateLastKnownGood:
			print "Updating last-known-good."
			lkgOutput = open(lkgFilename, "wt")
			lkgOutput.write(newname)
		else:
			print "Updating last-known-good is disabled."
	sys.exit(errorCode)
else:
	DumpWarnings(resultnew, False)
