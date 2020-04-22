#====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
#
# Purpose: Syncs, builds and runs tests on the steam code in
#          the currently active p4 clientspec
#
#=============================================================================

# INSTALLATION INSTRUCTIONS:
#
# 1. add the vc compiler solution compiler devenv.exe to the path (defaults to being in "C:\\Program Files\\Microsoft Visual Studio .NET 2003\\Common7\\IDE\\devenv.exe")
# 2. create a P4 clientspec that contains the steam code, eg. //steam/main/...
# 3. make that the default clientspec
# 4. sync to it
# 5. run this build script from inside that directory


import sys, os, string, re, time, smtplib, getopt, P4, SystemHelpers

# config options
g_bTest = 0                                     # 1: disables reporting to SRCDEV; only reports to admin
g_bRunTests = 0                                 # 1: enables testing after build is complete
g_bSync = 0                                     # 1: enables syncing to perforce 
g_bLockMutex = 0                                # 1: require mutex to be free before proceeding to build
g_bBuild = 0                                    # 1: build binaries
g_bDebug = 0                                    # 1: enables debug output
g_bDev = 0                                      # 1: script builds immediately upon start, regardless of presence of perforce state
g_bShaders = 0
g_bXBOX = 0

g_szEmailAlias = ""     # email alias of users interested in build failures
g_szAdministrator = ""   # address to auto-email if the script fails for some reason, test output
g_szMailhost = "" 	# set to hostname of machine running an SMTP server
g_szSenderEmail = ""     # email address that the failure mails come from; TODO: Get buildmachine email 

g_szBuildExe = ""                   # executable for compilation; devenv: .Net, BuildConsole: IncrediBuild
#g_szBuildExe = "devenv"                        # executable for compilation; devenv: .Net, BuildConsole: IncrediBuild
g_szBuildType = ""                      # build type; /rebuild or /build
g_szBuildFlags = ""                        # should be empty for devenv, /all for buildconsole

g_szTestingFile = ""               # name of testing file, located in .\Build Machine Tests\
g_szLocalLogFile = ""               # name of generated log file; currently not used
g_szPublishedErrorsDir = ""  # place to copy log file to reference in failure email
g_szTestDirectory = ""     # directory where the testing files are located

g_szBaseDir = os.getcwd()                       # directory from which the script is launched

g_szP4SrcFilesToWatch = ""     # path to src files to watch
g_szP4ForceFilesToWatch = ""    # path to bin files to watch
g_szP4SyncFilesToWatch = ""
g_szP4Mutex = ""                        # name of perforce mutex to wait for
g_szP4ChangeCounter = ""		# perforce counter containg the changelist number we're verifying
g_szP4VerifiedCounter = ""		# perforce counter the last changelist number we have verified

g_nP4MostRecentCheckin = 0                      # used to keep track of current checkin
g_nP4LastVerifiedCheckin = 0                    # used to keep track of last verified checkin

g_nSleepTimeBetweenChecks = 10                  # number of seconds to wait before checking for a free mutex

g_szBuildName = ""


# mails off a success checkin
def SendSuccessEmail( _szEmail, _szChangeNumber ):
    cMailport = smtplib.SMTP(g_szMailhost)
    szTestString = ""
    if g_bTest:
        szTestString = "TEST"
    print "sending success email to: " + _szEmail

    szMessage = 'From: ' + g_szBuildName + ' Builder ' + ' <' + g_szSenderEmail + '>\n' +\
              'To: ' + szTestString + _szEmail + '\n' +\
              'Subject: [ ' + g_szBuildName + ' build successful ]' +\
              '\n' +\
              '\n' +\
              'Your checkin:\n\n' +\
              _szChangeNumber +\
              '\n\n' +\
              'has been successfully built and verified against all tests.\n'

    if ( g_bTest | g_bDev ):
        cMailport.sendmail( g_szSenderEmail, g_szAdministrator, szMessage )
    else:       
        cMailport.sendmail( g_szSenderEmail, _szEmail, szMessage )
    cMailport.quit()

# print of debugging messages
def dPrint( _szText ):
    if g_bDebug:
        print _szText

