//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "hltvdirector.h"

class CDODHLTVDirector : public CHLTVDirector
{
public:
	DECLARE_CLASS( CDODHLTVDirector, CHLTVDirector );

	const char** GetModEvents();
	void SetHLTVServer( IHLTVServer *hltv );
	void CreateShotFromEvent( CHLTVGameEvent *event );
};

void CDODHLTVDirector::SetHLTVServer( IHLTVServer *hltv )
{
	BaseClass::SetHLTVServer( hltv );

	if ( m_pHLTVServer )
	{
		// mod specific events the director uses to find interesting shots
		ListenForGameEvent( "dod_point_captured" );
		ListenForGameEvent( "dod_capture_blocked" );
	}
}

void CDODHLTVDirector::CreateShotFromEvent( CHLTVGameEvent *event )
{
	// show event at least for 2 more seconds after it occured
	const char *name = event->m_Event->GetName();
	IGameEvent *shot = NULL;

	if ( !Q_strcmp( "dod_point_captured", name ) ||
		 !Q_strcmp( "dod_capture_blocked", name ) )
	{
		// try to find a capper or blocker
		const char *text = event->m_Event->GetString("blocker");
		int playerIndex = text[0];

		if ( playerIndex < 1 )
		{
			// maybe its a capper ?
			text = event->m_Event->GetString("cappers");
			playerIndex = text[0];
		}

		// if we found one, show him
		if ( playerIndex > 0 )
		{
            // shot player as primary, hostage as secondary target
			shot = gameeventmanager->CreateEvent( "hltv_chase", true );
			shot->SetInt( "target1", playerIndex );
			shot->SetInt( "target2", 0 );
			shot->SetFloat( "distance", 96.0f );
			shot->SetInt( "theta", 0 );
			shot->SetInt( "phi", 20 );

			// shot 2 seconds after event
			m_nNextShotTick = MIN( m_nNextShotTick, (event->m_Tick+TIME_TO_TICKS(2.0)) );
			m_iPVSEntity = playerIndex;
		}
	}
	else
	{
		// let baseclass create a shot
		BaseClass::CreateShotFromEvent( event );

		return;
	}

	if ( shot )
	{
		m_pHLTVServer->BroadcastEvent( shot );
		gameeventmanager->FreeEvent( shot );
		DevMsg("DrcCmd: %s\n", name );
	}
}

const char** CDODHLTVDirector::GetModEvents()
{
	// game events relayed to spectator clients
	static const char *s_modevents[] =
	{
		"hltv_status",
		"hltv_chat",
		"player_connect",
		"player_disconnect",
		"player_team",
		"player_info",
		"server_cvar",
		"player_death",
		"player_chat",
		"round_start",
		"round_end",
		// additional DoD:S events:
		"player_changeclass",
		"dod_warmup_begins",
		"dod_warmup_ends",
		"dod_round_start",
		"dod_restart_round",
		"dod_ready_restart",
		"dod_allies_ready",
		"dod_axis_ready",
		"dod_round_restart_seconds",
		"dod_team_scores",
		"dod_round_win",
		"dod_tick_points",
		"dod_game_over",
		"dod_broadcast_audio",
		"dod_point_captured",
		"dod_capture_blocked",
		"dod_top_cappers",
		"dod_timer_flash",
		"dod_bomb_planted",
		NULL
	};

	return s_modevents;
}

static CDODHLTVDirector s_HLTVDirector;	// singleton

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CHLTVDirector, IHLTVDirector, INTERFACEVERSION_HLTVDIRECTOR, s_HLTVDirector );

CHLTVDirector* HLTVDirector()
{
	return &s_HLTVDirector;
}

IGameSystem* HLTVDirectorSystem()
{
	return &s_HLTVDirector;
}