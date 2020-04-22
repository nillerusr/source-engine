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
#include "vstdlib/vstdlib.h"
#include "raytrace.h"
#include "mathlib/bumpvects.h"
#include "mathlib/halton.h"
#include "tier0/threadtools.h"
#include "tier0/progressbar.h"

// In order to handle intersections with wrapped copies, we repeat the bitmap triangles this many
// times
#define NREPS_TILE 1
extern int n_intersection_calculations;



struct SSBumpCalculationContext								// what each thread needs to see
{
	RayTracingEnvironment *m_pRtEnv;
	FloatBitMap_t *ret_bm;									// the bitmnap we are building
	FloatBitMap_t const *src_bm;
	int nrays_to_trace_per_pixel;
	float bump_scale;
	Vector *trace_directions;								// light source directions to trace
	Vector *normals;
	int min_y;												// range of scanlines to computer for
	int max_y;
	uint32 m_nOptionFlags;
	int thread_number;
};


static unsigned SSBumpCalculationThreadFN( void * ctx1 )
{
	SSBumpCalculationContext *ctx = ( SSBumpCalculationContext * ) ctx1;

	RayStream ray_trace_stream_ctx;

	RayTracingSingleResult * rslts = new
		RayTracingSingleResult[ctx->ret_bm->Width * ctx->nrays_to_trace_per_pixel];


	for( int y = ctx->min_y; y <= ctx->max_y; y++ )
	{
		if ( ctx->thread_number == 0 )
			ReportProgress("Computing output",(1+ctx->max_y-ctx->min_y),y-ctx->min_y);
		for( int r = 0; r < ctx->nrays_to_trace_per_pixel; r++ )
		{
			for( int x = 0; x < ctx->ret_bm->Width; x++ )
			{
				Vector surf_pnt( x, y, ctx->bump_scale * ctx->src_bm->Pixel( x, y, 3 ) );
				// move the ray origin up a hair
				surf_pnt.z += 0.55;
				Vector trace_end = surf_pnt;
				Vector trace_dir = ctx->trace_directions[ r ];
				trace_dir *= ( 1 + NREPS_TILE * 2 ) * max( ctx->src_bm->Width, ctx->src_bm->Height );
				trace_end += trace_dir;
				ctx->m_pRtEnv->AddToRayStream( ray_trace_stream_ctx, surf_pnt, trace_end,
											 & ( rslts[ r + ctx->nrays_to_trace_per_pixel * ( x )] ));
			}
		}
		if ( ctx->nrays_to_trace_per_pixel )
			ctx->m_pRtEnv->FinishRayStream( ray_trace_stream_ctx );
		// now, all ray tracing results are in the results buffer. Determine the visible self-shadowed
		// bump map lighting at each vertex in each basis direction
		for( int x = 0; x < ctx->src_bm->Width; x++ )
		{
			int nNumChannels = ( ctx->m_nOptionFlags & SSBUMP_OPTION_NONDIRECTIONAL ) ? 1 : 3;
			for( int c = 0; c < nNumChannels; c++ )
			{
				float sum_dots = 0;
				float sum_possible_dots = 0;
				Vector ldir = g_localBumpBasis[c];
				float ndotl = DotProduct( ldir, ctx->normals[x + y * ctx->src_bm->Width] );
				if ( ndotl < 0 )
					ctx->ret_bm->Pixel( x, y, c ) = 0;
				else
				{
					if ( ctx->nrays_to_trace_per_pixel )
					{
						RayTracingSingleResult *this_rslt =
							rslts + ctx->nrays_to_trace_per_pixel * ( x );
						for( int r = 0; r < ctx->nrays_to_trace_per_pixel; r++ )
						{
							float dot;
							if ( ctx->m_nOptionFlags & SSBUMP_OPTION_NONDIRECTIONAL )
								dot = ctx->trace_directions[r].z;
							else
								dot = DotProduct( ldir, ctx->trace_directions[r] );
							if ( dot > 0 )
							{
								sum_possible_dots += dot;
								if ( this_rslt[r].HitID == - 1 )
									sum_dots += dot;
							}
						}
					}
					else
					{
						sum_dots = sum_possible_dots = 1.0;
					}
					ctx->ret_bm->Pixel( x, y, c ) = ( ndotl * sum_dots ) / sum_possible_dots;
				}
			}
			if ( ctx->m_nOptionFlags & SSBUMP_OPTION_NONDIRECTIONAL )
			{
				ctx->ret_bm->Pixel( x, y, 1 ) = ctx->ret_bm->Pixel( x, y, 0 );					// copy height
				ctx->ret_bm->Pixel( x, y, 2 ) = ctx->ret_bm->Pixel( x, y, 0 );					// copy height
				ctx->ret_bm->Pixel( x, y, 3 ) = ctx->ret_bm->Pixel( x, y, 0 );					// copy height
			}
			else
			{
				ctx->ret_bm->Pixel( x, y, 3 ) = ctx->src_bm->Pixel( x, y, 3 );					// copy height
			}
		}
	}
	delete[] rslts;
	return 0;
}

