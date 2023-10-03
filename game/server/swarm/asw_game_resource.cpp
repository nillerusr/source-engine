#include "cbase.h"
#include "asw_game_resource.h"
#include "asw_objective.h"
#include "asw_marine_resource.h"
#include "asw_scanner_info.h"
#include "asw_player.h"
#include "asw_campaign_save.h"
#include "asw_campaign_info.h"
#include "asw_marine_profile.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static char s_szLastLeaderNetworkID[ASW_LEADERID_LEN] = {0};

LINK_ENTITY_TO_CLASS( asw_game_resource, CASW_Game_Resource );

extern void SendProxy_String_tToString( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

IMPLEMENT_SERVERCLASS_ST(CASW_Game_Resource, DT_ASW_Game_Resource)	
	//SendPropArray( SendPropEHandle( SENDINFO_ARRAY(m_MarineResources) ), m_MarineResources ),
	SendPropArray3( SENDINFO_ARRAY3(m_MarineResources), SendPropEHandle( SENDINFO_ARRAY(m_MarineResources) ) ),
	//SendPropArray( SendPropEHandle( SENDINFO_ARRAY(m_Objectives) ),	m_Objectives ),
	SendPropArray3( SENDINFO_ARRAY3(m_Objectives), SendPropEHandle( SENDINFO_ARRAY(m_Objectives) ) ),		
	//SendPropArray( SendPropBool( SENDINFO_ARRAY(m_bRosterSelected) ), m_bRosterSelected	),
	SendPropArray3	( SENDINFO_ARRAY3(m_iRosterSelected), SendPropInt( SENDINFO_ARRAY(m_iRosterSelected) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_bPlayerReady), SendPropBool( SENDINFO_ARRAY(m_bPlayerReady) ) ),
	
	
	SendPropEHandle (SENDINFO(m_Leader) ),
	SendPropInt(SENDINFO(m_iLeaderIndex), 8),
	SendPropEHandle (SENDINFO(m_hScannerInfo) ),
	SendPropInt(SENDINFO(m_iCampaignGame), 4),
	SendPropEHandle (SENDINFO(m_hCampaignSave) ),
	SendPropBool (SENDINFO(m_bOneMarineEach)),
	SendPropInt(SENDINFO(m_iMaxMarines)),
	SendPropBool (SENDINFO(m_bOfflineGame)),

	// marine skills
	SendPropArray3( SENDINFO_ARRAY3( m_iSkillSlot0 ), SendPropInt( SENDINFO_ARRAY( m_iSkillSlot0 ), ASW_NUM_MARINE_PROFILES, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iSkillSlot1 ), SendPropInt( SENDINFO_ARRAY( m_iSkillSlot1 ), ASW_NUM_MARINE_PROFILES, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iSkillSlot2 ), SendPropInt( SENDINFO_ARRAY( m_iSkillSlot2 ), ASW_NUM_MARINE_PROFILES, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iSkillSlot3 ), SendPropInt( SENDINFO_ARRAY( m_iSkillSlot3 ), ASW_NUM_MARINE_PROFILES, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iSkillSlot4 ), SendPropInt( SENDINFO_ARRAY( m_iSkillSlot4 ), ASW_NUM_MARINE_PROFILES, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iSkillSlotSpare ), SendPropInt( SENDINFO_ARRAY( m_iSkillSlotSpare ), ASW_NUM_MARINE_PROFILES, SPROP_UNSIGNED ) ),

	// player specific medals
	SendPropArray( SendPropString( SENDINFO_ARRAY( m_iszPlayerMedals ), 0, SendProxy_String_tToString ), m_iszPlayerMedals ),

	SendPropArray3( SENDINFO_ARRAY3( m_iKickVotes ), SendPropInt( SENDINFO_ARRAY( m_iKickVotes ), ASW_MAX_READY_PLAYERS, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iLeaderVotes ), SendPropInt( SENDINFO_ARRAY( m_iLeaderVotes ), ASW_MAX_READY_PLAYERS, SPROP_UNSIGNED ) ),

	SendPropInt(SENDINFO(m_iMoney)),
	SendPropInt(SENDINFO(m_iNextCampaignMission)),

	SendPropInt(SENDINFO(m_nDifficultySuggestion)),

	SendPropFloat( SENDINFO(m_fMapGenerationProgress) ),
	SendPropString( SENDINFO(m_szMapGenerationStatus) ),
	SendPropInt( SENDINFO(m_iRandomMapSeed) ),
END_SEND_TABLE()

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Game_Resource )
	DEFINE_AUTO_ARRAY( m_MarineResources, FIELD_EHANDLE ),
	DEFINE_AUTO_ARRAY( m_Objectives, FIELD_EHANDLE ),
	DEFINE_AUTO_ARRAY( m_iRosterSelected, FIELD_INTEGER ),	
	DEFINE_AUTO_ARRAY( m_bPlayerReady, FIELD_BOOLEAN ),	
	DEFINE_AUTO_ARRAY( m_iCampaignVote, FIELD_INTEGER ),
	DEFINE_FIELD( m_Leader, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iLeaderIndex, FIELD_INTEGER ),
	DEFINE_FIELD( m_hScannerInfo, FIELD_EHANDLE ),
	DEFINE_FIELD( m_NumObjectives, FIELD_INTEGER ),
	DEFINE_FIELD( m_iCampaignGame, FIELD_INTEGER ),
	DEFINE_FIELD( m_hCampaignSave, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iNumMarinesSelected, FIELD_INTEGER ),

	DEFINE_FIELD( m_iszPlayerMedals[0], FIELD_STRING ),
	DEFINE_FIELD( m_iszPlayerMedals[1], FIELD_STRING ),
	DEFINE_FIELD( m_iszPlayerMedals[2], FIELD_STRING ),
	DEFINE_FIELD( m_iszPlayerMedals[3], FIELD_STRING ),
	DEFINE_FIELD( m_iszPlayerMedals[4], FIELD_STRING ),
	DEFINE_FIELD( m_iszPlayerMedals[5], FIELD_STRING ),
	DEFINE_FIELD( m_iszPlayerMedals[6], FIELD_STRING ),
	DEFINE_FIELD( m_iszPlayerMedals[7], FIELD_STRING ),
