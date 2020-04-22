import os

szBaseDir = os.getcwd()

# changes to a new directory, independent of where we are on the drive
def ChangeDir(szDest):
    os.chdir(szBaseDir + szDest)

# deletes a file of disk, ignoring any assert if the file doesn't exist
def DeleteFile(szFile):
    try:
        os.unlink(szFile)
    except OSError, e:
        pass

# returns true if a file exists on disk
def FileExists(szFile):
    try:
        os.stat(szFile)
        return 1
    except OSError, e:
        return

def ListFiles(szExtension):
    try:
        szAllFiles = os.listdir(os.getcwd())
        return szAllFiles
    except OSError, e:
        return
    
