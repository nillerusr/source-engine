#include "cbase.h"
#include "asw_voting_missions.h"
#include "asw_player.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "vstdlib/random.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// todo: send proxy for our data table, so it only goes to one player?

LINK_ENTITY_TO_CLASS( asw_voting_missions, CASW_Voting_Missions );

extern void SendProxy_String_tToString( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

IMPLEMENT_SERVERCLASS_ST( CASW_Voting_Missions, DT_ASW_Voting_Missions )
	// mission names
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszMissionNames, 0 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszMissionNames, 1 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszMissionNames, 2 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszMissionNames, 3 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszMissionNames, 4 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszMissionNames, 5 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszMissionNames, 6 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszMissionNames, 7 ) ),
	// campaign names
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszCampaignNames, 0 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszCampaignNames, 1 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszCampaignNames, 2 ) ),
	// save names
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveNames, 0 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveNames, 1 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveNames, 2 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveNames, 3 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveNames, 4 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveNames, 5 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveNames, 6 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveNames, 7 ) ),
	// saved campaign names
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveCampaignNames, 0 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveCampaignNames, 1 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveCampaignNames, 2 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveCampaignNames, 3 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveCampaignNames, 4 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveCampaignNames, 5 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveCampaignNames, 6 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveCampaignNames, 7 ) ),
	// save date times
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveDateTimes, 0 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveDateTimes, 1 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveDateTimes, 2 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveDateTimes, 3 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveDateTimes, 4 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveDateTimes, 5 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveDateTimes, 6 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSaveDateTimes, 7 ) ),
	// save player names
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSavePlayerNames, 0 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSavePlayerNames, 1 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSavePlayerNames, 2 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSavePlayerNames, 3 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSavePlayerNames, 4 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSavePlayerNames, 5 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSavePlayerNames, 6 ) ),
	SendPropStringT( SENDINFO_NETWORKARRAYELEM( m_iszSavePlayerNames, 7 ) ),

	SendPropArray3( SENDINFO_ARRAY3( m_iSaveMissionsComplete ), SendPropInt( SENDINFO_ARRAY( m_iSaveMissionsComplete ), 8, SPROP_UNSIGNED ) ),
	SendPropInt( SENDINFO(m_iListType ) ),
	SendPropInt( SENDINFO(m_iNumMissions ) ),
	SendPropInt( SENDINFO(m_iNumOverviewMissions ) ),
	SendPropInt( SENDINFO(m_iNumCampaigns ) ),
	SendPropInt( SENDINFO(m_iNumSavedCampaigns ) ),
	SendPropInt( SENDINFO(m_nCampaignIndex) ),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Voting_Missions )
	DEFINE_ARRAY( m_iszMissionNames, FIELD_STRING, ASW_SAVES_PER_PAGE),
	DEFINE_ARRAY( m_iszCampaignNames, FIELD_STRING, ASW_CAMPAIGNS_PER_PAGE),
	DEFINE_ARRAY( m_iszSaveNames, FIELD_STRING, ASW_SAVES_PER_PAGE),
	DEFINE_ARRAY( m_iszSaveCampaignNames, FIELD_STRING, ASW_SAVES_PER_PAGE),
	DEFINE_ARRAY( m_iszSaveDateTimes, FIELD_STRING, ASW_SAVES_PER_PAGE),
	DEFINE_ARRAY( m_iszSavePlayerNames, FIELD_STRING, ASW_SAVES_PER_PAGE),
	DEFINE_ARRAY( m_iSaveMissionsComplete, FIELD_INTEGER, ASW_SAVES_PER_PAGE),
	DEFINE_FIELD( m_hPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iListType, FIELD_INTEGER ),	
	DEFINE_FIELD( m_iNumMissions, FIELD_INTEGER ),	
	DEFINE_FIELD( m_iNumOverviewMissions, FIELD_INTEGER ),
	DEFINE_FIELD( m_iNumCampaigns, FIELD_INTEGER ),	
	DEFINE_FIELD( m_iNumSavedCampaigns, FIELD_INTEGER ),
	DEFINE_FIELD( m_nOffset, FIELD_INTEGER ),
	DEFINE_FIELD( m_iNumSlots, FIELD_INTEGER ),
	DEFINE_THINKFUNC( ScanThink ),
END_DATADESC()

CASW_Voting_Missions::CASW_Voting_Missions()
{
	m_nCampaignIndex = -1;
	m_nOffset = 0;
	m_iNumSlots = ASW_SAVES_PER_PAGE;
	m_hPlayer = NULL;
	for (int i=0;i<ASW_SAVES_PER_PAGE;i++)
	{
		m_iszMissionNames.Set(i, NULL_STRING);
	}
}

// only transmit to the player we're preparing a map list for
int CASW_Voting_Missions::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	if ( m_hPlayer.Get() && m_hPlayer->edict() == pInfo->m_pClientEnt )
	{
		return FL_EDICT_ALWAYS;
	}
	return FL_EDICT_DONTSEND;
}

void CASW_Voting_Missions::Spawn()
{
	BaseClass::Spawn();
	SetThink(NULL);
}

