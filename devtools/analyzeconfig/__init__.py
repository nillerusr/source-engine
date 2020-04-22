"""
This file exists to mark this directory as being a Python package,
and to contain project specific configurations for parse_analyze_errors.py.
See http://www.python.org/doc/essays/packages.html for details.
Other Python scripts could be put into this directory and would be
imported as analyzeconfig.scriptname.
"""



"""
The VC++ 2012 compiler had a crashing bug when you use /analyze. Depending on the frequency
of this crash (which varies per project) we can either ignore it or try to proceed. If abortOnCompilerCrash
is true then parsing stops when a compiler crash is detected, no warnings are reported, and the
last-known-good is left unchanged so that the next build will give correct diffs.
"""
abortOnCompilerCrash = True



"""
Normally the last-known-good report is updated whenever a build passes with no fatal warnings. However if
the compiler keeps crashing then this means that the last-known-good will keep changing, leading to false
reports of new warnings. Also, a project may decide that they don't want any new warnings. This setting
controls updating of the last-known-good file.
"""
updateLastKnownGood = True



"""
Some warnings occur in Microsoft header files. We need to ignore these. Substring
matching is done on these.
"""
ignorePaths = [
	r"microsoft visual studio 10.0\vc",
	r"microsoft visual studio 11.0\vc",
	r"microsoft sdks\windows\v7.0a",
	r"microsoft xbox 360 sdk\include",
	r"windows kits\8.0\include",
	r"thirdparty\dxsdk\include",
	r"thirdparty\physx301\physxsdk",
	]



"""
This list contains warnings which /analyze can identify with (ideally) 100%
accuracy and which the team believes should always be fixed. This typically
includes varargs mismatches of all kinds.
The layout is designed to match the warningsToText array to make it easy to
copy between them.

This array should probably be empty when /analyze is first run on a project
but should be filled in as much as practical in order to avoid regressions.
"""
alwaysFatalWarnings = {
	4189 : "Local variable is initialized but not referenced",
	4789 : "Destination of memory copy is too small",
	6053 : "Call to <function> may not zero-terminate string",
	6057 : "Buffer overrun due to number of characters/number of bytes mismatch",
	6059 : "Incorrect length parameter",
	6063 : "Missing string argument",
	6064 : "Missing integer argument",
	6066 : "Non-pointer passed as parameter when pointer is required",
	6067 : "Parameter in call must be the address of the string",
	6209 : "Using sizeof when a character count might be needed. Annotate with OUT_Z_CAP or its relatives",
	6269 : "Possible incorrect order of operations: dereference ignored",
	6270 : "Missing float argument to varargs function",
	6271 : "Extra argument passed: parameter is not used by the format string",
	6272 : "Non-float passed as argument <number> when float is required",
	6273 : "Non-integer passed as a parameter when integer is required",
	6274 : "Non-character passed as parameter '5' when character is required",
	6278 : "Buffer is allocated with array new [], but deleted with scalar delete. Destructors will not be called",
	6281 : "Incorrect order of operations: relational operators have higher precedence than bitwise operators",
	6282 : "Incorrect operator: assignment of constant in Boolean context",
	6283 : "Buffer is allocated with array new [], but deleted with scalar delete",
	6284 : "Object passed as a parameter when string is required",
	6290 : "Bitwise operation on logical result: ! has higher precedence than &. Use && or (!(x & y)) instead",
	6293 : "Ill-defined for-loop:  counts down from minimum.",
	#6298 : "Using a read-only string <pointer> as a writable string argument",
	6302 : "Format string mismatch: character string passed as parameter when wide character string is required",
	6306 : "Incorrect call to 'fprintf*': consider using 'vfprintf*' which accepts a va_list as an argument",
	# This warning seems to show up in some template code so it can't be a fatal warning.
	#6313 : "Incorrect operator: zero-valued flag cannot be tested with bitwise-and. Use an equality test to check for zero-valued flags",
	6314 : "Incorrect order of operations: bitwise-or has higher precedence than the conditional-expression operator",
	6315 : "Incorrect order of operations: bitwise-and has higher precedence than bitwise-or",
	6316 : "Incorrect operator:  tested expression is constant and non-zero.  Use bitwise-and to determine whether bits are set.",
	6317 : "Incorrect operator: logical-not (!) is not interchangeable with ones-complement (~)",
	6328 : "Wrong parameter type passed",
	6334 : "Sizeof operator applied to an expression with an operator might yield unexpected results",
	6336 : "Arithmetic operator has precedence over question operator, use parentheses to clarify intent",
	6522 : "Invalid size specification: expression must be of integral type",
	6523 : "Invalid size specification: parameter 'size' not found",
	28252 : "Inconsistent annotation",
	28253 : "Inconsistent annotation",
	}



"""
This list contains warnings which /analyze can identify with (ideally) 100%
accuracy and which the team wishes to avoid introducing new instances of.
This could include variable shadowing, alloca in a loop, or other bugs
which are problematic to fix all legacy instances of.
"""
fatalWhenNewWarnings = {
	# Having variable shadowing be fatal on new warnings would be great, but it's best to see if
	# that is what the team wants.
	#6244 : "Local declaration shadows declaration of same name in global scope",
	#6246 : "Local declaration shadows declaration of same name in outer scope",
	6263 : "Using _alloca in a loop: this can quickly overflow stack",
}



# The paths on the build machines are very long and make reading the results cumbersome.
# Remap them down to something more compact.
remaps = {
	r"c:\buildslave_source\dota_staging_analyzebuild_win32\build\src" : r"D:\dota\staging\src",
	r"c:\buildslave_source2\buildbot\source2_analyzebuild_win32\build\src" : r"D:\source2\src",
	r"c:\buildslave_steam\steam_main_analyze_win32\build\src" : r"D:\clients\steam_main\src",
	}



# Some warnings are only kept on so that we can see the statistics they give us.
# They shouldn't count towards the total warning count.
# 6262: Excessive stack usage in function
# 28159: Consider using 'GetTickCount64' instead of 'GetTickCount'
# 6244: Local declaration shadows declaration of same name in global scope
# 6246: Local declaration shadows declaration of same name in outer scope
informationalWarnings = ["6262", "28159", "6244", "6246"]
