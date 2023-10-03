#include "cbase.h"
#include "clientmode_asw.h"
#include "vgui_int.h"
#include "hud.h"
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>
#include "iinput.h"
#include "ienginevgui.h"
#include "hud_macros.h"
#include "view_shared.h"
#include "hud_basechat.h"
#include "achievementmgr.h"
#include "fmtstr.h"
// asw
#include "asw_input.h"
#include "c_asw_marine.h"
#include "c_asw_marine_resource.h"
#include "c_asw_game_resource.h"
#include "c_asw_player.h"
#include "c_asw_campaign_save.h"
#include "asw_remote_turret_shared.h"
#include "asw_gamerules.h"
#include "asw_util_shared.h"
#include "asw_campaign_info.h"
#include "techmarinefailpanel.h"
#include "FadeInPanel.h"
#include <vgui/IInput.h>
#include "ivieweffects.h"
#include "shake.h"
#include <vgui/ILocalize.h>
#include "c_playerresource.h"
#include "asw_medal_store.h"
#include "TechMarineFailPanel.h"
#include "vgui\asw_vgui_info_message.h"
//#include "keydefs.h"
#include "soundenvelope.h"
#include "ivmodemanager.h"
#include "voice_status.h"
#include "PanelMetaClassMgr.h"
#include "asw_loading_panel.h"
#include "asw_logo_panel.h"
#include "c_user_message_register.h"
#include "inetchannelinfo.h"
#include "engine/IVDebugOverlay.h"
#include "debugoverlay_shared.h"
#include "viewpostprocess.h"
#include "shaderapi/ishaderapi.h"
#include "tier2/renderutils.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "nb_header_footer.h"
#include "asw_briefing.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define FAILED_CC_LOOKUP_FILENAME "materials/correction/cc_failed.raw"
#define FAILED_CC_FADE_TIME 1.0f
#define INFESTED_CC_LOOKUP_FILENAME "scripts/colorcorrection/infested_green.raw"
#define INFESTED_CC_FADE_TIME 0.5f

extern bool IsInCommentaryMode( void );

extern ConVar mat_object_motion_blur_enable;

// asw
ConVar asw_reconnect_after_outro( "asw_reconnect_after_outro", "0", 0, "If set, client will reconnect to server after outro" );
ConVar asw_hear_from_marine( "asw_hear_from_marine", "0", FCVAR_NONE, "Audio hearing position is at current marine's ears" );
ConVar asw_hear_pos_debug("asw_hear_pos_debug", "0", FCVAR_NONE, "Shows audio hearing position" );
ConVar asw_hear_height("asw_hear_height", "0", FCVAR_NONE, "If set, hearing audio position is this many units above the marine.  If number is negative, then hear position is number of units below the camera." );
ConVar asw_hear_fixed("asw_hear_fixed", "0", FCVAR_NONE, "If set, hearing audio position is locked in place.  Use asw_set_hear_pos to set the fixed position to the current audio location." );

Vector g_asw_vec_fixed_cam(-276.03076, -530.74951, -196.65625);
QAngle g_asw_ang_fixed_cam(42.610226, 90.289375, 0);
extern ConVar asw_vehicle_cam_height;
extern ConVar asw_vehicle_cam_pitch;
extern ConVar asw_vehicle_cam_dist;

Vector g_asw_vec_fixed_hear_pos = vec3_origin;
Vector g_asw_vec_last_hear_pos = vec3_origin;

ConVar default_fov( "default_fov", "75", FCVAR_CHEAT );
ConVar fov_desired( "fov_desired", "75", FCVAR_USERINFO, "Sets the base field-of-view.", true, 1.0, true, 75.0 );

vgui::HScheme g_hVGuiCombineScheme = 0;

vgui::DHANDLE<CASW_Logo_Panel> g_hLogoPanel;

static IClientMode *g_pClientMode[ MAX_SPLITSCREEN_PLAYERS ];
IClientMode *GetClientMode()
{
	Assert( engine->IsLocalPlayerResolvable() );
	return g_pClientMode[ engine->GetActiveSplitScreenPlayerSlot() ];
}

// Voice data
void VoxCallback( IConVar *var, const char *oldString, float oldFloat )
{
	if ( engine && engine->IsConnected() )
	{
		ConVarRef voice_vox( var->GetName() );
		if ( voice_vox.GetBool() /*&& voice_modenable.GetBool()*/ )
		{
			engine->ClientCmd( "voicerecord_toggle on\n" );
		}
		else
		{
			engine->ClientCmd( "voicerecord_toggle off\n" );
		}
	}
}
ConVar voice_vox( "voice_vox", "0", FCVAR_ARCHIVE, "Voice chat uses a vox-style always on", false, 0, true, 1, VoxCallback );

//--------------------------------------------------------------------------------------------------------
class CVoxManager : public CAutoGameSystem
{
public:
	CVoxManager() : CAutoGameSystem( "VoxManager" ) { }

	virtual void LevelInitPostEntity( void )
	{
		if ( voice_vox.GetBool() /*&& voice_modenable.GetBool()*/ )
		{
			engine->ClientCmd( "voicerecord_toggle on\n" );
		}
	}


	virtual void LevelShutdownPreEntity( void )
	{
		if ( voice_vox.GetBool() )
		{
			engine->ClientCmd( "voicerecord_toggle off\n" );
		}
	}
};


//--------------------------------------------------------------------------------------------------------
static CVoxManager s_VoxManager;


// --------------------------------------------------------------------------------- //
// CASWModeManager.
// --------------------------------------------------------------------------------- //

class CASWModeManager : public IVModeManager
{
public:
	virtual void	Init();
	virtual void	SwitchMode( bool commander, bool force ) {}
	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );
	virtual void	ActivateMouse( bool isactive ) {}
};

static CASWModeManager g_ModeManager;
IVModeManager *modemanager = ( IVModeManager * )&g_ModeManager;


// --------------------------------------------------------------------------------- //
// CASWModeManager implementation.
// --------------------------------------------------------------------------------- //

#define SCREEN_FILE		"scripts/vgui_screens.txt"

void CASWModeManager::Init()
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		g_pClientMode[ i ] = GetClientModeNormal();
	}

	PanelMetaClassMgr()->LoadMetaClassDefinitionFile( SCREEN_FILE );

	GetClientVoiceMgr()->SetHeadLabelOffset( 40 );
}

void CASWModeManager::LevelInit( const char *newmap )
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetClientMode()->LevelInit( newmap );
	}
}

void CASWModeManager::LevelShutdown( void )
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetClientMode()->LevelShutdown();
	}
}

ClientModeASW g_ClientModeNormal[ MAX_SPLITSCREEN_PLAYERS ];

