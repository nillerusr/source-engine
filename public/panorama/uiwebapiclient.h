//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose:  Helper classes/functions for performing web api requests from ui code
//
//=============================================================================//

#ifndef UIWEBAPICLIENT_H
#define UIWEBAPICLIENT_H

#ifdef _WIN32
#pragma once
#endif

#include "uievent.h"


namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Helper for performing async webapi requests
//-----------------------------------------------------------------------------
inline uint64 MakeAsyncJSONWebAPIRequest( EHTTPMethod eMethod, const char *pchWebAPIURL, CJSONWebAPIParams *pParams, HTTPCookieContainerHandle hCookieContianer, IUIPanel *pCallbackTargetPanel, void *pContext )
{
	return UIEngine()->InitiateAsyncJSONWebAPIRequest( eMethod, pchWebAPIURL, pCallbackTargetPanel, pContext, pParams, hCookieContianer );
}

//-----------------------------------------------------------------------------
// Purpose: Helper for performing async webapi requests
//-----------------------------------------------------------------------------
inline uint64 MakeAsyncJSONWebAPIRequest( EHTTPMethod eMethod, const char *pchWebAPIURL, CJSONWebAPIParams *pParams, HTTPCookieContainerHandle hCookieContianer, JSONWebAPIDelegate_t callback, void *pContext )
{
	return UIEngine()->InitiateAsyncJSONWebAPIRequest( eMethod, pchWebAPIURL, callback, pContext, pParams, hCookieContianer );
}


//-----------------------------------------------------------------------------
// Purpose: Helper for performing async webapi requests
//-----------------------------------------------------------------------------
inline uint32 MakeAsyncJSONWebAPIRequest( const char *pchWebAPIURL, IUIPanel *pCallbackTargetPanel, void *pContext )
{
	return UIEngine()->InitiateAsyncJSONWebAPIRequest( k_EHTTPMethodGET, pchWebAPIURL, pCallbackTargetPanel, pContext );
}


//-----------------------------------------------------------------------------
// Purpose: Helper for performing async webapi requests. When using this version, objects pointed to
// by the provided delegate must live for the duration of the web request. Before destroying those objects,
// you must get a completion callback or CancelAsyncJSONWebAPIRequest.
//
// Use panel & event versions above if you do not want to manage object & request lifetimes.
//-----------------------------------------------------------------------------
inline uint32 MakeAsyncJSONWebAPIRequest( const char *pchWebAPIURL, JSONWebAPIDelegate_t callback, void *pContext )
{
	return UIEngine()->InitiateAsyncJSONWebAPIRequest( k_EHTTPMethodGET, pchWebAPIURL, callback, pContext );
}


//-----------------------------------------------------------------------------
// Purpose: Cancels a job request made by MakeAsyncJSONWebAPIRequest
//-----------------------------------------------------------------------------
inline void CancelAsyncJSONWebAPIRequest( uint32 requestID ) 
{
	UIEngine()->CancelAsyncJSONWebAPIRequest( requestID );
}


} // namespace panorama

#endif // UIWEBAPICLIENT_H
