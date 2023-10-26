#include "cbase.h"
#include "c_asw_game_resource.h"
#include "c_asw_objective.h"
#include "c_asw_marine_resource.h"
#include "c_asw_scanner_info.h"
#include "c_asw_campaign_save.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_input.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Game_Resource, DT_ASW_Game_Resource, CASW_Game_Resource)
	RecvPropArray3( RECVINFO_ARRAY(m_MarineResources), RecvPropEHandle( RECVINFO( m_MarineResources[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_Objectives), RecvPropEHandle( RECVINFO( m_Objectives[0] ) ) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iRosterSelected), RecvPropInt( RECVINFO(m_iRosterSelected[0])) ),
	
	RecvPropArray3		( RECVINFO_ARRAY(m_bPlayerReady), RecvPropBool( RECVINFO(m_bPlayerReady[0])) ),
	
	RecvPropEHandle (RECVINFO(m_Leader) ),
	RecvPropInt(RECVINFO(m_iLeaderIndex)),
	RecvPropEHandle (RECVINFO(m_hScannerInfo) ),
	RecvPropEHandle (RECVINFO(m_hCampaignSave) ),
	RecvPropInt(RECVINFO(m_iCampaignGame)),
	RecvPropBool(RECVINFO(m_bOneMarineEach)),
	RecvPropInt(RECVINFO(m_iMaxMarines)),
	RecvPropBool(RECVINFO(m_bOfflineGame)),

	// marine skills
	RecvPropArray3( RECVINFO_ARRAY(m_iSkillSlot0), RecvPropInt( RECVINFO(m_iSkillSlot0[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iSkillSlot1), RecvPropInt( RECVINFO(m_iSkillSlot1[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iSkillSlot2), RecvPropInt( RECVINFO(m_iSkillSlot2[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iSkillSlot3), RecvPropInt( RECVINFO(m_iSkillSlot3[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iSkillSlot4), RecvPropInt( RECVINFO(m_iSkillSlot4[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iSkillSlotSpare), RecvPropInt( RECVINFO(m_iSkillSlotSpare[0]))),

	RecvPropArray( RecvPropString( RECVINFO( m_iszPlayerMedals[0]) ), m_iszPlayerMedals ),

	RecvPropArray3( RECVINFO_ARRAY(m_iKickVotes), RecvPropInt( RECVINFO(m_iKickVotes[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iLeaderVotes), RecvPropInt( RECVINFO(m_iLeaderVotes[0]))),

	RecvPropInt(RECVINFO(m_iMoney)),
	RecvPropInt(RECVINFO(m_iNextCampaignMission)),

	RecvPropInt(RECVINFO(m_nDifficultySuggestion)),

	RecvPropFloat( RECVINFO(m_fMapGenerationProgress) ),
	RecvPropString( RECVINFO(m_szMapGenerationStatus) ),
	RecvPropInt( RECVINFO(m_iRandomMapSeed) ),
END_RECV_TABLE()

C_ASW_Game_Resource *g_pASWGameResource = NULL;

C_ASW_Game_Resource::C_ASW_Game_Resource()
{
	g_pASWGameResource = this;

	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		m_MarineResources.Set(i, NULL);
	}
	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		m_Objectives.Set(i, NULL);
	}
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		m_iRosterSelected.Set(i, 0);
	}
	m_iCampaignGame = -1;
	m_iNumEnumeratedMarines = NULL;
	m_pCampaignInfo = NULL;
}

C_ASW_Game_Resource::~C_ASW_Game_Resource()
{
	if ( g_pASWGameResource == this )
	{
		g_pASWGameResource = NULL;
	}
}

C_ASW_Objective* C_ASW_Game_Resource::GetObjective(int i)
{
	if (i<0 || i>=ASW_MAX_OBJECTIVES)
		return NULL;

	if (m_Objectives[i] == NULL)
		return NULL;

	C_BaseEntity* c = m_Objectives[i];
	return static_cast<C_ASW_Objective*>(c);
}

C_ASW_Marine_Resource* C_ASW_Game_Resource::GetMarineResource(int i)
{
	if (i<0 || i>11)
		return NULL;

	if (m_MarineResources[i] == NULL)
		return NULL;

	C_BaseEntity* c = m_MarineResources[i];
	return static_cast<C_ASW_Marine_Resource*>(c);
}

int C_ASW_Game_Resource::GetIndexFor(C_ASW_Marine_Resource* pMarineResource)
{
	for (int i=0;i<GetMaxMarineResources();i++)
	{
		if (m_MarineResources[i] == pMarineResource)
			return i;
	}
	return -1;
}

bool C_ASW_Game_Resource::IsRosterSelected(int i)
{
	if (i<0 || i>=ASW_NUM_MARINE_PROFILES)
		return false;

	return m_iRosterSelected[i] == 1;
}

bool C_ASW_Game_Resource::IsRosterReserved(int i)
{
	if (i<0 || i>=ASW_NUM_MARINE_PROFILES)
		return false;
	return m_iRosterSelected[i] == 2;
}

C_ASW_Player* C_ASW_Game_Resource::GetLeader()
{
	return dynamic_cast<C_ASW_Player*>(m_Leader.Get());
}

C_ASW_Scanner_Info* C_ASW_Game_Resource::GetScannerInfo()
{
	return dynamic_cast<C_ASW_Scanner_Info*>(m_hScannerInfo.Get());
}

C_ASW_Campaign_Save* C_ASW_Game_Resource::GetCampaignSave()
{
	return dynamic_cast<C_ASW_Campaign_Save*>(m_hCampaignSave.Get());
}

int C_ASW_Game_Resource::GetNumMarineResources()
{
	int iNum = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		if (GetMarineResource(i))
			iNum++;
	}
	return iNum;
}

C_ASW_Game_Resource::CMarineToCrosshairInfo::tuple_t C_ASW_Game_Resource::CMarineToCrosshairInfo::tuple_t::INVALID;

/// @TODO can use integer compares for slightly better speed
static int __cdecl MarineTupleComparator( const C_ASW_Game_Resource::CMarineToCrosshairInfo::tuple_t * a, const C_ASW_Game_Resource::CMarineToCrosshairInfo::tuple_t * b )
{
	float dif = a->m_fDistToCursor - b->m_fDistToCursor;
	// have to do this because just returning a-b will hit rounding issues
	return ( dif < 0 ? -1 : ( dif > 0 ? 1 : 0 ) );
}


int C_ASW_Game_Resource::CMarineToCrosshairInfo::FindIndexForMarine( C_ASW_Marine *pMarine )
{
	CheckCache();
	for ( int i = 0 ; i < Count() ; ++i )
	{
		if ( GetElement(i).m_hMarine.Get() == pMarine )
			return i;
	}
	return -1;
}

#pragma warning(push)
#pragma warning( disable : 4706 )
/// @TODO can use a more optimal sorting strategy
void C_ASW_Game_Resource::CMarineToCrosshairInfo::RecomputeCache()
{
	VPROF("C_ASW_Game_Resource::CMarineToCrosshairInfo::RecomputeCache()");
	C_ASW_Game_Resource * RESTRICT  pGameResource = ASWGameResource();
	// purge the array.
	m_tMarines.RemoveAll();

	const Vector vecCrosshairAimingPos = ASWInput()->GetCrosshairAimingPos();

	for ( int i=0; i<pGameResource->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		C_ASW_Marine *pMarine;
		if ( pMR && (pMarine = pMR->GetMarineEntity()) )
		{
			float dist = (vecCrosshairAimingPos - pMR->GetMarineEntity()->GetAbsOrigin()).Length2D();
			m_tMarines.AddToTail( tuple_t(pMarine, dist) );
		}
	}

	m_tMarines.Sort( &MarineTupleComparator );

	m_iLastFrameCached = gpGlobals->framecount;
}
#pragma warning(pop)
