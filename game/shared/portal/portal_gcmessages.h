//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This file defines all of our over-the-wire net protocols for the
//			Game Coordinator for Portal.  Note that we never use types
//			with undefined length (like int).  Always use an explicit type 
//			(like int32).
//
//=============================================================================

#ifndef PORTAL_GCMESSAGES_H
#define PORTAL_GCMESSAGES_H
#ifdef _WIN32
#pragma once
#endif


enum EGCMsg
{
	k_EMsgGCPortalBase =								5000,
	k_EMsgGCReportWarKill =						k_EMsgGCPortalBase + 1, //War kill tracking. No longer in use

	// Development only messages
	k_EMsgGCPortalDEVBase =							6000,
	k_EMsgGCDev_GrantWarKill =					k_EMsgGCPortalDEVBase + 1, //War kill tracking. No longer in use
};


#endif
