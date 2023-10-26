//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_PIXEL_VISIBILITY_H
#define C_PIXEL_VISIBILITY_H
#ifdef _WIN32
#pragma once
#endif


const float PIXELVIS_DEFAULT_PROXY_SIZE = 2.0f;
const float PIXELVIS_DEFAULT_FADE_TIME = 0.0625f;

typedef int pixelvis_handle_t;
struct pixelvis_queryparams_t
{
	pixelvis_queryparams_t()
	{
		bSetup = false;
	}

	void Init( const Vector &origin, float proxySizeIn = PIXELVIS_DEFAULT_PROXY_SIZE, float proxyAspectIn = 1.0f, float fadeTimeIn = PIXELVIS_DEFAULT_FADE_TIME )
	{
		position = origin;
		proxySize = proxySizeIn;
		proxyAspect = proxyAspectIn;
		fadeTime = fadeTimeIn;
		bSetup = true;
		bSizeInScreenspace = false;
	}

	Vector		position;
	float		proxySize;
	float		proxyAspect;
	float		fadeTime;
	bool		bSetup;
	bool		bSizeInScreenspace;
};

float PixelVisibility_FractionVisible( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle );
float StandardGlowBlend( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle, int rendermode, int renderfx, int alpha, float *pscale );

void PixelVisibility_ShiftVisibilityViews( int nPlayerSlot, int iSourceViewID, int iDestViewID ); //mainly needed by portal mod to avoid a pop in visibility when teleporting the player

void PixelVisibility_EndCurrentView();
void PixelVisibility_EndScene();
float GlowSightDistance( const Vector &glowOrigin, bool bShouldTrace );

// returns true if the video hardware is doing the tests, false is traceline is doing so.
bool PixelVisibility_IsAvailable();

class COcclusionQuerySet //A set of occlusion query handles for the same object in different rendering views. Self-registers and shifts appropriately with portals. 
{
public:
	COcclusionQuerySet( void );
	~COcclusionQuerySet( void );
	
	//wrap your draws with these 2
	void BeginQueryDrawing( int iViewID, int iSplitScreenSlot );
	void EndQueryDrawing( int iViewID, int iSplitScreenSlot );

	//and here's your data, expect a frame of delay for performance reasons
	int QueryNumPixelsRendered( int iViewID, int iSplitScreenSlot );	
	float QueryPercentageOfScreenRendered( int iViewID, int iSplitScreenSlot ); // (Pixels rendered) / (total screen pixels)
	int QueryNumPixelsRenderedForAllViewsLastFrame( int iSplitScreenSlot );

	int GetLastFrameDrawn( int iViewID, int iSplitScreenSlot );

	//these implementations call the above with current view id and active splitscreen slot
	void BeginQueryDrawing( void );
	void EndQueryDrawing( void );
	int QueryNumPixelsRendered( void );
	float QueryPercentageOfScreenRendered( void );
	int QueryNumPixelsRenderedForAllViewsLastFrame( void );
	int GetLastFrameDrawn( void );

private:
	void *m_pManagedData; //no need to worry what's under the hood here
};

#endif // C_PIXEL_VISIBILITY_H
