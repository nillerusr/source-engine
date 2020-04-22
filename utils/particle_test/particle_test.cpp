//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "tier0/platform.h"
#include "mathlib/mathlib.h"
#include "mathlib/ssemath.h"
#include "particles.h"
#include "tier2/tier2.h"
#include "tier0/memdbgon.h"

void main(int argc,char **argv)
{
	InitCommandLineProgram( argc, argv );


	// test sse noise
	FourVectors start;
	start.LoadAndSwizzle(Vector(-1,4,3),Vector(0,7,8),Vector(8,1,2),Vector(0,0,0));
	FourVectors delta;
	delta.LoadAndSwizzle(Vector(.1,-.1,.05),Vector(.1,.1,.1),Vector(0,-.1,0),Vector(.1,0,0));
#ifdef TIME_IT
	float start_time=Plat_FloatTime();
	for(int sim=0;sim<1000*1000*10;sim++)
	{
		__m128 n=SSENoise( start );
		start+=delta;
	}
	printf("n/s=%f\n",(4*1000*1000*10.0)/(Plat_FloatTime()-start_time));
#endif
	for(int i=0;i<130;i++)
	{
		__m128 noise=SSENoise( start );
//		printf(" noise(x=%f)=%f\t%f\t%f\t%f\n",
		printf(" %f,%f,%f,%f,%f\n",
			   start.X(0),noise.m128_f32[0],
			   noise.m128_f32[1],
			   noise.m128_f32[2],
			   noise.m128_f32[3]);
		start+=delta;
	}

#if 0
	ReadParticleConfigFile("particles.cfg");
	ParticleCollection lots_o_particles( "fireball" );
	lots_o_particles.SetNActiveParticles( 1000000 );
	// kick the particles up into the air
	lots_o_particles.FillAttributeWithConstant( PARTICLE_ATTRIBUTE_XCOORD, 0.0 );
	lots_o_particles.FillAttributeWithConstant( PARTICLE_ATTRIBUTE_YCOORD, 0.0 );
	lots_o_particles.FillAttributeWithConstant( PARTICLE_ATTRIBUTE_ZCOORD, 10.0 );
	lots_o_particles.FillAttributeWithConstant( PARTICLE_ATTRIBUTE_PREV_XCOORD, 0.0 );
	lots_o_particles.FillAttributeWithConstant( PARTICLE_ATTRIBUTE_PREV_YCOORD, 0.0 );
	lots_o_particles.FillAttributeWithConstant( PARTICLE_ATTRIBUTE_PREV_ZCOORD, 0.0 );
	float start=Plat_FloatTime();
	for(int sim=0;sim<1000;sim++)
		lots_o_particles.Simulate( 0.01 );
	printf("p/s=%f\n",(100.0*1000000)/(Plat_FloatTime()-start));
#endif
														   
}
