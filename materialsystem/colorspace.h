//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef COLORSPACE_H
#define COLORSPACE_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/mathlib.h"
#include "mathlib/ssemath.h"
#include "mathlib/bumpvects.h"
#include "tier0/dbg.h"

extern float g_LinearToVertex[4096];	// linear (0..4) to screen corrected vertex space (0..1?)

// FIXME!!!  Get rid of this. . all of this should be in mathlib
namespace ColorSpace
{
	void SetGamma( float screenGamma, float texGamma, 
		           float overbright, bool allowCheats, bool linearFrameBuffer );

	// convert texture to linear 0..1 value
	float TextureToLinear( int c );

	// convert texture to linear 0..1 value
	int LinearToTexture( float f );

	float TexLightToLinear( int c, int exponent );

	// assume 0..4 range
	void LinearToLightmap( unsigned char *pDstRGB, const float *pSrcRGB );
	
	// assume 0..4 range
	void LinearToBumpedLightmap( const float *linearColor, const float *linearBumpColor1,
		const float *linearBumpColor2, const float *linearBumpColor3,
		unsigned char *ret, unsigned char *retBump1,
		unsigned char *retBump2, unsigned char *retBump3 );
	
	// converts 0..1 linear value to screen gamma (0..255)
	int LinearToScreenGamma( float f );

	FORCEINLINE void LinearToLightmap( unsigned char *pDstRGB, const float *pSrcRGB )
	{
		Vector tmpVect;
#if 1
		int i, j;
		for( j = 0; j < 3; j++ )
		{
			i = RoundFloatToInt( pSrcRGB[j] * 1024 );	// assume 0..4 range
			if (i < 0)
			{
				i = 0;
			}
			if (i > 4091)
			{
				i = 4091;
			}
			tmpVect[j] = g_LinearToVertex[i];
		}
#else		
		tmpVect[0] = LinearToVertexLight( pSrcRGB[0] );
		tmpVect[1] = LinearToVertexLight( pSrcRGB[1] );
		tmpVect[2] = LinearToVertexLight( pSrcRGB[2] );
#endif
		ColorClamp( tmpVect );
		
		pDstRGB[0] = RoundFloatToByte( tmpVect[0] * 255.0f );
		pDstRGB[1] = RoundFloatToByte( tmpVect[1] * 255.0f );
		pDstRGB[2] = RoundFloatToByte( tmpVect[2] * 255.0f );
	}

	// Clamp the three values for bumped lighting such that we trade off directionality for brightness.
	FORCEINLINE void ColorClampBumped( Vector& color1, Vector& color2, Vector& color3 )
	{
		Vector maxs;
		Vector *colors[3] = { &color1, &color2, &color3 };
		maxs[0] = VectorMaximum( color1 );
		maxs[1] = VectorMaximum( color2 );
		maxs[2] = VectorMaximum( color3 );

		// HACK!  Clean this up, and add some else statements
#define CONDITION(a,b,c) do { if( maxs[a] >= maxs[b] && maxs[b] >= maxs[c] ) { order[0] = a; order[1] = b; order[2] = c; } } while( 0 )
		
		int order[3] = {};
		CONDITION(0,1,2);
		CONDITION(0,2,1);
		CONDITION(1,0,2);
		CONDITION(1,2,0);
		CONDITION(2,0,1);
		CONDITION(2,1,0);

		int i;
		for( i = 0; i < 3; i++ )
		{
			float max = VectorMaximum( *colors[order[i]] );
			if( max <= 1.0f )
			{
				continue;
			}
			// This channel is too bright. . take half of the amount that we are over and 
			// add it to the other two channel.
			float factorToRedist = ( max - 1.0f ) / max;
			Vector colorToRedist = factorToRedist * *colors[order[i]];
			*colors[order[i]] -= colorToRedist;
			colorToRedist *= 0.5f;
			*colors[order[(i+1)%3]] += colorToRedist;
			*colors[order[(i+2)%3]] += colorToRedist;
		}

		ColorClamp( color1 );
		ColorClamp( color2 );
		ColorClamp( color3 );
		
		if( color1[0] < 0.f ) color1[0] = 0.f;
		if( color1[1] < 0.f ) color1[1] = 0.f;
		if( color1[2] < 0.f ) color1[2] = 0.f;
		if( color2[0] < 0.f ) color2[0] = 0.f;
		if( color2[1] < 0.f ) color2[1] = 0.f;
		if( color2[2] < 0.f ) color2[2] = 0.f;
		if( color3[0] < 0.f ) color3[0] = 0.f;
		if( color3[1] < 0.f ) color3[1] = 0.f;
		if( color3[2] < 0.f ) color3[2] = 0.f;
	}

