import sys, os, string, re, time, smtplib, getopt

# syncs the current clientspec
def Sync( szP4SrcFilesToWatch, bForce ):
    print "syncing to files " + szP4SrcFilesToWatch + "..."
    sForce = ""
    if bForce:
        sForce = "-f "
    aszSyncLines = string.split( szP4SrcFilesToWatch, ";" )

    for szLine in aszSyncLines:
        if szLine:
            print "p4 sync " + sForce + szLine
            os.popen('p4 sync ' + sForce + szLine)
    print "  sync completed"

# current changelist number for this clientspec
def SubmittedChangelist( szP4SrcFilesToWatch ):
    szChangeText = os.popen('p4 changes -m1 -s submitted ' + szP4SrcFilesToWatch).read()
    line = []
    line = string.split( szChangeText )
    if len(line) > 0:
        return line[1]
    
# returns a set of changes of range [start, end]    
def GetChangelistRange(start, end, szP4SrcFilesToWatch):
	if start and end:
		szChangeText = os.popen('p4 changes -m10 ' + szP4SrcFilesToWatch + '@' + start + ',' + end).read()
		return string.split(szChangeText, '\n');
	szResult = []
	return szResult
	
# returns the raw text of a set of the most recent submissions
def GetRecentCheckins( szP4SrcFilesToWatch ):
    szChangeText = os.popen('p4 changes -m5 -s submitted ' + szP4SrcFilesToWatch).read()
    return "Most recent checkins:\n" + szChangeText
    
# perforce counter access
def GetCounter(counter):
    return string.split(os.popen('p4 counter ' + counter).read(), '\n')[0]

#returns the raw text of all unverified checkins
def GetUnverifiedCheckins( szP4SrcFilesToWatch, szVerifiedCounter, szChangeCounter ):
    szCounter = GetCounter( szVerifiedCounter )
    szLastVerified = str( int( szCounter ) + 1 )
    szCurrentChange = GetCounter( szChangeCounter )
    szChangeText = os.popen('p4 changes -s submitted ' + szP4SrcFilesToWatch + '@' + szLastVerified + ',@' + szCurrentChange).read()
    return "Unverified checkins:\n" + szChangeText
    
# extracts an email from a "changes" output line
def GetEmailFromChangeLine(changeline):
    change = (string.split(changeline, ' '))[1]
    output = os.popen('p4 change -o ' + change).read()
    user = string.split(string.split(output, "User:")[2])[0]
    output = os.popen('p4 user -o ' + user).read()
    return string.split(string.split(output, "Email:")[2])[0]


# perforce counter setting
def SetCounter(counter, value):
    os.popen('p4 counter ' + counter + ' ' + value).read()

# checks to see if any update is currently available
def AnyNewCheckins( szChangeCounter, szP4SrcFilesToWatch ):    
    szChange = GetCounter(szChangeCounter)
    if not szChange:
        #is this the problem?  Every night all these things fail.
        return 0
    return szChange <> SubmittedChangelist( szP4SrcFilesToWatch )

def LockMutex( szMutex ):    
    szLogLines = os.popen('newp4mutex lock ' + szMutex + ' 3 steambuilder perforce:1666').read()
    count = string.count( szLogLines, '\n' )
    if ( count < 3 ):
        print szLogLines
        print "HERE IS THE WEIRD LOCK ERROR!"
        print szMutex + "mutex lock failed."
        return 0
    szT = string.split(szLogLines, '\n')[2]
    successLine = string.split( szT, ' ')
    if successLine[0] == "Success:":
        print szMutex + " mutex locked."
        return 1
    else:
        print szMutex + " mutex lock failed."
        return 0

def LockMutexOld( szMutex ):
    szLogLines = os.popen('p4mutex lock ' + szMutex + ' 3 steambuilder perforce:1666').read()
    count = string.count( szLogLines, '\n' )
    if ( count < 3 ):
        print szLogLines
        print "HERE IS THE WEIRD LOCK ERROR!"
        print szMutex + "mutex lock failed."
        return 0
    szT = string.split(szLogLines, '\n')[3]
    successLine = string.split( szT, ' ')
    if successLine[0] == "Success:":
        print szMutex + " mutex locked."
        return 1
    else:
        print szMutex + " mutex lock failed."
        return 0

def UnlockMutex( szMutex ):
    szLogLines = os.popen('newp4mutex release ' + szMutex + ' 3 steambuilder perforce:1666').read()
    count = string.count( szLogLines, '\n' )
    if ( count < 3 ):
        print szLogLines
        print "HERE IS THE WEIRD RELEASE ERROR!"
        print szMutex + "mutex release failed."
        return 0
    szT = string.split(szLogLines, '\n')[2]
   # print szT
    successLine = string.split( szT, ' ')
   # print successLine
    if successLine[0] == "Success:":
        print szMutex + " mutex released."
        return 1
    else:
        print szMutex + " mutex release failed."
        return 0

def Integrate( szBranch ):
    return os.popen('p4 integ -b ' + szBranch + ' -1 -i' ).read()

def Revert( szFiles ):
    return os.popen('p4 revert ' + szFiles).read()

def Resolve():
    return os.popen('p4 resolve -am').read()

def Fstat( szFiles ):
    szConflicts = os.popen('p4 fstat -Ru ' + szFiles).read()
    return szConflicts

def Changes( szFile, iNumResults ):
    return os.popen('p4 changes -m' + iNumResults + ' ' + szFile).read()

def Query( szMutex ):
    szLines = os.popen( 'p4mutex query ' + szMutex ).read()
    count = string.count( szLines, 'HELD' )
    if (count == 0 ):
        return 1
    else:
        print "Lock held"
        return 0

def SetClient( szClient ):
    os.environ['P4CLIENT'] = szClient
