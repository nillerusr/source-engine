#include "cbase.h"
#include "asw_marine_skills.h"
#include "collisionutils.h"
#ifdef CLIENT_DLL
	#include "c_asw_game_resource.h"
	#include "c_asw_marine_resource.h"
	#include "c_asw_marine.h"
	#include "c_playerresource.h"
	#include "c_asw_player.h"
	#include "engine/ivdebugoverlay.h"
	#define CASW_Game_Resource C_ASW_Game_Resource
	#define CASW_Marine_Resource C_ASW_Marine_Resource
	#define CASW_Player C_ASW_Player
	#define CASW_Marine C_ASW_Marine
#else
	#include "asw_marine_resource.h"
	#include "asw_marine.h"
	#include "asw_game_resource.h"
	#include "player_resource.h"
	#include "asw_player.h"
#endif

#include "asw_marine_profile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
extern ConVar asw_debug_clientside_avoidance;
#endif

int CASW_Game_Resource::GetSlotForSkill( int nProfileIndex, ASW_Skill nSkillIndex )
{
	if (nProfileIndex < 0 || nProfileIndex > ASW_NUM_MARINE_PROFILES)
		return -1;

	CASW_Marine_Profile *pProfile = MarineProfileList()->GetProfile( nProfileIndex );
	if ( !pProfile )
		return -1;

	for ( int i = 0; i < ASW_NUM_SKILL_SLOTS - 1; i++ )
	{
		if ( pProfile->GetSkillMapping( i ) == nSkillIndex )
			return i;
	}
	return -1;
}

int CASW_Game_Resource::GetMarineSkill( int nProfileIndex, int nSkillSlot )
{
	if (nProfileIndex < 0 || nProfileIndex > ASW_NUM_MARINE_PROFILES)
		return -1;

	switch ( nSkillSlot )
	{
		case ASW_SKILL_SLOT_0: return m_iSkillSlot0[ nProfileIndex ]; break;
		case ASW_SKILL_SLOT_1: return m_iSkillSlot1[ nProfileIndex ]; break;
		case ASW_SKILL_SLOT_2: return m_iSkillSlot2[ nProfileIndex ]; break;
		case ASW_SKILL_SLOT_3: return m_iSkillSlot3[ nProfileIndex ]; break;
		case ASW_SKILL_SLOT_4: return m_iSkillSlot4[ nProfileIndex ]; break;
		case ASW_SKILL_SLOT_SPARE: return m_iSkillSlotSpare[ nProfileIndex ]; break;
	}
	return -1;
}

int CASW_Game_Resource::GetMarineSkill( CASW_Marine_Resource *m, int nSkillSlot )
{
	return GetMarineSkill( m->GetProfileIndex(), nSkillSlot );
}

bool CASW_Game_Resource::AreAllOtherPlayersReady(int iPlayerEntIndex)
{
	// drop it down one since player ent indices start at 1, instead of 0
	iPlayerEntIndex--;

	for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
	{
		// found a connected player who isn't ready?
#ifdef CLIENT_DLL
		if (g_PR->IsConnected(i+1) && !m_bPlayerReady[i] && i!=iPlayerEntIndex)
			return false;
#else
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i + 1);
		// if they're not connected, skip them
		if (!pPlayer || !pPlayer->IsConnected())
		{
			continue;
		}

		// if they're not ready and not the leader checking
		if (!m_bPlayerReady[i] && i!=iPlayerEntIndex)
		{
			return false;
		}
#endif
	}
	return true;
}

bool CASW_Game_Resource::IsPlayerReady(CASW_Player *pPlayer)
{
	if (!pPlayer)
		return false;

	int i = pPlayer->entindex() -1 ;
	if (i<0 || i>ASW_MAX_READY_PLAYERS-1)
		return false;

	return m_bPlayerReady[i];
}

bool CASW_Game_Resource::IsPlayerReady(int iPlayerEntIndex)
{
	int i = iPlayerEntIndex -1 ;
	if (i<0 || i>ASW_MAX_READY_PLAYERS-1)
		return false;

	return m_bPlayerReady[i];
}

