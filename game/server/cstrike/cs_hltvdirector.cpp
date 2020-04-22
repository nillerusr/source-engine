//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "hltvdirector.h"
#include "igameevents.h"

class CCSHLTVDirector : public CHLTVDirector
{
public:
	DECLARE_CLASS( CCSHLTVDirector, CHLTVDirector );

	const char** GetModEvents();
	void SetHLTVServer( IHLTVServer *hltv );
	void CreateShotFromEvent( CHLTVGameEvent *event );

};

void CCSHLTVDirector::SetHLTVServer( IHLTVServer *hltv )
{
	BaseClass::SetHLTVServer( hltv );

	if ( m_pHLTVServer )
	{
		// mod specific events the director uses to find interesting shots
		ListenForGameEvent( "hostage_rescued" );
		ListenForGameEvent( "hostage_killed" );
		ListenForGameEvent( "hostage_hurt" );
		ListenForGameEvent( "hostage_follows" );
		ListenForGameEvent( "bomb_pickup" );
		ListenForGameEvent( "bomb_dropped" );
		ListenForGameEvent( "bomb_exploded" );
		ListenForGameEvent( "bomb_defused" );
		ListenForGameEvent( "bomb_planted" );
		ListenForGameEvent( "vip_escaped" );
		ListenForGameEvent( "vip_killed" );
	}
}

void CCSHLTVDirector::CreateShotFromEvent( CHLTVGameEvent *event )
{
	// show event at least for 2 more seconds after it occured
	const char *name = event->m_Event->GetName();
	IGameEvent *shot = NULL;

	if ( !Q_strcmp( "hostage_rescued", name ) ||
		 !Q_strcmp( "hostage_hurt", name ) ||
		 !Q_strcmp( "hostage_follows", name ) ||
		 !Q_strcmp( "hostage_killed", name ) )
	{
		CBaseEntity *player = UTIL_PlayerByUserId( event->m_Event->GetInt("userid") );
		
		if ( !player )
			return;

		// shot player as primary, hostage as secondary target
		shot = gameeventmanager->CreateEvent( "hltv_chase", true );
		shot->SetInt( "target1", player->entindex() );
		shot->SetInt( "target2", event->m_Event->GetInt("hostage") );
		shot->SetFloat( "distance", 96.0f );
		shot->SetInt( "theta", 40 );
		shot->SetInt( "phi", 20 );

		// shot 2 seconds after event
		m_nNextShotTick = MIN( m_nNextShotTick, (event->m_Tick+TIME_TO_TICKS(2.0)) );
		m_iPVSEntity = player->entindex();
	}

	else if ( !Q_strcmp( "bomb_pickup", name ) ||
		!Q_strcmp( "bomb_dropped", name ) ||
		!Q_strcmp( "bomb_planted", name ) ||
		!Q_strcmp( "bomb_defused", name ) )
	{
		CBaseEntity *player = UTIL_PlayerByUserId( event->m_Event->GetInt("userid") );

		if ( !player )
			return;

		shot = gameeventmanager->CreateEvent( "hltv_chase", true );
		shot->SetInt( "target1", player->entindex() );
		shot->SetInt( "target2", 0 );
		shot->SetFloat( "distance", 64.0f );
		shot->SetInt( "theta", 200 );
		shot->SetInt( "phi", 10 );

		// shot 2 seconds after pickup
		m_nNextShotTick = MIN( m_nNextShotTick, (event->m_Tick+TIME_TO_TICKS(2.0)) );
		m_iPVSEntity = player->entindex();
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

const char** CCSHLTVDirector::GetModEvents()
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
		// additional CS:S events:
		"bomb_planted",	
		"bomb_defused",
		"hostage_killed",
		"hostage_hurt",		
		NULL
	};

	return s_modevents;
}

static CCSHLTVDirector s_HLTVDirector;	// singleton

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CHLTVDirector, IHLTVDirector, INTERFACEVERSION_HLTVDIRECTOR, s_HLTVDirector );

CHLTVDirector* HLTVDirector()
{
	return &s_HLTVDirector;
}

IGameSystem* HLTVDirectorSystem()
{
	return &s_HLTVDirector;
}