void CASW_Voting_Missions::ScanThink()
{
	if (m_iListType == 0)		// if the player isn't looking at any particular list at the moment, don't bother updating our strings
	{
		SetThink( NULL );
		return;
	}

	if (!missionchooser || !missionchooser->LocalMissionSource())
		return;

	IASW_Mission_Chooser_Source* pMissionSource = missionchooser->LocalMissionSource();

	// let the source think, in case it needs to be scanning folders
	pMissionSource->Think();

	// player is looking at the list of missions
	if (m_iListType == 1)
	{
		if ( m_nCampaignIndex == -1 )
		{
			// make sure the source is setup to be looking at our page of missions (need to do this every time in case someone else is using the source too)
			pMissionSource->FindMissions(m_nOffset, m_iNumSlots, true);
			// copy them from the local source into our networked array
			ASW_Mission_Chooser_Mission* missions = pMissionSource->GetMissions();
			bool bChanged = false;
			for (int i=0;i<m_iNumSlots;i++)
			{
				if (i<ASW_SAVES_PER_PAGE && Q_strcmp(missions[i].m_szMissionName, STRING(m_iszMissionNames[i])))
				{
					bChanged = true;
					m_iszMissionNames.Set(i, AllocPooledString(missions[i].m_szMissionName));
					m_iszMissionNames.GetForModify(i);
				}
			}
			m_iNumMissions = pMissionSource->GetNumMissions(false);
			m_iNumOverviewMissions = pMissionSource->GetNumMissions(true);
		}
		else
		{
			// make sure the source is setup to be looking at our page of missions (need to do this every time in case someone else is using the source too)
			pMissionSource->FindMissionsInCampaign(m_nCampaignIndex, m_nOffset, m_iNumSlots);
			// copy them from the local source into our networked array
			ASW_Mission_Chooser_Mission* missions = pMissionSource->GetMissions();
			bool bChanged = false;
			for (int i=0;i<m_iNumSlots;i++)
			{
				if (i<ASW_SAVES_PER_PAGE && Q_strcmp(missions[i].m_szMissionName, STRING(m_iszMissionNames[i])))
				{
					bChanged = true;
					m_iszMissionNames.Set(i, AllocPooledString(missions[i].m_szMissionName));
					m_iszMissionNames.GetForModify(i);
				}
			}
			m_iNumMissions = pMissionSource->GetNumMissionsInCampaign( m_nCampaignIndex );
			m_iNumOverviewMissions = m_iNumMissions;
		}
	}
	else if (m_iListType == 2)	// player is looking at a list of campaigns
	{
		// make sure the source is setup to be looking at our page of campaign (need to do this every time in case someone else is using the source too)
		pMissionSource->FindCampaigns(m_nOffset, m_iNumSlots);
		// copy them from the local source into our networked array
		ASW_Mission_Chooser_Mission* campaigns = pMissionSource->GetCampaigns();
		for (int i=0;i<m_iNumSlots;i++)
		{
			if (i<ASW_CAMPAIGNS_PER_PAGE && Q_strcmp(campaigns[i].m_szMissionName, STRING(m_iszCampaignNames[i])))
			{
				m_iszCampaignNames.Set(i, AllocPooledString(campaigns[i].m_szMissionName));
			}
		}
		m_iNumCampaigns = pMissionSource->GetNumCampaigns();
	}
	else if (m_iListType == 3)	// player is looking at a list of saved campaign games
	{
		// make sure the source is setup to be looking at our page of saves (need to do this every time in case someone else is using the source too)
		bool bMulti = !( ASWGameResource() && ASWGameResource()->IsOfflineGame() );
		pMissionSource->FindSavedCampaigns(m_nOffset, m_iNumSlots, bMulti, (m_hPlayer.Get() && bMulti) ? m_hPlayer->GetASWNetworkID() : NULL);
		m_iNumSavedCampaigns = pMissionSource->GetNumSavedCampaigns(bMulti, (m_hPlayer.Get() && bMulti) ? m_hPlayer->GetASWNetworkID() : NULL);
		// copy them from the local source into our networked array
		ASW_Mission_Chooser_Saved_Campaign* saved = pMissionSource->GetSavedCampaigns();
		for (int i=0;i<m_iNumSlots;i++)
		{
			if (i<ASW_SAVES_PER_PAGE && Q_strcmp(saved[i].m_szSaveName, STRING(m_iszSaveNames[i])))
			{
				m_iszSaveNames.Set(i, AllocPooledString(saved[i].m_szSaveName));
				m_iszSaveCampaignNames.Set(i, AllocPooledString(saved[i].m_szCampaignName));
				m_iszSaveDateTimes.Set(i, AllocPooledString(saved[i].m_szDateTime));
				m_iszSavePlayerNames.Set(i, AllocPooledString(saved[i].m_szPlayerNames));
				m_iSaveMissionsComplete.Set(i, saved[i].m_iMissionsComplete);
			}
		}
	}
	
	SetThink( &CASW_Voting_Missions::ScanThink );
	SetNextThink(gpGlobals->curtime + 0.1f);
}

void CASW_Voting_Missions::SetListType(CASW_Player *pPlayer, int iListType, int nOffset, int iNumSlots, int iCampaignIndex)
{
	Msg("CASW_Voting_Missions::SetListType %d\n", iListType);
	m_hPlayer = pPlayer;
	m_iListType = iListType;
	m_nOffset = nOffset;
	m_iNumSlots = iNumSlots;
	m_nCampaignIndex = iCampaignIndex;
	SetThink( &CASW_Voting_Missions::ScanThink );
	SetNextThink(gpGlobals->curtime + 0.1f);
}