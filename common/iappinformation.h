//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef IAPPINFORMATION_H
#define IAPPINFORMATION_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"


//-----------------------------------------------------------------------------
// Purpose: Interface to running the game engine
//-----------------------------------------------------------------------------
abstract_class IAppInformation : public IBaseInterface
{
public:
	// the number of apps known about by the app
	virtual int GetAppCount() = 0;

	// details about this particular app
	// iApp is from [ 0 .. GetAppCount() )
	virtual const char *GetAppName( int iApp ) = 0;
	virtual const char *GetAppServerBrowserName( int iApp ) = 0;
	virtual const char *GetAppGameDir( int iApp ) = 0;
	virtual bool GetAppHasServers( int iApp ) = 0;
	virtual uint32 GetAppID( int iApp ) = 0;
	virtual bool GetAppIsSubscribed( int iApp ) = 0;
	virtual const char *GetAppIcon( int iApp ) = 0;
	// converting appID->iApp (needed since this whole interface is backwards)
	virtual int GetIAppForAppID( uint32 unAppID ) = 0;
};

#define APPINFORMATION_INTERFACE_VERSION "AppInformation002"


#endif // IAPPINFORMATION_H
