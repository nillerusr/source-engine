#ifndef _INCLUDED_C_ASW_CAMPAIGN_SAVE_H
#define _INCLUDED_C_ASW_CAMPAIGN_SAVE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"

class C_ASW_Player;
class CASW_Campaign_Info;

// This class describes the state of the current campaign game.  This is what gets saved when the players save their game.
//   When a savegame is loaded in, it is networked to all the clients (so they can bring up the campaign map, etc)

class C_ASW_Campaign_Save : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ASW_Campaign_Save, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_ASW_Campaign_Save();
	virtual ~C_ASW_Campaign_Save();

	virtual void OnDataChanged( DataUpdateType_t updateType );

	// savegame version (current is 0)
	CNetworkVar(int, m_iVersion);

	// the campaign this save game is meant for	
	char		m_CampaignName[255];
	char		m_szPreviousCampaignName[255];

	// the mission ID of the location the squad is current in
	CNetworkVar(int, m_iCurrentPosition);

	// a list of IDs describing the missions the squad has completed so far
	CNetworkVar(int, m_iNumMissionsComplete);
	
	CNetworkArray(int, m_NumRetries, ASW_MAX_MISSIONS_PER_CAMPAIGN);
	CNetworkArray(int, m_MissionComplete, ASW_MAX_MISSIONS_PER_CAMPAIGN);

	CNetworkVar(float, m_fVoteEndTime);
	CNetworkArray(int, m_NumVotes, ASW_MAX_MISSIONS_PER_CAMPAIGN);	// number of votes each mission has

	bool UsingFixedSkillPoints() { return m_bFixedSkillPoints.Get(); }
	CNetworkVar(bool, m_bFixedSkillPoints);

	CNetworkArray(bool, m_bMarineWounded, ASW_NUM_MARINE_PROFILES);
	CNetworkArray(bool, m_bMarineDead, ASW_NUM_MARINE_PROFILES);
	bool IsMarineWounded(int iProfileIndex);
	bool IsMarineAlive(int iProfileIndex);

	// data specific to each marine that needs to be saved
	char	m_MissionsCompleteNames[ ASW_NUM_MARINE_PROFILES ][255];
	char	m_Medals[ ASW_NUM_MARINE_PROFILES ][255];
	
	// single or multiplayer game
	CNetworkVar(bool, m_bMultiplayerGame);

	// date/time	
	char		m_DateTime[255];

	// helper functions for examining the campaign save state
	bool IsMissionLinkedToACompleteMission(int i, CASW_Campaign_Info* pCampaignInfo);

	// todo:  any extra data, such as optional objectives complete, fancy stuff unlocked?

	const char* GetCampaignName();

	int GetRetries();
};

class ISteamRemoteStorage;

bool GetFileFromRemoteStorage( ISteamRemoteStorage *pRemoteStorage, const char *pszRemoteFileName, const char *pszLocalFileName );
bool WriteFileToRemoteStorage( ISteamRemoteStorage *pRemoteStorage, const char *pszRemoteFileName, const char *pszLocalFileName );

#endif // _INCLUDED_C_ASW_CAMPAIGN_SAVE_H