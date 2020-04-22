//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "cbase.h"
#include "hud.h"
#include "clientmode_dod.h"
#include "cdll_client_int.h"
#include "iinput.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include <vgui_controls/AnimationController.h>
#include "ivmodemanager.h"
#include "buymenu.h"
#include "filesystem.h"
#include "vgui/IVGui.h"
#include "hud_basechat.h"
#include "view_shared.h"
#include "view.h"
#include "ivrenderview.h"
#include "model_types.h"
#include "iefx.h"
#include "dlight.h"
#include <imapoverview.h>
#include "c_playerresource.h"
#include <KeyValues.h>
#include "text_message.h"
#include "panelmetaclassmgr.h"
#include "dod_shareddefs.h"
#include "c_dod_player.h"
#include "physpropclientside.h"
#include "engine/IEngineSound.h"
#include "bitbuf.h"
#include "usermessages.h"
#include "prediction.h"
#include "vgui/ILocalize.h"
#include "dod_hud_freezepanel.h"
#include "dod_hud_chat.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHudChat;

ConVar default_fov( "default_fov", "90", FCVAR_CHEAT );

ConVar dod_playwinmusic( "dod_playwinmusic", "1", FCVAR_ARCHIVE );

IClientMode *g_pClientMode = NULL;

void MsgFunc_KillCam(bf_read &msg) 
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( !pPlayer )
		return;

	int newMode = msg.ReadByte();

	if ( newMode != g_nKillCamMode )
	{
#if !defined( NO_ENTITY_PREDICTION )
		if ( g_nKillCamMode == OBS_MODE_NONE )
		{
			// kill cam is switch on, turn off prediction
			g_bForceCLPredictOff = true;
		}
		else if ( newMode == OBS_MODE_NONE )
		{
			// kill cam is switched off, restore old prediction setting is we switch back to normal mode
			g_bForceCLPredictOff = false;
		}
#endif
		g_nKillCamMode = newMode;
	}

	g_nKillCamTarget1	= msg.ReadByte();
	g_nKillCamTarget2	= msg.ReadByte();
}

// --------------------------------------------------------------------------------- //
// CDODModeManager.
// --------------------------------------------------------------------------------- //

class CDODModeManager : public IVModeManager
{
public:
	virtual void	Init();
	virtual void	SwitchMode( bool commander, bool force ) {}
	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );
	virtual void	ActivateMouse( bool isactive ) {}
};

static CDODModeManager g_ModeManager;
IVModeManager *modemanager = ( IVModeManager * )&g_ModeManager;

// --------------------------------------------------------------------------------- //
// CDODModeManager implementation.
// --------------------------------------------------------------------------------- //

#define SCREEN_FILE		"scripts/vgui_screens.txt"

void CDODModeManager::Init()
{
	g_pClientMode = GetClientModeNormal();
	
	PanelMetaClassMgr()->LoadMetaClassDefinitionFile( SCREEN_FILE );
}

void CDODModeManager::LevelInit( const char *newmap )
{
	g_pClientMode->LevelInit( newmap );

#if !defined( NO_ENTITY_PREDICTION )
	if ( g_nKillCamMode > OBS_MODE_NONE )
	{
		g_bForceCLPredictOff = false;
	}
#endif

	g_nKillCamMode		= OBS_MODE_NONE;
	g_nKillCamTarget1	= 0;
	g_nKillCamTarget2	= 0;
}