END_DATADESC()

CASW_Game_Resource *g_pASWGameResource = NULL;

extern bool g_bOfflineGame;

int CASW_Game_Resource::s_nNumConsecutiveFailures = 0;
bool CASW_Game_Resource::s_bLeaderGivenDifficultySuggestion = false;

CASW_Game_Resource::CASW_Game_Resource()
{
	g_pASWGameResource = this;
	m_iNumEnumeratedMarines = NULL;
	m_pCampaignInfo = NULL;
	m_iMaxMarines = 4;
	m_bOneMarineEach = false;
	m_iNumMarinesSelected = 0;
	m_iRandomMapSeed = 0;
	m_iNextCampaignMission = -1;
	m_iEggsHatched = 0;
	m_iEggsKilled = 0;
	m_iStartingEggsInMap = -1;
	m_bAwardedDamageAmpAchievement = false;
	m_iAliensKilledWithDamageAmp = 0;
	m_iElectroStunnedAliens = 0;
	for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
	{
		m_bPlayerReady.Set(i, false);
		m_iKickVotes.Set(i, 0);
		m_iLeaderVotes.Set(i, 0);
		m_iCampaignVote[i] = -1;
	}
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		m_MarineResources.Set(i, NULL);		
	}
	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		m_Objectives.Set(i, NULL);
	}
	CASW_Scanner_Info* pScanner = (CASW_Scanner_Info*)CreateEntityByName("asw_scanner_info");
	if (pScanner)
	{
		pScanner->Spawn();
		m_hScannerInfo = pScanner;
	}

	m_bOfflineGame = g_bOfflineGame;
	m_iCampaignGame = 0;
	m_szCampaignSaveName[0] = '\0';
	// query map's launch options to see if we're in campaign mode
	KeyValues *pLaunchOptions = engine->GetLaunchOptions();
	if (pLaunchOptions)
	{
		KeyValues *pKey = pLaunchOptions->GetFirstSubKey();
		while ( pKey )
		{
			if ( !Q_stricmp( pKey->GetString(), "campaign" ) )
			{
				m_iCampaignGame = 1;
				KeyValues *pCampaignSaveName = pKey->GetNextKey();
				if (pCampaignSaveName)
					Q_snprintf(m_szCampaignSaveName, sizeof(m_szCampaignSaveName), "%s", pCampaignSaveName->GetString());
				break;
			}
			pKey = pKey->GetNextKey();
		}
	}

	// make skills default for single mission
	UpdateMarineSkills(NULL);
}