	FORCEINLINE void LinearToBumpedLightmap( const float *linearColor, const float *linearBumpColor1,
		const float *linearBumpColor2, const float *linearBumpColor3,
		unsigned char *ret, unsigned char *retBump1,
		unsigned char *retBump2, unsigned char *retBump3 )
	{
		const Vector &linearBump1 = *( ( const Vector * )linearBumpColor1 );
		const Vector &linearBump2 = *( ( const Vector * )linearBumpColor2 );
		const Vector &linearBump3 = *( ( const Vector * )linearBumpColor3 );

		Vector gammaGoal;
		// gammaGoal is premultiplied by 1/overbright, which we want
		gammaGoal[0] = LinearToVertexLight( linearColor[0] );
		gammaGoal[1] = LinearToVertexLight( linearColor[1] );
		gammaGoal[2] = LinearToVertexLight( linearColor[2] );
		Vector bumpAverage = linearBump1;
		bumpAverage += linearBump2;
		bumpAverage += linearBump3;
		bumpAverage *= ( 1.0f / 3.0f );
		
		Vector correctionScale;
		if( *( int * )&bumpAverage[0] != 0 && *( int * )&bumpAverage[1] != 0 && *( int * )&bumpAverage[2] != 0 )
		{
			// fast path when we know that we don't have to worry about divide by zero.
			VectorDivide( gammaGoal, bumpAverage, correctionScale );
//			correctionScale = gammaGoal / bumpSum;
		}
		else
		{
			correctionScale.Init( 0.0f, 0.0f, 0.0f );
			if( bumpAverage[0] != 0.0f )
			{
				correctionScale[0] = gammaGoal[0] / bumpAverage[0];
			}
			if( bumpAverage[1] != 0.0f )
			{
				correctionScale[1] = gammaGoal[1] / bumpAverage[1];
			}
			if( bumpAverage[2] != 0.0f )
			{
				correctionScale[2] = gammaGoal[2] / bumpAverage[2];
			}
		}
		Vector correctedBumpColor1;
		Vector correctedBumpColor2;
		Vector correctedBumpColor3;
		VectorMultiply( linearBump1, correctionScale, correctedBumpColor1 );
		VectorMultiply( linearBump2, correctionScale, correctedBumpColor2 );
		VectorMultiply( linearBump3, correctionScale, correctedBumpColor3 );

		Vector check = ( correctedBumpColor1 + correctedBumpColor2 + correctedBumpColor3 ) / 3.0f;

		ColorClampBumped( correctedBumpColor1, correctedBumpColor2, correctedBumpColor3 );

		ret[0] = RoundFloatToByte( gammaGoal[0] * 255.0f );
		ret[1] = RoundFloatToByte( gammaGoal[1] * 255.0f );
		ret[2] = RoundFloatToByte( gammaGoal[2] * 255.0f );
		retBump1[0] = RoundFloatToByte( correctedBumpColor1[0] * 255.0f );
		retBump1[1] = RoundFloatToByte( correctedBumpColor1[1] * 255.0f );
		retBump1[2] = RoundFloatToByte( correctedBumpColor1[2] * 255.0f );
		retBump2[0] = RoundFloatToByte( correctedBumpColor2[0] * 255.0f );
		retBump2[1] = RoundFloatToByte( correctedBumpColor2[1] * 255.0f );
		retBump2[2] = RoundFloatToByte( correctedBumpColor2[2] * 255.0f );
		retBump3[0] = RoundFloatToByte( correctedBumpColor3[0] * 255.0f );
		retBump3[1] = RoundFloatToByte( correctedBumpColor3[1] * 255.0f );
		retBump3[2] = RoundFloatToByte( correctedBumpColor3[2] * 255.0f );
	}

	
	uint16 LinearFloatToCorrectedShort( float in );

	inline unsigned short LinearToUnsignedShort( float in, int nFractionalBits )
	{
		unsigned short out;
		in = in * ( 1 << nFractionalBits );
		in = min( in, 65535.f );
		out = max( in, 0.0f );
		return out;
	}

	inline void ClampToHDR( const Vector &in, unsigned short out[3] )
	{
		Vector tmp = in;
		
		out[0] = LinearFloatToCorrectedShort( in.x );
		out[1] = LinearFloatToCorrectedShort( in.y );
		out[2] = LinearFloatToCorrectedShort( in.z );
	}