void CDODModeManager::LevelShutdown( void )
{
	g_pClientMode->LevelShutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeDODNormal::ClientModeDODNormal()
{
	m_pFreezePanel = NULL;
}

void ClientModeDODNormal::Init()
{
	BaseClass::Init();

	ListenForGameEvent( "dod_round_start" );
	ListenForGameEvent( "dod_broadcast_audio" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "dod_bomb_planted" );
	ListenForGameEvent( "dod_bomb_defused" );
	ListenForGameEvent( "dod_timer_flash" );

	usermessages->HookMessage( "KillCam", MsgFunc_KillCam );

	m_szLastRadioSound[0] = '\0';

	m_pFreezePanel = ( CDODFreezePanel * )GET_HUDELEMENT( CDODFreezePanel );
	Assert( m_pFreezePanel );
}
void ClientModeDODNormal::InitViewport()
{
	m_pViewport = new DODViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}

ClientModeDODNormal g_ClientModeNormal;

IClientMode *GetClientModeNormal()
{
	return &g_ClientModeNormal;
}


ClientModeDODNormal* GetClientModeDODNormal()
{
	Assert( dynamic_cast< ClientModeDODNormal* >( GetClientModeNormal() ) );

	return static_cast< ClientModeDODNormal* >( GetClientModeNormal() );
}

ConVar r_viewmodelfov( "r_viewmodelfov", "0", FCVAR_CHEAT );

float ClientModeDODNormal::GetViewModelFOV( void )
{
	float flFov = 90.0f;

	if( r_viewmodelfov.GetFloat() > 0 )
		return r_viewmodelfov.GetFloat();

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if( !pPlayer )
		return flFov;

	CWeaponDODBase *pWpn = pPlayer->GetActiveDODWeapon();

	if( pWpn )
	{
		flFov = pWpn->GetDODWpnData().m_flViewModelFOV;
	}

	return flFov;
}

int ClientModeDODNormal::GetDeathMessageStartHeight( void )
{
	return m_pViewport->GetDeathMessageStartHeight();
}

void ClientModeDODNormal::FireGameEvent( IGameEvent * event)
{
	const char *eventname = event->GetName();

	if ( !eventname || !eventname[0] )
		return;

	if ( Q_strcmp( "dod_round_start", eventname ) == 0 )
	{
		// Just tell engine to clear decals
		engine->ClientCmd( "r_cleardecals\n" );

		// recreate all client side physics props
		// check for physenv, because we sometimes crash on changelevel
		// if we get this message before fully connecting
		if ( physenv )
		{
            C_PhysPropClientside::RecreateAll();
		}
	}
	else if( Q_strcmp( "dod_broadcast_audio", eventname ) == 0 )
	{
		CLocalPlayerFilter filter;
		const char *pszSoundName = event->GetString("sound");

		if ( dod_playwinmusic.GetBool() == false )
		{
			if ( FStrEq( pszSoundName, "Game.USWin" ) || FStrEq( pszSoundName, "Game.GermanWin" ) )
			{
				return;
			}
		}
		C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, pszSoundName );
	}
	else if ( Q_strcmp( "dod_bomb_planted", eventname ) == 0 )
	{
		int defendingTeam = event->GetInt( "team" );

		C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();

		if ( !pLocalPlayer )
			return;

		const char *pszSound = "";
		const char *pszMessage = "";

		int localTeam = pLocalPlayer->GetTeamNumber();

		const char *pPlanterName = NULL;

		int iPlanterIndex = 0;

		if ( defendingTeam == localTeam )
		{
			// play defend sound
			switch( localTeam )
			{
			case TEAM_ALLIES:
				{
					pszSound = "Voice.US_C4EnemyPlanted";
					pszMessage = "#dod_bomb_us_enemy_planted";
				}
				break;
			case TEAM_AXIS:
				{
					pszSound = "Voice.German_C4EnemyPlanted";
					pszMessage = "#dod_bomb_ger_enemy_planted";
				}
				break;
			default:
				break;
			}
		}
		else
		{
			// play planting sound
			switch( localTeam )
			{
			case TEAM_ALLIES:
				{
					pszSound = "Voice.US_C4TeamPlanted";
					pszMessage = "#dod_bomb_us_team_planted";
				}
				break;
			case TEAM_AXIS:
				{
					pszSound = "Voice.German_C4TeamPlanted";
					pszMessage = "#dod_bomb_ger_team_planted";
				}
				break;
			default:
				break;
			}

			// Only show the planter name if its a team plant, not enemy plant
			iPlanterIndex = engine->GetPlayerForUserID( event->GetInt("userid") );
			pPlanterName = g_PR->GetPlayerName( iPlanterIndex );
		}		

		RadioMessage( pszSound, pszMessage, pPlanterName, iPlanterIndex );
	}
	else if ( Q_strcmp( "dod_bomb_defused", eventname ) == 0 )
	{
		int defusingTeam = event->GetInt( "team" );

		C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();

		if ( !pLocalPlayer )
			return;

		const char *pszSound = "";
		const char *pszMessage = "";

		int localTeam = pLocalPlayer->GetTeamNumber();

		if ( defusingTeam == localTeam )
		{
			// play defused sound
			switch( localTeam )
			{
			case TEAM_ALLIES:
				{
					pszSound = "Voice.US_C4Defused";
					pszMessage = "#dod_bomb_us_defused";
				}
				break;
			case TEAM_AXIS:
				{
					pszSound = "Voice.German_C4Defused";
					pszMessage = "#dod_bomb_ger_defused";
				}
				break;
			default:
				break;
			}

			int iDefuser = engine->GetPlayerForUserID( event->GetInt("userid") );
			const char *pDefuserName = g_PR->GetPlayerName( iDefuser );

			RadioMessage( pszSound, pszMessage, pDefuserName, iDefuser );
		}
	}
	else if ( Q_strcmp( "player_team", eventname ) == 0 )
	{
		C_BasePlayer *pPlayer = USERID2PLAYER( event->GetInt("userid") );

		if ( !pPlayer )
			return;

		bool bDisconnected = event->GetBool("disconnect");

		if ( bDisconnected )
			return;

		int team = event->GetInt( "team" );

		if ( pPlayer->IsLocalPlayer() )
		{
			// that's me
			pPlayer->TeamChange( team );
		}

		CBaseHudChat *hudChat = (CBaseHudChat *)GET_HUDELEMENT( CHudChat );

		if ( !hudChat )
			return;

		const char *pTemplate = NULL;

		if ( team == TEAM_ALLIES )
		{
			pTemplate = "#game_joined_allies";
		}
		else if ( team == TEAM_AXIS )
		{
			pTemplate = "#game_joined_axis";
		}
		else
		{
			pTemplate = "#game_joined_spectators";
		} 

		wchar_t szPlayerName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(), szPlayerName, sizeof(szPlayerName) );

		wchar_t wszPrint[128];
		char szPrint[128];

		g_pVGuiLocalize->ConstructString( wszPrint, sizeof(wszPrint), g_pVGuiLocalize->Find(pTemplate), 1, szPlayerName );
		g_pVGuiLocalize->ConvertUnicodeToANSI( wszPrint, szPrint, sizeof(szPrint) );

		hudChat->Printf( CHAT_FILTER_TEAMCHANGE, "%s",szPrint );
	}
	else if ( Q_strcmp( "dod_timer_flash", eventname ) == 0 )
	{
		C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();

		if ( !pLocalPlayer )
			return;

		const char *pszSound = "";
		const char *pszMessage = "";

		int localTeam = pLocalPlayer->GetTeamNumber();

		int iTimeRemaining = event->GetInt( "time_remaining", 0 );

		switch( iTimeRemaining )
		{
		case 60:
			switch( localTeam )
			{
			case TEAM_ALLIES:
				{
					pszSound = "Voice.US_OneMinute";
					pszMessage = "#dod_time_remaining_us_1_min";
				}
				break;
			case TEAM_AXIS:
				{
					pszSound = "Voice.German_OneMinute";
					pszMessage = "#dod_time_remaining_ger_1_min";
				}
				break;
			default:
				break;
			}
			break;
		case 120:
			switch( localTeam )
			{
			case TEAM_ALLIES:
				{
					pszSound = "Voice.US_TwoMinute";
					pszMessage = "#dod_time_remaining_us_2_min";
				}
				break;
			case TEAM_AXIS:
				{
					pszSound = "Voice.German_TwoMinute";
					pszMessage = "#dod_time_remaining_ger_2_min";
				}
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}

		RadioMessage( pszSound, pszMessage );
	}
	else
		BaseClass::FireGameEvent( event );
}


