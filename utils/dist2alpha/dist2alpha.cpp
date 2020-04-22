//========= Copyright Valve Corporation, All rights reserved. ============//
#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"
#include "tier2/tier2.h"
#include "tier0/memdbgon.h"

#define SQ(x) ((x)*(x))

int main( int argc, char ** argv )
{
	InitCommandLineProgram( argc, argv );

	int nCurArg = 1;
	float flSpread = 1.0;

	bool bCodeChannel[4];
	bCodeChannel[0] = false;
	bCodeChannel[1] = false;
	bCodeChannel[2] = false;
	bCodeChannel[3] = true;

	while (( nCurArg < argc ) && ( argv[nCurArg][0] == '-' ) )
	{
		switch( argv[nCurArg][1] )
		{
			case 's':										// spread
			{
				flSpread = atof( argv[++nCurArg] );
			}
			break;

			case '4':										// encode all 4 channels
			{
				bCodeChannel[0] = true;
				bCodeChannel[1] = true;
				bCodeChannel[2] = true;
			}
			break;

				
			default:
				printf("unrecogized option %s\n", argv[nCurArg] );
				exit(1);
		}
		nCurArg++;
		
	}
	argc -= ( nCurArg - 1 );
	argv += ( nCurArg - 1 );

	if ( argc != 5 )
	{
		printf( "format is dist2alpha src_bm.tga dest_bm.tga dest_width dest_height\n" );
		exit( 1 );
	}
	FloatBitMap_t hires_image( argv[1] );
	int out_w = atoi( argv[3] );
	int out_h = atoi( argv[4] );
	FloatBitMap_t lores( out_w, out_h );

	float max_rad = 2.0 * flSpread * max( hires_image.Width * ( 1.0 / out_w ), hires_image.Height * ( 1.0 / out_h ));
	int irad = ceil( max_rad );
	for( int comp = 0;comp < 4;comp ++ )
	{
		for( int y = 0;y < out_h;y ++ )
			for( int x = 0;x < out_w;x ++ )
			{
				int orig_x = FLerp( 0, hires_image.Width - 1, 0, out_w - 1, x );
				int orig_y = FLerp( 0, hires_image.Height - 1, 0, out_h - 1, y );
				if ( bCodeChannel[ comp ] )
				{
					float closest_dist = 100000;
					int inside_test = ( hires_image.Pixel( orig_x, orig_y, comp ) >.9 );
					for( int oy =- irad;oy <= irad;oy ++ )
						for( int ox =- irad;ox <= irad;ox ++ )
						{
							int x1 = orig_x + ox;
							int y1 = orig_y + oy;
							if ( ( x1 >= 0 ) && ( x1 < hires_image.Width ) && ( y1 >= 0 ) && ( y1 < hires_image.Height ) &&
								 ( inside_test != ( hires_image.Pixel( orig_x + ox, orig_y + oy, comp ) >.9 )) )
							{
								float d2 = sqrt( (float ) ( SQ( ox ) + SQ( oy )) );
								if ( d2 < closest_dist )
									closest_dist = d2;
							}
						}
					// done
					float dnum = min( 0.5f, FLerp( 0, .5, 0, max_rad, closest_dist ));
					if ( !inside_test )
						dnum =- dnum;
					lores.Pixel( x, y, comp ) = 0.5 + dnum;
				}
				else
					lores.Pixel( x, y, comp ) = hires_image.Pixel( orig_x, orig_y, comp );
			}
	}

	lores.WriteTGAFile( argv[2] );
	return 0;

}