CASW_Game_Resource::~CASW_Game_Resource()
{
	if (m_hScannerInfo.Get())
		delete m_hScannerInfo.Get();

	if ( g_pASWGameResource == this )
	{
		g_pASWGameResource = NULL;
	}
}

// always send this info to players
int CASW_Game_Resource::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

CASW_Objective* CASW_Game_Resource::GetObjective(int i)
{
	if (i<0 || i>=ASW_MAX_OBJECTIVES)
		return NULL;

	if (m_Objectives[i] == NULL)
		return NULL;

	CBaseEntity* c = m_Objectives[i].Get();
	return static_cast<CASW_Objective*>(c);
}

void CASW_Game_Resource::FindObjectivesOfClass(const char *szClass)
{
	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, szClass )) != NULL)
	{
		//CASW_Objective* pObjective = dynamic_cast<CASW_Objective*>(pEntity);
		m_Objectives.Set(m_NumObjectives, pEntity); //pObjective;
		m_NumObjectives++;
	}
}

void CASW_Game_Resource::FindObjectives()
{
	// search through all entities and populate the objectives array
	m_NumObjectives = 0;

	FindObjectivesOfClass("asw_objective_dummy");
	FindObjectivesOfClass("asw_objective_kill_eggs");
	FindObjectivesOfClass("asw_objective_destroy_goo");
	FindObjectivesOfClass("asw_objective_triggered");
	FindObjectivesOfClass("asw_objective_escape");
	FindObjectivesOfClass("asw_objective_survive");
	FindObjectivesOfClass("asw_objective_countdown");
	FindObjectivesOfClass("asw_objective_kill_aliens");
	
	// bubble sort objectives by their Y coord
	CASW_Objective* pObjective;
	//for (int k=0;k<m_NumObjectives-1;k++)
	//{
		//Msg("Objective %d has coord %f and title %s\n", k, GetObjective(k)->GetAbsOrigin().y, 
				//GetObjective(k)->m_ObjectiveTitle);
	//}
	for (int i=0;i<m_NumObjectives;i++)
	{
		for (int j=i+1; j<m_NumObjectives; j++)
		{
			if (GetObjective(j)->GetAbsOrigin().y > GetObjective(i)->GetAbsOrigin().y)
			{
				//Msg("Swapping %d (%f) with %d (%f)\n", i, GetObjective(i)->GetAbsOrigin().y,
					//j, GetObjective(j)->GetAbsOrigin().y);
				pObjective = GetObjective(j);
				m_Objectives.Set(j, GetObjective(i));
				m_Objectives.Set(i, pObjective);
			}
		}
	}
}

bool CASW_Game_Resource::IsRosterSelected(int i)
{
	if (i<0 || i>=ASW_NUM_MARINE_PROFILES)
		return false;

	return m_iRosterSelected[i] == 1;
}

bool CASW_Game_Resource::IsRosterReserved(int i)
{
	if (i<0 || i>=ASW_NUM_MARINE_PROFILES)
		return false;

	return m_iRosterSelected[i] == 2;
}

void CASW_Game_Resource::SetRosterSelected(int i, int iSelected)
{
	if (i<0 || i>=ASW_NUM_MARINE_PROFILES)
		return;
	
	if (iSelected != m_iRosterSelected[i])
	{
		// update num selected
		if (iSelected == 1 && m_iRosterSelected[i] != 1)
		{
			m_iNumMarinesSelected++;
		}
		else if (iSelected != 1 && m_iRosterSelected[i] == 1)
		{
			m_iNumMarinesSelected--;
		}
		m_iRosterSelected.Set(i, iSelected);
	}
}

// unselects a single marine
// tries to pick the player with the most marines selected
void CASW_Game_Resource::RemoveAMarine()
{
	int iHighest = 0;
	CASW_Player *pChosen = NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
	{
		CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));

		if ( pOtherPlayer && pOtherPlayer->IsConnected() )
		{
			int iMarines = GetNumMarines(pOtherPlayer);
			if (iHighest == 0 || iMarines > iHighest)
			{
				iHighest = iMarines;
				pChosen = pOtherPlayer;
			}
		}		
	}
	RemoveAMarineFor(pChosen);
}

