//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef FOUNDRYHELPERS_SERVER_H
#define FOUNDRYHELPERS_SERVER_H
#ifdef _WIN32
#pragma once
#endif


#include "foundry/iserverfoundry.h"
#include "enginecallback.h"


void FoundryHelpers_ClearEntityHighlightEffects();
void FoundryHelpers_AddEntityHighlightEffect( CBaseEntity *pEnt );


#endif // FOUNDRYHELPERS_SERVER_H
