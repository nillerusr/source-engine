//====== Copyright © 1996-2010, Valve Corporation, All rights reserved. =======
//
// Purpose: HTTP related enums and objects, stuff that both clients and server use should go here
//
//=============================================================================

#ifndef HTTP_H
#define HTTP_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steamhttpenums.h"
#include "tier1/KeyValues.h"
#include "tier1/netadr.h"
#include "gcsdk/bufferpool.h"

class CMsgHttpRequest;
class CMsgHttpResponse;
#include "tier0/memdbgon.h"

namespace GCSDK
{

// Container class for useful parsing methods and other utility code used by client or server http code
class CHTTPUtil
{
public:
	// Check if a status code allows a body to exist
	static bool BStatusCodeAllowsBody( EHTTPStatusCode eHTTPStatus );

};


class CHTTPRequest;
class CHTTPResponse;
class CHTTPServerClientConnection;

class CHTTPRequest
{
public:

	CHTTPRequest();
	CHTTPRequest( CMsgHttpRequest* pProto );
	CHTTPRequest( EHTTPMethod eMethod, const char *pchHost, const char *pchRelativeURL );
	CHTTPRequest( EHTTPMethod eMethod, const char *pchAbsoluteURL );
	~CHTTPRequest();

	// Get the method type for the request (ie, GET, POST, etc)
	EHTTPMethod GetEHTTPMethod() const { return ( EHTTPMethod )m_pProto->request_method(); }

	// Get the relative URL for the request
	const char *GetURL() const { return m_pProto->url().c_str(); }

	// Get the value of a GET parameter, using the default value if not set.  This is case-insensitive by default.
	const CMsgHttpRequest_QueryParam *GetGETParam( const char *pchGetParamName, bool bMatchCase = false ) const;
	const char *GetGETParamString( const char *pchGetParamName, const char *pchDefault, bool bMatchCase = false ) const;
	bool	GetGETParamBool( const char *pchGetParamName, bool bDefault, bool bMatchCase = false ) const;
	int32	GetGETParamInt32( const char *pchGetParamName, int32 nDefault, bool bMatchCase = false ) const;
	uint32	GetGETParamUInt32( const char *pchGetParamName, uint32 unDefault, bool bMatchCase = false ) const;
	int64	GetGETParamInt64( const char *pchGetParamName, int64 nDefault, bool bMatchCase = false ) const;
	uint64	GetGETParamUInt64( const char *pchGetParamName, uint64 unDefault, bool bMatchCase = false ) const;
	float	GetGETParamFloat( const char *pchGetParamName, float fDefault, bool bMatchCase = false ) const;


	// Get the value of a POST parameter, using the default value if not set.  This is case-insensitive by default.
	const CMsgHttpRequest_QueryParam *GetPOSTParam( const char *pchPostParamName, bool bMatchCase = false ) const;
	const char *GetPOSTParamString( const char *pchGetParamName, const char *pchDefault, bool bMatchCase = false ) const;
	bool	GetPOSTParamBool( const char *pchGetParamName, bool bDefault, bool bMatchCase = false ) const;
	int32	GetPOSTParamInt32( const char *pchGetParamName, int32 nDefault, bool bMatchCase = false ) const;
	uint32	GetPOSTParamUInt32( const char *pchGetParamName, uint32 unDefault, bool bMatchCase = false ) const;
	int64	GetPOSTParamInt64( const char *pchGetParamName, int64 nDefault, bool bMatchCase = false ) const;
	uint64	GetPOSTParamUInt64( const char *pchGetParamName, uint64 unDefault, bool bMatchCase = false ) const;
	float	GetPOSTParamFloat( const char *pchGetParamName, float fDefault, bool bMatchCase = false ) const;

