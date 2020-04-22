//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmxloader/dmxelement.h"
#include <ctype.h>
#include "tier1/utlbuffer.h"
#include "tier1/utlbufferutil.h"
#include <limits.h>
#include "dmxserializationdictionary.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CUtlBuffer;
extern const char *g_pAttributeTypeName[AT_TYPE_COUNT];


//-----------------------------------------------------------------------------
// a simple class to keep track of a stack of valid parsed symbols
//-----------------------------------------------------------------------------
class CDmxKeyValues2ErrorStack
{
public:
	CDmxKeyValues2ErrorStack();

	// Sets the filename to report with errors; sets the line number to 0
	void SetFilename( const char *pFilename );

	// Current line control
	void IncrementCurrentLine();
	void SetCurrentLine( int nLine );
	int GetCurrentLine() const;

	// entering a new keyvalues block, save state for errors
	// Not save symbols instead of pointers because the pointers can move!
	int Push( CUtlSymbol symName );

	// exiting block, error isn't in this block, remove.
	void Pop();

	// Allows you to keep the same stack level, but change the name as you parse peers
	void Reset( int stackLevel, CUtlSymbol symName );

	// Hit an error, report it and the parsing stack for context
	void ReportError( const char *pError, ... );

	static CUtlSymbolTable& GetSymbolTable() { return m_ErrorSymbolTable; }

private:
	enum
	{
		MAX_ERROR_STACK = 64
	};

	CUtlSymbol	m_errorStack[MAX_ERROR_STACK];
	const char *m_pFilename;
	int		m_nFileLine;
	int		m_errorIndex;
	int		m_maxErrorIndex;

	static CUtlSymbolTable m_ErrorSymbolTable;
};


CUtlSymbolTable CDmxKeyValues2ErrorStack::m_ErrorSymbolTable;


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CDmxKeyValues2ErrorStack g_KeyValues2ErrorStack;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDmxKeyValues2ErrorStack::CDmxKeyValues2ErrorStack() : 
	m_pFilename("NULL"), m_errorIndex(0), m_maxErrorIndex(0), m_nFileLine(1) 
{
}


//-----------------------------------------------------------------------------
// Sets the filename
//-----------------------------------------------------------------------------
void CDmxKeyValues2ErrorStack::SetFilename( const char *pFilename )
{
	m_pFilename = pFilename;
	m_maxErrorIndex = 0;
	m_nFileLine = 1;
}


//-----------------------------------------------------------------------------
// Current line control
//-----------------------------------------------------------------------------
void CDmxKeyValues2ErrorStack::IncrementCurrentLine()
{
	++m_nFileLine;
}

void CDmxKeyValues2ErrorStack::SetCurrentLine( int nLine )
{
	m_nFileLine = nLine;
}

int CDmxKeyValues2ErrorStack::GetCurrentLine() const
{
	return m_nFileLine;
}


//-----------------------------------------------------------------------------
// entering a new keyvalues block, save state for errors
// Not save symbols instead of pointers because the pointers can move!
//-----------------------------------------------------------------------------
int CDmxKeyValues2ErrorStack::Push( CUtlSymbol symName )
{
	if ( m_errorIndex < MAX_ERROR_STACK )
	{
		m_errorStack[m_errorIndex] = symName;
	}
	m_errorIndex++;
	m_maxErrorIndex = max( m_maxErrorIndex, (m_errorIndex-1) );
	return m_errorIndex-1;
}


//-----------------------------------------------------------------------------
// exiting block, error isn't in this block, remove.
//-----------------------------------------------------------------------------
void CDmxKeyValues2ErrorStack::Pop()
{
	m_errorIndex--;
	Assert(m_errorIndex>=0);
}


//-----------------------------------------------------------------------------
// Allows you to keep the same stack level, but change the name as you parse peers
//-----------------------------------------------------------------------------
void CDmxKeyValues2ErrorStack::Reset( int stackLevel, CUtlSymbol symName )
{
	Assert( stackLevel >= 0 && stackLevel < m_errorIndex );
	m_errorStack[stackLevel] = symName;
}


//-----------------------------------------------------------------------------
// Hit an error, report it and the parsing stack for context
//-----------------------------------------------------------------------------
void CDmxKeyValues2ErrorStack::ReportError( const char *pFmt, ... )
{
	char temp[2048];

	va_list args;
	va_start( args, pFmt );
	Q_vsnprintf( temp, sizeof( temp ), pFmt, args );
	va_end( args );

	Warning( "%s(%d) : %s\n", m_pFilename, m_nFileLine, temp );

	for ( int i = 0; i < m_maxErrorIndex; i++ )
	{
		if ( !m_errorStack[i].IsValid() )
			continue;

		if ( i < m_errorIndex )
		{
			Warning( "%s, ", GetSymbolTable().String( m_errorStack[i] ) );
		}
		else
		{
			Warning( "(*%s*), ", GetSymbolTable().String( m_errorStack[i] ) );
		}
	}
	Warning( "\n" );
}


//-----------------------------------------------------------------------------
// a simple helper that creates stack entries as it goes in & out of scope
//-----------------------------------------------------------------------------
class CKeyValues2ErrorContext
{
public:
	CKeyValues2ErrorContext( const char *pSymName )
	{
		Init( CDmxKeyValues2ErrorStack::GetSymbolTable().AddString( pSymName ) );
	}

	CKeyValues2ErrorContext( CUtlSymbol symName )
	{
		Init( symName );
	}

	~CKeyValues2ErrorContext()
	{
		g_KeyValues2ErrorStack.Pop();
	}

	void Reset( CUtlSymbol symName )
	{
		g_KeyValues2ErrorStack.Reset( m_stackLevel, symName );
	}

private:
	void Init( CUtlSymbol symName )
	{
		m_stackLevel = g_KeyValues2ErrorStack.Push( symName );
	}