int CASW_Game_Resource::CountAllAliveMarines( void )
{
	int m = GetMaxMarineResources();
	int iMarines = 0;
	for ( int i = 0; i < m; i++ )
	{
		CASW_Marine_Resource *pMR = GetMarineResource( i );
		if ( !pMR )
			continue;

		CASW_Marine *pMarine = pMR->GetMarineEntity();
		if ( !pMarine )
			continue;

		if ( pMR->GetHealthPercent() > 0 )
		{
			iMarines++;
		}
	}
	return iMarines;
}

// uses the marine list to quickly find all marines within a box
int CASW_Game_Resource::EnumerateMarinesInBox(Vector &mins, Vector &maxs)
{
	m_iNumEnumeratedMarines = 0;
	for (int i=0;i<GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = GetMarineResource(i);
		if (!pMR)
			continue;

		CASW_Marine *pMarine = pMR->GetMarineEntity();
		if (!pMarine)
			continue;

#ifdef CLIENT_DLL
		if (asw_debug_clientside_avoidance.GetBool())
		{
			Vector mid = (pMarine->WorldAlignMins() + pMarine->WorldAlignMaxs()) / 2.0f;
			Vector omin = pMarine->WorldAlignMins() - mid;
			Vector omax = pMarine->WorldAlignMaxs() - mid;
			debugoverlay->AddBoxOverlay( mid, omin, omax, vec3_angle, 0, 0, 255, true, 0 );
		}
#endif

		Vector omins = pMarine->WorldAlignMins() + pMarine->GetAbsOrigin();
		Vector omaxs = pMarine->WorldAlignMaxs() + pMarine->GetAbsOrigin();
		if (IsBoxIntersectingBox(mins, maxs, omins, omaxs))
		{
			m_pEnumeratedMarines[m_iNumEnumeratedMarines] = pMR->GetMarineEntity();
			m_iNumEnumeratedMarines++;
			if (m_iNumEnumeratedMarines >=12)
				break;
		}
	}
	return m_iNumEnumeratedMarines;
}

CASW_Marine * CASW_Game_Resource::EnumeratedMarine(int i)
{
	if (i<0 || i>=m_iNumEnumeratedMarines)
		return NULL;

	return m_pEnumeratedMarines[i];
}

// returns true if at least one marine has been selected
bool CASW_Game_Resource::AtLeastOneMarine()
{
	int m = GetMaxMarineResources();
	for (int i=0;i<m;i++)
	{
		if (GetMarineResource(i))
		{				
			return true;
		}
	}
	return false;
}

// returns how many marines this player has selected
int CASW_Game_Resource::GetNumMarines(CASW_Player *pPlayer, bool bAliveOnly)
{
	int m = GetMaxMarineResources();
	int iMarines = 0;
	for (int i=0;i<m;i++)
	{
		if (GetMarineResource(i) && (GetMarineResource(i)->GetCommander() == pPlayer || pPlayer == NULL) )
		{
			if (!bAliveOnly || GetMarineResource(i)->GetHealthPercent() > 0)
				iMarines++;
		}
	}
	return iMarines;
}

int CASW_Game_Resource::GetMarineResourceIndex( CASW_Marine_Resource *pMR )
{
	if ( !pMR )
		return -1; 

	for ( int i=0; i < GetMaxMarineResources(); i++ )
	{
		if ( pMR == GetMarineResource( i ) )
			return i;
	}
	return -1;
}

CASW_Marine_Resource* CASW_Game_Resource::GetFirstMarineResourceForPlayer( CASW_Player *pPlayer )
{
	for ( int i=0; i < GetMaxMarineResources(); i++ )
	{
		CASW_Marine_Resource *pMR = GetMarineResource(i);
		if (!pMR)
			continue;

		if ( pMR->GetCommander() == pPlayer )
			return pMR;
	}
	return NULL;
}