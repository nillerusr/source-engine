#ifndef _INCLUDED_C_ASW_VOTING_MISSION_CHOOSER_SOURCE_H
#define _INCLUDED_C_ASW_VOTING_MISSION_CHOOSER_SOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "missionchooser/iasw_mission_chooser_source.h"
#include "asw_shareddefs.h"

class C_ASW_Voting_Missions;

// provides lists of missions, saves and campaigns from a client entity
class C_ASW_Voting_Mission_Chooser_Source : public IASW_Mission_Chooser_Source
{
public:
	C_ASW_Voting_Mission_Chooser_Source();

	virtual void Think();
	virtual void IdleThink() { }

	// todo: calls to findX should check if the page/slots/req changed since the last call
	//  and if so, send a clientcommand to the server telling it to change the networked strings for this player
	
	virtual void FindMissionsInCampaign( int iCampaignIndex, int nMissionOffset, int iNumSlots );
	virtual int GetNumMissionsInCampaign( int iCampaignIndex );

	virtual void FindMissions(int nMissionOffset, int iNumSlots, bool bRequireOverview);
	virtual ASW_Mission_Chooser_Mission* GetMissions() { return m_missions; } // pass an array of mission names back
	virtual ASW_Mission_Chooser_Mission* GetMission( int nIndex, bool bRequireOverview );		// will return NULL when requesting a mission outside the found range
	virtual int	 GetNumMissions(bool bRequireOverview);
	
	virtual void FindCampaigns(int nCampaignOffset, int iNumSlots);
	virtual ASW_Mission_Chooser_Mission* GetCampaigns() { return m_campaigns; }	// Passes an array of campaign names back
	virtual ASW_Mission_Chooser_Mission* GetCampaign( int nIndex );		// will return NULL when requesting a campaign outside the found range
	virtual int	 GetNumCampaigns();

	virtual void FindSavedCampaigns(int page, int iNumSlots, bool bMultiplayer, const char *szFilterID);
	virtual ASW_Mission_Chooser_Saved_Campaign* GetSavedCampaigns() { return m_savedcampaigns; }	// passes an array of summary data for each save
	virtual ASW_Mission_Chooser_Saved_Campaign* GetSavedCampaign( int nIndex, bool bMultiplayer, const char *szFilterID );	// will return NULL when requesting a save outside the found range
	virtual int	 GetNumSavedCampaigns(bool bMultiplayer, const char *szFilterID);
	virtual void RefreshSavedCampaigns();

	KeyValues *GetMissionDetails( const char *szMissionName ) { return NULL; }
	KeyValues *GetCampaignDetails( const char *szCampaignName ) { return NULL; }

	ASW_Mission_Chooser_Mission m_missions[ASW_MISSIONS_PER_PAGE]; // this array needs to be large enough for either ASW_SAVES_PER_SCREEN (for when showing save games), or ASW_MISSIONS_PER_SCREEN (for when showing missions) or ASW_CAMPAIGNS_PER_SCREEN (for when showing campaigns)
	ASW_Mission_Chooser_Mission m_campaigns[ASW_CAMPAIGNS_PER_PAGE]; // this array needs to be large enough for either ASW_SAVES_PER_SCREEN (for when showing save games), or ASW_MISSIONS_PER_SCREEN (for when showing missions) or ASW_CAMPAIGNS_PER_SCREEN (for when showing campaigns)
	ASW_Mission_Chooser_Saved_Campaign m_savedcampaigns[ASW_SAVES_PER_PAGE]; // this array needs to be large enough for either ASW_SAVES_PER_SCREEN (for when showing save games), or ASW_MISSIONS_PER_SCREEN (for when showing missions) or ASW_CAMPAIGNS_PER_SCREEN (for when showing campaigns)

	void SetVotingMission(C_ASW_Voting_Missions* voting);

	// existence checks not supported with network chooser source
	bool MissionExists(const char *szCampaignName, bool bRequireOverview) { return false; }
	bool CampaignExists(const char *szCampaignName)  { return false; }
	bool SavedCampaignExists(const char *szSaveName)  { return false; }
	const char *GetPrettyMissionName(const char *szMapName) { return ""; }
	const char *GetPrettyCampaignName(const char *szCampaignName) { return ""; }
	const char* GetPrettySavedCampaignName(const char *szSaveName) { return ""; }
	virtual bool ASW_Campaign_CreateNewSaveGame(char *szFileName, int iFileNameMaxLen, const char *szCampaignName, bool bMultiplayerGame, const char *szStartingMission) { return false; }
	virtual void OnSaveDeleted(const char *szSaveName) { }
	virtual void OnSaveUpdated(const char *szSaveName) { }
	virtual int GetNumMissionsCompleted(const char *szSaveName) { return -1; }
	virtual void NotifySaveDeleted(const char *szSaveName) { } 
	virtual const char* GetCampaignSaveIntroMap(const char* szSaveName) { return NULL; }

	CHandle<C_ASW_Voting_Missions> m_hVotingMission;	// client entity holding networked map/campaign names from the server
	int m_nMissionOffset;
	int m_iNumMissionSlots;
	bool m_bRequireOverview;
	int m_nCampaignOffset;
	int m_iNumCampaignSlots;
	int m_nSaveOffset;
	int m_iNumSavedSlots;
	int m_nCampaignIndex;
	virtual void ResetCurrentPage();	// resets vars that track which page we're on
};

C_ASW_Voting_Mission_Chooser_Source* GetVotingMissionSource();

#endif // _INCLUDED_C_ASW_VOTING_MISSION_CHOOSER_SOURCE_H
