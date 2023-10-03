//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef CDLL_CLIENT_INT_H
#define CDLL_CLIENT_INT_H
#ifdef _WIN32
#pragma once
#endif

#include "iclientnetworkable.h"
#include "utllinkedlist.h"
#include "cdll_int.h"
#include "shareddefs.h"
#include "eiface.h"
#include "tier3/tier3.h"
#include "tier2/tier2_logging.h"

class IVModelRender;
class IVEngineClient;
class IVModelRender;
class IVEfx;
class IVRenderView;
class IVDebugOverlay;
class IMaterialSystem;
class IMaterialSystemStub;
class IDataCache;
class IMDLCache;
class IVModelInfoClient;
class IEngineVGui;
class ISpatialPartition;
class IBaseClientDLL;
class ISpatialPartition;
class IFileSystem;
class IStaticPropMgrClient;
class IShadowMgr;
class IEngineSound;
class IMatSystemSurface;
class IMaterialSystemHardwareConfig;
class ISharedGameRules;
class IEngineTrace;
class IGameUIFuncs;
class IGameEventManager2;
class IPhysicsGameTrace;
class CGlobalVarsBase;
class IClientTools;
class C_BaseAnimating;
class IColorCorrectionSystem;
class IInputSystem;
class ISceneFileCache;
class IXboxSystem;	// Xbox 360 only
class IAvi;
class IBik;
class CSteamAPIContext;
class IReplayHistoryManager;
class ISoundEmitterSystemBase;
enum CPULevel_t;
enum GPULevel_t;

extern IVModelRender *modelrender;
extern IVEngineClient	*engine;
extern IVModelRender *modelrender;
extern IVEfx *effects;
extern IVRenderView *render;
extern IVDebugOverlay *debugoverlay;
extern IMaterialSystemStub *materials_stub;
extern IVModelInfoClient *modelinfo;
extern IEngineVGui *enginevgui;
extern ISpatialPartition* partition;
extern IBaseClientDLL *clientdll;
extern IFileSystem *filesystem;
extern IStaticPropMgrClient *staticpropmgr;
extern IShadowMgr *shadowmgr;
extern IEngineSound *enginesound;
extern IEngineTrace *enginetrace;
extern IFileLoggingListener	*filelogginglistener;
extern IGameUIFuncs *gameuifuncs;
extern IGameEventManager2 *gameeventmanager;
extern IPhysicsGameTrace *physgametrace;
extern CGlobalVarsBase *gpGlobals;
extern IClientTools *clienttools;
extern IInputSystem *inputsystem;
extern ISceneFileCache *scenefilecache;
extern IXboxSystem *xboxsystem;	// Xbox 360 only
extern IAvi *avi;
extern IBik *bik;
extern IUploadGameStats *gamestatsuploader;
extern CSteamAPIContext *steamapicontext;
extern ISoundEmitterSystemBase *soundemitterbase;

#ifdef INFESTED_DLL
class IASW_Mission_Chooser;
extern IASW_Mission_Chooser *missionchooser;
#endif
#if defined( REPLAY_ENABLED )
extern IReplayHistoryManager *g_pReplayHistoryManager;
#endif

// Returns the CPU/GPU level
CPULevel_t GetCPULevel();
// Returns the actual value of the CPU level convar, even on the 360
CPULevel_t GetActualCPULevel();
GPULevel_t GetGPULevel();
void ConfigureCurrentSystemLevel();

// Set to true between LevelInit and LevelShutdown.
extern bool	g_bLevelInitialized;
extern bool g_bTextMode;
extern bool g_bEngineIsHLTV;

// Returns true if a new OnDataChanged event is registered for this frame.
bool AddDataChangeEvent( IClientNetworkable *ent, DataUpdateType_t updateType, int *pStoredEvent );

void ClearDataChangedEvent( int iStoredEvent );

