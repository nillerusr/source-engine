//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	MUST BE THE FIRST MODULE IN THE LINK PROCESS TO ACHIEVE @1
//
//	This is a 360 specific trick to force this import library and the new 360
//	link option /AUTODEF to put CreateInterface at @1 (360 lacks named exports) and
//	first in sequence.  Otherwise, the valve interface techique that does a
//	GetProcAddress( @1 ) gets the wrong function pointer. All other exported
//	functions can appear in any order, but the oridnals should be autogened sequential.
//===========================================================================//

#include "tier1/tier1.h"

// Should be the first function that the linker 'sees' as an export
void* CreateInterfaceThunk( const char *pName, int *pReturnCode )
{
	// descend into the real function
	return CreateInterface( pName, pReturnCode );
}
