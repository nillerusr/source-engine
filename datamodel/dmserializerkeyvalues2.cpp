//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmserializerkeyvalues2.h"
#include <ctype.h>
#include "datamodel/idatamodel.h"
#include "datamodel.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "dmattributeinternal.h"
#include "dmelementdictionary.h"
#include "DmElementFramework.h"
#include "tier1/utlbuffer.h"
#include <limits.h>


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CUtlBuffer;


//-----------------------------------------------------------------------------
// a simple class to keep track of a stack of valid parsed symbols
//-----------------------------------------------------------------------------
class CKeyValues2ErrorStack
{
public:
	CKeyValues2ErrorStack();

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
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CKeyValues2ErrorStack g_KeyValues2ErrorStack;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CKeyValues2ErrorStack::CKeyValues2ErrorStack() : 
	m_pFilename("NULL"), m_errorIndex(0), m_maxErrorIndex(0), m_nFileLine(1) 
{
}


//-----------------------------------------------------------------------------
// Sets the filename
//-----------------------------------------------------------------------------
void CKeyValues2ErrorStack::SetFilename( const char *pFilename )
{
	m_pFilename = pFilename;
	m_maxErrorIndex = 0;
	m_nFileLine = 1;
}


//-----------------------------------------------------------------------------
// Current line control
//-----------------------------------------------------------------------------
void CKeyValues2ErrorStack::IncrementCurrentLine()
{
	++m_nFileLine;
}

void CKeyValues2ErrorStack::SetCurrentLine( int nLine )
{
	m_nFileLine = nLine;
}

int CKeyValues2ErrorStack::GetCurrentLine() const
{
	return m_nFileLine;
}


//-----------------------------------------------------------------------------
// entering a new keyvalues block, save state for errors
// Not save symbols instead of pointers because the pointers can move!
//-----------------------------------------------------------------------------
int CKeyValues2ErrorStack::Push( CUtlSymbol symName )
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
void CKeyValues2ErrorStack::Pop()
{
	m_errorIndex--;
	Assert(m_errorIndex>=0);
}


//-----------------------------------------------------------------------------
// Allows you to keep the same stack level, but change the name as you parse peers
//-----------------------------------------------------------------------------
void CKeyValues2ErrorStack::Reset( int stackLevel, CUtlSymbol symName )
{
	Assert( stackLevel >= 0 && stackLevel < m_errorIndex );
	m_errorStack[stackLevel] = symName;
}


//-----------------------------------------------------------------------------
// Hit an error, report it and the parsing stack for context
//-----------------------------------------------------------------------------
void CKeyValues2ErrorStack::ReportError( const char *pFmt, ... )
{
	char temp[2048];

	va_list args;
	va_start( args, pFmt );
	Q_vsnprintf( temp, sizeof( temp ), pFmt, args );
	va_end( args );

	char temp2[2048];
	Q_snprintf( temp2, sizeof( temp2 ), "%s(%d) : %s\n", m_pFilename, m_nFileLine, temp );
	Warning( temp2 );

	for ( int i = 0; i < m_maxErrorIndex; i++ )
	{
		if ( !m_errorStack[i].IsValid() )
			continue;

		if ( i < m_errorIndex )
		{
			Warning( "%s, ", g_pDataModel->GetString( m_errorStack[i] ) );
		}
		else
		{
			Warning( "(*%s*), ", g_pDataModel->GetString( m_errorStack[i] ) );
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
		Init( g_pDataModel->GetSymbol( pSymName ) );
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
// Serialization class for Key Values 2
//-----------------------------------------------------------------------------
class CDmSerializerKeyValues2 : public IDmSerializer
{
public:
	CDmSerializerKeyValues2( bool bFlatMode ) : m_bFlatMode( bFlatMode ) {}

	// Inherited from IDMSerializer
	virtual const char *GetName() const { return m_bFlatMode ? "keyvalues2_flat" : "keyvalues2"; }
	virtual const char *GetDescription() const { return m_bFlatMode ? "KeyValues2 (flat)" : "KeyValues2"; }
	virtual bool StoresVersionInFile() const { return true; }
	virtual bool IsBinaryFormat() const { return false; }
	virtual int GetCurrentVersion() const { return 1; }
	virtual bool Serialize( CUtlBuffer &buf, CDmElement *pRoot );
	virtual bool Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
		const char *pSourceFormatName, int nSourceFormatVersion,
		DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot );

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

	// Methods related to serialization
	void SerializeArrayAttribute( CUtlBuffer& buf, CDmAttribute *pAttribute );
	void SerializeElementAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary &dict, CDmAttribute *pAttribute );
	void SerializeElementArrayAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary &dict, CDmAttribute *pAttribute );
	bool SerializeAttributes( CUtlBuffer& buf, CDmElementSerializationDictionary &dict, CDmElement *pElement );
	bool SaveElement( CUtlBuffer& buf, CDmElementSerializationDictionary &dict, CDmElement *pElement, bool bWriteDelimiters = true );

	// Methods related to unserialization
	void EatWhitespacesAndComments( CUtlBuffer &buf );
	TokenType_t ReadToken( CUtlBuffer &buf, CUtlBuffer &token );
	DmElementDictHandle_t CreateDmElement( const char *pElementType );
	bool UnserializeAttributeValueFromToken( CDmAttribute *pAttribute, CUtlBuffer &tokenBuf );
	bool UnserializeElementAttribute( CUtlBuffer &buf, DmElementDictHandle_t hElement, const char *pAttributeName, const char *pElementType );
	bool UnserializeElementArrayAttribute( CUtlBuffer &buf, DmElementDictHandle_t hElement, const char *pAttributeName );
	bool UnserializeArrayAttribute( CUtlBuffer &buf, DmElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType );
	bool UnserializeAttribute( CUtlBuffer &buf, DmElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType );
	bool UnserializeElement( CUtlBuffer &buf, const char *pElementType, DmElementDictHandle_t *pHandle );
	bool UnserializeElement( CUtlBuffer &buf, DmElementDictHandle_t *pHandle );
	bool UnserializeElements( CUtlBuffer &buf, DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot );

	// For unserialization
	CDmElementDictionary m_ElementDict;
	DmElementDictHandle_t m_hRoot;
	bool m_bFlatMode;
	DmConflictResolution_t m_idConflictResolution;
	DmFileId_t m_fileid;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CDmSerializerKeyValues2 s_DMSerializerKeyValues2( false );
static CDmSerializerKeyValues2 s_DMSerializerKeyValues2Flat( true );

void InstallKeyValues2Serializer( IDataModel *pFactory )
{
	pFactory->AddSerializer( &s_DMSerializerKeyValues2 );
	pFactory->AddSerializer( &s_DMSerializerKeyValues2Flat );
}


//-----------------------------------------------------------------------------
// Serializes a single element attribute
//-----------------------------------------------------------------------------
void CDmSerializerKeyValues2::SerializeElementAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary &dict, CDmAttribute *pAttribute )
{
	CDmElement *pElement = pAttribute->GetValueElement<CDmElement>();
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
		buf.Printf( "\"%s\" \"", g_pDataModel->GetAttributeNameForType( AT_ELEMENT ) );
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
void CDmSerializerKeyValues2::SerializeElementArrayAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary &dict, CDmAttribute *pAttribute )
{
	CDmrElementArray<> array( pAttribute );

	buf.Printf( "\n[\n" );
	buf.PushTab();

	int nCount = array.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pElement = array[i];
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
			const char *pAttributeType = AttributeTypeName( AT_ELEMENT );
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
void CDmSerializerKeyValues2::SerializeArrayAttribute( CUtlBuffer& buf, CDmAttribute *pAttribute )
{
	CDmrGenericArray array( pAttribute );
	int nCount = array.Count();

	buf.PutString( "\n[\n" );
	buf.PushTab();

	for ( int i = 0; i < nCount; ++i )
	{
		if ( pAttribute->GetType() != AT_STRING_ARRAY )
		{
			buf.PutChar( '\"' );
			buf.PushTab();
		}

		array.GetAttribute()->SerializeElement( i, buf );

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
bool CDmSerializerKeyValues2::SerializeAttributes( CUtlBuffer& buf, CDmElementSerializationDictionary &dict, CDmElement *pElement )
{
	// Collect the attributes to be written
	CDmAttribute **ppAttributes = ( CDmAttribute** )_alloca( pElement->AttributeCount() * sizeof( CDmAttribute* ) );
	int nAttributes = 0;
	for ( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( pAttribute->IsFlagSet( FATTRIB_DONTSAVE ) )
			continue;

		ppAttributes[ nAttributes++ ] = pAttribute;
	}

	// Now write them all out in reverse order, since FirstAttribute is actually the *last* attribute for perf reasons
	for ( int i = nAttributes - 1; i >= 0; --i )
	{
		CDmAttribute *pAttribute = ppAttributes[ i ];
		Assert( pAttribute );

		const char *pName = pAttribute->GetName( );
		DmAttributeType_t nAttrType = pAttribute->GetType();
		if ( nAttrType != AT_ELEMENT )
		{
			buf.Printf( "\"%s\" \"%s\" ", pName, g_pDataModel->GetAttributeNameForType( nAttrType ) );
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

bool CDmSerializerKeyValues2::SaveElement( CUtlBuffer& buf, CDmElementSerializationDictionary &dict, CDmElement *pElement, bool bWriteDelimiters )
{
	if ( bWriteDelimiters )
	{
		buf.Printf( "\"%s\"\n{\n", pElement->GetTypeString() );
	}
	buf.PushTab();

	// explicitly serialize id, now that it's no longer an attribute
	buf.Printf( "\"id\" \"%s\" ", g_pDataModel->GetAttributeNameForType( AT_OBJECTID ) );
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

bool CDmSerializerKeyValues2::Serialize( CUtlBuffer &outBuf, CDmElement *pRoot )
{
	SetSerializationDelimiter( GetCStringCharConversion() );
	SetSerializationArrayDelimiter( "," );

	// Save elements, attribute links
	CDmElementSerializationDictionary dict;
	dict.BuildElementList( pRoot, m_bFlatMode );

	// Save elements to buffer
	DmElementDictHandle_t i;
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

void CDmSerializerKeyValues2::EatWhitespacesAndComments( CUtlBuffer &buf )
{
	// eating white spaces and remarks loop
	int nMaxPut = buf.TellMaxPut() - buf.TellGet();
	int nOffset = 0;
	while ( nOffset < nMaxPut )	
	{
		// Eat whitespaces, keep track of line count
		const char *pPeek = NULL;
		while ( pPeek = (const char *)buf.PeekGet( sizeof(char), nOffset ) )
		{
			if ( !V_isspace( *pPeek ) )
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
		while ( pPeek = (const char *)buf.PeekGet( sizeof(char), nOffset ) )
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
CDmSerializerKeyValues2::TokenType_t CDmSerializerKeyValues2::ReadToken( CUtlBuffer &buf, CUtlBuffer &token )
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

	token.EnsureCapacity( nLength );
	buf.Get( token.Base(), nLength );
	token.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	token.SeekPut( CUtlBuffer::SEEK_HEAD, nLength );

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
DmElementDictHandle_t CDmSerializerKeyValues2::CreateDmElement( const char *pElementType )
{
	// See if we can create an element of that type
	DmElementHandle_t hElement = g_pDataModel->CreateElement( pElementType, "", m_fileid );
	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		g_KeyValues2ErrorStack.ReportError("Element uses unknown element type %s\n", pElementType );
		return ELEMENT_DICT_HANDLE_INVALID;
	}

	CDmElement *pElement = g_pDataModel->GetElement( hElement );
	CDmeElementAccessor::MarkBeingUnserialized( pElement, true );
	return m_ElementDict.InsertElement( pElement );
}


//-----------------------------------------------------------------------------
// Reads an attribute for an element
//-----------------------------------------------------------------------------
bool CDmSerializerKeyValues2::UnserializeElementAttribute( CUtlBuffer &buf, DmElementDictHandle_t hElement, const char *pAttributeName, const char *pElementType )
{
	CDmElement *pElement = m_ElementDict.GetElement( hElement );
	CDmAttribute *pAttribute = pElement->AddAttribute( pAttributeName, AT_ELEMENT );
	if ( !pAttribute )
	{
		g_KeyValues2ErrorStack.ReportError("Attempted to read an attribute (\"%s\") of unknown type %s!\n", pAttributeName, pElementType );
		return false;
	}

	DmElementDictHandle_t h;
	bool bOk = UnserializeElement( buf, pElementType, &h );
	if ( bOk )
	{
		CDmElement *pNewElement = m_ElementDict.GetElement( h );
		pAttribute->SetValue( pNewElement ? pNewElement->GetHandle() : DMELEMENT_HANDLE_INVALID );
	}
	return bOk;
}


//-----------------------------------------------------------------------------
// Reads an attribute for an element array
//-----------------------------------------------------------------------------
bool CDmSerializerKeyValues2::UnserializeElementArrayAttribute( CUtlBuffer &buf, DmElementDictHandle_t hElement, const char *pAttributeName )
{
	CDmElement *pElement = m_ElementDict.GetElement( hElement );
	CDmAttribute *pAttribute = pElement->AddAttribute( pAttributeName, AT_ELEMENT_ARRAY );
	if ( !pAttribute )
	{
		g_KeyValues2ErrorStack.ReportError("Attempted to read an attribute (\"%s\") of an inappropriate type!\n", pAttributeName );
		return false;
	}

	// Arrays first must have a '[' specified
	TokenType_t token;
	CUtlBuffer tokenBuf( 0, 0, CUtlBuffer::TEXT_BUFFER );
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
		if ( !Q_strncmp( pElementType, g_pDataModel->GetAttributeNameForType( AT_ELEMENT ), nLength ) )
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
			DmElementDictHandle_t hArrayElement;
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
bool CDmSerializerKeyValues2::UnserializeAttributeValueFromToken( CDmAttribute *pAttribute, CUtlBuffer &tokenBuf )
{
	// NOTE: This code is necessary because the attribute code is using Scanf
	// which is not really friendly toward delimiters, so we must pass in
	// non-delimited buffers. Sucky. There must be a better way of doing this
	const char *pBuf = (const char*)tokenBuf.Base();
	int nLength = tokenBuf.TellMaxPut();
	char *pTemp = (char*)stackalloc( nLength + 1 );

	bool bIsString = ( pAttribute->GetType() == AT_STRING ) || ( pAttribute->GetType() == AT_STRING_ARRAY );
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
	if ( pAttribute->GetType() < AT_FIRST_ARRAY_TYPE )
	{
		bOk = pAttribute->Unserialize( buf );
	}
	else
	{
		bOk = pAttribute->UnserializeElement( buf );
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
bool CDmSerializerKeyValues2::UnserializeArrayAttribute( CUtlBuffer &buf, DmElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType )
{
	CDmElement *pElement = m_ElementDict.GetElement( hElement );
	CDmAttribute *pAttribute = pElement->AddAttribute( pAttributeName, nAttrType );
	if ( !pAttribute )
	{
		g_KeyValues2ErrorStack.ReportError("Attempted to read an attribute (\"%s\") of an inappropriate type %s!\n", 
			pAttributeName, g_pDataModel->GetAttributeNameForType( nAttrType ) );
		return false;
	}

	// Arrays first must have a '[' specified
	TokenType_t token;
	CUtlBuffer tokenBuf( 0, 0, CUtlBuffer::TEXT_BUFFER );
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

		if ( !UnserializeAttributeValueFromToken( pAttribute, tokenBuf ) )
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
bool CDmSerializerKeyValues2::UnserializeAttribute( CUtlBuffer &buf, 
	DmElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType )
{
	// Read the attribute value
	CUtlBuffer tokenBuf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	TokenType_t token = ReadToken( buf, tokenBuf );
	if ( token != TOKEN_DELIMITED_STRING )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting quoted attribute value for attribute \"%s\", didn't find one!", pAttributeName );
		return false;
	}

	if ( ( nAttrType == AT_OBJECTID ) && !V_stricmp( pAttributeName, "id" ) )
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

		m_ElementDict.SetElementId( hElement, id, m_idConflictResolution );

		return true;
	}

	CDmElement *pElement = m_ElementDict.GetElement( hElement );
	CDmAttribute *pAttribute = pElement->AddAttribute( pAttributeName, nAttrType );
	if ( !pAttribute )
	{
		g_KeyValues2ErrorStack.ReportError("Attempted to read an attribute (\"%s\") of an inappropriate type %s!\n", 
			pAttributeName, g_pDataModel->GetAttributeNameForType( nAttrType ) );
		return false;
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
		if ( UnserializeAttributeValueFromToken( pAttribute, tokenBuf ) )
			return true;

		g_KeyValues2ErrorStack.ReportError("Error reading attribute \"%s\"", pAttributeName );
		return false;
	}
}


/*
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : includedKeys - 
//-----------------------------------------------------------------------------
void KeyValues::AppendIncludedKeys( CUtlVector< KeyValues * >& includedKeys )
{
	// Append any included keys, too...
	int includeCount = includedKeys.Count();
	int i;
	for ( i = 0; i < includeCount; i++ )
	{
		KeyValues *kv = includedKeys[ i ];
		Assert( kv );

		KeyValues *insertSpot = this;
		while ( insertSpot->GetNextKey() )
		{
			insertSpot = insertSpot->GetNextKey();
		}

		insertSpot->SetNextKey( kv );
	}
}

void KeyValues::ParseIncludedKeys( char const *resourceName, const char *filetoinclude, 
		IBaseFileSystem* pFileSystem, const char *pPathID, CUtlVector< KeyValues * >& includedKeys )
{
	Assert( resourceName );
	Assert( filetoinclude );
	Assert( pFileSystem );
	
	// Load it...
	if ( !pFileSystem )
	{
		return;
	}

	// Get relative subdirectory
	char fullpath[ 512 ];
	Q_strncpy( fullpath, resourceName, sizeof( fullpath ) );

	// Strip off characters back to start or first /
	bool done = false;
	int len = Q_strlen( fullpath );
	while ( !done )
	{
		if ( len <= 0 )
		{
			break;
		}
		
		if ( fullpath[ len - 1 ] == '\\' || 
			 fullpath[ len - 1 ] == '/' )
		{
			break;
		}

		// zero it
		fullpath[ len - 1 ] = 0;
		--len;
	}

	// Append included file
	Q_strncat( fullpath, filetoinclude, sizeof( fullpath ), COPY_ALL_CHARACTERS );

	KeyValues *newKV = new KeyValues( fullpath );

	// CUtlSymbol save = s_CurrentFileSymbol;	// did that had any use ???

	newKV->UsesEscapeSequences( m_bHasEscapeSequences );	// use same format as parent

	if ( newKV->LoadFromFile( pFileSystem, fullpath, pPathID ) )
	{
		includedKeys.AddToTail( newKV );
	}
	else
	{
		DevMsg( "KeyValues::ParseIncludedKeys: Couldn't load included keyvalue file %s\n", fullpath );
		newKV->deleteThis();
	}

	// s_CurrentFileSymbol = save;
}

//-----------------------------------------------------------------------------
// Read from a buffer...
//-----------------------------------------------------------------------------
bool KeyValues::LoadFromBuffer( char const *resourceName, const char *pBuffer, IBaseFileSystem* pFileSystem , const char *pPathID )
{
	char *pfile = const_cast<char *>(pBuffer);

	KeyValues *pPreviousKey = NULL;
	KeyValues *pCurrentKey = this;
	CUtlVector< KeyValues * > includedKeys;
	bool wasQuoted;
	g_KeyValues2ErrorStack.SetFilename( resourceName );	
	do 
	{
		// the first thing must be a key
		const char *s = ReadToken( &pfile, wasQuoted );
		
		if ( !pfile || !s || *s == 0 )
			break;

		if ( !Q_stricmp( s, "#include" ) )	// special include macro (not a key name)
		{
			s = ReadToken( &pfile, wasQuoted );
			// Name of subfile to load is now in s

			if ( !s || *s == 0 )
			{
				g_KeyValues2ErrorStack.ReportError("#include is NULL " );
			}
			else
			{
				ParseIncludedKeys( resourceName, s, pFileSystem, pPathID, includedKeys );
			}

			continue;
		}

		if ( !pCurrentKey )
		{
			pCurrentKey = new KeyValues( s );
			Assert( pCurrentKey );

			pCurrentKey->UsesEscapeSequences( m_bHasEscapeSequences ); // same format has parent use

			if ( pPreviousKey )
			{
				pPreviousKey->SetNextKey( pCurrentKey );
			}
		}
		else
		{
			pCurrentKey->SetName( s );
		}

		// get the '{'
		s = ReadToken( &pfile, wasQuoted );

		if ( s && *s == '{' && !wasQuoted )
		{
			// header is valid so load the file
			pCurrentKey->RecursiveLoadFromBuffer( resourceName, &pfile );
		}
		else
		{
			g_KeyValues2ErrorStack.ReportError("LoadFromBuffer: missing {" );
		}

		pPreviousKey = pCurrentKey;
		pCurrentKey = NULL;
	} while ( pfile != NULL );

	AppendIncludedKeys( includedKeys );

	g_KeyValues2ErrorStack.SetFilename( "" );	

	return true;
}
*/


//-----------------------------------------------------------------------------
// Unserializes a single element given the type name
//-----------------------------------------------------------------------------
bool CDmSerializerKeyValues2::UnserializeElement( CUtlBuffer &buf, const char *pElementType, DmElementDictHandle_t *pHandle )
{
	*pHandle = ELEMENT_DICT_HANDLE_INVALID;

	// Create the element
	DmElementDictHandle_t hElement = CreateDmElement( pElementType );
	if ( hElement == ELEMENT_DICT_HANDLE_INVALID )
		return false;

	// Report errors relative to this type name
	CKeyValues2ErrorContext errorReport( pElementType );

	TokenType_t token;
	CUtlBuffer tokenBuf( 0, 0, CUtlBuffer::TEXT_BUFFER );
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
		DmAttributeType_t nAttrType = g_pDataModel->GetAttributeTypeForName( pAttributeType );

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
bool CDmSerializerKeyValues2::UnserializeElement( CUtlBuffer &buf, DmElementDictHandle_t *pHandle )
{
	*pHandle = ELEMENT_DICT_HANDLE_INVALID;

	// First, read the type name
	CUtlBuffer tokenBuf( 0, 0, CUtlBuffer::TEXT_BUFFER );
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
bool CDmSerializerKeyValues2::Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
										   const char *pSourceFormatName, int nSourceFormatVersion,
										   DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot )
{
	bool bSuccess = UnserializeElements( buf, fileid, idConflictResolution, ppRoot );
	if ( !bSuccess )
		return false;

	return g_pDataModel->UpdateUnserializedElements( pSourceFormatName, nSourceFormatVersion, fileid, idConflictResolution, ppRoot );
}

bool CDmSerializerKeyValues2::UnserializeElements( CUtlBuffer &buf, DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot )
{
	*ppRoot = NULL;

	m_idConflictResolution = idConflictResolution;

	m_fileid = fileid;

	g_KeyValues2ErrorStack.SetFilename( g_pDataModel->GetFileName( fileid ) );
	m_hRoot = ELEMENT_DICT_HANDLE_INVALID;
	m_ElementDict.Clear();

	bool bOk = true;
	while ( buf.IsValid() )
	{
		DmElementDictHandle_t h;
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

	// mark all unserialized elements as done unserializing, and call Resolve()
	for ( DmElementDictHandle_t h = m_ElementDict.FirstElement();
		h != ELEMENT_DICT_HANDLE_INVALID;
		h = m_ElementDict.NextElement( h ) )
	{
		CDmElement *pElement = m_ElementDict.GetElement( h );
		if ( !pElement )
			continue;

		CDmeElementAccessor::MarkBeingUnserialized( pElement, false );
	}

	m_fileid = DMFILEID_INVALID;

	g_pDmElementFrameworkImp->RemoveCleanElementsFromDirtyList( );
	m_ElementDict.Clear();
	return bOk;
}
