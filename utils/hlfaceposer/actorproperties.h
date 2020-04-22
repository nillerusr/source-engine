//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ACTORPROPERTIES_H
#define ACTORPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CActorParams : public CBaseDialogParams
{
	// i/o actor name
	char		m_szName[ 256 ];
};

// Display/create actor info
int ActorProperties( CActorParams *params );

#endif // ACTORPROPERTIES_H
