//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
//
//===============================================================================

#include "cbase.h"
#include <crtmemdebug.h>
#include "vgui_int.h"
#include "clientmode.h"
#include "iinput.h"
#include "iviewrender.h"
#include "ivieweffects.h"
#include "ivmodemanager.h"
#include "prediction.h"
#include "clientsideeffects.h"
#include "particlemgr.h"
#include "steam/steam_api.h"
#include "smoke_fog_overlay.h"
#include "view.h"
#include "ienginevgui.h"
#include "iefx.h"
#include "enginesprite.h"
#include "networkstringtable_clientdll.h"
#include "voice_status.h"
#include "FileSystem.h"
#include "c_te_legacytempents.h"
#include "c_rope.h"
#include "engine/IShadowMgr.h"
#include "engine/IStaticPropMgr.h"
#include "hud_basechat.h"
#include "hud_crosshair.h"
#include "view_shared.h"
#include "env_wind_shared.h"
#include "detailobjectsystem.h"
#include "soundEnvelope.h"
#include "c_basetempentity.h"
#include "materialsystem/imaterialsystemstub.h"
#include "vguimatsurface/IMatSystemSurface.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "c_soundscape.h"
#include "engine/IVDebugOverlay.h"
#include "vguicenterprint.h"
#include "iviewrender_beams.h"
#include "tier0/vprof.h"
#include "engine/IEngineTrace.h"
#include "engine/ivmodelinfo.h"
#include "physics.h"
#include "usermessages.h"
#include "gamestringpool.h"
#include "c_user_message_register.h"
#include "igameuifuncs.h"
#include "saverestoretypes.h"
#include "saverestore.h"
#include "physics_saverestore.h"
#include "igameevents.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "kbutton.h"
#include "tier0/icommandline.h"
#include "vstdlib/jobthread.h"
#include "gamerules_register.h"
#include "game/client/iviewport.h"
#include "vgui_controls/AnimationController.h"
#include "bitmap/tgawriter.h"
#include "c_world.h"
#include "perfvisualbenchmark.h"	
#include "soundemittersystem/isoundemittersystembase.h"
#include "hud_closecaption.h"
#include "colorcorrectionmgr.h"
#include "physpropclientside.h"
#include "panelmetaclassmgr.h"
#include "c_vguiscreen.h"
#include "imessagechars.h"
#include "game/client/IGameClientExports.h"
#include "client_factorylist.h"
#include "ragdoll_shared.h"
#include "rendertexture.h"
#include "view_scene.h"
#include "iclientmode.h"
#include "con_nprint.h"
#include "inputsystem/iinputsystem.h"
#include "appframework/IAppSystemGroup.h"
#include "scenefilecache/ISceneFileCache.h"
#include "tier3/tier3.h"
#include "avi/iavi.h"
#include "ihudlcd.h"
#include "toolframework_client.h"
#include "hltvcamera.h"
#if defined( REPLAY_ENABLED )
#include "replaycamera.h"
#include "replay_ragdoll.h"
#include "replay_ragdoll.h"
#include "qlimits.h"
#include "engine/ireplayhistorymanager.h"
#endif
#include "ixboxsystem.h"
#include "matchmaking/imatchframework.h"
#include "cdll_bounded_cvars.h"
#include "matsys_controls/matsyscontrols.h"
#include "GameStats.h"
#include "videocfg/videocfg.h"
#include "tier2/tier2_logging.h"
#include "vscript/ivscript.h"
#include "activitylist.h"
#include "eventlist.h"
#ifdef GAMEUI_UISYSTEM2_ENABLED
#include "gameui.h"
#endif
#ifdef GAMEUI_EMBEDDED

#if defined( SWARM_DLL )
#include "swarm/gameui/swarm/basemodpanel.h"
#else
#error "GAMEUI_EMBEDDED"
#endif
#endif

#ifdef DEMOPOLISH_ENABLED
#include "demo_polish/demo_polish.h"
#endif

#include "imaterialproxydict.h"
#include "tier0/miniprofiler.h" 
#include "../../engine/iblackbox.h"
#include "c_rumble.h"
#include "viewpostprocess.h"



#ifdef INFESTED_PARTICLES
#include "c_asw_generic_emitter.h"
#endif

#ifdef INFESTED_DLL
#include "missionchooser/iasw_mission_chooser.h"

#endif

#include "tier1/UtlDict.h"
#include "keybindinglistener.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IClientMode *GetClientModeNormal();

// IF YOU ADD AN INTERFACE, EXTERN IT IN THE HEADER FILE.
IVEngineClient	*engine = NULL;
IVModelRender *modelrender = NULL;
IVEfx *effects = NULL;
IVRenderView *render = NULL;
IVDebugOverlay *debugoverlay = NULL;
IMaterialSystemStub *materials_stub = NULL;
IDataCache *datacache = NULL;
IVModelInfoClient *modelinfo = NULL;
IEngineVGui *enginevgui = NULL;
INetworkStringTableContainer *networkstringtable = NULL;
ISpatialPartition* partition = NULL;
IFileSystem *filesystem = NULL;
IShadowMgr *shadowmgr = NULL;
IStaticPropMgrClient *staticpropmgr = NULL;
IEngineSound *enginesound = NULL;
IUniformRandomStream *random = NULL;
static CGaussianRandomStream s_GaussianRandomStream;
CGaussianRandomStream *randomgaussian = &s_GaussianRandomStream;
ISharedGameRules *sharedgamerules = NULL;
IEngineTrace *enginetrace = NULL;
IFileLoggingListener *filelogginglistener = NULL;
IGameUIFuncs *gameuifuncs = NULL;
IGameEventManager2 *gameeventmanager = NULL;
ISoundEmitterSystemBase *soundemitterbase = NULL;
IInputSystem *inputsystem = NULL;
ISceneFileCache *scenefilecache = NULL;
IXboxSystem *xboxsystem = NULL;	// Xbox 360 only
IAvi *avi = NULL;
IBik *bik = NULL;
IUploadGameStats *gamestatsuploader = NULL;
IBlackBox *blackboxrecorder = NULL;
#ifdef INFESTED_DLL
IASW_Mission_Chooser *missionchooser = NULL;
#endif
#if defined( REPLAY_ENABLED )
IReplayHistoryManager *g_pReplayHistoryManager = NULL;
#endif

IScriptManager *scriptmanager = NULL;

IGameSystem *SoundEmitterSystem();
IGameSystem *ToolFrameworkClientSystem();
IViewRender *GetViewRenderInstance();

static CSteamAPIContext g_SteamAPIContext;
CSteamAPIContext *steamapicontext = &g_SteamAPIContext;


bool g_bEngineIsHLTV = false;

static bool g_bRequestCacheUsedMaterials = false;
void RequestCacheUsedMaterials()
{
	g_bRequestCacheUsedMaterials = true;
}

void ProcessCacheUsedMaterials()
{
	if ( !g_bRequestCacheUsedMaterials )
		return;

	g_bRequestCacheUsedMaterials = false;
	if ( materials )
	{
        materials->CacheUsedMaterials();
	}
}

static bool g_bHeadTrackingEnabled = false;

bool IsHeadTrackingEnabled()
{
#if defined( HL2_CLIENT_DLL )
	return g_bHeadTrackingEnabled;
#else
	return false;
#endif
}

void VGui_ClearVideoPanels();

// String tables
INetworkStringTable *g_pStringTableParticleEffectNames = NULL;
INetworkStringTable *g_pStringTableExtraParticleFiles = NULL;
INetworkStringTable *g_StringTableEffectDispatch = NULL;
INetworkStringTable *g_StringTableVguiScreen = NULL;
INetworkStringTable *g_pStringTableMaterials = NULL;
INetworkStringTable *g_pStringTableInfoPanel = NULL;
INetworkStringTable *g_pStringTableClientSideChoreoScenes = NULL;

static CGlobalVarsBase dummyvars( true );
// So stuff that might reference gpGlobals during DLL initialization won't have a NULL pointer.
// Once the engine calls Init on this DLL, this pointer gets assigned to the shared data in the engine
CGlobalVarsBase *gpGlobals = &dummyvars;
class CHudChat;
class CViewRender;

static C_BaseEntityClassList *s_pClassLists = NULL;
C_BaseEntityClassList::C_BaseEntityClassList()
{
	m_pNextClassList = s_pClassLists;
	s_pClassLists = this;
}
C_BaseEntityClassList::~C_BaseEntityClassList()
{
}

// Any entities that want an OnDataChanged during simulation register for it here.
class CDataChangedEvent
{
public:
	CDataChangedEvent() {}
	CDataChangedEvent( IClientNetworkable *ent, DataUpdateType_t updateType, int *pStoredEvent )
	{
		m_pEntity = ent;
		m_UpdateType = updateType;
		m_pStoredEvent = pStoredEvent;
	}

	IClientNetworkable	*m_pEntity;
	DataUpdateType_t	m_UpdateType;
	int					*m_pStoredEvent;
};

ISaveRestoreBlockHandler *GetEntitySaveRestoreBlockHandler();
ISaveRestoreBlockHandler *GetViewEffectsRestoreBlockHandler();

CUtlLinkedList<CDataChangedEvent, unsigned short> g_DataChangedEvents;
ClientFrameStage_t g_CurFrameStage = FRAME_UNDEFINED;


class IMoveHelper;

void DispatchHudText( const char *pszName );

static ConVar s_CV_ShowParticleCounts("showparticlecounts", "0", 0, "Display number of particles drawn per frame");
static ConVar s_cl_team("cl_team", "default", FCVAR_USERINFO|FCVAR_ARCHIVE, "Default team when joining a game");
static ConVar s_cl_class("cl_class", "default", FCVAR_USERINFO|FCVAR_ARCHIVE, "Default class when joining a game");

// Physics system
bool g_bLevelInitialized;
bool g_bTextMode = false;

static ConVar *g_pcv_ThreadMode = NULL;

// implements ACTIVE_SPLITSCREEN_PLAYER_GUARD (cdll_client_int.h)
CSetActiveSplitScreenPlayerGuard::CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, int slot, int nOldSlot, bool bSetVguiScreenSize ) :
	CVGuiScreenSizeSplitScreenPlayerGuard( bSetVguiScreenSize, slot, nOldSlot )
{
	if ( nOldSlot == slot && engine->IsLocalPlayerResolvable() )
	{
		m_bChanged = false;
		return;
	}

	m_bChanged = true;
	m_pchContext = pchContext;
	m_nLine = nLine;
	m_nSaveSlot = engine->SetActiveSplitScreenPlayerSlot( slot >= 0 ? slot : 0 );
	m_bSaveGetLocalPlayerAllowed = engine->SetLocalPlayerIsResolvable( pchContext, nLine, slot >= 0 );
}

CSetActiveSplitScreenPlayerGuard::CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, C_BaseEntity *pEntity, int nOldSlot, bool bSetVguiScreenSize ) :
	CVGuiScreenSizeSplitScreenPlayerGuard( bSetVguiScreenSize, pEntity, nOldSlot )
{
	int slot = C_BasePlayer::GetSplitScreenSlotForPlayer( pEntity );
	if ( slot == -1 )
	{
		m_bChanged = false;
		return;
	}

	if ( nOldSlot == slot && engine->IsLocalPlayerResolvable())
	{
		m_bChanged = false;
		return;
	}

	m_bChanged = true;
	m_pchContext = pchContext;
	m_nLine = nLine;
	m_nSaveSlot = engine->SetActiveSplitScreenPlayerSlot( slot >= 0 ? slot : 0 );
	m_bSaveGetLocalPlayerAllowed = engine->SetLocalPlayerIsResolvable( pchContext, nLine, slot >= 0 );
}