	int m_stackLevel;
};


//-----------------------------------------------------------------------------
// Element dictionary used in unserialization
//-----------------------------------------------------------------------------
typedef int DmxElementDictHandle_t;
enum
{
	ELEMENT_DICT_HANDLE_INVALID = (DmxElementDictHandle_t)~0
};


class CDmxElementDictionary
{
public:
	CDmxElementDictionary();

	DmxElementDictHandle_t InsertElement( CDmxElement *pElement );
	CDmxElement *GetElement( DmxElementDictHandle_t handle );
	void AddAttribute( CDmxAttribute *pAttribute, const DmObjectId_t &pElementId );
	void AddArrayAttribute( CDmxAttribute *pAttribute, DmxElementDictHandle_t hChild );
	void AddArrayAttribute( CDmxAttribute *pAttribute, const DmObjectId_t &pElementId );

	// Finds an element into the table
	DmxElementDictHandle_t FindElement( CDmxElement *pElement );
	DmxElementDictHandle_t FindElement( const DmObjectId_t &objectId );

	// Sets the element id for an element
	void SetElementId( DmxElementDictHandle_t hElement, const DmObjectId_t &objectId );

	// Hook up all element references (which were unserialized as object ids)
	void HookUpElementReferences();

	// Clears the dictionary
	void Clear();

	// iteration through elements
	DmxElementDictHandle_t FirstElement() { return 0; }
	DmxElementDictHandle_t NextElement( DmxElementDictHandle_t h )
	{
		return m_Dict.IsValidIndex( h+1 ) ? h+1 : ELEMENT_DICT_HANDLE_INVALID;
	}

private:
	struct DictInfo_t
	{
		CDmxElement *m_pElement;
		DmObjectId_t m_Id;
	};

	struct AttributeInfo_t
	{
		CDmxAttribute *m_pAttribute;
		DmAttributeType_t m_nType;	// AT_ELEMENT or AT_OBJECTID
		union
		{
			DmxElementDictHandle_t m_hElement;
			DmObjectId_t m_ObjectId;
		};
	};
	typedef CUtlVector<AttributeInfo_t> AttributeList_t;

	// Hook up all element references (which were unserialized as object ids)
	void HookUpElementAttributes();
	void HookUpElementArrayAttributes();

	CUtlVector< DictInfo_t > m_Dict;
	AttributeList_t m_Attributes;
	AttributeList_t m_ArrayAttributes;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDmxElementDictionary::CDmxElementDictionary()
{
}


//-----------------------------------------------------------------------------
// Clears the dictionary
//-----------------------------------------------------------------------------
void CDmxElementDictionary::Clear()
{
	m_Dict.Purge();
	m_Attributes.Purge();
	m_ArrayAttributes.Purge();
}


//-----------------------------------------------------------------------------
// Inserts an element into the table
//-----------------------------------------------------------------------------
DmxElementDictHandle_t CDmxElementDictionary::InsertElement( CDmxElement *pElement )
{
	// Insert it into the reconnection table
	DmxElementDictHandle_t h = m_Dict.AddToTail( );
	m_Dict[h].m_pElement = pElement;
	InvalidateUniqueId( &m_Dict[h].m_Id );
	return h;
}


//-----------------------------------------------------------------------------
// Sets the element id for an element
//-----------------------------------------------------------------------------
void CDmxElementDictionary::SetElementId( DmxElementDictHandle_t hElement, const DmObjectId_t &objectId )
{
	Assert( hElement != ELEMENT_DICT_HANDLE_INVALID );
	CopyUniqueId( objectId, &m_Dict[hElement].m_Id );
}


//-----------------------------------------------------------------------------
// Returns a particular element
//-----------------------------------------------------------------------------
CDmxElement *CDmxElementDictionary::GetElement( DmxElementDictHandle_t handle )
{
	if ( handle == ELEMENT_DICT_HANDLE_INVALID )
		return NULL;

	return m_Dict[ handle ].m_pElement;
}


//-----------------------------------------------------------------------------
// Adds an attribute to the fixup list
//-----------------------------------------------------------------------------
void CDmxElementDictionary::AddAttribute( CDmxAttribute *pAttribute, const DmObjectId_t &objectId )
{
	int i = m_Attributes.AddToTail();
	m_Attributes[i].m_nType = AT_OBJECTID;
	m_Attributes[i].m_pAttribute = pAttribute;
	CopyUniqueId( objectId, &m_Attributes[i].m_ObjectId );
}


//-----------------------------------------------------------------------------
// Adds an element of an attribute array to the fixup list
//-----------------------------------------------------------------------------
void CDmxElementDictionary::AddArrayAttribute( CDmxAttribute *pAttribute, DmxElementDictHandle_t hElement )
{
	int i = m_ArrayAttributes.AddToTail();
	m_ArrayAttributes[i].m_nType = AT_ELEMENT;
	m_ArrayAttributes[i].m_pAttribute = pAttribute;
	m_ArrayAttributes[i].m_hElement = hElement;
}

void CDmxElementDictionary::AddArrayAttribute( CDmxAttribute *pAttribute, const DmObjectId_t &objectId )
{
	int i = m_ArrayAttributes.AddToTail();
	m_ArrayAttributes[i].m_nType = AT_OBJECTID;
	m_ArrayAttributes[i].m_pAttribute = pAttribute;
	CopyUniqueId( objectId, &m_ArrayAttributes[i].m_ObjectId );
}


//-----------------------------------------------------------------------------
// Finds an element into the table
//-----------------------------------------------------------------------------
DmxElementDictHandle_t CDmxElementDictionary::FindElement( CDmxElement *pElement )
{
	int nCount = m_Dict.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pElement == m_Dict[i].m_pElement )
			return i;
	}
	return ELEMENT_DICT_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Finds an element into the table
