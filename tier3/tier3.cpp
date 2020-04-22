//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//

#include "tier3/tier3.h"
#include "tier0/dbg.h"
#include "istudiorender.h"
#include "vgui/IVGui.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "video/ivideoservices.h"
#include "movieobjects/idmemakefileutils.h"
#include "vphysics_interface.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "ivtex.h"


//-----------------------------------------------------------------------------
// These tier3 libraries must be set by any users of this library.
// They can be set by calling ConnectTier3Libraries.
// It is hoped that setting this, and using this library will be the common mechanism for
// allowing link libraries to access tier3 library interfaces
//-----------------------------------------------------------------------------
IStudioRender *g_pStudioRender = 0;
IStudioRender *studiorender = 0;
IMatSystemSurface *g_pMatSystemSurface = 0;
vgui::IInput *g_pVGuiInput = 0;
vgui::ISurface *g_pVGuiSurface = 0;
vgui::IPanel *g_pVGuiPanel = 0;
vgui::IVGui	*g_pVGui = 0;
vgui::ILocalize *g_pVGuiLocalize = 0;
vgui::ISchemeManager *g_pVGuiSchemeManager = 0;
vgui::ISystem *g_pVGuiSystem = 0;
IDataCache *g_pDataCache = 0;
IMDLCache *g_pMDLCache = 0;
IMDLCache *mdlcache = 0;
IVideoServices *g_pVideo = NULL;
IDmeMakefileUtils *g_pDmeMakefileUtils = 0;
IPhysicsCollision *g_pPhysicsCollision = 0;
ISoundEmitterSystemBase *g_pSoundEmitterSystem = 0;
IVTex *g_pVTex = 0;


//-----------------------------------------------------------------------------
// Call this to connect to all tier 3 libraries.
// It's up to the caller to check the globals it cares about to see if ones are missing
//-----------------------------------------------------------------------------
void ConnectTier3Libraries( CreateInterfaceFn *pFactoryList, int nFactoryCount )
{
	// Don't connect twice..
	Assert( !g_pStudioRender && !studiorender && !g_pMatSystemSurface && !g_pVGui && !g_pVGuiPanel && !g_pVGuiInput &&
		!g_pVGuiSurface && !g_pDataCache && !g_pMDLCache && !mdlcache && !g_pVideo &&
		!g_pDmeMakefileUtils && !g_pPhysicsCollision && !g_pVGuiLocalize && !g_pSoundEmitterSystem &&
		!g_pVGuiSchemeManager && !g_pVGuiSystem );

	for ( int i = 0; i < nFactoryCount; ++i )
	{
		if ( !g_pStudioRender )
		{
			g_pStudioRender = studiorender = ( IStudioRender * )pFactoryList[i]( STUDIO_RENDER_INTERFACE_VERSION, NULL );
		}
		if ( !g_pVGui )
		{
			g_pVGui = (vgui::IVGui*)pFactoryList[i]( VGUI_IVGUI_INTERFACE_VERSION, NULL );
		}
		if ( !g_pVGuiInput )
		{
			g_pVGuiInput = (vgui::IInput*)pFactoryList[i]( VGUI_INPUT_INTERFACE_VERSION, NULL );
		}
		if ( !g_pVGuiPanel )
		{
			g_pVGuiPanel = (vgui::IPanel*)pFactoryList[i]( VGUI_PANEL_INTERFACE_VERSION, NULL );
		}
		if ( !g_pVGuiSurface )
		{
			g_pVGuiSurface = (vgui::ISurface*)pFactoryList[i]( VGUI_SURFACE_INTERFACE_VERSION, NULL );
		}
		if ( !g_pVGuiSchemeManager )
		{
			g_pVGuiSchemeManager = (vgui::ISchemeManager*)pFactoryList[i]( VGUI_SCHEME_INTERFACE_VERSION, NULL );
		}
		if ( !g_pVGuiSystem )
		{
			g_pVGuiSystem = (vgui::ISystem*)pFactoryList[i]( VGUI_SYSTEM_INTERFACE_VERSION, NULL );
		}
		if ( !g_pVGuiLocalize )
		{
			g_pVGuiLocalize = (vgui::ILocalize*)pFactoryList[i]( VGUI_LOCALIZE_INTERFACE_VERSION, NULL );
		}
		if ( !g_pMatSystemSurface )
		{
			g_pMatSystemSurface = ( IMatSystemSurface * )pFactoryList[i]( MAT_SYSTEM_SURFACE_INTERFACE_VERSION, NULL );
		}
		if ( !g_pDataCache )
		{
			g_pDataCache = (IDataCache*)pFactoryList[i]( DATACACHE_INTERFACE_VERSION, NULL );
		}
		if ( !g_pMDLCache )
		{
			g_pMDLCache = mdlcache = (IMDLCache*)pFactoryList[i]( MDLCACHE_INTERFACE_VERSION, NULL );
		}
		if ( !g_pVideo )
		{
			g_pVideo = (IVideoServices *)pFactoryList[i](VIDEO_SERVICES_INTERFACE_VERSION, NULL);
		}
		if ( !g_pDmeMakefileUtils )
		{
			g_pDmeMakefileUtils = (IDmeMakefileUtils*)pFactoryList[i]( DMEMAKEFILE_UTILS_INTERFACE_VERSION, NULL );
		}
		if ( !g_pPhysicsCollision )
		{
			g_pPhysicsCollision = ( IPhysicsCollision* )pFactoryList[i]( VPHYSICS_COLLISION_INTERFACE_VERSION, NULL );
		}
		if ( !g_pSoundEmitterSystem )
		{
			g_pSoundEmitterSystem = ( ISoundEmitterSystemBase* )pFactoryList[i]( SOUNDEMITTERSYSTEM_INTERFACE_VERSION, NULL );
		}
		if ( !g_pVTex )
		{
			g_pVTex = ( IVTex * )pFactoryList[i]( IVTEX_VERSION_STRING, NULL );
		}
	}
}

void DisconnectTier3Libraries()
{
	g_pStudioRender = 0;
	studiorender = 0;
	g_pVGui = 0;
	g_pVGuiInput = 0;
	g_pVGuiPanel = 0;
	g_pVGuiSurface = 0;
	g_pVGuiLocalize = 0;
	g_pVGuiSchemeManager = 0;
	g_pVGuiSystem = 0;
	g_pMatSystemSurface = 0;
	g_pDataCache = 0;
	g_pMDLCache = 0;
	mdlcache = 0;
	g_pVideo = NULL;
	g_pPhysicsCollision = 0;
	g_pDmeMakefileUtils = NULL;
	g_pSoundEmitterSystem = 0;
	g_pVTex = NULL;
}
