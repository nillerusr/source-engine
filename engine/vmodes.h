//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef VMODES_H
#define VMODES_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct viddef_t
{
	unsigned int	width;		
	unsigned int	height;
	int				recalc_refdef;	// if non-zero, recalc vid-based stuff
	int				bits;
};


#endif //VMODES_H
