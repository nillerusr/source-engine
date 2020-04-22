//===== Copyright © 2005-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//


#ifndef INTERFACES_H
#define INTERFACES_H

#if defined( COMPILER_MSVC )
#pragma once
#endif


//-----------------------------------------------------------------------------
// Interface creation function
//-----------------------------------------------------------------------------
typedef void* (*CreateInterfaceFn)(const char *pName, int *pReturnCode);


//-----------------------------------------------------------------------------
// Macros to declare interfaces appropriate for various tiers
//-----------------------------------------------------------------------------
#if 1 || defined( TIER1_LIBRARY ) || defined( TIER2_LIBRARY ) || defined( TIER3_LIBRARY ) || defined( TIER4_LIBRARY ) || defined( APPLICATION )
#define DECLARE_TIER1_INTERFACE( _Interface, _Global )	extern _Interface * _Global;
#else
#define DECLARE_TIER1_INTERFACE( _Interface, _Global )
#endif

#if 1 || defined( TIER2_LIBRARY ) || defined( TIER3_LIBRARY ) || defined( TIER4_LIBRARY ) || defined( APPLICATION )
#define DECLARE_TIER2_INTERFACE( _Interface, _Global )	extern _Interface * _Global;
#else
#define DECLARE_TIER2_INTERFACE( _Interface, _Global )
#endif

#if 1 || defined( TIER3_LIBRARY ) || defined( TIER4_LIBRARY ) || defined( APPLICATION )
#define DECLARE_TIER3_INTERFACE( _Interface, _Global )	extern _Interface * _Global;
#else
#define DECLARE_TIER3_INTERFACE( _Interface, _Global )
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ICvar;
class IProcessUtils;
class ILocalize;
class IPhysics2;
class IPhysics2ActorManager;
class IPhysics2ResourceManager;
class IEventSystem;

class IAsyncFileSystem;
class IColorCorrectionSystem;
class IDebugTextureInfo;
class IFileSystem;
class IRenderHardwareConfig;
class IInputSystem;
class IInputStackSystem;
class IMaterialSystem;
class IMaterialSystem2;
class IMaterialSystemHardwareConfig;
class IMdlLib;
class INetworkSystem;
class IP4;
class IQueuedLoader;
class IResourceAccessControl;
class IPrecacheSystem;
class IRenderDevice;
class IRenderDeviceMgr;
class IResourceSystem;
class IVBAllocTracker;
class IXboxInstaller;
class IMatchFramework;
class ISoundSystem;
class IStudioRender;
class IMatSystemSurface;
class IGameUISystemMgr;
class IDataCache;
class IMDLCache;
class IAvi;
class IBik;
class IDmeMakefileUtils;
class IPhysicsCollision;
class ISoundEmitterSystemBase;
class IMeshSystem;
class IWorldRendererMgr;
class ISceneSystem;
class IVGuiRenderSurface;

namespace vgui
{
	class ISurface;
	class IVGui;
	class IInput;
	class IPanel;
	class IVGUILocalize;
	class ISchemeManager;
	class ISystem;
}



//-----------------------------------------------------------------------------
// Fills out global DLL exported interface pointers
//-----------------------------------------------------------------------------
#define CVAR_INTERFACE_VERSION					"VEngineCvar007"
DECLARE_TIER1_INTERFACE( ICvar, cvar );
DECLARE_TIER1_INTERFACE( ICvar, g_pCVar )

#define PROCESS_UTILS_INTERFACE_VERSION			"VProcessUtils002"
DECLARE_TIER1_INTERFACE( IProcessUtils, g_pProcessUtils );

#define VPHYSICS2_INTERFACE_VERSION				"Physics2 Interface v0.3"
DECLARE_TIER1_INTERFACE( IPhysics2, g_pPhysics2 );

#define VPHYSICS2_ACTOR_MGR_INTERFACE_VERSION	"Physics2 Interface ActorMgr v0.1"
DECLARE_TIER1_INTERFACE( IPhysics2ActorManager, g_pPhysics2ActorManager );

#define VPHYSICS2_RESOURCE_MGR_INTERFACE_VERSION "Physics2 Interface ResourceMgr v0.1"
DECLARE_TIER1_INTERFACE( IPhysics2ResourceManager, g_pPhysics2ResourceManager );

#define EVENTSYSTEM_INTERFACE_VERSION "EventSystem001"
DECLARE_TIER1_INTERFACE( IEventSystem, g_pEventSystem );

#define LOCALIZE_INTERFACE_VERSION 			"Localize_001"
DECLARE_TIER2_INTERFACE( ILocalize, g_pLocalize );
DECLARE_TIER3_INTERFACE( vgui::IVGUILocalize, g_pVGuiLocalize );

