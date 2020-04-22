#!/usr/bin/env python

import getopt
import os
import re
import sys
import subprocess

import pdb

def usage():
	print >> sys.stderr, 'checks in all open files in the default changelist, aggregating change comments from the provided list'
	print >> sys.stderr, ''
	print >> sys.stderr, 'usage:'
	print >> sys.stderr, sys.argv[0] + ' [-c p4client] [-p p4port] [-u p4user] [-d changelog (prefix)] [--changes "list of change numbers"]'
	print >> sys.stderr, ''
	print >> sys.stderr, 'the list of changes must either be the last argument, or be quoted, so '
	print >> sys.stderr, sys.argv[0] + ' ... --changes 1 2 3  [ok] '
	print >> sys.stderr, sys.argv[0] + ' ... --changes "1 2 3" ... [ok]'
	print >> sys.stderr, sys.argv[0] + ' ... -changes 1 2 3 ... [bad]'
	
def main():
	try:
		opts, args = getopt.getopt( sys.argv[1:], "c:p:u:d:", [ "changes=" ] )
	except getopt.GetoptError, err:
		print >> sys.stderr, str(err)
		usage()
		sys.exit(-1)

	p4user = None
	p4client = None
	p4port = None
	changelog = None
	changes = []
	p4cmdbase = [ "p4" ]
	for opt, arg in opts:
		if opt == "-c":
			p4cmdbase.extend( [ opt, arg ] )
			p4client = arg
		elif opt == "-p":
			p4cmdbase.extend( [ opt, arg ] )
			p4port = arg
		elif opt == "-u":
			p4cmdbase.extend( [ opt, arg ] )
			p4user = arg
		elif opt == "-d":
			# eat this one, we'll build our own changespec
			changelog = arg
		elif opt == "--changes":
			# and these are the change #'s to include descriptions of
			changes = arg.split()
			if args is not None:
				changes.extend( args )

	if p4user is None:
		p4user = os.getenv( "P4USER" )
	if p4client is None:
		p4client = os.getenv( "P4CLIENT" )
	if p4port is None:
		p4port = os.getenv( "P4PORT" )
		
	if p4user is None or p4client is None or p4port is None:
		print >> sys.stderr, "one or more p4 environment variables (p4user, p4client, p4port) aren't set." 
		usage()
		sys.exit(-1)

	
	# get the list of opened files 
	openFiles = []
	stdout = subprocess.Popen( " ".join( p4cmdbase ) + " opened -c default", shell=True, stdout=subprocess.PIPE ).stdout
	lines = stdout.readlines()
	stdout.close()

	filere = re.compile( "(.*)#.*\n" )
	for line in lines:
		m = filere.match( line )
		if m is not None:
			openFiles += [ m.groups()[ 0 ] ]

	if openFiles == []:
		sys.stderr.write( "no files to submit from the default changelist" )
		sys.exit(0)
		
	# get the changenotes from the specified changes
	changeLines = []
	for change in changes:
		stdout = subprocess.Popen( " ".join( p4cmdbase ) + " describe -s " + change, shell=True, stdout=subprocess.PIPE ).stdout
		lines = stdout.readlines()
		stdout.close()

		stopre = re.compile( "Affected files ..." )
		for line in lines:
			if stopre.match( line ):
				break
			changeLines += [ line.strip() ]

	change_spec = ''
	change_spec += 'Change: new\n'
	change_spec += 'Client: %s\n' % p4client
	change_spec += 'User: %s\n' % p4user
	change_spec += 'Description:\n'

	# if they supplied a changelog line, prepend that to the list of changes
	if changelog is not None:
		change_spec += '\t%s\n\n' % changelog
	# and now each line from all the changelists
	if changeLines != []:
		change_spec += '\t%s\n\n' % "Changes included in this submit:"
	for changeLine in changeLines:
		change_spec += '\t%s\n' % changeLine

	change_spec += 'Files:\n'
	for file in openFiles:
		change_spec += '\t%s\n' % file

	p = subprocess.Popen( " ".join( p4cmdbase ) + " submit -i", shell=True, stdin=subprocess.PIPE, stdout=sys.stdout, stderr=sys.stderr )
	p.communicate( change_spec )
	sys.exit( p.returncode )

if __name__ == '__main__':
	main()
