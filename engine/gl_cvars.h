//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef GL_CVARS_H
#define GL_CVARS_H

#ifdef _WIN32
#pragma once
#endif

#include "convar.h"
#include "cmd.h"

// Stuff that's dealt with by the material system
extern	ConVar	mat_wireframe;		// Draw the world in wireframe mode
extern	ConVar	mat_normals;		// Draw the world with vertex normals
extern	ConVar	mat_luxels;			// Draw lightmaps as checkerboards
extern  ConVar	mat_loadtextures;	// Can help load levels quickly for debugging.
extern	ConVar	mat_bumpbasis;		// Draw the world with the bump basis vectors drawn
extern	ConVar	mat_envmapsize;		// Dimensions of square skybox bitmap (in 3D screen shots, not game textures)
extern  ConVar  mat_envmaptgasize;
extern  ConVar  mat_levelflush;
extern	ConVar	mat_hdr_level;

static inline bool CanCheat()
{
	extern	ConVar	sv_cheats;
	extern	ConVar	cl_debug_respect_cheat_vars;

#ifdef _DEBUG
	bool bRespectCheatVars = cl_debug_respect_cheat_vars.GetBool() && !Cmd_IsRptActive();
	if ( bRespectCheatVars )
		return sv_cheats.GetBool();
	return true;
#else
	return sv_cheats.GetBool();
	// johns 07/10/2015 - Disabled -- RPT's conditions for activating are not sufficient for this to be reasonable -
	//                    though it requires the remote client have valve credentials to issue commands, it can be set
	//                    to 'active' on demand by clients, and thus needs to be reworked before it can be allowed to
	//                    bypass cheats.
	// return ( sv_cheats.GetBool() || Cmd_IsRptActive() );
#endif
}

static inline int WireFrameMode( void )
{
	if ( CanCheat() )
		return mat_wireframe.GetInt();
	return 0;
}

static inline bool ShouldDrawInWireFrameMode( void )
{
	if ( CanCheat() )
		return ( mat_wireframe.GetInt() != 0 );
	return false;
}

extern	ConVar	r_drawbrushmodels;

#endif	//GL_CVARS_H