	// Add a GET param to the request
	void SetGETParamString( const char *pchGetParamName, const char *pString ) { SetGETParamRaw( pchGetParamName, (uint8*)pString, Q_strlen(pString) ); }
	void SetGETParamBool( const char *pchPostParamName, bool bValue ) { SetGETParamRaw( pchPostParamName, (uint8*)(bValue ? "1" : "0"), 1 ); }
	void SetGETParamInt32( const char *pchPostParamName, int32 nValue ) { CNumStr str( nValue ); SetGETParamRaw( pchPostParamName, (uint8*)str.String(), Q_strlen( str ) ); }
	void SetGETParamUInt32( const char *pchPostParamName, uint32 unValue ) { CNumStr str( unValue ); SetGETParamRaw( pchPostParamName, (uint8*)str.String(), Q_strlen( str ) ); }
	void SetGETParamInt64( const char *pchPostParamName, int64 nValue ) { CNumStr str( nValue ); SetGETParamRaw( pchPostParamName, (uint8*)str.String(), Q_strlen( str ) ); }
	void SetGETParamUInt64( const char *pchPostParamName, uint64 unValue ) { CNumStr str( unValue ); SetGETParamRaw( pchPostParamName, (uint8*)str.String(), Q_strlen( str ) ); }
	void SetGETParamFloat( const char *pchPostParamName, float fValue ) { CNumStr str( fValue ); SetGETParamRaw( pchPostParamName, (uint8*)str.String(), Q_strlen( str ) ); }

	// Add a POST param to the request given a string for the name and value
	void SetPOSTParamString( const char *pchPostParamName, const char *pString ) { SetPOSTParamRaw( pchPostParamName, (uint8*)pString, Q_strlen(pString) ); }
	void SetPOSTParamBool( const char *pchPostParamName, bool bValue ) { SetPOSTParamRaw( pchPostParamName, (uint8*)(bValue ? "1" : "0"), 1 ); }
	void SetPOSTParamInt32( const char *pchPostParamName, int32 nValue ) { CNumStr str( nValue ); SetPOSTParamRaw( pchPostParamName, (uint8*)str.String(), Q_strlen( str ) ); }
	void SetPOSTParamUInt32( const char *pchPostParamName, uint32 unValue ) { CNumStr str( unValue ); SetPOSTParamRaw( pchPostParamName, (uint8*)str.String(), Q_strlen( str ) ); }
	void SetPOSTParamInt64( const char *pchPostParamName, int64 nValue ) { CNumStr str( nValue ); SetPOSTParamRaw( pchPostParamName, (uint8*)str.String(), Q_strlen( str ) ); }
	void SetPOSTParamUInt64( const char *pchPostParamName, uint64 unValue ) { CNumStr str( unValue ); SetPOSTParamRaw( pchPostParamName, (uint8*)str.String(), Q_strlen( str ) ); }
	void SetPOSTParamFloat( const char *pchPostParamName, float fValue ) { CNumStr str( fValue ); SetPOSTParamRaw( pchPostParamName, (uint8*)str.String(), Q_strlen( str ) ); }

	// Adds a POST param containing raw data to the request. If you are using the Web API, you probably do not want this function
	void SetPOSTParamRaw( const char *pchPostParamName, uint8 *pData, uint32 cubDataLen );

	// Raw iteration support for GET and POST parameters
	uint32 GetPOSTParamCount() const					{ return m_pProto->post_params_size(); }
	const char* GetPOSTParamName( uint32 i ) const		{ return m_pProto->post_params( i ).name().c_str(); }
	const char* GetPOSTParamValue( uint32 i ) const		{ return m_pProto->post_params( i ).value().c_str(); }
	uint32 GetGETParamCount() const						{ return m_pProto->get_params_size(); }
	const char* GetGETParamName( uint32 i ) const		{ return m_pProto->get_params( i ).name().c_str(); }
	const char* GetGETParamValue( uint32 i ) const		{ return m_pProto->get_params( i ).value().c_str(); }

	// Fetch a request header by header name and convert it to a time value
	RTime32 GetRequestHeaderTimeValue( const char *pchRequestHeaderName, RTime32 rtDefault = 0 ) const;

	// Fetch a request headers value by header name
	const char *GetRequestHeaderValue( const char *pchRequestHeaderName, const char *pchDefault = NULL ) const;

	// Set a header field for the request
	void SetRequestHeaderValue( const char *pchHeaderName, const char *pchHeaderString );

	// Set the method for the request object
	void SetEHTTPMethod( EHTTPMethod eMethod ) { m_pProto->set_request_method( eMethod ); }

	// Set the relative URL for the request
	void SetURL( const char *pchURL ) 
	{ 
		AssertMsg( pchURL && pchURL[0] == '/', "URLs must start with the slash (/) character. Param: %s", pchURL );
		m_pProto->set_url( pchURL );
	}

	void SetURLDirect( const char *pchURL, size_t size ) 
	{ 
		AssertMsg( pchURL && pchURL[0] == '/', "URLs must start with the slash (/) character. Param: %s", pchURL );
		m_pProto->set_url( pchURL, size );
	}

