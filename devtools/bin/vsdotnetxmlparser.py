
import re
import os, ezxmlfile


# Until I find a better XML parser, this one acts like xml.parsers.expat but it preserves the \r's and \n's inside
# attribute strings in .vcproj files.
class VSDotNetXMLParserError:
	pass


def GetLineNumber( data, filePos ):
	lines = data.split( '\n' )
	testOffset = 0
	for i,x in enumerate( lines ):
		testOffset += len(x) + 1
		if testOffset >= filePos:
			return i+1
	return -1
			

verbose = 0

class VSDotNetXMLParser:
	def __init__( self ):
		self.XmlDeclHandler = None
		self.StartElementHandler = None
		self.EndElementHandler = None

		self.reStartElement = re.compile( r'\s*<(?P<blockName>[^ /\t\n\r\f\v>]+)(?P<noAttrs>>)?' )
		self.reEndElement = re.compile( r'\s*</(?P<blockName>\S+)>' )
		self.reAttribute = re.compile( r'\s*(?P<attrName>\S+)="(?P<attrValue>.*?)"', re.DOTALL )
		self.reEndAttributes = re.compile( r'\s*>' )
		self.reEndAttributesNoSubElements = re.compile( r'\s*(\/|\?)>' )

	def Parse( self, data, final ):
		curFilePos = 0
		elementDepth = 0
		self.__bReadXMLHeader = 0		

		# First read the XML header.
		while 1:		
			m = self.reStartElement.match( data, curFilePos )
			if m:
				curFilePos = m.end()

				# Read the element name and get all its attributes.
				elementName = m.group('blockName')
				attributes = []
				
				# No attributes?
				if m.group('noAttrs') == '>':
					elementDepth += 1
					self.__CallElementHandler( elementName, [], 0 )
					continue

				if verbose:
					print 'elem: ' + elementName

				while 1:
					m = self.reAttribute.match( data, curFilePos )
					if m:
						if verbose:
							print 'attr: %s, value: %s' % (m.group('attrName'), m.group('attrValue'))
						curFilePos = m.end()
						attributes.append( m.group('attrName') )
						attributes.append( m.group('attrValue') )
						continue

					m = self.reEndAttributesNoSubElements.match( data, curFilePos )
					if m:
						if verbose:
							print 'endattr'
						curFilePos = m.end()
						self.__CallElementHandler( elementName, attributes, 1 )
						break

					m = self.reEndAttributes.match( data, curFilePos )
					if m:
						if verbose:
							print 'endattr2'
						curFilePos = m.end()
						elementDepth += 1
						self.__CallElementHandler( elementName, attributes, 0 )
						break
					else:
						raise VSDotNetXMLParserError

			else:
				m = self.reEndElement.match( data, curFilePos )
				if m:
					if verbose:
						print 'endelem'
					curFilePos = m.end()
					elementDepth -= 1
					self.EndElementHandler( '<end element name not supported>' )
				else:
					# When we're done with the file, the depth should be 0.
					if elementDepth != 0:
						print 'line %d, depth: %d' % (GetLineNumber( data, curFilePos ), elementDepth)
						raise VSDotNetXMLParserError
					break

		# Must at least have a header!
		if not self.__bReadXMLHeader:
			raise VSDotNetXMLParserError

	
	def __CallElementHandler( self, elementName, attributes, bEnd ):
		if self.__bReadXMLHeader:
			self.StartElementHandler( elementName, attributes )
			if bEnd:
				self.EndElementHandler( '<end element name not supported>' )
		else:
			# First element must be the XML header.
			if elementName != '?xml' or not bEnd:
				raise VSDotNetXMLParserError
			
			versionString = encodingString = None	
			for(i,a) in enumerate( attributes ):
				if (i & 1) == 0:
					if a == 'version':
						versionString = attributes[i+1]
					elif a == 'encoding':
						encodingString = attributes[i+1]

			if not versionString or not encodingString:
				raise VSDotNetXMLParserError

			self.XmlDeclHandler( versionString, encodingString, 1 )
			self.__bReadXMLHeader = 1

		

def LoadVCProj( filename ):
	f = open( filename, 'rb' )
	return ezxmlfile.EZXMLFile( f.read() )


def FindInList( theList, elem ):
	for i,val in enumerate( theList ):
		if val == elem:
			return i
	return -1


def IsExcludedFromProjects( e, validProjects ):
	for c in e.Children:
		if c.Name == "FileConfiguration":
			if FindInList( validProjects, c.GetAttributeValue( 'Name' ) ) != -1:
				if c.GetAttributeValue( 'ExcludedFromBuild' ) == 'TRUE':
					return 1
	return 0


def StripConfigBlocks_R( e, validProjects ):
	newChildren = []

	# Strip out unwanted configuration blocks.
	if e.Name == 'Configuration' or e.Name == 'FileConfiguration':
		bValid = 0
		v = e.GetAttributeValue( 'Name' )
		for p in validProjects:
			if p == v:
				bValid = 1
				break

		if not bValid:
			return 0

	# Strip out files that are excluded from the validProjects.
	if e.Name == "File":
		if IsExcludedFromProjects( e, validProjects ):
			return 0
	

	# Recurse..
	newChildren = []
	for child in e.Children:
		if StripConfigBlocks_R( child, validProjects ):
			newChildren.append( child )
	e.Children = newChildren
	return 1


def RemoveEmptyFilterBlocks_R( e ):
	if e.Name == "Filter" and len( e.Children ) == 0:
		return 0

	# Recurse..
	newChildren = []
	for child in e.Children:
		if RemoveEmptyFilterBlocks_R( child ):
			newChildren.append( child )
	e.Children = newChildren
	return 1


def WriteSeparateVCProj( f, validProjects, outFilename ):
	outFile = open( outFilename, 'wb' )
	
	# Make a copy of f so we're not trashing its data.
	#f = copy.deepcopy( f )

	# Strip out the source control crap.
	e  = f.GetElement( 'VisualStudioProject' )
	e.RemoveAttribute( 'SccProjectName' )
	e.RemoveAttribute( 'SccAuxPath' )
	e.RemoveAttribute( 'SccLocalPath' )
	e.RemoveAttribute( 'SccProvider' )

	# Now strip out blocks that are for 
	StripConfigBlocks_R( f.RootElement, validProjects )	
	RemoveEmptyFilterBlocks_R( f.RootElement )	

	f.WriteFile( outFile )
	outFile.close()
	print "Wrote %s" % outFilename
