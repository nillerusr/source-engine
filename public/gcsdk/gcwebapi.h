//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CWAPI header for GC access to the Web API server
//
//=============================================================================

#ifndef GCWEBAPI_H
#define GCWEBAPI_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/memdbgon.h"

enum EWebAPIPrivilege
{
	k_EWebApiPriv_Invalid				= -1,
	k_EWebApiPriv_None					= 0,			// fully public, no auth needed
	k_EWebApiPriv_Key					= 1,			// Requires valid key
	k_EWebApiPriv_PublisherKey			= 2,			// Requires publisher key
	k_EWebApiPriv_PublisherKeyOwnsApp	= 3,			// Requires publisher key and publisher owns appid
	//k_EWebApiPriv_Account	= 1,						// user must have a Steam account with password set
};

enum EWebApiParamType
{
	k_EWebApiParamTypeInvalid	= -1,

	k_EWebApiParamTypeInt32		= 0,
	k_EWebApiParamTypeUInt32	= 1,
	k_EWebApiParamTypeInt64		= 2,
	k_EWebApiParamTypeUInt64	= 3,
	k_EWebApiParamTypeFloat		= 4,
	k_EWebApiParamTypeString	= 5,
	k_EWebApiParamTypeBool		= 6,
	k_EWebApiParamTypeRawBinary = 7,
};

const char *PchNameFromEWebApiParamType( int eWebApiParamType );

typedef KeyValues *(*GCWebAPIInterfaceMapCreationFunc_t)();

class CGCWebAPIInterfaceMapRegistrar
{
public:
	CGCWebAPIInterfaceMapRegistrar( GCWebAPIInterfaceMapCreationFunc_t pFunc )
	{
		VecInstance().AddToTail( pFunc );
	}

	static CUtlVector< GCWebAPIInterfaceMapCreationFunc_t > & VecInstance();
};


//
// Macros for use registering interfaces in webapi_interfacemap.h
//
#define BEGIN_GCWEB_INTERFACE_BLOCK( pchInterfaceName ) \
	KeyValues *CreateWebAPIInterfaceMap_##pchInterfaceName() \
{ \
	KeyValues *pkvInterface = new KeyValues( #pchInterfaceName ); 

#define DECLARE_GCWEBAPI_METHOD( pchMethodName, unVersion, eHTTPMethod, pchJobName, ePriv ) \
{ \
	KeyValues *pkvMethod = pkvInterface->FindKey( pchMethodName #unVersion, true ); \
	pkvMethod->SetString( "name", pchMethodName ); \
	pkvMethod->SetInt( "version", unVersion ); \
	pkvMethod->SetInt( "http_method", eHTTPMethod ); \
	pkvMethod->SetString( "job_name", pchJobName); \
	pkvMethod->SetInt( "priv", ePriv ); 

#define REQUIRED_GCWEBAPI_PARAM( pchName, eType, pchDescription ) \
{ \
	KeyValues *pkvParams = pkvMethod->FindKey( "params", true ); \
	AssertMsg( Q_stricmp( pchName, "format" ) != 0, "'format' is a magic reserved API param for specifying output format!" ); \
	KeyValues *pkvParam = pkvParams->FindKey( pchName, true ); \
	pkvParam->SetString( "description", pchDescription ); \
	pkvParam->SetInt( "type", eType ); \
	pkvParam->SetInt( "optional", 0 ); \
}

#define OPTIONAL_GCWEBAPI_PARAM( pchName, eType, pchDescription ) \
{ \
	KeyValues *pkvParams = pkvMethod->FindKey( "params", true ); \
	AssertMsg( Q_stricmp( pchName, "format" ) != 0, "'format' is a magic reserved API param for specifying output format!" ); \
	KeyValues *pkvParam = pkvParams->FindKey( pchName, true ); \
	pkvParam->SetString( "description", pchDescription ); \
	pkvParam->SetInt( "type", eType ); \
	pkvParam->SetInt( "optional", 1 ); \
}

#define END_GCWEBAPI_METHOD() \
} 


#define END_GCWEB_INTERFACE_BLOCK( pchInterfaceName ) \
	return pkvInterface; \
} \
	CGCWebAPIInterfaceMapRegistrar g_Register_GCWebAPIInterfaceMapCreator_##pchInterfaceName( &CreateWebAPIInterfaceMap_##pchInterfaceName );

#include "tier0/memdbgoff.h"

#endif // GCWEBAPI_H
