//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GC_SCHEMA_H
#define GC_SCHEMA_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "steam/steamtypes.h"

#include "tier0/dbg.h"

// include this before checksum_crc specifically to avoid the 
// CRC references
#include "steam/steamclientpublic.h"

#include "tier1/utlmap.h"
#include "tier1/utlstring.h"
#include "tier1/utlbuffer.h"
#include "tier1/mempool.h"
#include "tier1/tsmempool.h"
#include "tier1/tsmultimempool.h"
#include "tier1/fmtstr.h"

#include "vstdlib/coroutine.h"

// public stuff
#include "gamecoordinator/igcsqlresultsetlist.h"

// These are first since they're used all over
#include "gcconstants.h"
#include "refcount.h"

// SQL Access stuff
#include "sqlaccess/record.h"
#include "sqlaccess/schema.h"

#include "sqlaccess/recordinfo.h"
#include "sqlaccess/schemafull.h"




#endif