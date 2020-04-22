//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "disp_defs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ---------------------------------------------------------------------- //
// Global tables.
// ---------------------------------------------------------------------- //

// These map CCoreDispSurface neighbor orientations (which are actually edge indices)
// into our 'degrees of rotation' representation.
int g_CoreDispNeighborOrientationMap[4][4] =
{
	{ORIENTATION_CCW_180, ORIENTATION_CCW_270, ORIENTATION_CCW_0,   ORIENTATION_CCW_90},
	{ORIENTATION_CCW_90,  ORIENTATION_CCW_180, ORIENTATION_CCW_270, ORIENTATION_CCW_0},
	{ORIENTATION_CCW_0,   ORIENTATION_CCW_90,  ORIENTATION_CCW_180, ORIENTATION_CCW_270},
	{ORIENTATION_CCW_270, ORIENTATION_CCW_0,   ORIENTATION_CCW_90,  ORIENTATION_CCW_180}
};



// ---------------------------------------------------------------------- //
// Global variables.
// ---------------------------------------------------------------------- //

CUtlVector<unsigned char>	g_DispLMAlpha;
CUtlVector<unsigned char, CHunkMemory<unsigned char> >	g_DispLightmapSamplePositions;
CUtlVector<CDispGroup*>		g_DispGroups;

bool						g_bDispOrthoRender = false;

ConVar						r_DrawDisp( "r_DrawDisp", "1", FCVAR_CHEAT, "Toggles rendering of displacment maps" );
ConVar						r_DispWalkable( "r_DispWalkable", "0", FCVAR_CHEAT );
ConVar						r_DispBuildable( "r_DispBuildable", "0", FCVAR_CHEAT );
