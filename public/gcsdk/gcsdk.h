//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: includes all the headers required for the GC SDK. Include this
//			in your stdafx.h
//
//=============================================================================

#ifndef GCSDK_H
#define GCSDK_H
#ifdef _WIN32
#pragma once
#endif

#if defined(_WIN32) || defined(_WIN64)
#pragma once
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#endif

#include "tier0/platform.h"

#include "gcsteamdefines.h"

#include "steam/steamtypes.h"

#include "tier0/dbg.h"
#ifdef DBGFLAG_VALIDATE
#include "tier0/validator.h"
#endif
#include "tier0/vprof.h"
#include "tier0/fasttimer.h"

// include this before checksum_crc specifically to avoid the 
// CRC references
#include "steam/steamclientpublic.h"

#include "tier1/utlmap.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlpriorityqueue.h"
#include "tier1/utlstring.h"
#include "tier1/utlbuffer.h"
#include "tier1/utldict.h"
#include "tier1/utlhashmaplarge.h"
#include "tier1/mempool.h"
#include "tier1/tsmempool.h"
#include "tier1/tsmultimempool.h"
#include "tier1/checksum_crc.h"
#include "tier1/fmtstr.h"
#include "tier1/KeyValues.h"
#include "tier1/strtools.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlsymbollarge.h"

#include "vstdlib/coroutine.h"
#include "vstdlib/osversion.h"

// public stuff
#include "gamecoordinator/igcsqlresultsetlist.h"
#include "misc.h"

// These are first since they're used all over
#include "gcconstants.h"
#include "refcount.h"
#include "netpacket.h"
#include "gcmsg.h"
#include "msgprotobuf.h"
#include "gc_convar.h"

// SQL Access stuff
#include "sqlaccess/record.h"
#include "sqlaccess/schema.h"
#include "sqlaccess/recordinfo.h"
#include "sqlaccess/schemafull.h"
#include "sqlaccess/columnset.h"
#include "sqlaccess/sqlrecord.h"
#include "sqlaccess/sqlutil.h"
#include "sqlaccess/sqlaccess.h"

#include "messagelist.h"
#include "gchost.h"
#include "gclogger.h"
#include "gcsqlquery.h"
#include "jobtime.h"
#include "job.h"
#include "jobmgr.h"
#include "netpacketpool.h"
#include "gcsystemmsgs.h"
#include "gcwgjobmgr.h"
#include "gcbase.h"
#include "gcsession.h"
#include "sharedobject.h"
#include "protobufsharedobject.h"
#include "schemasharedobject.h"
#include "sharedobjectcache.h"
#include "gcdirtyfield.h"
#include "gc_sharedobjectcache.h"
#include "http.h"
#include "gcwebapi.h"
#include "gcwebapikey.h"
#include "webapi_response.h"
#include "gcjob.h"
#include "msgprotobuf.h"
#include "sdocache.h"

#endif // GCSDK_H
