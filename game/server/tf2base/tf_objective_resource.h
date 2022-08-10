//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: DOD's objective resource, transmits all objective states to players
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJECTIVE_RESOURCE_H
#define TF_OBJECTIVE_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_shareddefs.h"
#include "team_objectiveresource.h"

class CTFObjectiveResource : public CBaseTeamObjectiveResource
{
	DECLARE_CLASS( CTFObjectiveResource, CBaseTeamObjectiveResource );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
};

#endif	// TF_OBJECTIVE_RESOURCE_H

