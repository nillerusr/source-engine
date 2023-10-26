#include "cbase.h"
#include "asw_objective_survive.h"
#include "asw_gamerules.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "asw_marine.h"
#include "triggers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_objective_survive, CASW_Objective_Survive );

BEGIN_DATADESC( CASW_Objective_Survive )
	
END_DATADESC()


CASW_Objective_Survive::CASW_Objective_Survive() : CASW_Objective()
{
}


CASW_Objective_Survive::~CASW_Objective_Survive()
{
}

void CASW_Objective_Survive::Spawn()
{
	BaseClass::Spawn();

	m_bOptional = true;
}

void CASW_Objective_Survive::MarineKilled(CASW_Marine* pMarine)
{
	if (IsObjectiveComplete() || IsObjectiveFailed())
		return;

	if (pMarine)
	{
		SetFailed(true);
	}
}