void FloatBitMap_t::ComputeVertexPositionsAndNormals( float flHeightScale, Vector **ppPosOut, Vector **ppNormalOut ) const
{
	Vector *verts = new Vector[Width * Height];
	// first, calculate vertex positions
	for( int y = 0; y < Height; y++ )
		for( int x = 0; x < Width; x++ )
		{
			Vector * out = verts + x + y * Width;
			out->x = x;
			out->y = y;
			out->z = flHeightScale * Pixel( x, y, 3 );
		}

	Vector *normals = new Vector[Width * Height];
	// now, calculate normals, smoothed
	for( int y = 0; y < Height; y++ )
		for( int x = 0; x < Width; x++ )
		{
			// now, calculcate average normal
			Vector avg_normal( 0, 0, 0 );
			for( int xofs =- 1;xofs <= 1;xofs++ )
				for( int yofs =- 1;yofs <= 1;yofs++ )
				{
					int x0 = ( x + xofs );
					if ( x0 < 0 )
						x0 += Width;
					int y0 = ( y + yofs );
					if ( y0 < 0 )
						y0 += Height;
					x0 = x0 % Width;
					y0 = y0 % Height;
					int x1 = ( x0 + 1 ) % Width;
					int y1 = ( y0 + 1 ) % Height;
					// now, form the two triangles from this vertex
					Vector p0 = verts[x0 + y0 * Width];
					Vector e1 = verts[x1 + y0 * Width];
					e1 -= p0;
					Vector e2 = verts[x0 + y1 * Width];
					e2 -= p0;
					Vector n1;
					CrossProduct( e1, e2, n1 );
					if ( n1.z < 0 )
						n1.Negate();
					e1 = verts[x + y1 * Width];
					e1 -= p0;
					e2 = verts[x1 + y1 * Width];
					e2 -= p0;
					Vector n2;
					CrossProduct( e1, e2, n2 );
					if ( n2.z < 0 )
						n2.Negate();
					n1.NormalizeInPlace();
					n2.NormalizeInPlace();
					avg_normal += n1;
					avg_normal += n2;
				}
			avg_normal.NormalizeInPlace();
			normals[x + y * Width]= avg_normal;
		}
	*ppPosOut = verts;
	*ppNormalOut = normals;
}

