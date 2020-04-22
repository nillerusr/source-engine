//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"
#include "tier1/strtools.h"
#include "tier2/tier2.h"
#include "tier0/memdbgon.h"

static float RangeAdjust( float x )
{
	return (2*(x-.5));
}

static float saturate_and_square( float x )
{
	x=max(0.f,min(1.f, x) );
	return x * x;
}

#define OO_SQRT_3 0.57735025882720947f
static Vector bumpBasis[3] = {
	Vector( 0.81649661064147949f, 0.0f, OO_SQRT_3 ),
	Vector(  -0.40824833512306213f, 0.70710676908493042f, OO_SQRT_3 ),
	Vector(  -0.40824821591377258f, -0.7071068286895752f, OO_SQRT_3 )
};

void main(int argc,char **argv)
{
	InitCommandLineProgram( argc, argv );
	if (argc != 2)
	{
		printf("format is 'normal2ssbump filename.tga\n");
	}
	else
	{
		ReportProgress( "reading src texture",0,0 );
		FloatBitMap_t src_texture(argv[1]);
		for(int y=0;y<src_texture.Height;y++)
		{
			ReportProgress( "Converting to ssbump format",src_texture.Height,y );
			for(int x=0;x<src_texture.Width;x++)
			{
				Vector n( RangeAdjust( src_texture.Pixel( x,y,0 ) ),
						  RangeAdjust( src_texture.Pixel( x,y,1 ) ),
						  RangeAdjust( src_texture.Pixel( x,y,2 ) ) );
				Vector dp( saturate_and_square( DotProduct( n, bumpBasis[0] ) ),
						   saturate_and_square( DotProduct( n, bumpBasis[1] ) ),
						   saturate_and_square( DotProduct( n, bumpBasis[2] ) ) );
				float sum = DotProduct(dp, Vector(1,1,1));
				dp *= 1.0/sum;
				src_texture.Pixel(x,y,0) = dp.x;
				src_texture.Pixel(x,y,1) = dp.y;
				src_texture.Pixel(x,y,2) = dp.z;
			}
		}
		char oname[1024];
		strcpy(oname,argv[1]);
		char *dot=Q_stristr(oname,"_normal");
		if (! dot)
			dot=strchr(oname,'.');
		if (! dot)
			dot=oname+strlen(oname);
		strcpy(dot,"_ssbump.tga");
		printf( "\nWriting %s\n",oname );
		src_texture.WriteTGAFile( oname );
	}

}
