//========= Copyright Valve Corporation, All rights reserved. ============//
#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"
#include "tier2/tier2.h"
#include "tier0/memdbgon.h"
#include "raytrace.h"
#include "bitmap/tgawriter.h"

void main(int argc,char **argv)
{
	InitCommandLineProgram( argc, argv );

	if (argc != 5)
	{
		printf("format is 'rt_test src_image dest_image xsize ysize'\n");
	}
	else
	{
		ReportProgress("reading src texture",0,0);
		FloatBitMap_t src_texture(argv[1]);
		int xsize = atoi( argv[3] );
		int ysize = atoi( argv[4] );
		
		// render a simple scene of a terrain, using a bitmap for color data and its alpha channel for the height
		RayTracingEnvironment rt_Env;
		int id = 1;
		float flXScale = (1.0/(src_texture.Width-1) );
		float flZScale = (1.0/(src_texture.Height-1) );
		for( int y=0; y < src_texture.Height-1; y++ )
			for(int x=0 ; x < src_texture.Width-1; x++ )
			{
				Vector vecVerts[2][2];
				for(int iy=0 ; iy < 2; iy++)
					for(int ix=0 ; ix < 2; ix++)
					{
						vecVerts[ix][iy].x =  2.0* ( ( x+ix )*flXScale-0.5 );
						if ( ( x+ix == src_texture.Width-1 ) || ( y+iy==src_texture.Height-1 ) )
							vecVerts[ix][iy].y = 0;
						else
							vecVerts[ix][iy].y = 0.3*src_texture.Pixel( x+ix, y+iy, 1 );
						vecVerts[ix][iy].z =  -2.0* ( ( y+iy )*flZScale-0.5 );
					}
				Vector vecColor( GammaToLinear(src_texture.Pixel(x,y,0)),
								 GammaToLinear( src_texture.Pixel( x, y, 1 )),
								 GammaToLinear( src_texture.Pixel( x, y, 2 )) );
				rt_Env.AddTriangle( id++, vecVerts[0][0], vecVerts[1][0], vecVerts[1][1], vecColor );
				rt_Env.AddTriangle( id++, vecVerts[0][0], vecVerts[0][1], vecVerts[1][1], vecColor );
			}
		rt_Env.AddTriangle( id++, Vector(0,0,-.2), Vector(.2,0,.2), Vector( -.2,0,.2), Vector( 0,0,1 ) );
		printf("n triangles %d\n",id);
		ReportProgress("Creating kd-tree",0,0);
		float stime = Plat_FloatTime();
		rt_Env.SetupAccelerationStructure();
		printf("kd built time := %d\n", (int) ( Plat_FloatTime() - stime ) );
		rt_Env.AddInfinitePointLight( Vector( 0,5, 0), Vector( .1,.1,.1 ));
		// lets render a frame
		uint32 *buf=reinterpret_cast<uint32 *> ( MemAlloc_AllocAligned( xsize * ysize * 4 , 16 ) );

		Vector EyePos(0,2,0);
		ReportProgress("Rendering",0,0);

// 		rt_Env.RenderScene( xsize, ysize, xsize, buf, Vector( 0, 0.5, -1.0 ),
// 							Vector( -1, 1, 0), 
// 							Vector( 1, 1, 0 ),
// 							Vector( -1, -1, 0 ),
// 							Vector( 1, -1, 0 ) );
		float curtime = Plat_FloatTime();
		for(int i=0;i<10;i++)
		{
			rt_Env.RenderScene( xsize, ysize, xsize, buf,
								EyePos,
								Vector( -1, 0,1)-EyePos, 
								Vector( 1, 0, 1 )-EyePos,
								Vector( -1, 0, -1 )-EyePos,
								Vector( 1, 0, -1 )-EyePos );
		}
		float etime=Plat_FloatTime()-curtime;
		printf("pixels traced and lit per second := %f\n",(10*xsize*ysize)*(1.0/etime));
		TGAWriter::WriteTGAFile( "test.tga", xsize, ysize, IMAGE_FORMAT_RGBA8888,
								 reinterpret_cast<uint8 *> (buf), 4*xsize );
		
		MemAlloc_FreeAligned( buf );
	}

}
