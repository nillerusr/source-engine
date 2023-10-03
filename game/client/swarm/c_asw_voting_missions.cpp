#include "cbase.h"
#include "c_asw_voting_missions.h"
#include "c_asw_voting_mission_chooser_source.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Voting_Missions, DT_ASW_Voting_Missions, CASW_Voting_Missions)
	// mission names
	RecvPropString( RECVINFO(m_iszMissionNames[0]) ),
	RecvPropString( RECVINFO(m_iszMissionNames[1]) ),
	RecvPropString( RECVINFO(m_iszMissionNames[2]) ),
	RecvPropString( RECVINFO(m_iszMissionNames[3]) ),
	RecvPropString( RECVINFO(m_iszMissionNames[4]) ),
	RecvPropString( RECVINFO(m_iszMissionNames[5]) ),
	RecvPropString( RECVINFO(m_iszMissionNames[6]) ),
	RecvPropString( RECVINFO(m_iszMissionNames[7]) ),
	// campaign names
	RecvPropString( RECVINFO(m_iszCampaignNames[0]) ),
	RecvPropString( RECVINFO(m_iszCampaignNames[1]) ),
	RecvPropString( RECVINFO(m_iszCampaignNames[2]) ),
	// save names
	RecvPropString( RECVINFO(m_iszSaveNames[0]) ),
	RecvPropString( RECVINFO(m_iszSaveNames[1]) ),
	RecvPropString( RECVINFO(m_iszSaveNames[2]) ),
	RecvPropString( RECVINFO(m_iszSaveNames[3]) ),
	RecvPropString( RECVINFO(m_iszSaveNames[4]) ),
	RecvPropString( RECVINFO(m_iszSaveNames[5]) ),
	RecvPropString( RECVINFO(m_iszSaveNames[6]) ),
	RecvPropString( RECVINFO(m_iszSaveNames[7]) ),
	// saved campaign names
	RecvPropString( RECVINFO(m_iszSaveCampaignNames[0]) ),
	RecvPropString( RECVINFO(m_iszSaveCampaignNames[1]) ),
	RecvPropString( RECVINFO(m_iszSaveCampaignNames[2]) ),
	RecvPropString( RECVINFO(m_iszSaveCampaignNames[3]) ),
	RecvPropString( RECVINFO(m_iszSaveCampaignNames[4]) ),
	RecvPropString( RECVINFO(m_iszSaveCampaignNames[5]) ),
	RecvPropString( RECVINFO(m_iszSaveCampaignNames[6]) ),
	RecvPropString( RECVINFO(m_iszSaveCampaignNames[7]) ),
	// save date times
	RecvPropString( RECVINFO(m_iszSaveDateTimes[0]) ),
	RecvPropString( RECVINFO(m_iszSaveDateTimes[1]) ),
	RecvPropString( RECVINFO(m_iszSaveDateTimes[2]) ),
	RecvPropString( RECVINFO(m_iszSaveDateTimes[3]) ),
	RecvPropString( RECVINFO(m_iszSaveDateTimes[4]) ),
	RecvPropString( RECVINFO(m_iszSaveDateTimes[5]) ),
	RecvPropString( RECVINFO(m_iszSaveDateTimes[6]) ),
	RecvPropString( RECVINFO(m_iszSaveDateTimes[7]) ),
	// save date times
	RecvPropString( RECVINFO(m_iszSavePlayerNames[0]) ),
	RecvPropString( RECVINFO(m_iszSavePlayerNames[1]) ),
	RecvPropString( RECVINFO(m_iszSavePlayerNames[2]) ),
	RecvPropString( RECVINFO(m_iszSavePlayerNames[3]) ),
	RecvPropString( RECVINFO(m_iszSavePlayerNames[4]) ),
	RecvPropString( RECVINFO(m_iszSavePlayerNames[5]) ),
	RecvPropString( RECVINFO(m_iszSavePlayerNames[6]) ),
	RecvPropString( RECVINFO(m_iszSavePlayerNames[7]) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iSaveMissionsComplete), RecvPropInt( RECVINFO(m_iSaveMissionsComplete[0]))),	
	RecvPropInt( RECVINFO(m_iListType ) ),
	RecvPropInt( RECVINFO(m_iNumMissions ) ),
	RecvPropInt( RECVINFO(m_iNumOverviewMissions ) ),
	RecvPropInt( RECVINFO(m_iNumCampaigns ) ),
	RecvPropInt( RECVINFO(m_iNumSavedCampaigns ) ),
	RecvPropInt( RECVINFO(m_nCampaignIndex ) ),
END_RECV_TABLE()

C_ASW_Voting_Missions::C_ASW_Voting_Missions()
{
	m_iNumMissions = 0;
	m_iNumOverviewMissions = 0;
	m_iNumCampaigns = 0;
	m_iNumSavedCampaigns = 0;
	m_bLaunchedChooser = false;
	m_nCampaignIndex = -1;
}


void C_ASW_Voting_Missions::Update()
{	
	if (m_iListType != 0 && !m_bLaunchedChooser)	// todo: handle the player closing the window?
	{
		Msg("Launching chooser on client! listtype is %d\n", m_iListType);
		if (GetVotingMissionSource())
		{
			GetVotingMissionSource()->SetVotingMission(this);
			GetVotingMissionSource()->ResetCurrentPage();
		}
		m_bLaunchedChooser = true;
	}
}