CSetActiveSplitScreenPlayerGuard::~CSetActiveSplitScreenPlayerGuard()
{
	if ( !m_bChanged )
		return;

	engine->SetActiveSplitScreenPlayerSlot( m_nSaveSlot );
	engine->SetLocalPlayerIsResolvable( m_pchContext, m_nLine, m_bSaveGetLocalPlayerAllowed );
}

static CUtlRBTree< const char *, int > g_Hacks( 0, 0, DefLessFunc( char const * ) );

CON_COMMAND( cl_dumpsplithacks, "Dump split screen workarounds." )
{
	for ( int i = g_Hacks.FirstInorder(); i != g_Hacks.InvalidIndex(); i = g_Hacks.NextInorder( i ) )
	{
		Msg( "%s\n", g_Hacks[ i ] );
	}
}

CHackForGetLocalPlayerAccessAllowedGuard::CHackForGetLocalPlayerAccessAllowedGuard( char const *pszContext, bool bOldState )
{
	if ( bOldState )
	{
		m_bChanged = false;
		return;
	}

	m_bChanged = true;
	m_pszContext = pszContext;
	if ( g_Hacks.Find( pszContext ) == g_Hacks.InvalidIndex() )
	{
		g_Hacks.Insert( pszContext );
	}
	m_bSaveGetLocalPlayerAllowed = engine->SetLocalPlayerIsResolvable( pszContext, 0, true );
}

CHackForGetLocalPlayerAccessAllowedGuard::~CHackForGetLocalPlayerAccessAllowedGuard()
{
	if ( !m_bChanged )
		return;
	engine->SetLocalPlayerIsResolvable( m_pszContext, 0, m_bSaveGetLocalPlayerAllowed );
}

CVGuiScreenSizeSplitScreenPlayerGuard::CVGuiScreenSizeSplitScreenPlayerGuard( bool bActive, int slot, int nOldSlot )
{
	if ( !bActive )
	{
		m_bNoRestore = true;
		return;
	}

	if ( vgui::surface()->IsScreenSizeOverrideActive() && nOldSlot == slot && engine->IsLocalPlayerResolvable() )
	{
		m_bNoRestore = true;
		return;
	}

	m_bNoRestore = false;
	vgui::surface()->GetScreenSize( m_nOldSize[ 0 ], m_nOldSize[ 1 ] );
	int x, y, w, h;
	VGui_GetHudBounds( slot >= 0 ? slot : 0, x, y, w, h );
	m_bOldSetting = vgui::surface()->ForceScreenSizeOverride( true, w, h );
}

CVGuiScreenSizeSplitScreenPlayerGuard::CVGuiScreenSizeSplitScreenPlayerGuard( bool bActive, C_BaseEntity *pEntity, int nOldSlot )
{
	if ( !bActive )
	{
		m_bNoRestore = true;
		return;
	}

	int slot = C_BasePlayer::GetSplitScreenSlotForPlayer( pEntity );
	if ( vgui::surface()->IsScreenSizeOverrideActive() && nOldSlot == slot && engine->IsLocalPlayerResolvable() )
	{
		m_bNoRestore = true;
		return;
	}

	m_bNoRestore = false;
	vgui::surface()->GetScreenSize( m_nOldSize[ 0 ], m_nOldSize[ 1 ] );
	// Get size for this user
	int x, y, w, h;
	VGui_GetHudBounds( slot >= 0 ? slot : 0, x, y, w, h );
	m_bOldSetting = vgui::surface()->ForceScreenSizeOverride( true, w, h );
}

CVGuiScreenSizeSplitScreenPlayerGuard::~CVGuiScreenSizeSplitScreenPlayerGuard()
{
	if ( m_bNoRestore )
		return;
	vgui::surface()->ForceScreenSizeOverride( m_bOldSetting, m_nOldSize[ 0 ], m_nOldSize[ 1 ] );
}

CVGuiAbsPosSplitScreenPlayerGuard::CVGuiAbsPosSplitScreenPlayerGuard( int slot, int nOldSlot, bool bInvert /*=false*/ )
{
	if ( nOldSlot == slot && engine->IsLocalPlayerResolvable() && vgui::surface()->IsScreenPosOverrideActive() )
	{
		m_bNoRestore = true;
		return;
	}

	m_bNoRestore = false;

	// Get size for this user
	int x, y, w, h;
	VGui_GetHudBounds( slot, x, y, w, h );
	if ( bInvert )
	{
		x = -x;
		y = -y;
	}

	vgui::surface()->ForceScreenPosOffset( true, x, y );
}

CVGuiAbsPosSplitScreenPlayerGuard::~CVGuiAbsPosSplitScreenPlayerGuard()
{
	if ( m_bNoRestore )
		return;
	vgui::surface()->ForceScreenPosOffset( false, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: interface for gameui to modify voice bans
//-----------------------------------------------------------------------------
class CGameClientExports : public IGameClientExports
{
public:
	// ingame voice manipulation
	bool IsPlayerGameVoiceMuted(int playerIndex)
	{
		return GetClientVoiceMgr()->IsPlayerBlocked(playerIndex);
	}

	void MutePlayerGameVoice(int playerIndex)
	{
		GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, true);
	}

	void UnmutePlayerGameVoice(int playerIndex)
	{
		GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, false);
	}
	

	void OnGameUIActivated( void )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "gameui_activated" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}
	}

	void OnGameUIHidden( void )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "gameui_hidden" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}
	}

	// if true, the gameui applies the blur effect
	bool ClientWantsBlurEffect( void )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
		if ( GetViewPortInterface()->GetActivePanel() && GetViewPortInterface()->GetActivePanel()->WantsBackgroundBlurred() )
			return true;

		return false;
	}
};

EXPOSE_SINGLE_INTERFACE( CGameClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION );

class CClientDLLSharedAppSystems : public IClientDLLSharedAppSystems
{
public:
	CClientDLLSharedAppSystems()
	{
		AddAppSystem( "soundemittersystem", SOUNDEMITTERSYSTEM_INTERFACE_VERSION );
		AddAppSystem( "scenefilecache", SCENE_FILE_CACHE_INTERFACE_VERSION );
#ifdef GAMEUI_UISYSTEM2_ENABLED
		AddAppSystem( "client", GAMEUISYSTEMMGR_INTERFACE_VERSION );
#endif
#ifdef INFESTED_DLL
		AddAppSystem( "missionchooser", ASW_MISSION_CHOOSER_VERSION );
#endif
	}

	virtual int	Count()
	{
		return m_Systems.Count();
	}
	virtual char const *GetDllName( int idx )
	{
		return m_Systems[ idx ].m_pModuleName;
	}
	virtual char const *GetInterfaceName( int idx )
	{
		return m_Systems[ idx ].m_pInterfaceName;
	}
private:
	void AddAppSystem( char const *moduleName, char const *interfaceName )
	{
		AppSystemInfo_t sys;
		sys.m_pModuleName = moduleName;
		sys.m_pInterfaceName = interfaceName;
		m_Systems.AddToTail( sys );
	}

	CUtlVector< AppSystemInfo_t >	m_Systems;
};

EXPOSE_SINGLE_INTERFACE( CClientDLLSharedAppSystems, IClientDLLSharedAppSystems, CLIENT_DLL_SHARED_APPSYSTEMS );


//-----------------------------------------------------------------------------
// Helper interface for voice.
//-----------------------------------------------------------------------------
class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	virtual void GetPlayerTextColor(int entindex, int color[3])
	{
		color[0] = color[1] = color[2] = 128;
	}

	virtual void UpdateCursorState()
	{
	}

	virtual bool			CanShowSpeakerLabels()
	{
		return true;
	}
};
static CHLVoiceStatusHelper g_VoiceStatusHelper;

//-----------------------------------------------------------------------------
// Code to display which entities are having their bones setup each frame.
//-----------------------------------------------------------------------------

ConVar cl_ShowBoneSetupEnts( "cl_ShowBoneSetupEnts", "0", 0, "Show which entities are having their bones setup each frame." );

class CBoneSetupEnt
{
public:
	char m_ModelName[128];
	int m_Index;
	int m_Count;
};

bool BoneSetupCompare( const CBoneSetupEnt &a, const CBoneSetupEnt &b )
{
	return a.m_Index < b.m_Index;
}

CUtlRBTree<CBoneSetupEnt> g_BoneSetupEnts( BoneSetupCompare );


void TrackBoneSetupEnt( C_BaseAnimating *pEnt )
{
#ifdef _DEBUG
	if ( !cl_ShowBoneSetupEnts.GetInt() )
		return;

	CBoneSetupEnt ent;
	ent.m_Index = pEnt->entindex();
	unsigned short i = g_BoneSetupEnts.Find( ent );
	if ( i == g_BoneSetupEnts.InvalidIndex() )
	{
		Q_strncpy( ent.m_ModelName, modelinfo->GetModelName( pEnt->GetModel() ), sizeof( ent.m_ModelName ) );
		ent.m_Count = 1;
		g_BoneSetupEnts.Insert( ent );
	}
	else
	{
		g_BoneSetupEnts[i].m_Count++;
	}
#endif
}

