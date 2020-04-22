//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef RENDER_H
#define RENDER_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"

// render.h -- public interface to refresh functions

extern float r_blend; // Global blending factor for the current entity
extern float r_colormod[3]; // Global color modulation for the current entity
extern bool g_bIsBlendingOrModulating;


//-----------------------------------------------------------------------------
// Current view
//-----------------------------------------------------------------------------
inline const Vector &CurrentViewOrigin()
{
#ifdef DBGFLAG_ASSERT
	extern bool g_bCanAccessCurrentView;
#endif
	extern Vector g_CurrentViewOrigin;
	Assert( g_bCanAccessCurrentView );
	return g_CurrentViewOrigin;
}

inline const Vector &CurrentViewForward()
{
#ifdef DBGFLAG_ASSERT
	extern bool g_bCanAccessCurrentView;
#endif
	extern Vector g_CurrentViewForward;
	Assert( g_bCanAccessCurrentView );
	return g_CurrentViewForward;
}

inline const Vector &CurrentViewRight()
{
#ifdef DBGFLAG_ASSERT
	extern bool g_bCanAccessCurrentView;
#endif
	extern Vector g_CurrentViewRight;
	Assert( g_bCanAccessCurrentView );
	return g_CurrentViewRight;
}

inline const Vector &CurrentViewUp()
{
#ifdef DBGFLAG_ASSERT
	extern bool g_bCanAccessCurrentView;
#endif
	extern Vector g_CurrentViewUp;
	Assert( g_bCanAccessCurrentView );
	return g_CurrentViewUp;
}


//-----------------------------------------------------------------------------
// Main view
//-----------------------------------------------------------------------------
inline const Vector &MainViewOrigin()
{
	extern Vector g_MainViewOrigin;
	return g_MainViewOrigin;
}

inline const Vector &MainViewForward()
{
	extern Vector g_MainViewForward;
	return g_MainViewForward;
}

inline const Vector &MainViewRight()
{
	extern Vector g_MainViewRight;
	return g_MainViewRight;
}

inline const Vector &MainViewUp()
{
	extern Vector g_MainViewUp;
	return g_MainViewUp;
}


void R_Init (void);
void R_LevelInit(void);
void R_LevelShutdown(void);

// Loads world geometry. Called when map changes or dx level changes
void R_LoadWorldGeometry( bool bDXChange = false );

#include "view_shared.h"
#include "ivrenderview.h"

class VMatrix;

abstract_class IRender
{
public:
	virtual void	FrameBegin( void ) = 0;
	virtual void	FrameEnd( void ) = 0;
	
	virtual void	ViewSetupVis( bool novis, int numorigins, const Vector origin[] ) = 0;

	virtual void	ViewDrawFade( byte *color, IMaterial* pFadeMaterial ) = 0;

	virtual void	DrawSceneBegin( void ) = 0;
	virtual void	DrawSceneEnd( void ) = 0;

	virtual IWorldRenderList * CreateWorldList() = 0;
	virtual void	BuildWorldLists( IWorldRenderList *pList, WorldListInfo_t* pInfo, int iForceViewLeaf, const VisOverrideData_t* pVisData, bool bShadowDepth, float *pReflectionWaterHeight ) = 0;
	virtual void	DrawWorldLists( IWorldRenderList *pList, unsigned long flags, float waterZAdjust ) = 0;

	// UNDONE: these are temporary functions that will end up on the other
	// side of this interface
	// accessors
//	virtual const Vector& UnreflectedViewOrigin() = 0;
	virtual const Vector& ViewOrigin( void ) = 0;
	virtual const QAngle& ViewAngles( void ) = 0;
	virtual CViewSetup const &ViewGetCurrent( void ) = 0;
	virtual const VMatrix& ViewMatrix( void ) = 0;
	virtual const VMatrix& WorldToScreenMatrix( void ) = 0;
	virtual float	GetFramerate( void ) = 0;
	virtual float	GetZNear( void ) = 0;
	virtual float	GetZFar( void ) = 0;

	// Query current fov and view model fov
	virtual float	GetFov( void ) = 0;
	virtual float	GetFovY( void ) = 0;
	virtual float	GetFovViewmodel( void ) = 0;


	// Compute the clip-space coordinates of a point in 3D
	// Clip-space is normalized screen coordinates (-1 to 1 in x and y)
	// Returns true if the point is behind the camera
	virtual bool	ClipTransformWithProjection ( const VMatrix& worldToScreen, const Vector& point, Vector* pClip ) = 0;
	// Same, using the current engine's matrices.
	virtual bool	ClipTransform( Vector const& point, Vector* pClip ) = 0;

	// Compute the screen-space coordinates of a point in 3D
	// This returns actual pixels
	// Returns true if the point is behind the camera
	virtual bool ScreenTransform( Vector const& point, Vector* pScreen ) = 0;

	virtual void Push3DView( const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes ) = 0;
	virtual void Push3DView( const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes, ITexture* pDepthTexture ) = 0;
	virtual void Push2DView( const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes ) = 0;
	virtual void PopView( Frustum frustumPlanes ) = 0;
	virtual void SetMainView( const Vector &vecOrigin, const QAngle &angles ) = 0;
	virtual void ViewSetupVisEx( bool novis, int numorigins, const Vector origin[], unsigned int &returnFlags ) = 0;
	virtual void OverrideViewFrustum( Frustum custom ) = 0;
	virtual void UpdateBrushModelLightmap( model_t *model, IClientRenderable *Renderable ) = 0;
	virtual void BeginUpdateLightmaps( void ) = 0;
	virtual void EndUpdateLightmaps( void ) = 0;
	virtual bool InLightmapUpdate( void ) const = 0;
};

void R_PushDlights (void);

// UNDONE Remove this - pass this around to functions/systems that need it.
extern IRender *g_EngineRenderer;
#endif			// RENDER_H
