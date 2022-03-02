//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cbase.h"

#if defined( REPLAY_ENABLED )

#include "cs_replay.h"
#include "c_cs_player.h"
#include "cs_gamestats_shared.h"
#include "cs_client_gamestats.h"
#include "clientmode_shared.h"
#include "replay/ireplaymoviemanager.h"
#include "replay/ireplayfactory.h"
#include "replay/ireplayscreenshotmanager.h"
#include "replay/screenshot.h"
#include <time.h>

//----------------------------------------------------------------------------------------

extern IReplayScreenshotManager *g_pReplayScreenshotManager;

//----------------------------------------------------------------------------------------

CCSReplay::CCSReplay()
{
}

CCSReplay::~CCSReplay()
{
}

void CCSReplay::OnBeginRecording()
{
	BaseClass::OnBeginRecording();

	// Setup the newly created replay
	C_CSPlayer* pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pPlayer )
	{
		SetPlayerClass( pPlayer->PlayerClass() );
		SetPlayerTeam( pPlayer->GetTeamNumber() );
	}
}

void CCSReplay::OnEndRecording()
{
	if ( gameeventmanager )
	{
		gameeventmanager->RemoveListener( this );
	}

	BaseClass::OnEndRecording();
}

void CCSReplay::OnComplete()
{
	BaseClass::OnComplete();
}

void CCSReplay::Update()
{
	BaseClass::Update();
}

float CCSReplay::GetSentryKillScreenshotDelay()
{
	ConVarRef replay_screenshotsentrykilldelay( "replay_screenshotsentrykilldelay" );
	return replay_screenshotsentrykilldelay.IsValid() ? replay_screenshotsentrykilldelay.GetFloat() : 0.5f;
}

void CCSReplay::FireGameEvent( IGameEvent *pEvent )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	CaptureScreenshotParams_t params;
	V_memset( &params, 0, sizeof( params ) );

	if ( FStrEq( pEvent->GetName(), "player_death" ) )
	{
		ConVarRef replay_debug( "replay_debug" );
		if ( replay_debug.IsValid() && replay_debug.GetBool() )
		{
			DevMsg( "%i: CCSReplay::FireGameEvent(): player_death\n", gpGlobals->tickcount );
		}

		int nKillerID = pEvent->GetInt( "attacker" );
		int nVictimID = pEvent->GetInt( "userid" );
		int nDeathFlags = pEvent->GetInt( "death_flags" );

		const char *pWeaponName = pEvent->GetString( "weapon" );

		// Suicide?
		bool bSuicide = nKillerID == nVictimID;

		// Try to get killer
		C_CSPlayer *pKiller = ToCSPlayer( USERID2PLAYER( nKillerID ) );

		// Try to get victim
		C_CSPlayer *pVictim = ToCSPlayer( USERID2PLAYER( nVictimID ) );

		// Inflictor was a sentry gun?
		bool bSentry = V_strnicmp( pWeaponName, "obj_sentrygun", 13 ) == 0;

		// Is the killer the local player?
		if ( nKillerID == pLocalPlayer->GetUserID() &&
			 !bSuicide &&
			 !bSentry )
		{
			// Domination?
			if ( nDeathFlags & REPLAY_DEATH_DOMINATION )
			{
				AddDomination( nVictimID );
			}

			// Revenge?
			if ( pEvent->GetInt( "death_flags" ) & REPLAY_DEATH_REVENGE ) 
			{
				AddRevenge( nVictimID );
			}

			// Add victim info to kill list
			if ( pVictim )
			{
				AddKill( pVictim->GetPlayerName(), pVictim->PlayerClass() );
			}
		
			// Take a quick screenshot with some delay
			ConVarRef replay_screenshotkilldelay( "replay_screenshotkilldelay" );
			if ( replay_screenshotkilldelay.IsValid() )
			{
				params.m_flDelay = GetKillScreenshotDelay();
				g_pReplayScreenshotManager->CaptureScreenshot( params );
			}
		}
		
		// Player death?
		else if ( pKiller &&
				  nVictimID == pLocalPlayer->GetUserID() )
		{
			// Record who killed the player if not a suicide
			if ( !bSuicide )
			{
				RecordPlayerDeath( pKiller->GetPlayerName(), pKiller->PlayerClass() );
			}

			// Take screenshot - taking a screenshot during feign death is cool, too.
			ConVarRef replay_deathcammaxverticaloffset( "replay_deathcammaxverticaloffset" );
			ConVarRef replay_playerdeathscreenshotdelay( "replay_playerdeathscreenshotdelay" );
			params.m_flDelay = replay_playerdeathscreenshotdelay.IsValid() ? replay_playerdeathscreenshotdelay.GetFloat() : 0.0f;
			params.m_nEntity = pLocalPlayer->entindex();
			params.m_posCamera.Init( 0,0, replay_deathcammaxverticaloffset.IsValid() ? replay_deathcammaxverticaloffset.GetFloat() : 150 );
			params.m_angCamera.Init( 90, 0, 0 );	// Look straight down
			params.m_bUseCameraAngles = true;
			params.m_bIgnoreMinTimeBetweenScreenshots = true;
			g_pReplayScreenshotManager->CaptureScreenshot( params );
		}
	}
}

bool CCSReplay::IsValidClass( int iClass ) const
{
	return ( iClass >= CS_CLASS_NONE && iClass < CS_NUM_CLASSES );
}

bool CCSReplay::IsValidTeam( int iTeam ) const
{
	return ( iTeam == TEAM_TERRORIST || iTeam == TEAM_CT );
}

bool CCSReplay::GetCurrentStats( RoundStats_t &out )
{
	out = g_CSClientGameStats.GetLifetimeStats();
	return true;
}

const char *CCSReplay::GetStatString( int iStat ) const
{
	return CSStatProperty_Table[ iStat ].szSteamName;
}

const char *CCSReplay::GetPlayerClass( int iClass ) const
{
	Assert( iClass >= CS_CLASS_NONE && iClass < CS_NUM_CLASSES );
	return GetCSClassInfo( iClass )->m_pClassName;
}

bool CCSReplay::Read( KeyValues *pIn )
{
	return BaseClass::Read( pIn );
}

void CCSReplay::Write( KeyValues *pOut )
{
	BaseClass::Write( pOut );
}

const char *CCSReplay::GetMaterialFriendlyPlayerClass() const
{
	return BaseClass::GetMaterialFriendlyPlayerClass();
}

void CCSReplay::DumpGameSpecificData() const
{
	BaseClass::DumpGameSpecificData();
}

//----------------------------------------------------------------------------------------

class CCSReplayFactory : public IReplayFactory
{
public:
	virtual CReplay *Create()
	{
		 return new CCSReplay();
	}
};

static CCSReplayFactory s_ReplayManager;
IReplayFactory *g_pReplayFactory = &s_ReplayManager;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CCSReplayFactory, IReplayFactory, INTERFACE_VERSION_REPLAY_FACTORY, s_ReplayManager );

#endif