void ClientModeDODNormal::PostRenderVGui()
{
}

bool ClientModeDODNormal::ShouldDrawViewModel()
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	
	CWeaponDODBase *pWpn = pPlayer->GetActiveDODWeapon();

	if( pWpn && pWpn->ShouldDrawViewModel() == false )
	{
		return false;
	}

	return BaseClass::ShouldDrawViewModel();
}

void ClientModeDODNormal::RadioMessage( const char *pszSoundName, const char *pszSubtitle, const char *pszSender /* = NULL */, int iSenderIndex /* = 0 */ )
{
	C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( !pLocalPlayer )
	{
		return;
	}

	int color = COLOR_PLAYERNAME;

	// stop the last played radio message
	if ( Q_strlen( m_szLastRadioSound ) > 0 )
	{
		C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();
		if ( pLocalPlayer )
		{
			pLocalPlayer->StopSound( m_szLastRadioSound );
		}
	}

	Q_strncpy( m_szLastRadioSound, pszSoundName, sizeof(m_szLastRadioSound) );

	// Play the radio alert
	char szCmd[128];
	Q_snprintf( szCmd, sizeof(szCmd), "playgamesound %s", pszSoundName );

	engine->ClientCmd( szCmd );

	// Print a message to chat
	wchar_t wszPrint[128];
	char szPrint[128];

	g_pVGuiLocalize->ConstructString( wszPrint, sizeof(wszPrint), g_pVGuiLocalize->Find(pszSubtitle), 0 );
	g_pVGuiLocalize->ConvertUnicodeToANSI( wszPrint, szPrint, sizeof(szPrint) );

	CBaseHudChat *hudChat = (CBaseHudChat *)GET_HUDELEMENT( CHudChat );

	if ( !hudChat )
		return;
	
	wchar_t *pwLoc = g_pVGuiLocalize->Find( "#dod_radio_prefix" );
	char szPrefix[16];
	g_pVGuiLocalize->ConvertUnicodeToANSI( pwLoc, szPrefix, sizeof(szPrefix) );

	pwLoc = g_pVGuiLocalize->Find( pszSubtitle );
	char szSuffix[512];
	g_pVGuiLocalize->ConvertUnicodeToANSI( pwLoc, szSuffix, sizeof(szSuffix) );

	if ( pszSender )
	{
		hudChat->ChatPrintf( iSenderIndex, CHAT_FILTER_NONE, "%c%s %s %c: %s", COLOR_PLAYERNAME, szPrefix, g_PR->GetPlayerName( iSenderIndex ), COLOR_NORMAL, szSuffix );
	}
	else
	{
		hudChat->Printf( CHAT_FILTER_NONE, "%c%s %c: %s", color, szPrefix, COLOR_NORMAL, szSuffix );
	}
}

//-----------------------------------------------------------------------------
// Purpose: See if hud elements want key input. Return 0 if the key is swallowed
//-----------------------------------------------------------------------------
int	ClientModeDODNormal::HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( m_pFreezePanel )
	{
		m_pFreezePanel->HudElementKeyInput( down, keynum, pszCurrentBinding );
	}

	return BaseClass::HudElementKeyInput( down, keynum, pszCurrentBinding );
}