IClientMode *GetClientModeNormal()
{
	Assert( engine->IsLocalPlayerResolvable() );
	return &g_ClientModeNormal[ engine->GetActiveSplitScreenPlayerSlot() ];
}

ClientModeASW* GetClientModeASW()
{
	Assert( engine->IsLocalPlayerResolvable() );
	return &g_ClientModeNormal[ engine->GetActiveSplitScreenPlayerSlot() ];
}

// these vgui panels will be closed at various times (e.g. when the level ends/starts)
static char const *s_CloseWindowNames[]={
	"g_BriefingFrame",
	"g_PlayerListFrame",
	"m_CampaignFrame",
	"ComputerFrame",
	"ComputerContainer",
	"m_MissionCompleteFrame",
	"Credits",
	"CainMail",
	"TechFail",
	"QueenHealthPanel",
	"ForceReady",
	"ReviveDialog",
	"SkillToSpendDialog",
	"InfoMessageWindow",
	"SkipIntro",
	"InGameBriefingContainer",
	"HoldoutWaveAnnouncePanel",
	"HoldoutWaveEndPanel",
	"ResupplyFrame",
};

//-----------------------------------------------------------------------------
// Purpose: this is the viewport that contains all the hud elements
//-----------------------------------------------------------------------------
class CHudViewport : public CBaseViewport
{
private:
	DECLARE_CLASS_SIMPLE( CHudViewport, CBaseViewport );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		GetHud().InitColors( pScheme );

		SetPaintBackgroundEnabled( false );
	}

	virtual void CreateDefaultPanels( void ) { /* don't create any panels yet*/ };
};
//--------------------------------------------------------------------------------------------------------

// See interface.h/.cpp for specifics:  basically this ensures that we actually Sys_UnloadModule the dll and that we don't call Sys_LoadModule 
//  over and over again.
static CDllDemandLoader g_GameUI( "gameui" );

class FullscreenASWViewport : public CHudViewport
{
private:
	DECLARE_CLASS_SIMPLE( FullscreenASWViewport, CHudViewport );

private:
	virtual void InitViewportSingletons( void )
	{
		SetAsFullscreenViewportInterface();
	}
};

class ClientModeASWFullscreen : public	ClientModeASW
{
	DECLARE_CLASS_SIMPLE( ClientModeASWFullscreen, ClientModeASW );
public:
	virtual void InitViewport()
	{
		// Skip over BaseClass!!!
		BaseClass::BaseClass::InitViewport();
		m_pViewport = new FullscreenASWViewport();
		m_pViewport->Start( gameuifuncs, gameeventmanager );
	}
	virtual void Init()
	{
		CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
		if ( gameUIFactory )
		{
			IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
			if ( NULL != pGameUI )
			{
				// insert stats summary panel as the loading background dialog
				CASW_Loading_Panel *pPanel = GASWLoadingPanel();
				pPanel->InvalidateLayout( false, true );
				pPanel->SetVisible( false );
				pPanel->MakePopup( false );
				pGameUI->SetLoadingBackgroundDialog( pPanel->GetVPanel() );

				// add ASI logo to main menu
				CASW_Logo_Panel *pLogo = new CASW_Logo_Panel( NULL, "ASILogo" );
				vgui::VPANEL GameUIRoot = enginevgui->GetPanel( PANEL_GAMEUIDLL );
				pLogo->SetParent( GameUIRoot );
				g_hLogoPanel = pLogo;
			}		
		}

		// 
		//CASW_VGUI_Debug_Panel *pDebugPanel = new CASW_VGUI_Debug_Panel( GetViewport(), "ASW Debug Panel" );
		//g_hDebugPanel = pDebugPanel;

		// Skip over BaseClass!!!
		BaseClass::BaseClass::Init();

		// Load up the combine control panel scheme
		if ( !g_hVGuiCombineScheme )
		{
			g_hVGuiCombineScheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), IsXbox() ? "resource/ClientScheme.res" : "resource/CombinePanelScheme.res", "CombineScheme" );
			if (!g_hVGuiCombineScheme)
			{
				Warning( "Couldn't load combine panel scheme!\n" );
			}
		}
	}
	void Shutdown()
	{
		DestroyASWLoadingPanel();
		if (g_hLogoPanel.Get())
		{
			delete g_hLogoPanel.Get();
		}
	}
};

//--------------------------------------------------------------------------------------------------------
static ClientModeASWFullscreen g_FullscreenClientMode;
IClientMode *GetFullscreenClientMode( void )
{
	return &g_FullscreenClientMode;
}


// gives the current marine some 'poison blur' (causes motion blur over his screen for the duration, making it harder to see)
static void __MsgFunc_ASWBlur( bf_read &msg )
{
	float duration = msg.ReadShort() * 0.1f;
	
	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if ( local )
	{
		C_ASW_Marine *marine = local->GetMarine();
		if (marine)
		{
			marine->SetPoisoned(duration);
		}
	}
}

// we get this message when players proceed after completing the last mission in a campaign
static void __MsgFunc_ASWCampaignCompleted( bf_read &msg )
{	
	CASW_Player *pPlayer = CASW_Player::GetLocalASWPlayer();
	// let our clientside medal store increase the number of completed campaigns by 1
	if (pPlayer && ASWGameResource())
	{
		// count how many marines we have
		C_ASW_Game_Resource *pGameResource = ASWGameResource();
		int iMarines = 0;
		for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
		{
			C_ASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
			if (pMR && pMR->GetCommanderIndex() == pPlayer->entindex())
			{
				iMarines++;
			}
		}
#ifdef USE_MEDAL_STORE
		if (iMarines > 0 && GetMedalStore() && ASWGameRules() && !ASWGameRules()->m_bCheated
			&& !engine->IsPlayingDemo())
			GetMedalStore()->OnIncreaseCounts(0,1,0, (gpGlobals->maxClients <= 1));
#endif
	}

#ifdef OUTRO_MAP
	// check for changing to the outro map
	//int iDedicated =
						msg.ReadByte();	
	int iHostIndex = msg.ReadByte();	

	Msg("[C] __MsgFunc_ASWCampaignCompleted. playerindex=%d hostindex =%d offline=%d\n",
		pPlayer ? pPlayer->entindex() : -1, iHostIndex, (gpGlobals->maxClients <= 1));
	if (pPlayer && pPlayer->entindex() == iHostIndex)
	{
		// don't launch the outro map clientside if we're a server
		Msg("[C] We're the host, so not doing client map outro\n");
	}
	else
	{
		// if we're a client, flag us for a reconnect once the outro is done
		ConVar *var = (ConVar *)cvar->FindVar( "asw_reconnect_after_outro" );
		if (var)
		{
			engine->ClientCmd("asw_save_server");	/// save the server we're on, so we can reconnect to it later
			var->SetValue(1);
		}		
		Msg("[C] Doing client map outro\n");
		// find the outro map name
		CASW_Campaign_Info *pCampaign = ASWGameRules() ? ASWGameRules()->GetCampaignInfo() : NULL;
		Msg("[C] Campaign debug info:\n");
		if (pCampaign)
			pCampaign->DebugInfo();
		const char *pszOutro = "outro_jacob";	 // the default
		if (pCampaign)
		{
			const char *pszCustomOutro = STRING(pCampaign->m_OutroMap);
			if (pszCustomOutro && ( !Q_strnicmp( pszCustomOutro, "outro", 5 ) ))
			{
				pszOutro = pszCustomOutro;
				Msg("[C] Using custom outro: %s\n", pszCustomOutro);
			}
			else
			{
				Msg("[C] No valid custom outro defined in campaign info, using default\n");
			}
		}
		else
		{
			Msg("[C] Using default outro as we can't find the campaign info\n");
		}
		char buffer[256];
		Q_snprintf(buffer, sizeof(buffer), "disconnect\nwait\nwait\nmaxplayers 1\nmap %s", pszOutro);
		Msg("[C] Sending client outro command: %s\n", buffer);
		engine->ClientCmd(buffer);
	}
#endif
}

