//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include <math.h>
#include "mathlib/bumpvects.h"
#include "colorspace.h"
#include "materialsystem_global.h"
#include "IHardwareConfigInternal.h"
#include "materialsystem/materialsystem_config.h"

// NOTE: This has to be the last file included
#include "tier0/memdbgon.h"

//static float			texLightToLinear[256];	// texlight (0..255) to linear (0..4)
static float			textureToLinear[256];	// texture (0..255) to linear (0..1)
static int				linearToTexture[1024];	// linear (0..1) to texture (0..255)
static int				linearToScreen[1024];	// linear (0..1) to gamma corrected vertex light (0..255)
float					g_LinearToVertex[4096];	// linear (0..4) to screen corrected vertex space (0..1?)
static int				linearToLightmap[4096];	// linear (0..4) to screen corrected texture value (0..255)

void ColorSpace::SetGamma( float screenGamma, float texGamma, 
						   float overbright, bool allowCheats, bool linearFrameBuffer )
{
	int		i, inf;
	float	g1, g3;
	float	g;
	float	brightness = 0.0f; // This used to be configurable. . hardcode to 0.0

	if( linearFrameBuffer )
	{
		screenGamma = 1.0f;
	}

	g = screenGamma;
	
	// clamp values to prevent cheating in multiplayer
	if( !allowCheats )
	{
		if (brightness > 2.0f)
			brightness = 2.0f;

		if (g < 1.8f)
			g = 1.8f;
	}

	if (g > 3.0) 
		g = 3.0;

	g = 1.0f / g;
	g1 = texGamma * g; 

	// pow( textureColor, g1 ) converts from on-disk texture space to framebuffer space
	
	if (brightness <= 0.0f) 
	{
		g3 = 0.125;
	}
	else if (brightness > 1.0f) 
	{
		g3 = 0.05f;
	}
	else 
	{
		g3 = 0.125f - (brightness * brightness) * 0.075f;
	}

	for (i=0 ; i<1024 ; i++)
	{
		float f;

		f = i / 1023.0f;

		// scale up
		if (brightness > 1.0f)
			f = f * brightness;

		// shift up
		if (f <= g3)
			f = (f / g3) * 0.125f;
		else 
			f = 0.125f + ((f - g3) / (1.0f - g3)) * 0.875f;

		// convert linear space to desired gamma space
		inf = ( int )( 255 * pow ( f, g ) );

		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		linearToScreen[i] = inf;
	}

	for (i=0 ; i<256 ; i++)
	{
		// convert from nonlinear texture space (0..255) to linear space (0..1)
		textureToLinear[i] =  ( float )pow( i / 255.0f, texGamma );
	}

	for (i=0 ; i<1024 ; i++)
	{
		// convert from linear space (0..1) to nonlinear texture space (0..255)
		linearToTexture[i] =  ( int )( pow( i / 1023.0f, 1.0f / texGamma ) * 255 );
	}

#if 0
	for (i=0 ; i<256 ; i++)
	{
		float f;

		// convert from nonlinear lightmap space (0..255) to linear space (0..4)
		f =  ( float )( (i / 255.0f) * sqrt( 4 ) );
		f = f * f;

		texLightToLinear[i] = f;
	}
#endif
	
	float f, overbrightFactor;
	
	// Can't do overbright without texcombine
	// UNDONE: Add GAMMA ramp to rectify this

	if ( !HardwareConfig() )
	{
		overbright = 1.0f;
	}
	if ( overbright == 2.0 )
	{
		overbrightFactor = 0.5;
	}
	else if ( overbright == 4.0 )
	{
		overbrightFactor = 0.25;
	}
	else
	{
		overbrightFactor = 1.0;
	}
	
	for (i=0 ; i<4096 ; i++)
	{
		// convert from linear 0..4 (x1024) to screen corrected vertex space (0..1?)
		f = ( float )pow ( i/1024.0f, 1.0f / screenGamma );
		
		g_LinearToVertex[i] = f * overbrightFactor;
		if (g_LinearToVertex[i] > 1)
			g_LinearToVertex[i] = 1;
		
		linearToLightmap[i] = ( int )( f * 255 * overbrightFactor );
		if (linearToLightmap[i] > 255)
			linearToLightmap[i] = 255;
	}
}

// convert texture to linear 0..1 value
float ColorSpace::TextureToLinear( int c )
{
	if (c < 0)
		return 0;
	if (c > 255)
		return 1.0f;

	return textureToLinear[c];
}

// convert texture to linear 0..1 value
int ColorSpace::LinearToTexture( float f )
{
	int i;
	i = ( int )( f * 1023.0f );	// assume 0..1 range
	if (i < 0)
		i = 0;
	if (i > 1023)
		i = 1023;

	return linearToTexture[i];
}

float ColorSpace::TexLightToLinear( int c, int exponent )
{
//	return texLightToLinear[ c ];
	// optimize me
	return ( float )c * ( float )pow( 2.0f, exponent ) * ( 1.0f / 255.0f );
}

// converts 0..1 linear value to screen gamma (0..255)
int ColorSpace::LinearToScreenGamma( float f )
{
	int i;
	i = ( int )( f * 1023.0f );	// assume 0..1 range
	if (i < 0)
		i = 0;
	if (i > 1023)
		i = 1023;

	return linearToScreen[i];
}


uint16 ColorSpace::LinearFloatToCorrectedShort( float in )
{
	uint16 out;
	in = min( in * 4096.0, 65535.0 );
	out = max( in, 0.0f );

	return out;
}