//-----------------------------------------------------------------------------
DmxElementDictHandle_t CDmxElementDictionary::FindElement( const DmObjectId_t &objectId )
{
	int nCount = m_Dict.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( IsUniqueIdEqual( objectId, m_Dict[i].m_Id ) )
			return i;
	}
	return ELEMENT_DICT_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Hook up all element references (which were unserialized as object ids)
//-----------------------------------------------------------------------------
void CDmxElementDictionary::HookUpElementAttributes()
{
	int n = m_Attributes.Count();
	for ( int i = 0; i < n; ++i )
	{
		Assert( m_Attributes[i].m_nType == AT_OBJECTID );

		DmxElementDictHandle_t hElement = FindElement( m_Attributes[i].m_ObjectId );
		CDmxElement *pElement = GetElement( hElement );
		m_Attributes[i].m_pAttribute->SetValue( pElement );
	}
}


//-----------------------------------------------------------------------------
// Hook up all element array references
//-----------------------------------------------------------------------------
void CDmxElementDictionary::HookUpElementArrayAttributes()
{
	int n = m_ArrayAttributes.Count();
	for ( int i = 0; i < n; ++i )
	{
		CUtlVector< CDmxElement* > &array = m_ArrayAttributes[i].m_pAttribute->GetArrayForEdit<CDmxElement*>();

		if ( m_ArrayAttributes[i].m_nType == AT_ELEMENT )
		{
			CDmxElement *pElement = GetElement( m_ArrayAttributes[i].m_hElement );
			array.AddToTail( pElement );
		}
		else
		{
			// search id->handle table (both loaded and unloaded) for id, and if not found, create a new handle, map it to the id and return it
			DmxElementDictHandle_t hElement = FindElement( m_ArrayAttributes[i].m_ObjectId );
			CDmxElement *pElement = GetElement( hElement );
			array.AddToTail( pElement );
		}
	}
}


//-----------------------------------------------------------------------------
// Hook up all element references (which were unserialized as object ids)
//-----------------------------------------------------------------------------
void CDmxElementDictionary::HookUpElementReferences()
{
	HookUpElementArrayAttributes();
	HookUpElementAttributes();
}



//-----------------------------------------------------------------------------
// Unserialization class for Key Values 2
//-----------------------------------------------------------------------------
class CDmxSerializerKeyValues2
{
public:
	bool Unserialize( const char *pFileName, CUtlBuffer &buf, CDmxElement **ppRoot );
	bool Serialize( CUtlBuffer &buf, CDmxElement *pRoot, const char *pFileName );

private:
	enum TokenType_t
	{
		TOKEN_INVALID = -1,		// A bogus token
		TOKEN_OPEN_BRACE,		// {
		TOKEN_CLOSE_BRACE,		// }
		TOKEN_OPEN_BRACKET,		// [
		TOKEN_CLOSE_BRACKET,	// ]
		TOKEN_COMMA,			// ,
//		TOKEN_STRING,			// Any non-quoted string
		TOKEN_DELIMITED_STRING,	// Any quoted string
		TOKEN_INCLUDE,			// #include
		TOKEN_EOF,				// End of buffer
	};

	// Methods related to unserialization
	void EatWhitespacesAndComments( CUtlBuffer &buf );
	TokenType_t ReadToken( CUtlBuffer &buf, CUtlBuffer &token );
	DmxElementDictHandle_t CreateDmxElement( const char *pElementType );
	bool UnserializeAttributeValueFromToken( CDmxAttribute *pAttribute, DmAttributeType_t type, CUtlBuffer &tokenBuf );
	bool UnserializeElementAttribute( CUtlBuffer &buf, DmxElementDictHandle_t hElement, const char *pAttributeName, const char *pElementType );
	bool UnserializeElementArrayAttribute( CUtlBuffer &buf, DmxElementDictHandle_t hElement, const char *pAttributeName );
	bool UnserializeArrayAttribute( CUtlBuffer &buf, DmxElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType );
	bool UnserializeAttribute( CUtlBuffer &buf, DmxElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType );
	bool UnserializeElement( CUtlBuffer &buf, const char *pElementType, DmxElementDictHandle_t *pHandle );
	bool UnserializeElement( CUtlBuffer &buf, DmxElementDictHandle_t *pHandle );

	// Methods related to serialization
	void SerializeArrayAttribute( CUtlBuffer& buf, CDmxAttribute *pAttribute );
	void SerializeElementAttribute( CUtlBuffer& buf, CDmxSerializationDictionary &dict, CDmxAttribute *pAttribute );
	void SerializeElementArrayAttribute( CUtlBuffer& buf, CDmxSerializationDictionary &dict, CDmxAttribute *pAttribute );
	bool SerializeAttributes( CUtlBuffer& buf, CDmxSerializationDictionary &dict, CDmxElement *pElement );
	bool SaveElement( CUtlBuffer& buf, CDmxSerializationDictionary &dict, CDmxElement *pElement, bool bWriteDelimiters = true );

	// For unserialization
	CDmxElementDictionary m_ElementDict;
	DmxElementDictHandle_t m_hRoot;
};


//-----------------------------------------------------------------------------
// Serializes a single element attribute
//-----------------------------------------------------------------------------
void CDmxSerializerKeyValues2::SerializeElementAttribute( CUtlBuffer& buf, CDmxSerializationDictionary &dict, CDmxAttribute *pAttribute )
{
	CDmxElement *pElement = pAttribute->GetValue< CDmxElement* >();
	if ( dict.ShouldInlineElement( pElement ) )
	{
		buf.Printf( "\"%s\"\n{\n", pElement->GetTypeString() );
		if ( pElement )
		{
			SaveElement( buf, dict, pElement, false );
		}
		buf.Printf( "}\n" );
	}
	else
	{
		buf.Printf( "\"%s\" \"", g_pAttributeTypeName[ AT_ELEMENT ] );
		if ( pElement )
		{
			::Serialize( buf, pElement->GetId() );
		}
		buf.PutChar( '\"' );
	}
}


