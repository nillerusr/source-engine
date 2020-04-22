//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "tier0/platform.h"
#include <stdio.h>
#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"
#include "tier2/tier2.h"

#define BRIGHT_THRESH 0.90      // pixels within this % of average are "bright"
#define GROUND_IMPORTANCE 0.2   // weight for downward pointing skymap pixels

float Importance(Vector const &direction)
{
	// this returns a scale factor which can be used to recurd the importance of certain
	// directions.  in particular, this version makes the ground a lot less important than the sky
	if (direction.z>.2)
		return 1.0;
	if (direction.z>0)
		return FLerp(1.0,GROUND_IMPORTANCE,.2,0,direction.z);
	else
		return GROUND_IMPORTANCE;

}

void main(int argc,char **argv)
{
	InitCommandLineProgram(argc, argv);
	if (argc!=2)
	{
		printf("format is %s basename\n",argv[0]);
	}
	else
	{
		FloatCubeMap_t cmap(argv[1]);
		// find the brightest pixel. We will consider the pixels neat this to be the
		// ones contrinbuting to the light source
		float max_color=cmap.BrightestColor();
		float threshhold=max_color*.90;
		// now, find average color of non-bright pixels
		float sumweights=0.0;
		Vector AverageColor(0,0,0);
		for(int f=0;f<6;f++)
			for(int y=0;y<cmap.face_maps[f].Height;y++)
				for(int x=0;x<cmap.face_maps[f].Width;x++)
				{
					Vector clr(
						cmap.face_maps[f].Pixel(x,y,0),
						cmap.face_maps[f].Pixel(x,y,1),
						cmap.face_maps[f].Pixel(x,y,2));
					float mag=clr.Length();
					if (mag<threshhold)
					{
						float weight=Importance(cmap.PixelDirection(f,x,y));
						sumweights+=weight;
						AverageColor+=weight*clr;
					}
				}
		AverageColor*=(1.0/sumweights);

		Vector avg_light_dir(0,0,0);
		Vector AverageHue(0,0,0);
		// now, find average direction and color of bright pixels
		for(int f=0;f<6;f++)
			for(int y=0;y<cmap.face_maps[f].Height;y++)
				for(int x=0;x<cmap.face_maps[f].Width;x++)
				{
					Vector clr(
						cmap.face_maps[f].Pixel(x,y,0),
						cmap.face_maps[f].Pixel(x,y,1),
						cmap.face_maps[f].Pixel(x,y,2));
					float mag=clr.Length();
					if (mag>threshhold)
					{
						clr-=AverageColor;
						AverageHue+=clr;
						Vector pdir=cmap.PixelDirection(f,x,y);
						pdir*=clr.Length();
						avg_light_dir+=pdir;
					}
				}
		VectorNormalize(AverageHue);
		VectorNormalize(avg_light_dir);

		printf("Point light dir=%f %f %f\n",avg_light_dir.x,avg_light_dir.y,avg_light_dir.z);
//		printf("Point light color=%f %f %f\n",AverageHue.x,AverageHue.y,AverageHue.z);
		printf("Point light color=%d %d %d 255\n",
			( int )( 255 * pow( AverageHue.x, 1.0f / 2.2f ) ),
			( int )( 255 * pow( AverageHue.y, 1.0f / 2.2f ) ),
			( int )( 255 * pow( AverageHue.z, 1.0f / 2.2f ) ) );

		// now, output ambient cube maps for image-based lighting. During this pass, we will also
		// correct the ambient color

		FloatCubeMap_t conv(32,32);

		Vector AmbientColor(0,0,0);
		float sumweights_amb=0;
		
		for(int f=0;f<6;f++)
			for(int y=0;y<conv.face_maps[f].Height;y++)
				for(int x=0;x<conv.face_maps[f].Width;x++)
				{
					Vector pdir=conv.PixelDirection(f,x,y);
					float dot=pdir.Dot(avg_light_dir);
					if (dot<0) dot=0;

					float sumdot=0;
					Vector sumlight(0,0,0);
					for(int f1=0;f1<6;f1++)
						for(int y1=0;y1<cmap.face_maps[f].Height;y1+=20)
							for(int x1=0;x1<cmap.face_maps[f].Width;x1+=20)
							{
								Vector sdir=cmap.PixelDirection(f1,x1,y1);
								float dot_sphere=sdir.Dot(pdir);
								if (dot_sphere>0)
								{
									sumdot+=dot_sphere;
									for(int comp=0;comp<3;comp++)
										sumlight[comp]+=
											dot_sphere*cmap.face_maps[f1].Pixel(x1,y1,comp);
								}
						}
					sumlight*=1.0/sumdot;
					// sumlight is the desired lighting

					// use our calculated point light source to find the error
					float weight=Importance(pdir);
					sumweights_amb+=weight;
					for(int comp=0;comp<3;comp++)
					{
						conv.face_maps[f].Pixel(x,y,comp)=sumlight[comp]; 
						AmbientColor[comp]+=weight*(sumlight[comp]-dot*AverageHue[comp]);
					}

				}
		AmbientColor*=1.0/sumweights_amb;
		
		conv.WritePFMs("ambient_cube_");
		
		
//		printf("Ambient color=%f %f %f\n",AmbientColor.x,AmbientColor.y,AmbientColor.z);
		// convert to gamma space. . .
		printf("Ambient color=%d %d %d 255\n",
			( int )( 255 * pow( AmbientColor.x, 1.0f/2.2f ) ),
			( int )( 255 * pow( AmbientColor.y, 1.0f/2.2f ) ),
			( int )( 255 * pow( AmbientColor.z, 1.0f/2.2f ) ) );

		for(int f=0;f<6;f++)
			for(int y=0;y<cmap.face_maps[f].Height;y++)
				for(int x=0;x<cmap.face_maps[f].Width;x++)
				{
					Vector pdir=cmap.PixelDirection(f,x,y);
					float dot=pdir.Dot(avg_light_dir);
					if (dot<0) dot=0;
					dot=pow(dot,7);
					for(int comp=0;comp<3;comp++)
						cmap.face_maps[f].Pixel(x,y,comp)=AmbientColor[comp]+dot*AverageHue[comp];
				}
		cmap.WritePFMs("directional_plus_ambient_");
	}



}

