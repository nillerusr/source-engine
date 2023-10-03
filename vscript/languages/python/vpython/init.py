# Main python module - establishes the root scope for all python vscripts

# hook up to the eclipse pydev debug server
#import pydevd       #path to pydevd.py must be on the PYTHONPATH environment variable
#pydevd.settrace()

#import os
#print("working directory: %s", os.getcwd()) # print working directory
#
#import sys
#print("module search path: %s",sys.path)  # print module search path

#sys.path.append('u:/projects/sob/src/vscript/languages/python/vpython')
#U:\projects\sob\src\vscript\languages\python\vpython
#print "modified module search path: " sys.path

#import ctypes

print "init.py just ran!"

def sayhowdy():
    print "howdy"
    
if __name__ == '__main__':
    print "hello world"