

# Assuming all functions begin with ')' followed by '{', just find the matching brace and
# add a line with 'g_pVCR->SyncToken("<random string here>");'

import dlexer
import sys


class BlankStruct:
	pass


def MatchParensBack( list, iStart ):
	parenCount = -1
	for i in range( 0, iStart ):
		if list[iStart-i].id == __TOKEN_OPENPAREN:
			parenCount += 1
		elif list[iStart-i].id == __TOKEN_CLOSEPAREN:
			parenCount -= 1
		
		if parenCount == 0:
			return iStart - i
	
	return -1


if len( sys.argv ) >= 2:

	# Setup the parser.
	parser = dlexer.DLexer( 0 )

	__TOKEN_NEWLINE = parser.AddToken( '\n' )
	__TOKEN_WHITESPACE = parser.AddToken( '[ \\t\\f\\v]+' )
	__TOKEN_OPENBRACE  = parser.AddToken( '{' )
	__TOKEN_CLOSEBRACE = parser.AddToken( '}' )
	__TOKEN_OPENPAREN  = parser.AddToken( '\(' )
	__TOKEN_CLOSEPAREN = parser.AddToken( '\)' )
	__TOKEN_COMMENT = parser.AddToken( r"\/\/.*" )
	
	__TOKEN_CONST = parser.AddToken( "const" )
	__TOKEN_IF = parser.AddToken( "if" )
	__TOKEN_WHILE = parser.AddToken( "while" )
	__TOKEN_FOR = parser.AddToken( "for" )
	__TOKEN_SWITCH = parser.AddToken( "switch" )
	
	validChars = r"\~\@\#\$\%\^\&\!\,\w\.-/\[\]\<\>\""
	__TOKEN_IDENT = parser.AddToken( '[' + validChars + ']+' )
	__TOKEN_OPERATOR = parser.AddToken( "\=|\+" )
	__TOKEN_SCOPE_OPERATOR = parser.AddToken( "::" )
	__TOKEN_IGNORE = parser.AddToken( r"\#|\;|\:|\||\?|\'|\\|\*|\-|\`" )

	head = None

	# First, read all the tokens into a list.
	list = []
	parser.BeginReadFile( sys.argv[1] )
	while 1:
		m = parser.GetToken()
		if m:
			list.append( m )
		else:
			break

	
	# Make a list of all the non-whitespace ones.
	nw = []
	for token in list:
		if token.id == __TOKEN_NEWLINE or token.id == __TOKEN_WHITESPACE:
			token.iNonWhitespace = -2222
		else:
			token.iNonWhitespace = len( nw )
			nw.append( token )

	
	# Get ready to output sync tokens.
	file = open( sys.argv[1], 'r' )
	fileLines = file.readlines()
	file.close()

	
	curLine = 1
	iCur = 0

	file = open( sys.argv[1], 'w' )

	# Now, search for the patterns we're interested in.
	# Look for <ident>::<ident> '(' <idents...> ')' followed by a '{'. This would be a function.
	for token in list:
		file.write( token.val )
		if token.id == __TOKEN_NEWLINE:
			curLine += 1

		if token.id == __TOKEN_OPENBRACE:
			i = token.iNonWhitespace
			if i >= 6:
				if nw[i-1].id == __TOKEN_CLOSEPAREN:
					pos = MatchParensBack( nw, i-2 )
					if pos != -1:
						if nw[pos-1].id == __TOKEN_IDENT:
							#ADD PROLOGUE CODE HERE
							#file.write( "\n\tg_pVCR->SyncToken( \"%d_%s\" ); // AUTO-GENERATED SYNC TOKEN\n" % (iCur, nw[pos-1].val) )
							iCur += 1
							
							# TEST CODE TO PRINT OUT FUNCTION NAMES
							#if nw[pos-2].id == __TOKEN_SCOPE_OPERATOR:
							#	print "%d: %s::%s" % ( curLine, nw[pos-3].val, nw[pos-1].val )
							#else:
							#	print "%d: %s" % ( curLine, nw[pos-1].val )

	file.close()

else:

	print "VCRMode_AddSyncTokens <filename>"

