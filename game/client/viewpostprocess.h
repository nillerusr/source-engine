//========== Copyright © 2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifndef VIEWPOSTPROCESS_H
#define VIEWPOSTPROCESS_H

#if defined( _WIN32 )
#pragma once
#endif

#include "postprocess_shared.h"

struct RenderableInstance_t;

void DoEnginePostProcessing( int x, int y, int w, int h, bool bFlashlightIsOn, bool bPostVGui = false );
void DoImageSpaceMotionBlur( const CViewSetup &view );
bool IsDepthOfFieldEnabled();
void DoDepthOfField( const CViewSetup &view );
void BlurEntity( IClientRenderable *pRenderable, bool bPreDraw, int drawFlags, const RenderableInstance_t &instance, const CViewSetup &view, int x, int y, int w, int h );

void UpdateMaterialSystemTonemapScalar();

float GetCurrentTonemapScale();

void SetOverrideTonemapScale( bool bEnableOverride, float flTonemapScale );

void SetOverridePostProcessingDisable( bool bForceOff );

void DoBlurFade( float flStrength, float flDesaturate, int x, int y, int w, int h );

void SetPostProcessParams( const PostProcessParameters_t *pPostProcessParameters );

void SetViewFadeParams( byte r, byte g, byte b, byte a, bool bModulate );

#endif // VIEWPOSTPROCESS_H
