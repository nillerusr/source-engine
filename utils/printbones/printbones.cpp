//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdio.h>
#include <string.h>
#include "studio.h"

int g_NumBonesInLOD[MAX_NUM_LODS];

void Usage( void )
{
	fprintf( stderr, "Usage: printbones blah.mdl\n" );
	exit( -1 );
}

void PrintSpaces( int numSpaces )
{
	int i;
	for( i = 0; i < numSpaces; i++ )
	{
		printf( "  " );
	}
}

void PrintBoneGraphRecursive( studiohdr_t *phdr, int boneIndex, int level )
{
	int j;
	PrintSpaces( level );
	printf( "%d \"%s\" ", boneIndex, phdr->pBone( boneIndex )->pszName() );
	int i;
	for( i = 0; i < 8; i++ )
	{
		if( phdr->pBone( boneIndex )->flags & ( BONE_USED_BY_VERTEX_LOD0 << i ) )
		{
			printf( "lod%d ", i );
			g_NumBonesInLOD[i]++;
		}
	}
	printf( "\n" );
	for( j = 0; j < phdr->numbones; j++ )
	{
		mstudiobone_t *pBone = phdr->pBone( j );
		if( pBone->parent == boneIndex )
		{
			PrintBoneGraphRecursive( phdr, j, level+1 );
		}
	}
}

int main( int argc, char **argv )
{
	if( argc != 2 )
	{
		Usage();
	}

	memset( g_NumBonesInLOD, 0, sizeof( int ) * MAX_NUM_LODS );
	
	FILE *fp;
	fp = fopen( argv[1], "rb" );
	if( !fp )
	{
		fprintf( stderr, "Can't open: %s\n", argv[1] );
		Usage();
	}
	fseek( fp, 0, SEEK_END );
	int len = ftell( fp );
	rewind( fp );

	studiohdr_t *pStudioHdr = ( studiohdr_t * )malloc( len );
	fread( pStudioHdr, 1, len, fp );
	Studio_ConvertStudioHdrToNewVersion( pStudioHdr );
	if( pStudioHdr->version != STUDIO_VERSION )
	{
		fprintf( stderr, "Wrong version (%d != %d)\n", ( int )pStudioHdr->version, ( int )STUDIO_VERSION );
		Usage();
	}

	int i;
	for( i = 0; i < pStudioHdr->numbones; i++ )
	{
		mstudiobone_t *pBone = pStudioHdr->pBone( i );
		if( pBone->parent == -1 )
		{
			PrintBoneGraphRecursive( pStudioHdr, i, 0 );
		}
	}
	
	fclose( fp );

	for( i = 0; i < MAX_NUM_LODS; i++ )
	{
		printf( "%d bones used in lod %d\n", g_NumBonesInLOD[i], i );
	}

	return 0;
}