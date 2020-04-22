//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//*****************************************************************************
//
// Contents:
//
//		
//
// Authors:	Taylor Sherman
//
// Target restrictions:
//
// Tool restrictions:
//
// Things to do:
//
//		
//
//*****************************************************************************

#ifndef INCLUDED_STEAM_FINDSTEAMSERVERS_H
#define INCLUDED_STEAM_FINDSTEAMSERVERS_H


#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif

#ifdef STEAM_FINDSERVERS_STATIC_LIB

	#define STEAM_FSS_CALL
	#define STEAM_FSS_API

#else

	#ifndef STEAM_API
		#ifdef STEAM_EXPORTS
			#define STEAM_API __declspec(dllexport)
		#else
			#define STEAM_API __declspec(dllimport)
		#endif
	#endif

	#ifndef STEAM_CALL
		#define STEAM_CALL __cdecl
	#endif

	#define STEAM_FSS_CALL STEAM_CALL
	#define STEAM_FSS_API STEAM_API
#endif

#include <limits.h>
#include "steamcommon.h"

/******************************************************************************
**
** Types
**
******************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif


enum
{
	eSteamFindSteamServersLibraryError = -1,
	eSteamFindSteamServersLibraryBusy = -2
};

// returns number of IP addresses returned by the GDS for this server type
// negative return means error
STEAM_FSS_API int STEAM_FSS_CALL SteamFindServersNumServers(ESteamServerType eServerType);

// Get nth ipaddr:port for this server type
// buffer needs to be 22 chars long: aaa.bbb.ccc.ddd:12345 plus null
//
// returns 0 if succsessful, negative is error
STEAM_FSS_API int STEAM_FSS_CALL SteamFindServersIterateServer(ESteamServerType eServerType, unsigned int nServer, char *szIpAddrPort, int szIpAddrPortLen);

STEAM_FSS_API const char * STEAM_FSS_CALL SteamFindServersGetErrorString();

#ifdef __cplusplus
}
#endif

#endif /* #ifndef INCLUDED_STEAM_COMMON_STEAMCOMMON_H */