FloatBitMap_t *FloatBitMap_t::ComputeSelfShadowedBumpmapFromHeightInAlphaChannel(
	float bump_scale, int nrays_to_trace_per_pixel, 
	uint32 nOptionFlags ) const
{

	// first, add all the triangles from the height map to the "world".
	// we will make multiple copies to handle wrapping
	int tcnt = 1;

	Vector * verts;
	Vector * normals;
	ComputeVertexPositionsAndNormals( bump_scale, & verts, & normals );

	RayTracingEnvironment rtEnv;
	rtEnv.Flags |= RTE_FLAGS_DONT_STORE_TRIANGLE_COLORS;	// save some ram

	if ( nrays_to_trace_per_pixel )
	{
		rtEnv.MakeRoomForTriangles( ( 1 + 2 * NREPS_TILE ) * ( 1 + 2 * NREPS_TILE ) * 2 * Height * Width );

		// now, add a whole mess of triangles to trace against
		for( int tilex =- NREPS_TILE; tilex <= NREPS_TILE; tilex++ )
			for( int tiley =- NREPS_TILE; tiley <= NREPS_TILE; tiley++ )
			{
				int min_x = 0;
				int max_x = Width - 1;
				int min_y = 0;
				int max_y = Height - 1;
				if ( tilex < 0 )
					min_x = Width / 2;
				if ( tilex > 0 )
					max_x = Width / 2;
				if ( tiley < 0 )
					min_y = Height / 2;
				if ( tiley > 0 )
					max_y = Height / 2;
				for( int y = min_y; y <= max_y; y++ )
					for( int x = min_x; x <= max_x; x++ )
					{
						Vector ofs( tilex * Width, tiley * Height, 0 );
						int x1 = ( x + 1 ) % Width;
						int y1 = ( y + 1 ) % Height;
						Vector v0 = verts[x + y * Width];
						Vector v1 = verts[x1 + y * Width];
						Vector v2 = verts[x1 + y1 * Width];
						Vector v3 = verts[x + y1 * Width];
						v0.x = x; v0.y = y;
						v1.x = x + 1; v1.y = y;
						v2.x = x + 1; v2.y = y + 1;
						v3.x = x; v3.y = y + 1;
						v0 += ofs; v1 += ofs; v2 += ofs; v3 += ofs;
						rtEnv.AddTriangle( tcnt++, v0, v1, v2, Vector( 1, 1, 1 ) );
						rtEnv.AddTriangle( tcnt++, v0, v3, v2, Vector( 1, 1, 1 ) );
					}
			}
		//printf("added %d triangles\n",tcnt-1);
		ReportProgress("Creating kd-tree",0,0);
		rtEnv.SetupAccelerationStructure();
		// ok, now we have built a structure for ray intersection. we will take advantage
		// of the SSE ray tracing code by intersecting rays as a batch.
	}

	// We need to calculate for each vertex (i.e. pixel) of the heightmap, how "much" of the world
	// it can see in each basis direction. we will do this by sampling a sphere of rays around the
	// vertex, and using dot-product weighting to measure the lighting contribution in each basis
	// direction.  note that the surface normal is not used here. The surface normal will end up
	// being reflected in the result because of rays being blocked when they try to pass through
	// the planes of the triangles touching the vertex.

	// note that there is no reason inter-bounced lighting could not be folded into this
	// calculation.

	FloatBitMap_t * ret = new FloatBitMap_t( Width, Height );


	Vector *trace_directions=new Vector[nrays_to_trace_per_pixel];
	DirectionalSampler_t my_sphere_sampler;
	for( int r=0; r < nrays_to_trace_per_pixel; r++)
	{
		Vector trace_dir=my_sphere_sampler.NextValue();
//		trace_dir=Vector(1,0,0);
		trace_dir.z=fabs(trace_dir.z);						// upwards facing only
		trace_directions[ r ]= trace_dir;
	}

	volatile SSBumpCalculationContext ctxs[32];
	ctxs[0].m_pRtEnv =& rtEnv;
	ctxs[0].ret_bm = ret;
	ctxs[0].src_bm = this;
	ctxs[0].nrays_to_trace_per_pixel = nrays_to_trace_per_pixel;
	ctxs[0].bump_scale = bump_scale;
	ctxs[0].trace_directions = trace_directions;
	ctxs[0].normals = normals;
	ctxs[0].min_y = 0;
	ctxs[0].max_y = Height - 1;
	ctxs[0].m_nOptionFlags = nOptionFlags;
	int nthreads = min( 32, (int)GetCPUInformation()->m_nPhysicalProcessors );

	ThreadHandle_t waithandles[32];
	int starty = 0;
	int ystep = Height / nthreads;
	for( int t = 0;t < nthreads; t++ )
	{
		if ( t )
			memcpy( (void * ) ( & ctxs[t] ), ( void * ) & ctxs[0], sizeof( ctxs[0] ));
		ctxs[t].thread_number = t;
		ctxs[t].min_y = starty;
		if ( t != nthreads - 1 )
			ctxs[t].max_y = min( Height - 1, starty + ystep - 1 );
		else
			ctxs[t].max_y = Height - 1;
		waithandles[t]= CreateSimpleThread( SSBumpCalculationThreadFN, ( SSBumpCalculationContext * ) & ctxs[t] );
		starty += ystep;
	}
	for(int t=0;t<nthreads;t++)
	{
		ThreadJoin( waithandles[t] );
	}
	if ( nOptionFlags & SSBUMP_MOD2X_DETAIL_TEXTURE )
	{
		const float flOutputScale = 0.5 * ( 1.0 / .57735026 ); // normalize so that a flat normal yields 0.5
		// scale output weights by color channel
		for( int nY = 0; nY < Height; nY++ )
			for( int nX = 0; nX < Width; nX++ )
			{
				float flScale = flOutputScale * (2.0/3.0) * ( Pixel( nX, nY, 0 ) + Pixel( nX, nY, 1 ) + Pixel( nX, nY, 2 ) );
				ret->Pixel( nX, nY, 0 ) *= flScale;
				ret->Pixel( nX, nY, 1 ) *= flScale;
				ret->Pixel( nX, nY, 2 ) *= flScale;
			}
	}
	
	delete[] verts;
	delete[] trace_directions;
	delete[] normals;
	return ret;												// destructor will clean up rtenv
}

// generate a conventional normal map from a source with height stored in alpha.
FloatBitMap_t *FloatBitMap_t::ComputeBumpmapFromHeightInAlphaChannel( float bump_scale ) const
{
	Vector *verts;
	Vector *normals;
	ComputeVertexPositionsAndNormals( bump_scale, &verts, &normals );
	FloatBitMap_t *ret=new FloatBitMap_t( Width, Height );
	for( int y = 0; y < Height; y++ )
		for( int x = 0; x < Width; x++ )
		{
			Vector const & N = normals[ x + y * Width ];
			ret->Pixel( x, y, 0 ) = 0.5+ 0.5 * N.x;
			ret->Pixel( x, y, 1 ) = 0.5+ 0.5 * N.y;
			ret->Pixel( x, y, 2 ) = 0.5+ 0.5 * N.z;
			ret->Pixel( x, y, 3 ) = Pixel( x, y, 3 );
		}
	return ret;
}

