//===== Copyright © 2005-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "interfaces/interfaces.h"


//-----------------------------------------------------------------------------
// Tier 1 libraries
//-----------------------------------------------------------------------------
ICvar *cvar = 0;
ICvar *g_pCVar = 0;
IProcessUtils *g_pProcessUtils = 0;
static bool s_bConnected = false;
IPhysics2 *g_pPhysics2 = 0;
IPhysics2ActorManager *g_pPhysics2ActorManager = 0;
IPhysics2ResourceManager *g_pPhysics2ResourceManager = 0;
IEventSystem *g_pEventSystem = 0;
ILocalize *g_pLocalize = 0;

// for utlsortvector.h
#ifndef _WIN32
void *g_pUtlSortVectorQSortContext = NULL;
#endif



//-----------------------------------------------------------------------------
// Tier 2 libraries
//-----------------------------------------------------------------------------
IResourceSystem *g_pResourceSystem = 0;
IRenderDeviceMgr *g_pRenderDeviceMgr = 0;
IFileSystem *g_pFullFileSystem = 0;
IAsyncFileSystem *g_pAsyncFileSystem = 0;
IMaterialSystem *materials = 0;
IMaterialSystem *g_pMaterialSystem = 0;
IMaterialSystem2 *g_pMaterialSystem2 = 0;
IInputSystem *g_pInputSystem = 0;
IInputStackSystem *g_pInputStackSystem = 0;
INetworkSystem *g_pNetworkSystem = 0;
ISoundSystem *g_pSoundSystem = 0;
IMaterialSystemHardwareConfig *g_pMaterialSystemHardwareConfig = 0;
IDebugTextureInfo *g_pMaterialSystemDebugTextureInfo = 0;
IVBAllocTracker *g_VBAllocTracker = 0;
IColorCorrectionSystem *colorcorrection = 0;
IP4 *p4 = 0;
IMdlLib *mdllib = 0;
IQueuedLoader *g_pQueuedLoader = 0;
IResourceAccessControl *g_pResourceAccessControl = 0;
IPrecacheSystem *g_pPrecacheSystem = 0;
ISceneSystem *g_pSceneSystem = 0;

#if defined( PLATFORM_X360 )
IXboxInstaller *g_pXboxInstaller = 0;
#endif

IMatchFramework *g_pMatchFramework = 0;
IGameUISystemMgr *g_pGameUISystemMgr = 0;


//-----------------------------------------------------------------------------
// Not exactly a global, but we're going to keep track of these here anyways
//-----------------------------------------------------------------------------
IRenderDevice *g_pRenderDevice = 0;
IRenderHardwareConfig *g_pRenderHardwareConfig = 0;


//-----------------------------------------------------------------------------
// Tier3 libraries
//-----------------------------------------------------------------------------
IMeshSystem *g_pMeshSystem = 0;
IStudioRender *g_pStudioRender = 0;
IStudioRender *studiorender = 0;
IMatSystemSurface *g_pMatSystemSurface = 0;
vgui::IInput *g_pVGuiInput = 0;
vgui::ISurface *g_pVGuiSurface = 0;
vgui::IPanel *g_pVGuiPanel = 0;
vgui::IVGui	*g_pVGui = 0;
vgui::IVGUILocalize *g_pVGuiLocalize = 0;
vgui::ISchemeManager *g_pVGuiSchemeManager = 0;
vgui::ISystem *g_pVGuiSystem = 0;
IDataCache *g_pDataCache = 0;
IMDLCache *g_pMDLCache = 0;
IMDLCache *mdlcache = 0;
IAvi *g_pAVI = 0;
IBik *g_pBIK = 0;
IDmeMakefileUtils *g_pDmeMakefileUtils = 0;
IPhysicsCollision *g_pPhysicsCollision = 0;
ISoundEmitterSystemBase *g_pSoundEmitterSystem = 0;
IWorldRendererMgr *g_pWorldRendererMgr = 0;
IVGuiRenderSurface *g_pVGuiRenderSurface = 0;


//-----------------------------------------------------------------------------
// Mapping of interface string to globals
//-----------------------------------------------------------------------------
struct InterfaceGlobals_t
{
	const char *m_pInterfaceName;
	void *m_ppGlobal;
};

