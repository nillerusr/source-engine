#include "cbase.h"
#include "asw_objective.h"
#include "asw_game_resource.h"
#include "asw_mission_manager.h"
#include "asw_gamerules.h"
#include "asw_marine_resource.h"
#include "asw_marine_speech.h"
#include "asw_marine.h"
#include "asw_objective_escape.h"
#include "game_timescale_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_mission_manager, CASW_Mission_Manager );


BEGIN_DATADESC( CASW_Mission_Manager )
	
END_DATADESC()

ConVar asw_last_marine_dead_delay( "asw_last_marine_dead_delay", "0.6f", FCVAR_CHEAT, "How long to wait after the last marine has died before failing the mission" );

CASW_Mission_Manager::CASW_Mission_Manager()
{
	m_bDoneLeavingChatter = false;
	m_flLastMarineDeathTime = 0.0f;
	m_bAllMarinesDead = false;
}


CASW_Mission_Manager::~CASW_Mission_Manager()
{
}

void CASW_Mission_Manager::Spawn()
{
	BaseClass::Spawn();

	SetThink( &CASW_Mission_Manager::ManagerThink );
	SetNextThink( gpGlobals->curtime + 1.0f );
}

void CASW_Mission_Manager::ManagerThink()
{
	if ( m_bAllMarinesDead )	// once all marines are dead, we need to check for mission complete periodically to see if the marine_dead_delay time has passed
	{
		CheckMissionComplete();
	}
	SetNextThink( gpGlobals->curtime + 1.0f );
}

bool CASW_Mission_Manager::CheckMissionComplete()
{
	bool bFailed = false;
	bool bSuccess = true;
	bool bAtLeastOneObjective = false;

	// notify all objectives about this event
	if ( !ASWGameResource() )
		return false;

	int iIncomplete = 0;
	int iNumObjectives = 0;
	bool bEscapeIncomplete = false;
	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		CASW_Objective* pObjective = ASWGameResource()->GetObjective(i);
		if (pObjective)
		{
			bAtLeastOneObjective = true;
			iNumObjectives++;
			if (!pObjective->IsObjectiveComplete() && !pObjective->IsObjectiveOptional())
			{
				bSuccess = false;
				iIncomplete++;
				if (iIncomplete == 1 && !m_bDoneLeavingChatter)
				{
					CASW_Objective_Escape *pEscape = dynamic_cast<CASW_Objective_Escape*>(pObjective);
					if (pEscape)
						bEscapeIncomplete = true;
				}
			}
			if (pObjective->IsObjectiveFailed())
			{
				bFailed = true;					
			}
		}
	}

	m_bAllMarinesDead = AllMarinesDead();
	if ( m_bAllMarinesDead && ( gpGlobals->curtime > m_flLastMarineDeathTime + asw_last_marine_dead_delay.GetFloat() || GameTimescale()->GetCurrentTimescale() < 1.0f ) )
	{
		bFailed = true;
	}
	

	if (bSuccess && bAtLeastOneObjective)
	{
		MissionSuccess();
		return true;
	}
	else if (bFailed)
	{
		MissionFail();
		return true;
	}
	else
	{
		if (bEscapeIncomplete && iIncomplete)
		{
			// make a marine do the 'time to leave' speech
			if ( ASWGameResource() )
			{
				CASW_Game_Resource *pGameResource = ASWGameResource();
				// count how many live marines we have
				int iNearby = 0;						
				for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
				{
					CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
					CASW_Marine *pMarine = pMR ? pMR->GetMarineEntity() : NULL;
					if (pMarine && pMarine->GetHealth() > 0)
								iNearby++;
				}
				int iChatter = random->RandomInt(0, iNearby-1);
				for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
				{
					CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
					CASW_Marine *pMarine = pMR ? pMR->GetMarineEntity() : NULL;
					if (pMarine && pMarine->GetHealth() > 0)
					{
						if (iChatter <= 0)
						{
							pMarine->GetMarineSpeech()->QueueChatter(CHATTER_TIME_TO_LEAVE, gpGlobals->curtime + 3.0f, gpGlobals->curtime + 6.0f);
							break;
						}
						iChatter--;
					}
				}						
			}
			m_bDoneLeavingChatter = true;
		}
	}
	return false;
}

void CASW_Mission_Manager::AlienKilled(CBaseEntity* pAlien)
{
	// notify all objectives about this event
	if ( !ASWGameResource() )
		return;

	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		CASW_Objective* pObjective = ASWGameResource()->GetObjective(i);
		if (pObjective)
			pObjective->AlienKilled(pAlien);
	}
}

