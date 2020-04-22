//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	COMMON.CPP
//
//	Common/Misc specialized support routines not uniquely owned.
//=====================================================================================//
#include "vxconsole.h"

vprofState_e	g_vprof_state = VPROF_OFF;

//-----------------------------------------------------------------------------
//	NotImplementedYet
//
//-----------------------------------------------------------------------------
void NotImplementedYet()
{
	Sys_MessageBox( "Attention!", "Sorry, Not Implemented Yet." );
}

//-----------------------------------------------------------------------------
//	VProf_GetState
//
//-----------------------------------------------------------------------------
vprofState_e VProf_GetState()
{
	return g_vprof_state;
}

//-----------------------------------------------------------------------------
//	VProf_Enable
//
//	Coordinates multiple vprof commands for proper exclusion
//-----------------------------------------------------------------------------
void VProf_Enable( vprofState_e state )
{
	char	commandString[256];

	switch ( state )
	{
		case VPROF_CPU:
			strcpy( commandString, "vprof_off ; vprof_on ; vprof_update cpu" );
			break;

		case VPROF_TEXTURE:
			strcpy( commandString, "vprof_off ; vprof_on ; vprof_update texture" );
			break;

		case VPROF_TEXTUREFRAME:
			strcpy( commandString, "vprof_off ; vprof_on ; vprof_update texture_frame" );
			break;

		default:
			state = VPROF_OFF;
			strcpy( commandString, "vprof_off" );
			break;

	}

	// track state
	if ( g_vprof_state != state )
	{
		g_vprof_state = state;

		// do command
		ProcessCommand( commandString );

		// update all the dependant titles
		CpuProfile_SetTitle();
		TexProfile_SetTitle();
	}
}


