//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Expose the IVEngineServer implementation to engine code.
//
// $NoKeywords: $
//=============================================================================//

#ifndef VENGINESERVER_IMPL_H
#define VENGINESERVER_IMPL_H

#include "eiface.h"

// The engine can call its own exposed functions in here rather than 
// splitting them into naked functions and sharing.
extern IVEngineServer *g_pVEngineServer;

// Used to seed the random # stream
void SeedRandomNumberGenerator( bool random_invariant );

void InvalidateSharedEdictChangeInfos();


#endif // VENGINESERVER_IMPL_H

