#include "cbase.h"
#include "asw_scanner_info.h"
#include "asw_scanner_objects_enumerator.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "asw_marine.h"
#include "asw_door.h"
#include <coordsize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_scanner_info, CASW_Scanner_Info );

IMPLEMENT_SERVERCLASS_ST(CASW_Scanner_Info, DT_ASW_Scanner_Info)	
	SendPropArray3	( SENDINFO_ARRAY3(m_iBlipX), SendPropInt( SENDINFO_ARRAY(m_iBlipX), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iBlipY), SendPropInt( SENDINFO_ARRAY(m_iBlipY), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_index), SendPropInt( SENDINFO_ARRAY(m_index) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_BlipType), SendPropInt( SENDINFO_ARRAY(m_BlipType), 2, SPROP_UNSIGNED ) ),	
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Scanner_Info )
	DEFINE_FUNCTION( UpdateBlips ),
END_DATADESC()

CASW_Scanner_Info::CASW_Scanner_Info()
{
	for (int i=0;i<ASW_SCANNER_MAX_BLIPS;i++)
	{
		m_iBlipX.Set(i, 0);
		m_iBlipY.Set(i, 0);
		m_index.Set(i, 0);
		m_BlipType.Set(i, 0);
	}
}

CASW_Scanner_Info::~CASW_Scanner_Info()
{
	
}

void CASW_Scanner_Info::Spawn()
{
	BaseClass::Spawn();
	SetThink( &CASW_Scanner_Info::UpdateBlips );
	SetNextThink( gpGlobals->curtime + 1.0f );
}

// always send this info to players
int CASW_Scanner_Info::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

// fill in the array with blip positions
void CASW_Scanner_Info::UpdateBlips()
{
	// flag all blips as inactive for now
	for (int i=0;i<ASW_SCANNER_MAX_BLIPS;i++)
	{
		m_bActiveBlip[i] = false;
	}
	// add blips around each tech marine
	if ( ASWGameResource() )
	{
		CASW_Game_Resource* pGameResource = ASWGameResource();
		for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
		{
			CASW_Marine_Resource *pMarineResource = pGameResource->GetMarineResource(i);
			if (pMarineResource && pMarineResource->GetMarineEntity() && pMarineResource->GetHealthPercent() > 0)
			{
				if (pMarineResource->GetProfile() && pMarineResource->GetProfile()->CanScanner())
				{
					float fScannerRange = MarineSkills()->GetSkillBasedValueByMarineResource(pMarineResource, ASW_MARINE_SKILL_SCANNER);
					AddBlips(pMarineResource->GetMarineEntity()->GetAbsOrigin(), fScannerRange);
				}
			}
		}
	}
	
	// remove inactive blips
	for (int i=0;i<ASW_SCANNER_MAX_BLIPS;i++)
	{
		if (!m_bActiveBlip[i])
		{
			m_iBlipX.Set(i, 0);
			m_iBlipY.Set(i, 0);
			m_index.Set(i, 0);
			m_BlipType.Set(i, 0);
		}
	}
	SetNextThink( gpGlobals->curtime + 1.0f );
}

void CASW_Scanner_Info::AddBlips(Vector vecScannerCenter, float fDist)
{
	// enumerate all aliens/moving doors within the radius
	// for each one, set the blip x + y and index to the entindex
	CASW_Scanner_Objects_Enumerator BlipEntities( fDist, vecScannerCenter );
	partition->EnumerateElementsInSphere( ASW_PARTITION_ALL_SERVER_EDICTS , vecScannerCenter, fDist, false, &BlipEntities );

	int c = BlipEntities.GetObjectCount();
	for (int i=0;i<c;i++)
	{
		CBaseEntity *pEnt = BlipEntities.GetObject(i);
		if (pEnt)
		{
			// check if this entity has an entry already, otherwise use the first free slot
			int slot = -1;
			for (int k=ASW_SCANNER_MAX_BLIPS-1;k>=0;k--)
			{
				if (m_index.Get(k) == pEnt->entindex())
				{
					slot = k;
					break;
				}
				else if (m_index.Get(k) == 0)
				{
					slot = k;	// empty slot
				}
			}
			// set type depending on alien/door/useable item
			int iType = 0;
			if (pEnt->Classify() == CLASS_ASW_BUTTON_PANEL || pEnt->Classify() == CLASS_ASW_COMPUTER_AREA )	// computer/button area
				iType = 1;
			else if (pEnt->Classify() == CLASS_ASW_DOOR)		// door
			{
				iType = 2;
				CASW_Door *pDoor = dynamic_cast<CASW_Door*>(pEnt);
				if (pDoor && !pDoor->m_bShowsOnScanner)	// skip doors that don't want to be on the scanner
					continue;
			}
			if (slot != -1)
			{
				m_iBlipX.Set(slot, (int) pEnt->WorldSpaceCenter().x);
				m_iBlipY.Set(slot, (int) pEnt->WorldSpaceCenter().y);
				m_index.Set(slot, pEnt->entindex());				
				m_BlipType.Set(slot, iType);
				m_bActiveBlip[slot] = true;
			}
		}
	}
}