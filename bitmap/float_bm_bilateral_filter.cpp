//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include <tier0/platform.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "bitmap/float_bm.h"
#include <tier2/tier2.h>
#include "tier0/threadtools.h"
#include "tier0/progressbar.h"

struct TBFCalculationContext
{
	int min_y,max_y;											// range to calculate in this thread
	int thread_number;
	int radius_in_pixels;
	float edge_threshold_value;
	FloatBitMap_t const *orig_bm;
	FloatBitMap_t *dest_bm;
};

static unsigned TBFCalculationThreadFN( void *ctx1 )
{
	TBFCalculationContext *ctx = (TBFCalculationContext *) ctx1;
	for(int y=ctx->min_y; y <= ctx->max_y; y++)
	{
		if ( ctx->thread_number == 0 )
			ReportProgress("Performing bilateral filter",(1+ctx->max_y-ctx->min_y),
						   y-ctx->min_y);
		for(int x=0; x < ctx->dest_bm->Width; x++)
			for(int c=0;c<4;c++)
			{
				float sum_weights=0;
				float filter_sum=0;
				float centerp=ctx->orig_bm->Pixel(x,y,c);
				for(int iy=-ctx->radius_in_pixels; iy <= ctx->radius_in_pixels; iy++)
					for(int ix=-ctx->radius_in_pixels; ix <= ctx->radius_in_pixels; ix++)
					{
						float this_p=ctx->orig_bm->PixelWrapped(x+ix,y+iy,c);
						
						// caluclate the g() term. We use a gaussian
						float exp1=(ix*ix+iy*iy)*(1.0/(2.0*ctx->radius_in_pixels*.033));
						float g=exp(-exp1);
						// calculate the "similarity" term. We use a triangle filter
						float s=1.0;
						float cdiff=fabs(centerp-this_p);
						s= (cdiff>ctx->edge_threshold_value)?0:
							FLerp(1,0,0,ctx->edge_threshold_value,cdiff);
						sum_weights += s*g;
						filter_sum += s*g*this_p;
					}
				ctx->dest_bm->Pixel(x,y,c)=filter_sum/sum_weights;
			}
	}
	return 0;
}

void FloatBitMap_t::TileableBilateralFilter( int radius_in_pixels, 
											 float edge_threshold_value )
{
	FloatBitMap_t orig( this );								// need a copy for the source
	TBFCalculationContext ctxs[32];
	ctxs[0].radius_in_pixels = radius_in_pixels;
	ctxs[0].edge_threshold_value = edge_threshold_value;
	ctxs[0].orig_bm = &orig;
	ctxs[0].dest_bm = this;
	int nthreads = min( 32, (int)GetCPUInformation()->m_nPhysicalProcessors );
	ThreadHandle_t waithandles[32];
	int starty=0;
	int ystep=Height/nthreads;

	for(int t=0;t<nthreads;t++)
	{
		if (t)
			ctxs[t]=ctxs[0];
		ctxs[t].thread_number=t;
		ctxs[t].min_y=starty;
		if (t != nthreads-1)
			ctxs[t].max_y=min(Height-1,starty+ystep-1);
		else
			ctxs[t].max_y=Height-1;
		waithandles[t]=CreateSimpleThread(TBFCalculationThreadFN, &ctxs[t]);
		starty+=ystep;
	}
	for(int t=0;t<nthreads;t++)
	{
		ThreadJoin( waithandles[t] );
	}
}