static InterfaceGlobals_t g_pInterfaceGlobals[] =
{
	{ CVAR_INTERFACE_VERSION, &cvar },
	{ CVAR_INTERFACE_VERSION, &g_pCVar },
	{ EVENTSYSTEM_INTERFACE_VERSION, &g_pEventSystem },
	{ PROCESS_UTILS_INTERFACE_VERSION, &g_pProcessUtils },
	{ VPHYSICS2_INTERFACE_VERSION, &g_pPhysics2 },
	{ VPHYSICS2_ACTOR_MGR_INTERFACE_VERSION, &g_pPhysics2ActorManager },
	{ VPHYSICS2_RESOURCE_MGR_INTERFACE_VERSION, &g_pPhysics2ResourceManager },
	{ FILESYSTEM_INTERFACE_VERSION, &g_pFullFileSystem },
	{ ASYNCFILESYSTEM_INTERFACE_VERSION, &g_pAsyncFileSystem },
	{ RESOURCESYSTEM_INTERFACE_VERSION, &g_pResourceSystem },
	{ MATERIAL_SYSTEM_INTERFACE_VERSION, &g_pMaterialSystem },
	{ MATERIAL_SYSTEM_INTERFACE_VERSION, &materials },
	{ MATERIAL_SYSTEM2_INTERFACE_VERSION, &g_pMaterialSystem2 },
	{ INPUTSYSTEM_INTERFACE_VERSION, &g_pInputSystem },
	{ INPUTSTACKSYSTEM_INTERFACE_VERSION, &g_pInputStackSystem },
	{ NETWORKSYSTEM_INTERFACE_VERSION, &g_pNetworkSystem },
	{ RENDER_DEVICE_MGR_INTERFACE_VERSION, &g_pRenderDeviceMgr },
	{ MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, &g_pMaterialSystemHardwareConfig },
	{ SOUNDSYSTEM_INTERFACE_VERSION, &g_pSoundSystem },
	{ DEBUG_TEXTURE_INFO_VERSION, &g_pMaterialSystemDebugTextureInfo },
	{ VB_ALLOC_TRACKER_INTERFACE_VERSION, &g_VBAllocTracker },
	{ COLORCORRECTION_INTERFACE_VERSION, &colorcorrection },
	{ P4_INTERFACE_VERSION, &p4 },
	{ MDLLIB_INTERFACE_VERSION, &mdllib },
	{ QUEUEDLOADER_INTERFACE_VERSION, &g_pQueuedLoader },
	{ RESOURCE_ACCESS_CONTROL_INTERFACE_VERSION, &g_pResourceAccessControl },
	{ PRECACHE_SYSTEM_INTERFACE_VERSION, &g_pPrecacheSystem },
	{ STUDIO_RENDER_INTERFACE_VERSION, &g_pStudioRender },
	{ STUDIO_RENDER_INTERFACE_VERSION, &studiorender },
	{ VGUI_IVGUI_INTERFACE_VERSION, &g_pVGui },
	{ VGUI_INPUT_INTERFACE_VERSION, &g_pVGuiInput },
	{ VGUI_PANEL_INTERFACE_VERSION, &g_pVGuiPanel },
	{ VGUI_SURFACE_INTERFACE_VERSION, &g_pVGuiSurface },
	{ VGUI_SCHEME_INTERFACE_VERSION, &g_pVGuiSchemeManager },
	{ VGUI_SYSTEM_INTERFACE_VERSION, &g_pVGuiSystem },
	{ LOCALIZE_INTERFACE_VERSION, &g_pLocalize },
	{ LOCALIZE_INTERFACE_VERSION, &g_pVGuiLocalize },
	{ MAT_SYSTEM_SURFACE_INTERFACE_VERSION, &g_pMatSystemSurface },
	{ DATACACHE_INTERFACE_VERSION, &g_pDataCache },
	{ MDLCACHE_INTERFACE_VERSION, &g_pMDLCache },
	{ MDLCACHE_INTERFACE_VERSION, &mdlcache },
	{ AVI_INTERFACE_VERSION, &g_pAVI },
	{ BIK_INTERFACE_VERSION, &g_pBIK },
	{ DMEMAKEFILE_UTILS_INTERFACE_VERSION, &g_pDmeMakefileUtils },
	{ VPHYSICS_COLLISION_INTERFACE_VERSION, &g_pPhysicsCollision },
	{ SOUNDEMITTERSYSTEM_INTERFACE_VERSION, &g_pSoundEmitterSystem },
	{ MESHSYSTEM_INTERFACE_VERSION, &g_pMeshSystem },
	{ RENDER_DEVICE_INTERFACE_VERSION, &g_pRenderDevice },
	{ RENDER_HARDWARECONFIG_INTERFACE_VERSION, &g_pRenderHardwareConfig },
	{ SCENESYSTEM_INTERFACE_VERSION, &g_pSceneSystem },
	{ WORLD_RENDERER_MGR_INTERFACE_VERSION, &g_pWorldRendererMgr },
	{ RENDER_SYSTEM_SURFACE_INTERFACE_VERSION, &g_pVGuiRenderSurface },

#if defined( _X360 )
	{ XBOXINSTALLER_INTERFACE_VERSION, &g_pXboxInstaller },
#endif

	{ MATCHFRAMEWORK_INTERFACE_VERSION, &g_pMatchFramework },
	{ GAMEUISYSTEMMGR_INTERFACE_VERSION, &g_pGameUISystemMgr },
};


//-----------------------------------------------------------------------------
// The # of times this DLL has been connected
//-----------------------------------------------------------------------------
static int s_nConnectionCount = 0;


