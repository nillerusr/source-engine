//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A set of utilities to render standard shapes
//
//===========================================================================//

#include "tier2/meshutils.h"


//-----------------------------------------------------------------------------
// Helper methods to create various standard index buffer types
//-----------------------------------------------------------------------------
void GenerateSequentialIndexBuffer( unsigned short* pIndices, int nIndexCount, int nFirstVertex )
{
	if ( !pIndices )
		return;

	// Format the sequential buffer
	for ( int i = 0; i < nIndexCount; ++i )
	{
		pIndices[i] = (unsigned short)( i + nFirstVertex );
	}
}

void GenerateQuadIndexBuffer( unsigned short* pIndices, int nIndexCount, int nFirstVertex )
{
	if ( !pIndices )
		return;

	// Format the quad buffer
	int i;
	int numQuads = nIndexCount / 6;
	int baseVertex = nFirstVertex;
	for ( i = 0; i < numQuads; ++i)
	{
		// Triangle 1
		pIndices[0] = (unsigned short)( baseVertex );
		pIndices[1] = (unsigned short)( baseVertex + 1 );
		pIndices[2] = (unsigned short)( baseVertex + 2 );

		// Triangle 2
		pIndices[3] = (unsigned short)( baseVertex );
		pIndices[4] = (unsigned short)( baseVertex + 2 );
		pIndices[5] = (unsigned short)( baseVertex + 3 );

		baseVertex += 4;
		pIndices += 6;
	}
}

void GeneratePolygonIndexBuffer( unsigned short* pIndices, int nIndexCount, int nFirstVertex )
{
	if ( !pIndices )
		return;

	int i;
	int numPolygons = nIndexCount / 3;
	for ( i = 0; i < numPolygons; ++i)
	{
		// Triangle 1
		pIndices[0] = (unsigned short)( nFirstVertex );
		pIndices[1] = (unsigned short)( nFirstVertex + i + 1 );
		pIndices[2] = (unsigned short)( nFirstVertex + i + 2 );
		pIndices += 3;
	}
}


void GenerateLineStripIndexBuffer( unsigned short* pIndices, int nIndexCount, int nFirstVertex )
{
	if ( !pIndices )
		return;

	int i;
	int numLines = nIndexCount / 2;
	for ( i = 0; i < numLines; ++i)
	{
		pIndices[0] = (unsigned short)( nFirstVertex + i );
		pIndices[1] = (unsigned short)( nFirstVertex + i + 1 );
		pIndices += 2;
	}
}

void GenerateLineLoopIndexBuffer( unsigned short* pIndices, int nIndexCount, int nFirstVertex )
{
	if ( !pIndices )
	{
		return;
	}

	int i;
	int numLines = nIndexCount / 2;

	pIndices[0] = (unsigned short)( nFirstVertex + numLines - 1 );
	pIndices[1] = (unsigned short)( nFirstVertex );
	pIndices += 2;

	for ( i = 1; i < numLines; ++i)
	{
		pIndices[0] = (unsigned short)( nFirstVertex + i - 1 );
		pIndices[1] = (unsigned short)( nFirstVertex + i );
		pIndices += 2;
	}
}
