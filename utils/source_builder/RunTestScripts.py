
import sys, os, string, re, time, smtplib, getopt, P4, SystemHelpers

def RunResultingTestFiles(listTestFiles, szErrorResults ):
    while len(listTestFiles):
        szFilename = listTestFiles.pop()
        os.system( szFilename )
        szErrorResult = os.popen( "type errors.txt" ).read()        
        if szErrorResult:
            szErrorResult = "Script failed: " + szFilename + "\n" + szErrorResult
            os.remove("errors.txt")
        szErrorResults += szErrorResult
    return szErrorResults


if __name__ == '__main__':
    szErrorResults = ""
    SystemHelpers.ChangeDir("\\src\\unittests\\autotestscripts\\")
    os.environ['PATH'] += os.pathsep + "d:\\main\\src\\devtools\\bin\\" + \
                          os.pathsep + "d:\\main\\game\\bin\\"
    listTestFiles = SystemHelpers.ListFiles(".py")
    szErrorResults += RunResultingTestFiles(listTestFiles, szErrorResults)
    listTestFiles = SystemHelpers.ListFiles(".cmd")
    szErrorResults += RunResultingTestFiles(listTestFiles, szErrorResults)
    listTestFiles = SystemHelpers.ListFiles(".bat")
    szErrorResults += RunResultingTestFiles(listTestFiles, szErrorResults)
    listTestFiles = SystemHelpers.ListFiles(".pl")
    szErrorResults += RunResultingTestFiles(listTestFiles, szErrorResults)


    print szErrorResults

