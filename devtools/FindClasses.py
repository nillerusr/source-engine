

# Assuming all functions begin with ')' followed by '{', just find the matching brace and
# add a line with 'g_pVCR->SyncToken("<random string here>");'

import dlexer
import sys
import WildcardSearch


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
__TOKEN_CLASS = parser.AddToken( "class" )
__TOKEN_PUBLIC = parser.AddToken( "public" )
__TOKEN_TYPEDEF = parser.AddToken( "typedef" )
__TOKEN_BASECLASS = parser.AddToken( "BaseClass" )

validChars = r"\~\@\#\$\%\^\&\!\w\.-/\[\]\<\>\""
__TOKEN_IDENT = parser.AddToken( '[' + validChars + ']+' )
__TOKEN_OPERATOR = parser.AddToken( "\=|\+" )
__TOKEN_SCOPE_OPERATOR = parser.AddToken( "::" )
__TOKEN_COLON = parser.AddToken( ":" )
__TOKEN_IGNORE = parser.AddToken( r"\#|\;|\:|\||\?|\'|\\|\*|\-|\`|\," )

for i in range( 1, len( sys.argv ) ):
	for filename in WildcardSearch.WildcardSearch( sys.argv[i] ):

		head = None

		# First, read all the tokens into a list.
		list = []
		parser.BeginReadFile( filename )
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

		curLine = 1
			
		# Now, search for the patterns we're interested in.
		# Look for 'class <ident> : public <ident> { 
		curClassName = ""
		curBaseClassName = ""
		for token in list:
			if token.id == __TOKEN_NEWLINE:
				curLine += 1
			elif token.id == __TOKEN_CLASS:
				i = token.iNonWhitespace
				if nw[i+1].id == __TOKEN_IDENT and nw[i+2].id == __TOKEN_COLON and nw[i+3].id == __TOKEN_PUBLIC and nw[i+4].id == __TOKEN_IDENT:
					curClassName = nw[i+1].val
					curBaseClassName = nw[i+4].val
					print "class %s : public %s" % (curClassName, curBaseClassName)