#define RENDER_DEVICE_MGR_INTERFACE_VERSION		"RenderDeviceMgr001"
DECLARE_TIER2_INTERFACE( IRenderDeviceMgr, g_pRenderDeviceMgr );

#define FILESYSTEM_INTERFACE_VERSION			"VFileSystem017"
DECLARE_TIER2_INTERFACE( IFileSystem, g_pFullFileSystem );

#define ASYNCFILESYSTEM_INTERFACE_VERSION		"VNewAsyncFileSystem001"
DECLARE_TIER2_INTERFACE( IAsyncFileSystem, g_pAsyncFileSystem );

#define RESOURCESYSTEM_INTERFACE_VERSION		"ResourceSystem004"
DECLARE_TIER2_INTERFACE( IResourceSystem, g_pResourceSystem );

#define MATERIAL_SYSTEM_INTERFACE_VERSION		"VMaterialSystem080"
DECLARE_TIER2_INTERFACE( IMaterialSystem, materials );
DECLARE_TIER2_INTERFACE( IMaterialSystem, g_pMaterialSystem );

#define MATERIAL_SYSTEM2_INTERFACE_VERSION		"VMaterialSystem2_001"
DECLARE_TIER2_INTERFACE( IMaterialSystem2, g_pMaterialSystem2 );

#define INPUTSYSTEM_INTERFACE_VERSION			"InputSystemVersion001"
DECLARE_TIER2_INTERFACE( IInputSystem, g_pInputSystem );

#define INPUTSTACKSYSTEM_INTERFACE_VERSION		"InputStackSystemVersion001"
DECLARE_TIER2_INTERFACE( IInputStackSystem, g_pInputStackSystem );

#define NETWORKSYSTEM_INTERFACE_VERSION			"NetworkSystemVersion001"
DECLARE_TIER2_INTERFACE( INetworkSystem, g_pNetworkSystem );

#define MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION		"MaterialSystemHardwareConfig013"
DECLARE_TIER2_INTERFACE( IMaterialSystemHardwareConfig, g_pMaterialSystemHardwareConfig );

#define DEBUG_TEXTURE_INFO_VERSION				"DebugTextureInfo001"
DECLARE_TIER2_INTERFACE( IDebugTextureInfo, g_pMaterialSystemDebugTextureInfo );

#define VB_ALLOC_TRACKER_INTERFACE_VERSION		"VBAllocTracker001"
DECLARE_TIER2_INTERFACE( IVBAllocTracker, g_VBAllocTracker );

#define COLORCORRECTION_INTERFACE_VERSION		"COLORCORRECTION_VERSION_1"
DECLARE_TIER2_INTERFACE( IColorCorrectionSystem, colorcorrection );

#define P4_INTERFACE_VERSION					"VP4002"
DECLARE_TIER2_INTERFACE( IP4, p4 );

#define MDLLIB_INTERFACE_VERSION				"VMDLLIB001"
DECLARE_TIER2_INTERFACE( IMdlLib, mdllib );

#define QUEUEDLOADER_INTERFACE_VERSION			"QueuedLoaderVersion001"
DECLARE_TIER2_INTERFACE( IQueuedLoader, g_pQueuedLoader );

#define RESOURCE_ACCESS_CONTROL_INTERFACE_VERSION	"VResourceAccessControl001"
DECLARE_TIER2_INTERFACE( IResourceAccessControl, g_pResourceAccessControl );

#define PRECACHE_SYSTEM_INTERFACE_VERSION		"VPrecacheSystem001"
DECLARE_TIER2_INTERFACE( IPrecacheSystem, g_pPrecacheSystem );

#if defined( _X360 )
#define XBOXINSTALLER_INTERFACE_VERSION			"XboxInstallerVersion001"
DECLARE_TIER2_INTERFACE( IXboxInstaller, g_pXboxInstaller );
#endif

#define MATCHFRAMEWORK_INTERFACE_VERSION		"MATCHFRAMEWORK_001"
DECLARE_TIER2_INTERFACE( IMatchFramework, g_pMatchFramework );

#define GAMEUISYSTEMMGR_INTERFACE_VERSION	"GameUISystemMgr001"
DECLARE_TIER3_INTERFACE( IGameUISystemMgr, g_pGameUISystemMgr );


//-----------------------------------------------------------------------------
// Not exactly a global, but we're going to keep track of these here anyways
// NOTE: Appframework deals with connecting these bad boys. See materialsystem2app.cpp
//-----------------------------------------------------------------------------
#define RENDER_DEVICE_INTERFACE_VERSION			"RenderDevice001"
DECLARE_TIER2_INTERFACE( IRenderDevice, g_pRenderDevice );

