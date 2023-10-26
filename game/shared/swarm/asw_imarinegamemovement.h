//======== (C) Copyright 1999, 2000 Valve, L.L.C. All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================

#if !defined( IMARINEGAMEMOVEMENT_H )
#define IMARINEGAMEMOVEMENT_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"
#include "interface.h"
#include "imovehelper.h"
#include "const.h"
#include "igamemovement.h"

//-----------------------------------------------------------------------------
// Name of the class implementing the game movement.
//-----------------------------------------------------------------------------

#define INTERFACENAME_MARINEGAMEMOVEMENT	"MarineGameMovement001"

//-----------------------------------------------------------------------------
// Purpose: The basic marine movement interface
//-----------------------------------------------------------------------------

class IMarineGameMovement
{
public:
	virtual			~IMarineGameMovement( void ) {}
	
	// Process the current movement command
	virtual void	ProcessMovement( CBasePlayer *pPlayer, CBaseEntity *pMarine, CMoveData *pMove ) = 0;		

	// Allows other parts of the engine to find out the normal and ducked player bbox sizes
	virtual Vector const&	GetPlayerMins( bool ducked ) const = 0;
	virtual Vector const&	GetPlayerMaxs( bool ducked ) const = 0;
	virtual Vector const&   GetPlayerViewOffset( bool ducked ) const = 0;
};


#endif // IMARINEGAMEMOVEMENT_H