//-----------------------------------------------------------------------------
// At each level of connection, we're going to keep track of which interfaces
// we filled in. When we disconnect, we'll clear those interface pointers out.
//-----------------------------------------------------------------------------
struct ConnectionRegistration_t
{
	void *m_ppGlobalStorage;
	int m_nConnectionPhase;
};

static int s_nRegistrationCount = 0;
static ConnectionRegistration_t s_pConnectionRegistration[ ARRAYSIZE(g_pInterfaceGlobals) + 1 ];

void RegisterInterface( CreateInterfaceFn factory, const char *pInterfaceName, void **ppGlobal )
{
	if ( !(*ppGlobal) )
	{
		*ppGlobal = factory( pInterfaceName, NULL );
		if ( *ppGlobal )
		{
			Assert( s_nRegistrationCount < ARRAYSIZE(s_pConnectionRegistration) );
			ConnectionRegistration_t &reg = s_pConnectionRegistration[s_nRegistrationCount++];
			reg.m_ppGlobalStorage = ppGlobal;
			reg.m_nConnectionPhase = s_nConnectionCount;
		}
	}
}

void ReconnectInterface( CreateInterfaceFn factory, const char *pInterfaceName, void **ppGlobal )
{
	*ppGlobal = factory( pInterfaceName, NULL );

	bool bFound = false;
	Assert( s_nRegistrationCount < ARRAYSIZE(s_pConnectionRegistration) );
	for ( int i = 0; i < s_nRegistrationCount; ++i )
	{
		ConnectionRegistration_t &reg = s_pConnectionRegistration[i];
		if ( reg.m_ppGlobalStorage != ppGlobal )
			continue;

		reg.m_ppGlobalStorage = ppGlobal;
		bFound = true;
	}

	if ( !bFound && *ppGlobal )
	{
		Assert( s_nRegistrationCount < ARRAYSIZE(s_pConnectionRegistration) );
		ConnectionRegistration_t &reg = s_pConnectionRegistration[s_nRegistrationCount++];
		reg.m_ppGlobalStorage = ppGlobal;
		reg.m_nConnectionPhase = s_nConnectionCount;
	}
}


//-----------------------------------------------------------------------------
// Call this to connect to all tier 1 libraries.
// It's up to the caller to check the globals it cares about to see if ones are missing
//-----------------------------------------------------------------------------
void ConnectInterfaces( CreateInterfaceFn *pFactoryList, int nFactoryCount )
{
	if ( s_nRegistrationCount < 0 )
	{
		Error( "APPSYSTEM: In ConnectInterfaces(), s_nRegistrationCount is %d!\n", s_nRegistrationCount );
	}
	else if ( s_nRegistrationCount == 0 )
	{
		for ( int i = 0; i < nFactoryCount; ++i )
		{
			for ( int j = 0; j < ARRAYSIZE( g_pInterfaceGlobals ); ++j )
			{
				RegisterInterface( pFactoryList[i], g_pInterfaceGlobals[j].m_pInterfaceName, (void**)g_pInterfaceGlobals[j].m_ppGlobal );
			}
		}
	}
	else
	{
		// This is no longer questionable: ConnectInterfaces() is expected to be called multiple times for a file that exports multiple interfaces.
		// Warning("APPSYSTEM: ConnectInterfaces() was called twice for the same DLL.\nThis is expected behavior in building reslists, but questionable otherwise.\n");
		for ( int i = 0; i < nFactoryCount; ++i )
		{
			for ( int j = 0; j < ARRAYSIZE( g_pInterfaceGlobals ); ++j )
			{
				ReconnectInterface( pFactoryList[i], g_pInterfaceGlobals[j].m_pInterfaceName, (void**)g_pInterfaceGlobals[j].m_ppGlobal );
			}
		}
	}
	++s_nConnectionCount;
}

void DisconnectInterfaces()
{
	Assert( s_nConnectionCount > 0 );
	if ( --s_nConnectionCount < 0 )
		return;

	for ( int i = 0; i < s_nRegistrationCount; ++i )
	{
		if ( s_pConnectionRegistration[i].m_nConnectionPhase != s_nConnectionCount )
			continue;

		// Disconnect!
		*(void**)(s_pConnectionRegistration[i].m_ppGlobalStorage) = 0;
	}
}


//-----------------------------------------------------------------------------
// Reloads an interface
//-----------------------------------------------------------------------------
void ReconnectInterface( CreateInterfaceFn factory, const char *pInterfaceName )
{
	for ( int i = 0; i < ARRAYSIZE( g_pInterfaceGlobals ); ++i )
	{
		if ( strcmp( g_pInterfaceGlobals[i].m_pInterfaceName, pInterfaceName ) )
			continue;		
		ReconnectInterface( factory, g_pInterfaceGlobals[i].m_pInterfaceName, (void**)g_pInterfaceGlobals[i].m_ppGlobal );
	}
}
