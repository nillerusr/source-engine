//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Normal HUD mode
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "clientmode_shared.h"
#include "iinput.h"
#include "view_shared.h"
#include "iviewrender.h"
#include "hud_basechat.h"
#include "weapon_selection.h"
#include <vgui/IVGUI.h>
#include <vgui/Cursor.h>
#include <vgui/IPanel.h>
#include "engine/ienginesound.h"
#include <keyvalues.h>
#include <vgui_controls/AnimationController.h>
#include "vgui_int.h"
#include "hud_macros.h"
#include "hltvcamera.h"
#if defined( REPLAY_ENABLED )
#include "replaycamera.h"
#endif
#include "particlemgr.h"
#include "c_vguiscreen.h"
#include "c_team.h"
#include "c_rumble.h"
#include "fmtstr.h"
#include "achievementmgr.h"
#include "c_playerresource.h"
#include <vgui/ILocalize.h>
#if defined( _X360 )
#include "xbox/xbox_console.h"
#endif
#include "matchmaking/imatchframework.h"



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHudWeaponSelection;
class CHudChat;

static vgui::HContext s_hVGuiContext = DEFAULT_VGUI_CONTEXT;

ConVar cl_drawhud( "cl_drawhud", "1", FCVAR_CHEAT, "Enable the rendering of the hud" );
ConVar hud_takesshots( "hud_takesshots", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Auto-save a scoreboard screenshot at the end of a map." );

extern ConVar v_viewmodel_fov;

extern bool IsInCommentaryMode( void );

CON_COMMAND( hud_reloadscheme, "Reloads hud layout and animation scripts." )
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		ClientModeShared *mode = ( ClientModeShared * )GetClientModeNormal();
		if ( mode )
		{
			mode->ReloadScheme();
		}
	}
	ClientModeShared *mode = ( ClientModeShared * )GetFullscreenClientMode();
	if ( mode )
	{
		mode->ReloadSchemeWithRoot( VGui_GetFullscreenRootVPANEL() );
	}
}

#if 0
CON_COMMAND_F( crash, "Crash the client. Optional parameter -- type of crash:\n 0: read from NULL\n 1: write to NULL\n 2: DmCrashDump() (xbox360 only)", FCVAR_CHEAT )
{
	int crashtype = 0;
	int dummy;
	if ( args.ArgC() > 1 )
	{
		crashtype = Q_atoi( args[1] );
	}
	switch (crashtype)
	{
	case 0:
		dummy = *((int *) NULL);
		Msg("Crashed! %d\n", dummy); // keeps dummy from optimizing out
		break;
	case 1:
		*((int *)NULL) = 42;
		break;
#if defined( _X360 )
	case 2:
		XBX_CrashDump( false );
		break;
	case 3:
		XBX_CrashDumpFullHeap( true );
		break;
#endif
	default:
		Msg("Unknown variety of crash. You have now failed to crash. I hope you're happy.\n");
		break;
	}
}
#endif // _DEBUG

static void __MsgFunc_Rumble( bf_read &msg )
{
	unsigned char waveformIndex;
	unsigned char rumbleData;
	unsigned char rumbleFlags;

	waveformIndex = msg.ReadByte();
	rumbleData = msg.ReadByte();
	rumbleFlags = msg.ReadByte();

	int userID = XBX_GetActiveUserId();

	RumbleEffect( userID, waveformIndex, rumbleData, rumbleFlags );
}

