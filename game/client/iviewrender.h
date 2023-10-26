//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#if !defined( IVIEWRENDER_H )
#define IVIEWRENDER_H
#ifdef _WIN32
#pragma once
#endif


#include "ivrenderview.h"


#define MAX_DEPTH_TEXTURE_SHADOWS 1
#define MAX_DEPTH_TEXTURE_HIGHRES_SHADOWS 0

#define MAX_DEPTH_TEXTURE_SHADOWS_TOOLS 8
#define MAX_DEPTH_TEXTURE_HIGHRES_SHADOWS_TOOLS 0


// These are set as it draws reflections, refractions, etc, so certain effects can avoid 
// drawing themselves in reflections.
enum DrawFlags_t
{
	DF_RENDER_REFRACTION	= 0x1,
	DF_RENDER_REFLECTION	= 0x2,

	DF_CLIP_Z				= 0x4,
	DF_CLIP_BELOW			= 0x8,

	DF_RENDER_UNDERWATER	= 0x10,
	DF_RENDER_ABOVEWATER	= 0x20,
	DF_RENDER_WATER			= 0x40,

	DF_UNUSED1				= 0x100,
	DF_WATERHEIGHT			= 0x200,
	DF_UNUSED2				= 0x400,
	DF_DRAWSKYBOX			= 0x800,

	DF_FUDGE_UP				= 0x1000,

	DF_DRAW_ENTITITES		= 0x2000,

	DF_SKIP_WORLD			= 0x4000,
	DF_SKIP_WORLD_DECALS_AND_OVERLAYS	= 0x8000,

	DF_UNUSED5				= 0x10000,
	DF_SAVEGAMESCREENSHOT	= 0x20000,
	DF_CLIP_SKYBOX			= 0x40000,

	DF_SHADOW_DEPTH_MAP		= 0x100000	// Currently rendering a shadow depth map
};


//-----------------------------------------------------------------------------
// Purpose: View setup and rendering
//-----------------------------------------------------------------------------
class CViewSetup;
class C_BaseEntity;
struct vrect_t;
class C_BaseViewModel;

abstract_class IViewRender
{
public:
	// SETUP
	// Initialize view renderer
	virtual void		Init( void ) = 0;

	// Clear any systems between levels
	virtual void		LevelInit( void ) = 0;
	virtual void		LevelShutdown( void ) = 0;

	// Shutdown
	virtual void		Shutdown( void ) = 0;

	// RENDERING
	// Called right before simulation. It must setup the view model origins and angles here so 
	// the correct attachment points can be used during simulation.	
	virtual void		OnRenderStart() = 0;

	// Called to render the entire scene
	virtual	void		Render( vrect_t *rect ) = 0;

	// Called to render just a particular setup ( for timerefresh and envmap creation )
	// First argument is 3d view setup, second is for the HUD (in most cases these are ==, but in split screen the client .dll handles this differently)
	virtual void		RenderView( const CViewSetup &view, const CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw ) = 0;

	// What are we currently rendering? Returns a combination of DF_ flags.
	virtual int GetDrawFlags() = 0;

	// MISC
	// Start and stop pitch drifting logic
	virtual void		StartPitchDrift( void ) = 0;
	virtual void		StopPitchDrift( void ) = 0;

	// This can only be called during rendering (while within RenderView).
	virtual VPlane*		GetFrustum() = 0;

	virtual bool		ShouldDrawBrushModels( void ) = 0;

	virtual const CViewSetup *GetPlayerViewSetup( int nSlot = -1 ) const = 0;
	virtual const CViewSetup *GetViewSetup( void ) const = 0;

	virtual void		DisableVis( void ) = 0;

	virtual int			BuildWorldListsNumber() const = 0;

	virtual void		SetCheapWaterStartDistance( float flCheapWaterStartDistance ) = 0;
	virtual void		SetCheapWaterEndDistance( float flCheapWaterEndDistance ) = 0;

	virtual void		GetWaterLODParams( float &flCheapWaterStartDistance, float &flCheapWaterEndDistance ) = 0;

	virtual void		DriftPitch (void) = 0;

	virtual void		SetScreenOverlayMaterial( IMaterial *pMaterial ) = 0;
	virtual IMaterial	*GetScreenOverlayMaterial( ) = 0;

	virtual void		WriteSaveGameScreenshot( const char *pFilename ) = 0;
	virtual void		WriteSaveGameScreenshotOfSize( const char *pFilename, int width, int height ) = 0;

	// Draws another rendering over the top of the screen
	virtual void		QueueOverlayRenderView( const CViewSetup &view, int nClearFlags, int whatToDraw ) = 0;

	// Returns znear and zfar
	virtual float		GetZNear() = 0;
	virtual float		GetZFar() = 0;

	virtual void		GetScreenFadeDistances( float *min, float *max ) = 0;
	virtual bool		AllowScreenspaceFade( void ) = 0;

	virtual C_BaseEntity *GetCurrentlyDrawingEntity() = 0;
	virtual void		SetCurrentlyDrawingEntity( C_BaseEntity *pEnt ) = 0;

	virtual bool		UpdateShadowDepthTexture( ITexture *pRenderTarget, ITexture *pDepthTexture, const CViewSetup &shadowView ) = 0;

	virtual void		FreezeFrame( float flFreezeTime ) = 0;

	virtual void		InitFadeData( void ) = 0;
};

extern IViewRender *view;

extern IViewRender *GetViewRenderInstance();

#endif // IVIEWRENDER_H
