//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//===========================================================================//

#ifndef DATACACHE_COMMON_H
#define DATACACHE_COMMON_H

#if defined( _WIN32 )
#pragma once
#endif

#include "tier3/tier3.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IDataCacheSection;
FORWARD_DECLARE_HANDLE( memhandle_t );
typedef memhandle_t DataCacheHandle_t;


//-----------------------------------------------------------------------------
// Console commands
//-----------------------------------------------------------------------------
extern ConVar developer;


#endif // DATACACHE_COMMON_H