// we get this message after watching the outro
static void __MsgFunc_ASWReconnectAfterOutro( bf_read &msg )
{	
	if (ASWGameRules() && ASWGameRules()->IsOutroMap())
	{
		// clear the reconnect flag
		ConVar *var = (ConVar *)cvar->FindVar( "asw_reconnect_after_outro" );
		Msg("Client checking for reconnect\n");
		if (var && var->GetInt() != 0)
		{
			var->SetValue(0);

			Msg("  sending asw_retry_saved_server cmd\n");
			// reconnect to the previous server
			engine->ClientCmd("asw_retry_saved_server");
		}
		else		// if we're not reconnecting to a server after watching the outro, go back to the main menu
		{
			if (gpGlobals->maxClients <= 1)
			{
				engine->ClientCmd("disconnect\nwait\nwait\nmap_background SwarmSelectionScreen");
			}
		}
	}
}

// we get this message when the players have lost their only tech marine and are unable to complete the mission
static void __MsgFunc_ASWTechFailure( bf_read &msg )
{
	GetClientModeASW()->m_bTechFailure = true;
	//TechMarineFailPanel::ShowTechFailPanel();
}

ClientModeASW::ClientModeASW()
{
	m_bTechFailure = false;
	m_bLaunchedBriefing = false;
	m_bLaunchedDebrief = false;
	m_bLaunchedCampaignMap = false;
	m_bLaunchedOutro = false;
	m_hLastInfoMessage = NULL;
	m_pBriefingMusic = NULL;
	m_pGameUI = NULL;
	m_bSpectator = false;
	m_fNextProgressUpdate = 0;
	m_aAwardedExperience.Purge();

	m_pCurrentPostProcessController = NULL;
	m_PostProcessLerpTimer.Invalidate();
	m_pCurrentColorCorrection = NULL;
}

ClientModeASW::~ClientModeASW()
{
}

void ClientModeASW::Init()
{
	BaseClass::Init();

	gameeventmanager->AddListener( this, "asw_mission_restart", false );
	gameeventmanager->AddListener( this, "game_newmap", false );
	HOOK_MESSAGE( ASWBlur );
	HOOK_MESSAGE( ASWCampaignCompleted );
	HOOK_MESSAGE( ASWReconnectAfterOutro );
	HOOK_MESSAGE( ASWTechFailure );
}

void ClientModeASW::Shutdown()
{
	if ( ASWBackgroundMovie() )
	{
		ASWBackgroundMovie()->ClearCurrentMovie();
	}
	DestroyASWLoadingPanel();
	if (g_hLogoPanel.Get())
	{
		delete g_hLogoPanel.Get();
	}
}

void ClientModeASW::InitViewport()
{
	m_pViewport = new CHudViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}

// asw - let us drive the fixed camera position around
static bool s_bFixedInputActive = false;
static int s_nOldCursor[2] = {0, 0};
void ASW_Handle_Fixed_Input( bool active )
{
	if ( s_bFixedInputActive ^ active )
	{
		if ( s_bFixedInputActive && !active )
		{
			// Restore mouse
			vgui::input()->SetCursorPos( s_nOldCursor[0], s_nOldCursor[1] );
		}
		else
		{
			// todO: set the initial fixed vectors here..
			vgui::input()->GetCursorPos( s_nOldCursor[0], s_nOldCursor[1] );
		}
	}

	if ( active )
	{
		float f = 0.0f;
		float s = 0.0f;
		float u = 0.0f;

		bool shiftdown = vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT );
		float movespeed = shiftdown ? 40.0f : 400.0f;

		if ( vgui::input()->IsKeyDown( KEY_W ) )
		{
			f = movespeed * gpGlobals->frametime;
		}
		if ( vgui::input()->IsKeyDown( KEY_S ) )
		{
			f = -movespeed * gpGlobals->frametime;
		}
		if ( vgui::input()->IsKeyDown( KEY_A ) )
		{
			s = -movespeed * gpGlobals->frametime;
		}
		if ( vgui::input()->IsKeyDown( KEY_D ) )
		{
			s = movespeed * gpGlobals->frametime;
		}
		if ( vgui::input()->IsKeyDown( KEY_X ) )
		{
			u = movespeed * gpGlobals->frametime;
		}
		if ( vgui::input()->IsKeyDown( KEY_Z ) )
		{
			u = -movespeed * gpGlobals->frametime;
		}

		int mx, my;
		int dx, dy;

		vgui::input()->GetCursorPos( mx, my );

		dx = mx - s_nOldCursor[0];
		dy = my - s_nOldCursor[1];

		vgui::input()->SetCursorPos( s_nOldCursor[0], s_nOldCursor[1] );

		// Convert to pitch/yaw

		float pitch = (float)dy * 0.22f;
		float yaw = -(float)dx * 0.22;

		// Apply mouse
		g_asw_ang_fixed_cam.x += pitch;

		g_asw_ang_fixed_cam.x = clamp( g_asw_ang_fixed_cam.x, -89.0f, 89.0f );

		g_asw_ang_fixed_cam.y += yaw;
		if ( g_asw_ang_fixed_cam.y > 180.0f )
		{
			g_asw_ang_fixed_cam.y -= 360.0f;
		}
		else if ( g_asw_ang_fixed_cam.y < -180.0f )
		{
			g_asw_ang_fixed_cam.y += 360.0f;
		}

		// Now apply forward, side, up

		Vector fwd, side, up;

		AngleVectors( g_asw_ang_fixed_cam, &fwd, &side, &up );

		g_asw_vec_fixed_cam += fwd * f;
		g_asw_vec_fixed_cam += side * s;
		g_asw_vec_fixed_cam += up * u;

		static Vector s_vecServerInformPos = Vector(0,0,0);
		if ((s_vecServerInformPos - g_asw_vec_fixed_cam).Length() > 100.0f)
		{
			s_vecServerInformPos = g_asw_vec_fixed_cam;
			char buffer[128];
			Q_snprintf(buffer, sizeof(buffer), "cl_freecam %d %d %d", (int) g_asw_vec_fixed_cam.x, (int) g_asw_vec_fixed_cam.y, (int) g_asw_vec_fixed_cam.z);
			engine->ClientCmd(buffer);
		}
	}

	s_bFixedInputActive = active;
}