# mails off the current error string
def SendFailureEmail( _szErr ):
    cMailport = smtplib.SMTP( g_szMailhost )
    szTestString = ""
    if g_bTest:
        szTestString = "TEST"
    print "sending failure email to: " + g_szEmailAlias
    print "failure reason: " + _szErr

    szMessage = 'From: ' + g_szBuildName + ' Builder' + ' <' + g_szSenderEmail + '>\n' +\
              'To: ' + szTestString + g_szEmailAlias + '\n' +\
              'Subject: [ ' + g_szBuildName + ' branch broken ]' +\
              '\n' +\
              '\n' +\
              _szErr +\
              '\n' +\
              P4.GetUnverifiedCheckins( g_szP4SrcFilesToWatch, g_szP4VerifiedCounter, g_szP4ChangeCounter )              

    if g_bTest | g_bDev:
        cMailport.sendmail( g_szSenderEmail, g_szAdministrator, szMessage )
    else:
        cMailport.sendmail( g_szSenderEmail, szTestString + g_szEmailAlias, szMessage )
    cMailport.quit()

# use to email the admin about any unexpected errors that occur
def ComplainToAdmin( _szErr ):
    cMailport = smtplib.SMTP(g_szMailhost)
    print "sending script failure email to: " + g_szAdministrator
    print "failure reason: |" + _szErr + "|"

    szMessage = 'From: ' + g_szBuildName + " Builder" + ' <' + g_szSenderEmail + '>\n' +\
              'To: ' + g_szAdministrator + '\n' +\
              'Subject: [ ' + g_szBuildName + ' builder build script error ]' +\
              '\n' +\
              '\n' +\
              'error reason:\n' +\
              _szErr +\
              '\n'
              
    cMailport.sendmail(g_szSenderEmail, g_szAdministrator, szMessage)
    cMailport.quit()

# parses out the reason for failure from a test
def GetTestFailureReason( _nBuild ):

    # start our error message
    szMsg = "Test log file:\n"

    # make a directory to copy the error files into, based on the build config and the p4 changelist number
    nBuildNum = P4.GetCounter(g_szP4ChangeCounter)
    szErrDir = g_szPublishedErrorsDir + nBuildNum + "\\" + _nBuild
    os.popen("mkdir " + szErrDir, "r").read()
    
    # copy all the files to the errors dir
    os.popen("copy " + _nBuild + "\\*.*" + " " + szErrDir + " /Y", "r").read()
    # add the error log to the failure message
    szMsg += "    " + szErrDir + "\\" + g_szLocalLogFile + "\n"

    # add the minidumps to the failure message
    rgMinidumps = string.split( os.popen("dir " + _nBuild + "\\*.mdmp /b", "r").read(), "\n" )
    if len(rgMinidumps) > 1:
        szMsg += "\nCrash dump:\n"
        for szMinidump in rgMinidumps:
            if len(szMinidump) > 16:
                szMsg += "    " + szErrDir + "\\" + szMinidump + "\n"
        szMsg += "To view the minidump, click the link above then click \"open\" on the next dialog.\nHit \"F5\" in the now open debugger.  When it asks for the source code, change the start of the suggested path from \"c:\\\" to \"\\\\steam3builder\\\".\n"

    # delete the minidumps, so they don't confuse us next time
    os.popen("del " + _nBuild + "\\*.mdmp", "r").read()
        

    # parse out the error from the logfile 
    szLog = os.popen( "type " + _nBuild + "\\" + szLocalLogFile, "r").read()
    szLogLines = []
    szLogLines = string.split( szLog, "\n" )
    bFoundErr = 0
    for szLine in szLogLines:
        aszToken = string.split(szLine, ' ')
        if len( aszToken ) > 3:
            if aszToken[0] == "***" and aszToken[1] == "TEST" and aszToken[2] == "FAILED":
                # probably an error line, add to the list
                szMsg += szLine
                szMsg += "\n"
                bFoundErr = 1

    # if we haven't found any error lines, use the last line of the file
    if bFoundErr == 0 and len(szLogLines) > 0:
        szMsg += szLogLines[len(szLogLines) - 1]

    return szMsg


