#ifndef _INCLUDED_ASW_VOTING_MISSIONS_H
#define _INCLUDED_ASW_VOTING_MISSIONS_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"
#include "missionchooser/iasw_mission_chooser_source.h"

class CASW_Player;

// this class asks the server's local mission source to find maps, campaigns and saved games
//  then puts them into networked strings to network down to an individual player
class CASW_Voting_Missions : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Voting_Missions, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Voting_Missions();

	virtual void Spawn();
	void SetListType(CASW_Player *pPlayer, int iListType, int nOffset, int iNumSlots, int iCampaignIndex=-1);
	void ScanThink();
	int ShouldTransmit( const CCheckTransmitInfo *pInfo );

	CHandle<CASW_Player> m_hPlayer;	// the player this entity is networking map names to		

	CNetworkVar(int, m_iListType); // 0 = none, 1 = missions, 2 = campaigns, 3 = saved games
	int m_nOffset;
	int m_iNumSlots;
	CNetworkVar(int, m_nCampaignIndex);

	CNetworkVar(int, m_iNumMissions);
	CNetworkVar(int, m_iNumOverviewMissions);
	CNetworkVar(int, m_iNumCampaigns);
	CNetworkVar(int, m_iNumSavedCampaigns);

	CNetworkArray( string_t, m_iszMissionNames, ASW_SAVES_PER_PAGE );
	CNetworkArray( string_t, m_iszCampaignNames, ASW_CAMPAIGNS_PER_PAGE );
	CNetworkArray( string_t, m_iszSaveNames, ASW_SAVES_PER_PAGE );
	CNetworkArray( string_t, m_iszSaveCampaignNames, ASW_SAVES_PER_PAGE );
	CNetworkArray( string_t, m_iszSaveDateTimes, ASW_SAVES_PER_PAGE );
	CNetworkArray( string_t, m_iszSavePlayerNames, ASW_SAVES_PER_PAGE );
	CNetworkArray( int, m_iSaveMissionsComplete, ASW_SAVES_PER_PAGE );
};


#endif /* _INCLUDED_ASW_WEAPON_RIFLE_H */