ConVar asw_camera_shake( "asw_camera_shake", "1", FCVAR_NONE, "Enable camera shakes" );
ConVar asw_fix_cam( "asw_fix_cam", "-1", FCVAR_CHEAT, "Set to 1 to fix the camera in place." );

void ClientModeASW::OverrideView( CViewSetup *pSetup )
{
	QAngle camAngles;

	// Let the player override the view.
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if(!pPlayer)
		return;

	pPlayer->OverrideView( pSetup );

	if( ::input->CAM_IsThirdPerson() )
	{
		int omx, omy;
		ASWInput()->ASW_GetCameraLocation(pPlayer, pSetup->origin, pSetup->angles, omx, omy, true);
#ifdef CLIENT_DLL
		if ( asw_camera_shake.GetBool() )
		{
			GetViewEffects()->ApplyShake( pSetup->origin, pSetup->angles, 1.0 );
		}
#endif
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
	else	// asw: make sure we're not tilted
	{		
		pSetup->angles[ROLL] = 0;
	}

	if ( asw_fix_cam.GetInt() == 1 )
	{
		g_asw_vec_fixed_cam = pSetup->origin;
		g_asw_ang_fixed_cam = pSetup->angles;
		asw_fix_cam.SetValue( 0 );
	}
	else if ( asw_fix_cam.GetInt() == 0 )
	{
		pSetup->origin = g_asw_vec_fixed_cam;
		pSetup->angles = g_asw_ang_fixed_cam;

		ASW_Handle_Fixed_Input( vgui::input()->IsKeyDown( KEY_LSHIFT ) && vgui::input()->IsMouseDown( MOUSE_LEFT ) );
	}
}

class MapDescription_t
{
public:
	MapDescription_t( const char *szMapName, unsigned int nFileSize, unsigned int nEntityStringLength ) :
		m_szMapName( szMapName ), m_nFileSize( nFileSize ), m_nEntityStringLength( nEntityStringLength )
	{
	}
	const char *m_szMapName;
	unsigned int m_nFileSize;
	unsigned int m_nEntityStringLength;
};

static MapDescription_t s_OfficialMaps[]=
{
	MapDescription_t( "maps/asi-jac1-landingbay_01.bsp",	45793676, 412634 ),
	MapDescription_t( "maps/asi-jac1-landingbay_02.bsp",	25773204, 324141 ),
	MapDescription_t( "maps/asi-jac2-deima.bsp",			44665212, 345367 ),
	MapDescription_t( "maps/asi-jac3-rydberg.bsp",			27302616, 359228 ),
	MapDescription_t( "maps/asi-jac4-residential.bsp",		31244468, 455840 ),
	MapDescription_t( "maps/asi-jac6-sewerjunction.bsp",	18986884, 287554 ),
	MapDescription_t( "maps/asi-jac7-timorstation.bsp",		37830468, 506193 ),
};

void ClientModeASW::LevelInit( const char *newmap )
{
	// reset ambient light
	static ConVarRef mat_ambient_light_r( "mat_ambient_light_r" );
	static ConVarRef mat_ambient_light_g( "mat_ambient_light_g" );
	static ConVarRef mat_ambient_light_b( "mat_ambient_light_b" );

	if ( mat_ambient_light_r.IsValid() )
	{
		mat_ambient_light_r.SetValue( "0" );
	}

	if ( mat_ambient_light_g.IsValid() )
	{
		mat_ambient_light_g.SetValue( "0" );
	}

	if ( mat_ambient_light_b.IsValid() )
	{
		mat_ambient_light_b.SetValue( "0" );
	}

	// Load color correction lookup
	m_CCFailedHandle = g_pColorCorrectionMgr->AddColorCorrection( FAILED_CC_LOOKUP_FILENAME );
	if ( m_CCFailedHandle != INVALID_CLIENT_CCHANDLE )
	{
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCFailedHandle, 0.0f );
	}
	m_fFailedCCWeight = 0.0f;
	m_bTechFailure = false;

	m_CCInfestedHandle = g_pColorCorrectionMgr->AddColorCorrection( INFESTED_CC_LOOKUP_FILENAME );
	if ( m_CCInfestedHandle != INVALID_CLIENT_CCHANDLE )
	{
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCInfestedHandle, 0.0f );
	}
	m_fInfestedCCWeight = 0.0f;

	m_aAchievementsEarned.Purge();

	BaseClass::LevelInit(newmap);

	m_aAwardedExperience.Purge();

	// asw make sure the camera is pointing right
	if (engine->IsLevelMainMenuBackground())
	{
		::input->CAM_ToFirstPerson();
	}
	else
	{
		::input->CAM_ToThirdPerson();
		// create a black panel to fade in
		FadeInPanel *pFadeIn = dynamic_cast<FadeInPanel*>(GetViewport()->FindChildByName("FadeIn", true));
		if (pFadeIn)
		{
			//Msg("Already have a fade in panel! (m_bSlowRemove=%d m_fIngameTime=%f cur=%f)\n", pFadeIn->m_bSlowRemove, pFadeIn->m_fIngameTime, gpGlobals->curtime);
			if (UTIL_ASW_MissionHasBriefing(newmap) || (!Q_stricmp(newmap, "tutorial")))
			{
				pFadeIn->m_fIngameTime = 0;
				pFadeIn->m_bSlowRemove = false;
				pFadeIn->SetAlpha(255);
				pFadeIn->MoveToFront();
				vgui::GetAnimationController()->RunAnimationCommand(pFadeIn, "alpha", 255, 0.0f, 0.1f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			}
			else
			{
				pFadeIn->SetVisible(false);
				pFadeIn->MarkForDeletion();
			}
		}
		if (!pFadeIn && UTIL_ASW_MissionHasBriefing(newmap)) // don't create one of these if there's already one around, or in a map that doesn't have briefing
		{
			//Msg("%f: creating fadein panel\n", gpGlobals->curtime);
			new FadeInPanel(m_pViewport, "FadeIn");
		}
		else if (!pFadeIn && !Q_stricmp(newmap, "tutorial"))	// give the tutorial a nice fade in
		{
			//Msg("tutorial creating fadein panel\n");
			FadeInPanel *pTutorialFade = new FadeInPanel(m_pViewport, "FadeIn");
			if (pTutorialFade)
				pTutorialFade->AllowFastRemove();
		}
	}

	// asw: make sure no windows are left open from before
	ASW_CloseAllWindows();
	m_hMissionCompleteFrame = NULL;
	m_hCampaignFrame = NULL;
	m_hOutroFrame = NULL;
	m_hInGameBriefingFrame = NULL;
	m_bLaunchedBriefing = false;
	m_bLaunchedDebrief = false;
	m_bLaunchedCampaignMap = false;
	m_bLaunchedOutro = false;
	m_hLastInfoMessage = NULL;	

	// update keybinds shown on the HUD
	engine->ClientCmd("asw_update_binds");

	// clear any DSP effects
	CLocalPlayerFilter filter;
	enginesound->SetRoomType( filter, 0 );
	enginesound->SetPlayerDSP( filter, 0, true );

	if ( Briefing() )
	{
		Briefing()->ResetLastChatterTime();
	}

	const char *szMapName = engine->GetLevelName();
	unsigned int nFileSize = g_pFullFileSystem->Size( szMapName, "GAME" );

	unsigned int nEntityLen = Q_strlen( engine->GetMapEntitiesString() );

	m_bOfficialMap = false;
	for ( int i = 0; i < NELEMS( s_OfficialMaps ); i++ )
	{
		if ( !Q_stricmp( szMapName, s_OfficialMaps[i].m_szMapName ) )
		{
			m_bOfficialMap = ( ( s_OfficialMaps[i].m_nFileSize == nFileSize ) && ( s_OfficialMaps[i].m_nEntityStringLength == nEntityLen ) );
			break;
		}
	}

#ifdef _DEBUG
	Msg( "map %s file size is %d\n", szMapName, nFileSize );
	Msg( "    entity string is %d chars long\n", nEntityLen );
	Msg( "Official map = %d\n", m_bOfficialMap );
#endif
}

