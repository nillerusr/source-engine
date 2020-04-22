
import sys, re, vsdotnetxmlparser



class EZXMLError:
	pass


class EZXMLElement:
	"""
		Members:
			Name		: string
			Attrs		: list with attributes [(name,value), (name, value), ...]
			Children	: list of XMLElements
			Parent		: parent XMLElement
	"""
	def GetAttributeValue( self, attrName ):
		for a in self.Attrs:
			if a[0] == attrName:
				return a[1]
		return None

	def RemoveAttribute( self, attrName ):
		for i,a in enumerate( self.Attrs ):
			if a[0] == attrName:
				self.Attrs.pop( i )
				return 1

		return 0

	def RemoveFromParent( self ):
		if not self.Parent:
			raise EZXMLError

		self.Parent.Children.remove( self )


class EZXMLFile:
	#
	# PUBLIC INTERFACE
	#
	def __init__( self, data ):
		self.Version = ''
		self.Encoding = ''
		self.RootElement = None
		self.__CurElement = None
		self.__ElementIndexRE = re.compile( r'<(?P<index>.+)>(?P<name>.+)' )

		#p = xml.parsers.expat.ParserCreate()
		p = vsdotnetxmlparser.VSDotNetXMLParser()

		p.ordered_attributes = 1
		p.XmlDeclHandler = self.__xml_decl_handler
		p.StartElementHandler = self.__start_element
		p.EndElementHandler = self.__end_element

		p.Parse( data, 1 )


	# Write the current contents to a file.
	def WriteFile( self, file ):
		file.write( '<?xml version="%s" encoding="%s"?>\r\n' % (self.Version, self.Encoding) )
		if self.RootElement:
			self.__WriteFile_R( file, self.RootElement, 0 )

	"""
		Find an element (starting at the root).
		If elementName has backslashes in it (like 'VisualStudioProject\Configurations\Tool'), then it will recurse into each element.
		Each element can also have an optional count telling which element to find:
			'VisualStudioProject\Configurations\<3>Tool'
	"""
	def GetElement( self, elementName ):
		if not self.RootElement:
			return None

		pathList = elementName.split( '\\' )
		return self.__GetElementByPath_R( [self.RootElement], pathList, 0 )

	
	"""
		AttributePath is specified the same way as GetElement.
		Returns None if the path to the element or attribute doesn't exist.
	"""
	def GetAttribute( self, attributePath ):
		if not self.RootElement:
			return None

		pathList = attributePath.split( '\\' )
		attributeName = pathList.pop()
		e = self.__GetElementByPath_R( [self.RootElement], pathList, 0 )
		if e:
			return e.GetAttributeValue( attributeName )
		else:
			return None	



	#
	# INTERNAL STUFF
	#
	def __GetElementByPath_R( self, curElementList, pathList, index ):
		indexToFind = 1
		indexFound = 0
		m = self.__ElementIndexRE.match( pathList[index] )
		if m:
			nameToFind = m.group('name').upper()
			indexToFind = int( m.group('index') )
		else:
			nameToFind = pathList[index].upper()

		for x in curElementList:
			if x.Name.upper() == nameToFind:
				indexFound += 1
				if indexFound == indexToFind:
					if index == len( pathList ) - 1:
						return x
					else:
						return self.__GetElementByPath_R( x.Children, pathList, index+1 )

		return None

	def __WriteFile_R( self, file, curElement, indentLevel ):
		tabChars = ""
		for i in xrange(0,indentLevel):
			tabChars = tabChars + '\t'
		file.write( tabChars )

		keyListLen = len( curElement.Attrs )
		if keyListLen == 0:
			file.write( '<%s>\r\n' % (curElement.Name) )
		else:
			file.write( '<%s\r\n' % (curElement.Name) )
		
		bClosedBlock = 0
		for i,e in enumerate( curElement.Attrs ):
			file.write( tabChars + '\t' )
			file.write( '%s="%s"' % (e[0], e[1]) )

			# VS.NET XML files use this special syntax on elements that have attributes and no children.
			if len( curElement.Children ) == 0 and i == keyListLen-1 and curElement.Name != 'File':
				file.write( '/>\r\n' )
				bClosedBlock = 1
			elif i == keyListLen-1:
				file.write( '>\r\n' )
			else:
				file.write( '\r\n' )
		
		for child in curElement.Children:
			self.__WriteFile_R( file, child, indentLevel+1 )

		if not bClosedBlock:
			file.write( tabChars )
			file.write( '</%s>\r\n' % (curElement.Name) )
			
	# XML parser handler functions
	def __xml_decl_handler( self, version, encoding, standalone ):
		self.Version = version
		self.Encoding = encoding

	def __start_element( self, name, attrs ):
		e = EZXMLElement()
		e.Name = name
		e.Attrs = zip( attrs[::2], attrs[1::2] )  # Cool slicing trick.. this takes a list like [1,2,3,4] and makes it into [[1,2], [3,4]]
		e.Children = None
		e.Parent = self.__CurElement
		e.Children = []
		if self.__CurElement:
			self.__CurElement.Children.append( e )
		else:
			self.RootElement = e
		self.__CurElement = e			
	
	def __end_element( self, name ):
		self.__CurElement = self.__CurElement.Parent
