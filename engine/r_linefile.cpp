//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "render_pch.h"
#include "draw.h"
#include "server.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"
#include "tier2/renderutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CUtlVector< Vector >	g_Points;
//-----------------------------------------------------------------------------
// Purpose: Draw the currently loaded line file
// Input  : g_Points - list of points
//-----------------------------------------------------------------------------
void Linefile_Draw( void )
{
	Vector *points = g_Points.Base();
	int pointCount = g_Points.Size();

	for ( int i = 0; i < pointCount-1; i++ )
	{
		RenderLine( points[i], points[i+1], Color( 255, 255, 0, 255 ), true );
	}
}


//-----------------------------------------------------------------------------
// Purpose: parse the map.lin file from disk
//			this file contains a list of line segments illustrating a leak in
//			the map
//-----------------------------------------------------------------------------
void Linefile_Read_f( void )
{
	Vector	org;
	int		r;
	int		c;
	char	name[MAX_OSPATH];

	g_Points.Purge();

	Q_snprintf( name, sizeof( name ), "maps/%s.lin", sv.GetMapName() );

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFileSystem->ReadFile( name, NULL, buf ) )
	{
		ConMsg ("couldn't open %s\n", name);
		return;
	}
	
	ConMsg ("Reading %s...\n", name);
	c = 0;

	for ( ;; )
	{
		r = buf.Scanf ("%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;
		
		g_Points.AddToTail( org );
	}

	ConMsg ("%i lines read\n", c);
}
