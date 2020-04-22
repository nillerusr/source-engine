//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replaysystem.h"
#include "cl_replaymanager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

void OnReplayEnableChanged( IConVar *pVar, const char *pOldValue, float flOldValue );
void OnReplayRecordingChanged( IConVar *pVar, const char *pOldValue, float flOldValue );

//----------------------------------------------------------------------------------------

// Replicated
ConVar replay_enable( "replay_enable", "0", FCVAR_REPLICATED | FCVAR_DONTRECORD, "Enable Replay recording on server", true, 0, true, 1, OnReplayEnableChanged );
ConVar replay_recording( "replay_recording", "0", FCVAR_REPLICATED | FCVAR_DONTRECORD | FCVAR_HIDDEN, "", true, 0, true, 1, OnReplayRecordingChanged );

ConVar replay_flushinterval( "replay_flushinterval", "15", FCVAR_DONTRECORD | FCVAR_ARCHIVE, "Replay system will flush to disk a maximum of every replay_flushinterval seconds.", true, 1.0f, true, 60.0f );

//----------------------------------------------------------------------------------------

//
// A little class to keep OnReplayEnableChanged() from recursing unnecessarily
//
class CSimpleCounter
{
public:
	CSimpleCounter() { ++m_nCounter; }
	~CSimpleCounter() { --m_nCounter; }

	int GetCounter() const { return m_nCounter; }

private:
	static int m_nCounter;
};

int CSimpleCounter::m_nCounter = 0;

//----------------------------------------------------------------------------------------

void OnReplayEnableChanged( IConVar *pVar, const char *pOldValue, float flOldValue )
{
	// We want to avoid recursing when we SetValue() on replay_enable (ie 'var')
	CSimpleCounter counter;
	if ( counter.GetCounter() != 1 )
		return;

	if ( !g_pEngine->IsDedicated() )
		return;

	ConVarRef var( pVar );
	if ( (int)flOldValue == var.GetInt() )
		return;

	/*
	ConVarRef tv_enable( "tv_enable" );
	if ( var.GetBool() && tv_enable.IsValid() && tv_enable.GetBool() )
	{
		var.SetValue( 0 );
		Warning( "Error: SourceTV is enabled.  Please disable SourceTV if you wish to enable Replay.\n" );
		return;
	}
	*/

	const int nNewValue = var.GetInt();
	if ( nNewValue )
	{
		g_pServerReplayContext->FlagForConVarSanityCheck();
	}
	else
	{
		// Reset value - note that the recursion depth counter will keep this from being dumb.
		var.SetValue( 0 );

		// End recording, which will clear the value again.
		g_pReplay->SV_EndRecordingSession( false );
	}

	g_pEngine->RecalculateTags();
}

void OnReplayRecordingChanged( IConVar *pVar, const char *pOldValue, float flOldValue )
{
	if ( g_pEngine->IsDedicated() )
		return;

#if !defined( DEDICATED )
	// If we're playing back a replay, we don't care
	if ( g_pEngineClient->IsPlayingReplayDemo() )
		return;

	// Client-only
	CL_GetReplayManager()->OnReplayRecordingCvarChanged();
#endif
}
