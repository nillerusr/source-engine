//====== Copyright © 1996-2010, Valve Corporation, All rights reserved. =======
//
// Purpose: HTTP related enums and objects, stuff that both clients and server use should go here
//
//=============================================================================

#include "stdafx.h"
#include <time.h>

#include "msgprotobuf.h"
#include "rtime.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace GCSDK;

//-----------------------------------------------------------------------------
// Purpose: Check if the given status code is one that by spec allows a body, or is
//          one that "MUST NOT" include an entity body
//-----------------------------------------------------------------------------
bool CHTTPUtil::BStatusCodeAllowsBody( EHTTPStatusCode eHTTPStatus )
{
	switch( eHTTPStatus )
	{
	case k_EHTTPStatusCode100Continue:
	case k_EHTTPStatusCode101SwitchingProtocols:
	case k_EHTTPStatusCode204NoContent:
	case k_EHTTPStatusCode205ResetContent:
	case k_EHTTPStatusCode304NotModified:
		return false;

	default:
		break;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Wrapper to ease use of Steam Web APIs
//-----------------------------------------------------------------------------
CSteamAPIRequest::CSteamAPIRequest( EHTTPMethod eMethod, const char *pchInterface, const char *pchMethod, int nVersion ) :
	CHTTPRequest( eMethod, "api.steampowered.com", CFmtStr( "/%s/%s/v%04d/", pchInterface, pchMethod, nVersion ) )
{
	if ( k_EHTTPMethodPOST == eMethod )
	{
		SetPOSTParamString( "format", "vdf" );
		SetPOSTParamString( "key", GGCBase()->GetSteamAPIKey() );
	}
	else
	{
		SetGETParamString( "format", "vdf" );
		SetGETParamString( "key", GGCBase()->GetSteamAPIKey() );
	}

	EUniverse universe = GGCHost()->GetUniverse();
	if ( universe == k_EUniverseBeta )
	{
		SetHostname( "api-beta.steampowered.com" );
	}
	else if ( universe == k_EUniverseDev )
	{
		// Set this to your dev universe API endpoint if not local
		SetHostname( "localhost:8282" );
	}
}

CHTTPRequest::CHTTPRequest( CMsgHttpRequest* pProto )
{
	Init( pProto );
}

//-----------------------------------------------------------------------------
// Purpose: Construct a request object, it's invalid until data is setup
//-----------------------------------------------------------------------------
CHTTPRequest::CHTTPRequest()
{
	Init( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Common init code
//-----------------------------------------------------------------------------
void CHTTPRequest::Init( CMsgHttpRequest* pProto )
{
	//see if we are wrapping a proto that already exists
	if( pProto )
	{
		m_pProto = pProto;
		m_bOwnProto = false;
	}
	else
	{
		m_pProto = new CMsgHttpRequest;
		m_bOwnProto = true;

		SetEHTTPMethod( k_EHTTPMethodInvalid );
		// Default URL
		SetURL( "/" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHTTPRequest::CHTTPRequest( EHTTPMethod eMethod, const char *pchHost, const char *pchRelativeURL )
{
	Init( NULL );

	SetEHTTPMethod( eMethod );
	SetHostname( pchHost );
	SetURL( pchRelativeURL );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHTTPRequest::CHTTPRequest( EHTTPMethod eMethod, const char *pchAbsoluteURL )
{
	Init( NULL );

	SetEHTTPMethod( eMethod );

	// We need to break the URL down into host + relativeURL
	const char *pchHost = Q_strstr( pchAbsoluteURL, "://" );
	if ( pchHost )
	{
		// Skip past protocol
		pchHost += 3;

		// Find the next /, that is the beginning of the actual URL
		const char *pchURL = Q_strstr( pchHost, "/");
		if ( !pchURL )
		{
			// No URL specified after host? just default to / then.
			SetURL( "/" );
		}
		else
		{
			// Ok, now we must remove the query string
			const char *pchQueryString = Q_strstr( pchURL, "?" );
			if ( !pchQueryString )
			{
				// No query string, full thing is the URL
				SetURL( pchURL );
			}
			else
			{
				// URL is everything before query string
				SetURLDirect( pchURL, pchQueryString - pchURL );

				Assert( *pchQueryString == '?' );
				pchQueryString++;
				if( *pchQueryString )
				{
					m_pProto->mutable_get_params()->Clear();
					CUtlVector<CMsgHttpRequest_QueryParam> vecParams;
					RetrieveURLEncodedData( pchQueryString, Q_strlen( pchQueryString ), vecParams );
					FOR_EACH_VEC( vecParams, i )
					{
						*m_pProto->add_get_params() = vecParams[i];
					}
				}
			}
		}

		// Is there a userinfo separator in the hostname portion? We don't support
		// username/password authentication in the URL, but we must still skip it.
		const char *pchUserinfoSep = strchr( pchHost, '@' );
		if ( pchUserinfoSep &&  ( !pchURL || pchUserinfoSep < pchURL ) )
		{
			pchHost = pchUserinfoSep + 1;
		}

		// If we found a URL only set upto that, otherwise set everything as host
		if ( pchURL )
			SetHostnameDirect( pchHost, pchURL-pchHost );
		else
			SetHostname( pchHost );
	}
	else
	{
		AssertMsg( false, "Bad absolute URL to CHTTPRequest constructor, must start with protocol://" );
		EmitError( SPEW_GC, "Bad absolute URL to CHTTPRequest constructor, must start with protocol://" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CHTTPRequest::~CHTTPRequest()
{
	if( m_bOwnProto )
		delete m_pProto;
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a GET parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
const CMsgHttpRequest_QueryParam *CHTTPRequest::GetGETParam( const char *pchGetParamName, bool bMatchCase ) const
{
	const uint32 nNumParams = m_pProto->get_params_size(); 
	for( uint32 nParam = 0; nParam < nNumParams; nParam++ )
	{
		const CMsgHttpRequest_QueryParam& param = m_pProto->get_params( nParam );
		if ( bMatchCase )
		{
			if ( Q_strcmp( param.name().c_str(), pchGetParamName ) == 0 )
				return &param;
		}
		else
		{
			if ( Q_stricmp( param.name().c_str(), pchGetParamName ) == 0 )
				return &param;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a GET parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
const char *CHTTPRequest::GetGETParamString( const char *pchGetParamName, const char *pchDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetGETParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return pchDefault;

	return pParam->value().c_str();
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a GET parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
bool CHTTPRequest::GetGETParamBool( const char *pchGetParamName, bool bDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetGETParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return bDefault;

	return ( Q_atoi( pParam->value().c_str() ) != 0);
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a GET parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
int32 CHTTPRequest::GetGETParamInt32( const char *pchGetParamName, int32 nDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetGETParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return nDefault;

	return Q_atoi( pParam->value().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a GET parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
uint32 CHTTPRequest::GetGETParamUInt32( const char *pchGetParamName, uint32 unDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetGETParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return unDefault;

	return (uint32)V_atoui64( pParam->value().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a GET parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
int64 CHTTPRequest::GetGETParamInt64( const char *pchGetParamName, int64 nDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetGETParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return nDefault;

	return V_atoi64( pParam->value().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a GET parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
uint64 CHTTPRequest::GetGETParamUInt64( const char *pchGetParamName, uint64 unDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetGETParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return unDefault;

	return V_atoui64( pParam->value().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a GET parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
float CHTTPRequest::GetGETParamFloat( const char *pchGetParamName, float fDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetGETParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return fDefault;

	return Q_atof( pParam->value().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a POST parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
const CMsgHttpRequest_QueryParam *CHTTPRequest::GetPOSTParam( const char *pchPostParamName, bool bMatchCase ) const
{
	const uint32 nNumParams = m_pProto->post_params_size(); 
	for( uint32 nParam = 0; nParam < nNumParams; nParam++ )
	{
		const CMsgHttpRequest_QueryParam& param = m_pProto->post_params( nParam );
		if ( bMatchCase )
		{
			if ( Q_strcmp( param.name().c_str(), pchPostParamName ) == 0 )
				return &param;
		}
		else
		{
			if ( Q_stricmp( param.name().c_str(), pchPostParamName ) == 0 )
				return &param;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a POST parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
const char *CHTTPRequest::GetPOSTParamString( const char *pchGetParamName, const char *pchDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetPOSTParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return pchDefault;

	return pParam->value().c_str();
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a POST parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
bool CHTTPRequest::GetPOSTParamBool( const char *pchGetParamName, bool bDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetPOSTParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return bDefault;

	return ( Q_atoi( pParam->value().c_str() ) != 0);
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a POST parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
int32 CHTTPRequest::GetPOSTParamInt32( const char *pchGetParamName, int32 nDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetPOSTParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return nDefault;

	return Q_atoi( pParam->value().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a POST parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
uint32 CHTTPRequest::GetPOSTParamUInt32( const char *pchGetParamName, uint32 unDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetPOSTParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return unDefault;

	return (uint32)V_atoui64( pParam->value().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a POST parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
int64 CHTTPRequest::GetPOSTParamInt64( const char *pchGetParamName, int64 nDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetPOSTParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return nDefault;

	return V_atoi64( pParam->value().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a POST parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
uint64 CHTTPRequest::GetPOSTParamUInt64( const char *pchGetParamName, uint64 unDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetPOSTParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return unDefault;

	return V_atoui64( pParam->value().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: Get the value of a POST parameter, this is case-insensitive by default.
//-----------------------------------------------------------------------------
float CHTTPRequest::GetPOSTParamFloat( const char *pchGetParamName, float fDefault, bool bMatchCase ) const
{
	const CMsgHttpRequest_QueryParam *pParam = GetPOSTParam( pchGetParamName, bMatchCase );
	if ( !pParam )
		return fDefault;

	return Q_atof( pParam->value().c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: Add a GET param to the request
//-----------------------------------------------------------------------------
void CHTTPRequest::SetGETParamRaw( const char *pchGetParamName, uint8 *pData, uint32 cubDataLen )
{
	// See if it already exists, and overwrite then (case sensitive!)
	CMsgHttpRequest_QueryParam *pParam = const_cast< CMsgHttpRequest_QueryParam* >( GetGETParam( pchGetParamName, true ) );
	if ( !pParam )
	{
		// Add a new one then
		pParam = m_pProto->add_get_params();
		pParam->set_name( pchGetParamName );
	}

	pParam->set_value( pData, cubDataLen );
}


//-----------------------------------------------------------------------------
// Purpose: Add a POST param to the request given a string for the name and value
//-----------------------------------------------------------------------------
void CHTTPRequest::SetPOSTParamRaw( const char *pchPostParamName, uint8 *pData, uint32 cubDataLen )
{
	// See if it already exists, and overwrite then (case sensitive!)
	CMsgHttpRequest_QueryParam *pParam = const_cast< CMsgHttpRequest_QueryParam* >( GetPOSTParam( pchPostParamName, true ) );
	if ( !pParam )
	{
		// Add a new one then
		pParam = m_pProto->add_post_params();
		pParam->set_name( pchPostParamName );
	}

	pParam->set_value( pData, cubDataLen );
}


//-----------------------------------------------------------------------------
// Fetch a request headers value by header name
//-----------------------------------------------------------------------------
const char * CHTTPRequest::GetRequestHeaderValue( const char *pchRequestHeaderName, const char *pchDefault ) const
{
	const uint32 nNumHeaders = m_pProto->headers_size();
	for( uint32 nHeader = 0; nHeader < nNumHeaders; nHeader++ )
	{
		const CMsgHttpRequest_RequestHeader& header = m_pProto->headers( nHeader );
		if( Q_stricmp( header.name().c_str(), pchRequestHeaderName ) == 0 )
		{
			return header.value().c_str();
		}
	}

	return pchDefault;	
}

//-----------------------------------------------------------------------------
// Set a header field for the request
//-----------------------------------------------------------------------------
void CHTTPRequest::SetRequestHeaderValue( const char *pchHeaderName, const char *pchHeaderString ) 
{
	const uint32 nNumHeaders = m_pProto->headers_size();
	for( uint32 nHeader = 0; nHeader < nNumHeaders; nHeader++ )
	{
		CMsgHttpRequest_RequestHeader* pHeader = m_pProto->mutable_headers( nHeader );
		if( Q_stricmp( pHeader->name().c_str(), pchHeaderName ) == 0 )
		{
			pHeader->set_value( pchHeaderString );
			return;
		}
	}

	CMsgHttpRequest_RequestHeader* pHeader = m_pProto->add_headers();
	pHeader->set_name( pchHeaderName );
	pHeader->set_value( pchHeaderString );
}

//-----------------------------------------------------------------------------
// Purpose: Fetch a request header by header name and convert it to a time value
// This is just a helpful wrapper of GetRequestHeaderValue that deals with
// parsing the time value
//-----------------------------------------------------------------------------
RTime32 CHTTPRequest::GetRequestHeaderTimeValue( const char *pchRequestHeaderName, RTime32 rtDefault ) const
{
	const char *pchValue = GetRequestHeaderValue( pchRequestHeaderName );
	if( !pchValue )
		return rtDefault;

	return CRTime::RTime32FromHTTPDateString( pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set the body data for the request
//-----------------------------------------------------------------------------
void CHTTPRequest::SetBodyData( const void *pubData, uint32 cubData ) 
{ 
	m_pProto->set_body( pubData, cubData );
}


//-----------------------------------------------------------------------------
// Purpose: Get data out of a query string
//-----------------------------------------------------------------------------
void CHTTPRequest::RetrieveURLEncodedData( const char *pchQueryString, int nQueryStrLen, CUtlVector<CMsgHttpRequest_QueryParam> &vecParams )
{
	CUtlBuffer bufParamName( 0, 64 );
	CUtlBuffer bufParamValue( 0, 128 );

	// We shouldn't get passed the ? from a query string, but if we do just skip it and parse ok anyway
	if ( nQueryStrLen && pchQueryString[0] == '?' )
	{
		++pchQueryString;
		--nQueryStrLen;
	}

	bool bInParamValue = false;
	int iTokenStart = 0;
	for( int i=0; i < nQueryStrLen; ++i )
	{
		if ( pchQueryString[i] == '=' && !bInParamValue )
		{
			// = switches to value from name, starts a new value token
			bufParamName.Put( &pchQueryString[iTokenStart], i - iTokenStart );
			bInParamValue = true;
			iTokenStart = i + 1;
		}
		else if ( pchQueryString[i] == '&' )
		{
			// & terminates a value and starts a new name token
			if ( bInParamValue )
			{
				bufParamValue.Put( &pchQueryString[iTokenStart], i - iTokenStart );

				int iIndex = vecParams.AddToTail();
				CMsgHttpRequest_QueryParam *pParam = &vecParams[iIndex];

				uint32 unNameLen = Q_URLDecode( (char*)bufParamName.Base(), bufParamName.TellPut(), (const char*)bufParamName.Base(), bufParamName.TellPut() );
				pParam->set_name( (const char*)bufParamName.Base(), unNameLen );

				uint32 unDataLen = (uint32)Q_URLDecode( (char*)bufParamValue.Base(), bufParamValue.TellPut(), (const char*)bufParamValue.Base(), bufParamValue.TellPut() );
				pParam->set_value( (const uint8*)bufParamValue.Base(), unDataLen );
			}
			bufParamName.Clear();
			bufParamValue.Clear();
			bInParamValue = false;
			iTokenStart = i+1;
		}
	}

	// Use any left over value from the end of the query string
	if ( bInParamValue )
	{
		bufParamValue.Put( &pchQueryString[iTokenStart], nQueryStrLen - iTokenStart );

		int iIndex = vecParams.AddToTail();
		CMsgHttpRequest_QueryParam *pParam = &vecParams[iIndex];

		uint32 unNameLen = Q_URLDecode( (char*)bufParamName.Base(), bufParamName.TellPut(), (const char*)bufParamName.Base(), bufParamName.TellPut() );
		pParam->set_name( (const char*)bufParamName.Base(), unNameLen );

		uint32 unDataLen = (uint32)Q_URLDecode( (char*)bufParamValue.Base(), bufParamValue.TellPut(), (const char*)bufParamValue.Base(), bufParamValue.TellPut() );
		pParam->set_value( (const uint8*)bufParamValue.Base(), unDataLen );
	}
}


//----------------------------------------------------------------------------
// Purpose: Gets a singleton buffer pool for HTTP responses 
//----------------------------------------------------------------------------
static GCConVar http_response_max_pool_size_mb( "http_response_max_pool_size_mb", "10", "Maximum size in bytes of the HTTP Response buffer pool" );
static GCConVar http_response_init_buffer_size( "http_response_init_buffer_size", "65536", "Initial buffer size for buffers in the HTTP Response buffer pool" );
/*static*/ CBufferPool &CHTTPResponse::GetBufferPool()
{
	static CBufferPool s_bufferPool( "HTTP Response", http_response_max_pool_size_mb, http_response_init_buffer_size, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::CONTAINS_CRLF );
	return s_bufferPool;
}


//-----------------------------------------------------------------------------
// Purpose: Construct a response object, it defaults to being a 500 internal server error response
//-----------------------------------------------------------------------------
CHTTPResponse::CHTTPResponse() : 
m_pbufBody( GetBufferPool().GetBuffer() ),
m_eStatusCode( k_EHTTPStatusCode500InternalServerError )
{
	m_pkvResponseHeaders = new KeyValues( "ResponseHeaders" );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CHTTPResponse::~CHTTPResponse() 
{
	GetBufferPool().ReturnBuffer( m_pbufBody );
	m_pbufBody = NULL;

	if ( m_pkvResponseHeaders )
		m_pkvResponseHeaders->deleteThis();

	m_pkvResponseHeaders = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the "expiration" response header to a given number of seconds 
// ahead or behind the current time.  You can set the Expires header directly as well,
// this is just a helper so you don't have to deal with time formatting.
//-----------------------------------------------------------------------------
void CHTTPResponse::SetExpirationHeaderDeltaFromNow( int32 nSecondsFromNow )
{
	time_t rawtime;
	time( &rawtime );
	rawtime += nSecondsFromNow;
	SetHeaderTimeValue( "expires", rawtime );
}


//-----------------------------------------------------------------------------
// Purpose: Formats a time value and sets it as a header. This is a helper so
// you don't have to deal with time formatting
//-----------------------------------------------------------------------------
void CHTTPResponse::SetHeaderTimeValue( const char *pchHeaderName, RTime32 rtTimestamp )
{
	char rgchDate[128];
	struct tm tmStruct;
	time_t rawtime = rtTimestamp;
	Plat_gmtime( &rawtime, &tmStruct );
	DbgVerify( strftime( rgchDate, 128, "%a, %d %b %Y %H:%M:%S GMT", &tmStruct ) );
	SetResponseHeaderValue( pchHeaderName, rgchDate );
}


//-----------------------------------------------------------------------------
// Purpose: Serializes the response into a message object (for proxying between
// back-end Steam servers).
//-----------------------------------------------------------------------------
void CHTTPResponse::SerializeIntoProtoBuf( CMsgHttpResponse & response ) const
{
	MEM_ALLOC_CREDIT();
	response.set_status_code( m_eStatusCode );

	FOR_EACH_VALUE( m_pkvResponseHeaders, pkvRequestHeader )
	{
		const char *pchName = pkvRequestHeader->GetName();
		const char *pchValue = pkvRequestHeader->GetString();

		if ( pchName && pchValue )
		{
			CMsgHttpResponse_ResponseHeader *pHeader = response.add_headers();
			pHeader->set_name( pchName );
			pHeader->set_value( pchValue );
		}
	}

	if( m_pbufBody->TellPut() > 0 )
		response.set_body( m_pbufBody->Base(), m_pbufBody->TellPut() );
}


//-----------------------------------------------------------------------------
// Purpose: Deserializes the response from a message object (for proxying between
// back-end Steam servers).
//-----------------------------------------------------------------------------
void CHTTPResponse::DeserializeFromProtoBuf( const CMsgHttpResponse & response )
{
	m_eStatusCode = (EHTTPStatusCode)response.status_code();

	for( int i=0; i<response.headers_size(); i++ )
	{
		m_pkvResponseHeaders->SetString( response.headers(i).name().c_str(), response.headers(i).value().c_str() );
	}

	if( response.has_body() )
		m_pbufBody->Put( response.body().data(), response.body().size() );
}