# performs a single test run of the specified exe with the specified test parameters
def RunTest( _szCommand ):

    aszParms = string.split( _szCommand, " ", 1)
    print "running " + _szCommand
    print os.popen( _szCommand, "r" ).read()

    # return success
    return aszParms[0]

def RunCompare( _szCommand ):
    szResult = os.popen( _szCommand, "r" ).read()
    return szResult

def SearchFileForErrors( _szFile, _szError ):
    print ("Searching " + _szFile + " for occurences of " + _szError + ".\n")
    bIncludeNext = 0
    szErrorResults = ""
    aszFileLines = string.split( os.popen( "type " + _szFile, "r").read(), "\n" )

    for szLine in aszFileLines:
        if ( bIncludeNext == 1 ):
            szErrorResults += szLine + "\n"
            bIncludeNext = 0
        aszParms = string.split(szLine, " ", 1)
        if (aszParms[0] == _szError):
            #there is a UnitTest error, warning, or assert
            szErrorResults += "\n" + szLine + "\n"
            bIncludeNext = 1;
    return szErrorResults
 
# runs a set of tests as defined by the test script file
def RunTestScript( ):
    print ("Enter Testing")
    # make sure we're in the right dir
    SystemHelpers.ChangeDir("..")
    # load the script
    szReturn = os.popen( "RunTestScripts.py" ).read()
    return szReturn

# runs a single build, and runs the tests on the build if specified
def RunBuild( _szBuild ):
    # launch devstudio to build the solution
    szCmd = _szBuild
    aszToken = string.split(szCmd, " ", 3)

    if aszToken[0] == "devenv":
        #we are building this. Find out what it is.
        if aszToken[2] == "/project":
            #building a project.  Grab it.
            aszToken = string.split(szCmd, " ", 5)
            szCmd = g_szBuildExe + " " + aszToken[1] + " /project " + aszToken[3] + " " + g_szBuildType + " " + aszToken[5] 
        else:
            #not build a specific project.  Probably everything under lostcoast.
            szCmd = g_szBuildExe + " " + aszToken[1] + " " + g_szBuildType + " " + aszToken[3] + " " + g_szBuildFlags
    else:
        #didn't see the devenv line; not building this but it isn't an error either.
        return ""
                
    print "building: " + szCmd
    szOutput = os.popen(szCmd, "r").read()
    # parse the output for any errors
    aszOutputLines = string.split(szOutput, "\n")
    bSuccess = 1    
    szBuildErr = "\n\n\nError building configuration " + _szBuild + ":\n"

    for szLine in aszOutputLines:
        if g_bDebug:
            print szLine;
        aszTokens = string.split(string.lstrip(szLine), ' ', 4)
        if len(aszTokens) > 1:
            if aszTokens[0] == "--------------------Configuration:":
                bPrintedProject = 0
                szProject = aszTokens[1]
            if aszTokens[1] == ":" and aszTokens[2] == "error" and aszTokens[3] != "PRJ0019":
                # probably an error line, add to the list
                count = string.count( szBuildErr, aszTokens[4])
                if ( count == 0 ):
                    if ( bPrintedProject == 0 ):
                        szBuildErr += "Project: " + szProject + "\n"
                        bPrintedProject = 1                    
                    szBuildErr += szLine + "\n"
                #err2 += szLine + "\n"
                bSuccess = 0
            if aszTokens[1] == ":" and aszTokens[2] == "error" and aszTokens[3] == "PRJ0019":
                # the delete error line, add to the list
                szBuildErr += "IGNORE: "
                szBuildErr += szLine + "\n"
                #err2 += szLine + "\n"
        aszTokens = string.split(string.lstrip(szLine), ' ')        
        if len(aszTokens) > 4:
            # can't do this: the weird delete error will trigger this and we need to ignore that
            # check that the "Rebuild All: x succeeded, y failed, z skipped line says no failures
            #if szT[0] == "Rebuild" and szT[3] rrorMessages + "The " + szTestName + " test failed\nThe error is " + szCompareLine
            #if szErrorMessages== "succeeded," and szT[5] == "failed," and not szT[4] == "0":
                    # failure
            #       bSuccess = 0
            # check for linker errors
            if aszTokens[0] == "LINK" and aszTokens[1] == ":" and aszTokens[2] == "fatal" and aszTokens[3] == "error":
                if ( bPrintedProject == 0 ):
                    szBuildErr += "Project: " + szProject + "\n"
                    bPrintedProject = 1 
                # linker error line, add to the list
                szBuildErr += szLine + "\n"
                #err2 += szLine + "\n"
                bSuccess = 0
            # can't do this either:  Delete file problem
            # check the standard error dealie.
            # if szT[1] == '-' and szT[3] == "error(s)," and szT[2] != '0':
                #we have a build error here
            #    err += szLine
            #    err += "\n"
            #check for fatal error
            if aszTokens[2] == "fatal" and aszTokens[3] == "error":
                if ( bPrintedProject == 0 ):
                    szBuildErr += "Project: " + szProject + "\n"
                    bPrintedProject = 1 
                #fatal error
                szBuildErr += szLine + "\n"
                #err2 += szLine + "\n"
                bSuccess = 0
    
    # return immediately if failed    
    if not bSuccess:
        print szBuildErr
        szBuildErr + "\n\n"
        #ComplainToAdmin(err2)
        return szBuildErr

    return ""