void ClientModeASW::LevelShutdown( void )
{
	BaseClass::LevelShutdown();

	// asw:shutdown all vgui windows
	ASW_CloseAllWindows();
	m_hMissionCompleteFrame = NULL;
	m_hCampaignFrame = NULL;
	m_hOutroFrame = NULL;
	m_bLaunchedBriefing = false;
	m_bLaunchedDebrief = false;
	m_bLaunchedCampaignMap = false;
	m_bLaunchedOutro = false;
	m_bTechFailure = false;
	m_hLastInfoMessage = NULL;
	m_hInGameBriefingFrame = NULL;
	m_aAwardedExperience.Purge();
	// asw make sure the cam is in third person mode, so we don't load new maps with long laggy first person view
	::input->CAM_ToThirdPerson();

	m_fNextProgressUpdate = 0;
}

void ClientModeASW::FireGameEvent( IGameEvent *event )
{
	const char *eventname = event->GetName();

	if ( Q_strcmp( "asw_mission_restart", eventname ) == 0 )
	{
		ASW_CloseAllWindows();
		C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if (pPlayer)
		{
			pPlayer->OnMissionRestart();
		}
	}
	else if ( Q_strcmp( "game_newmap", eventname ) == 0 )
	{
		engine->ClientCmd("exec newmapsettings\n");
	}

	if ( !Q_strcmp( "achievement_earned", eventname ) )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GetSplitScreenPlayerSlot() );

		CBaseHudChat *hudChat = CBaseHudChat::GetHudChat();

		if ( this == GetFullscreenClientMode() )
			return;

		int iPlayerIndex = event->GetInt( "player" );
		C_ASW_Player *pPlayer = ToASW_Player( UTIL_PlayerByIndex( iPlayerIndex ) );
		///C_ASW_Marine *pMarine = pPlayer ? pPlayer->GetMarine() : NULL;
		int iAchievement = event->GetInt( "achievement" );

		if ( pPlayer == C_ASW_Player::GetLocalASWPlayer() )
		{
			m_aAchievementsEarned.AddToTail( iAchievement );
		}
		else
		{
			pPlayer->m_aNonLocalPlayerAchievementsEarned.AddToTail();
		}

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
		BaseClass::FireGameEvent(event);
	}
}


extern vgui::DHANDLE<vgui::Frame> g_hBriefingFrame;

// Close all ASW specific VGUI windows that the player might have open
void ClientModeASW::ASW_CloseAllWindows()
{
	ASW_CloseAllWindowsFrom(GetViewport());

	g_hBriefingFrame = NULL;

	// make sure we don't have a mission chooser up
	//Msg("clientmode asw closing vote windows with asw_vote_chooser cc\n");
	engine->ClientCmd("asw_vote_chooser -1");
}

// recursive search for matching window names
void ClientModeASW::ASW_CloseAllWindowsFrom(vgui::Panel* pPanel)
{
	if (!pPanel)
		return;

	int num_names = NELEMS(s_CloseWindowNames);

	for (int k=0;k<pPanel->GetChildCount();k++)
	{
		Panel *pChild = pPanel->GetChild(k);
		if (pChild)
		{
			ASW_CloseAllWindowsFrom(pChild);
		}
	}

	// When VGUI is shutting down (i.e. if the player closes the window), GetName() can return NULL
	const char *pPanelName = pPanel->GetName();
	if ( pPanelName != NULL )
	{
		for (int i=0;i<num_names;i++)
		{
			if ( !strcmp( pPanelName, s_CloseWindowNames[i] ) )
			{
				pPanel->SetVisible(false);
				pPanel->MarkForDeletion();
			}
		}
	}
}


void ClientModeASW::StartBriefingMusic()
{
	CLocalPlayerFilter filter;
	
	if (!m_pBriefingMusic)		
	{
		m_pBriefingMusic = CSoundEnvelopeController::GetController().SoundCreate( filter, 0, "asw_song.briefing" );
	}

	if (m_pBriefingMusic)
	{
		CSoundEnvelopeController::GetController().Play( m_pBriefingMusic, 1.0, 100 );			
	}
}

void ClientModeASW::StopBriefingMusic(bool bInstantly)
{
	if (m_pBriefingMusic)
	{
		if (bInstantly)
		{
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			controller.SoundDestroy( m_pBriefingMusic );
			m_pBriefingMusic = NULL;
		}
		else
		{
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			controller.SoundFadeOut( m_pBriefingMusic, 1.0, true );
			m_pBriefingMusic = NULL;
		}
	}
}