//-----------------------------------------------------------------------------
// Serializes an array element attribute
//-----------------------------------------------------------------------------
void CDmxSerializerKeyValues2::SerializeElementArrayAttribute( CUtlBuffer& buf, CDmxSerializationDictionary &dict, CDmxAttribute *pAttribute )
{
	const CUtlVector<CDmxElement*> &array = pAttribute->GetArray< CDmxElement* >();

	buf.Printf( "\n[\n" );
	buf.PushTab();

	int nCount = array.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pElement = array[i];
		if ( dict.ShouldInlineElement( pElement ) )
		{
			buf.Printf( "\"%s\"\n{\n", pElement->GetTypeString() );
			if ( pElement )
			{
				SaveElement( buf, dict, pElement, false );
			}
			buf.PutChar( '}' );
		}
		else
		{
			const char *pAttributeType = g_pAttributeTypeName[ AT_ELEMENT ];
			buf.Printf( "\"%s\" \"", pAttributeType );
			if ( pElement )
			{
				::Serialize( buf, pElement->GetId() );
			}
			buf.PutChar( '\"' );
		}

		if ( i != nCount - 1 )
		{
			buf.PutChar( ',' );
		}
		buf.PutChar( '\n' );
	}

	buf.PopTab();
	buf.Printf( "]" );
}


//-----------------------------------------------------------------------------
// Serializes array attributes
//-----------------------------------------------------------------------------
void CDmxSerializerKeyValues2::SerializeArrayAttribute( CUtlBuffer& buf, CDmxAttribute *pAttribute )
{
	int nCount = pAttribute->GetArrayCount();

	buf.PutString( "\n[\n" );
	buf.PushTab();

	for ( int i = 0; i < nCount; ++i )
	{
		if ( pAttribute->GetType() != AT_STRING_ARRAY )
		{
			buf.PutChar( '\"' );
			buf.PushTab();
		}

		pAttribute->SerializeElement( i, buf );

		if ( pAttribute->GetType() != AT_STRING_ARRAY )
		{
			buf.PopTab();
			buf.PutChar( '\"' );
		}

		if ( i != nCount - 1 )
		{
			buf.PutChar( ',' );
		}
		buf.PutChar( '\n' );
	}
	buf.PopTab();
	buf.PutChar( ']' );
}


//-----------------------------------------------------------------------------
// Serializes all attributes in an element
//-----------------------------------------------------------------------------
static int SortAttributeByName(const void *p1, const void *p2 )
{
	const CDmxAttribute **ppAtt1 = (const CDmxAttribute**)p1;
	const CDmxAttribute **ppAtt2 = (const CDmxAttribute**)p2;
	const char *pAttName1 = (*ppAtt1)->GetName();
	const char *pAttName2 = (*ppAtt2)->GetName();
	return Q_stricmp( pAttName1, pAttName2 );
}

bool CDmxSerializerKeyValues2::SerializeAttributes( CUtlBuffer& buf, CDmxSerializationDictionary &dict, CDmxElement *pElement )
{
	int nCount = pElement->AttributeCount();
	CDmxAttribute **ppAttributes = (CDmxAttribute**)stackalloc( nCount * sizeof(CDmxAttribute*) );
	for ( int i = 0; i < nCount; ++i )
	{
		ppAttributes[i] = pElement->GetAttribute( i );
	}

	// Sort by name
	qsort( ppAttributes, nCount, sizeof(CDmxAttribute*), SortAttributeByName );

	for ( int i = 0; i < nCount; ++i )
	{
		CDmxAttribute *pAttribute = ppAttributes[ i ];

		const char *pName = pAttribute->GetName( );
		DmAttributeType_t nAttrType = pAttribute->GetType();
		if ( nAttrType != AT_ELEMENT )
		{
			buf.Printf( "\"%s\" \"%s\" ", pName, g_pAttributeTypeName[ nAttrType ] );
		}
		else
		{
			// Elements either serialize their type name or "element" depending on whether they are inlined
			buf.Printf( "\"%s\" ", pName );
		}

		switch( nAttrType )
		{
		default:
			if ( nAttrType >= AT_FIRST_ARRAY_TYPE )
			{
				SerializeArrayAttribute( buf, pAttribute );
			}
			else
			{
				if ( pAttribute->SerializesOnMultipleLines() )
				{
					buf.PutChar( '\n' );
				}

				buf.PutChar( '\"' );
				buf.PushTab();
				pAttribute->Serialize( buf );
				buf.PopTab();
				buf.PutChar( '\"' );
			}
			break;

		case AT_STRING:
			// Don't explicitly add string delimiters; serialization does that.
			pAttribute->Serialize( buf );
			break;

		case AT_ELEMENT:
			SerializeElementAttribute( buf, dict, pAttribute );
			break;

		case AT_ELEMENT_ARRAY:
			SerializeElementArrayAttribute( buf, dict, pAttribute );
			break;
		}

		buf.PutChar( '\n' );
	}

	return true;
}