//-----------------------------------------------------------------------------
// Precaches a material
//-----------------------------------------------------------------------------
void PrecacheMaterial( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMaterialIndex( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts precached material indices into strings
//-----------------------------------------------------------------------------
const char *GetMaterialNameFromIndex( int nIndex );

//-----------------------------------------------------------------------------
// Precache-related methods for particle systems
//-----------------------------------------------------------------------------
int PrecacheParticleSystem( const char *pParticleSystemName );
int GetParticleSystemIndex( const char *pParticleSystemName );
const char *GetParticleSystemNameFromIndex( int nIndex );


//-----------------------------------------------------------------------------
// Precache-related methods for effects
//-----------------------------------------------------------------------------
void PrecacheEffect( const char *pEffectName );


//-----------------------------------------------------------------------------
// Called during bone setup to test perf
//-----------------------------------------------------------------------------
void TrackBoneSetupEnt( C_BaseAnimating *pEnt );

bool IsEngineThreaded();

class CVGuiScreenSizeSplitScreenPlayerGuard
{
public:
	CVGuiScreenSizeSplitScreenPlayerGuard( bool bActive, int slot, int nOldSlot );
	CVGuiScreenSizeSplitScreenPlayerGuard( bool bActive, C_BaseEntity *pEntity, int nOldSlot );
	~CVGuiScreenSizeSplitScreenPlayerGuard();
private:

	bool m_bNoRestore;
	bool m_bOldSetting;
	int m_nOldSize[ 2 ];
};

class CSetActiveSplitScreenPlayerGuard : public CVGuiScreenSizeSplitScreenPlayerGuard
{
public:
	CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, int slot, int nOldSlot, bool bSetVguiScreenSize );
	CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, C_BaseEntity *pEntity, int nOldSlot, bool bSetVguiScreenSize );
	~CSetActiveSplitScreenPlayerGuard();
private:
	bool	m_bChanged;
	char const *m_pchContext;
	int m_nLine;
	int	 m_nSaveSlot;
	bool m_bSaveGetLocalPlayerAllowed;
};

class CHackForGetLocalPlayerAccessAllowedGuard
{
public:
	CHackForGetLocalPlayerAccessAllowedGuard( char const *pszContext, bool bOldSlot );
	~CHackForGetLocalPlayerAccessAllowedGuard();
private:
	bool m_bChanged;
	char const *m_pszContext;
	bool m_bSaveGetLocalPlayerAllowed;
};

class CVGuiAbsPosSplitScreenPlayerGuard
{
public:
	CVGuiAbsPosSplitScreenPlayerGuard( int slot, int nOldSlot, bool bInvert = false );
	~CVGuiAbsPosSplitScreenPlayerGuard();
private:
	bool m_bNoRestore;
};

int FirstValidSplitScreenSlot();
int NextValidSplitScreenSlot( int i );
bool IsValidSplitScreenSlot( int i );
void IterateRemoteSplitScreenViewSlots_Push( bool bSet ); //some split screen loops should be made aware of remote views we'd like to use
void IterateRemoteSplitScreenViewSlots_Pop( void );
class C_BasePlayer;
void AddRemoteSplitScreenViewPlayer( C_BasePlayer *pPlayer ); //adds a nonlocal player as a view we'd like to see in split screen
void RemoveRemoteSplitScreenViewPlayer( C_BasePlayer *pPlayer ); //removes a nonlocal player view from split screen
C_BasePlayer *GetSplitScreenViewPlayer( int nSlot ); //allows return of split screen views of nonlocal players
bool IsLocalSplitScreenPlayer( int nSlot ); //true is a full splitscreen player. false is just a view of a remote player

#if defined( SPLIT_SCREEN_STUBS )

#define VGUI_SCREENSIZE_SPLITSCREEN_GUARD( slot ) 
#define ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot )
#define ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( entity )

#define ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( slot )
#define ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT_VGUI( entity )

#define HACK_GETLOCALPLAYER_GUARD( desc )
#define VGUI_ABSPOS_SPLITSCREEN_GUARD( slot )
#define VGUI_ABSPOS_SPLITSCREEN_GUARD_INVERT( slot )

#define FOR_EACH_VALID_SPLITSCREEN_PLAYER( iteratorName ) for ( int iteratorName = 0; iteratorName == 0; ++iteratorName )

#define ASSERT_LOCAL_PLAYER_RESOLVABLE()
#define ASSERT_LOCAL_PLAYER_NOT_RESOLVABLE() 
#define GET_ACTIVE_SPLITSCREEN_SLOT() ( 0 )

FORCEINLINE uint32 ComputeSplitscreenRenderingFlags( IClientRenderable *pRenderable )
{
	return 0xFFFFFFFF;
}

#else