void CASW_Game_Resource::RemoveAMarineFor(CASW_Player *pPlayer)
{
	if (!pPlayer || !ASWGameRules())
		return;

	// find this player's first marine and remove it
	int m = GetMaxMarineResources();
	for (int i=0;i<m;i++)
	{
		if (GetMarineResource(i) && GetMarineResource(i)->GetCommander() == pPlayer)
		{
			ASWGameRules()->RosterDeselect(pPlayer, GetMarineResource(i)->GetProfileIndex());
			return;
		}
	}
}


bool CASW_Game_Resource::AddMarineResource( CASW_Marine_Resource *m, int nPreferredSlot )
{
	if ( nPreferredSlot != -1 )
	{
		CASW_Marine_Resource *pExisting = static_cast<CASW_Marine_Resource*>( m_MarineResources[ nPreferredSlot ].Get() );
		if ( pExisting != NULL )
		{
			// if the existing is owned by someone else, then we abort
			if ( pExisting->GetCommander() != m->GetCommander() )
				return false;

			SetRosterSelected( pExisting->GetProfileIndex(), 0 );
			UTIL_Remove( pExisting );
		}

		m_MarineResources.Set( nPreferredSlot, m );

		// the above causes strange cases where the client copy of this networked array is incorrect
		// so we flag each element dirty to cause a complete update, which seems to fix the problem
		for (int k=0;k<ASW_MAX_MARINE_RESOURCES;k++)
		{
			m_MarineResources.GetForModify(k);
		}
		return true;
	}

	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		if (m_MarineResources[i] == NULL)	// found a free slot
		{
			m_MarineResources.Set(i, m);

			// the above causes strange cases where the client copy of this networked array is incorrect
			// so we flag each element dirty to cause a complete update, which seems to fix the problem
			for (int k=0;k<ASW_MAX_MARINE_RESOURCES;k++)
			{
				m_MarineResources.GetForModify(k);
			}
			return true;
		}
	}
	Msg("Couldn't add new marine resource to list as no free slots\n");
	return false;
}

void CASW_Game_Resource::DeleteMarineResource(CASW_Marine_Resource *m)
{
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		if (GetMarineResource(i) == m)
		{
			m_MarineResources.Set(i, NULL);
			UTIL_Remove(m);
			// need to shuffle all others up
			for (int k=i;k<(ASW_MAX_MARINE_RESOURCES-1);k++)
			{
				CASW_Marine_Resource *other = GetMarineResource(k+1);
				m_MarineResources.Set(k, other);
				m_MarineResources.Set(k+1, NULL);
			}
		}
	}
	// the above causes strange cases where the client copy of this networked array is incorrect
	// so we flag each element dirty to cause a complete update, which seems to fix the problem
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		m_MarineResources.GetForModify(i);
	}
}

CASW_Marine_Resource* CASW_Game_Resource::GetMarineResource(int i)
{
	if (i<0 || i>=ASW_MAX_MARINE_RESOURCES || m_MarineResources[i] == NULL)
		return NULL;

	CBaseEntity* c = m_MarineResources[i].Get();
	return static_cast<CASW_Marine_Resource*>(c);
}

CASW_Scanner_Info* CASW_Game_Resource::GetScannerInfo()
{
	CASW_Scanner_Info* pScanner = dynamic_cast<CASW_Scanner_Info*>(m_hScannerInfo.Get());
	return pScanner;
}