int	ClientModeASW::KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( engine->Con_IsVisible() )
		return 1;

	// annoyingly we can't intercept ESC here, it'll still bring up the gameui stuff
	//  but we'll use it for a couple of things anyway
	if (keynum == KEY_ESCAPE)
	{
		// ESC can be used up to close an info message
		if (CASW_VGUI_Info_Message::CloseInfoMessage())
			return 0;

		// or closing the F1 panel
		if (GetViewport())
		{
			vgui::Panel *pPanel = GetViewport()->FindChildByName("g_PlayerListFrame", true);
			if (pPanel)	
			{
				pPanel->SetVisible(false);
				pPanel->MarkForDeletion();
				return 0;
			}
		}

		vgui::Panel *pContainer = GetClientMode()->GetViewport()->FindChildByName("InGameBriefingContainer", true);
		if (pContainer)	
		{
			pContainer->SetVisible(false);
			pContainer->MarkForDeletion();
			pContainer = NULL;
			return 0;
		}
	}

	return BaseClass::KeyInput(down, keynum, pszCurrentBinding);
}

void asw_set_hear_pos_cc()
{
	g_asw_vec_fixed_hear_pos = g_asw_vec_last_hear_pos;
}
ConCommand asw_set_hear_pos( "asw_set_hear_pos", asw_set_hear_pos_cc, "Sets fixed audio position to the current position", FCVAR_CHEAT );

void ClientModeASW::OverrideAudioState( AudioState_t *pAudioState )
{
	// lower hearing origin since cam is so far from the action
	Vector hear_pos = pAudioState->m_Origin;
	if (::input->CAM_IsThirdPerson())
	{
		// put audio position on our current marine if we have one selected
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		CASW_Marine *pMarine = NULL;
		if ( pPlayer && ( asw_hear_from_marine.GetBool() || asw_hear_height.GetFloat() != 0 ) )
		{
			pMarine = pPlayer->GetSpectatingMarine();
			if ( !pMarine )
			{
				pMarine = pPlayer->GetMarine();
			}
		}
		
		if ( pMarine )
		{
			if ( asw_hear_height.GetFloat() > 0 )
			{
				Vector vecForward;
				AngleVectors( pAudioState->m_Angles, &vecForward );
				pAudioState->m_Origin = pMarine->EyePosition() - vecForward * asw_hear_height.GetFloat();
			}
			else if ( asw_hear_height.GetFloat() < 0 )
			{
				Vector vecForward;
				AngleVectors( pAudioState->m_Angles, &vecForward );
				pAudioState->m_Origin -= (vecForward * asw_hear_height.GetFloat() );
			}
		}
		else
		{
			Vector vecForward;
			AngleVectors( pAudioState->m_Angles, &vecForward );
			pAudioState->m_Origin += (vecForward * 412.0f * 0.7f);	// hardcoded 412 is bad
		}
	}

	g_asw_vec_last_hear_pos = pAudioState->m_Origin;
	if ( asw_hear_fixed.GetBool() )
	{
		pAudioState->m_Origin = g_asw_vec_fixed_hear_pos;
	}

	if ( asw_hear_pos_debug.GetBool() )
	{
		float flBaseSize = 10;
		float flHeight = 80;
		Vector vForward, vRight, vUp;
		AngleVectors( pAudioState->m_Angles, &vForward, &vRight, &vUp );
		debugoverlay->AddTriangleOverlay( pAudioState->m_Origin+vRight*flBaseSize/2, pAudioState->m_Origin-vRight*flBaseSize/2, pAudioState->m_Origin+vForward*flHeight, 255, 255, 0, 255, false, 0.1 );
		debugoverlay->AddTriangleOverlay( pAudioState->m_Origin+vForward*flHeight, pAudioState->m_Origin-vRight*flBaseSize/2, pAudioState->m_Origin+vRight*flBaseSize/2, 255, 255, 0, 255, false, 0.1 );
		NDebugOverlay::Box( pAudioState->m_Origin, -Vector( 3, 3, 3 ), Vector( 3, 3, 3 ), 255, 64, 0, 255, 0.1f );
	}
}

void ClientModeASW::Update( void )
{
	UpdatePostProcessingEffects();

	engine->SetMouseWindowLock( ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_INGAME && !enginevgui->IsGameUIVisible() );

#ifndef _X360

#endif
}

void ClientModeASW::DoPostScreenSpaceEffects( const CViewSetup *pSetup )
{
	CMatRenderContextPtr pRenderContext( materials );

	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	bool bSniperScope = ( pPlayer != NULL ) && ( pPlayer->IsSniperScopeActive() );

	extern bool g_bRenderingGlows;
	
	if ( bSniperScope )
	{
		DrawSniperScopeStencilMask();
	}

	if ( mat_object_motion_blur_enable.GetBool() )
	{
		DoObjectMotionBlur( pSetup );
	}
	
	// Render object glows and selectively-bloomed objects (under sniper scope)
	g_bRenderingGlows = true;
	g_GlowObjectManager.RenderGlowEffects( pSetup, GetSplitScreenPlayerSlot() );
	g_bRenderingGlows = false;

	if ( bSniperScope )
	{
		// Update full frame FB 0 with result of glow pass so it can be used by sniper scope
		ITexture *pFullFrameFB = materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
		pRenderContext->CopyRenderTargetToTexture( pFullFrameFB );
	}
}

void ClientModeASW::OnColorCorrectionWeightsReset( void )
{
	C_ColorCorrection *pNewColorCorrection = NULL;
	C_ColorCorrection *pOldColorCorrection = m_pCurrentColorCorrection;
	CASW_Player *pPlayer = CASW_Player::GetLocalASWPlayer();
	if ( pPlayer )
	{
		pNewColorCorrection = pPlayer->GetActiveColorCorrection();
	}

	if ( m_CCFailedHandle != INVALID_CLIENT_CCHANDLE && ASWGameRules() )
	{
		m_fFailedCCWeight = Approach( TechMarineFailPanel::s_pTechPanel ? 1.0f : 0.0f, m_fFailedCCWeight, gpGlobals->frametime * ( 1.0f / FAILED_CC_FADE_TIME ) );
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCFailedHandle, m_fFailedCCWeight );

		// If the mission was failed due to a dead tech, disable the environmental color correction in favor of the mission failed color correction
		if ( m_fFailedCCWeight != 0.0f && m_pCurrentColorCorrection )
		{
			m_pCurrentColorCorrection->EnableOnClient( false );
			m_pCurrentColorCorrection = NULL;
		}
	}

	if ( m_CCInfestedHandle != INVALID_CLIENT_CCHANDLE && ASWGameRules() )
	{
		C_ASW_Marine *pMarine = C_ASW_Marine::GetLocalMarine();
		m_fInfestedCCWeight = Approach( pMarine && pMarine->IsInfested() ? 1.0f : 0.0f, m_fInfestedCCWeight, gpGlobals->frametime * ( 1.0f / INFESTED_CC_FADE_TIME ) );
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCInfestedHandle, m_fInfestedCCWeight );

		// If the mission was failed due to a dead tech, disable the environmental color correction in favor of the mission failed color correction
		if ( m_fInfestedCCWeight != 0.0f && m_pCurrentColorCorrection )
		{
			m_pCurrentColorCorrection->EnableOnClient( false );
			m_pCurrentColorCorrection = NULL;
		}
	}

	// Only blend between environmental color corrections if there is no failure/infested-induced color correction
	if ( pNewColorCorrection != pOldColorCorrection && m_fFailedCCWeight == 0.0f && m_fInfestedCCWeight == 0.0f )
	{
		if ( pOldColorCorrection )
		{
			pOldColorCorrection->EnableOnClient( false );
		}
		if ( pNewColorCorrection )
		{
			pNewColorCorrection->EnableOnClient( true, pOldColorCorrection == NULL );
		}
		m_pCurrentColorCorrection = pNewColorCorrection;
	}
}