#define VGUI_SCREENSIZE_SPLITSCREEN_GUARD( slot ) CVGuiScreenSizeSplitScreenPlayerGuard s_VGuiSSGuard( slot, engine->GetActiveSplitScreenPlayerSlot() );
#define ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot )	CSetActiveSplitScreenPlayerGuard g_SSGuard( __FILE__, __LINE__, slot, engine->GetActiveSplitScreenPlayerSlot(), false );
#define ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( entity )	CSetActiveSplitScreenPlayerGuard g_SSEGuard( __FILE__, __LINE__, entity, engine->GetActiveSplitScreenPlayerSlot(), false );

#define ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( slot )	CSetActiveSplitScreenPlayerGuard g_SSGuardNoVgui( __FILE__, __LINE__, slot, engine->GetActiveSplitScreenPlayerSlot(), true );
#define ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT_VGUI( entity )	CSetActiveSplitScreenPlayerGuard g_SSEGuardNoVgui( __FILE__, __LINE__, entity, engine->GetActiveSplitScreenPlayerSlot(), true );


#define HACK_GETLOCALPLAYER_GUARD( desc )	CHackForGetLocalPlayerAccessAllowedGuard g_HackGLPGuard( desc, engine->IsLocalPlayerResolvable() );
#define VGUI_ABSPOS_SPLITSCREEN_GUARD( slot ) CVGuiAbsPosSplitScreenPlayerGuard s_VGuiAbsPosGuard( slot, engine->GetActiveSplitScreenPlayerSlot() );
#define VGUI_ABSPOS_SPLITSCREEN_GUARD_INVERT( slot ) CVGuiAbsPosSplitScreenPlayerGuard s_VGuiAbsPosGuard( slot, engine->GetActiveSplitScreenPlayerSlot(), true );

#define FOR_EACH_VALID_SPLITSCREEN_PLAYER( iteratorName )						\
	for ( int iteratorName = FirstValidSplitScreenSlot();				\
				iteratorName != -1;												\
				iteratorName = NextValidSplitScreenSlot( iteratorName ) )	

// Uncomment this to be able to look for guard asserts in release builds
// #define SS_SHIPPING_ASSERTS

#if defined( SS_SHIPPING_ASSERTS )
#define ASSERT_LOCAL_PLAYER_RESOLVABLE() _AssertMsg( engine->IsLocalPlayerResolvable(), _T("Assertion Failed: ") _T("engine->IsLocalPlayerResolvable()"), ((void)0), false )
#else
#define ASSERT_LOCAL_PLAYER_RESOLVABLE() Assert( engine->IsLocalPlayerResolvable() );
#endif

#if defined( SS_SHIPPING_ASSERTS )
#define ASSERT_LOCAL_PLAYER_NOT_RESOLVABLE() _AssertMsg( !engine->IsLocalPlayerResolvable(), _T("Assertion Failed: ") _T("!engine->IsLocalPlayerResolvable()"), ((void)0), false )
#else
#define ASSERT_LOCAL_PLAYER_NOT_RESOLVABLE() Assert( !engine->IsLocalPlayerResolvable() );
#endif

#define GET_ACTIVE_SPLITSCREEN_SLOT() engine->GetActiveSplitScreenPlayerSlot()

//-----------------------------------------------------------------------------
// Default implementation of compute splitscreen rendering flags
//-----------------------------------------------------------------------------
FORCEINLINE uint32 ComputeSplitscreenRenderingFlags( IClientRenderable *pRenderable )
{
	if ( IsSplitScreenSupported() )
	{
		int nFlags = 0;
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			if ( pRenderable->ShouldDrawForSplitScreenUser( i ) )
			{
				nFlags |= 1 << i;
			}
		}
		return nFlags;
	}

	return 0xFFFFFFFF;
}

#endif

inline C_BasePlayer *GetSplitScreenViewPlayer( void ) { return GetSplitScreenViewPlayer( GET_ACTIVE_SPLITSCREEN_SLOT() ); };
inline bool IsLocalSplitScreenPlayer( void ) { return IsLocalSplitScreenPlayer( GET_ACTIVE_SPLITSCREEN_SLOT() ); };

// Returns XBX_GetUserId( GET_ACTIVE_SPLITSCREEN_SLOT() )
int XBX_GetActiveUserId();

#define XBX_GetPrimaryUserId() _Use_XBX_GetActiveUserId_Instead

#endif // CDLL_CLIENT_INT_H