	FORCEINLINE void 
		LinearToBumpedLightmap( const float *linearColor, const float *linearBumpColor1,
		const float *linearBumpColor2, const float *linearBumpColor3,
		float *ret, float *retBump1,
		float *retBump2, float *retBump3 )
	{
		const Vector &linearUnbumped = *( ( const Vector * )linearColor );
		Vector linearBump1 = *( ( const Vector * )linearBumpColor1 );
		Vector linearBump2 = *( ( const Vector * )linearBumpColor2 );
		Vector linearBump3 = *( ( const Vector * )linearBumpColor3 );

		// find a scale factor which makes the average of the 3 bumped mapped vectors match the
		// straight up vector (if possible), so that flat bumpmapped areas match non-bumpmapped
		// areas.
		// Note: According to Alex, this code is completely wrong.
		// Because the bump vectors constitute a orthonormal basis, one does not simply average
		// them to get the straight-up result. In fact they are added together then multiplied
		// by 0.575
		Vector bumpAverage = linearBump1;
		bumpAverage += linearBump2;
		bumpAverage += linearBump3;
		bumpAverage *= ( 1.0f / 3.0f );
		
		Vector correctionScale;

		if( *( int * )&bumpAverage[0] != 0 &&
			*( int * )&bumpAverage[1] != 0 &&
			*( int * )&bumpAverage[2] != 0 )
		{
			// fast path when we know that we don't have to worry about divide by zero.
			VectorDivide( linearUnbumped, bumpAverage, correctionScale );
		}
		else
		{
			correctionScale.Init( 0.0f, 0.0f, 0.0f );
			if( bumpAverage[0] != 0.0f )
			{
				correctionScale[0] = linearUnbumped[0] / bumpAverage[0];
			}
			if( bumpAverage[1] != 0.0f )
			{
				correctionScale[1] = linearUnbumped[1] / bumpAverage[1];
			}
			if( bumpAverage[2] != 0.0f )
			{
				correctionScale[2] = linearUnbumped[2] / bumpAverage[2];
			}
		}
		linearBump1 *= correctionScale;
		linearBump2 *= correctionScale;
		linearBump3 *= correctionScale;

		*((Vector *)ret) = linearUnbumped;
		*((Vector *)retBump1) = linearBump1;
		*((Vector *)retBump2) = linearBump2;
		*((Vector *)retBump3) = linearBump3;
	}


	// The domain of the inputs is floats [0 .. 16.0f]
	// the output range is also floats [0 .. 16.0f] (eg, compression to short does not happen)
	FORCEINLINE void 
		LinearToBumpedLightmap( FLTX4 linearColor, FLTX4 linearBumpColor1,
		FLTX4 linearBumpColor2, FLTX4 linearBumpColor3,
		fltx4 &ret, fltx4 &retBump1,		// I pray that with inlining
		fltx4 &retBump2, fltx4 &retBump3 )  // these will be returned on registers
	{
		// preload 3.0f onto the returns so that we don't need to multiply the bumpAverage by it
		// straight away (eg, reschedule this dependent op)
		static const fltx4 vThree = { 3.0f, 3.0f, 3.0f, 0.0f };
		fltx4 retValBump1 = MulSIMD( vThree, linearBumpColor1);
		fltx4 retValBump2 = MulSIMD( vThree, linearBumpColor2);
		fltx4 retValBump3 = MulSIMD( vThree, linearBumpColor3);

		// find a scale factor which makes the average of the 3 bumped mapped vectors match the
		// straight up vector (if possible), so that flat bumpmapped areas match non-bumpmapped
		// areas.
		fltx4 bumpAverage = AddSIMD(AddSIMD(linearBumpColor1, linearBumpColor2), linearBumpColor3); // actually average * 3

		// find the zero terms so that we can quash their channels in the output
		fltx4 zeroTerms = CmpEqSIMD(bumpAverage, Four_Zeros);
		fltx4 correctionScale = ReciprocalEstSIMD(bumpAverage);  // each channel is now 1.0f / (average[x] * 3.0f)

		// divide unbumped linear by the average to get the correction scale
		correctionScale = MulSIMD( AndNotSIMD(zeroTerms, linearColor), // crush values that were zero in bumpAverage. (saves on dep latency)
								   correctionScale);				   // still has an extra 1/3 factor multiplied in

		// multiply this against three to get the return values
		ret = linearColor;
		retBump1 = MulSIMD(retValBump1, correctionScale);
		retBump2 = MulSIMD(retValBump2, correctionScale);
		retBump3 = MulSIMD(retValBump3, correctionScale);
	}

	// input: floats [0 .. 16.0f]
	// output: shorts [0 .. 65535]
	FORCEINLINE void 
		LinearToBumpedLightmap( const float *linearColor, const float *linearBumpColor1,
		const float *linearBumpColor2, const float *linearBumpColor3,
		unsigned short *ret, unsigned short *retBump1,
		unsigned short *retBump2, unsigned short *retBump3 )
	{
		Vector linearUnbumped, linearBump1, linearBump2, linearBump3;
		LinearToBumpedLightmap( linearColor, linearBumpColor1, linearBumpColor2, linearBumpColor3, &linearUnbumped.x, &linearBump1.x, &linearBump2.x, &linearBump3.x );

		ClampToHDR( linearUnbumped, ret );
		ClampToHDR( linearBump1, retBump1 );
		ClampToHDR( linearBump2, retBump2 );
		ClampToHDR( linearBump3, retBump3 );
	}
};

#endif // COLORSPACE_H
