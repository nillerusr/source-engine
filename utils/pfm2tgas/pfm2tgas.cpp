//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include <tier0/platform.h>
#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"
#include <tier2/tier2.h>

#define N_TEXTURES_OUTPUT 3
#define TEXTURE_TO_TEXTURE_SCALE 8.0
#define BASE_SCALE 0.25

static float Truncate8( float f )
{
	uint8 px= (uint8) min(255.0, (f*255.0));
	return px*(1.0/255.0);
}

static float ErrorWeights[]={ 0.3, 0.59, 0.11 };

void main(int argc,char **argv)
{
	InitCommandLineProgram( argc, argv );
	if ( argc==3)
	{
		FloatBitMap_t src_texture;
		src_texture.LoadFromPFM( argv[1] );
		float sfactor=atof(argv[2]);
		// we will split the float texture into 2 textures.
		FloatBitMap_t output_textures[N_TEXTURES_OUTPUT];
		for(int o=0; o<N_TEXTURES_OUTPUT; o++)
		{
			output_textures[o].AllocateRGB( src_texture.Width, src_texture.Height );
		}
		for(int y=0; y < src_texture.Height; y++)
			for(int x=0; x< src_texture.Width; x++)
			{
				for(int c=0;c<3;c++)
					src_texture.Pixel(x,y,c) *= sfactor;
				float curscale=BASE_SCALE;
				float px_error[N_TEXTURES_OUTPUT];
				float sum_error=0;
				for(int o=0; o<N_TEXTURES_OUTPUT; o++)
				{
					px_error[o]=0.;
					for(int c=0;c<3;c++)
					{
						float opixel=Truncate8( src_texture.Pixel( x, y, c )/curscale );

						output_textures[o].Pixel( x, y, c )= opixel;
						float expanded_pixel = curscale * opixel;
						float err= fabs( src_texture.Pixel( x, y, c )- expanded_pixel );
						if (src_texture.Pixel(x,y,c) > curscale)
							err *=2;
						px_error[o] += ErrorWeights[o] * err;
					}
					sum_error += px_error[o];
					curscale *= TEXTURE_TO_TEXTURE_SCALE;
				}
				// now, set the weight for each map based upon closeness of fit between the scales
				for(int o=0; o<N_TEXTURES_OUTPUT; o++)
				{
					if (sum_error == 0)						// possible, I guess, for black
					{
						if ( o == 0)
							src_texture.Pixel(x, y, 3) = 1.0;
						else
							src_texture.Pixel(x, y, 3) = 0.0;
					}
					else
						output_textures[o].Pixel(x, y, 3)=1.0-(px_error[o]/sum_error);
				}
			}
		// now, output results
		for(int o=0;o<N_TEXTURES_OUTPUT;o++)
		{
			char fnamebuf[1024];
			strcpy(fnamebuf, argv[1]);
			char *dot=strchr(fnamebuf,'.');
			if (! dot)
				dot=fnamebuf+strlen(fnamebuf);
			sprintf(dot,"_%d.tga",o);
			output_textures[o].WriteTGAFile(fnamebuf);
		}
	}
	else
		printf("format is 'pfm2tgas xxx.pfm scalefactor'\n");
}
