//===== Copyright © 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Revision: $
// $NoKeywords: $
//
// Fast path model rendering
//
//===========================================================================//

#ifndef MODELRENDERSYSTEM_H
#define MODELRENDERSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "clientleafsystem.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct ModelRenderSystemData_t
{
	IClientRenderable *m_pRenderable;
	IClientModelRenderable *m_pModelRenderable;
	RenderableInstance_t m_InstanceData;
};
	
struct TranslucentInstanceRenderData_t
{
	StudioModelArrayInfo_t *m_pModelInfo;
	StudioArrayInstanceData_t *m_pInstanceData;
};

struct TranslucentTempData_t
{
	int m_nColorMeshHandleCount;
	DataCacheHandle_t *m_pColorMeshHandles;
	bool m_bReleaseRenderData;
};

enum ModelRenderMode_t
{
	MODEL_RENDER_MODE_NORMAL,
	MODEL_RENDER_MODE_SHADOW_DEPTH,
	MODEL_RENDER_MODE_RTT_SHADOWS
};

//-----------------------------------------------------------------------------
// The client leaf system
//-----------------------------------------------------------------------------
abstract_class IModelRenderSystem
{
public:
	virtual void DrawModels( ModelRenderSystemData_t *pModels, int nCount, ModelRenderMode_t renderMode ) = 0;

	virtual void ComputeTranslucentRenderData( ModelRenderSystemData_t *pModels, int nCount, TranslucentInstanceRenderData_t *pRenderData, TranslucentTempData_t *pTempData ) = 0;
	virtual void CleanupTranslucentTempData( TranslucentTempData_t *pTempData ) = 0;

	virtual IMaterial *GetFastPathColorMaterial() = 0;
};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
extern IModelRenderSystem *g_pModelRenderSystem;


#endif	// MODELRENDERSYSTEM_H