void ClientModeASW::DrawSniperScopeStencilMask()
{
	extern ConVar asw_sniper_scope_radius;
	CMatRenderContextPtr pRenderContext( materials );
	PIXEvent myEvent( pRenderContext, "SniperScopeStencil" );

	int nCursorX, nCursorY;
	ASWInput()->GetSimulatedFullscreenMousePos( &nCursorX, &nCursorY );

	const int NUM_CIRCLE_POINTS = 40;
	Vector2D vScreenSpaceCoordinates[ NUM_CIRCLE_POINTS ];

	int nScreenWidth, nScreenHeight;
	pRenderContext->GetRenderTargetDimensions( nScreenWidth, nScreenHeight );
	Vector2D vScreenSize( nScreenWidth, nScreenHeight );

	// Clear the screen stencil value to 1 to inhibit glows everywhere except in the sniper scope circle, where it will be set to 0
	pRenderContext->ClearStencilBufferRectangle( 0, 0, nScreenWidth, nScreenHeight, 1 );
	float width = asw_sniper_scope_radius.GetFloat() * ( float ) nScreenHeight / 480.0f;
	float height = asw_sniper_scope_radius.GetFloat() * ( float ) nScreenHeight / 480.0f;
	for ( int i = 0; i < NUM_CIRCLE_POINTS; i++ )
	{
		float flAngle = 2.0f * M_PI * ( (float) i / (float) NUM_CIRCLE_POINTS );
		vScreenSpaceCoordinates[i] = Vector2D( nCursorX + width * cos( flAngle ), nCursorY + height * sin( flAngle ) );
		vScreenSpaceCoordinates[i].x = vScreenSpaceCoordinates[i].x / vScreenSize.x * 2.0f - 1.0f;
		vScreenSpaceCoordinates[i].y = 1.0f - vScreenSpaceCoordinates[i].y / vScreenSize.y * 2.0f;
	}		

	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 0;
	stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
	stencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
	stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
	stencilState.m_ZFailOp = SHADER_STENCILOP_SET_TO_REFERENCE;
	pRenderContext->SetStencilState( stencilState );
	
	pRenderContext->OverrideDepthEnable( true, false );
	IMaterial *pMaterial = materials->FindMaterial( "engine/writestencil", TEXTURE_GROUP_OTHER, true );
	DrawNDCSpaceUntexturedPolygon( pMaterial, NUM_CIRCLE_POINTS, vScreenSpaceCoordinates, NULL );
	pRenderContext->OverrideDepthEnable( false, false );
}

void ClientModeASW::DoObjectMotionBlur( const CViewSetup *pSetup )
{
	if ( g_ObjectMotionBlurManager.GetDrawableObjectCount() <= 0 )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	ITexture *pFullFrameFB1 = materials->FindTexture( "_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET );

	//
	// Render Velocities into a full-frame FB1
	//
	IMaterial *pGlowColorMaterial = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );
	
	pRenderContext->PushRenderTargetAndViewport();
	pRenderContext->SetRenderTarget( pFullFrameFB1 );
	pRenderContext->Viewport( 0, 0, pSetup->width, pSetup->height );

	// Red and Green are x- and y- screen-space velocities biased and packed into the [0,1] range.
	// A value of 127 gets mapped to 0, a value of 0 gets mapped to -1, and a value of 255 gets mapped to 1.
	//
	// Blue is set to 1 within the object's bounds and 0 outside, and is used as a mask to ensure that
	// motion blur samples only pull from the core object itself and not surrounding pixels (even though
	// the area being blurred is larger than the core object).
	//
	// Alpha is not used
	pRenderContext->ClearColor4ub( 127, 127, 0, 0 );
	// Clear only color, not depth & stencil
	pRenderContext->ClearBuffers( true, false, false );

	// Save off state
	Vector vOrigColor;
	render->GetColorModulation( vOrigColor.Base() );

	// Use a solid-color unlit material to render velocity into the buffer
	g_pStudioRender->ForcedMaterialOverride( pGlowColorMaterial );
	g_ObjectMotionBlurManager.DrawObjects();	
	g_pStudioRender->ForcedMaterialOverride( NULL );

	render->SetColorModulation( vOrigColor.Base() );
	
	pRenderContext->PopRenderTargetAndViewport();

	//
	// Render full-screen pass
	//
	IMaterial *pMotionBlurMaterial;
	IMaterialVar *pFBTextureVariable;
	IMaterialVar *pVelocityTextureVariable;
	bool bFound1 = false, bFound2 = false;

	// Make sure our render target of choice has the results of the engine post-process pass
	ITexture *pFullFrameFB = materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
	pRenderContext->CopyRenderTargetToTexture( pFullFrameFB );

	pMotionBlurMaterial = materials->FindMaterial( "effects/object_motion_blur", TEXTURE_GROUP_OTHER, true );
	pFBTextureVariable = pMotionBlurMaterial->FindVar( "$fb_texture", &bFound1, true );
	pVelocityTextureVariable = pMotionBlurMaterial->FindVar( "$velocity_texture", &bFound2, true );
	if ( bFound1 && bFound2 )
	{
		pFBTextureVariable->SetTextureValue( pFullFrameFB );
		
		pVelocityTextureVariable->SetTextureValue( pFullFrameFB1 );

		int nWidth, nHeight;
		pRenderContext->GetRenderTargetDimensions( nWidth, nHeight );

		pRenderContext->DrawScreenSpaceRectangle( pMotionBlurMaterial, 0, 0, nWidth, nHeight, 0.0f, 0.0f, nWidth - 1, nHeight - 1, nWidth, nHeight );
	}
}

