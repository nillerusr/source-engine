//========= Copyright Valve Corporation, All rights reserved. ============//
#include "tier0/platform.h"
#include "mathlib/mathlib.h"
#include "mathlib/spherical_geometry.h"
#include "tier2/tier2.h"
#include "mathlib/halton.h"
#include "bitmap/float_bm.h"
#include "tier0/memdbgon.h"

void main(int argc,char **argv)
{
	InitCommandLineProgram( argc, argv );

	// 1/8th of the sphere
	float a1=UnitSphereTriangleArea( Vector( 1, 0, 0 ), Vector( 0, 0, -1 ), Vector( 0, 1, 0 ) );
	printf( "right spherical triangle projected percentage=%2.4f\n", a1 / ( 4 * M_PI ));

	// a small one
	Vector v1 = Vector( 1, 0, 0 );
	Vector v2 = v1 + Vector( 0, 0.2, 0 );
	Vector v3 = v1 + Vector( 0, 0, 0.2 );
	v2.NormalizeInPlace();
	v3.NormalizeInPlace();
	float a2=UnitSphereTriangleArea( v1, v2, v3 );
	printf( "small spherical triangle projected percentage=%2.5f\n", a2 / ( 4* M_PI ) );

	// now, create a cubemap and sum the area of each of its cells
	FloatCubeMap_t envMap( 10, 10 );
	float flAreaSum = 0.;
	for( int nFace = 0 ; nFace < 6; nFace ++ )
	{
		for( int nY = 0 ; nY < 9; nY++ )
			for( int nX = 0 ; nX < 9; nX++ )
			{
				Vector v00 = envMap.PixelDirection( nFace, nX, nY );
				Vector v01 = envMap.PixelDirection( nFace, nX, nY + 1  );
				Vector v10 = envMap.PixelDirection( nFace, nX + 1, nY );
				Vector v11 = envMap.PixelDirection( nFace, nX + 1 , nY + 1 );
				v00.NormalizeInPlace();
				v01.NormalizeInPlace();
				v10.NormalizeInPlace();
				v11.NormalizeInPlace();
				flAreaSum += UnitSphereTriangleArea( v00, v01, v10 );
				flAreaSum += UnitSphereTriangleArea( v10, v11, v01 );
			}
	}
	printf( "sum of areas of cubemap cells = %2.2f\n", flAreaSum / ( 4.0 * M_PI ) );
	
#if 0  // visual spherical harmonics as (confusing) point sets
	// spherical harmonics
	DirectionalSampler_t sampler;
	for(int i = 0 ; i < 50000; i++ )
	{
		Vector dir=sampler.NextValue();
		float SH = SphericalHarmonic( 4, 3, dir );
		float r=0;
		float g=1; //0.5+0.5*DotProduct( dir, Vector( 0, 0, 1 ) );
		float b=0;
		if ( SH < 0 )
		{
			SH = -SH;
			r=g;
			g=0;
		}
		r *= SH;
		g *= SH;
		b *= SH;
		float rad= SH * 4.0; //4.0; //SH *= 8.0;
		printf( "2\n" );
		printf( "%f %f %f %f %f %f\n",
				dir.x * rad, dir.y * rad, dir.z * rad, r, g, b );
		rad += 0.03;
		printf( "%f %f %f %f %f %f\n",
				dir.x * rad, dir.y * rad, dir.z * rad, r, g, b );
	}
#endif

}