def RunBuildBatch():    
    bSuccess = 1
    #aszBatchBuildLines = string.split( os.popen( "type " + szBatchFile, "r").read(), "\n" )
    #bIsDevLine = 0
    szBuildErrs = ""
    szBuildResult = RunBuild( "devenv everything.sln /build \"debug|win32\" " )

    szBuildResult += RunBuild( "devenv everything.sln /build \"release|win32\" " )
    szTestResult = RunTestScript()
    if szBuildResult != "":
        szBuildErrs += szBuildResult
        bSuccess = 0
    if szTestResult != "\n":
        szBuildErrs += szTestResult
        bSuccess = 0
    if not bSuccess:
        SendFailureEmail(szBuildErrs)
    return bSuccess   
    
    
# runs all the builds
def RunAllBuilds():    
    bSuccess = 1
    szBuildErrs = ""
    SystemHelpers.ChangeDir("\game")
    if g_bBuild:
        SystemHelpers.ChangeDir("\src")
                
        # build the shadercompiler and all shaders
        if g_bShaders:
            print( "Compiling shadercompile" )
            os.system( "devenv shadercompile.sln /rebuild release > silence" )
            # should check here to make sure the shadercompile worked
            SystemHelpers.ChangeDir("\\src\\materialsystem\\stdshaders")
            print( "Building shaders" )
            child_stdin, child_stdout, child_stderr = os.popen3( "buildallshaders.bat" )
            print( child_stdout.read() )
            szSomeShaderErr = child_stderr.read()
            szShaderErrors = ""
            aszShaderLines = string.split( szSomeShaderErr, "\n" )
            for szLine in aszShaderLines:
                dPrint( szLine )
                nCount = string.count( szLine, 'U1073:' )
                if nCount > 0:
                    aszToken = string.split( szLine, " " )
                    szShaderName = aszToken[9]
                    szShaderErrors = szShaderErrors + "The shader file " + szShaderName + " is missing and failed during buildallshaders.bat.\n"
                    if szShaderErrors:
                        SendFailureEmail( szShaderErrors )

        SystemHelpers.ChangeDir("\\src")
        bSuccess = bSuccess & RunBuildBatch();

#XBOX Section
        if g_bXBOX:            
            szXBoxOutput = RunBuild( "devenv source_x360.sln /rebuild \"release|xbox 360\"" )
            if "\n\n\nError building configuration " + "devenv source_x360.sln /rebuild release" + ":\n" != szXBoxOutput:
                bSuccess = 0
                SendFailureEmail( szXBoxOutput )
     #delete tier0.dll 
        
        
    if g_bRunTests:
        szTestErrors = RunTestScript()
        if szTestErrors != "\n":
            # success = 0
            ComplainToAdmin( szTestErrors )
    return bSuccess