void DisplayBoneSetupEnts()
{
#ifdef _DEBUG
	if ( !cl_ShowBoneSetupEnts.GetInt() )
		return;

	unsigned short i;
	int nElements = 0;
	for ( i=g_BoneSetupEnts.FirstInorder(); i != g_BoneSetupEnts.LastInorder(); i=g_BoneSetupEnts.NextInorder( i ) )
		++nElements;
		
	engine->Con_NPrintf( 0, "%d bone setup ents (name/count/entindex) ------------", nElements );

	con_nprint_s printInfo;
	printInfo.time_to_live = -1;
	printInfo.fixed_width_font = true;
	printInfo.color[0] = printInfo.color[1] = printInfo.color[2] = 1;
	
	printInfo.index = 2;
	for ( i=g_BoneSetupEnts.FirstInorder(); i != g_BoneSetupEnts.LastInorder(); i=g_BoneSetupEnts.NextInorder( i ) )
	{
		CBoneSetupEnt *pEnt = &g_BoneSetupEnts[i];
		
		if ( pEnt->m_Count >= 3 )
		{
			printInfo.color[0] = 1;
			printInfo.color[1] = printInfo.color[2] = 0;
		}
		else if ( pEnt->m_Count == 2 )
		{
			printInfo.color[0] = (float)200 / 255;
			printInfo.color[1] = (float)220 / 255;
			printInfo.color[2] = 0;
		}
		else
		{
			printInfo.color[0] = printInfo.color[0] = printInfo.color[0] = 1;
		}
		engine->Con_NXPrintf( &printInfo, "%25s / %3d / %3d", pEnt->m_ModelName, pEnt->m_Count, pEnt->m_Index );
		printInfo.index++;
	}

	g_BoneSetupEnts.RemoveAll();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: engine to client .dll interface
//-----------------------------------------------------------------------------
class CHLClient : public IBaseClientDLL
{
public:
	CHLClient();

	virtual int						Connect( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals );
	virtual int						Init( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals );

	virtual void					PostInit();
	virtual void					Shutdown( void );

	virtual void					LevelInitPreEntity( const char *pMapName );
	virtual void					LevelInitPostEntity();
	virtual void					LevelShutdown( void );

	virtual ClientClass				*GetAllClasses( void );

	virtual int						HudVidInit( void );
	virtual void					HudProcessInput( bool bActive );
	virtual void					HudUpdate( bool bActive );
	virtual void					HudReset( void );
	virtual void					HudText( const char * message );

	// Mouse Input Interfaces
	virtual void					IN_ActivateMouse( void );
	virtual void					IN_DeactivateMouse( void );
	virtual void					IN_Accumulate( void );
	virtual void					IN_ClearStates( void );
	virtual bool					IN_IsKeyDown( const char *name, bool& isdown );
	// Raw signal
	virtual int						IN_KeyEvent( int eventcode, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual void					IN_SetSampleTime( float frametime );
	// Create movement command
	virtual void					CreateMove ( int sequence_number, float input_sample_frametime, bool active );
	virtual void					ExtraMouseSample( float frametime, bool active );
	virtual bool					WriteUsercmdDeltaToBuffer( int nSlot, bf_write *buf, int from, int to, bool isnewcommand );	
	virtual void					EncodeUserCmdToBuffer( int nSlot, bf_write& buf, int slot );
	virtual void					DecodeUserCmdFromBuffer( int nSlot, bf_read& buf, int slot );


	virtual void					View_Render( vrect_t *rect );
	virtual void					RenderView( const CViewSetup &view, int nClearFlags, int whatToDraw );
	virtual void					View_Fade( ScreenFade_t *pSF );
	
	virtual void					SetCrosshairAngle( const QAngle& angle );

	virtual void					InitSprite( CEngineSprite *pSprite, const char *loadname );
	virtual void					ShutdownSprite( CEngineSprite *pSprite );

	virtual int						GetSpriteSize( void ) const;

	virtual void					VoiceStatus( int entindex, int iSsSlot, qboolean bTalking );

	virtual void					InstallStringTableCallback( const char *tableName );

	virtual void					FrameStageNotify( ClientFrameStage_t curStage );

	virtual bool					DispatchUserMessage( int msg_type, bf_read &msg_data );

	// Save/restore system hooks
	virtual CSaveRestoreData  *SaveInit( int size );
	virtual void			SaveWriteFields( CSaveRestoreData *, const char *, void *, datamap_t *, typedescription_t *, int );
	virtual void			SaveReadFields( CSaveRestoreData *, const char *, void *, datamap_t *, typedescription_t *, int );
	virtual void			PreSave( CSaveRestoreData * );
	virtual void			Save( CSaveRestoreData * );
	virtual void			WriteSaveHeaders( CSaveRestoreData * );
	virtual void			ReadRestoreHeaders( CSaveRestoreData * );
	virtual void			Restore( CSaveRestoreData *, bool );
	virtual void			DispatchOnRestore();
	virtual void			WriteSaveGameScreenshot( const char *pFilename );

	// Given a list of "S(wavname) S(wavname2)" tokens, look up the localized text and emit
	//  the appropriate close caption if running with closecaption = 1
	virtual void			EmitSentenceCloseCaption( char const *tokenstream );
	virtual void			EmitCloseCaption( char const *captionname, float duration );

	virtual CStandardRecvProxies* GetStandardRecvProxies();

	virtual bool			CanRecordDemo( char *errorMsg, int length ) const;

	virtual void			OnDemoRecordStart( char const* pDemoBaseName );
	virtual void			OnDemoRecordStop();
	virtual void			OnDemoPlaybackStart( char const* pDemoBaseName );
	virtual void			OnDemoPlaybackStop();

	virtual void			RecordDemoPolishUserInput( int nCmdIndex );

	// Cache replay ragdolls
	virtual bool			CacheReplayRagdolls( const char* pFilename, int nStartTick );

	// save game screenshot writing
	virtual void			WriteSaveGameScreenshotOfSize( const char *pFilename, int width, int height );

	// Gets the location of the player viewpoint
	virtual bool			GetPlayerView( CViewSetup &playerView );

	virtual bool			ShouldHideLoadingPlaque( void );

	virtual void			InvalidateMdlCache();

	virtual void			OnActiveSplitscreenPlayerChanged( int nNewSlot );
	virtual void			OnSplitScreenStateChanged();
	virtual void			CenterStringOff();


	virtual void			OnScreenSizeChanged( int nOldWidth, int nOldHeight );
	virtual IMaterialProxy *InstantiateMaterialProxy( const char *proxyName );

	virtual vgui::VPANEL	GetFullscreenClientDLLVPanel( void );
	virtual void			MarkEntitiesAsTouching( IClientEntity *e1, IClientEntity *e2 );
	virtual void			OnKeyBindingChanged( ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding );
	virtual bool			HandleGameUIEvent( const InputEvent_t &event );

public:
	void PrecacheMaterial( const char *pMaterialName );

	virtual void			SetBlurFade( float scale );
	
	virtual void			ResetHudCloseCaption();

	virtual bool			SupportsRandomMaps();

private:
	void UncacheAllMaterials( );
	void ResetStringTablePointers();

	CUtlRBTree< IMaterial * > m_CachedMaterials;

	CHudCloseCaption		*m_pHudCloseCaption;
};


CHLClient gHLClient;
IBaseClientDLL *clientdll = &gHLClient;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CHLClient, IBaseClientDLL, CLIENT_DLL_INTERFACE_VERSION, gHLClient );


//-----------------------------------------------------------------------------
// Precaches a material
//-----------------------------------------------------------------------------
void PrecacheMaterial( const char *pMaterialName )
{
	gHLClient.PrecacheMaterial( pMaterialName );
}

//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMaterialIndex( const char *pMaterialName )
{
	if (pMaterialName)
	{
		int nIndex = g_pStringTableMaterials->FindStringIndex( pMaterialName );
		Assert( nIndex >= 0 );
		if (nIndex >= 0)
			return nIndex;
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts precached material indices into strings
//-----------------------------------------------------------------------------
const char *GetMaterialNameFromIndex( int nIndex )
{
	if (nIndex != (g_pStringTableMaterials->GetMaxStrings() - 1))
	{
		return g_pStringTableMaterials->GetString( nIndex );
	}
	else
	{
		return NULL;
	}
}


//-----------------------------------------------------------------------------
// Precaches a particle system
//-----------------------------------------------------------------------------
int PrecacheParticleSystem( const char *pParticleSystemName )
{
	int nIndex = g_pStringTableParticleEffectNames->AddString( false, pParticleSystemName );
	g_pParticleSystemMgr->PrecacheParticleSystem( nIndex, pParticleSystemName );
	return nIndex;
}


//-----------------------------------------------------------------------------
// Converts a previously precached particle system into an index
//-----------------------------------------------------------------------------
int GetParticleSystemIndex( const char *pParticleSystemName )
{
	if ( pParticleSystemName )
	{
		int nIndex = g_pStringTableParticleEffectNames->FindStringIndex( pParticleSystemName );
		if ( nIndex != INVALID_STRING_INDEX )
			return nIndex;
		DevWarning("Client: Missing precache for particle system \"%s\"!\n", pParticleSystemName );
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts precached particle system indices into strings
//-----------------------------------------------------------------------------
const char *GetParticleSystemNameFromIndex( int nIndex )
{
	if ( nIndex < g_pStringTableParticleEffectNames->GetMaxStrings() )
		return g_pStringTableParticleEffectNames->GetString( nIndex );
	return "error";
}


//-----------------------------------------------------------------------------
// Precache-related methods for effects
//-----------------------------------------------------------------------------
void PrecacheEffect( const char *pEffectName )
{
	// Bring in dependent resources
	g_pPrecacheSystem->Cache( g_pPrecacheHandler, DISPATCH_EFFECT, pEffectName, true, RESOURCE_LIST_INVALID, true );
}


//-----------------------------------------------------------------------------
// Returns true if host_thread_mode is set to non-zero (and engine is running in threaded mode)
//-----------------------------------------------------------------------------
bool IsEngineThreaded()
{
	if ( g_pcv_ThreadMode )
	{
		return g_pcv_ThreadMode->GetBool();
	}
	return false;
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CHLClient::CHLClient() 
{
	// Kinda bogus, but the logic in the engine is too convoluted to put it there
	g_bLevelInitialized = false;
	m_pHudCloseCaption = NULL;

	SetDefLessFunc( m_CachedMaterials );
}


extern IGameSystem *ViewportClientSystem();

// enable threaded init functions on x360
static ConVar cl_threaded_init("cl_threaded_init", IsX360() ? "1" : "0");

bool InitParticleManager()
{
	if (!ParticleMgr()->Init(MAX_TOTAL_PARTICLES, materials))
		return false;

	return true;
}

bool InitGameSystems( CreateInterfaceFn appSystemFactory )
{

	if (!VGui_Startup( appSystemFactory ))
		return false;

	vgui::VGui_InitMatSysInterfacesList( "ClientDLL", &appSystemFactory, 1 );

	// Add the client systems.	

	// Client Leaf System has to be initialized first, since DetailObjectSystem uses it
	IGameSystem::Add( GameStringSystem() );
	IGameSystem::Add( g_pPrecacheRegister );
	IGameSystem::Add( SoundEmitterSystem() );
	IGameSystem::Add( ToolFrameworkClientSystem() );
	IGameSystem::Add( ClientLeafSystem() );
	IGameSystem::Add( DetailObjectSystem() );
	IGameSystem::Add( ViewportClientSystem() );
	IGameSystem::Add( g_pClientShadowMgr );
	IGameSystem::Add( g_pColorCorrectionMgr );
#ifdef GAMEUI_UISYSTEM2_ENABLED
	IGameSystem::Add( g_pGameUIGameSystem );
#endif
	IGameSystem::Add( ClientThinkList() );
	IGameSystem::Add( ClientSoundscapeSystem() );
	IGameSystem::Add( PerfVisualBenchmark() );

#if defined( CLIENT_DLL ) && defined( COPY_CHECK_STRESSTEST )
	IGameSystem::Add( GetPredictionCopyTester() );
#endif

	ActivityList_Init();
	ActivityList_RegisterSharedActivities();
	EventList_Init();
	EventList_RegisterSharedEvents();

	modemanager->Init( );

	// Load the ClientScheme just once
	vgui::scheme()->LoadSchemeFromFileEx( VGui_GetFullscreenRootVPANEL(), "resource/ClientScheme.res", "ClientScheme");

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetClientMode()->InitViewport();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->InitViewport();
		}
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetHud().Init();
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetClientMode()->Init();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->Init();
		}
	}

	if ( !IGameSystem::InitAllSystems() )
		return false;

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetClientMode()->Enable();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->EnableWithRootPanel( VGui_GetFullscreenRootVPANEL() );
		}
	}	

	// Each mod is required to implement this
	view = GetViewRenderInstance();
	if ( !view )
	{
		Error( "GetViewRenderInstance() must be implemented by game." );
	}

	view->Init();
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetViewEffects()->Init();
	}

	C_BaseTempEntity::PrecacheTempEnts();

	input->Init_All();

	VGui_CreateGlobalPanels();

	InitSmokeFogOverlay();

	// Register user messages..
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		CUserMessageRegister::RegisterAll();
	}

	ClientVoiceMgr_Init();

	// Embed voice status icons inside chat element
	{
		vgui::VPANEL parent = enginevgui->GetPanel( PANEL_CLIENTDLL );
		GetClientVoiceMgr()->Init( &g_VoiceStatusHelper, parent );
	}

	if ( !PhysicsDLLInit( appSystemFactory ) )
		return false;

	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetEntitySaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetPhysSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetViewEffectsRestoreBlockHandler() );

	ClientWorldFactoryInit();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the DLL is first loaded.
// Input  : engineFactory - 
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::Connect( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals )
{
	InitCRTMemDebug();
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	// Hook up global variables
	gpGlobals = pGlobals;

	ConnectTier1Libraries( &appSystemFactory, 1 );
	ConnectTier2Libraries( &appSystemFactory, 1 );
	ConnectTier3Libraries( &appSystemFactory, 1 );

#ifndef _X360
	SteamAPI_InitSafe();
	g_SteamAPIContext.Init();

#ifdef INFESTED_DLL
	
#endif
#endif

	// Initialize the console variables.
	ConVar_Register( FCVAR_CLIENTDLL );

	return true;
}