#define RENDER_HARDWARECONFIG_INTERFACE_VERSION		"RenderHardwareConfig001"
DECLARE_TIER2_INTERFACE( IRenderHardwareConfig, g_pRenderHardwareConfig );

#define SOUNDSYSTEM_INTERFACE_VERSION		"SoundSystem001"
DECLARE_TIER2_INTERFACE( ISoundSystem, g_pSoundSystem );

#define MESHSYSTEM_INTERFACE_VERSION			"MeshSystem001"
DECLARE_TIER3_INTERFACE( IMeshSystem, g_pMeshSystem );

#define STUDIO_RENDER_INTERFACE_VERSION			"VStudioRender026"
DECLARE_TIER3_INTERFACE( IStudioRender, g_pStudioRender );
DECLARE_TIER3_INTERFACE( IStudioRender, studiorender );

#define MAT_SYSTEM_SURFACE_INTERFACE_VERSION	"MatSystemSurface006"
DECLARE_TIER3_INTERFACE( IMatSystemSurface, g_pMatSystemSurface );

#define RENDER_SYSTEM_SURFACE_INTERFACE_VERSION	"RenderSystemSurface001"
DECLARE_TIER3_INTERFACE( IVGuiRenderSurface, g_pVGuiRenderSurface );

#define SCENESYSTEM_INTERFACE_VERSION			"SceneSystem_001"
DECLARE_TIER3_INTERFACE( ISceneSystem, g_pSceneSystem );

#define VGUI_SURFACE_INTERFACE_VERSION			"VGUI_Surface031"
DECLARE_TIER3_INTERFACE( vgui::ISurface, g_pVGuiSurface );

#define SCHEME_SURFACE_INTERFACE_VERSION		"SchemeSurface001"

#define VGUI_INPUT_INTERFACE_VERSION			"VGUI_Input005"
DECLARE_TIER3_INTERFACE( vgui::IInput, g_pVGuiInput );

#define VGUI_IVGUI_INTERFACE_VERSION			"VGUI_ivgui008"
DECLARE_TIER3_INTERFACE( vgui::IVGui, g_pVGui );

#define VGUI_PANEL_INTERFACE_VERSION			"VGUI_Panel009"
DECLARE_TIER3_INTERFACE( vgui::IPanel, g_pVGuiPanel );

#define VGUI_SCHEME_INTERFACE_VERSION			"VGUI_Scheme010"
DECLARE_TIER3_INTERFACE( vgui::ISchemeManager, g_pVGuiSchemeManager );

#define VGUI_SYSTEM_INTERFACE_VERSION			"VGUI_System010"
DECLARE_TIER3_INTERFACE( vgui::ISystem, g_pVGuiSystem );

#define DATACACHE_INTERFACE_VERSION				"VDataCache003"
DECLARE_TIER3_INTERFACE( IDataCache, g_pDataCache );	// FIXME: Should IDataCache be in tier2?

#define MDLCACHE_INTERFACE_VERSION				"MDLCache004"
DECLARE_TIER3_INTERFACE( IMDLCache, g_pMDLCache );
DECLARE_TIER3_INTERFACE( IMDLCache, mdlcache );

#define AVI_INTERFACE_VERSION					"VAvi001"
DECLARE_TIER3_INTERFACE( IAvi, g_pAVI );

#define BIK_INTERFACE_VERSION					"VBik001"
DECLARE_TIER3_INTERFACE( IBik, g_pBIK );

#define DMEMAKEFILE_UTILS_INTERFACE_VERSION		"VDmeMakeFileUtils001"
DECLARE_TIER3_INTERFACE( IDmeMakefileUtils, g_pDmeMakefileUtils );

#define VPHYSICS_COLLISION_INTERFACE_VERSION	"VPhysicsCollision007"
DECLARE_TIER3_INTERFACE( IPhysicsCollision, g_pPhysicsCollision );

#define SOUNDEMITTERSYSTEM_INTERFACE_VERSION	"VSoundEmitter003"
DECLARE_TIER3_INTERFACE( ISoundEmitterSystemBase, g_pSoundEmitterSystem );

#define WORLD_RENDERER_MGR_INTERFACE_VERSION	"WorldRendererMgr001"
DECLARE_TIER3_INTERFACE( IWorldRendererMgr, g_pWorldRendererMgr );

//-----------------------------------------------------------------------------
// Fills out global DLL exported interface pointers
//-----------------------------------------------------------------------------
void ConnectInterfaces( CreateInterfaceFn *pFactoryList, int nFactoryCount );
void DisconnectInterfaces();


//-----------------------------------------------------------------------------
// Reconnects an interface
//-----------------------------------------------------------------------------
void ReconnectInterface( CreateInterfaceFn factory, const char *pInterfaceName );


#endif // INTERFACES_H