void CASW_Game_Resource::SetLeader(CASW_Player *pPlayer)
{
	// check for auto-readying our old leader
	if (m_Leader.Get() && m_Leader.Get() != pPlayer && ASWGameRules() && m_Leader->m_bRequestedSpectator)
	{
		int iPlayerIndex = m_Leader->entindex() - 1;
		if (ASWGameRules()->GetGameState() == ASW_GS_BRIEFING || ASWGameRules()->GetGameState()==ASW_GS_DEBRIEF)
		{
			// player index is out of range
			if (iPlayerIndex >= 0 && iPlayerIndex < ASW_MAX_READY_PLAYERS)
			{
				Msg("Autoreadying old leader, who wanted to be a spectator\n");
				m_bPlayerReady.Set(iPlayerIndex, true);
			}
		}
		else if (ASWGameRules()->GetGameState() == ASW_GS_CAMPAIGNMAP && ASWGameRules()->GetCampaignSave())
		{
			Msg(" old leader telling campaign save that we're spectating\n", iPlayerIndex);
			ASWGameRules()->GetCampaignSave()->PlayerSpectating(m_Leader.Get());
		}
	}
	if (!pPlayer)
	{
		m_Leader = NULL;
		m_iLeaderIndex = -1;
		return;
	}

	m_Leader = pPlayer;
	m_iLeaderIndex = pPlayer->entindex();
	
	if (ASWGameRules()->GetGameState() == ASW_GS_BRIEFING || ASWGameRules()->GetGameState()==ASW_GS_DEBRIEF)
	{
		// unready leader
		int iPlayerIndex = m_iLeaderIndex - 1;

		// player index is out of range
		if (iPlayerIndex >= 0 && iPlayerIndex < ASW_MAX_READY_PLAYERS)
		{
			m_bPlayerReady.Set(iPlayerIndex, false);
		}
	}
}

CASW_Campaign_Save* CASW_Game_Resource::GetCampaignSave()
{
	CASW_Campaign_Save* pSave = dynamic_cast<CASW_Campaign_Save*>(m_hCampaignSave.Get());
	return pSave;
}

CASW_Campaign_Save* CASW_Game_Resource::CreateCampaignSave()
{
	CASW_Campaign_Save* pSave = (CASW_Campaign_Save*)CreateEntityByName("asw_campaign_save");
	if (pSave)
	{
		pSave->Spawn();
		m_hCampaignSave = pSave;
	}
	m_hCampaignSave = pSave;
	pSave = GetCampaignSave();
	return pSave;
}

void CASW_Game_Resource::UpdateMarineSkills( CASW_Campaign_Save *pCampaign )
{
	if ( !pCampaign || pCampaign->UsingFixedSkillPoints() )
	{
		// update skills with defaults
		for (int iProfileIndex=0;iProfileIndex<ASW_NUM_MARINE_PROFILES;iProfileIndex++)
		{
			CASW_Marine_Profile *pProfile = MarineProfileList()->m_Profiles[iProfileIndex];
			if (pProfile)
			{
				m_iSkillSlot0.Set( iProfileIndex, pProfile->m_iStaticSkills[ ASW_SKILL_SLOT_0 ] );
				m_iSkillSlot1.Set( iProfileIndex, pProfile->m_iStaticSkills[ ASW_SKILL_SLOT_1 ] );
				m_iSkillSlot2.Set( iProfileIndex, pProfile->m_iStaticSkills[ ASW_SKILL_SLOT_2 ] );
				m_iSkillSlot3.Set( iProfileIndex, pProfile->m_iStaticSkills[ ASW_SKILL_SLOT_3 ] );
				m_iSkillSlot4.Set( iProfileIndex, pProfile->m_iStaticSkills[ ASW_SKILL_SLOT_4 ] );
				m_iSkillSlotSpare.Set( iProfileIndex, pProfile->m_iStaticSkills[ ASW_SKILL_SLOT_SPARE ] );
			}
			else
			{
				m_iSkillSlot0.Set( iProfileIndex, 0 );
				m_iSkillSlot1.Set( iProfileIndex, 0 );
				m_iSkillSlot2.Set( iProfileIndex, 0 );
				m_iSkillSlot3.Set( iProfileIndex, 0 );
				m_iSkillSlot4.Set( iProfileIndex, 0 );
				m_iSkillSlotSpare.Set( iProfileIndex, 0 );
			}
		}
	}
	else
	{
		// get skills from the campaign save
		for (int iProfileIndex=0;iProfileIndex<ASW_NUM_MARINE_PROFILES;iProfileIndex++)
		{
			m_iSkillSlot0.Set( iProfileIndex, pCampaign->m_iMarineSkill[iProfileIndex][ASW_SKILL_SLOT_0] );
			m_iSkillSlot1.Set( iProfileIndex, pCampaign->m_iMarineSkill[iProfileIndex][ASW_SKILL_SLOT_1] );
			m_iSkillSlot2.Set( iProfileIndex, pCampaign->m_iMarineSkill[iProfileIndex][ASW_SKILL_SLOT_2] );
			m_iSkillSlot3.Set( iProfileIndex, pCampaign->m_iMarineSkill[iProfileIndex][ASW_SKILL_SLOT_3] );
			m_iSkillSlot4.Set( iProfileIndex, pCampaign->m_iMarineSkill[iProfileIndex][ASW_SKILL_SLOT_4] );
			m_iSkillSlotSpare.Set( iProfileIndex, pCampaign->m_iMarineSkill[iProfileIndex][ASW_SKILL_SLOT_SPARE] );
		}
	}
}

