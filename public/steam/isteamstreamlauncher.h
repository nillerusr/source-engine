//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: interface to streaming launcher functions in Steam
//
//=============================================================================

#ifndef ISTEAMSTREAMLAUNCHER_H
#define ISTEAMSTREAMLAUNCHER_H
#ifdef _WIN32
#pragma once
#endif

#include "isteamclient.h"

enum EStreamLauncherResult
{
	k_EStreamLaunchResultSuccess			= 0,
	k_EStreamLaunchResultFailure			= 1,	// Unknown streaming launch failure
	k_EStreamLaunchResultAlreadyStreaming	= 2,	// The UI is already streaming to a different application
	k_EStreamLaunchResultInvalidLauncher	= 3,	// The streaming launcher doesn't exist, or failed signature check
	k_EStreamLaunchResultNotReady			= 4,	// The UI isn't ready to go into streaming mode (Desktop UI login?)
};

//-----------------------------------------------------------------------------
// Purpose: interface to streaming launcher functions in Steam
//-----------------------------------------------------------------------------
class ISteamStreamLauncher
{
public:
	// Switch Steam to Big Picture mode and optionally set a launcher to run all games
	virtual EStreamLauncherResult StartStreaming( const char *pchLauncher = NULL ) = 0;

	// Switch Steam back to the original mode and clear the launcher
	virtual void StopStreaming() = 0;
};

#define STEAMSTREAMLAUNCHER_INTERFACE_VERSION "SteamStreamLauncher001"


// callbacks
#if defined( VALVE_CALLBACK_PACK_SMALL )
#pragma pack( push, 4 )
#elif defined( VALVE_CALLBACK_PACK_LARGE )
#pragma pack( push, 8 )
#else
#error isteamclient.h must be included
#endif 

//-----------------------------------------------------------------------------
// The big picture window is visible and ready for streaming
//-----------------------------------------------------------------------------
struct BigPictureStreamingResult_t
{
	enum { k_iCallback = k_iSteamStreamLauncherCallbacks + 1 };
	bool m_bSuccess;
	void *m_hwnd;
};

//-----------------------------------------------------------------------------
// The user requested that the application stop streaming
//-----------------------------------------------------------------------------
struct StopStreamingRequest_t
{
	enum { k_iCallback = k_iSteamStreamLauncherCallbacks + 2 };
};

//-----------------------------------------------------------------------------
// The big picture window is closed and no longer ready for streaming
//-----------------------------------------------------------------------------
struct BigPictureStreamingDone_t
{
	enum { k_iCallback = k_iSteamStreamLauncherCallbacks + 3 };
};

//-----------------------------------------------------------------------------
// Steam is about to restart, continue streaming when it is available again
//-----------------------------------------------------------------------------
struct BigPictureStreamRestarting_t
{
	enum { k_iCallback = k_iSteamStreamLauncherCallbacks + 4 };
};

#pragma pack( pop )

#endif // ISTEAMSTREAMLAUNCHER_H
