//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_OBJECTIVE_RESOURCE_H
#define C_TF_OBJECTIVE_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_shareddefs.h"
#include "const.h"
#include "c_baseentity.h"
#include <igameresources.h>
#include "c_team_objectiveresource.h"

class C_TFObjectiveResource : public C_BaseTeamObjectiveResource
{
	DECLARE_CLASS( C_TFObjectiveResource, C_BaseTeamObjectiveResource );
public:
	DECLARE_CLIENTCLASS();

					C_TFObjectiveResource();
	virtual			~C_TFObjectiveResource();

	const char		*GetGameSpecificCPCappingSwipe( int index, int iCappingTeam );
	const char		*GetGameSpecificCPBarFG( int index, int iOwningTeam );
	const char		*GetGameSpecificCPBarBG( int index, int iCappingTeam );
	void			SetCappingTeam( int index, int team );

};

inline C_TFObjectiveResource *TFObjectiveResource()
{
	return static_cast<C_TFObjectiveResource*>(g_pObjectiveResource);
}

#endif // C_TF_OBJECTIVE_RESOURCE_H