# builds from a local branch and runs a subset of tests
def PerformSourceBuild():

    # build and test in each configuration
    if not RunAllBuilds():
        print "Source build failed"
        return 0

    print "Source build: SUCCESS"
    return 1
    

# syncs, builds, runs tests
def PerformDailyBuild():
    print "  changes detected, starting daily build"
    # update the counter to be what we're verifying
    change = P4.SubmittedChangelist( g_szP4SrcFilesToWatch )
    g_nP4MostRecentCheckin = change
    g_nP4LastVerifiedCheckin = P4.GetCounter(g_szP4VerifiedCounter)
    if g_nP4MostRecentCheckin and g_nP4LastVerifiedCheckin:
        print "Most recent checkin is " + g_nP4MostRecentCheckin + "\n"
        print "Last verified checkin is " + g_nP4LastVerifiedCheckin + "\n"
    # the p4 command can occasionally fail to deliver a valid changelist number, unclear why
    # can't update the counter, it just means we'll run twice
    if change:
        P4.SetCounter(g_szP4ChangeCounter, change)
    # sync to the new files
    if ( g_bSync ):
        SystemHelpers.ChangeDir("\\src")
        print( "Cleaning\n" )
        os.system("cleanalltargets.bat > silence")
        SystemHelpers.ChangeDir("\\")        
        print "Synching force files."
        P4.Sync( g_szP4ForceFilesToWatch, 1 )
        print "Synching other files."
        P4.Sync( g_szP4SyncFilesToWatch, 0 )
        print( "Setting up VPC" )
        os.system("setupVPC.bat")
        
    #P4.UnlockMutex(g_szP4Mutex)
    # build and test in each configuration
    if not RunAllBuilds():
        print "Daily build failed"
        return
        
    # send a success email, from past the last successful checkin to the current
    if change:
        szVerifiedOrig = P4.GetCounter(g_szP4VerifiedCounter)
        if szVerifiedOrig:
            szVerifiedPlusOne = str( int( szVerifiedOrig ) + 1 )
            changes = P4.GetChangelistRange(szVerifiedPlusOne, change, g_szP4SrcFilesToWatch );
            for ch in changes:
                if len(ch) > 1:
                    szEmail = P4.GetEmailFromChangeLine(ch)
                    SendSuccessEmail(szEmail, ch)
                    #SendSuccessEmail("jason", ch)
            # remember this change that we've verified
            P4.SetCounter(g_szP4VerifiedCounter, change)

    print "Daily build: AN UNEQUIVOCAL SUCCESS"

def PrintConfig():
    print("Configuration:")
    print("test = " + str(g_bTest))
    print("run_tests = " + str(g_bRunTests))
    print("lock_mutex = " + str(g_bLockMutex))
    print("build = " + str(g_bBuild))
    print("debug = " + str(g_bDebug))
    print("dev = " + str(g_bDev))
    print("shaders = " + str(g_bShaders))
    print("sync = " + str(g_bSync))
    print("email_alias = " + g_szEmailAlias)
    print("admin_email = " + g_szAdministrator)
    print("mail_host = " + g_szMailhost)
    print("sender_email = " + g_szSenderEmail)
    print("build_exe = " + g_szBuildExe)
    print("build_type = " + g_szBuildType)
    print("build_flags = " + g_szBuildFlags)
    print("test_file = " + g_szTestingFile)
    print("log_file = " + g_szLocalLogFile)
    print("error_dir = " + g_szPublishedErrorsDir)
    print("test_dir = " + g_szTestDirectory)
    print("src_files = " + g_szP4SrcFilesToWatch)
    print("force_files = " + g_szP4ForceFilesToWatch)
    print("sync_files = " + g_szP4SyncFilesToWatch)
    print("mutex = " + g_szP4Mutex)
    print("change_counter = " + g_szP4ChangeCounter)
    print("verify_counter = " + g_szP4VerifiedCounter)
    print("build_name = " + g_szBuildName)

