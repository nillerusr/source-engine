//========= Copyright © 1996-2010, Valve LLC, All rights reserved. ============
//
// Purpose: Implementation for CWebAPIResponse objects
//
//=============================================================================

#include "stdafx.h"
#include "thirdparty/JSON_parser/JSON_parser.h"

using namespace GCSDK;

#include "tier0/memdbgoff.h"

// !FIXME! DOTAMERGE
//IMPLEMENT_CLASS_MEMPOOL_MT( CWebAPIValues, 1000, UTLMEMORYPOOL_GROW_SLOW );

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Helper for emitting properly escaped json string values
//-----------------------------------------------------------------------------
void EmitJSONString( CUtlBuffer &outputBuffer, const char *pchValue )
{
	outputBuffer.PutChar( '"' );

	if ( pchValue )
	{
		int i = 0;
		while( pchValue[i] )
		{
			switch ( pchValue[i] )
			{
			case '"':
				outputBuffer.Put( "\\\"", 2 );
				break;
			case '\\':
				outputBuffer.Put( "\\\\", 2 );
				break;
			case '\n':
				outputBuffer.Put( "\\n", 2 );
				break;
			case '\r':
				outputBuffer.Put( "\\r", 2 );
				break;
			case '\t':
				outputBuffer.Put( "\\t", 2 );
				break;
			default:
				if ( (uint8) pchValue[i] < 32 )
				{
					outputBuffer.Put( "\\u00", 4 );
					outputBuffer.PutChar( ( pchValue[i] & 16 ) ? '1' : '0' );
					outputBuffer.PutChar( "0123456789abcdef"[ pchValue[i] & 0xF ] );
				}
				else
				{
					outputBuffer.PutChar( pchValue[i] );
				}
			}
			++i;
		}
	}

	outputBuffer.PutChar( '"' );
}