void CASW_Mission_Manager::MarineKilled(CASW_Marine* pMarine)
{
	m_flLastMarineDeathTime = gpGlobals->curtime;

	// notify all objectives about this event
	if ( !ASWGameResource() )
		return;

	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		CASW_Objective* pObjective = ASWGameResource()->GetObjective(i);
		if (pObjective)
			pObjective->MarineKilled(pMarine);
	}

	// check if our mission is over (from all the marines dying)
	CheckMissionComplete();
}

void CASW_Mission_Manager::EggKilled(CASW_Egg* pEgg)
{
	// notify all objectives about this event
	if ( !ASWGameResource() )
		return;

	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		CASW_Objective* pObjective = ASWGameResource()->GetObjective(i);
		if (pObjective)
			pObjective->EggKilled(pEgg);
	}
}

void CASW_Mission_Manager::GooKilled(CASW_Alien_Goo* pGoo)
{
	// notify all objectives about this event
	if ( !ASWGameResource() )
		return;

	//Msg("Mission manager sees Alien Goo killed!\n");

	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		CASW_Objective* pObjective = ASWGameResource()->GetObjective(i);
		if (pObjective)
			pObjective->GooKilled(pGoo);
	}
}

void CASW_Mission_Manager::MissionStarted(void)
{
	// notify all objectives about this event
	if ( !ASWGameResource() )
		return;

	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		CASW_Objective* pObjective = ASWGameResource()->GetObjective(i);
		if (pObjective)
			pObjective->MissionStarted();
	}
}

// marines screwed up
void CASW_Mission_Manager::MissionFail(void)
{
	// notify all objectives about this event
	if (!ASWGameRules() || !ASWGameResource()
		|| ASWGameRules()->GetGameState() == ASW_GS_DEBRIEF)
		return;

	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		CASW_Objective* pObjective = ASWGameResource()->GetObjective(i);
		if (pObjective)
			pObjective->MissionFail();
	}
	//  tell the game rules the mission is over
	ASWGameRules()->MissionComplete(false);
}

// marines have succeeded in finishing the mission! yay
void CASW_Mission_Manager::MissionSuccess(void)
{
	// notify all objectives about this event
	if (!ASWGameRules() || !ASWGameResource()
		|| ASWGameRules()->GetGameState() == ASW_GS_DEBRIEF)
		return;

	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		CASW_Objective* pObjective = ASWGameResource()->GetObjective(i);
		if (pObjective)
			pObjective->MissionSuccess();
	}
	//  tell the game rules the mission is over
	ASWGameRules()->MissionComplete(true);
}


bool CASW_Mission_Manager::AllMarinesDead()
{
	if (!ASWGameRules() || !ASWGameResource())
		return false;

	// can't be all dead if we haven't left briefing yet!
	if (ASWGameRules()->GetGameState() < ASW_GS_INGAME)
		return false;

	int iMax = ASWGameResource()->GetMaxMarineResources();
	for (int i=0;i<iMax;i++)
	{
		CASW_Marine_Resource *pMarineResource = ASWGameResource()->GetMarineResource(i);
		if (pMarineResource && pMarineResource->GetHealthPercent() > 0)
		{
			return false;	// we have a live marine, so they're not all dead
		}
	}
	return true;  // arg all dead!
}

void CASW_Mission_Manager::CheatCompleteMission()
{
	Msg("CheatCompleteMission\n");
	// notify all objectives about this event
	if ( !ASWGameResource() )
		return;

	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		CASW_Objective* pObjective = ASWGameResource()->GetObjective(i);
		if (pObjective)
		{
			pObjective->SetComplete(true);
		}
	}
	CheckMissionComplete();
}

CASW_Objective_Escape* CASW_Mission_Manager::GetEscapeObjective()
{
	if ( m_hEscapeObjective.Get() )
		return m_hEscapeObjective.Get();

	for ( int i=0; i < ASW_MAX_OBJECTIVES; i++ )  // 12 is max number of objectives 
	{
		CASW_Objective_Escape *pEscape = dynamic_cast<CASW_Objective_Escape*>( ASWGameResource()->GetObjective( i ) );
		if ( pEscape )
		{
			m_hEscapeObjective = pEscape;
			return pEscape;
		}
	}
	return NULL;
}