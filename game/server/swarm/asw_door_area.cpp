#include "cbase.h"
#include "asw_door_area.h"
#include "asw_door.h"
#include "asw_marine.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "asw_weapon_welder_shared.h"
#include "asw_marine_speech.h"
#include "asw_fail_advice.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( trigger_asw_door_area, CASW_Door_Area );


BEGIN_DATADESC( CASW_Door_Area )
	
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CASW_Door_Area, DT_ASW_Door_Area)	

END_SEND_TABLE()

CASW_Door_Area::CASW_Door_Area()
{	
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	m_fNextCutCheck = 0;
}

bool CASW_Door_Area::HasWelder(CASW_Marine *pMarine)
{
	CASW_Weapon_Welder *pWelder = dynamic_cast<CASW_Weapon_Welder*>(pMarine->GetASWWeapon(2));
	if (pWelder)
		return true;

	return false;
}

void CASW_Door_Area::ActivateMultiTrigger(CBaseEntity *pActivator)
{
	if (GetNextThink() > gpGlobals->curtime)
		return;         // still waiting for reset time

	BaseClass::ActivateMultiTrigger(pActivator);
	
	CASW_Door* pDoor = GetASWDoor();
	if (!pDoor)
		return;

	// check for shouting out about a sealed door
	if (pDoor->m_bDoCutShout)
	{
		if (gpGlobals->curtime > m_fNextCutCheck)
		{
			CASW_Marine *pMarine = CASW_Marine::AsMarine( pActivator );
			if ( pMarine )
			{
				// check if there's another marine nearby with a welder
				CASW_Game_Resource *pGameResource = ASWGameResource();
				if (pGameResource)
				{
					bool bFound = false;
					for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
					{
						CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
						CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
						if (pOtherMarine && pActivator != pOtherMarine
									&& (pMarine->GetAbsOrigin().DistTo(pOtherMarine->GetAbsOrigin()) < 800)
									&& HasWelder(pOtherMarine) )
							bFound = true;
					}
					if (bFound)
					{
						if (pMarine->GetMarineSpeech()->Chatter(CHATTER_REQUEST_CUT_DOOR))
						{
							pDoor->m_bDoCutShout = false;
							if (pDoor->m_bDoAutoShootChatter)
								pDoor->m_bDoAutoShootChatter = false;	// don't need to automatic shout about shooting a door if we've already shouted about cutting it
						}
						m_fNextCutCheck = gpGlobals->curtime + 2.0f;	// check again sooner if we tried to say the line but couldn't for some reason
					}
					else
					{
						m_fNextCutCheck = gpGlobals->curtime + 10.0f;
					}
				}				
			}
		}
	}
	if (pDoor->m_bDoAutoShootChatter)
	{
		CASW_Marine *pMarine = CASW_Marine::AsMarine( pActivator );
		if (pMarine)
		{
			pDoor->DoAutoDoorShootChatter(pMarine);
		}
	}

	if ( !RequirementsMet( pActivator ) )
	{
		return;
	}

	if ( pDoor->IsAutoOpen() )
	{
		pDoor->AutoOpen(pActivator);

		if ( pDoor->CanWeld() )
		{
			ASWFailAdvice()->OnAlienOpenDoor();
		}
	}
}

CASW_Door* CASW_Door_Area::GetASWDoor()
{
	return dynamic_cast<CASW_Door*>(m_hUseTarget.Get());
}