	// Set body data
	void SetBodyData( const void *pubData, uint32 cubData );
	bool BHasBodyData()									{ return m_pProto->has_body(); }
	uint GetBodyDataSize() const							{ return (uint)m_pProto->body().size(); }
	const char* GetBodyData() const						{ return m_pProto->body().c_str(); }

	// Set hostname for request to target (or host it was received on)
	void SetHostname( const char *pchHost ) { m_pProto->set_hostname( pchHost ); }
	void SetHostnameDirect( const char *pchHost, uint32 unLength ) { m_pProto->set_hostname( pchHost, unLength ); }
	const char *GetHostname() const { return m_pProto->hostname().c_str(); }

	// Set the overall timeout in seconds for this request
	void SetAbsoluteTimeoutSeconds( uint32 unSeconds ) { m_pProto->set_absolute_timeout( unSeconds ); }

	// Get the total absolute timeout value for the request
	uint32 GetAbsoluteTimeoutSeconds() { return m_pProto->absolute_timeout(); }

	//accesses the HTTP message
	const CMsgHttpRequest& GetProtoObj() const		{ return *m_pProto; }

	static void RetrieveURLEncodedData( const char *pchQueryString, int nQueryStrLen, CUtlVector<CMsgHttpRequest_QueryParam> &vecParams );

protected:

	// Adds a GET param containing raw data to the request. If you are using the Web API, you probably do not want this function
	void SetGETParamRaw( const char *pchGetParamName, uint8 *pData, uint32 cubDataLen );

	// Common initialization code
	void Init( CMsgHttpRequest* pProto );

	//the protobuf object that stores all of our contents
	CMsgHttpRequest*	m_pProto;
	//whether or not we own this proto and should free it when the object is destroyed
	bool				m_bOwnProto;
};


//-----------------------------------------------------------------------------
// Purpose: A wrapper to CHTTPRequest for Steam WebAPIs. Host, mode, and API key 
//	are set automatically
//-----------------------------------------------------------------------------
class CSteamAPIRequest : public CHTTPRequest
{
public:
	CSteamAPIRequest( EHTTPMethod eMethod, const char *pchInterface, const char *pchMethod, int nVersion );
};


class CHTTPResponse
{
public:
	CHTTPResponse();
	virtual ~CHTTPResponse();

	// Get a specific headers data 
	const char *GetResponseHeaderValue( const char *pchName, const char *pchDefault = NULL ) const { return m_pkvResponseHeaders->GetString( pchName, pchDefault ); }

	// Set a specific headers data, will clobber any existing value for that header
	virtual void SetResponseHeaderValue( const char *pchName, const char *pchValue ) { m_pkvResponseHeaders->SetString( pchName, pchValue ); }

	// Set status code for response
	virtual void SetStatusCode( EHTTPStatusCode eStatusCode ) { m_eStatusCode = eStatusCode; }

	// Set the expiration header based on a time delta from now
	void SetExpirationHeaderDeltaFromNow( int32 nSecondsFromNow );

	// Set the expiration header based on a time delta from now
	void SetHeaderTimeValue( const char *pchHeaderName, RTime32 rtTimestamp );

	// Get the entire headers KV (in case you want to iterate them all or such)
	KeyValues *GetResponseHeadersKV() const { return m_pkvResponseHeaders; }

	// Accessors to the body data in the response
	const uint8 *GetPubBody() const			{ return (uint8*)m_pbufBody->Base(); }
	uint32 GetCubBody() const				{ return m_pbufBody->TellPut(); }
	const CUtlBuffer *GetBodyBuffer() const { return m_pbufBody; }
	CUtlBuffer *GetBodyBuffer()				{ return m_pbufBody; }

	// Accessor
	EHTTPStatusCode GetStatusCode() const { return m_eStatusCode; }

	// writes the response into a protobuf for use in messages
	void SerializeIntoProtoBuf( CMsgHttpResponse & response ) const;

	// reads the response out of a protobuf from a message
	void DeserializeFromProtoBuf( const CMsgHttpResponse & response );

protected:
	static CBufferPool &GetBufferPool();

	EHTTPStatusCode m_eStatusCode;
	CUtlBuffer *m_pbufBody;
	KeyValues *m_pkvResponseHeaders;
};

}
#include "tier0/memdbgoff.h"

#endif // HTTP_H
