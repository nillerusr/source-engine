//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Font effects that operate on linear rgba data
//
//=====================================================================================//

#ifndef _FONTEFFECTS_H
#define _FONTEFFECTS_H

#ifdef _WIN32
#pragma once
#endif

void ApplyScanlineEffectToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, int iScanLines );
void ApplyGaussianBlurToTexture(int rgbaWide, int rgbaTall, unsigned char *rgba, int iBlur );
void ApplyDropShadowToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, int iDropShadowOffset );
void ApplyOutlineToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, int iOutlineSize );
void ApplyRotaryEffectToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, bool bRotary );

#endif