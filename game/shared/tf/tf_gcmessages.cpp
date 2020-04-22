//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Provides names for GC message types for TF
//
//=============================================================================


#include "cbase.h"
#include "gcsdk/gcsdk_auto.h"
#include "tf_gcmessages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

uint32 GetKickBanPlayerReason( const char *pReasonString )
{
	if ( Q_strncmp( pReasonString, "other", 5 ) == 0 )
	{
		return kVoteKickBanPlayerReason_Other;
	}
	else if ( Q_strncmp( pReasonString, "cheating", 8 ) == 0 )
	{
		return kVoteKickBanPlayerReason_Cheating;
	}
	else if ( Q_strncmp( pReasonString, "idle", 4 ) == 0 )
	{
		return kVoteKickBanPlayerReason_Idle;
	}
	else if ( Q_strncmp( pReasonString, "scamming", 8 ) == 0 )
	{
		return kVoteKickBanPlayerReason_Scamming;
	}
	return kVoteKickBanPlayerReason_Other;
}