bool CDmxSerializerKeyValues2::SaveElement( CUtlBuffer& buf, CDmxSerializationDictionary &dict, CDmxElement *pElement, bool bWriteDelimiters )
{
	if ( bWriteDelimiters )
	{
		buf.Printf( "\"%s\"\n{\n", pElement->GetTypeString() );
	}
	buf.PushTab();

	// explicitly serialize id, now that it's no longer an attribute
	buf.Printf( "\"id\" \"%s\" ", g_pAttributeTypeName[ AT_OBJECTID ] );
	buf.PutChar( '\"' );
	::Serialize( buf, pElement->GetId() );
	buf.PutString( "\"\n" );

	SerializeAttributes( buf, dict, pElement );

	buf.PopTab();
	if ( bWriteDelimiters )
	{
		buf.Printf( "}\n" );
	}
	return true;
}

bool CDmxSerializerKeyValues2::Serialize( CUtlBuffer &outBuf, CDmxElement *pRoot, const char *pFormatName )
{
	SetSerializationDelimiter( GetCStringCharConversion() );
	SetSerializationArrayDelimiter( "," );

	bool bFlatMode = !Q_stricmp( pFormatName, "keyvalues2_flat" );

	// Save elements, attribute links
	CDmxSerializationDictionary dict;
	dict.BuildElementList( pRoot, bFlatMode );

	// Save elements to buffer
	DmxSerializationHandle_t i;
	for ( i = dict.FirstRootElement(); i != ELEMENT_DICT_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		SaveElement( outBuf, dict, dict.GetRootElement( i ) );
		outBuf.PutChar( '\n' );
	}

	SetSerializationDelimiter( NULL );
	SetSerializationArrayDelimiter( NULL );

	return true;
}


//-----------------------------------------------------------------------------
// Eats whitespaces and c++ style comments
//-----------------------------------------------------------------------------
#pragma warning (disable:4706)

void CDmxSerializerKeyValues2::EatWhitespacesAndComments( CUtlBuffer &buf )
{
	// eating white spaces and remarks loop
	int nMaxPut = buf.TellMaxPut() - buf.TellGet();
	int nOffset = 0;
	while ( nOffset < nMaxPut )	
	{
		// Eat whitespaces, keep track of line count
		const char *pPeek = NULL;
		while ( (pPeek = (const char *)buf.PeekGet( sizeof(char), nOffset ) ) )
		{
			if ( !isspace( *pPeek ) )
				break;

			if ( *pPeek == '\n' )
			{
				g_KeyValues2ErrorStack.IncrementCurrentLine();
			}
			if ( ++nOffset >= nMaxPut )
				break;
		}

		// If we don't have a a c++ style comment next, we're done
		pPeek = (const char *)buf.PeekGet( 2 * sizeof(char), nOffset );
		if ( ( nOffset >= nMaxPut ) || !pPeek || ( pPeek[0] != '/' ) || ( pPeek[1] != '/' ) )
			break;

		// Deal with c++ style comments
		nOffset += 2;

		// read complete line
		while ( ( pPeek = (const char *)buf.PeekGet( sizeof(char), nOffset ) ) )
		{
			if ( *pPeek == '\n' )
				break;
			if ( ++nOffset >= nMaxPut )
				break;
		}

		g_KeyValues2ErrorStack.IncrementCurrentLine();
	}

	buf.SeekGet( CUtlBuffer::SEEK_CURRENT, nOffset );
}

#pragma warning (default:4706)