void ClientModeASW::UpdatePostProcessingEffects()
{
	C_PostProcessController *pNewPostProcessController = NULL;
	CASW_Player *pPlayer = CASW_Player::GetLocalASWPlayer();
	if ( pPlayer )
	{
		pNewPostProcessController = pPlayer->GetActivePostProcessController();
	}

	// Figure out new endpoints for parameter lerping
	if ( pNewPostProcessController != m_pCurrentPostProcessController )
	{
		m_LerpStartPostProcessParameters = m_CurrentPostProcessParameters;
		m_LerpEndPostProcessParameters = pNewPostProcessController ? pNewPostProcessController->m_PostProcessParameters : PostProcessParameters_t();
		m_pCurrentPostProcessController = pNewPostProcessController;

		float flFadeTime = pNewPostProcessController ? pNewPostProcessController->m_PostProcessParameters.m_flParameters[ PPPN_FADE_TIME ] : 0.0f;
		if ( flFadeTime <= 0.0f )
		{
			flFadeTime = 0.001f;
		}
		m_PostProcessLerpTimer.Start( flFadeTime );
	}

	// Lerp between start and end
	float flLerpFactor = 1.0f - m_PostProcessLerpTimer.GetRemainingRatio();
	for ( int nParameter = 0; nParameter < POST_PROCESS_PARAMETER_COUNT; ++ nParameter )
	{
		m_CurrentPostProcessParameters.m_flParameters[ nParameter ] = 
			Lerp( 
				flLerpFactor, 
				m_LerpStartPostProcessParameters.m_flParameters[ nParameter ], 
				m_LerpEndPostProcessParameters.m_flParameters[ nParameter ] );
	}
	SetPostProcessParams( &m_CurrentPostProcessParameters );
}

bool ClientModeASW::HasAwardedExperience( C_ASW_Player *pPlayer )
{
	return ( pPlayer && m_aAwardedExperience.Find( pPlayer->entindex() ) != m_aAwardedExperience.InvalidIndex() );
}

void ClientModeASW::SetAwardedExperience( C_ASW_Player *pPlayer )
{
	if ( !pPlayer )
		return;

	m_aAwardedExperience.AddToTail( pPlayer->entindex() );
}

void ClientModeASW::ToggleMessageMode()
{
	if ( m_pChatElement->GetMessageMode() != MM_NONE )
	{
		m_pChatElement->StopMessageMode();
	}
	else
	{
		m_pChatElement->StartMessageMode( MM_SAY );
	}
}

// BroadcastPatchAudio() - same as BroadcastAudio(), but play using a static array of CSoundPatch so we can 
// fade, change pitch, etc
static const int MAX_BROADCAST_SOUND_PATCHES = 16;
static CSoundPatch *s_pBroadcastSoundPatches[ MAX_BROADCAST_SOUND_PATCHES ];

static int FindFreeSoundPatchIndex()
{
	for( int i = 0; i < MAX_BROADCAST_SOUND_PATCHES; i++ )
	{
		if ( s_pBroadcastSoundPatches[ i ] == NULL )
		{
			return i;
		}
		if ( !CSoundEnvelopeController::GetController().SoundIsStillPlaying( s_pBroadcastSoundPatches[ i ] ) )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( s_pBroadcastSoundPatches[ i ] );
			s_pBroadcastSoundPatches[ i ] = NULL;
			return i;
		}
	}
	return -1;
}

static int FindSoundPatchIndex( const char *szSoundScriptName ) 
{
	for( int i = 0; i < MAX_BROADCAST_SOUND_PATCHES; i++ )
	{
		if ( s_pBroadcastSoundPatches[ i ] == NULL )
		{
			continue;
		}
		if ( !CSoundEnvelopeController::GetController().SoundIsStillPlaying( s_pBroadcastSoundPatches[ i ] ) )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( s_pBroadcastSoundPatches[ i ] );
			s_pBroadcastSoundPatches[ i ] = NULL;
			continue;
		}

		if( !Q_stricmp( CSoundEnvelopeController::GetController().SoundGetScriptName( s_pBroadcastSoundPatches[ i ] ), szSoundScriptName ) )
		{
			return i;
		}
	}
	return -1;
}

void __MsgFunc_BroadcastPatchAudio( bf_read &msg )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );

	int nSoundPatchIndex = FindFreeSoundPatchIndex();

	char szString[2048];
	msg.ReadString( szString, sizeof(szString) );
	
	if ( nSoundPatchIndex < 0 )
	{
		Warning( "%s(): Out of broadcast sound patches (MAX_BROADCAST_SOUND_PATCHES = %d)\n", __FUNCTION__, MAX_BROADCAST_SOUND_PATCHES );
		return;
	}

	CLocalPlayerFilter filter;
	s_pBroadcastSoundPatches[ nSoundPatchIndex ] = CSoundEnvelopeController::GetController().SoundCreate( filter, SOUND_FROM_WORLD, szString );
	CSoundEnvelopeController::GetController().Play( s_pBroadcastSoundPatches[ nSoundPatchIndex ], 1.0, 100 );
}
USER_MESSAGE_REGISTER( BroadcastPatchAudio );

void __MsgFunc_BroadcastStopPatchAudio( bf_read &msg )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );

	char szString[2048];
	msg.ReadString( szString, sizeof(szString) );

	int nSoundPatchIndex = FindSoundPatchIndex( szString );

	if ( nSoundPatchIndex == -1 )
	{
		Warning( "%s(): Cannot find broadcast patch %s to stop\n", __FUNCTION__, szString );
		return;
	}

	float flFadeOutTime = msg.ReadFloat();

	if ( flFadeOutTime == 0.0 )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( s_pBroadcastSoundPatches[ nSoundPatchIndex ] );
		s_pBroadcastSoundPatches[ nSoundPatchIndex ] = NULL;
	}
	else
	{
		CSoundEnvelopeController::GetController().SoundFadeOut( s_pBroadcastSoundPatches[ nSoundPatchIndex ], flFadeOutTime );
	}
}
USER_MESSAGE_REGISTER( BroadcastStopPatchAudio );

void __MsgFunc_BroadcastAudio( bf_read &msg )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );

	char szString[2048];
	msg.ReadString( szString, sizeof(szString) );

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, szString );
}
USER_MESSAGE_REGISTER( BroadcastAudio );

void __MsgFunc_BroadcastStopAudio( bf_read &msg )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );

	char szString[2048];
	msg.ReadString( szString, sizeof(szString) );

	C_BaseEntity::StopSound( SOUND_FROM_LOCAL_PLAYER, szString );
}
USER_MESSAGE_REGISTER( BroadcastStopAudio );