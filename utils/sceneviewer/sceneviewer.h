//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H
#ifdef _WIN32
#pragma once
#endif


namespace vgui
{
	class ISurface;
	class IVGui;
	class IPanel;
}

class IMatSystemSurface;
class IStudioRender;
struct MaterialSystem_Config_t;


extern IMatSystemSurface *g_pMatSystemSurface;
extern IStudioRender *g_pStudioRender;
extern const MaterialSystem_Config_t *g_pMaterialSystemConfig;
extern vgui::ISurface *g_pVGuiSurface;
extern vgui::IVGui	*g_pVGui;
extern vgui::IPanel	*g_pVGuiPanel;


// temporary HACK
class _Window_t;
extern _Window_t *g_pWindow;

#endif // SCENEVIEWER_H