//-----------------------------------------------------------------------------
// Reads a single token, points the token utlbuffer at it
//-----------------------------------------------------------------------------
CDmxSerializerKeyValues2::TokenType_t CDmxSerializerKeyValues2::ReadToken( CUtlBuffer &buf, CUtlBuffer &token )
{
	EatWhitespacesAndComments( buf );

	// if message text buffers go over this size
 	// change this value to make sure they will fit
 	// affects loading of last active chat window
	if ( !buf.IsValid() || ( buf.TellGet() == buf.TellMaxPut() ) )
		return TOKEN_EOF;

	// Compute token length and type
	int nLength = 0;
	TokenType_t t = TOKEN_INVALID;
	char c = *((const char *)buf.PeekGet());
	switch( c )
	{
	case '{':
		nLength = 1;
		t = TOKEN_OPEN_BRACE;
		break;

	case '}':
		nLength = 1;
		t = TOKEN_CLOSE_BRACE;
		break;

	case '[':
		nLength = 1;
		t = TOKEN_OPEN_BRACKET;
		break;

	case ']':
		nLength = 1;
		t = TOKEN_CLOSE_BRACKET;
		break;

	case ',':
		nLength = 1;
		t = TOKEN_COMMA;
		break;

	case '\"':
		// NOTE: The -1 is because peek includes room for the /0
		nLength = buf.PeekDelimitedStringLength( GetCStringCharConversion(), false ) - 1;
		if ( (nLength <= 1) || ( *(const char *)buf.PeekGet( nLength - 1 ) != '\"' ))
		{
			g_KeyValues2ErrorStack.ReportError( "Unexpected EOF in quoted string" );
			t = TOKEN_INVALID;
		}
		else
		{
			t = TOKEN_DELIMITED_STRING;
		}
		break;

	default:
		t = TOKEN_INVALID;
		break;
	}

	// Point the token buffer to the token + update the original buffer get index
	token.SetExternalBuffer( (void*)buf.PeekGet(), nLength, nLength, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
	buf.SeekGet( CUtlBuffer::SEEK_CURRENT, nLength );

	// Count the number of crs in the token + update the current line
	const char *pMem = (const char *)token.Base();
	for ( int i = 0; i < nLength; ++i )
	{
		if ( pMem[i] == '\n' )
		{
			g_KeyValues2ErrorStack.IncrementCurrentLine();
		}
	}

	return t;
}


//-----------------------------------------------------------------------------
// Creates a scene object, adds it to the element dictionary
//-----------------------------------------------------------------------------
DmxElementDictHandle_t CDmxSerializerKeyValues2::CreateDmxElement( const char *pElementType )
{
	// See if we can create an element of that type
	CDmxElement *pElement = new CDmxElement( pElementType );
	return m_ElementDict.InsertElement( pElement );
}


//-----------------------------------------------------------------------------
// Reads an attribute for an element
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeElementAttribute( CUtlBuffer &buf, DmxElementDictHandle_t hElement, const char *pAttributeName, const char *pElementType )
{
	CDmxElement *pElement = m_ElementDict.GetElement( hElement );
	if ( pElement->HasAttribute( pAttributeName ) )
	{
		g_KeyValues2ErrorStack.ReportError( "Attribute \"%s\" was defined more than once.\n", pAttributeName );
		return false;
	}
	
	CDmxAttribute *pAttribute;
	{
		CDmxElementModifyScope modify( pElement );
		pAttribute = pElement->AddAttribute( pAttributeName );
	}

	DmxElementDictHandle_t h;
	bool bOk = UnserializeElement( buf, pElementType, &h );
	if ( bOk )
	{
		CDmxElement *pNewElement = m_ElementDict.GetElement( h );
		pAttribute->SetValue( pNewElement );
	}
	return bOk;
}


//-----------------------------------------------------------------------------
// Reads an attribute for an element array
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeElementArrayAttribute( CUtlBuffer &buf, DmxElementDictHandle_t hElement, const char *pAttributeName )
{
	CDmxElement *pElement = m_ElementDict.GetElement( hElement );
	if ( pElement->HasAttribute( pAttributeName ) )
	{
		g_KeyValues2ErrorStack.ReportError( "Attribute \"%s\" was defined more than once.\n", pAttributeName );
		return false;
	}

	CDmxAttribute *pAttribute;
	{
		CDmxElementModifyScope modify( pElement );
		pAttribute = pElement->AddAttribute( pAttributeName );
	}

	// Arrays first must have a '[' specified
	TokenType_t token;
	CUtlBuffer tokenBuf;
	CUtlCharConversion *pConv;
	token = ReadToken( buf, tokenBuf );
	if ( token != TOKEN_OPEN_BRACKET )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting '[', didn't find it!" );
		return false;
	}

	int nElementIndex = 0;

	// Now read a list of array values, separated by commas
	while ( buf.IsValid() )
	{
		token = ReadToken( buf, tokenBuf );
		if ( token == TOKEN_INVALID || token == TOKEN_EOF )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting ']', didn't find it!" );
			return false;
		}

		// Then, keep reading until we hit a ']'
		if ( token == TOKEN_CLOSE_BRACKET )
			break;

		// If we've already read in an array value, we need to read a comma next
		if ( nElementIndex > 0 )
		{
			if ( token != TOKEN_COMMA )
			{
				g_KeyValues2ErrorStack.ReportError( "Expecting ',', didn't find it!" );
				return false;
			}

			// Read in the next thing, which should be a value
			token = ReadToken( buf, tokenBuf );
		}

		// Ok, we must be reading an array type value
		if ( token != TOKEN_DELIMITED_STRING )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting element type, didn't find it!" );
			return false;
		}

		// Get the element type out
		pConv = GetCStringCharConversion();
		int nLength = tokenBuf.PeekDelimitedStringLength( pConv );
		char *pElementType = (char*)stackalloc( nLength * sizeof(char) );
		tokenBuf.GetDelimitedString( pConv, pElementType, nLength );

		// Use the element type to figure out if we're using a element reference or an inlined element
		if ( !Q_strncmp( pElementType, g_pAttributeTypeName[AT_ELEMENT], nLength ) )
		{
			token = ReadToken( buf, tokenBuf );

			// Ok, we must be reading an array type value
			if ( token != TOKEN_DELIMITED_STRING )
			{
				g_KeyValues2ErrorStack.ReportError( "Expecting element reference, didn't find it!" );
				return false;
			}

			// Get the element type out
			pConv = GetCStringCharConversion();
			nLength = tokenBuf.PeekDelimitedStringLength( pConv );
			char *pElementId = (char*)stackalloc( nLength  * sizeof(char) );
			tokenBuf.GetDelimitedString( pConv, pElementId, nLength );

			DmObjectId_t id;
			if ( !UniqueIdFromString( &id, pElementId ) )
			{
				g_KeyValues2ErrorStack.ReportError( "Encountered invalid element ID data!" );
				return false;
			}

			Assert( IsUniqueIdValid( id ) );
			m_ElementDict.AddArrayAttribute( pAttribute, id );
		}
		else
		{
			DmxElementDictHandle_t hArrayElement;
			bool bOk = UnserializeElement( buf, pElementType, &hArrayElement );
			if ( !bOk )
				return false;
			m_ElementDict.AddArrayAttribute( pAttribute, hArrayElement );
		}

		// Ok, we've read in another value
		++nElementIndex;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Unserializes an attribute from a token buffer
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeAttributeValueFromToken( CDmxAttribute *pAttribute, DmAttributeType_t type, CUtlBuffer &tokenBuf )
{
	// NOTE: This code is necessary because the attribute code is using Scanf
	// which is not really friendly toward delimiters, so we must pass in
	// non-delimited buffers. Sucky. There must be a better way of doing this
	const char *pBuf = (const char*)tokenBuf.Base();
	int nLength = tokenBuf.TellMaxPut();
	char *pTemp = (char*)stackalloc( nLength + 1 );

	bool bIsString = ( type == AT_STRING ) || ( type == AT_STRING_ARRAY );
	if ( !bIsString )
	{
		nLength = tokenBuf.PeekDelimitedStringLength( GetCStringCharConversion() );
		tokenBuf.GetDelimitedString( GetCStringCharConversion(), pTemp, nLength + 1 );
		pBuf = pTemp;
	}
	else
	{
		SetSerializationDelimiter( GetCStringCharConversion() );
	}

	bool bOk;
	CUtlBuffer buf( pBuf, nLength, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
	if ( type < AT_FIRST_ARRAY_TYPE )
	{
		bOk = pAttribute->Unserialize( type, buf );
	}
	else
	{
		bOk = pAttribute->UnserializeElement( type, buf );
	}

	if ( bIsString )
	{
		SetSerializationDelimiter( NULL );
	}

	return bOk;
}


//-----------------------------------------------------------------------------
// Reads an attribute for an element array
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeArrayAttribute( CUtlBuffer &buf, DmxElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType )
{
	CDmxElement *pElement = m_ElementDict.GetElement( hElement );
	if ( pElement->HasAttribute( pAttributeName ) )
	{
		g_KeyValues2ErrorStack.ReportError( "Encountered duplicate attribute definition for attribute \"%s\"!", pAttributeName );
		return false;
	}

	CDmxAttribute *pAttribute;
	{
		CDmxElementModifyScope modify( pElement );
		pAttribute = pElement->AddAttribute( pAttributeName );
		}

	// Arrays first must have a '[' specified
	TokenType_t token;
	CUtlBuffer tokenBuf;
	token = ReadToken( buf, tokenBuf );
	if ( token != TOKEN_OPEN_BRACKET )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting '[', didn't find it!" );
		return false;
	}

	int nElementIndex = 0;

	// Now read a list of array values, separated by commas
	while ( buf.IsValid() )
	{
		token = ReadToken( buf, tokenBuf );
		if ( token == TOKEN_INVALID || token == TOKEN_EOF )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting ']', didn't find it!" );
			return false;
		}

		// Then, keep reading until we hit a ']'
		if ( token == TOKEN_CLOSE_BRACKET )
			break;

		// If we've already read in an array value, we need to read a comma next
		if ( nElementIndex > 0 )
		{
			if ( token != TOKEN_COMMA )
			{
				g_KeyValues2ErrorStack.ReportError( "Expecting ',', didn't find it!" );
				return false;
			}

			// Read in the next thing, which should be a value
			token = ReadToken( buf, tokenBuf );
		}

		// Ok, we must be reading an attributearray value
		if ( token != TOKEN_DELIMITED_STRING )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting array attribute value, didn't find it!" );
			return false;
		}

		if ( !UnserializeAttributeValueFromToken( pAttribute, nAttrType, tokenBuf ) )
		{
			g_KeyValues2ErrorStack.ReportError("Error reading in array attribute \"%s\" element %d", pAttributeName, nElementIndex );
			return false;
		}

		// Ok, we've read in another value
		++nElementIndex;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Reads an attribute for an element
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeAttribute( CUtlBuffer &buf, 
	DmxElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType )
{
	// Read the attribute value
	CUtlBuffer tokenBuf;
	TokenType_t token = ReadToken( buf, tokenBuf );
	if ( token != TOKEN_DELIMITED_STRING )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting quoted attribute value for attribute \"%s\", didn't find one!", pAttributeName );
		return false;
	}

	CDmxElement *pElement = m_ElementDict.GetElement( hElement );
	if ( ( nAttrType == AT_OBJECTID ) && !Q_strnicmp( pAttributeName, "id", 3 ) )
	{
		CUtlCharConversion *pConv = GetCStringCharConversion();
		int nLength = tokenBuf.PeekDelimitedStringLength( pConv );
		char *pElementId = (char*)stackalloc( nLength * sizeof(char) );
		tokenBuf.GetDelimitedString( pConv, pElementId, nLength );

		DmObjectId_t id;
		if ( !UniqueIdFromString( &id, pElementId ) )
		{
			g_KeyValues2ErrorStack.ReportError( "Encountered invalid element ID data!" );
			return false;
		}

		m_ElementDict.SetElementId( hElement, id );
		pElement->SetId( id );
		return true;
	}

	if ( pElement->HasAttribute( pAttributeName ) )
	{
		g_KeyValues2ErrorStack.ReportError( "Encountered duplicate attribute definition for attribute \"%s\"!", pAttributeName );
		return false;
	}

	CDmxAttribute *pAttribute;
	{
		CDmxElementModifyScope modify( pElement );
		pAttribute = pElement->AddAttribute( pAttributeName );
	}

	switch( nAttrType )
	{
	case AT_ELEMENT:
		{
			// Get the attribute value out
			CUtlCharConversion *pConv = GetCStringCharConversion();
			int nLength = tokenBuf.PeekDelimitedStringLength( pConv );
			char *pAttributeValue = (char*)stackalloc( nLength * sizeof(char) );
			tokenBuf.GetDelimitedString( pConv, pAttributeValue, nLength );

			// No string? that's ok, it means we have a NULL pointer
			if ( !pAttributeValue[0] )
				return true;

			DmObjectId_t id;
			if ( !UniqueIdFromString( &id, pAttributeValue ) )
			{
				g_KeyValues2ErrorStack.ReportError("Invalid format for element ID encountered for attribute \"%s\"", pAttributeName );
				return false;
			}

			m_ElementDict.AddAttribute( pAttribute, id );
		}
		return true;

	default:
		if ( UnserializeAttributeValueFromToken( pAttribute, nAttrType, tokenBuf ) )
			return true;

		g_KeyValues2ErrorStack.ReportError("Error reading attribute \"%s\"", pAttributeName );
		return false;
	}
}


