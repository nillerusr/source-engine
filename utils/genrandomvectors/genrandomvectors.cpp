//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <stdlib.h>
#include <stdio.h>
#include "vstdlib/random.h"
#include "math.h"

void main( int argc, char **argv )
{
	if( argc != 2 )
	{
		fprintf( stderr, "Usage: genrandomvectors <numvects>\n" );
		exit( -1 );
	}

	int nVectors = atoi( argv[1] );
	int i;
	RandomSeed( 0 );
	for( i = 0; i < nVectors; i++ )
	{
		float z = RandomFloat( -1.0f, 1.0f );
		float t = RandomFloat( 0, 2 * M_PI );
		float r = sqrt( 1.0f - z * z );
		float x = r * cos( t );
		float y = r * sin( t );
		printf( "Vector( %f, %f, %f ),\n", x, y, z );
	}
}