//-----------------------------------------------------------------------------
// Purpose: Helper for emitting properly escaped XML string values, we always use UTF8,
// so we only really need to encode & ' " < >
//-----------------------------------------------------------------------------
void EmitXMLString( CUtlBuffer &outputBuffer, const char *pchValue )
{
	if ( pchValue )
	{
		int i = 0;
		while( pchValue[i] )
		{
			switch ( pchValue[i] )
			{
			case '&':
				outputBuffer.Put( "&amp;", 5 );
				break;
			case '\'':
				outputBuffer.Put( "&apos;", 6 );
				break;
			case '"':
				outputBuffer.Put( "&quot;", 6 );
				break;
			case '<':
				outputBuffer.Put( "&lt;", 4 );
				break;
			case '>':
				outputBuffer.Put( "&gt;", 4 );
				break;
			default:
				outputBuffer.PutChar( pchValue[i] );
			}
			++i;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper for emitting properly escaped VDF string values, we always use UTF8,
// and we escape only " and \
//-----------------------------------------------------------------------------
void EmitVDFString( CUtlBuffer &outputBuffer, const char *pchValue )
{
	outputBuffer.PutChar( '"' );

	if ( pchValue )
	{
		int i = 0;
		while( pchValue[i] )
		{
			switch ( pchValue[i] )
			{
			case '\\':
				outputBuffer.Put( "\\\\", 2 );
				break;
			case '"':
				outputBuffer.Put( "\\\"", 2 );
				break;
			default:
				outputBuffer.PutChar( pchValue[i] );
			}
			++i;
		}
	}

	outputBuffer.PutChar( '"' );
}

namespace GCSDK
{

enum { k_LineBreakEveryNGroups = 18 }; // line break every 18 groups of 4 characters (every 72 characters)

uint32 Base64EncodeMaxOutput( const uint32 cubData, const char *pszLineBreak )
{
	// terminating null + 4 chars per 3-byte group + line break after every 18 groups (72 output chars) + final line break
	uint32 nGroups = (cubData+2)/3;
	uint32 cchRequired = 1 + nGroups*4 + ( pszLineBreak ? Q_strlen(pszLineBreak)*(1+(nGroups-1)/k_LineBreakEveryNGroups) : 0 );
	return cchRequired;
}

bool Base64Encode( const uint8 *pubData, uint32 cubData, char *pchEncodedData, uint32 *pcchEncodedData, const char *pszLineBreak )
{
	if ( pchEncodedData == NULL )
	{
		AssertMsg( *pcchEncodedData == 0, "NULL output buffer with non-zero size passed to Base64Encode" );
		*pcchEncodedData = Base64EncodeMaxOutput( cubData, pszLineBreak );
		return true;
	}
	
	const uint8 *pubDataEnd = pubData + cubData;
	char *pchEncodedDataStart = pchEncodedData;
	uint32 unLineBreakLen = pszLineBreak ? Q_strlen( pszLineBreak ) : 0;
	int nNextLineBreak = unLineBreakLen ? k_LineBreakEveryNGroups : INT_MAX;

	const char * const pszBase64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	uint32 cchEncodedData = *pcchEncodedData;
	if ( cchEncodedData == 0 )
		goto out_of_space;

	--cchEncodedData; // pre-decrement for the terminating null so we don't forget about it

	// input 3 x 8-bit, output 4 x 6-bit
	while ( pubDataEnd - pubData >= 3 )
	{
		if ( cchEncodedData < 4 + unLineBreakLen )
			goto out_of_space;
		
		if ( nNextLineBreak == 0 )
		{
			memcpy( pchEncodedData, pszLineBreak, unLineBreakLen );
			pchEncodedData += unLineBreakLen;
			cchEncodedData -= unLineBreakLen;
			nNextLineBreak = k_LineBreakEveryNGroups;
		}

		uint32 un24BitsData;
		un24BitsData  = (uint32) pubData[0] << 16;
		un24BitsData |= (uint32) pubData[1] << 8;
		un24BitsData |= (uint32) pubData[2];
		pubData += 3;

		pchEncodedData[0] = pszBase64Chars[ (un24BitsData >> 18) & 63 ];
		pchEncodedData[1] = pszBase64Chars[ (un24BitsData >> 12) & 63 ];
		pchEncodedData[2] = pszBase64Chars[ (un24BitsData >>  6) & 63 ];
		pchEncodedData[3] = pszBase64Chars[ (un24BitsData      ) & 63 ];
		pchEncodedData += 4;
		cchEncodedData -= 4;
		--nNextLineBreak;
	}

	// Clean up remaining 1 or 2 bytes of input, pad output with '='
	if ( pubData != pubDataEnd )
	{
		if ( cchEncodedData < 4 + unLineBreakLen )
			goto out_of_space;

		if ( nNextLineBreak == 0 )
		{
			memcpy( pchEncodedData, pszLineBreak, unLineBreakLen );
			pchEncodedData += unLineBreakLen;
			cchEncodedData -= unLineBreakLen;
		}

		uint32 un24BitsData;
		un24BitsData = (uint32) pubData[0] << 16;
		if ( pubData+1 != pubDataEnd )
		{
			un24BitsData |= (uint32) pubData[1] << 8;
		}
		pchEncodedData[0] = pszBase64Chars[ (un24BitsData >> 18) & 63 ];
		pchEncodedData[1] = pszBase64Chars[ (un24BitsData >> 12) & 63 ];
		pchEncodedData[2] = pubData+1 != pubDataEnd ? pszBase64Chars[ (un24BitsData >> 6) & 63 ] : '=';
		pchEncodedData[3] = '=';
		pchEncodedData += 4;
		cchEncodedData -= 4;
	}

	if ( unLineBreakLen )
	{
		if ( cchEncodedData < unLineBreakLen )
			goto out_of_space;
		memcpy( pchEncodedData, pszLineBreak, unLineBreakLen );
		pchEncodedData += unLineBreakLen;
		cchEncodedData -= unLineBreakLen;
	}

	*pchEncodedData = 0;
	*pcchEncodedData = pchEncodedData - pchEncodedDataStart;
	return true;

out_of_space:
	*pchEncodedData = 0;
	*pcchEncodedData = Base64EncodeMaxOutput( cubData, pszLineBreak );
	AssertMsg( false, "CCrypto::Base64Encode: insufficient output buffer (up to n*4/3+5 bytes required, plus linebreaks)" );
	return false;
}

bool Base64Decode( const char *pchData, uint32 cchDataMax, uint8 *pubDecodedData, uint32 *pcubDecodedData, bool bIgnoreInvalidCharacters )
{
	uint32 cubDecodedData = *pcubDecodedData;
	uint32 cubDecodedDataOrig = cubDecodedData;

	if ( pubDecodedData == NULL )
	{
		AssertMsg( *pcubDecodedData == 0, "NULL output buffer with non-zero size passed to Base64Decode" );
		cubDecodedDataOrig = cubDecodedData = ~0u;
	}
	
	// valid base64 character range: '+' (0x2B) to 'z' (0x7A)
	// table entries are 0-63, -1 for invalid entries, -2 for '='
	static const char rgchInvBase64[] = {
		62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
		-1, -1, -1, -2, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,
		 8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
		23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31,
		32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
		47, 48, 49, 50, 51
	};
	COMPILE_TIME_ASSERT( Q_ARRAYSIZE(rgchInvBase64) == 0x7A - 0x2B + 1 );

	uint32 un24BitsWithSentinel = 1;
	while ( cchDataMax-- > 0 )
	{
		char c = *pchData++;

		if ( (uint8)(c - 0x2B) >= Q_ARRAYSIZE( rgchInvBase64 ) )
		{
			if ( c == '\0' )
				break;

			if ( !bIgnoreInvalidCharacters && !( c == '\r' || c == '\n' || c == '\t' || c == ' ' ) )
				goto decode_failed;
			else
				continue;
		}

		c = rgchInvBase64[(uint8)(c - 0x2B)];
		if ( c < 0 )
		{
			if ( c == -2 ) // -2 -> terminating '='
				break;

			if ( !bIgnoreInvalidCharacters )
				goto decode_failed;
			else
				continue;
		}

		un24BitsWithSentinel <<= 6;
		un24BitsWithSentinel |= c;
		if ( un24BitsWithSentinel & (1<<24) )
		{
			if ( cubDecodedData < 3 ) // out of space? go to final write logic
				break;
			if ( pubDecodedData )
			{
				pubDecodedData[0] = (uint8)( un24BitsWithSentinel >> 16 );
				pubDecodedData[1] = (uint8)( un24BitsWithSentinel >> 8);
				pubDecodedData[2] = (uint8)( un24BitsWithSentinel );
				pubDecodedData += 3;
			}
			cubDecodedData -= 3;
			un24BitsWithSentinel = 1;
		}
	}

	// If un24BitsWithSentinel contains data, output the remaining full bytes
	if ( un24BitsWithSentinel >= (1<<6) )
	{
		// Possibilities are 3, 2, 1, or 0 full output bytes.
		int nWriteBytes = 3;
		while ( un24BitsWithSentinel < (1<<24) )
		{
			nWriteBytes--;
			un24BitsWithSentinel <<= 6;
		}

		// Write completed bytes to output
		while ( nWriteBytes-- > 0 )
		{
			if ( cubDecodedData == 0 )
			{
				AssertMsg( false, "CCrypto::Base64Decode: insufficient output buffer (up to n*3/4+2 bytes required)" );
				goto decode_failed;
			}
			if ( pubDecodedData )
			{
				*pubDecodedData++ = (uint8)(un24BitsWithSentinel >> 16);
			}
			--cubDecodedData;
			un24BitsWithSentinel <<= 8;
		}
	}
	
	*pcubDecodedData = cubDecodedDataOrig - cubDecodedData;
	return true;

decode_failed:
	*pcubDecodedData = cubDecodedDataOrig - cubDecodedData;
	return false;
}

}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWebAPIResponse::CWebAPIResponse()
{
	m_pValues = NULL;
	m_unExpirationSeconds = 0;
	m_rtLastModified = 0;
	m_bExtendedArrays = false;
	m_bJSONAnonymousRootNode = false;
	m_eStatusCode = k_EHTTPStatusCode500InternalServerError;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CWebAPIResponse::~CWebAPIResponse()
{
	if ( m_pValues)
		delete m_pValues;
	m_pValues = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Outputs formatted data to buffer
//-----------------------------------------------------------------------------
bool CWebAPIResponse::BEmitFormattedOutput( EWebAPIOutputFormat eFormat, CUtlBuffer &outputBuffer, size_t unMaxResultSize )
{
	VPROF_BUDGET( "CWebAPIResponse::BEmitFormattedOutput", VPROF_BUDGETGROUP_STEAM );
	outputBuffer.Clear();

	switch( eFormat )
	{
		case k_EWebAPIOutputFormat_JSON:
			return BEmitJSON( outputBuffer, unMaxResultSize );
		case k_EWebAPIOutputFormat_XML:
			return BEmitXML( outputBuffer, unMaxResultSize );
		case k_EWebAPIOutputFormat_VDF:
			return BEmitVDF( outputBuffer, unMaxResultSize );
		case k_EWebAPIOutputFormat_ParameterEncoding:
			return BEmitParameterEncoding( outputBuffer );
		default:
			return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Emits JSON formatted representation of response
//-----------------------------------------------------------------------------
bool CWebAPIResponse::BEmitJSON( CUtlBuffer &outputBuffer, size_t unMaxResultSize )
{
	outputBuffer.PutChar( '{' );
	outputBuffer.PutChar( '\n' );

	CWebAPIValues *pValues = m_pValues;

	//if we have an anonymous root, get the first child instead of the root itself
	if ( m_bJSONAnonymousRootNode && m_pValues )
	{
		pValues = m_pValues->GetFirstChild();
	}

	if ( pValues )
	{
		if( !CWebAPIValues::BEmitJSONRecursive( pValues, outputBuffer, 1, unMaxResultSize, m_bExtendedArrays ) )
			return false;
	}

	outputBuffer.PutChar( '\n' );
	outputBuffer.PutChar( '}' );
	if ( !outputBuffer.IsValid() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Emits KeyValues .vdf style formatted representation of response
//-----------------------------------------------------------------------------
bool CWebAPIResponse::BEmitVDF( CUtlBuffer &outputBuffer, size_t unMaxResultSize )
{
	if ( m_pValues )
		if( !CWebAPIValues::BEmitVDFRecursive( m_pValues, outputBuffer, 0, 0, unMaxResultSize, m_bExtendedArrays ) )
			return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Emits XML formatted representation of response
//-----------------------------------------------------------------------------
bool CWebAPIResponse::BEmitXML( CUtlBuffer &outputBuffer, size_t unMaxResultSize )
{
	const char *pchProlog = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	outputBuffer.Put( pchProlog, Q_strlen( pchProlog ) );

	outputBuffer.Put( "<!DOCTYPE ", Q_strlen( "<!DOCTYPE " ) );
	if ( m_pValues && m_pValues->GetName() )
		EmitXMLString( outputBuffer, m_pValues->GetName() );
	outputBuffer.PutChar('>');
	outputBuffer.PutChar('\n');

	if ( m_pValues )
		if( !CWebAPIValues::BEmitXMLRecursive( m_pValues, outputBuffer, 0, unMaxResultSize ) )
			return false;

	if ( !outputBuffer.IsValid() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Emits Parameter Encoding formatted representation of response
//-----------------------------------------------------------------------------
bool CWebAPIResponse::BEmitParameterEncoding( CUtlBuffer &outputBuffer )
{
	if ( !m_pValues )
		return true;

	CWebAPIValues *pValue = m_pValues->GetFirstChild();
	while ( pValue != NULL )
	{
		outputBuffer.Put( pValue->GetName(), Q_strlen( pValue->GetName() ) );
		outputBuffer.Put( "=", 1 );

		CUtlString sValue;
		switch ( pValue->GetType() )
		{
		case k_EWebAPIValueType_Object:
			Assert( false );
			return false;	// no cursive values
		case k_EWebAPIValueType_NumericArray:
			Assert( false );
			return false;	// no arrays
		case k_EWebAPIValueType_BinaryBlob:
			Assert( false );
			return false;	// no binary

		case k_EWebAPIValueType_Int32:
			sValue = CNumStr( pValue->GetInt32Value() );
			break;
		case k_EWebAPIValueType_Int64:
			sValue = CNumStr( pValue->GetInt64Value() );
			break;
		case k_EWebAPIValueType_UInt32:
			sValue = CNumStr( pValue->GetUInt32Value() );
			break;
		case k_EWebAPIValueType_UInt64:
			sValue = CNumStr( pValue->GetUInt64Value() );
			break;
		case k_EWebAPIValueType_Double:
			sValue = CNumStr( pValue->GetDoubleValue() );
			break;
		case k_EWebAPIValueType_String:
			pValue->GetStringValue( sValue );
			break;
		case k_EWebAPIValueType_Bool:
			sValue = CNumStr( pValue->GetBoolValue() );
			break;
		}
		outputBuffer.Put( sValue, sValue.Length() );

		pValue = pValue->GetNextChild();
		if ( pValue )
			outputBuffer.Put( "&", 1 );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Resets the response to be empty
//-----------------------------------------------------------------------------
void CWebAPIResponse::Clear()
{
	if ( m_pValues )
		delete m_pValues;

	m_pValues = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Access the root value element in the response
//-----------------------------------------------------------------------------
CWebAPIValues *CWebAPIResponse::CreateRootValue( const char *pchName )
{
	if ( m_pValues )
	{
		AssertMsg( false, "CWwebAPIResponse::CreateRootValue called while root already existed." );
		Clear();
	}

	m_pValues = new CWebAPIValues( pchName );
	return m_pValues;
}


//----------------------------------------------------------------------------
// Purpose: Gets a singleton buffer pool for webapi values
//----------------------------------------------------------------------------
#ifdef GC
static GCConVar webapi_values_max_pool_size_mb( "webapi_values_max_pool_size_mb", "10", "Maximum size in bytes of the WebAPIValues buffer pool" );
static GCConVar webapi_values_init_buffer_size( "webapi_values_init_buffer_size", "65536", "Initial buffer size for buffers in the WebAPIValues buffer pool" );
/*static*/ CBufferPoolMT &CWebAPIValues::GetBufferPool()
{
	static CBufferPoolMT s_bufferPool( "WebAPIValues", webapi_values_max_pool_size_mb, webapi_values_init_buffer_size );
	return s_bufferPool;
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWebAPIValues::CWebAPIValues( CWebAPIValues *pParent, const char *pchName, EWebAPIValueType eValueType, const char *pchArrayElementNames )
{
	InitInternal( pParent, -1, eValueType, pchArrayElementNames );
	SetName( pchName );
}
	

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWebAPIValues::CWebAPIValues( const char *pchName )
{
	InitInternal( NULL, -1, k_EWebAPIValueType_Object, NULL );
	SetName( pchName );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWebAPIValues::CWebAPIValues( const char *pchName, const char *pchArrayElementNames )
{
	InitInternal( NULL, -1, k_EWebAPIValueType_NumericArray, pchArrayElementNames );
	SetName( pchName );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWebAPIValues::CWebAPIValues( CWebAPIValues *pParent, int nNamePos, EWebAPIValueType eValueType, const char *pchArrayElementNames )
{
	InitInternal( pParent, nNamePos, eValueType, pchArrayElementNames );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
void CWebAPIValues::InitInternal( CWebAPIValues *pParent, int nNamePos, EWebAPIValueType eValueType, const char *pchArrayElementNames )
{
	if ( NULL == pParent )
	{
#ifdef GC
		m_pStringBuffer = GetBufferPool().GetBuffer();
#else
		m_pStringBuffer = new CUtlBuffer;
#endif
	}
	else
	{
		m_pStringBuffer = pParent->m_pStringBuffer;
	}

	m_nNamePos = nNamePos;

	m_eValueType = eValueType;
	if ( m_eValueType == k_EWebAPIValueType_NumericArray )
	{
		Assert( pchArrayElementNames );
		m_nArrayChildElementNamePos = m_pStringBuffer->TellPut();
		m_pStringBuffer->PutString( pchArrayElementNames );
	}

	m_pFirstChild = NULL;
	m_pLastChild = NULL;
	m_pNextPeer = NULL;
	m_pParent = pParent;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CWebAPIValues::~CWebAPIValues()
{
	ClearValue();

	CWebAPIValues *pChild = m_pFirstChild;
	while( pChild )
	{
		CWebAPIValues *pDelete = pChild;
		pChild = pChild->m_pNextPeer;
		delete pDelete;
	}
	m_pFirstChild = NULL;
	m_pNextPeer = NULL;

	if ( NULL == m_pParent )
	{
#ifdef GC
		GetBufferPool().ReturnBuffer( m_pStringBuffer );
#else
		delete m_pStringBuffer;
#endif
	}

	// This two ptrs are just for optimized traversal at runtime, deleting just
	// our first child and next peer will lead to the full tree being deleted correctly.
	m_pLastChild = NULL;
	m_pParent = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the name of the values node
//-----------------------------------------------------------------------------
void CWebAPIValues::SetName( const char * pchName ) 
{
	if ( pchName == NULL )
	{
		AssertMsg( false, "CWebAPIValues constructed with NULL name, breaks some output serialization.  Shouldn't do this." );
		m_nNamePos = -1;
	}
	else
	{
		// Shouldn't use ', ", &, <, or > in names since they can't output well in XML.  : is no good since it implies namespacing in XML.
		// Assert about it so we don't end up with responses that are badly formed in XML output.
		int unLen = 0;
		while ( pchName[unLen] != 0 )
		{
			if ( pchName[unLen] == '\'' || pchName[unLen] == '"' || pchName[unLen] == '&'
				|| pchName[unLen] == '>' || pchName[unLen] == '>' || pchName[unLen] == ':' )
			{
				AssertMsg( false, "Shouldn't use any of '\"&<>: in CWebAPIValues node names, you used %s", pchName );
				break;
			}
			++unLen;
		}

		m_nNamePos = m_pStringBuffer->TellPut();
		m_pStringBuffer->PutString( pchName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Assert that we don't have any child nodes, this is used when setting a 
// native type value.  We don't support having both our own value and children.  You 
// are either an array of more values, or you are a value yourself.
//-----------------------------------------------------------------------------
void CWebAPIValues::AssertNoChildren()
{
	AssertMsg( m_pFirstChild == NULL, "CWebAPIValues has child nodes, but you are trying to set a direct value for it.  Can't have both children and your own value." );
}

//-----------------------------------------------------------------------------
// Purpose:  Clears any existing value, freeing memory if needed
//-----------------------------------------------------------------------------
void CWebAPIValues::ClearValue()
{
	m_eValueType = k_EWebAPIValueType_Object;
}


//-----------------------------------------------------------------------------
// Purpose: Setter
//-----------------------------------------------------------------------------
void CWebAPIValues::SetStringValue( const char *pchValue )
{
	ClearValue();
	AssertNoChildren();
	m_eValueType = k_EWebAPIValueType_String;
	if ( pchValue == NULL )
	{
		m_nStrValuePos = -1;
	}
	else
	{
		m_nStrValuePos = m_pStringBuffer->TellPut();
		m_pStringBuffer->PutString( pchValue );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Setter
//-----------------------------------------------------------------------------
void CWebAPIValues::SetInt32Value( int32 nValue )
{
	ClearValue();
	AssertNoChildren();
	m_eValueType = k_EWebAPIValueType_Int32;
	m_nValue = nValue;
}


//-----------------------------------------------------------------------------
// Purpose: Setter
//-----------------------------------------------------------------------------
void CWebAPIValues::SetUInt32Value( uint32 unValue )
{
	ClearValue();
	AssertNoChildren();
	m_eValueType = k_EWebAPIValueType_UInt32;
	m_unValue = unValue;
}


//-----------------------------------------------------------------------------
// Purpose: Setter
//-----------------------------------------------------------------------------
void CWebAPIValues::SetInt64Value ( int64 lValue )
{
	ClearValue();
	AssertNoChildren();
	m_eValueType = k_EWebAPIValueType_Int64;
	m_lValue = lValue;
}


//-----------------------------------------------------------------------------
// Purpose: Setter
//-----------------------------------------------------------------------------
void CWebAPIValues::SetUInt64Value( uint64 ulValue )
{
	ClearValue();
	AssertNoChildren();
	m_eValueType = k_EWebAPIValueType_UInt64;
	m_ulValue = ulValue;
}


//-----------------------------------------------------------------------------
// Purpose: Setter
//-----------------------------------------------------------------------------
void CWebAPIValues::SetDoubleValue( double flValue )
{
	ClearValue();
	AssertNoChildren();
	m_eValueType = k_EWebAPIValueType_Double;
	m_flValue = flValue;
}


//-----------------------------------------------------------------------------
// Purpose: Setter
//-----------------------------------------------------------------------------
void CWebAPIValues::SetBoolValue( bool bValue )
{
	ClearValue();
	AssertNoChildren();
	m_eValueType = k_EWebAPIValueType_Bool;
	m_bValue = bValue;
}


//-----------------------------------------------------------------------------
// Purpose: Setter
//-----------------------------------------------------------------------------
void CWebAPIValues::SetNullValue()
{
	ClearValue();
	AssertNoChildren();
	m_eValueType = k_EWebAPIValueType_Null;
}


//-----------------------------------------------------------------------------
// Purpose: Setter
//-----------------------------------------------------------------------------
void CWebAPIValues::SetBinaryValue( const uint8 *pValue, uint32 unBytes )
{
	ClearValue();
	AssertNoChildren();
	m_eValueType = k_EWebAPIValueType_BinaryBlob;
	if ( pValue == NULL || unBytes < 1 )
	{
		m_BinaryValue.m_nDataPos = 0;
		m_BinaryValue.m_unBytes = 0;
	}
	else
	{
		m_BinaryValue.m_unBytes = unBytes;
		m_BinaryValue.m_nDataPos = m_pStringBuffer->TellPut();
		m_pStringBuffer->Put( pValue, unBytes );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the type currently held by the node
//-----------------------------------------------------------------------------
EWebAPIValueType CWebAPIValues::GetType() const
{
	return m_eValueType;
}


//-----------------------------------------------------------------------------
// Purpose: Get int32 value
//-----------------------------------------------------------------------------
int32 CWebAPIValues::GetInt32Value() const
{
	// we can read uint64 values this way too
	switch ( m_eValueType )
	{
	case k_EWebAPIValueType_Int32:
		return m_nValue;
	case k_EWebAPIValueType_UInt32: 	// because we can't tell the type of an int when we parse this node might have different type
	case k_EWebAPIValueType_UInt64:
	case k_EWebAPIValueType_String:
		{
			uint32 uiVal = GetUInt32Value();
			AssertMsg( uiVal < INT_MAX, "GetInt32Value called on node with %u, which is too big to fit in an Int32", uiVal );
			return (int32)uiVal;
		}
	}

	AssertMsg( false, "Shouldn't call CWebAPIValues::GetInt32Value unless value type is int32, uint32, uint64, or string. %d does not.", m_eValueType );
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Get uint32 value
//-----------------------------------------------------------------------------
uint32 CWebAPIValues::GetUInt32Value() const
{
	// we can read uint64 values this way too
	switch ( m_eValueType )
	{
	case k_EWebAPIValueType_UInt32:
		return m_unValue;
	case k_EWebAPIValueType_UInt64:
	case k_EWebAPIValueType_String:
		{
			uint64 uiVal = GetUInt64Value();
			AssertMsg( uiVal <= UINT_MAX, "GetUInt32Value called on node with %llu, which is too big to fit in an UInt32", uiVal );
			return (uint32)uiVal;
		}
	}

	AssertMsg( false, "Shouldn't call CWebAPIValues::GetUInt32Value unless value type is uint32, uint64, or string. %d does not", m_eValueType );
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Get int64 value
//-----------------------------------------------------------------------------
int64 CWebAPIValues::GetInt64Value() const
{
	// we can read int32 values this way too
	switch ( m_eValueType )
	{
	case k_EWebAPIValueType_Int32:
		return GetInt32Value();
	case k_EWebAPIValueType_Int64:
		return m_lValue;
	case k_EWebAPIValueType_String:
		if ( m_nStrValuePos < 0 )
		{
			return 0ull;
		}
		else
		{
#if defined(_PS3) || defined(POSIX)
			return strtoll( (const char *)m_pStringBuffer->Base() +  m_nStrValuePos, NULL, 10);
#else
			return _strtoi64( (const char *)m_pStringBuffer->Base() +  m_nStrValuePos, NULL, 10);
#endif
		}
	default:
		AssertMsg1( false, "Shouldn't call CWebAPIValues::GetInt64Value unless value type matches. %d does not", m_eValueType );
		return 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get uint64 value
//-----------------------------------------------------------------------------
uint64 CWebAPIValues::GetUInt64Value() const
{
	// we can read uint32 values this way too
	switch ( m_eValueType )
	{
	case k_EWebAPIValueType_UInt32:
		return GetUInt32Value();
	case k_EWebAPIValueType_UInt64:
		return m_ulValue;
	case k_EWebAPIValueType_String:
		if ( m_nStrValuePos < 0 )
		{
			return 0ull;
		}
		else
		{
#if defined(_PS3) || defined(POSIX)
			return strtoull( (const char *)m_pStringBuffer->Base() +  m_nStrValuePos, NULL, 10);
#else
			return _strtoui64( (const char *)m_pStringBuffer->Base() +  m_nStrValuePos, NULL, 10);
#endif
		}
	default:
		AssertMsg1( false, "Shouldn't call CWebAPIValues::GetUInt64Value unless value type matches. %d does not", m_eValueType );
		return 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get double value
//-----------------------------------------------------------------------------
double CWebAPIValues::GetDoubleValue() const
{
	switch ( m_eValueType )
	{
	case k_EWebAPIValueType_Int32:
		return (double)m_nValue;
	case k_EWebAPIValueType_UInt32:
		return (double)m_unValue;
	case k_EWebAPIValueType_Int64:
		return (double)m_lValue;
	case k_EWebAPIValueType_UInt64:
		return (double)m_ulValue;
	case k_EWebAPIValueType_Double:
		return m_flValue;
	default:
		AssertMsg1( false, "Shouldn't call CWebAPIValues::GetDoubleValue unless value type matches. %d does not", m_eValueType );
		return 0.0f;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get bool value
//-----------------------------------------------------------------------------
bool CWebAPIValues::GetBoolValue() const
{
	if ( m_eValueType != k_EWebAPIValueType_Bool )
	{
		AssertMsg( false, "Shouldn't call CWebAPIValues::GetBoolValue unless value type matches" );
		return false;
	}

	return m_bValue;
}



//-----------------------------------------------------------------------------
// Purpose: Get string value
//-----------------------------------------------------------------------------
void CWebAPIValues::GetStringValue( CUtlString &stringOut ) const
{
	switch ( m_eValueType )
	{
	case k_EWebAPIValueType_String:
		if ( m_nStrValuePos < 0 )
		{
			stringOut.Clear();
		}
		else
		{
			stringOut = (const char *)m_pStringBuffer->Base() +  m_nStrValuePos;
		}

		return;
	case k_EWebAPIValueType_Int32:
		stringOut = CNumStr( m_nValue ).String();
		return;
	case k_EWebAPIValueType_Int64:
		stringOut = CNumStr( m_lValue ).String();
		return;
	case k_EWebAPIValueType_UInt32:
		stringOut = CNumStr( m_unValue ).String();
		return;
	case k_EWebAPIValueType_UInt64:
		stringOut = CNumStr( m_ulValue ).String();
		return;
	case k_EWebAPIValueType_Double:
		stringOut = CNumStr( m_flValue ).String();
		return;
	case k_EWebAPIValueType_Bool:
		stringOut = m_bValue ? "true" : "false";
		return;
	default:
		AssertMsg1( false, "CWebAPIValues::GetStringValue(), unable to convert data type %d to string", m_eValueType );
		stringOut = "";
		return;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get binary blob value
//-----------------------------------------------------------------------------
void CWebAPIValues::GetBinaryValue( CUtlBuffer &bufferOut ) const
{
	if ( m_eValueType != k_EWebAPIValueType_BinaryBlob )
	{
		AssertMsg( false, "Shouldn't call CWebAPIValues::GetBinaryValue unless value type matches" );
		bufferOut.Clear();
		return;
	}

	bufferOut.Clear();
	bufferOut.EnsureCapacity( m_BinaryValue.m_unBytes );
	if ( m_BinaryValue.m_unBytes )
		bufferOut.Put( (byte*)m_pStringBuffer->Base() + m_BinaryValue.m_nDataPos, m_BinaryValue.m_unBytes );

	return;
}


//-----------------------------------------------------------------------------
// Purpose:  Create a  child array of this node.  Note that array nodes can only
// have un-named children, in XML the pchArrayElementNames value will be used
// as the element name for each of the children of the array, in JSON it will simply
// be a numerically indexed [] array.
//-----------------------------------------------------------------------------
CWebAPIValues * CWebAPIValues::CreateChildArray( const char *pchName, const char *pchArrayElementNames )
{
	return CreateChildInternal( pchName, k_EWebAPIValueType_NumericArray, pchArrayElementNames );
}


//-----------------------------------------------------------------------------
// Purpose:  Create a child of this node.  Note, it's possible to create multiple,
// children with the same name, but you really don't want to.  We'll assert about it
// in debug builds to detect, but not in release.  If you do create duplicates you'll
// have broken JSON output.
//-----------------------------------------------------------------------------
CWebAPIValues * CWebAPIValues::CreateChildObject( const char *pchName )
{
	return CreateChildInternal( pchName, k_EWebAPIValueType_Object );
}

//-----------------------------------------------------------------------------
// Purpose: Return an existing child object - otherwise create one and return that.
//-----------------------------------------------------------------------------
CWebAPIValues *CWebAPIValues::FindOrCreateChildObject( const char *pchName )
{
	CWebAPIValues *pChild = FindChild( pchName );
	if ( pChild )
	{
		return pChild;
	}

	return CreateChildObject( pchName );
}

//-----------------------------------------------------------------------------
// Purpose: Add a child object to the array, this should only be called on objects that are of the array type
//-----------------------------------------------------------------------------
CWebAPIValues * CWebAPIValues::AddChildObjectToArray()
{
	if ( m_eValueType != k_EWebAPIValueType_NumericArray )
	{
		AssertMsg( m_eValueType == k_EWebAPIValueType_NumericArray, "Can't add array elements to CWebAPIVAlues unless type is of numeric array." );
		return NULL;
	}

	// Use child element array name as name of all children of arrays
	return CreateChildInternal( NULL, k_EWebAPIValueType_Object );
}


//-----------------------------------------------------------------------------
// Purpose: Add a child array to the array, this should only be called on objects that are of the array type
//-----------------------------------------------------------------------------
CWebAPIValues * CWebAPIValues::AddChildArrayToArray( const char * pchArrayElementNames )
{
	if ( m_eValueType != k_EWebAPIValueType_NumericArray )
	{
		AssertMsg( m_eValueType == k_EWebAPIValueType_NumericArray, "Can't add array elements to CWebAPIVAlues unless type is of numeric array." );
		return NULL;
	}

	// Use child element array name as name of all children of arrays
	return CreateChildInternal( NULL, k_EWebAPIValueType_NumericArray, pchArrayElementNames );
}

//-----------------------------------------------------------------------------
// Purpose:  Internal helper for creating children
//-----------------------------------------------------------------------------
CWebAPIValues * CWebAPIValues::CreateChildInternal( const char *pchName, EWebAPIValueType eValueType, const char *pchArrayElementNames )
{
	// Shouldn't create children if you have a direct value.  You are either an object or array of children,
	// or a native value type.  Not both.

	AssertMsg( m_eValueType == k_EWebAPIValueType_Object || m_eValueType == k_EWebAPIValueType_NumericArray, "You are trying to create a child node of a CWebAPIValues object, but it has a direct value already.  Can't have children and a value." );
	if ( m_eValueType != k_EWebAPIValueType_Object && m_eValueType != k_EWebAPIValueType_NumericArray )
		ClearValue();

	// Shouldn't create named children if you are a numeric array
	CWebAPIValues *pNewNode;
	if  ( m_eValueType == k_EWebAPIValueType_NumericArray )
	{
		if ( pchName )
		{
			AssertMsg( false, "Can't create named child of CWebAPIValues object of type NumericArray.  Should call AddArrayElement instead of CreateChild*." );
		}
		// Force name to match what all items in the array should use
		pNewNode = new CWebAPIValues( this, m_nArrayChildElementNamePos, eValueType, pchArrayElementNames );
	}
	else
	{
		pNewNode = new CWebAPIValues( this, pchName, eValueType, pchArrayElementNames );
	}

	if ( eValueType == k_EWebAPIValueType_NumericArray )
	{
		Assert( pchArrayElementNames );
	}

	if ( !m_pFirstChild )
	{
		m_pLastChild = m_pFirstChild = pNewNode;
		return m_pFirstChild;
	}
	else
	{

		CWebAPIValues *pCurLastChild = m_pLastChild;

#ifdef _DEBUG
		// In debug, traverse all children so we can check for duplicate names, which will break JSON output!
		pCurLastChild = m_pFirstChild;
		if ( m_eValueType != k_EWebAPIValueType_NumericArray )
		{
			if ( Q_stricmp( pCurLastChild->GetName(), pchName ) == 0 )
			{
				AssertMsg( false, "Trying to create CWebAPIValues child with name %s that conflicts with existing child.  Breaks JSON output!", pchName );
			}
		}

		while ( pCurLastChild->m_pNextPeer )
		{
			pCurLastChild = pCurLastChild->m_pNextPeer;
			if ( m_eValueType != k_EWebAPIValueType_NumericArray )
			{
				if ( Q_stricmp( pCurLastChild->GetName(), pchName ) == 0 )
				{
					AssertMsg( false, "Trying to create CWebAPIValues child with name %s that conflicts with existing child.  Breaks JSON output!", pchName );
				}
			}
		}
		// Also, in debug assert last child ptr looks correct
		Assert( m_pLastChild == pCurLastChild );
#endif


		m_pLastChild = pCurLastChild->m_pNextPeer = pNewNode;
		return m_pLastChild;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set a child node's string value
//-----------------------------------------------------------------------------
void CWebAPIValues::SetChildStringValue( const char *pchChildName, const char *pchValue )
{
	CreateChildObject( pchChildName )->SetStringValue( pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set a child node's int32 value
//-----------------------------------------------------------------------------
void CWebAPIValues::SetChildInt32Value( const char *pchChildName, int32 nValue )
{
	CreateChildObject( pchChildName )->SetInt32Value( nValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set a child node's uint32 value
//-----------------------------------------------------------------------------
void CWebAPIValues::SetChildUInt32Value( const char *pchChildName, uint32 unValue )
{
	CreateChildObject( pchChildName )->SetUInt32Value( unValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set a child node's int64 value
//-----------------------------------------------------------------------------
void CWebAPIValues::SetChildInt64Value ( const char *pchChildName, int64 lValue )
{
	CreateChildObject( pchChildName )->SetInt64Value( lValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set a child node's uint64 value
//-----------------------------------------------------------------------------
void CWebAPIValues::SetChildUInt64Value( const char *pchChildName, uint64 ulValue )
{
	CreateChildObject( pchChildName )->SetUInt64Value( ulValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set a child node's double value
//-----------------------------------------------------------------------------
void CWebAPIValues::SetChildDoubleValue( const char *pchChildName, double flValue )
{
	CreateChildObject( pchChildName )->SetDoubleValue( flValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set a child node's binary blob value
//-----------------------------------------------------------------------------
void CWebAPIValues::SetChildBinaryValue( const char *pchChildName, const uint8 *pValue, uint32 unBytes )
{
	CreateChildObject( pchChildName )->SetBinaryValue( pValue, unBytes );
}


//-----------------------------------------------------------------------------
// Purpose: Set a child node's boolean value
//-----------------------------------------------------------------------------
void CWebAPIValues::SetChildBoolValue( const char *pchChildName, bool bValue )
{
	CreateChildObject( pchChildName )->SetBoolValue( bValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set a child node's boolean value
//-----------------------------------------------------------------------------
void CWebAPIValues::SetChildNullValue( const char *pchChildName )
{
	CreateChildObject( pchChildName )->SetNullValue();
}


//-----------------------------------------------------------------------------
// Purpose: Get a child node's int32 value or return the default if the node doesn't exist
//-----------------------------------------------------------------------------
int32 CWebAPIValues::GetChildInt32Value( const char *pchChildName, int32 nDefault ) const
{
	const CWebAPIValues *pChild = FindChild( pchChildName );
	if( pChild )
		return pChild->GetInt32Value();
	else
		return nDefault;
}


//-----------------------------------------------------------------------------
// Purpose: Get a child node's uint32 value or return the default if the node doesn't exist
//-----------------------------------------------------------------------------
uint32 CWebAPIValues::GetChildUInt32Value( const char *pchChildName, uint32 unDefault ) const
{
	const CWebAPIValues *pChild = FindChild( pchChildName );
	if( pChild )
		return pChild->GetUInt32Value();
	else
		return unDefault;
}



//-----------------------------------------------------------------------------
// Purpose: Get a child node's int64 value or return the default if the node doesn't exist
//-----------------------------------------------------------------------------
int64 CWebAPIValues::GetChildInt64Value( const char *pchChildName, int64 lDefault ) const
{
	const CWebAPIValues *pChild = FindChild( pchChildName );
	if( pChild )
		return pChild->GetInt64Value();
	else
		return lDefault;
}



//-----------------------------------------------------------------------------
// Purpose: Get a child node's uint64 value or return the default if the node doesn't exist
//-----------------------------------------------------------------------------
uint64 CWebAPIValues::GetChildUInt64Value( const char *pchChildName, uint64 ulDefault ) const
{
	const CWebAPIValues *pChild = FindChild( pchChildName );
	if( pChild )
		return pChild->GetUInt64Value();
	else
		return ulDefault;
}



//-----------------------------------------------------------------------------
// Purpose: Get a child node's double value or return the default if the node doesn't exist
//-----------------------------------------------------------------------------
double CWebAPIValues::GetChildDoubleValue( const char *pchChildName, double flDefault ) const
{
	const CWebAPIValues *pChild = FindChild( pchChildName );
	if( pChild )
		return pChild->GetDoubleValue();
	else
		return flDefault;
}



//-----------------------------------------------------------------------------
// Purpose: Get a child node's string value or return the default if the node doesn't exist
//-----------------------------------------------------------------------------
void CWebAPIValues::GetChildStringValue( CUtlString &stringOut, const char *pchChildName, const char *pchDefault ) const
{
	const CWebAPIValues *pChild = FindChild( pchChildName );
	if( pChild )
	{
		pChild->GetStringValue( stringOut );
	}
	else
	{
		stringOut = pchDefault;
	}
}



//-----------------------------------------------------------------------------
// Purpose: Get a child node's binary blob value (returns false if the child wasn't found)
//-----------------------------------------------------------------------------
bool CWebAPIValues::BGetChildBinaryValue( CUtlBuffer &bufferOut, const char *pchChildName ) const
{
	const CWebAPIValues *pChild = FindChild( pchChildName );
	if( pChild )
	{
		pChild->GetBinaryValue( bufferOut );
		return true;
	}
	else
	{
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get a child node's binary blob value (returns false if the child wasn't found)
//-----------------------------------------------------------------------------
bool CWebAPIValues::IsChildNullValue( const char *pchChildName ) const
{
	const CWebAPIValues *pChild = FindChild( pchChildName );
	if( pChild )
		return pChild->IsNullValue();
	else
		return false;
}



//-----------------------------------------------------------------------------
// Purpose: Get a child node's bool value or return the default if the node doesn't exist
//-----------------------------------------------------------------------------
bool CWebAPIValues::GetChildBoolValue( const char *pchChildName, bool bDefault ) const
{
	const CWebAPIValues *pChild = FindChild( pchChildName );
	if( pChild )
		return pChild->GetBoolValue();
	else
		return bDefault;
}


//-----------------------------------------------------------------------------
// Purpose:  Find first matching child by name, O(N) on number of children, this class isn't designed for searching
//-----------------------------------------------------------------------------
CWebAPIValues * CWebAPIValues::FindChild( const char *pchName )
{
	CWebAPIValues *pCurLastChild = m_pFirstChild;
	
	while ( pCurLastChild )
	{
		if ( Q_stricmp( pCurLastChild->GetName(), pchName ) == 0 )
			return pCurLastChild;

		pCurLastChild = pCurLastChild->m_pNextPeer;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose:  Get the first child of this node
//-----------------------------------------------------------------------------
CWebAPIValues * CWebAPIValues::GetFirstChild()
{
	return m_pFirstChild;
}


//-----------------------------------------------------------------------------
// Purpose:  Call this on the returned value from GetFirstChild() or a previous GetNextChild() call to
// proceed to the next child of the parent GetFirstChild() was originally called on.
//-----------------------------------------------------------------------------
CWebAPIValues * CWebAPIValues::GetNextChild()
{
	return m_pNextPeer;
}


//-----------------------------------------------------------------------------
// Purpose:  Call this on any node to return the parent of that node
//-----------------------------------------------------------------------------
CWebAPIValues * CWebAPIValues::GetParent()
{
	return m_pParent;
}

//-----------------------------------------------------------------------------
// Purpose:  Deletes a child node by name
//-----------------------------------------------------------------------------
void CWebAPIValues::DeleteChild( const char *pchName )
{
	CWebAPIValues *pChild = NULL;	// child we're examining, could be NULL at exit if we don't find it
	CWebAPIValues *pPrev = NULL;	// previous sibling, or NULL

	for ( pChild = m_pFirstChild; pChild != NULL; pPrev = pChild, pChild = pChild->m_pNextPeer )
	{
		if ( !Q_stricmp( pChild->GetName(), pchName ) )
		{
			if ( pChild == m_pFirstChild )
			{
				// first child, fixup parent's pointer to take pChild out
				Assert( pPrev == NULL );
				m_pFirstChild = pChild->m_pNextPeer;
			}
			else
			{
				// not first child, fixup sibling's pointer to take pChild out
				Assert( pPrev != NULL );
				pPrev->m_pNextPeer = pChild->m_pNextPeer;
			}

			// clean up next ptr on child so we don't double free
			pChild->m_pNextPeer = NULL;

			// fixup last child pointer if pChild is the last child
			if ( pChild == m_pLastChild )
			{
				m_pLastChild = pPrev;
			}

			break;
		}
	}

	// debug only, check that there is no child by the specified name any more and that it was excised OK
	AssertMsg( FindChild( pchName ) == NULL, "cwebapivalues deleted child is still lurking" );
	Assert( pChild == NULL || pChild->m_pNextPeer == NULL );

	// now take the removed child out of the heap
	delete pChild;
}

//-----------------------------------------------------------------------------
// Purpose: Emits JSON formatted representation of values
//
// when bEmitOldStyleArrays is true, arrays are emitted as a child of a singlet object, and any empty arrays
// will be emitted as having a single null member.
//
// when bEmitOldStyleArrays is false, arrays are emitted bare (no subobject) and empty arrays
// are emitted empty.
//-----------------------------------------------------------------------------
bool CWebAPIValues::BEmitJSONRecursive( const CWebAPIValues *pCurrent, CUtlBuffer &outputBuffer, int nTabLevel, size_t unMaxResultSize, bool bEmitOldStyleArrays )
{
	bool bSuccess = true;


	while( pCurrent )
	{
		// don't let the buffer grow until it consumes all available memory
		if( unMaxResultSize && (size_t)outputBuffer.TellMaxPut() > unMaxResultSize )
		{
			return false;
		}

		// Can't emit nameless nodes in JSON.  Nodes should always have a name.
		Assert( pCurrent->GetName() );
		if ( pCurrent->GetName() )
		{
			for( int i=0; i < nTabLevel; ++i )
				outputBuffer.PutChar ( '\t' );

			if ( !pCurrent->m_pParent || pCurrent->m_pParent->GetType() != k_EWebAPIValueType_NumericArray )
			{
				EmitJSONString( outputBuffer, pCurrent->GetName() );
				outputBuffer.PutChar( ':' );
				outputBuffer.PutChar( ' ' );
			}

			if ( pCurrent->m_eValueType == k_EWebAPIValueType_Object || pCurrent->m_eValueType == k_EWebAPIValueType_NumericArray )
			{

				if( bEmitOldStyleArrays || pCurrent->m_eValueType == k_EWebAPIValueType_Object )
				{
					outputBuffer.PutChar( '{' );
					outputBuffer.PutChar( '\n' );
				}

				if ( pCurrent->m_eValueType == k_EWebAPIValueType_NumericArray )
				{
					if( bEmitOldStyleArrays )
					{
						for( int i=0; i < nTabLevel+1; ++i )
							outputBuffer.PutChar ( '\t' );

						EmitJSONString( outputBuffer, (const char *)pCurrent->m_pStringBuffer->Base() + pCurrent->m_nArrayChildElementNamePos );
						outputBuffer.PutChar( ':' );
						outputBuffer.PutChar( ' ' );
					}

					outputBuffer.PutChar( '[' );
					outputBuffer.PutChar( '\n' );
				}

				// First add any children
				if ( pCurrent->m_pFirstChild )
				{
					int nChildTabLevel = nTabLevel+1;
					if ( pCurrent->m_eValueType == k_EWebAPIValueType_NumericArray && bEmitOldStyleArrays )
						++nChildTabLevel;

					bSuccess = BEmitJSONRecursive( pCurrent->m_pFirstChild, outputBuffer, nChildTabLevel, unMaxResultSize, bEmitOldStyleArrays );
					if ( !bSuccess )
						return false;
				}
				else if ( bEmitOldStyleArrays )
				{
					for( int i=0; i < nTabLevel + 1; ++i )
						outputBuffer.PutChar ( '\t' );

					outputBuffer.Put( "null", 4 );
				}

				outputBuffer.PutChar( '\n' );

				for( int i=0; i < nTabLevel; ++i )
					outputBuffer.PutChar ( '\t' );

				if ( pCurrent->m_eValueType == k_EWebAPIValueType_NumericArray )
				{
					if( bEmitOldStyleArrays )
						outputBuffer.PutChar( '\t' );
					outputBuffer.PutChar( ']' );
					outputBuffer.PutChar( '\n' );

					for( int i=0; i < nTabLevel; ++i )
						outputBuffer.PutChar ( '\t' );
				}

				if( bEmitOldStyleArrays || pCurrent->m_eValueType == k_EWebAPIValueType_Object )
				{
					outputBuffer.PutChar( '}' );
				}
			}
			else
			{

				switch ( pCurrent->m_eValueType )
				{
				case k_EWebAPIValueType_Int32:
					{
						CNumStr numStr( pCurrent->m_nValue );
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_Int64:
					{
						CNumStr numStr( pCurrent->m_lValue );
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_UInt32:
					{
						CNumStr numStr( pCurrent->m_unValue );
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_UInt64:
					{
						CNumStr numStr( pCurrent->m_ulValue );
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_Double:
					{
						CNumStr numStr( pCurrent->m_flValue );
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;

				case k_EWebAPIValueType_String:
					{
						if ( pCurrent->m_nStrValuePos < 0 )
							outputBuffer.Put( "null", 4 );
						else
							EmitJSONString( outputBuffer, (const char *)pCurrent->m_pStringBuffer->Base() + pCurrent->m_nStrValuePos );
					}
					break;

				case k_EWebAPIValueType_Bool:
					{
						if ( !pCurrent->m_bValue )
							outputBuffer.Put( "false", 5 );
						else
							outputBuffer.Put( "true", 4 );
					}
					break;

				case k_EWebAPIValueType_Null:
					{
						outputBuffer.Put( "null", 4 );
					}
					break;

				case k_EWebAPIValueType_BinaryBlob:
					{
						if ( pCurrent->m_BinaryValue.m_unBytes == 0 )
							outputBuffer.Put( "null", 4 );
						else
						{
							CUtlMemory<char> buffEncoded;
							DbgVerify( Base64EncodeIntoUTLMemory( (const uint8 *)pCurrent->m_pStringBuffer->Base() + pCurrent->m_BinaryValue.m_nDataPos, pCurrent->m_BinaryValue.m_unBytes, buffEncoded ) );
							EmitJSONString( outputBuffer, buffEncoded.Base() );
						}
					}
					break;
				default:
					break;
				}
			}
		}

		// Now, check for any peers
		if ( bSuccess && pCurrent->m_pNextPeer )
		{
			outputBuffer.PutChar( ',' );
			outputBuffer.PutChar( '\n' );

			pCurrent = pCurrent->m_pNextPeer;
		}
		else
		{
			// We're done, or failing early
			pCurrent = NULL;
		}
	}

	return bSuccess && outputBuffer.IsValid();
}


//-----------------------------------------------------------------------------
// Purpose: Emits KeyValues .vdf style formatted representation of values
//-----------------------------------------------------------------------------
bool CWebAPIValues::BEmitVDFRecursive( const CWebAPIValues *pCurrent, CUtlBuffer &outputBuffer, int nTabLevel, uint32 nArrayElement, size_t unMaxResultSize, bool bIncludeArrayElementName  )
{
	bool bSuccess = true;

	// We can have lots of peers, so this is an optimization to avoid tail recursion and resulting stack-overflows!	
	while( pCurrent )
	{
		// don't let the buffer grow until it consumes all available memory
		if( unMaxResultSize && (size_t)outputBuffer.TellMaxPut() > unMaxResultSize )
		{
			return false;
		}
		if ( pCurrent->GetName() )
		{
			for( int i=0; i < nTabLevel; ++i )
				outputBuffer.PutChar ( '\t' );

			// Open node, special naming when inside arrays
			if ( pCurrent->m_pParent && pCurrent->m_pParent->GetType() == k_EWebAPIValueType_NumericArray )
			{
				CNumStr numStr( nArrayElement );
				numStr.AddQuotes();
				outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
			}
			else
			{
				EmitVDFString( outputBuffer, pCurrent->GetName() );
			}

			if ( pCurrent->m_eValueType == k_EWebAPIValueType_Object || pCurrent->m_eValueType == k_EWebAPIValueType_NumericArray )
			{
				outputBuffer.PutChar( '\n' );

				for( int i=0; i < nTabLevel; ++i )
					outputBuffer.PutChar ( '\t' );

				outputBuffer.PutChar( '{' );
				outputBuffer.PutChar( '\n' );

				if ( pCurrent->m_eValueType == k_EWebAPIValueType_NumericArray && bIncludeArrayElementName )
				{
					for( int i=0; i < nTabLevel+1; ++i )
						outputBuffer.PutChar ( '\t' );

					EmitVDFString( outputBuffer, (const char *)pCurrent->m_pStringBuffer->Base() + pCurrent->m_nArrayChildElementNamePos );
					outputBuffer.PutChar( '\n' );

					for( int i=0; i < nTabLevel+1; ++i )
						outputBuffer.PutChar ( '\t' );

					outputBuffer.PutChar( '{' );
					outputBuffer.PutChar( '\n' );
				}

				if ( pCurrent->m_pFirstChild )
				{
					int nChildTabLevel = nTabLevel+1;
					if ( pCurrent->m_eValueType == k_EWebAPIValueType_NumericArray && bIncludeArrayElementName )
						++nChildTabLevel;

					bSuccess = BEmitVDFRecursive( pCurrent->m_pFirstChild, outputBuffer, nChildTabLevel, 0, unMaxResultSize, bIncludeArrayElementName );
					if ( !bSuccess )
						return false;
				}

				if ( pCurrent->m_eValueType == k_EWebAPIValueType_NumericArray && bIncludeArrayElementName  )
				{
					outputBuffer.PutChar( '\n' );

					for( int i=0; i < nTabLevel+1; ++i )
						outputBuffer.PutChar ( '\t' );

					outputBuffer.PutChar( '}' );
				}

				outputBuffer.PutChar( '\n' );

				for( int i=0; i < nTabLevel; ++i )
					outputBuffer.PutChar ( '\t' );

				outputBuffer.PutChar( '}' );
			}
			else
			{
				outputBuffer.PutChar( '\t' );

				switch ( pCurrent->m_eValueType )
				{
				case k_EWebAPIValueType_Int32:
					{
						CNumStr numStr( pCurrent->m_nValue );
						numStr.AddQuotes();
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_Int64:
					{
						CNumStr numStr( pCurrent->m_lValue );
						numStr.AddQuotes();
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_UInt32:
					{
						CNumStr numStr( pCurrent->m_unValue );
						numStr.AddQuotes();
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_UInt64:
					{
						CNumStr numStr( pCurrent->m_ulValue );
						numStr.AddQuotes();
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_Double:
					{
						CNumStr numStr( pCurrent->m_flValue );
						numStr.AddQuotes();
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;

				case k_EWebAPIValueType_String:
					{
						EmitVDFString( outputBuffer, pCurrent->m_nStrValuePos >= 0 ? ( (const char *)pCurrent->m_pStringBuffer->Base() + pCurrent->m_nStrValuePos ) : NULL );
					}
					break;

				case k_EWebAPIValueType_Bool:
					{
						if ( !pCurrent->m_bValue )
							outputBuffer.Put( "\"0\"", 3 );
						else
							outputBuffer.Put( "\"1\"", 3 );
					}
					break;

				case k_EWebAPIValueType_Null:
					{
						outputBuffer.Put ("\"\"", 2 );
					}
					break;

				case k_EWebAPIValueType_BinaryBlob:
					{
						if ( pCurrent->m_BinaryValue.m_unBytes == 0 )
							outputBuffer.Put( "\"\"", 2 );
						else
						{
							CUtlMemory<char> buffEncoded;
							DbgVerify( Base64EncodeIntoUTLMemory( (const uint8 *)pCurrent->m_pStringBuffer->Base() + pCurrent->m_BinaryValue.m_nDataPos, pCurrent->m_BinaryValue.m_unBytes, buffEncoded ) );
							EmitVDFString( outputBuffer, buffEncoded.Base() );
						}
					}
					break;
				default:
					break;
				}
			}
		}

		// Now, check for any peers
		if ( bSuccess && pCurrent->m_pNextPeer )
		{
			outputBuffer.PutChar( '\n' );
			pCurrent = pCurrent->m_pNextPeer;
			nArrayElement += 1;
		}
		else
		{
			// We're done, or failing early
			pCurrent = NULL;
		}
	}

	return bSuccess && outputBuffer.IsValid();
}


//-----------------------------------------------------------------------------
// Purpose: Emits XML formatted representation of values
//-----------------------------------------------------------------------------
bool CWebAPIValues::BEmitXMLRecursive( const CWebAPIValues *pCurrent, CUtlBuffer &outputBuffer, int nTabLevel, size_t unMaxResultSize )
{
	bool bSuccess = true;

	while( pCurrent )
	{

		// don't let the buffer grow until it consumes all available memory
		if( unMaxResultSize && (size_t)outputBuffer.TellMaxPut() > unMaxResultSize )
		{
			return false;
		}

		// Can't emit nameless nodes in XML.  Nodes should always have a name.
		Assert( pCurrent->GetName() );
		if ( pCurrent->GetName() )
		{
			for( int i=0; i < nTabLevel; ++i )
				outputBuffer.PutChar ( '\t' );

			// Open node
			outputBuffer.PutChar( '<' );
			EmitXMLString( outputBuffer, pCurrent->GetName() );
			outputBuffer.PutChar( '>' );

			if ( pCurrent->m_eValueType == k_EWebAPIValueType_Object || pCurrent->m_eValueType == k_EWebAPIValueType_NumericArray )
			{
				// First add any children
				if ( pCurrent->m_pFirstChild )
				{
					outputBuffer.PutChar( '\n' );

					bSuccess = BEmitXMLRecursive( pCurrent->m_pFirstChild, outputBuffer, nTabLevel+1, unMaxResultSize );
					if ( !bSuccess )
						return false;

					outputBuffer.PutChar( '\n' );

					// Return to correct tab level, for when we close element below
					for( int i=0; i < nTabLevel; ++i )
						outputBuffer.PutChar ( '\t' );
				}
			}
			else
			{

				switch ( pCurrent->m_eValueType )
				{
				case k_EWebAPIValueType_Int32:
					{
						CNumStr numStr( pCurrent->m_nValue );
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_Int64:
					{
						CNumStr numStr( pCurrent->m_lValue );
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_UInt32:
					{
						CNumStr numStr( pCurrent->m_unValue );
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_UInt64:
					{
						CNumStr numStr( pCurrent->m_ulValue );
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;
				case k_EWebAPIValueType_Double:
					{
						CNumStr numStr( pCurrent->m_flValue );
						outputBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					}
					break;

				case k_EWebAPIValueType_String:
					{
						if ( pCurrent->m_nStrValuePos < 0 )
							outputBuffer.Put( "null", 4 );
						else
							EmitXMLString( outputBuffer, (const char *)pCurrent->m_pStringBuffer->Base() + pCurrent->m_nStrValuePos );
					}
					break;

				case k_EWebAPIValueType_Bool:
					{
						if ( !pCurrent->m_bValue )
							outputBuffer.Put( "false", 5 );
						else
							outputBuffer.Put( "true", 4 );
					}
					break;

				case k_EWebAPIValueType_Null:
					{
						outputBuffer.Put ("null", 4 );
					}
					break;

				case k_EWebAPIValueType_BinaryBlob:
					{
						if ( pCurrent->m_BinaryValue.m_unBytes == 0 )
							outputBuffer.Put( "null", 4 );
						else
						{
							CUtlMemory<char> buffEncoded;
							DbgVerify( Base64EncodeIntoUTLMemory( (const uint8 *)pCurrent->m_pStringBuffer->Base() + pCurrent->m_BinaryValue.m_nDataPos, pCurrent->m_BinaryValue.m_unBytes, buffEncoded ) );
							EmitXMLString( outputBuffer, buffEncoded.Base() );
						}
					}
					break;
				default:
					break;
				}
			}
		}

		// Close element
		outputBuffer.PutChar( '<' );
		outputBuffer.PutChar( '/' );
		EmitXMLString( outputBuffer, pCurrent->GetName() );
		outputBuffer.PutChar( '>' );

		// Now, check for any peers
		if ( bSuccess && pCurrent->m_pNextPeer )
		{
			outputBuffer.PutChar( '\n' );
			pCurrent = pCurrent->m_pNextPeer;
		}
		else
		{
			// We're done, or failing early
			pCurrent = NULL;
		}
	}

	return bSuccess && outputBuffer.IsValid();
}


struct JSONParserContext_t
{
	CWebAPIValues *m_pCurrentNode;
	CWebAPIValues *m_pRootNode;
	CUtlString m_sChildName;
	bool m_bIsKey;
};

static int JSONParserCallback(void* void_ctx, int type, const JSON_value* value)
{
	JSONParserContext_t *ctx = (JSONParserContext_t *)void_ctx;
	CWebAPIValues *pCreatedNode = NULL;
	switch(type) 
	{
		case JSON_T_ARRAY_BEGIN:
			// handle the root node
			if( !ctx->m_pRootNode )
			{
				Assert( !ctx->m_pCurrentNode );
				ctx->m_pRootNode = ctx->m_pCurrentNode = new CWebAPIValues( ctx->m_sChildName, "e" );
				break;
			}

			if( ctx->m_pCurrentNode->IsArray() )
			{
				Assert( ctx->m_sChildName.IsEmpty() );
				ctx->m_pCurrentNode = ctx->m_pCurrentNode->AddChildArrayToArray( "e" );
			}
			else
			{
				Assert( !ctx->m_sChildName.IsEmpty() );
				ctx->m_pCurrentNode = ctx->m_pCurrentNode->CreateChildArray( ctx->m_sChildName, "e" );
				ctx->m_sChildName.Clear();
			}
			break;

		case JSON_T_ARRAY_END:
			ctx->m_pCurrentNode = ctx->m_pCurrentNode->GetParent();
			break;

		case JSON_T_OBJECT_BEGIN:
			// this might be the start of the root object itself
			if( !ctx->m_pRootNode )
			{
				ctx->m_pRootNode = ctx->m_pCurrentNode = new CWebAPIValues( ctx->m_sChildName );
				break;
			}

			if( ctx->m_pCurrentNode->IsArray() )
			{
				Assert( ctx->m_sChildName.IsEmpty() );
				ctx->m_pCurrentNode = ctx->m_pCurrentNode->AddChildObjectToArray();
			}
			else
			{
				Assert( !ctx->m_sChildName.IsEmpty() );
				ctx->m_pCurrentNode = ctx->m_pCurrentNode->CreateChildObject( ctx->m_sChildName );
				ctx->m_sChildName.Clear();
			}
			break;

		case JSON_T_OBJECT_END:
			ctx->m_pCurrentNode = ctx->m_pCurrentNode->GetParent();
			break;

		case JSON_T_KEY:
			ctx->m_sChildName = value->vu.str.value;
			break;   

		case JSON_T_INTEGER:
		case JSON_T_FLOAT:
		case JSON_T_NULL:
		case JSON_T_TRUE:
		case JSON_T_FALSE:
		case JSON_T_STRING:
			// create the new node for this value
			if( ctx->m_pCurrentNode->IsArray() )
			{
				pCreatedNode = ctx->m_pCurrentNode->AddChildObjectToArray();
			}
			else
			{
				pCreatedNode = ctx->m_pCurrentNode->CreateChildObject( ctx->m_sChildName );
				ctx->m_sChildName.Clear();
			}

			// set the actual value
			switch( type )
			{
			case JSON_T_INTEGER:

				// try to figure out what type to use
				if( value->vu.integer_value >= 0 )
				{
					// unsigned
					if( value->vu.integer_value <= UINT_MAX )
					{
						pCreatedNode->SetUInt32Value( (uint32)value->vu.integer_value );
					}
					else
					{
						pCreatedNode->SetUInt64Value( (uint64)value->vu.integer_value );
					}
				}
				else
				{
					// signed
					if( value->vu.integer_value >= INT_MIN )
					{
						pCreatedNode->SetInt32Value( (int32)value->vu.integer_value );
					}
					else
					{
						pCreatedNode->SetInt64Value( value->vu.integer_value );
					}
				}
				break;
	
			case JSON_T_FLOAT:
				pCreatedNode->SetDoubleValue( value->vu.float_value );
				break;

			case JSON_T_NULL:
				pCreatedNode->SetNullValue();
				break;

			case JSON_T_TRUE:
				pCreatedNode->SetBoolValue( true );
				break;

			case JSON_T_FALSE:
				pCreatedNode->SetBoolValue( false );
				break;

			case JSON_T_STRING:
				pCreatedNode->SetStringValue( value->vu.str.value );
				break;

			}
			break;

		default:
			Assert( false );
			break;
	}

	return 1;
}


// parses JSON into a tree of CWebAPIValues nodes with this as the root
CWebAPIValues *CWebAPIValues::ParseJSON( CUtlBuffer &inputBuffer )
{
	//
	// if there's nothing to parse, just early out
	inputBuffer.EatWhiteSpace();
	if( inputBuffer.GetBytesRemaining() == 0 )
		return NULL;

	// if the first character is the start of a string,
	// wrap the whole thing in an object so we can parse it.
	// We'll unwrap it at the end
	char cFirst = *(char *)inputBuffer.PeekGet();
	bool bWrapContent =  cFirst == '\"';

	JSON_config config;

	struct JSON_parser_struct* jc = NULL;

	init_JSON_config(&config);

	JSONParserContext_t context;
	context.m_pCurrentNode = NULL;
	context.m_pRootNode = NULL;
	context.m_bIsKey = false;

	config.depth                  = 19;
	config.callback               = &JSONParserCallback;
	config.allow_comments         = 1;
	config.handle_floats_manually = 0;
	config.callback_ctx = &context;
	config.malloc = malloc;
	config.free = free;

	jc = new_JSON_parser(&config);

	bool bSuccess = true;
	if( bWrapContent )
		JSON_parser_char( jc, '{' );
	while( inputBuffer.GetBytesRemaining( ) > 0 )
	{
		if( !JSON_parser_char( jc, (unsigned char)inputBuffer.GetChar() ) )
		{
			bSuccess = false;
			break;
		}
	}

	if( bWrapContent )
	{
		JSON_parser_char( jc, '}' );
	}

	if( bSuccess )
	{
		if (!JSON_parser_done(jc)) 
		{
			bSuccess = false;
		}
	}

	delete_JSON_parser(jc);

	// unwrap the root node
	if( bWrapContent && bSuccess )
	{
		CWebAPIValues *pWrapRoot = context.m_pRootNode;
		if( !pWrapRoot )
		{
			bSuccess = false;
		}
		else
		{
			CWebAPIValues *pRealRoot = pWrapRoot->GetFirstChild();
			if( !pRealRoot )
			{
				bSuccess = false;
			}
			else
			{
				if( pRealRoot->GetNextChild() )
				{
					bSuccess = false;
				}
				else
				{
					pWrapRoot->m_pFirstChild = NULL;
					pRealRoot->m_pParent = NULL;
					context.m_pRootNode = pRealRoot;

					delete pWrapRoot;
				}
			}
		}
	}

	if( bSuccess )
	{
		return context.m_pRootNode;
	}
	else
	{
		delete context.m_pRootNode;
		return NULL;
	}
}


CWebAPIValues *CWebAPIValues::ParseJSON( const char *pchJSONString )
{
	CUtlBuffer bufJSON( pchJSONString, Q_strlen( pchJSONString), CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
	return ParseJSON( bufJSON );
}


void CWebAPIValues::CopyFrom( const CWebAPIValues *pSource )
{
	switch( pSource->GetType() )
	{
		case k_EWebAPIValueType_Int32:
			SetInt32Value( pSource->GetInt32Value() );
			break;
		case k_EWebAPIValueType_Int64:
			SetInt64Value( pSource->GetInt64Value() );
			break;
		case k_EWebAPIValueType_UInt32:
			SetUInt32Value( pSource->GetUInt32Value() );
			break;
		case k_EWebAPIValueType_UInt64:
			SetUInt64Value( pSource->GetUInt64Value() );
			break;
		case k_EWebAPIValueType_Double:
			SetDoubleValue( pSource->GetDoubleValue() );
			break;

		case k_EWebAPIValueType_String:
			{
				CUtlString sValue;
				pSource->GetStringValue( sValue );
				SetStringValue( sValue );
			}
			break;

		case k_EWebAPIValueType_Bool:
			SetBoolValue( pSource->GetBoolValue() );
			break;

		case k_EWebAPIValueType_Null:
			SetNullValue( );
			break;

		case k_EWebAPIValueType_BinaryBlob:
			{
				CUtlBuffer bufValue;
				pSource->GetBinaryValue( bufValue );
				SetBinaryValue( (uint8 *)bufValue.Base(), bufValue.TellMaxPut() );
			}
			break;
			
		case k_EWebAPIValueType_Object:
			{
				ClearValue();
				m_eValueType = k_EWebAPIValueType_Object;

				const CWebAPIValues *pSourceChild = pSource->GetFirstChild();
				while( pSourceChild )
				{
					CWebAPIValues *pChild;
					if( pSourceChild->IsArray() )
						pChild = CreateChildArray( pSourceChild->GetName(), pSourceChild->GetElementName() );
					else
						pChild = CreateChildObject( pSourceChild->GetName() );

					pChild->CopyFrom( pSourceChild );
					pSourceChild = pSourceChild->GetNextChild();
				}
			}
			break;

		case k_EWebAPIValueType_NumericArray:
			{
				// the type should have been set when this node was first created
				Assert( m_eValueType == k_EWebAPIValueType_NumericArray );
				m_eValueType = k_EWebAPIValueType_NumericArray;

				const CWebAPIValues *pSourceChild = pSource->GetFirstChild();
				while( pSourceChild )
				{
					CWebAPIValues *pChild;
					if( pSourceChild->IsArray() )
						pChild = AddChildArrayToArray( pSourceChild->GetElementName() );
					else
						pChild = AddChildObjectToArray( );

					pChild->CopyFrom( pSourceChild );
					pSourceChild = pSourceChild->GetNextChild();
				}
			}
			break;

		default:
			AssertMsg( false, "Unknown type in CWebAPIValues::CopyFrom" );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fills a WebAPI key values from a corresponding protobuf message.
//-----------------------------------------------------------------------------
bool ProtoBufHelper::RecursiveAddProtoBufToWebAPIValues( CWebAPIValues *pWebAPIRoot, const ::google::protobuf::Message & msg )
{
	using ::google::protobuf::FieldDescriptor;
	const ::google::protobuf::Descriptor *pDescriptor = msg.GetDescriptor();
	const ::google::protobuf::Reflection *pReflection = msg.GetReflection();

	for ( int iField = 0; iField < pDescriptor->field_count(); iField++ )
	{
		const ::google::protobuf::FieldDescriptor *pField = pDescriptor->field( iField );
		const char *pFieldName = pField->name().c_str();

		if ( pField->is_repeated() )
		{
			if ( pReflection->FieldSize( msg, pField ) == 0 )
			{
				continue;		// No need to create the array if it is empty
								// If the field has been disabled externally, it has been cleared already (and thus will be skipped)
			}

			// bugbug: Is there a way to get this from the google API?
			const char *pchTypeName = "unknown_type";
			switch ( pField->cpp_type() )
			{
			case FieldDescriptor::CPPTYPE_INT32:	pchTypeName = "int32"; break;
			case FieldDescriptor::CPPTYPE_INT64:	pchTypeName = "int64"; break;
			case FieldDescriptor::CPPTYPE_UINT32:	pchTypeName = "uint32"; break;
			case FieldDescriptor::CPPTYPE_DOUBLE:	pchTypeName = "double"; break;
			case FieldDescriptor::CPPTYPE_FLOAT:	pchTypeName = "float"; break;
			case FieldDescriptor::CPPTYPE_BOOL:		pchTypeName = "bool"; break;
			case FieldDescriptor::CPPTYPE_ENUM:		pchTypeName = "enum"; break;
			case FieldDescriptor::CPPTYPE_STRING:
				{
					if ( pField->type() == FieldDescriptor::TYPE_STRING )
					{
						pchTypeName = "string";
					}
					else
					{
						AssertMsg1( pField->type() == FieldDescriptor::TYPE_BYTES, "Unrecognized field type: %d", pField->type() );
						pchTypeName = "bytes";
					}
					break;
				}
			case FieldDescriptor::CPPTYPE_MESSAGE:	pchTypeName = "message"; break;
			}

			CWebAPIValues *pContainer = pWebAPIRoot->CreateChildArray( pFieldName, pchTypeName );
			for ( int iRepeated = 0; iRepeated < pReflection->FieldSize( msg, pField ); iRepeated++ )
			{
				switch ( pField->cpp_type() )
				{
				case FieldDescriptor::CPPTYPE_INT32:	pContainer->SetChildInt32Value( NULL, pReflection->GetRepeatedInt32( msg, pField, iRepeated ) );			break;
				case FieldDescriptor::CPPTYPE_INT64:	pContainer->SetChildInt64Value( NULL, pReflection->GetRepeatedInt64( msg, pField, iRepeated ) );			break;
				case FieldDescriptor::CPPTYPE_UINT32:	pContainer->SetChildUInt32Value( NULL, pReflection->GetRepeatedUInt32( msg, pField, iRepeated ) );			break;
				case FieldDescriptor::CPPTYPE_UINT64:	pContainer->SetChildUInt64Value( NULL, pReflection->GetRepeatedUInt64( msg, pField, iRepeated ) );			break;
				case FieldDescriptor::CPPTYPE_DOUBLE:	pContainer->SetChildDoubleValue( NULL, pReflection->GetRepeatedDouble( msg, pField, iRepeated ) );			break;
				case FieldDescriptor::CPPTYPE_FLOAT:	pContainer->SetChildDoubleValue( NULL, pReflection->GetRepeatedFloat( msg, pField, iRepeated ) );			break;
				case FieldDescriptor::CPPTYPE_BOOL:		pContainer->SetChildBoolValue( NULL, pReflection->GetRepeatedBool( msg, pField, iRepeated ) );				break;
				case FieldDescriptor::CPPTYPE_ENUM:		pContainer->SetChildInt32Value( NULL, pReflection->GetRepeatedEnum( msg, pField, iRepeated )->number() );	break;
				case FieldDescriptor::CPPTYPE_STRING:
					{
						const std::string &strValue = pReflection->GetRepeatedString( msg, pField, iRepeated );
						if ( pField->type() == FieldDescriptor::TYPE_STRING )
						{
							pContainer->SetChildStringValue( NULL, strValue.c_str() );
						}
						else
						{
							// Binary blobs are automatically encoded in Base64 when converted to string by Web request
							pContainer->SetChildBinaryValue( NULL, (const uint8 *)strValue.c_str(), strValue.size() );
						}
						break;
					}
				case FieldDescriptor::CPPTYPE_MESSAGE:
					{
						const ::google::protobuf::Message &subMsg = pReflection->GetRepeatedMessage( msg, pField, iRepeated );
						CWebAPIValues *pChild = pContainer->CreateChildObject( NULL );
						if ( RecursiveAddProtoBufToWebAPIValues( pChild, subMsg ) == false )
						{
							return false;
						}
						break;
					}
				default:
					AssertMsg1( false, "Unknown cpp_type %d", pField->cpp_type() );
					return false;
				}
			}
		}
		else
		{
			if ( ( ( pReflection->HasField( msg, pField ) == false ) && ( pField->has_default_value() == false ) )
			//	|| pField->options().GetExtension( is_field_disabled_externally ) // Steam vesion supports this
			)
			{
				continue;		// If no field set and no default value set, there is no need to send the field
								// Or it is externally disabled (it has been cleared already but there still may be a default value set)
			}

			switch ( pField->cpp_type() )
			{
			case FieldDescriptor::CPPTYPE_INT32:	pWebAPIRoot->SetChildInt32Value( pFieldName, pReflection->GetInt32( msg, pField ) );			break;
			case FieldDescriptor::CPPTYPE_INT64:	pWebAPIRoot->SetChildInt64Value( pFieldName, pReflection->GetInt64( msg, pField ) );			break;
			case FieldDescriptor::CPPTYPE_UINT32:	pWebAPIRoot->SetChildUInt32Value( pFieldName, pReflection->GetUInt32( msg, pField ) );			break;
			case FieldDescriptor::CPPTYPE_UINT64:	pWebAPIRoot->SetChildUInt64Value( pFieldName, pReflection->GetUInt64( msg, pField ) );			break;
			case FieldDescriptor::CPPTYPE_DOUBLE:	pWebAPIRoot->SetChildDoubleValue( pFieldName, pReflection->GetDouble( msg, pField ) );			break;
			case FieldDescriptor::CPPTYPE_FLOAT:	pWebAPIRoot->SetChildDoubleValue( pFieldName, pReflection->GetFloat( msg, pField ) );			break;
			case FieldDescriptor::CPPTYPE_BOOL:		pWebAPIRoot->SetChildBoolValue( pFieldName, pReflection->GetBool( msg, pField ) );				break;
			case FieldDescriptor::CPPTYPE_ENUM:		pWebAPIRoot->SetChildInt32Value( pFieldName, pReflection->GetEnum( msg, pField )->number() );	break;
			case FieldDescriptor::CPPTYPE_STRING:
				{
					const std::string &strValue = pReflection->GetString( msg, pField );
					if ( pField->type() == FieldDescriptor::TYPE_STRING )
					{
						pWebAPIRoot->SetChildStringValue( pFieldName, strValue.c_str() );
					}
					else
					{
						AssertMsg1( pField->type() == FieldDescriptor::TYPE_BYTES, "Unrecognized field type: %d", pField->type() );
						// Binary blobs are automatically encoded in Base64 when converted to string by Web request
						pWebAPIRoot->SetChildBinaryValue( pFieldName, (const uint8 *)strValue.c_str(), strValue.size() );
					}
					break;
				}
			case FieldDescriptor::CPPTYPE_MESSAGE:
				{
					CWebAPIValues *pChild = pWebAPIRoot->CreateChildObject( pFieldName );
#undef GetMessage	// Work around unfortunate Microsoft macro
					const ::google::protobuf::Message &subMsg = pReflection->GetMessage( msg, pField );
#ifdef UNICODE
#define GetMessage  GetMessageW
#else
#define GetMessage  GetMessageA
#endif // !UNICODE
					if ( RecursiveAddProtoBufToWebAPIValues( pChild, subMsg ) == false )
					{
						return false;
					}
					break;
				}
			default:
				AssertMsg1( false, "Unknown cpp_type %d", pField->cpp_type() );
				return false;
			}
		}
	}
	return true;
}

