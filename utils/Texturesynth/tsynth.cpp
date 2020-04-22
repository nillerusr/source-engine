//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "tier0/platform.h"
#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"
#include "tier2/tier2.h"
#include "bitmap/tgaloader.h"

#define NEIGHBORHOOD_SIZE 5

void SynthesizeTexture(FloatBitMap_t & output, FloatBitMap_t const & input)
{
	// init output with histogram-equalized random pixels
	output.InitializeWithRandomPixelsFromAnotherFloatBM(input);

	// build image pyramids
	FloatImagePyramid_t input_pyramid(input,PYRAMID_MODE_GAUSSIAN);
	FloatImagePyramid_t output_pyramid(output,PYRAMID_MODE_GAUSSIAN);

	// now, synthesize texture from lowest res to highest
	for(int level=min(input_pyramid.m_nLevels,output_pyramid.m_nLevels)-2;level>=0;level--)
	{
		FloatBitMap_t & output=*(output_pyramid.Level(level));
		FloatBitMap_t & output0=*(output_pyramid.Level(level+1));
		FloatBitMap_t & input=*(input_pyramid.Level(level));
		FloatBitMap_t & input0=*(input_pyramid.Level(level+1));
		if ( 
			(input.Width < NEIGHBORHOOD_SIZE*3) ||
			(input.Height < NEIGHBORHOOD_SIZE*3)
			)
		{
			printf("skip level %d\n",level);
			continue;
		}
		// now, synthesize each pixel
		for(int yloop=0;yloop<output.Height;yloop++)
		{
			int yo=((yloop+NEIGHBORHOOD_SIZE) % output.Height);
			printf("level %d line %d\n",level,yo);
			for(int xo=0;xo<output.Width;xo++)
			{
				int best_pixel_x=-1, best_pixel_y=-1;
				float best_error=1.0e22;
				// traverse all neighborhoods of src
				for(int nblk_y=NEIGHBORHOOD_SIZE;nblk_y<input.Height;nblk_y++)
					for(int nblk_x=NEIGHBORHOOD_SIZE;nblk_x<input.Width-NEIGHBORHOOD_SIZE;nblk_x++)
					{
						float our_error=0;
						// now, compare this block to the neighborhood around our output pixel
						for(int oy=-NEIGHBORHOOD_SIZE;oy<=0;oy++)
						{
							int xlimit=NEIGHBORHOOD_SIZE/2;
							if (oy==0)
								xlimit=-1;					// on last line, don't step past our output pixel
							for(int ox=-NEIGHBORHOOD_SIZE/2;ox<=xlimit;ox++)
								for(int c=0;c<3;c++)
								{
									float pix_diff=
										output.PixelWrapped(xo+ox,yo+oy,c)-
										input.Pixel(nblk_x+ox,nblk_y+oy,c);
									our_error+=pix_diff*pix_diff;
									float pix_diff0=
										output0.PixelWrapped(xo+ox,yo+oy,c)-
										input0.PixelClamped(nblk_x+ox,nblk_y+oy,c);
									our_error+=pix_diff0*pix_diff0;
								}
						}
						if (our_error < best_error)
						{
							best_pixel_x=nblk_x;
							best_pixel_y=nblk_y;
							best_error=our_error;
						}
					}
				for(int c=0;c<3;c++)
					output.Pixel(xo,yo,c)=input.Pixel(best_pixel_x,best_pixel_y,c);
			}
		}
	}
	output_pyramid.WriteTGAs("synth_out");
}



void main(int argc,char **argv)
{
	InitCommandLineProgram( argc, argv );
	FloatBitMap_t src_texture(argv[1]);

	int out_width = atoi( argv[2] );
	int out_height = atoi( argv[3] );


	FloatBitMap_t output_map(out_width,out_height);

	SynthesizeTexture(output_map,src_texture);

}
