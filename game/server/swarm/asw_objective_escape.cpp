#include "cbase.h"
#include "asw_objective_escape.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "asw_marine.h"
#include "triggers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_objective_escape, CASW_Objective_Escape );

BEGIN_DATADESC( CASW_Objective_Escape )
	DEFINE_FIELD( m_hTrigger, FIELD_EHANDLE ),
	DEFINE_INPUTFUNC( FIELD_VOID, "MarineInEscapeArea", InputMarineInEscapeArea ),
END_DATADESC()

CUtlVector<CASW_Objective_Escape*> g_aEscapeObjectives;


// dummy objectives are complete automatically
CASW_Objective_Escape::CASW_Objective_Escape() : CASW_Objective()
{
	m_hTrigger = NULL;
	g_aEscapeObjectives.AddToTail( this );
}


CASW_Objective_Escape::~CASW_Objective_Escape()
{
	g_aEscapeObjectives.FindAndRemove( this );
}

void CASW_Objective_Escape::InputMarineInEscapeArea( inputdata_t &inputdata )
{
	CBaseTrigger* pTrig = dynamic_cast<CBaseTrigger*>(inputdata.pCaller);
	if (!pTrig)
	{
		Msg("Error: Escape objective input called by something that wasn't a trigger\n");
		return;
	}
	if (pTrig != GetTrigger() && GetTrigger()!= NULL)
	{
		Msg("Error: Escape objective input called by two different triggers.  Only 1 escape area is allowed per map.\n");
		return;
	}
	m_hTrigger = pTrig;

	CheckEscapeStatus();
}

void CASW_Objective_Escape::CheckEscapeStatus()
{
	if (OtherObjectivesComplete() && AllLiveMarinesInExit())
		SetComplete(true);
}

// are all other non-optional objectives complete?
bool CASW_Objective_Escape::OtherObjectivesComplete()
{
	if ( !ASWGameResource() )
		return false;
	
	CASW_Game_Resource* pGameResource = ASWGameResource();
	for (int i=0;i<12;i++)
	{
		CASW_Objective* pObjective = pGameResource->GetObjective(i);
		// if another objective isn't optional and isn't complete, then we're not ready to escape
		if (pObjective && pObjective!=this
			&& !pObjective->IsObjectiveOptional() && !pObjective->IsObjectiveComplete())
		{
			return false;
		}
	}
	return true;
}

bool CASW_Objective_Escape::AllLiveMarinesInExit()
{
	if ( !ASWGameResource() ||!GetTrigger() )
		return false;
	
	CASW_Game_Resource* pGameResource = ASWGameResource();
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
		if (pMR && pMR->GetHealthPercent() > 0
			&& pMR->GetMarineEntity())
		{
			// we've got a live marine, check if he's in the exit area
			if (!GetTrigger()->IsTouching(pMR->GetMarineEntity()))
				return false;	// a live marine isn't in the exit zone
		}
	}
	return true;
}

CBaseTrigger* CASW_Objective_Escape::GetTrigger()
{
	return dynamic_cast<CBaseTrigger*>(m_hTrigger.Get());
}