bool CASW_Game_Resource::SetMarineSkill( int nProfileIndex, int nSkillSlot, int nValue )
{
	if ( nProfileIndex < 0 || nProfileIndex > ASW_NUM_MARINE_PROFILES )
		return false;
	if ( nValue < 0 || nValue > 5 )
		return false;

	switch ( nSkillSlot )
	{
		case ASW_SKILL_SLOT_0: m_iSkillSlot0.Set( nProfileIndex, nValue ); break;
		case ASW_SKILL_SLOT_1: m_iSkillSlot1.Set( nProfileIndex, nValue ); break;
		case ASW_SKILL_SLOT_2: m_iSkillSlot2.Set( nProfileIndex, nValue ); break;
		case ASW_SKILL_SLOT_3: m_iSkillSlot3.Set( nProfileIndex, nValue ); break;
		case ASW_SKILL_SLOT_4: m_iSkillSlot4.Set( nProfileIndex, nValue ); break;
		case ASW_SKILL_SLOT_SPARE: m_iSkillSlotSpare.Set( nProfileIndex, nValue ); break;
		default: return false; break;
	}

	return true;
}

CASW_Player* CASW_Game_Resource::GetLeader()
{
	return dynamic_cast<CASW_Player*>(m_Leader.Get());
}

CASW_Marine* CASW_Game_Resource::FindMarineByVoiceType( ASW_Voice_Type voice )
{
	int m = GetMaxMarineResources();	
	for (int i=0;i<m;i++)
	{
		if (GetMarineResource(i) && GetMarineResource(i)->GetProfile() && GetMarineResource(i)->GetProfile()->m_VoiceType == voice)
		{
			return GetMarineResource(i)->GetMarineEntity();
		}
	}
	return NULL;
}

void CASW_Game_Resource::RememberLeaderID()
{
	if (GetLeader())
	{
		SetLastLeaderNetworkID(GetLeader()->GetASWNetworkID());
	}
}

const char* CASW_Game_Resource::GetLastLeaderNetworkID()
{
	return s_szLastLeaderNetworkID;
}

void CASW_Game_Resource::SetLastLeaderNetworkID(const char* szID)
{
	Q_snprintf(s_szLastLeaderNetworkID, sizeof(s_szLastLeaderNetworkID), "%s", szID);
}

int CASW_Game_Resource::GetAliensKilledInThisMission()
{
	int nCount = 0;
	int m = GetMaxMarineResources();	
	for ( int i = 0; i < m; i++ )
	{
		CASW_Marine_Resource *pMR = GetMarineResource( i );
		if ( !pMR )
			continue;

		nCount += pMR->m_iAliensKilled.Get();
	}
	return nCount;
}

void CASW_Game_Resource::OnMissionFailed( void )
{
	s_nNumConsecutiveFailures++;

	if ( !s_bLeaderGivenDifficultySuggestion && s_nNumConsecutiveFailures >= 4 )
	{
		s_bLeaderGivenDifficultySuggestion = true;
		m_nDifficultySuggestion = 2;
	}
}

void CASW_Game_Resource::OnMissionCompleted( bool bWellDone )
{
	if ( !s_bLeaderGivenDifficultySuggestion && s_nNumConsecutiveFailures == 0 && bWellDone )
	{
		s_bLeaderGivenDifficultySuggestion = true;
		m_nDifficultySuggestion = 1;
	}
	else
	{
		m_nDifficultySuggestion = 0;
	}

	s_nNumConsecutiveFailures = 0;
}