def ParseConfigFile(configFileName):
     aszBatchBuildLines = string.split( os.popen( "type " + configFileName, "r").read(), "\n" )
     global g_bTest
     global g_bRunTests
     global g_bLockMutex
     global g_bBuild
     global g_bDebug
     global g_bDev
     global g_bShaders
     global g_bSync
     global g_szEmailAlias
     global g_szAdministrator
     global g_szMailhost
     global g_szSenderEmail
     global g_szBuildExe
     global g_szBuildType
     global g_szBuildFlags
     global g_szTestingFile
     global g_szLocalLogFile
     global g_szPublishedErrorsDir
     global g_szTestDirectory
     global g_szP4SrcFilesToWatch
     global g_szP4ForceFilesToWatch
     global g_szP4SyncFilesToWatch
     global g_szP4Mutex
     global g_szP4ChangeCounter
     global g_szP4VerifiedCounter
     global g_szBuildName
     for szLine in aszBatchBuildLines:
         aszTokens = string.split(string.lstrip(szLine), ' ', 2)
         firstToken = aszTokens[0]
         if firstToken == '#' or firstToken == '':
             continue
         secondToken = aszTokens[1]
         if firstToken == "test":
             g_bTest = int(secondToken)
         elif firstToken == "run_tests":
             g_bRunTests = int(secondToken)
         elif firstToken == "lock_mutex":
             g_bLockMutex = int(secondToken)
         elif firstToken == "build":
             g_bBuild = int(secondToken)
         elif firstToken == "debug":
             g_bDebug = int(secondToken)
         elif firstToken == "dev":
             g_bDev = int(secondToken)
         elif firstToken == "shaders":
             g_bShaders = int(secondToken)
         elif firstToken == "sync":
             g_bSync = int(secondToken)
         elif firstToken == "email_alias":
             g_szEmailAlias = secondToken
         elif firstToken == "admin_email":
             g_szAdministrator = secondToken
         elif firstToken == "mail_host":
             g_szMailhost = secondToken
         elif firstToken == "sender_email":
             g_szSenderEmail = secondToken
         elif firstToken == "build_exe":
             g_szBuildExe = secondToken
         elif firstToken == "build_type":
             g_szBuildType = secondToken
         elif firstToken == "build_flags":
             g_szBuildFlags = secondToken
         elif firstToken == "test_file":
             g_szTestingFile = secondToken
         elif firstToken == "log_file":
             g_szLocalLogFile = secondToken
         elif firstToken == "error_dir":
             g_szPublishedErrorsDir = secondToken
         elif firstToken == "test_dir":
             g_szTestDirectory = secondToken
         elif firstToken == "src_files":
             g_szP4SrcFilesToWatch += secondToken 
         elif firstToken == "force_files":
             g_szP4ForceFilesToWatch += secondToken + ";"
         elif firstToken == "sync_files":
             g_szP4SyncFilesToWatch += secondToken + ";"
         elif firstToken == "mutex":
             g_szP4Mutex = secondToken
         elif firstToken == "change_counter":
             g_szP4ChangeCounter = secondToken
         elif firstToken == "verify_counter":
             g_szP4VerifiedCounter = secondToken
         elif firstToken == "build_name":
             g_szBuildName = secondToken
     PrintConfig()

#-----------------------------------------------------------------------------
# Main 
#-----------------------------------------------------------------------------
if __name__ == '__main__':
    try:
                print "----------------------------------------------------"
                print g_szBuildName + " BUILD SCRIPT STARTED"
                ParseConfigFile(sys.argv[1])

                while 1:
                    if (g_bDev | P4.AnyNewCheckins( g_szP4ChangeCounter, g_szP4SrcFilesToWatch )):
                        print "Changes Detected.\n"
                        if ( (g_bTest & ~g_bLockMutex) | ~g_bLockMutex | P4.Query(g_szP4Mutex) ):
                            PerformDailyBuild()
                            g_bDev = 0
                            print ""
                            print "------------------------------------------"
                            print "waiting for changes to be detected..."
                        else:
                            time.sleep( g_nSleepTimeBetweenChecks - ( g_bTest * g_nSleepTimeBetweenChecks ))
                    else:
                        time.sleep( g_nSleepTimeBetweenChecks )
    except RuntimeError, e:
        ComplainToAdmin(e)
        
