//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef STUDIO_RENDER_H
#define STUDIO_RENDER_H
#ifdef _WIN32
#pragma once
#endif


extern Vector	g_viewtarget;

extern Vector	g_flexedverts[];
extern Vector	g_flexednorms[];
extern int		g_flexages[];

extern DrawModelInfo_t g_DrawModelInfo;
extern DrawModelResults_t g_DrawModelResults;
extern bool g_bDrawModelInfoValid;
	

#endif // STUDIO_RENDER_H