static void __MsgFunc_VGUIMenu( bf_read &msg )
{
	char panelname[2048]; 

	msg.ReadString( panelname, sizeof(panelname) );

	bool bShow = msg.ReadByte()!= 0;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	int count = msg.ReadByte();

	KeyValues *keys = NULL;

	if ( count > 0 )
	{
		keys = new KeyValues("data");

		for ( int i=0; i<count; i++)
		{
			char name[255];
			char data[255];

			msg.ReadString( name, sizeof(name) );
			msg.ReadString( data, sizeof(data) );

			keys->SetString( name, data );
		}
	}

	GetViewPortInterface()->ShowPanel( panelname, bShow, keys, true );

	// Don't do this since ShowPanel auto-deletes the keys
	// keys->deleteThis();

	// is the server telling us to show the scoreboard (at the end of a map)?
	if ( Q_stricmp( panelname, "scores" ) == 0 )
	{
		if ( hud_takesshots.GetBool() == true )
		{
			GetHud().SetScreenShotTime( gpGlobals->curtime + 1.0 ); // take a screenshot in 1 second
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeShared::ClientModeShared()
{
	m_pViewport = NULL;
	m_pChatElement = NULL;
	m_pWeaponSelection = NULL;
	m_nRootSize[ 0 ] = m_nRootSize[ 1 ] = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeShared::~ClientModeShared()
{
	// VGui_Shutdown() should have deleted/NULL'd
	Assert( !m_pViewport );
}

void ClientModeShared::ReloadScheme( void )
{
	ReloadSchemeWithRoot( VGui_GetClientDLLRootPanel() );
}

void ClientModeShared::ReloadSchemeWithRoot( vgui::VPANEL pRoot )
{
	if ( pRoot )
	{
		int wide, tall;
		vgui::ipanel()->GetSize(pRoot, wide, tall);
		m_nRootSize[ 0 ] = wide;
		m_nRootSize[ 1 ] = tall;
	}

	m_pViewport->ReloadScheme( "resource/ClientScheme.res" );
	if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
	{
		ClearKeyValuesCache();
	}
	// Msg( "Reload scheme [%d]\n", GET_ACTIVE_SPLITSCREEN_SLOT() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::Init()
{
	InitChatHudElement();

	InitWeaponSelectionHudElement();

	// Derived ClientMode class must make sure m_Viewport is instantiated
	Assert( m_pViewport );
	m_pViewport->LoadHudLayout();

	ListenForGameEvent( "player_connect" );
	ListenForGameEvent( "player_disconnect" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "server_cvar" );
	ListenForGameEvent( "player_changename" );
	ListenForGameEvent( "player_fullyjoined" );	
	ListenForGameEvent( "teamplay_broadcast_audio" );
	ListenForGameEvent( "achievement_earned" );

#ifndef _XBOX
	HLTVCamera()->Init();
#if defined( REPLAY_ENABLED )
	ReplayCamera()->Init();
#endif
#endif

	m_CursorNone = vgui::dc_none;

	HOOK_MESSAGE( VGUIMenu );
	HOOK_MESSAGE( Rumble );
}

void ClientModeShared::InitChatHudElement()
{
	m_pChatElement = CBaseHudChat::GetHudChat();
	Assert( m_pChatElement );
}

void ClientModeShared::InitWeaponSelectionHudElement()
{
	m_pWeaponSelection = ( CBaseHudWeaponSelection * )GET_HUDELEMENT( CHudWeaponSelection );
	Assert( m_pWeaponSelection );
}

void ClientModeShared::InitViewport()
{
}


void ClientModeShared::VGui_Shutdown()
{
	delete m_pViewport;
	m_pViewport = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::Shutdown()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
bool ClientModeShared::CreateMove( float flInputSampleTime, CUserCmd *cmd )
{
	// Let the player override the view.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if(!pPlayer)
		return true;

	// Let the player at it
	return pPlayer->CreateMove( flInputSampleTime, cmd );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSetup - 
//-----------------------------------------------------------------------------
void ClientModeShared::OverrideView( CViewSetup *pSetup )
{
	QAngle camAngles;

	// Let the player override the view.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if(!pPlayer)
		return;

	pPlayer->OverrideView( pSetup );

	if( ::input->CAM_IsThirdPerson() )
	{
		Vector cam_ofs;

		::input->CAM_GetCameraOffset( cam_ofs );

		camAngles[ PITCH ] = cam_ofs[ PITCH ];
		camAngles[ YAW ] = cam_ofs[ YAW ];
		camAngles[ ROLL ] = 0;

		Vector camForward, camRight, camUp;
		AngleVectors( camAngles, &camForward, &camRight, &camUp );

		VectorMA( pSetup->origin, -cam_ofs[ ROLL ], camForward, pSetup->origin );

		static ConVarRef c_thirdpersonshoulder( "c_thirdpersonshoulder" );
		if ( c_thirdpersonshoulder.GetBool() )
		{
			static ConVarRef c_thirdpersonshoulderoffset( "c_thirdpersonshoulderoffset" );
			static ConVarRef c_thirdpersonshoulderheight( "c_thirdpersonshoulderheight" );
			static ConVarRef c_thirdpersonshoulderaimdist( "c_thirdpersonshoulderaimdist" );

			// add the shoulder offset to the origin in the cameras right vector
			VectorMA( pSetup->origin, c_thirdpersonshoulderoffset.GetFloat(), camRight, pSetup->origin );

			// add the shoulder height to the origin in the cameras up vector
			VectorMA( pSetup->origin, c_thirdpersonshoulderheight.GetFloat(), camUp, pSetup->origin );

			// adjust the yaw to the aim-point
			camAngles[ YAW ] += RAD2DEG( atan(c_thirdpersonshoulderoffset.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );

			// adjust the pitch to the aim-point
			camAngles[ PITCH ] += RAD2DEG( atan(c_thirdpersonshoulderheight.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );
		}

		// Override angles from third person camera
		VectorCopy( camAngles, pSetup->angles );
	}
	else if (::input->CAM_IsOrthographic())
	{
		pSetup->m_bOrtho = true;
		float w, h;
		::input->CAM_OrthographicSize( w, h );
		w *= 0.5f;
		h *= 0.5f;
		pSetup->m_OrthoLeft   = -w;
		pSetup->m_OrthoTop    = -h;
		pSetup->m_OrthoRight  = w;
		pSetup->m_OrthoBottom = h;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawEntity(C_BaseEntity *pEnt)
{
	return true;
}

bool ClientModeShared::ShouldDrawParticles( )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Allow weapons to override mouse input (for binoculars)
//-----------------------------------------------------------------------------
void ClientModeShared::OverrideMouseInput( float *x, float *y )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	C_BaseCombatWeapon *pWeapon = pPlayer ? pPlayer->GetActiveWeapon() : NULL;;
	if ( pWeapon )
	{
		pWeapon->OverrideMouseInput( x, y );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawViewModel()
{
	return true;
}

bool ClientModeShared::ShouldDrawDetailObjects( )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawCrosshair( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Don't draw the current view entity if we are not in 3rd person
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawLocalPlayer( C_BasePlayer *pPlayer )
{
	if ( pPlayer->IsViewEntity() && !pPlayer->ShouldDrawLocalPlayer() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: The mode can choose to not draw fog
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawFog( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::AdjustEngineViewport( int& x, int& y, int& width, int& height )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::PreRender( CViewSetup *pSetup )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::PostRender()
{
	// Let the particle manager simulate things that haven't been simulated.
	ParticleMgr()->PostRender();
}

void ClientModeShared::PostRenderVGui()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::Update()
{
	if ( m_pViewport->IsVisible() != cl_drawhud.GetBool() )
	{
		m_pViewport->SetVisible( cl_drawhud.GetBool() );
	}

	UpdateRumbleEffects( XBX_GetActiveUserId() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::OnColorCorrectionWeightsReset( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float ClientModeShared::GetColorCorrectionScale( void ) const
{
	return 0.0f;
}

//-----------------------------------------------------------------------------
// This processes all input before SV Move messages are sent
//-----------------------------------------------------------------------------

void ClientModeShared::ProcessInput(bool bActive)
{
	GetHud().ProcessInput( bActive );
}

//-----------------------------------------------------------------------------
// Purpose: We've received a keypress from the engine. Return 1 if the engine is allowed to handle it.
//-----------------------------------------------------------------------------
int	ClientModeShared::KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( engine->Con_IsVisible() )
		return 1;

	// Should we start typing a message?
	if ( pszCurrentBinding &&
		( Q_strcmp( pszCurrentBinding, "messagemode" ) == 0 ||
		Q_strcmp( pszCurrentBinding, "say" ) == 0 ) )
	{
		if ( down )
		{
			StartMessageMode( MM_SAY );
		}
		return 0;
	}
	else if ( pszCurrentBinding &&
		( Q_strcmp( pszCurrentBinding, "messagemode2" ) == 0 ||
		Q_strcmp( pszCurrentBinding, "say_team" ) == 0 ) )
	{
		if ( down )
		{
			StartMessageMode( MM_SAY_TEAM );
		}
		return 0;
	}

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if ( IsJoystickCode( keynum ) )
	{
		keynum = GetBaseButtonCode( keynum );
	}

	// if ingame spectator mode, let spectator input intercept key event here
	if( pPlayer &&
		( pPlayer->GetObserverMode() > OBS_MODE_DEATHCAM ) &&
		!HandleSpectatorKeyInput( down, keynum, pszCurrentBinding ) )
	{
		return 0;
	}

	// Let game-specific hud elements get a crack at the key input
	if ( !HudElementKeyInput( down, keynum, pszCurrentBinding ) )
	{
		return 0;
	}

	C_BaseCombatWeapon *pWeapon = pPlayer ? pPlayer->GetActiveWeapon() : NULL;
	if ( pWeapon )
	{
		return pWeapon->KeyInput( down, keynum, pszCurrentBinding );
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Helper to find if a binding exists in a possible chain of bindings
//-----------------------------------------------------------------------------
static bool ContainsBinding( const char *pszBindingString, const char *pszBinding )
{
	if ( !strchr( pszBindingString, ';' ) )
	{
		return !Q_stricmp( pszBindingString, pszBinding );
	}
	else
	{
		// Tokenize the binding name
		char szBinding[ 256 ];
		Q_strncpy( szBinding, pszBindingString, sizeof( szBinding ) );

		const char *pToken = strtok( szBinding, ";" );

		while ( pToken )
		{
			if ( !Q_stricmp( pszBinding, pToken ) )
			{
				return true;
			}

			pToken = strtok( NULL, ";" );
		}
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: See if spectator input occurred. Return 0 if the key is swallowed.
//-----------------------------------------------------------------------------
int ClientModeShared::HandleSpectatorKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	// we are in spectator mode, open spectator menu
	if ( down && pszCurrentBinding && ContainsBinding( pszCurrentBinding, "+duck" ) )
	{
		m_pViewport->ShowPanel( PANEL_SPECMENU, true );
		return 0; // we handled it, don't handle twice or send to server
	}
	else if ( down && pszCurrentBinding && ContainsBinding( pszCurrentBinding, "+attack" ) )
	{
		engine->ClientCmd( "spec_next" );
		return 0;
	}
	else if ( down && pszCurrentBinding && ContainsBinding( pszCurrentBinding, "+attack2" ) )
	{
		engine->ClientCmd( "spec_prev" );
		return 0;
	}
	else if ( down && pszCurrentBinding && ContainsBinding( pszCurrentBinding, "+jump" ) )
	{
		engine->ClientCmd( "spec_mode" );
		return 0;
	}
	else if ( down && pszCurrentBinding && ContainsBinding( pszCurrentBinding, "+strafe" ) )
	{
		HLTVCamera()->SetAutoDirector( true );
#if defined( REPLAY_ENABLED )
		ReplayCamera()->SetAutoDirector( true );
#endif
		return 0;
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: See if hud elements want key input. Return 0 if the key is swallowed
//-----------------------------------------------------------------------------
int ClientModeShared::HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( GetFullscreenClientMode() && GetFullscreenClientMode() != this &&
		!GetFullscreenClientMode()->HudElementKeyInput( down, keynum, pszCurrentBinding ) )
		return 0;

	if ( m_pWeaponSelection )
	{
		if ( !m_pWeaponSelection->KeyInput( down, keynum, pszCurrentBinding ) )
		{
			return 0;
		}
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::Panel *ClientModeShared::GetMessagePanel()
{
	if ( m_pChatElement && m_pChatElement->GetInputPanel() && m_pChatElement->GetInputPanel()->IsVisible() )
		return m_pChatElement->GetInputPanel();

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: The player has started to type a message
//-----------------------------------------------------------------------------
void ClientModeShared::StartMessageMode( int iMessageModeType )
{
	// Can only show chat UI in multiplayer!!!
	if ( gpGlobals->maxClients == 1 )
	{
		return;
	}
	if ( m_pChatElement )
	{
		m_pChatElement->StartMessageMode( iMessageModeType );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *newmap - 
//-----------------------------------------------------------------------------
void ClientModeShared::LevelInit( const char *newmap )
{
	m_pViewport->GetAnimationController()->StartAnimationSequence("LevelInit");

	// Tell the Chat Interface
	if ( m_pChatElement )
	{
		m_pChatElement->LevelInit( newmap );
	}

	// we have to fake this event clientside, because clients connect after that
	IGameEvent *event = gameeventmanager->CreateEvent( "game_newmap" );
	if ( event )
	{
		event->SetString("mapname", newmap );
		gameeventmanager->FireEventClientSide( event );
	}

	// Create a vgui context for all of the in-game vgui panels...
	if ( s_hVGuiContext == DEFAULT_VGUI_CONTEXT )
	{
		s_hVGuiContext = vgui::ivgui()->CreateContext();
	}

	// Reset any player explosion/shock effects
	CLocalPlayerFilter filter;
	enginesound->SetPlayerDSP( filter, 0, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::LevelShutdown( void )
{
	if ( m_pChatElement )
	{
	m_pChatElement->LevelShutdown();
	}
	if ( s_hVGuiContext != DEFAULT_VGUI_CONTEXT )
	{
		vgui::ivgui()->DestroyContext( s_hVGuiContext );
		s_hVGuiContext = DEFAULT_VGUI_CONTEXT;
	}

	// Reset any player explosion/shock effects
	CLocalPlayerFilter filter;
	enginesound->SetPlayerDSP( filter, 0, true );
}

void ClientModeShared::Enable()
{
	vgui::VPANEL pRoot = VGui_GetClientDLLRootPanel();
	EnableWithRootPanel( pRoot );
}

void ClientModeShared::EnableWithRootPanel( vgui::VPANEL pRoot )
{
	// Add our viewport to the root panel.
	if( pRoot != NULL )
	{
		m_pViewport->SetParent( pRoot );
	}

	// All hud elements should be proportional
	// This sets that flag on the viewport and all child panels
	m_pViewport->SetProportional( true );

	m_pViewport->SetCursor( m_CursorNone );
	vgui::surface()->SetCursor( m_CursorNone );

	m_pViewport->SetVisible( true );
	if ( m_pViewport->IsKeyBoardInputEnabled() )
	{
		m_pViewport->RequestFocus();
	}

	Layout();
}


void ClientModeShared::Disable()
{
	vgui::VPANEL pRoot;

	// Remove our viewport from the root panel.
	if( ( pRoot = VGui_GetClientDLLRootPanel() ) != NULL )
	{
		m_pViewport->SetParent( (vgui::VPANEL)NULL );
	}

	m_pViewport->SetVisible( false );
}


void ClientModeShared::Layout( bool bForce /*= false*/)
{
	vgui::VPANEL pRoot;
	int wide, tall;

	// Make the viewport fill the root panel.
	if( ( pRoot = m_pViewport->GetVParent() ) != NULL )
	{
		vgui::ipanel()->GetSize(pRoot, wide, tall);
		bool changed = wide != m_nRootSize[ 0 ] || tall != m_nRootSize[ 1 ];
		m_pViewport->SetBounds(0, 0, wide, tall);
		if ( changed || bForce )
		{
			ReloadSchemeWithRoot( pRoot );
		}
	}
}

float ClientModeShared::GetViewModelFOV( void )
{
	return v_viewmodel_fov.GetFloat();
}

vgui::Panel *ClientModeShared::GetPanelFromViewport( const char *pchNamePath )
{
	char szTagetName[ 256 ];
	Q_strncpy( szTagetName, pchNamePath, sizeof(szTagetName) );

	char *pchName = szTagetName;

	char *pchEndToken = strchr( pchName, ';' );
	if ( pchEndToken )
	{
		*pchEndToken = '\0';
	}

	char *pchNextName = strchr( pchName, '/' );
	if ( pchNextName )
	{
		*pchNextName = '\0';
		pchNextName++;
	}

	// Comma means we want to count to a specific instance by name
	int nInstance = 0;

	char *pchInstancePos = strchr( pchName, ',' );
	if ( pchInstancePos )
	{
		*pchInstancePos = '\0';
		pchInstancePos++;

		nInstance = atoi( pchInstancePos );
	}

	// Find the child
	int nCurrentInstance = 0;
	vgui::Panel *pPanel = NULL;

	for ( int i = 0; i < GetViewport()->GetChildCount(); i++ )
	{
		Panel *pChild = GetViewport()->GetChild( i );
		if ( !pChild )
			continue;

		if ( stricmp( pChild->GetName(), pchName ) == 0 )
		{
			nCurrentInstance++;

			if ( nCurrentInstance > nInstance )
			{
				pPanel = pChild;
				break;
			}
		}
	}

	pchName = pchNextName;

	while ( pPanel )
	{
		if ( !pchName || pchName[ 0 ] == '\0' )
		{
			break;
		}

		pchNextName = strchr( pchName, '/' );
		if ( pchNextName )
		{
			*pchNextName = '\0';
			pchNextName++;
		}

		// Comma means we want to count to a specific instance by name
		nInstance = 0;

		pchInstancePos = strchr( pchName, ',' );
		if ( pchInstancePos )
		{
			*pchInstancePos = '\0';
			pchInstancePos++;

			nInstance = atoi( pchInstancePos );
		}

		// Find the child
		nCurrentInstance = 0;
		vgui::Panel *pNextPanel = NULL;

		for ( int i = 0; i < pPanel->GetChildCount(); i++ )
		{
			Panel *pChild = pPanel->GetChild( i );
			if ( !pChild )
				continue;

			if ( stricmp( pChild->GetName(), pchName ) == 0 )
			{
				nCurrentInstance++;

				if ( nCurrentInstance > nInstance )
				{
					pNextPanel = pChild;
					break;
				}
			}
		}

		pPanel = pNextPanel;
		pchName = pchNextName;
	}

	return pPanel;
}

class CHudChat;

bool PlayerNameNotSetYet( const char *pszName )
{
	if ( pszName && pszName[0] )
	{
		// Don't show "unconnected" if we haven't got the players name yet
		if ( Q_strnicmp(pszName,"unconnected",11) == 0 )
			return true;
		if ( Q_strnicmp(pszName,"NULLNAME",11) == 0 )
			return true;
	}

	return false;
}

void ClientModeShared::FireGameEvent( IGameEvent *event )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GetSplitScreenPlayerSlot() );

	CBaseHudChat *hudChat = CBaseHudChat::GetHudChat();

	const char *eventname = event->GetName();

	if ( Q_strcmp( "player_connect", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;
		if ( !hudChat )
			return;
		if ( PlayerNameNotSetYet(event->GetString("name")) )
			return;

		if ( !IsInCommentaryMode() )
		{
			wchar_t wszLocalized[100];
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString("name"), wszPlayerName, sizeof(wszPlayerName) );
			g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_connecting" ), 1, wszPlayerName );

			char szLocalized[100];
			g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

			hudChat->Printf( CHAT_FILTER_JOINLEAVE, "%s", szLocalized );
		}
	}
	else if ( Q_strcmp( "player_disconnect", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;
		C_BasePlayer *pPlayer = USERID2PLAYER( event->GetInt("userid") );

		if ( !hudChat || !pPlayer )
			return;
		if ( PlayerNameNotSetYet(event->GetString("name")) )
			return;

		if ( !IsInCommentaryMode() )
		{
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(), wszPlayerName, sizeof(wszPlayerName) );

			wchar_t wszReason[64];
			g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString("reason"), wszReason, sizeof(wszReason) );

			wchar_t wszLocalized[100];
			if (IsPC())
			{
				g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_left_game" ), 2, wszPlayerName, wszReason );
			}
			else
			{
				g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_left_game" ), 1, wszPlayerName );
			}

			char szLocalized[100];
			g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

			hudChat->Printf( CHAT_FILTER_JOINLEAVE, "%s", szLocalized );
		}
	}
	else if ( Q_strcmp( "player_fullyjoined", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;
		if ( !hudChat )
			return;
		if ( PlayerNameNotSetYet(event->GetString("name")) )
			return;

		wchar_t wszLocalized[100];
		wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString("name"), wszPlayerName, sizeof(wszPlayerName) );
		g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_joined_game" ), 1, wszPlayerName );

		char szLocalized[100];
		g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

		hudChat->Printf( CHAT_FILTER_JOINLEAVE, "%s", szLocalized );
	}
	else if ( Q_strcmp( "player_team", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;

		C_BasePlayer *pPlayer = USERID2PLAYER( event->GetInt("userid") );
		if ( !hudChat )
			return;
		if ( !pPlayer )
			return;

		bool bDisconnected = event->GetBool("disconnect");

		if ( bDisconnected )
			return;

		int team = event->GetInt( "team" );
		bool bAutoTeamed = event->GetBool( "autoteam", false );
		bool bSilent = event->GetBool( "silent", false );

		const char *pszName = pPlayer->GetPlayerName();
		if ( PlayerNameNotSetYet(pszName) )
			return;

		if ( !bSilent )
		{
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode( pszName, wszPlayerName, sizeof(wszPlayerName) );

			wchar_t wszTeam[64];
			C_Team *pTeam = GetGlobalTeam( team );
			if ( pTeam )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( pTeam->Get_Name(), wszTeam, sizeof(wszTeam) );
			}
			else
			{
				_snwprintf ( wszTeam, sizeof( wszTeam ) / sizeof( wchar_t ), L"%d", team );
			}

			if ( !IsInCommentaryMode() )
			{
				wchar_t wszLocalized[100];
				if ( bAutoTeamed )
				{
					g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_joined_autoteam" ), 2, wszPlayerName, wszTeam );
				}
				else
				{
					g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_joined_team" ), 2, wszPlayerName, wszTeam );
				}

				char szLocalized[100];
				g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

				hudChat->Printf( CHAT_FILTER_TEAMCHANGE, "%s", szLocalized );
			}
		}

		if ( C_BasePlayer::IsLocalPlayer( pPlayer ) )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( pPlayer );

			// that's me
			pPlayer->TeamChange( team );
		}
	}
	else if ( Q_strcmp( "player_changename", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;

		if ( !hudChat )
			return;

		const char *pszOldName = event->GetString("oldname");
		if ( PlayerNameNotSetYet(pszOldName) )
			return;

		wchar_t wszOldName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode( pszOldName, wszOldName, sizeof(wszOldName) );

		wchar_t wszNewName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString( "newname" ), wszNewName, sizeof(wszNewName) );

		wchar_t wszLocalized[100];
		g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_changed_name" ), 2, wszOldName, wszNewName );

		char szLocalized[100];
		g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

		hudChat->Printf( CHAT_FILTER_NAMECHANGE, "%s", szLocalized );
	}
	else if ( Q_strcmp( "teamplay_broadcast_audio", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;

		int team = event->GetInt( "team" );

		bool bValidTeam = false;

		if ( (GetLocalTeam() && GetLocalTeam()->GetTeamNumber() == team) )
		{
			bValidTeam = true;
		}

		//If we're in the spectator team then we should be getting whatever messages the person I'm spectating gets.
		if ( bValidTeam == false )
		{
			CBasePlayer *pSpectatorTarget = UTIL_PlayerByIndex( GetSpectatorTarget() );

			if ( pSpectatorTarget && (GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE) )
			{
				if ( pSpectatorTarget->GetTeamNumber() == team )
				{
					bValidTeam = true;
				}
			}
		}

		if ( team == 0 && GetLocalTeam() > 0 )
		{
			bValidTeam = false;
		}

		if ( bValidTeam == true )
		{
			CLocalPlayerFilter filter;
			const char *pszSoundName = event->GetString("sound");
			C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, pszSoundName );
		}
	}
	else if ( Q_strcmp( "teamplay_broadcast_audio", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;

		int team = event->GetInt( "team" );
		if ( !team || (GetLocalTeam() && GetLocalTeam()->GetTeamNumber() == team) )
		{
			CLocalPlayerFilter filter;
			const char *pszSoundName = event->GetString("sound");
			C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, pszSoundName );
		}
	}
	else if ( Q_strcmp( "server_cvar", eventname ) == 0 )
	{
		if ( IsX360() && !developer.GetBool() )
			return;

		if ( this == GetFullscreenClientMode() )
			return;

		if ( !IsInCommentaryMode() )
		{
			wchar_t wszCvarName[64];
			g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString("cvarname"), wszCvarName, sizeof(wszCvarName) );

			wchar_t wszCvarValue[16];
			g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString("cvarvalue"), wszCvarValue, sizeof(wszCvarValue) );

			wchar_t wszLocalized[100];
			g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_server_cvar_changed" ), 2, wszCvarName, wszCvarValue );

			char szLocalized[100];
			g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

			hudChat->Printf( CHAT_FILTER_SERVERMSG, "%s", szLocalized );
		}
	}
	else if ( Q_strcmp( "achievement_earned", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;

		int iPlayerIndex = event->GetInt( "player" );
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
		int iAchievement = event->GetInt( "achievement" );

		if ( !hudChat || !pPlayer )
			return;

		if ( !IsInCommentaryMode() )
		{
			CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
			if ( !pAchievementMgr )
				return;

			IAchievement *pAchievement = pAchievementMgr->GetAchievementByID( iAchievement, GetSplitScreenPlayerSlot() );
			if ( pAchievement )
			{
				if ( !pPlayer->IsDormant() )
				{
					// no particle effect if the local player is the one with the achievement or the player is dead
					if ( !C_BasePlayer::IsLocalPlayer( pPlayer ) && pPlayer->IsAlive() ) 
					{
						//tagES using the "head" attachment won't work for CS and DoD
						pPlayer->ParticleProp()->Create( "achieved", PATTACH_POINT_FOLLOW, "head" );
					}

					pPlayer->OnAchievementAchieved( iAchievement );
				}

				if ( g_PR )
				{
					wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
					g_pVGuiLocalize->ConvertANSIToUnicode( g_PR->GetPlayerName( iPlayerIndex ), wszPlayerName, sizeof( wszPlayerName ) );

					const wchar_t *pchLocalizedAchievement = ACHIEVEMENT_LOCALIZED_NAME_FROM_STR( pAchievement->GetName() );
					if ( pchLocalizedAchievement )
					{
						wchar_t wszLocalizedString[128];
						g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), g_pVGuiLocalize->Find( "#Achievement_Earned" ), 2, wszPlayerName, pchLocalizedAchievement );

						char szLocalized[128];
						g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalizedString, szLocalized, sizeof( szLocalized ) );

						hudChat->ChatPrintf( iPlayerIndex, CHAT_FILTER_SERVERMSG, "%s", szLocalized );
					}
				}
			}
		}
	}
	else
	{
		DevMsg( 2, "Unhandled GameEvent in ClientModeShared::FireGameEvent - %s\n", event->GetName()  );
	}
}





//-----------------------------------------------------------------------------
// In-game VGUI context 
//-----------------------------------------------------------------------------
void ClientModeShared::ActivateInGameVGuiContext( vgui::Panel *pPanel )
{
	vgui::ivgui()->AssociatePanelWithContext( s_hVGuiContext, pPanel->GetVPanel() );
	vgui::ivgui()->ActivateContext( s_hVGuiContext );
}

void ClientModeShared::DeactivateInGameVGuiContext()
{
	vgui::ivgui()->ActivateContext( DEFAULT_VGUI_CONTEXT );
}

int ClientModeShared::GetSplitScreenPlayerSlot() const
{
	int nSplitScreenUserSlot = vgui::ipanel()->GetMessageContextId( m_pViewport->GetVPanel() );
	Assert( nSplitScreenUserSlot != -1 );
	return nSplitScreenUserSlot;
}
