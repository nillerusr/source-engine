//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include <math.h>

#define OCTAVE_COUNT				4
#define OCTAVE_COUNT_MINUS_ONE		OCTAVE_COUNT - 1

//-----------------------------------------------------------------------------
// quick random number generator - returns a value -1.0f to 1.0
//-----------------------------------------------------------------------------
float RandomNoise2D( int x, int y )
{
	int n = x + y * 57;
	n = ( n << 13 ) ^ n;
	
	float value ( 1.0f - ( float )( ( n * ( n * n * 15731 + 789221 ) + 1376312589 ) & 0x7fffffff ) / 1073741824.0f );

	return value;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float SmoothNoise2D( int x, int y )
{
	float corners = ( RandomNoise2D( x-1, y-1 ) + RandomNoise2D( x+1, y-1 ) + 
		              RandomNoise2D( x-1, y+1 ) + RandomNoise2D( x+1, y+1 ) ) / 16.0f;
	float sides = ( RandomNoise2D( x-1, y ) + RandomNoise2D( x+1, y ) +
		            RandomNoise2D( x, y-1 ) + RandomNoise2D( x, y+1 ) ) / 8.0f;
	float center = RandomNoise2D( x, y ) / 4.0f;
	return corners + sides + center;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline float InterpLinear( float a, float b, float x )
{
	return ( a * ( 1.0f - x ) + b * x );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline float InterpCos( float a, float b, float x )
{
	float alpha = x * 3.1415927f; /* PI */
	float frac = 1.0f - ( float )cos( alpha ) * 0.5f;
	return ( a * ( 1.0f - frac ) + b * frac );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline float InterpCubic( float a, float b, float c, float d, float x )
{
	float P = ( d - c ) - ( a - b );
	float Q = ( a - b ) - P;
	float R = ( c - a );
	float S = b;

	return ( ( P * x * 3.0f ) + ( Q * x * 2.0f ) + ( R * x ) + S );
}
  

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float InterpNoise2D( float x, float y )
{
	int iX = ( int )x;
	float fracX = x - iX;

	int iY = ( int )y;
	float fracY = y - iY;

	float n[4];
	n[0] = SmoothNoise2D( iX, iY );
	n[1] = SmoothNoise2D( iX+1, iY );
	n[2] = SmoothNoise2D( iX, iY+1 );
	n[3] = SmoothNoise2D( iX+1, iY+1 );

	float interpX[2];
	interpX[0] = InterpLinear( n[0], n[1], fracX );
	interpX[1] = InterpLinear( n[2], n[3], fracX );

	return InterpLinear( interpX[0], interpX[1], fracY );
}

//-----------------------------------------------------------------------------
// Purpose:
//   Input: x - 
//          y -
//          rockiness - 0.0f - 1.0f (0.0 = smooth, 1.0f = jagged)
//  Output:
//-----------------------------------------------------------------------------
float PerlinNoise2D( float x, float y, float rockiness )
{
	float total = 0.0;
	float persistence = rockiness;

	for( int ndxOctave = 0; ndxOctave < OCTAVE_COUNT_MINUS_ONE; ndxOctave++ )
	{
		float frequency = ( float )pow( 2.f, ndxOctave );
		float amplitude = ( float )pow( persistence, ndxOctave );
	
		total += InterpNoise2D( x * frequency, y * frequency ) * amplitude;
	}

	return total;
}


//-----------------------------------------------------------------------------
// Purpose:
//   Input: x - 
//          y -
//          rockiness - 0.0f - 1.0f (0.0 = smooth, 1.0f = jagged)
//  Output: is between -1.0f and 1.0f
//-----------------------------------------------------------------------------
float PerlinNoise2DScaled( float x, float y, float rockiness )
{
	float total = 0.0;
	float persistence = rockiness;

	for( int ndxOctave = 0; ndxOctave < OCTAVE_COUNT_MINUS_ONE; ndxOctave++ )
	{
		float frequency = ( float )pow( 2.f, ndxOctave );
		float amplitude = ( float )pow( persistence, ndxOctave );
	
		total += InterpNoise2D( x * frequency, y * frequency ) * amplitude;
	}

	total /= ( float )OCTAVE_COUNT;

	return total;
}