//-----------------------------------------------------------------------------
// Unserializes a single element given the type name
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeElement( CUtlBuffer &buf, const char *pElementType, DmxElementDictHandle_t *pHandle )
{
	*pHandle = ELEMENT_DICT_HANDLE_INVALID;

	// Create the element
	DmxElementDictHandle_t hElement = CreateDmxElement( pElementType );

	// Report errors relative to this type name
	CKeyValues2ErrorContext errorReport( pElementType );

	TokenType_t token;
	CUtlBuffer tokenBuf;
	CUtlCharConversion *pConv;
	int nLength;

	// Then we expect a '{'
	token = ReadToken( buf, tokenBuf );
	if ( token != TOKEN_OPEN_BRACE )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting '{', didn't find it!" );
		return false;
	}

	while ( buf.IsValid() )
	{
		token = ReadToken( buf, tokenBuf );
		if ( token == TOKEN_INVALID || token == TOKEN_EOF )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting '}', didn't find it!" );
			return false;
		}

		// Then, keep reading until we hit a '}'
		if ( token == TOKEN_CLOSE_BRACE )
			break;

		// Ok, we must be reading an attribute
		if ( token != TOKEN_DELIMITED_STRING )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting attribute name, didn't find it!" );
			return false;
		}

		// First, read an attribute name
		pConv = GetCStringCharConversion();
		nLength = tokenBuf.PeekDelimitedStringLength( pConv );
		char *pAttributeName = (char*)stackalloc( nLength * sizeof(char) );
		tokenBuf.GetDelimitedString( pConv, pAttributeName, nLength );

		// Next, read an attribute type
		token = ReadToken( buf, tokenBuf );
		if ( token != TOKEN_DELIMITED_STRING )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting attribute type for attribute %s, didn't find it!", pAttributeName );
			return false;
		}

		pConv = GetCStringCharConversion();
		nLength = tokenBuf.PeekDelimitedStringLength( pConv );
		char *pAttributeType = (char*)stackalloc( nLength * sizeof(char) );
		tokenBuf.GetDelimitedString( pConv, pAttributeType, nLength );

		DmAttributeType_t nAttrType = AT_UNKNOWN;
		for ( int i = 0; i < AT_TYPE_COUNT; ++i )
		{
			if ( !Q_stricmp( g_pAttributeTypeName[i], pAttributeType ) )
			{
				nAttrType = (DmAttributeType_t)i;
				break;
			}
		}

		// Next, read an attribute value
		bool bOk = true;
		switch( nAttrType )
		{
		case AT_UNKNOWN:
			bOk = UnserializeElementAttribute( buf, hElement, pAttributeName, pAttributeType );
			break;

		case AT_ELEMENT_ARRAY:
			bOk = UnserializeElementArrayAttribute( buf, hElement, pAttributeName );
			break;

		default:
			if ( nAttrType >= AT_FIRST_ARRAY_TYPE )
			{
				bOk = UnserializeArrayAttribute( buf, hElement, pAttributeName, nAttrType );
			}
			else
			{
				bOk = UnserializeAttribute( buf, hElement, pAttributeName, nAttrType );
			}
			break;
		}

		if ( !bOk )
			return false;
	}

	*pHandle = hElement;
	return true;
}

	
//-----------------------------------------------------------------------------
// Unserializes a single element
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeElement( CUtlBuffer &buf, DmxElementDictHandle_t *pHandle )
{
	*pHandle = ELEMENT_DICT_HANDLE_INVALID;

	// First, read the type name
	CUtlBuffer tokenBuf;
	CUtlCharConversion* pConv;

	TokenType_t token = ReadToken( buf, tokenBuf );
	if ( token == TOKEN_INVALID )
		return false;
	
	if ( token == TOKEN_EOF )
		return true;

	// Get the type name out
	if ( token != TOKEN_DELIMITED_STRING )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting element type name, didn't find it!" );
		return false;
	}

	pConv = GetCStringCharConversion();
	int nLength = tokenBuf.PeekDelimitedStringLength( pConv );
	char *pTypeName = (char*)stackalloc( nLength * sizeof(char) );
	tokenBuf.GetDelimitedString( pConv, pTypeName, nLength );

	return UnserializeElement( buf, pTypeName, pHandle );
}

			   
//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::Unserialize( const char *pFileName, CUtlBuffer &buf, CDmxElement **ppRoot )
{
	*ppRoot = NULL;

	g_KeyValues2ErrorStack.SetFilename( pFileName );
	m_hRoot = ELEMENT_DICT_HANDLE_INVALID;
	m_ElementDict.Clear();

	bool bOk = true;
	while ( buf.IsValid() )
	{
		DmxElementDictHandle_t h;
		bOk = UnserializeElement( buf, &h );
		if ( !bOk || ( h == ELEMENT_DICT_HANDLE_INVALID ) )
			break;

		if ( m_hRoot == ELEMENT_DICT_HANDLE_INVALID )
		{
			m_hRoot = h;
		}
	}

	// do this *before* getting the root, since the first element might be deleted due to id conflicts
	m_ElementDict.HookUpElementReferences();

	*ppRoot = m_ElementDict.GetElement( m_hRoot );
	m_ElementDict.Clear();

	if ( !bOk )
	{
		CleanupDMX( *ppRoot );
		*ppRoot = NULL;
	}

	return bOk;
}


//-----------------------------------------------------------------------------
// Unserialization entry point for text files (assumes version has been stripped)
//-----------------------------------------------------------------------------
bool UnserializeTextDMX( const char *pFileName, CUtlBuffer &buf, CDmxElement **ppRoot )
{
	CDmxSerializerKeyValues2 dmxUnserializer;
	return dmxUnserializer.Unserialize( pFileName, buf, ppRoot );
}


bool SerializeTextDMX( const char *pFileName, CUtlBuffer &buf, CDmxElement *pRoot )
{
	CDmxSerializerKeyValues2 dmxSerializer;
	return dmxSerializer.Serialize( buf, pRoot, pFileName );
}
