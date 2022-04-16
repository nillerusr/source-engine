//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Provides names for GC message types for Portal
//
//=============================================================================

#include "cbase.h"

#ifdef USE_GC_IN_PORTAL1
#include "gcsdk/gcsdk.h"
#include "portal_gcmessages.h"

//-----------------------------------------------------------------------------
// Purpose: A big array of message types for keeping track of their names
//-----------------------------------------------------------------------------
GCSDK::MsgInfo_t g_MsgInfo[] =
{
	DECLARE_GC_MSG( k_EMsgGCReportWarKill ),

	DECLARE_GC_MSG( k_EMsgGCDev_GrantWarKill ),
};

void InitGCPortalMessageTypes()
{
	static GCSDK::CMessageListRegistration m_reg( g_MsgInfo, Q_ARRAYSIZE(g_MsgInfo) );
}

#endif //#ifdef USE_GC_IN_PORTAL1