int CHLClient::Init( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals )
{

	COM_TimestampedLog( "ClientDLL factories - Start" );
	// We aren't happy unless we get all of our interfaces.
	// please don't collapse this into one monolithic boolean expression (impossible to debug)
	if ( (engine = (IVEngineClient *)appSystemFactory( VENGINE_CLIENT_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (modelrender = (IVModelRender *)appSystemFactory( VENGINE_HUDMODEL_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (effects = (IVEfx *)appSystemFactory( VENGINE_EFFECTS_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (enginetrace = (IEngineTrace *)appSystemFactory( INTERFACEVERSION_ENGINETRACE_CLIENT, NULL )) == NULL )
		return false;
	if ( (filelogginglistener = (IFileLoggingListener *)appSystemFactory(FILELOGGINGLISTENER_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (render = (IVRenderView *)appSystemFactory( VENGINE_RENDERVIEW_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (debugoverlay = (IVDebugOverlay *)appSystemFactory( VDEBUG_OVERLAY_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (datacache = (IDataCache*)appSystemFactory(DATACACHE_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( !mdlcache )
		return false;
	if ( (modelinfo = (IVModelInfoClient *)appSystemFactory(VMODELINFO_CLIENT_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (enginevgui = (IEngineVGui *)appSystemFactory(VENGINE_VGUI_VERSION, NULL )) == NULL )
		return false;
	if ( (networkstringtable = (INetworkStringTableContainer *)appSystemFactory(INTERFACENAME_NETWORKSTRINGTABLECLIENT,NULL)) == NULL )
		return false;
	if ( (partition = (ISpatialPartition *)appSystemFactory(INTERFACEVERSION_SPATIALPARTITION, NULL)) == NULL )
		return false;
	if ( (shadowmgr = (IShadowMgr *)appSystemFactory(ENGINE_SHADOWMGR_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (staticpropmgr = (IStaticPropMgrClient *)appSystemFactory(INTERFACEVERSION_STATICPROPMGR_CLIENT, NULL)) == NULL )
		return false;
	if ( (enginesound = (IEngineSound *)appSystemFactory(IENGINESOUND_CLIENT_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (filesystem = (IFileSystem *)appSystemFactory(FILESYSTEM_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (random = (IUniformRandomStream *)appSystemFactory(VENGINE_CLIENT_RANDOM_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (gameuifuncs = (IGameUIFuncs * )appSystemFactory( VENGINE_GAMEUIFUNCS_VERSION, NULL )) == NULL )
		return false;
	if ( (gameeventmanager = (IGameEventManager2 *)appSystemFactory(INTERFACEVERSION_GAMEEVENTSMANAGER2,NULL)) == NULL )
		return false;
	if ( (soundemitterbase = (ISoundEmitterSystemBase *)appSystemFactory(SOUNDEMITTERSYSTEM_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (inputsystem = (IInputSystem *)appSystemFactory(INPUTSYSTEM_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( IsPC() && !IsPosix() && (avi = (IAvi *)appSystemFactory(AVI_INTERFACE_VERSION, NULL)) == NULL )
		return false;
#if !defined( _X360 ) || defined( BINK_ENABLED_FOR_X360 )
	if ( (bik = (IBik *)appSystemFactory(BIK_INTERFACE_VERSION, NULL)) == NULL )
		return false;
#endif
	if ( (scenefilecache = (ISceneFileCache *)appSystemFactory( SCENE_FILE_CACHE_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (blackboxrecorder = (IBlackBox *)appSystemFactory(BLACKBOX_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (xboxsystem = (IXboxSystem *)appSystemFactory( XBOXSYSTEM_INTERFACE_VERSION, NULL )) == NULL )
		return false;
#if defined( REPLAY_ENABLED )
	if ( IsPC() && (g_pReplayHistoryManager = (IReplayHistoryManager *)appSystemFactory( REPLAYHISTORYMANAGER_INTERFACE_VERSION, NULL )) == NULL )
		return false;
#endif
#ifndef _XBOX
	if ( ( gamestatsuploader = (IUploadGameStats *)appSystemFactory( INTERFACEVERSION_UPLOADGAMESTATS, NULL )) == NULL )
		return false;
#endif
	if (!g_pMatSystemSurface)
		return false;

#ifdef INFESTED_DLL
	if ( (missionchooser = (IASW_Mission_Chooser *)appSystemFactory(ASW_MISSION_CHOOSER_VERSION, NULL)) == NULL )
		return false;
#endif


	if ( !CommandLine()->CheckParm( "-noscripting") )
	{
		scriptmanager = (IScriptManager *)appSystemFactory( VSCRIPT_INTERFACE_VERSION, NULL );
	}

	factorylist_t factories;
	factories.appSystemFactory = appSystemFactory;
	FactoryList_Store( factories );

	COM_TimestampedLog( "soundemitterbase->Connect" );
	// Yes, both the client and game .dlls will try to Connect, the soundemittersystem.dll will handle this gracefully
	if ( !soundemitterbase->Connect( appSystemFactory ) )
	{
		return false;
	}

	if ( CommandLine()->FindParm( "-textmode" ) )
		g_bTextMode = true;

	if ( CommandLine()->FindParm( "-makedevshots" ) )
		g_MakingDevShots = true;

	if ( CommandLine()->FindParm( "-headtracking" ) )
		g_bHeadTrackingEnabled = true;

	// Not fatal if the material system stub isn't around.
	materials_stub = (IMaterialSystemStub*)appSystemFactory( MATERIAL_SYSTEM_STUB_INTERFACE_VERSION, NULL );

	if( !g_pMaterialSystemHardwareConfig )
		return false;

	// Hook up the gaussian random number generator
	s_GaussianRandomStream.AttachToStream( random );

	g_pcv_ThreadMode = g_pCVar->FindVar( "host_thread_mode" );




	COM_TimestampedLog( "InitGameSystems" );

	bool bInitSuccess = false;
	if ( cl_threaded_init.GetBool() )
	{
		CFunctorJob *pGameJob = new CFunctorJob( CreateFunctor( InitParticleManager ) );
		g_pThreadPool->AddJob( pGameJob );
		bInitSuccess = InitGameSystems( appSystemFactory );
		pGameJob->WaitForFinishAndRelease();
	}
	else
	{
		COM_TimestampedLog( "ParticleMgr()->Init" );
		if (!ParticleMgr()->Init(MAX_TOTAL_PARTICLES, materials))
			return false;
		COM_TimestampedLog( "InitGameSystems - Start" );
		bInitSuccess = InitGameSystems( appSystemFactory );
		COM_TimestampedLog( "InitGameSystems - End" );
	}


#ifdef INFESTED_PARTICLES	// let the emitter cache load in our standard
	g_ASWGenericEmitterCache.PrecacheTemplates();
#endif

	COM_TimestampedLog( "C_BaseAnimating::InitBoneSetupThreadPool" );

	C_BaseAnimating::InitBoneSetupThreadPool();

	// This is a fullscreen element, so only lives on slot 0!!!
	m_pHudCloseCaption = GET_FULLSCREEN_HUDELEMENT( CHudCloseCaption );

	COM_TimestampedLog( "ClientDLL Init - Finish" );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called after client & server DLL are loaded and all systems initialized
//-----------------------------------------------------------------------------
void CHLClient::PostInit()
{
	COM_TimestampedLog( "IGameSystem::PostInitAllSystems - Start" );
	IGameSystem::PostInitAllSystems();
	COM_TimestampedLog( "IGameSystem::PostInitAllSystems - Finish" );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the client .dll is being dismissed
//-----------------------------------------------------------------------------
void CHLClient::Shutdown( void )
{


	ActivityList_Free();
	EventList_Free();

	VGui_ClearVideoPanels();

	C_BaseAnimating::ShutdownBoneSetupThreadPool();
	ClientWorldFactoryShutdown();

	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetViewEffectsRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetPhysSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetEntitySaveRestoreBlockHandler() );

	ClientVoiceMgr_Shutdown();


	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetClientMode()->Disable();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->Disable();
		}
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetClientMode()->Shutdown();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->Shutdown();
		}
	}

	input->Shutdown_All();
	C_BaseTempEntity::ClearDynamicTempEnts();
	TermSmokeFogOverlay();
	view->Shutdown();
	g_pParticleSystemMgr->UncacheAllParticleSystems();
	UncacheAllMaterials();

	IGameSystem::ShutdownAllSystems();

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		GetHud().Shutdown();
	}

	VGui_Shutdown();
	
	ClearKeyValuesCache();

#ifndef NO_STEAM
	g_SteamAPIContext.Clear();
	// SteamAPI_Shutdown(); << Steam shutdown is controlled by engine
#ifdef INFESTED_DLL
	
#endif
#endif
	
	DisconnectTier3Libraries( );
	DisconnectTier2Libraries( );
	ConVar_Unregister();
	DisconnectTier1Libraries( );
}


//-----------------------------------------------------------------------------
// Purpose: 
//  Called when the game initializes
//  and whenever the vid_mode is changed
//  so the HUD can reinitialize itself.
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::HudVidInit( void )
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().VidInit();
	}

	GetClientVoiceMgr()->VidInit();

	return 1;
}

//-----------------------------------------------------------------------------
// Method used to allow the client to filter input messages before the 
// move record is transmitted to the server
//-----------------------------------------------------------------------------
void CHLClient::HudProcessInput( bool bActive )
{
	GetClientMode()->ProcessInput( bActive );
}

//-----------------------------------------------------------------------------
// Purpose: Called when shared data gets changed, allows dll to modify data
// Input  : bActive - 
//-----------------------------------------------------------------------------
void CHLClient::HudUpdate( bool bActive )
{
	float frametime = gpGlobals->frametime;

	GetClientVoiceMgr()->Frame( frametime );

	ASSERT_LOCAL_PLAYER_NOT_RESOLVABLE();

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().UpdateHud( bActive );
	}

	ASSERT_LOCAL_PLAYER_NOT_RESOLVABLE();

	{
		C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 
		IGameSystem::UpdateAllSystems( frametime );
	}

	// run vgui animations
	vgui::GetAnimationController()->UpdateAnimations( Plat_FloatTime() );

	hudlcd->SetGlobalStat( "(time_int)", VarArgs( "%d", (int)gpGlobals->curtime ) );
	hudlcd->SetGlobalStat( "(time_float)", VarArgs( "%.2f", gpGlobals->curtime ) );

	// I don't think this is necessary any longer, but I will leave it until
	// I can check into this further.
	C_BaseTempEntity::CheckDynamicTempEnts();
}

//-----------------------------------------------------------------------------
// Purpose: Called to restore to "non"HUD state.
//-----------------------------------------------------------------------------
void CHLClient::HudReset( void )
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().VidInit();
	}

	PhysicsReset();
}

//-----------------------------------------------------------------------------
// Purpose: Called to add hud text message
//-----------------------------------------------------------------------------
void CHLClient::HudText( const char * message )
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		DispatchHudText( message );
	}
}


//-----------------------------------------------------------------------------
// Handler for input events for the new game ui system
//-----------------------------------------------------------------------------
bool CHLClient::HandleGameUIEvent( const InputEvent_t &inputEvent )
{
#ifdef GAMEUI_UISYSTEM2_ENABLED
	// TODO: when embedded UI will be used for HUD, we will need it to maintain
	// a separate screen for HUD and a separate screen stack for pause menu & main menu.
	// for now only render embedded UI in pause menu & main menu
	BaseModUI::CBaseModPanel *pBaseModPanel = BaseModUI::CBaseModPanel::GetSingletonPtr();
	if ( !pBaseModPanel || !pBaseModPanel->IsVisible() )
		return false;

	return g_pGameUIGameSystem->RegisterInputEvent( inputEvent );
#else
	return false;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : ClientClass
//-----------------------------------------------------------------------------
ClientClass *CHLClient::GetAllClasses( void )
{
	return g_pClientClassHead;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_ActivateMouse( void )
{
	input->ActivateMouse();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_DeactivateMouse( void )
{
	input->DeactivateMouse();
}

extern ConVar in_forceuser;
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_Accumulate ( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( in_forceuser.GetInt() );
	input->AccumulateMouse( GET_ACTIVE_SPLITSCREEN_SLOT() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_ClearStates ( void )
{
	input->ClearStates();
}

//-----------------------------------------------------------------------------
// Purpose: Engine can query for particular keys
// Input  : *name - 
//-----------------------------------------------------------------------------
bool CHLClient::IN_IsKeyDown( const char *name, bool& isdown )
{
	kbutton_t *key = input->FindKey( name );
	if ( !key )
	{
		return false;
	}
	
	isdown = ( key->GetPerUser().state & 1 ) ? true : false;

	// Found the key by name
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Engine can issue a key event
// Input  : eventcode - 
//			keynum - 
//			*pszCurrentBinding - 
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::IN_KeyEvent( int eventcode, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	return input->KeyEvent( eventcode, keynum, pszCurrentBinding );
}

void CHLClient::ExtraMouseSample( float frametime, bool active )
{
	bool bSave = C_BaseEntity::IsAbsRecomputationsEnabled();
	C_BaseEntity::EnableAbsRecomputations( true );

	ABS_QUERY_GUARD( true );

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 

	MDLCACHE_CRITICAL_SECTION();
	input->ExtraMouseSample( frametime, active );

	C_BaseEntity::EnableAbsRecomputations( bSave );
}

void CHLClient::IN_SetSampleTime( float frametime )
{
	input->Joystick_SetSampleTime( frametime );
	input->IN_SetSampleTime( frametime );
}
//-----------------------------------------------------------------------------
// Purpose: Fills in usercmd_s structure based on current view angles and key/controller inputs
// Input  : frametime - timestamp for last frame
//			*cmd - the command to fill in
//			active - whether the user is fully connected to a server
//-----------------------------------------------------------------------------
void CHLClient::CreateMove ( int sequence_number, float input_sample_frametime, bool active )
{

	Assert( C_BaseEntity::IsAbsRecomputationsEnabled() );
	Assert( C_BaseEntity::IsAbsQueriesValid() );

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 

	MDLCACHE_CRITICAL_SECTION();
	input->CreateMove( sequence_number, input_sample_frametime, active );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buf - 
//			from - 
//			to - 
//-----------------------------------------------------------------------------
bool CHLClient::WriteUsercmdDeltaToBuffer( int nSlot, bf_write *buf, int from, int to, bool isnewcommand )
{
	return input->WriteUsercmdDeltaToBuffer( nSlot, buf, from, to, isnewcommand );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//			buffersize - 
//-----------------------------------------------------------------------------
void CHLClient::EncodeUserCmdToBuffer( int nSlot, bf_write& buf, int slot )
{
	input->EncodeUserCmdToBuffer( nSlot, buf, slot );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//			buffersize - 
//			slot - 
//-----------------------------------------------------------------------------
void CHLClient::DecodeUserCmdFromBuffer( int nSlot, bf_read& buf, int slot )
{
	input->DecodeUserCmdFromBuffer( nSlot, buf, slot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::View_Render( vrect_t *rect )
{
	VPROF( "View_Render" );

	// UNDONE: This gets hit at startup sometimes, investigate - will cause NaNs in calcs inside Render()
	if ( rect->width == 0 || rect->height == 0 )
		return;

	view->Render( rect );
	UpdatePerfStats();
}


//-----------------------------------------------------------------------------
// Gets the location of the player viewpoint
//-----------------------------------------------------------------------------
bool CHLClient::GetPlayerView( CViewSetup &playerView )
{
	playerView = *view->GetPlayerViewSetup();
	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CHLClient::InvalidateMdlCache()
{
	C_BaseAnimating *pAnimating;
	for ( C_BaseEntity *pEntity = ClientEntityList().FirstBaseEntity(); pEntity; pEntity = ClientEntityList().NextBaseEntity(pEntity) )
	{
		pAnimating = pEntity->GetBaseAnimating();
		if ( pAnimating )
		{
			pAnimating->InvalidateMdlCache();
		}
	}
	CStudioHdr::CActivityToSequenceMapping::ResetMappings();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSF - 
//-----------------------------------------------------------------------------
void CHLClient::View_Fade( ScreenFade_t *pSF )
{
	if ( pSF != NULL )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetViewEffects()->Fade( *pSF );
		}
	}
}

// CPU level
//-----------------------------------------------------------------------------
void ConfigureCurrentSystemLevel( );
void OnCPULevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar cpu_level( "cpu_level", "2", 0, "CPU Level - Default: High", OnCPULevelChanged );
CPULevel_t GetCPULevel()
{
	if ( IsX360() )
		return CPU_LEVEL_360;

	return GetActualCPULevel();
}

CPULevel_t GetActualCPULevel()
{
	// Should we cache system_level off during level init?
	CPULevel_t nSystemLevel = (CPULevel_t)clamp( cpu_level.GetInt(), 0, CPU_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}



//-----------------------------------------------------------------------------
// GPU level
//-----------------------------------------------------------------------------
void OnGPULevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar gpu_level( "gpu_level", "3", 0, "GPU Level - Default: High", OnGPULevelChanged );
GPULevel_t GetGPULevel()
{
	if ( IsX360() )
		return GPU_LEVEL_360;

	// Should we cache system_level off during level init?
	GPULevel_t nSystemLevel = (GPULevel_t)clamp( gpu_level.GetInt(), 0, GPU_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}


//-----------------------------------------------------------------------------
// System Memory level
//-----------------------------------------------------------------------------
void OnMemLevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar mem_level( "mem_level", "2", 0, "Memory Level - Default: High", OnMemLevelChanged );
MemLevel_t GetMemLevel()
{
	if ( IsX360() )
		return MEM_LEVEL_360;

	// Should we cache system_level off during level init?
	MemLevel_t nSystemLevel = (MemLevel_t)clamp( mem_level.GetInt(), 0, MEM_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}

//-----------------------------------------------------------------------------
// GPU Memory level
//-----------------------------------------------------------------------------
void OnGPUMemLevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar gpu_mem_level( "gpu_mem_level", "2", 0, "Memory Level - Default: High", OnGPUMemLevelChanged );
GPUMemLevel_t GetGPUMemLevel()
{
	if ( IsX360() )
		return GPU_MEM_LEVEL_360;

	// Should we cache system_level off during level init?
	GPUMemLevel_t nSystemLevel = (GPUMemLevel_t)clamp( gpu_mem_level.GetInt(), 0, GPU_MEM_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}

void ConfigureCurrentSystemLevel()
{
	int nCPULevel = GetCPULevel();
	if ( nCPULevel == CPU_LEVEL_360 )
	{
		nCPULevel = 360;
	}

	int nGPULevel = GetGPULevel();
	if ( nGPULevel == GPU_LEVEL_360 )
	{
		nGPULevel = 360;
	}

	int nMemLevel = GetMemLevel();
	if ( nMemLevel == MEM_LEVEL_360 )
	{
		nMemLevel = 360;
	}

	int nGPUMemLevel = GetGPUMemLevel();
	if ( nGPUMemLevel == GPU_MEM_LEVEL_360 )
	{
		nGPUMemLevel = 360;
	}

#if defined( SWARM_DLL )
	char szModName[32] = "swarm";
#elif defined ( HL2_EPISODIC )
	char szModName[32] = "ep2";
#elif defined ( SDK_CLIENT_DLL )
	char szModName[32] = "sdk";
#endif

	UpdateSystemLevel( nCPULevel, nGPULevel, nMemLevel, nGPUMemLevel, VGui_IsSplitScreen(), szModName );

	if ( engine )
	{
		engine->ConfigureSystemLevel( nCPULevel, nGPULevel );
	}

	C_BaseEntity::UpdateVisibilityAllEntities();
	if ( view )
	{
		view->InitFadeData();
	}
}



//-----------------------------------------------------------------------------
// Purpose: Per level init
//-----------------------------------------------------------------------------
void CHLClient::LevelInitPreEntity( char const* pMapName )
{
	// HACK: Bogus, but the logic is too complicated in the engine
	if (g_bLevelInitialized)
		return;
	g_bLevelInitialized = true;

	engine->TickProgressBar();

	input->LevelInit();

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetViewEffects()->LevelInit();
	}
	
	// Tell mode manager that map is changing
	modemanager->LevelInit( pMapName );
	ParticleMgr()->LevelInit();

	ClientVoiceMgr_LevelInit();

	hudlcd->SetGlobalStat( "(mapname)", pMapName );

	C_BaseTempEntity::ClearDynamicTempEnts();
	clienteffects->Flush();
	view->LevelInit();
	tempents->LevelInit();
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		ResetToneMapping(1.0);
	}

	IGameSystem::LevelInitPreEntityAllSystems(pMapName);

	ResetWindspeed();

#if !defined( NO_ENTITY_PREDICTION )
	// don't do prediction if single player!
	// don't set direct because of FCVAR_USERINFO
	if ( gpGlobals->maxClients > 1 )
	{
		if ( !cl_predict->GetInt() )
		{
			engine->ClientCmd( "cl_predict 1" );
		}
	}
	else
	{
		if ( cl_predict->GetInt() )
		{
			engine->ClientCmd( "cl_predict 0" );
		}
	}
#endif

	// Check low violence settings for this map
	g_RagdollLVManager.SetLowViolence( pMapName );

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

		engine->TickProgressBar();

		GetHud().LevelInit();
	}

#if defined( REPLAY_ENABLED )
	// Initialize replay ragdoll recorder
	if ( !engine->IsPlayingDemo() )
	{
		CReplayRagdollRecorder::Instance().Init();
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Per level init
//-----------------------------------------------------------------------------
void CHLClient::LevelInitPostEntity( )
{
	ABS_QUERY_GUARD( true );

	IGameSystem::LevelInitPostEntityAllSystems();
	C_PhysPropClientside::RecreateAll();
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetCenterPrint()->Clear();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reset our global string table pointers
//-----------------------------------------------------------------------------
void CHLClient::ResetStringTablePointers()
{
	g_pStringTableParticleEffectNames = NULL;
	g_StringTableEffectDispatch = NULL;
	g_StringTableVguiScreen = NULL;
	g_pStringTableMaterials = NULL;
	g_pStringTableInfoPanel = NULL;
	g_pStringTableClientSideChoreoScenes = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Per level de-init
//-----------------------------------------------------------------------------
void CHLClient::LevelShutdown( void )
{
	// HACK: Bogus, but the logic is too complicated in the engine
	if (!g_bLevelInitialized)
	{
		ResetStringTablePointers();
		return;
	}

	g_bLevelInitialized = false;

	// Disable abs recomputations when everything is shutting down
	CBaseEntity::EnableAbsRecomputations( false );

	// Level shutdown sequence.
	// First do the pre-entity shutdown of all systems
	IGameSystem::LevelShutdownPreEntityAllSystems();

	C_PhysPropClientside::DestroyAll();

	modemanager->LevelShutdown();

	// Remove temporary entities before removing entities from the client entity list so that the te_* may
	// clean up before hand.
	tempents->LevelShutdown();

	// Now release/delete the entities
	cl_entitylist->Release();

	C_BaseEntityClassList *pClassList = s_pClassLists;
	while ( pClassList )
	{
		pClassList->LevelShutdown();
		pClassList = pClassList->m_pNextClassList;
	}

	// Now do the post-entity shutdown of all systems
	IGameSystem::LevelShutdownPostEntityAllSystems();

	view->LevelShutdown();
	beams->ClearBeams();
	ParticleMgr()->RemoveAllEffects();
	
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		StopAllRumbleEffects( hh );
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().LevelShutdown();
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetCenterPrint()->Clear();
	}

	ClientVoiceMgr_LevelShutdown();

	messagechars->Clear();

	g_pParticleSystemMgr->LevelShutdown();
	g_pParticleSystemMgr->UncacheAllParticleSystems();
	UncacheAllMaterials();

#ifdef _XBOX
	ReleaseRenderTargets();
#endif

	// string tables are cleared on disconnect from a server, so reset our global pointers to NULL
	ResetStringTablePointers();

	CStudioHdr::CActivityToSequenceMapping::ResetMappings();

#if defined( REPLAY_ENABLED )
	// Shutdown the ragdoll recorder
	CReplayRagdollRecorder::Instance().Shutdown();
	CReplayRagdollCache::Instance().Shutdown();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Engine received crosshair offset ( autoaim )
// Input  : angle - 
//-----------------------------------------------------------------------------
void CHLClient::SetCrosshairAngle( const QAngle& angle )
{
#ifndef INFESTED_DLL
	CHudCrosshair *crosshair = GET_HUDELEMENT( CHudCrosshair );
	if ( crosshair )
	{
		crosshair->SetCrosshairAngle( angle );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Helper to initialize sprite from .spr semaphor
// Input  : *pSprite - 
//			*loadname - 
//-----------------------------------------------------------------------------
void CHLClient::InitSprite( CEngineSprite *pSprite, const char *loadname )
{
	if ( pSprite )
	{
		pSprite->Init( loadname );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSprite - 
//-----------------------------------------------------------------------------
void CHLClient::ShutdownSprite( CEngineSprite *pSprite )
{
	if ( pSprite )
	{
		pSprite->Shutdown();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tells engine how much space to allocate for sprite objects
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::GetSpriteSize( void ) const
{
	return sizeof( CEngineSprite );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : entindex - 
//			bTalking - 
//-----------------------------------------------------------------------------
void CHLClient::VoiceStatus( int entindex, int iSsSlot, qboolean bTalking )
{
	GetClientVoiceMgr()->UpdateSpeakerStatus( entindex, iSsSlot, !!bTalking );
}


//-----------------------------------------------------------------------------
// Called when the string table for materials changes
//-----------------------------------------------------------------------------
void OnMaterialStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	gHLClient.PrecacheMaterial( newString );
	RequestCacheUsedMaterials();
}


//-----------------------------------------------------------------------------
// Called when the string table for dispatch effects changes
//-----------------------------------------------------------------------------
void OnEffectStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	g_pPrecacheSystem->Cache( g_pPrecacheHandler, DISPATCH_EFFECT, newString, true, RESOURCE_LIST_INVALID, true );
	RequestCacheUsedMaterials();
}


//-----------------------------------------------------------------------------
// Called when the string table for particle systems changes
//-----------------------------------------------------------------------------
void OnParticleSystemStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	g_pParticleSystemMgr->PrecacheParticleSystem( stringNumber, newString );
	RequestCacheUsedMaterials();
}

//-----------------------------------------------------------------------------
// Called when the string table for particle files changes
//-----------------------------------------------------------------------------
void OnPrecacheParticleFile( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	g_pParticleSystemMgr->ShouldLoadSheets( true );
	g_pParticleSystemMgr->ReadParticleConfigFile( newString, true, false );
	g_pParticleSystemMgr->DecommitTempMemory();
}

//-----------------------------------------------------------------------------
// Called when the string table for VGUI changes
//-----------------------------------------------------------------------------
void OnVguiScreenTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	vgui::Panel *pPanel = PanelMetaClassMgr()->CreatePanelMetaClass( newString, 100, NULL, NULL );
	if ( pPanel )
		PanelMetaClassMgr()->DestroyPanelMetaClass( pPanel );
}

//-----------------------------------------------------------------------------
// Purpose: Preload the string on the client (if single player it should already be in the cache from the server!!!)
// Input  : *object - 
//			*stringTable - 
//			stringNumber - 
//			*newString - 
//			*newData - 
//-----------------------------------------------------------------------------
void OnSceneStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
}

//-----------------------------------------------------------------------------
// Purpose: Hook up any callbacks here, the table definition has been parsed but 
//  no data has been added yet
//-----------------------------------------------------------------------------
void CHLClient::InstallStringTableCallback( const char *tableName )
{
	// Here, cache off string table IDs
	if (!Q_strcasecmp(tableName, "VguiScreen"))
	{
		// Look up the id 
		g_StringTableVguiScreen = networkstringtable->FindTable( tableName );

		// When the material list changes, we need to know immediately
		g_StringTableVguiScreen->SetStringChangedCallback( NULL, OnVguiScreenTableChanged );
	}
	else if (!Q_strcasecmp(tableName, "Materials"))
	{
		// Look up the id 
		g_pStringTableMaterials = networkstringtable->FindTable( tableName );

		// When the material list changes, we need to know immediately
		g_pStringTableMaterials->SetStringChangedCallback( NULL, OnMaterialStringTableChanged );
	}
	else if ( !Q_strcasecmp( tableName, "EffectDispatch" ) )
	{
		g_StringTableEffectDispatch = networkstringtable->FindTable( tableName );

		// When the material list changes, we need to know immediately
		g_StringTableEffectDispatch->SetStringChangedCallback( NULL, OnEffectStringTableChanged );
	}
	else if ( !Q_strcasecmp( tableName, "InfoPanel" ) )
	{
		g_pStringTableInfoPanel = networkstringtable->FindTable( tableName );
	}
	else if ( !Q_strcasecmp( tableName, "Scenes" ) )
	{
		g_pStringTableClientSideChoreoScenes = networkstringtable->FindTable( tableName );
		networkstringtable->SetAllowClientSideAddString( g_pStringTableClientSideChoreoScenes, true );
		g_pStringTableClientSideChoreoScenes->SetStringChangedCallback( NULL, OnSceneStringTableChanged );
	}
	else if ( !Q_strcasecmp( tableName, "ParticleEffectNames" ) )
	{
		g_pStringTableParticleEffectNames = networkstringtable->FindTable( tableName );
		networkstringtable->SetAllowClientSideAddString( g_pStringTableParticleEffectNames, true );
		// When the particle system list changes, we need to know immediately
		g_pStringTableParticleEffectNames->SetStringChangedCallback( NULL, OnParticleSystemStringTableChanged );
	}
	else if ( !Q_strcasecmp( tableName, "ExtraParticleFilesTable" ) )
	{
		g_pStringTableExtraParticleFiles = networkstringtable->FindTable( tableName );
		networkstringtable->SetAllowClientSideAddString( g_pStringTableExtraParticleFiles, true );
		// When the particle system list changes, we need to know immediately
		g_pStringTableExtraParticleFiles->SetStringChangedCallback( NULL, OnPrecacheParticleFile );
	}
	else
	{
		// Pass tablename to gamerules last if all other checks fail
		InstallStringTableCallback_GameRules( tableName );
	}

}


//-----------------------------------------------------------------------------
// Material precache
//-----------------------------------------------------------------------------
void CHLClient::PrecacheMaterial( const char *pMaterialName )
{
	Assert( pMaterialName );

	int nLen = Q_strlen( pMaterialName );
	char *pTempBuf = (char*)stackalloc( nLen + 1 );
	memcpy( pTempBuf, pMaterialName, nLen + 1 );
	char *pFound = Q_strstr( pTempBuf, ".vmt\0" );
	if ( pFound )
	{
		*pFound = 0;
	}
		
	IMaterial *pMaterial = materials->FindMaterial( pTempBuf, TEXTURE_GROUP_PRECACHED );
	if ( !IsErrorMaterial( pMaterial ) )
	{
		int idx = m_CachedMaterials.Find( pMaterial );
		if ( idx == m_CachedMaterials.InvalidIndex() )
		{
			pMaterial->IncrementReferenceCount();
			m_CachedMaterials.Insert( pMaterial );
		}
	}
}

void CHLClient::UncacheAllMaterials( )
{
	for ( int i = m_CachedMaterials.FirstInorder(); i != m_CachedMaterials.InvalidIndex(); i = m_CachedMaterials.NextInorder( i ) )
	{
		m_CachedMaterials[i]->DecrementReferenceCount();
	}
	m_CachedMaterials.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHLClient::DispatchUserMessage( int msg_type, bf_read &msg_data )
{
	return usermessages->DispatchUserMessage( msg_type, msg_data );
}


void SimulateEntities()
{
	VPROF_BUDGET("Client SimulateEntities", VPROF_BUDGETGROUP_CLIENT_SIM);

	// Service timer events (think functions).
  	ClientThinkList()->PerformThinkFunctions();

	C_BaseEntity::SimulateEntities();
}


bool AddDataChangeEvent( IClientNetworkable *ent, DataUpdateType_t updateType, int *pStoredEvent )
{
	Assert( ent );
	// Make sure we don't already have an event queued for this guy.
	if ( *pStoredEvent >= 0 )
	{
		Assert( g_DataChangedEvents[*pStoredEvent].m_pEntity == ent );

		// DATA_UPDATE_CREATED always overrides DATA_UPDATE_CHANGED.
		if ( updateType == DATA_UPDATE_CREATED )
			g_DataChangedEvents[*pStoredEvent].m_UpdateType = updateType;
	
		return false;
	}
	else
	{
		*pStoredEvent = g_DataChangedEvents.AddToTail( CDataChangedEvent( ent, updateType, pStoredEvent ) );
		return true;
	}
}


void ClearDataChangedEvent( int iStoredEvent )
{
	if ( iStoredEvent != -1 )
		g_DataChangedEvents.Remove( iStoredEvent );
}


void ProcessOnDataChangedEvents()
{
	VPROF_("ProcessOnDataChangedEvents", 1, VPROF_BUDGETGROUP_CLIENT_SIM, false, BUDGETFLAG_CLIENT);
	int nSave = GET_ACTIVE_SPLITSCREEN_SLOT();
	bool bSaveAccess = engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );

	FOR_EACH_LL( g_DataChangedEvents, i )
	{
		CDataChangedEvent *pEvent = &g_DataChangedEvents[i];

		// Reset their stored event identifier.		
		*pEvent->m_pStoredEvent = -1;


		// Send the event.
		IClientNetworkable *pNetworkable = pEvent->m_pEntity;

		pNetworkable->OnDataChanged( pEvent->m_UpdateType );
	}

	g_DataChangedEvents.Purge();

	engine->SetActiveSplitScreenPlayerSlot( nSave );
	engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, bSaveAccess );
}


void UpdateClientRenderableInPVSStatus()
{
	// Vis for this view should already be setup at this point.

	// For each client-only entity, notify it if it's newly coming into the PVS.
	CUtlLinkedList<CClientEntityList::CPVSNotifyInfo,unsigned short> &theList = ClientEntityList().GetPVSNotifiers();
	FOR_EACH_LL( theList, i )
	{
		CClientEntityList::CPVSNotifyInfo *pInfo = &theList[i];

		if ( pInfo->m_InPVSStatus & INPVS_YES )
		{
			// Ok, this entity already thinks it's in the PVS. No need to notify it.
			// We need to set the INPVS_YES_THISFRAME flag if it's in this frame at all, so we 
			// don't tell the entity it's not in the PVS anymore at the end of the frame.
			if ( !( pInfo->m_InPVSStatus & INPVS_THISFRAME ) )
			{
				if ( g_pClientLeafSystem->IsRenderableInPVS( pInfo->m_pRenderable ) )
				{
					pInfo->m_InPVSStatus |= INPVS_THISFRAME;
				}
			}
		}
		else
		{
			// This entity doesn't think it's in the PVS yet. If it is now in the PVS, let it know.
			if ( g_pClientLeafSystem->IsRenderableInPVS( pInfo->m_pRenderable ) )
			{
				pInfo->m_InPVSStatus |= ( INPVS_YES | INPVS_THISFRAME | INPVS_NEEDSNOTIFY );
			}
		}
	}	
}

void UpdatePVSNotifiers()
{
	MDLCACHE_CRITICAL_SECTION();

	// At this point, all the entities that were rendered in the previous frame have INPVS_THISFRAME set
	// so we can tell the entities that aren't in the PVS anymore so.
	CUtlLinkedList<CClientEntityList::CPVSNotifyInfo,unsigned short> &theList = ClientEntityList().GetPVSNotifiers();
	FOR_EACH_LL( theList, i )
	{
		CClientEntityList::CPVSNotifyInfo *pInfo = &theList[i];

		// If this entity thinks it's in the PVS, but it wasn't in the PVS this frame, tell it so.
		if ( pInfo->m_InPVSStatus & INPVS_YES )
		{
			if ( pInfo->m_InPVSStatus & INPVS_THISFRAME )
			{
				if ( pInfo->m_InPVSStatus & INPVS_NEEDSNOTIFY )
				{
					pInfo->m_pNotify->OnPVSStatusChanged( true );
				}
				// Clear it for the next time around.
				pInfo->m_InPVSStatus &= ~( INPVS_THISFRAME | INPVS_NEEDSNOTIFY );
			}
			else
			{
				pInfo->m_InPVSStatus &= ~INPVS_YES;
				pInfo->m_pNotify->OnPVSStatusChanged( false );
			}
		}
	}
}


void OnRenderStart()
{
	VPROF( "OnRenderStart" );
	MDLCACHE_CRITICAL_SECTION();
	MDLCACHE_COARSE_LOCK();



	partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, true );
	C_BaseEntity::SetAbsQueriesValid( false );

	Rope_ResetCounters();

	// Interpolate server entities and move aiments.
	{
		PREDICTION_TRACKVALUECHANGESCOPE( "interpolation" );
		C_BaseEntity::InterpolateServerEntities();
	}

	{
		// vprof node for this bloc of math
		VPROF( "OnRenderStart: dirty bone caches");
		// Invalidate any bone information.
		C_BaseAnimating::InvalidateBoneCaches();
		C_BaseFlex::InvalidateFlexCaches();

		C_BaseEntity::SetAbsQueriesValid( true );
		C_BaseEntity::EnableAbsRecomputations( true );

		// Enable access to all model bones except view models.
		// This is necessary for aim-ent computation to occur properly
		C_BaseAnimating::PushAllowBoneAccess( true, false, "OnRenderStart->CViewRender::SetUpView" ); // pops in CViewRender::SetUpView

		// FIXME: This needs to be done before the player moves; it forces
		// aiments the player may be attached to to forcibly update their position
		C_BaseEntity::MarkAimEntsDirty();
	}

	// Make sure the camera simulation happens before OnRenderStart, where it's used.
	// NOTE: the only thing that happens in CAM_Think is thirdperson related code.
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		input->CAM_Think();
	}

	C_BaseAnimating::PopBoneAccess( "OnRenderStart->CViewRender::SetUpView" ); // pops the (true, false) bone access set in OnRenderStart

	// Enable access to all model bones until rendering is done
	C_BaseAnimating::PushAllowBoneAccess( true, true, "CViewRender::SetUpView->OnRenderEnd" ); // pop is in OnRenderEnd()



#ifdef DEMOPOLISH_ENABLED
	// Update demo polish subsystem if necessary
	DemoPolish_Think();
#endif
	
	// This will place all entities in the correct position in world space and in the KD-tree
	// NOTE: Doing this before view->OnRenderStart() because the player can be in hierarchy with
	// a client-side animated entity.  So the viewport position is dependent on this animation sometimes.
	C_BaseAnimating::UpdateClientSideAnimations();

	// This will place the player + the view models + all parent
	// entities	at the correct abs position so that their attachment points
	// are at the correct location
	view->OnRenderStart();
	partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, false );

	// Process OnDataChanged events.
	ProcessOnDataChangedEvents();

	// Reset the overlay alpha. Entities can change the state of this in their think functions.
	g_SmokeFogOverlayAlpha = 0;	

	// This must occur prior to SimulatEntities,
	// which is where the client thinks for c_colorcorrection + c_colorcorrectionvolumes
	// update the color correction weights.
	// FIXME: The place where IGameSystem::Update is called should be in here
	// so we don't have to explicitly call ResetColorCorrectionWeights + SimulateEntities, etc.

	C_BaseAnimating::ThreadedBoneSetup();

	// Simulate all the entities.
	SimulateEntities();
	PhysicsSimulate();

	{
		VPROF_("Client TempEnts", 0, VPROF_BUDGETGROUP_CLIENT_SIM, false, BUDGETFLAG_CLIENT);
		// This creates things like temp entities.
		engine->FireEvents();

		// Update temp entities
		tempents->Update();

		// Update temp ent beams...
		beams->UpdateTempEntBeams();
		
		// Lock the frame from beam additions
		SetBeamCreationAllowed( false );
	}

	// Update particle effects (eventually, the effects should use Simulate() instead of having
	// their own update system).
	{
		VPROF_BUDGET( "ParticleMgr()->Simulate", VPROF_BUDGETGROUP_PARTICLE_SIMULATION );
		ParticleMgr()->Simulate( gpGlobals->frametime );
	}

	// Now that the view model's position is setup and aiments are marked dirty, update
	// their positions so they're in the leaf system correctly.
	C_BaseEntity::CalcAimEntPositions();

	// For entities marked for recording, post bone messages to IToolSystems
	if ( ToolsEnabled() )
	{
		C_BaseEntity::ToolRecordEntities();
	}

#if defined( REPLAY_ENABLED )
	// This will record any ragdolls if Replay mode is enabled on the server
	CReplayRagdollRecorder::Instance().Think();
	CReplayRagdollCache::Instance().Think();
#endif

	// update dynamic light state. Necessary for light cache to work properly for d- and elights
	engine->UpdateDAndELights();

	// Finally, link all the entities into the leaf system right before rendering.
	C_BaseEntity::AddVisibleEntities();

	g_pClientLeafSystem->RecomputeRenderableLeaves();
	g_pClientShadowMgr->ReprojectShadows();
	g_pClientShadowMgr->AdvanceFrame();
	g_pClientLeafSystem->DisableLeafReinsertion( true );
}

void OnRenderEnd()
{
	g_pClientLeafSystem->DisableLeafReinsertion( false );

	// Disallow access to bones (access is enabled in CViewRender::SetUpView).
	C_BaseAnimating::PopBoneAccess( "CViewRender::SetUpView->OnRenderEnd" );

	UpdatePVSNotifiers();

	DisplayBoneSetupEnts();
}



void CHLClient::FrameStageNotify( ClientFrameStage_t curStage )
{
	g_CurFrameStage = curStage;
	g_bEngineIsHLTV = engine->IsHLTV();

	switch( curStage )
	{
	default:
		break;

	case FRAME_RENDER_START:
		{
			VPROF( "CHLClient::FrameStageNotify FRAME_RENDER_START" );

			engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );

			// Last thing before rendering, run simulation.
			OnRenderStart();
		}
		break;
		
	case FRAME_RENDER_END:
		{
			VPROF( "CHLClient::FrameStageNotify FRAME_RENDER_END" );
			OnRenderEnd();

			engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );

			PREDICTION_SPEWVALUECHANGES();
		}
		break;
		
	case FRAME_NET_UPDATE_START:
		{
			VPROF( "CHLClient::FrameStageNotify FRAME_NET_UPDATE_START" );
			// disabled all recomputations while we update entities
			C_BaseEntity::EnableAbsRecomputations( false );
			C_BaseEntity::SetAbsQueriesValid( false );
			Interpolation_SetLastPacketTimeStamp( engine->GetLastTimeStamp() );
			partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, true );

			PREDICTION_STARTTRACKVALUE( "netupdate" );
		}
		break;
	case FRAME_NET_UPDATE_END:
		{
			ProcessCacheUsedMaterials();

			// reenable abs recomputation since now all entities have been updated
			C_BaseEntity::EnableAbsRecomputations( true );
			C_BaseEntity::SetAbsQueriesValid( true );
			partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, false );

			PREDICTION_ENDTRACKVALUE();
		}
		break;
	case FRAME_NET_UPDATE_POSTDATAUPDATE_START:
		{
			VPROF( "CHLClient::FrameStageNotify FRAME_NET_UPDATE_POSTDATAUPDATE_START" );
			PREDICTION_STARTTRACKVALUE( "postdataupdate" );
		}
		break;
	case FRAME_NET_UPDATE_POSTDATAUPDATE_END:
		{
			VPROF( "CHLClient::FrameStageNotify FRAME_NET_UPDATE_POSTDATAUPDATE_END" );
			PREDICTION_ENDTRACKVALUE();
			// Let prediction copy off pristine data
			prediction->PostEntityPacketReceived();
			HLTVCamera()->PostEntityPacketReceived();
#if defined( REPLAY_ENABLED )
			ReplayCamera()->PostEntityPacketReceived();
#endif
		}
		break;
	case FRAME_START:
		{
			// Mark the frame as open for client fx additions
			SetFXCreationAllowed( true );
			SetBeamCreationAllowed( true );
			C_BaseEntity::CheckCLInterpChanged();
			engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );
		}
		break;
	}
}

CSaveRestoreData *SaveInit( int size );

// Save/restore system hooks
CSaveRestoreData  *CHLClient::SaveInit( int size )
{
	return ::SaveInit(size);
}

void CHLClient::SaveWriteFields( CSaveRestoreData *pSaveData, const char *pname, void *pBaseData, datamap_t *pMap, typedescription_t *pFields, int fieldCount )
{
	CSave saveHelper( pSaveData );
	saveHelper.WriteFields( pname, pBaseData, pMap, pFields, fieldCount );
}

void CHLClient::SaveReadFields( CSaveRestoreData *pSaveData, const char *pname, void *pBaseData, datamap_t *pMap, typedescription_t *pFields, int fieldCount )
{
	CRestore restoreHelper( pSaveData );
	restoreHelper.ReadFields( pname, pBaseData, pMap, pFields, fieldCount );
}

void CHLClient::PreSave( CSaveRestoreData *s )
{
	g_pGameSaveRestoreBlockSet->PreSave( s );
}

void CHLClient::Save( CSaveRestoreData *s )
{
	CSave saveHelper( s );
	g_pGameSaveRestoreBlockSet->Save( &saveHelper );
}

void CHLClient::WriteSaveHeaders( CSaveRestoreData *s )
{
	CSave saveHelper( s );
	g_pGameSaveRestoreBlockSet->WriteSaveHeaders( &saveHelper );
	g_pGameSaveRestoreBlockSet->PostSave();
}

void CHLClient::ReadRestoreHeaders( CSaveRestoreData *s )
{
	CRestore restoreHelper( s );
	g_pGameSaveRestoreBlockSet->PreRestore();
	g_pGameSaveRestoreBlockSet->ReadRestoreHeaders( &restoreHelper );
}

void CHLClient::Restore( CSaveRestoreData *s, bool b )
{
	CRestore restore(s);
	g_pGameSaveRestoreBlockSet->Restore( &restore, b );
	g_pGameSaveRestoreBlockSet->PostRestore();
}

static CUtlVector<EHANDLE> g_RestoredEntities;

void AddRestoredEntity( C_BaseEntity *pEntity )
{
	if ( !pEntity )
		return;

	g_RestoredEntities.AddToTail( EHANDLE(pEntity) );
}

void CHLClient::DispatchOnRestore()
{
	for ( int i = 0; i < g_RestoredEntities.Count(); i++ )
	{
		if ( g_RestoredEntities[i] != NULL )
		{
			MDLCACHE_CRITICAL_SECTION();
			g_RestoredEntities[i]->OnRestore();
		}
	}
	g_RestoredEntities.RemoveAll();
}

void CHLClient::WriteSaveGameScreenshot( const char *pFilename )
{
	// Single player doesn't support split screen yet!!!
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
	view->WriteSaveGameScreenshot( pFilename );
}

// Given a list of "S(wavname) S(wavname2)" tokens, look up the localized text and emit
//  the appropriate close caption if running with closecaption = 1
void CHLClient::EmitSentenceCloseCaption( char const *tokenstream )
{
	extern ConVar closecaption;
	
	if ( !closecaption.GetBool() )
		return;

	if ( m_pHudCloseCaption )
	{
		m_pHudCloseCaption->ProcessSentenceCaptionStream( tokenstream );
	}
}


void CHLClient::EmitCloseCaption( char const *captionname, float duration )
{
	extern ConVar closecaption;

	if ( !closecaption.GetBool() )
		return;

	if ( m_pHudCloseCaption )
	{
		m_pHudCloseCaption->ProcessCaption( captionname, duration );
	}
}

CStandardRecvProxies* CHLClient::GetStandardRecvProxies()
{
	return &g_StandardRecvProxies;
}

bool CHLClient::CanRecordDemo( char *errorMsg, int length ) const
{
	if ( GetClientModeNormal() )
	{
		return GetClientModeNormal()->CanRecordDemo( errorMsg, length );
	}

	return true;
}

void CHLClient::OnDemoRecordStart( char const* pDemoBaseName )
{
#ifdef DEMOPOLISH_ENABLED
	if ( IsDemoPolishEnabled() )
	{
		if ( !CDemoPolishRecorder::Instance().Init( pDemoBaseName ) )
		{
			CDemoPolishRecorder::Instance().Shutdown();
		}
	}
#endif
}

void CHLClient::OnDemoRecordStop()
{
#ifdef DEMOPOLISH_ENABLED
	if ( DemoPolish_GetRecorder().m_bInit )
	{
		DemoPolish_GetRecorder().Shutdown();
	}
#endif
}

void CHLClient::OnDemoPlaybackStart( char const* pDemoBaseName )
{
#ifdef DEMOPOLISH_ENABLED
	if ( IsDemoPolishEnabled() )
	{
		Assert( pDemoBaseName );
		if ( !DemoPolish_GetController().Init( pDemoBaseName ) )
		{
			DemoPolish_GetController().Shutdown();
		}
	}
#endif

#if defined( REPLAY_ENABLED )
	// Load any ragdoll override frames from disk
	char szRagdollFile[MAX_OSPATH];
	V_snprintf( szRagdollFile, sizeof(szRagdollFile), "%s.dmx", pDemoBaseName );
	CReplayRagdollCache::Instance().Init( szRagdollFile );
#endif
}

void CHLClient::OnDemoPlaybackStop()
{
#ifdef DEMOPOLISH_ENABLED
	if ( DemoPolish_GetController().m_bInit )
	{
		DemoPolish_GetController().Shutdown();
	}
#endif

#if defined( REPLAY_ENABLED )
	CReplayRagdollCache::Instance().Shutdown();
#endif
}

void CHLClient::RecordDemoPolishUserInput( int nCmdIndex )
{
#ifdef DEMOPOLISH_ENABLED
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	Assert( engine->IsRecordingDemo() );
	Assert( IsDemoPolishEnabled() );	// NOTE: cl_demo_polish_enabled checked in engine.
	
	CUserCmd const* pUserCmd = input->GetUserCmd( nSlot, nCmdIndex );
	Assert( pUserCmd );
	if ( pUserCmd )
	{
		DemoPolish_GetRecorder().RecordUserInput( pUserCmd );
	}
#endif
}

bool CHLClient::CacheReplayRagdolls( const char* pFilename, int nStartTick )
{
#if defined( REPLAY_ENABLED )
	return Replay_CacheRagdolls( pFilename, nStartTick );
#else
	return false;
#endif
}

// NEW INTERFACES
// save game screenshot writing
void CHLClient::WriteSaveGameScreenshotOfSize( const char *pFilename, int width, int height )
{
	view->WriteSaveGameScreenshotOfSize( pFilename, width, height );
}

// See RenderViewInfo_t
void CHLClient::RenderView( const CViewSetup &setup, int nClearFlags, int whatToDraw )
{
	VPROF("RenderView");
	view->RenderView( setup, setup, nClearFlags, whatToDraw );
}

bool CHLClient::ShouldHideLoadingPlaque( void )
{
	return false;

}

void CHLClient::OnActiveSplitscreenPlayerChanged( int nNewSlot )
{
}

void CHLClient::OnSplitScreenStateChanged()
{
	VGui_OnSplitScreenStateChanged();
	IterateRemoteSplitScreenViewSlots_Push( true );
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( i );
		GetClientMode()->Layout();
		GetHud().OnSplitScreenStateChanged();
	}
	IterateRemoteSplitScreenViewSlots_Pop();

	GetFullscreenClientMode()->Layout( true );

	vgui::surface()->ResetFontCaches();

	// Update visibility for all ents so that the second viewport for the split player guy looks right, etc.
	C_BaseEntityIterator iterator;
	C_BaseEntity *pEnt;
	while ( (pEnt = iterator.Next()) != NULL )	
	{
		pEnt->UpdateVisibility();
	}
}

void CHLClient::CenterStringOff()
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetCenterPrint()->Clear();
	}

}

void CHLClient::OnScreenSizeChanged( int nOldWidth, int nOldHeight )
{
	// Tell split screen system
	VGui_OnScreenSizeChanged();
}

IMaterialProxy *CHLClient::InstantiateMaterialProxy( const char *proxyName )
{
#ifdef GAMEUI_UISYSTEM2_ENABLED
	IMaterialProxy *pProxy = g_pGameUIGameSystem->CreateProxy( proxyName );
	if ( pProxy )
		return pProxy;
#endif
	return GetMaterialProxyDict().CreateProxy( proxyName );
}

vgui::VPANEL CHLClient::GetFullscreenClientDLLVPanel( void )
{
	return VGui_GetFullscreenRootVPANEL();
}

int XBX_GetActiveUserId()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return XBX_GetUserId( GET_ACTIVE_SPLITSCREEN_SLOT() );
}

//-----------------------------------------------------------------------------
// Purpose: Marks entities as touching
// Input  : *e1 - 
//			*e2 - 
//-----------------------------------------------------------------------------
void CHLClient::MarkEntitiesAsTouching( IClientEntity *e1, IClientEntity *e2 )
{
	CBaseEntity *entity = e1->GetBaseEntity();
	CBaseEntity *entityTouched = e2->GetBaseEntity();
	if ( entity && entityTouched )
	{
		trace_t tr;
		UTIL_ClearTrace( tr );
		tr.endpos = (entity->GetAbsOrigin() + entityTouched->GetAbsOrigin()) * 0.5;
		entity->PhysicsMarkEntitiesAsTouching( entityTouched, tr );
	}
}

class CKeyBindingListenerMgr : public IKeyBindingListenerMgr
{
public:
	struct BindingListeners_t
	{
		BindingListeners_t()
		{
		}

		BindingListeners_t( const BindingListeners_t &other )
		{
			m_List.CopyArray( other.m_List.Base(), other.m_List.Count() );
		}

		CUtlVector< IKeyBindingListener * > m_List;
	};

	// Callback when button is bound
	virtual void AddListenerForCode( IKeyBindingListener *pListener, ButtonCode_t buttonCode )
	{
		CUtlVector< IKeyBindingListener * > &list = m_CodeListeners[ buttonCode ];
		if ( list.Find( pListener ) != list.InvalidIndex() )
			return;
		list.AddToTail( pListener );
	}

	// Callback whenver binding is set to a button
	virtual void AddListenerForBinding( IKeyBindingListener *pListener, char const *pchBindingString )
	{
		int idx = m_BindingListeners.Find( pchBindingString );
		if ( idx == m_BindingListeners.InvalidIndex() )
		{
			idx = m_BindingListeners.Insert( pchBindingString );
		}

		CUtlVector< IKeyBindingListener * > &list = m_BindingListeners[ idx ].m_List;
		if ( list.Find( pListener ) != list.InvalidIndex() )
			return;
		list.AddToTail( pListener );
	}

	virtual void RemoveListener( IKeyBindingListener *pListener )
	{
		for ( int i = 0; i < ARRAYSIZE( m_CodeListeners ); ++i )
		{
			CUtlVector< IKeyBindingListener * > &list = m_CodeListeners[ i ];
			list.FindAndRemove( pListener );
		}

		for ( int i = m_BindingListeners.First(); i != m_BindingListeners.InvalidIndex(); i = m_BindingListeners.Next( i ) )
		{
			CUtlVector< IKeyBindingListener * > &list = m_BindingListeners[ i ].m_List;
			list.FindAndRemove( pListener );
		}
	}

	void OnKeyBindingChanged( ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding )
	{
		int nSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

		CUtlVector< IKeyBindingListener * > &list = m_CodeListeners[ buttonCode ];
		for ( int i = 0 ; i < list.Count(); ++i )
		{
			list[ i ]->OnKeyBindingChanged( nSplitScreenSlot, buttonCode, pchKeyName, pchNewBinding );
		}

		int idx = m_BindingListeners.Find( pchNewBinding );
		if ( idx != m_BindingListeners.InvalidIndex() )
		{
			CUtlVector< IKeyBindingListener * > &list = m_BindingListeners[ idx ].m_List;
			for ( int i = 0 ; i < list.Count(); ++i )
			{
				list[ i ]->OnKeyBindingChanged( nSplitScreenSlot, buttonCode, pchKeyName, pchNewBinding );
			}
		}
	}

private:
	CUtlVector< IKeyBindingListener * > m_CodeListeners[ BUTTON_CODE_COUNT ];
	CUtlDict< BindingListeners_t, int > m_BindingListeners;
};

static CKeyBindingListenerMgr g_KeyBindingListenerMgr;

IKeyBindingListenerMgr *g_pKeyBindingListenerMgr = &g_KeyBindingListenerMgr;
void CHLClient::OnKeyBindingChanged( ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding )
{
	g_KeyBindingListenerMgr.OnKeyBindingChanged( buttonCode, pchKeyName, pchNewBinding );
}

void CHLClient::SetBlurFade( float scale )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetClientMode()->SetBlurFade( scale );
	}
}

void CHLClient::ResetHudCloseCaption()
{
	if ( !IsX360() )
	{
		// only xbox needs to force the close caption system to remount
		return;
	}

	if ( m_pHudCloseCaption )
	{
		// force the caption dictionary to remount
		m_pHudCloseCaption->InitCaptionDictionary( NULL, true );
	}
}

bool CHLClient::SupportsRandomMaps()
{
#ifdef INFESTED_DLL
	return true;
#else
	return false;
#endif
}

extern IViewRender *view;

//-----------------------------------------------------------------------------
// Purpose: interface from materialsystem to client, currently just for recording into tools
//-----------------------------------------------------------------------------
class CClientMaterialSystem : public IClientMaterialSystem
{
	virtual HTOOLHANDLE GetCurrentRecordingEntity()
	{
		if ( !ToolsEnabled() )
			return HTOOLHANDLE_INVALID;

		if ( !clienttools->IsInRecordingMode() )
			return HTOOLHANDLE_INVALID;

		C_BaseEntity *pEnt = view->GetCurrentlyDrawingEntity();
		if ( !pEnt || !pEnt->IsToolRecording() )
			return HTOOLHANDLE_INVALID;

		return pEnt->GetToolHandle();
	}
	virtual void PostToolMessage( HTOOLHANDLE hEntity, KeyValues *pMsg )
	{
		ToolFramework_PostToolMessage( hEntity, pMsg );
	}
};

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CClientMaterialSystem s_ClientMaterialSystem;
IClientMaterialSystem *g_pClientMaterialSystem = &s_ClientMaterialSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CClientMaterialSystem, IClientMaterialSystem, VCLIENTMATERIALSYSTEM_INTERFACE_VERSION, s_ClientMaterialSystem );

