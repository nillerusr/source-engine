//===== Copyright © 2005-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//

#include <tier1/tier1.h>
#include "tier0/dbg.h"
#include "interfaces/interfaces.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"




//-----------------------------------------------------------------------------
// Call this to connect to all tier 1 libraries.
// It's up to the caller to check the globals it cares about to see if ones are missing
//-----------------------------------------------------------------------------
void ConnectTier1Libraries( CreateInterfaceFn *pFactoryList, int nFactoryCount )
{
	ConnectInterfaces( pFactoryList, nFactoryCount );
}

void DisconnectTier1Libraries()
{
	DisconnectInterfaces();
}
