
import re


class DLexToken:
	"""
	DLexToken contains:
		'id'  - the ID of the token (match with the value DLexer.AddToken returns).
		'val' - the token's string.
		'lineNumber' - the line the token was encountered on.
	"""
	pass	


class DLexState:
	pass


class DLexer:
	"""
	DLex is a simple lexer simulator. Here is how to use it.
	
	1. Call AddToken to add the regular expressions that it will parse. Add them in 
	   order of precedence. Store the value returned from AddToken so you can compare
	   it to the token ID returned by GetToken to determine what kind of token was found.

	2. Call BeginRead or BeginReadFile to setup the initial file.

	3. Repeatedly call GetToken. 
	       If it returns None, then there are no more tokens that match your specifications. 
		   If it returns a value, then it is a DLexToken with.
	"""

	def __init__( self, bSkipWhitespace=1 ):
		self.__tokens = []
		self.__curTokenID = 0
		self.__notnewline = re.compile( '[^\\r\\n]*' )
		
		self.__bSkipWhitespace = bSkipWhitespace
		if bSkipWhitespace:
			self.__whitespace = re.compile( '[ \\t\\f\\v]+' )
			self.__newline = re.compile( '[\\r\\n]' )


	def GetErrorTokenID( self ):
		return -1
	

	def AddToken( self, expr, flags=0 ):
		tokenID = self.__curTokenID
		self.__tokens.append( [tokenID, re.compile( expr, flags )] )
		self.__curTokenID += 1
		return tokenID


	# Store and restore the state.
	def BackupState( self ):
		ret = DLexState()
		ret.lineNumber = self.__lineNumber
		ret.currentCharacter = self.__currentCharacter
		ret.fileLen = self.__fileLen
		return ret

	def RestoreState( self, state ):
		self.__lineNumber = state.lineNumber
		self.__currentCharacter = state.currentCharacter
		self.__fileLen = state.fileLen


	def BeginRead( self, str ):
		self.__curString = str
		self.__lineNumber = 1
		self.__currentCharacter = 0
		self.__fileLen = len( str )


	def BeginReadFile( self, fileName ):
		file = open( fileName, 'r' )
		self.BeginRead( file.read() )
		file.close()

	
	def GetToken( self ):
		# Skip whitespace.
		self.__SkipWhitespace()

		# Now return the first token that we have a match for.
		for token in self.__tokens:
			m = token[1].match( self.__curString, self.__currentCharacter )
			if m:
				ret = DLexToken()
				ret.id = token[0]
				ret.val = self.__curString[ m.start() : m.end() ]
				ret.lineNumber = self.__lineNumber
				self.__currentCharacter = m.end()
				return ret
		
		if self.__currentCharacter < self.__fileLen:
			print "NO MATCH FOR '%s'" % self.__curString[ self.__currentCharacter : self.__currentCharacter+35 ]
			ret = DLexToken()
			ret.id = self.GetErrorTokenID()
			ret.val = self.__curString[ self.__currentCharacter : ]
			self.__currentCharacter = self.__fileLen
			return ret
			#print "%d" % t

		return None


	def GetLineNumber( self ):
		return self.__lineNumber


	def GetPercentComplete( self ):
		return (self.__currentCharacter * 100) / self.__fileLen


	def GetLineContents( self ):
		m = self.__notnewline.match( self.__curString, self.__currentCharacter )
		if m:
			return self.__curString[ m.start() : m.end() ]
		else:
			return ""


	def __SkipWhitespace( self ):
		if self.__bSkipWhitespace:
			while 1:
				a = self.__whitespace.match( self.__curString, self.__currentCharacter )
				b = self.__newline.match( self.__curString, self.__currentCharacter )
				if a:
					self.__currentCharacter = a.end()
					continue
				elif b:
					self.__currentCharacter = b.end()
					self.__lineNumber += 1
					continue
				else:
					break

