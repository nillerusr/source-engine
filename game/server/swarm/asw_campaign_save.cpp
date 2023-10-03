#include "cbase.h"
#include "asw_campaign_save.h"
#include "asw_campaign_info.h"
#include "asw_player.h"
#include "asw_gamerules.h"
#include "filesystem.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "asw_marine_resource.h"
#include "asw_game_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_campaign_save, CASW_Campaign_Save );

ConVar asw_custom_skill_points( "asw_custom_skill_points", "0", FCVAR_ARCHIVE, "If set, marines will start with no skill points and will spend them as they progress through the campaign." );

void ASWSendProxy_String_tToString( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	string_t *pString = (string_t*)pData;
	pOut->m_pString = (char*)STRING( *pString );
}

IMPLEMENT_SERVERCLASS_ST(CASW_Campaign_Save, DT_ASW_Campaign_Save)	
	SendPropString(SENDINFO(m_CampaignName)),	
	SendPropInt(SENDINFO(m_iCurrentPosition)),
	SendPropInt(SENDINFO(m_iNumMissionsComplete)),
	SendPropArray3( SENDINFO_ARRAY3( m_MissionComplete ), SendPropInt( SENDINFO_ARRAY( m_MissionComplete ), 8, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_NumRetries ), SendPropInt( SENDINFO_ARRAY( m_NumRetries ), 9, SPROP_UNSIGNED ) ),
	
	SendPropArray3( SENDINFO_ARRAY3( m_bMarineWounded ), SendPropBool( SENDINFO_ARRAY( m_bMarineWounded ) ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_bMarineDead ), SendPropBool( SENDINFO_ARRAY( m_bMarineDead ) ) ),
	
	SendPropArray( SendPropString( SENDINFO_ARRAY( m_MissionsCompleteNames ), 0, ASWSendProxy_String_tToString ), m_MissionsCompleteNames ),
	SendPropArray( SendPropString( SENDINFO_ARRAY( m_Medals ), 0, ASWSendProxy_String_tToString ), m_Medals ),	
	SendPropBool(SENDINFO(m_bMultiplayerGame)),
	SendPropString(SENDINFO(m_DateTime)),

	SendPropArray3( SENDINFO_ARRAY3( m_NumVotes ), SendPropInt( SENDINFO_ARRAY( m_NumVotes ), 8, SPROP_UNSIGNED ) ),
	SendPropFloat( SENDINFO(m_fVoteEndTime) ),
	SendPropBool( SENDINFO( m_bFixedSkillPoints ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Campaign_Save )
	DEFINE_THINKFUNC( MoveThink ),
	DEFINE_THINKFUNC( VoteEndThink ),
	DEFINE_FIELD( m_bNextMissionVoteEnded, FIELD_BOOLEAN )
END_DATADESC()

CASW_Campaign_Save::CASW_Campaign_Save()
{
	m_iVersion = ASW_CURRENT_SAVE_VERSION;
	m_CampaignName.GetForModify()[0] = 0;
	m_iCurrentPosition = 0;	
	m_iNumMissionsComplete = 0;
	m_iInitialNumMissionsComplete = 0;
	m_iNumPlayers = 0;
	m_bMultiplayerGame = false;
	m_CurrentSaveFileName = NULL_STRING;	
	m_DateTime.GetForModify()[0] = 0;
	m_iLowestSkillLevelPlayed = 0;
	m_PlayerIDs.Purge();
	m_PlayerNames.Purge();
	m_fVoteEndTime = 0;	
	m_bFixedSkillPoints = true;
	m_iMoveDestination = -1;
	for (int i=0;i<ASW_MAX_MISSIONS_PER_CAMPAIGN;i++)
	{
		m_MissionComplete.Set(i, 0);
		m_NumRetries.Set(i, 0);
		m_NumVotes.Set(i, 0);
	}
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{		
		m_MissionsCompleteNames.Set(i, NULL_STRING);
		m_Medals.Set(i, NULL_STRING);
		m_bMarineDead.Set(i, false);
		m_bMarineWounded.Set(i, false);
		m_LastCommanders[i] = NULL_STRING;
		m_LastMarineResourceSlot[i] = 0;

		for (int k=0;k<ASW_NUM_SKILL_SLOTS;k++)
		{
			m_iMarineSkill[i][k] = 0;
			m_iPreviousMarineSkill[i][k] = 0;
		}
		// give 2 skill points to spend at the start
		m_iMarineSkill[i][ASW_SKILL_SLOT_SPARE] = 2;
		m_iPreviousMarineSkill[i][ASW_SKILL_SLOT_SPARE] = 2;
		m_iParasitesKilled[i] = 0;
	}
	m_iNumDeaths = 0;
}

CASW_Campaign_Save::~CASW_Campaign_Save()
{
}

// reads in current save data from a keyvalues file
bool CASW_Campaign_Save::LoadGameFromFile(const char *szFileName)
{
	// make sure the path and extension are correct
	char szFullFileName[256];
	char tempbuffer[256];
	Q_snprintf(tempbuffer, sizeof(tempbuffer), "%s", szFileName);	
	Q_SetExtension( tempbuffer, ".campaignsave", sizeof(tempbuffer) );
	const char *pszNoPathName = Q_UnqualifiedFileName(tempbuffer);	
	Q_snprintf(szFullFileName, sizeof(szFullFileName), "save/%s", pszNoPathName);	

	KeyValues *pSaveKeyValues = new KeyValues( szFileName );
	if (pSaveKeyValues->LoadFromFile(filesystem, szFullFileName))
	{
		m_CurrentSaveFileName = AllocPooledString(szFullFileName);

		m_iVersion = pSaveKeyValues->GetInt("Version");		
		m_iLowestSkillLevelPlayed = pSaveKeyValues->GetInt("SkillLevel");
		Q_strncpy( m_CampaignName.GetForModify(), pSaveKeyValues->GetString("CampaignName"), 255 );
		m_iCurrentPosition = pSaveKeyValues->GetInt("CurrentPosition");
		m_iNumMissionsComplete = pSaveKeyValues->GetInt("NumMissionsComplete");
		m_iInitialNumMissionsComplete = pSaveKeyValues->GetInt("InitialNumMissionsComplete");
		m_bMultiplayerGame = pSaveKeyValues->GetInt("Multiplayer") != 0;		
		Q_strncpy( m_DateTime.GetForModify(), pSaveKeyValues->GetString("DateTime"), 255 );		
		m_iNumDeaths = pSaveKeyValues->GetInt("NumDeaths");
		m_bFixedSkillPoints = !asw_custom_skill_points.GetBool(); //pSaveKeyValues->GetBool( "FixedSkillPoints", true );
		
		m_iNumPlayers = pSaveKeyValues->GetInt("NumPlayers");
		m_PlayerNames.Purge();
		m_PlayerIDs.Purge();

		// go through each sub section, adding the relevant details
		KeyValues *pkvSubSection = pSaveKeyValues->GetFirstSubKey();
		while ( pkvSubSection )
		{
			// mission details
			if (Q_stricmp(pkvSubSection->GetName(), "MISSION")==0)
			{
				int MissionID = pkvSubSection->GetInt("MissionID");
				if (MissionID >=0 && MissionID < ASW_MAX_MISSIONS_PER_CAMPAIGN)
				{
					m_MissionComplete.Set(MissionID, pkvSubSection->GetInt("MissionComplete"));
					m_NumRetries.Set(MissionID, pkvSubSection->GetInt("NumRetries"));
				}
			}

			// marine details
			if (Q_stricmp(pkvSubSection->GetName(), "MARINE")==0)
			{
				int MarineID = pkvSubSection->GetInt("MarineID");
				if (MarineID >=0 && MarineID < ASW_NUM_MARINE_PROFILES)
				{
					m_iMarineSkill[MarineID][ASW_SKILL_SLOT_0] = pkvSubSection->GetInt("SkillSlot0");
					m_iMarineSkill[MarineID][ASW_SKILL_SLOT_1] = pkvSubSection->GetInt("SkillSlot1");
					m_iMarineSkill[MarineID][ASW_SKILL_SLOT_2] = pkvSubSection->GetInt("SkillSlot2");
					m_iMarineSkill[MarineID][ASW_SKILL_SLOT_3] = pkvSubSection->GetInt("SkillSlot3");
					m_iMarineSkill[MarineID][ASW_SKILL_SLOT_4] = pkvSubSection->GetInt("SkillSlot4");
					m_iMarineSkill[MarineID][ASW_SKILL_SLOT_SPARE] = pkvSubSection->GetInt("SkillSlotSpare");

					m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_0] = pkvSubSection->GetInt("UndoSkillSlot0");
					m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_1] = pkvSubSection->GetInt("UndoSkillSlot1");
					m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_2] = pkvSubSection->GetInt("UndoSkillSlot2");
					m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_3] = pkvSubSection->GetInt("UndoSkillSlot3");
					m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_4] = pkvSubSection->GetInt("UndoSkillSlot4");
					m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_SPARE] = pkvSubSection->GetInt("UndoSkillSlotSpare");

					m_iParasitesKilled[MarineID] = pkvSubSection->GetInt("ParasitesKilled");

					m_MissionsCompleteNames.Set(MarineID, AllocPooledString(pkvSubSection->GetString("MissionsCompleted")));
					m_Medals.Set(MarineID, AllocPooledString(pkvSubSection->GetString("Medals")));

					m_bMarineWounded.Set(MarineID, (pkvSubSection->GetInt("Wounded") == 1));
					m_bMarineDead.Set(MarineID, (pkvSubSection->GetInt("Dead") == 1));
				}					
			}

			// player name
			if (Q_stricmp(pkvSubSection->GetName(), "PLAYER")==0)
			{
				string_t stringName = AllocPooledString(pkvSubSection->GetString("PlayerName"));
				m_PlayerNames.AddToTail(stringName);				
			}
			// player ID
			if (Q_stricmp(pkvSubSection->GetName(), "DATA")==0)
			{
				string_t stringID = AllocPooledString(pkvSubSection->GetString("DataBlock"));
				m_PlayerIDs.AddToTail(stringID);								
			}
			// last commanders
			if (Q_stricmp(pkvSubSection->GetName(), "COMM")==0)
			{
				for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
				{
					char buffer[16];
					Q_snprintf(buffer, sizeof(buffer), "Comm%d", i);
					string_t stringID = AllocPooledString(pkvSubSection->GetString(buffer));					
					m_LastCommanders[i] = stringID;
					Q_snprintf(buffer, sizeof(buffer), "Slot%d", i);
					m_LastMarineResourceSlot[i] = pkvSubSection->GetInt(buffer);
				}				
			}
			
			pkvSubSection = pkvSubSection->GetNextKey();
		}
		return true;
	}
	Msg("Failed to load KeyValues from file %s\n", szFullFileName);
	return false;
}

// saves current save data to a keyvalues file
bool CASW_Campaign_Save::SaveGameToFile(const char *szFileName)
{
	// make sure the path and extension are correct
	char szFullFileName[256];
	char tempbuffer[256];
	if (szFileName==NULL)
	{
		if (m_CurrentSaveFileName == NULL_STRING)
		{
			Msg("Error: Couldn't save game to file as we have no name already and you didn't supply one.\n");
			return false;
		}
		Q_snprintf(szFullFileName, sizeof(szFullFileName), "%s", STRING(m_CurrentSaveFileName));
	}
	else
	{
		Q_snprintf(tempbuffer, sizeof(tempbuffer), "%s", szFileName);	
		Q_SetExtension( tempbuffer, ".campaignsave", sizeof(tempbuffer) );
		const char *pszNoPathName = Q_UnqualifiedFileName(tempbuffer);	
		Q_snprintf(szFullFileName, sizeof(szFullFileName), "save/%s", pszNoPathName);
	}

	// before saving, check if this is actually a completed campaign
	if (ASWGameRules())
	{
		int iLeft = ASWGameRules()->CampaignMissionsLeft();
		if (iLeft <= 0)
		{
			Msg("Deleting completed campaign %s", szFullFileName);
			filesystem->RemoveFile( szFullFileName, "GAME" );
			
			// notify the local mission source that a save has been deleted
			if (missionchooser && missionchooser->LocalMissionSource())
			{
				const char *pszNoPathName = Q_UnqualifiedFileName(szFullFileName);
				missionchooser->LocalMissionSource()->NotifySaveDeleted(pszNoPathName);
			}
			return true;
		}
	}
	//  we don't want completed campaigns lingering on disk

	KeyValues *pSaveKeyValues = new KeyValues( "CAMPAIGNSAVE" );
	
	pSaveKeyValues->SetInt("Version", m_iVersion);
	pSaveKeyValues->SetInt("SkillLevel", m_iLowestSkillLevelPlayed);
	pSaveKeyValues->SetString("CampaignName", m_CampaignName.Get());
	pSaveKeyValues->SetInt("CurrentPosition", m_iCurrentPosition);
	pSaveKeyValues->SetInt("NumMissionsComplete", m_iNumMissionsComplete);
	pSaveKeyValues->SetInt("InitialNumMissionsComplete", m_iInitialNumMissionsComplete);
	pSaveKeyValues->SetInt("Multiplayer", m_bMultiplayerGame ? 1 : 0);
	pSaveKeyValues->SetInt( "NumDeaths", m_iNumDeaths );
	pSaveKeyValues->SetBool( "FixedSkillPoints", m_bFixedSkillPoints );

	// update date
	int year, month, dayOfWeek, day, hour, minute, second;
	missionchooser->GetCurrentTimeAndDate(&year, &month, &dayOfWeek, &day, &hour, &minute, &second);
	//year = month = dayOfWeek = day = hour = minute = second = 0;
	Q_snprintf(m_DateTime.GetForModify(), 255, "%02d/%02d/%d %02d:%02d", month, day, year, hour, minute);
	pSaveKeyValues->SetString("DateTime", m_DateTime.Get());
	
	// write out each mission's status
	KeyValues *pSubSection;
	for (int i=0; i<ASW_MAX_MISSIONS_PER_CAMPAIGN; i++)
	{
		pSubSection = new KeyValues("MISSION");
		pSubSection->SetInt("MissionID", i);
		pSubSection->SetInt("MissionComplete", m_MissionComplete[i]);
		pSubSection->SetInt("NumRetries", m_NumRetries[i]);
		pSaveKeyValues->AddSubKey(pSubSection);
	}

	// write out each marine's stats
	for (int MarineID=0; MarineID<ASW_NUM_MARINE_PROFILES; MarineID++)
	{
		pSubSection = new KeyValues("MARINE");
		pSubSection->SetInt("MarineID", MarineID);

		pSubSection->SetInt("SkillSlot0", m_iMarineSkill[MarineID][ASW_SKILL_SLOT_0]);
		pSubSection->SetInt("SkillSlot1", m_iMarineSkill[MarineID][ASW_SKILL_SLOT_1]);
		pSubSection->SetInt("SkillSlot2", m_iMarineSkill[MarineID][ASW_SKILL_SLOT_2]);
		pSubSection->SetInt("SkillSlot3", m_iMarineSkill[MarineID][ASW_SKILL_SLOT_3]);
		pSubSection->SetInt("SkillSlot4", m_iMarineSkill[MarineID][ASW_SKILL_SLOT_4]);
		pSubSection->SetInt("SkillSlotSpare", m_iMarineSkill[MarineID][ASW_SKILL_SLOT_SPARE]);

		pSubSection->SetInt("UndoSkillSlot0", m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_0]);
		pSubSection->SetInt("UndoSkillSlot1", m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_1]);
		pSubSection->SetInt("UndoSkillSlot2", m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_2]);
		pSubSection->SetInt("UndoSkillSlot3", m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_3]);
		pSubSection->SetInt("UndoSkillSlot4", m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_4]);
		pSubSection->SetInt("UndoSkillSlotSpare", m_iPreviousMarineSkill[MarineID][ASW_SKILL_SLOT_SPARE]);
		
		pSubSection->SetInt("ParasitesKilled", m_iParasitesKilled[MarineID]);
		pSubSection->SetString("MissionsCompleted", STRING(m_MissionsCompleteNames[MarineID]));
		pSubSection->SetString("Medals", STRING(m_Medals[MarineID]));

		int iWound = IsMarineWounded(MarineID) ? 1 : 0;		
		pSubSection->SetInt("Wounded", iWound);
		int iDead = IsMarineAlive(MarineID) ? 0 : 1;
		pSubSection->SetInt("Dead", iDead);

		pSaveKeyValues->AddSubKey(pSubSection);
	}

	// check for any new players to add to our list
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CASW_Player* pPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));

		if ( pPlayer )
		{
			// first check his network ID
			const char *pszNetworkID = pPlayer->GetASWNetworkID();
			// check if it's in our list
			bool bFound = false;
			for (int k=0;k<m_PlayerIDs.Count();k++)
			{
				const char *p = STRING(m_PlayerIDs[k]);
				if (!Q_strcmp(p, pszNetworkID))
					bFound = true;
			}
			if (!bFound)
			{
				// add it to the list
				string_t stringID = AllocPooledString(pszNetworkID);
				m_PlayerIDs.AddToTail(stringID);
			}

			// then check player name
			const char *pszPlayerName = pPlayer->GetPlayerName();
			bFound = false;
			for (int k=0;k<m_PlayerNames.Count();k++)
			{
				const char *p = STRING(m_PlayerNames[k]);
				if (!Q_strcmp(p, pszPlayerName))
					bFound = true;
			}
			if (!bFound)
			{
				// add it to the list
				string_t stringName = AllocPooledString(pszPlayerName);
				m_PlayerNames.AddToTail(stringName);				
			}
		}
	}

	pSaveKeyValues->SetInt("NumPlayers", m_PlayerNames.Count());	
	
	// write out player names
	for (int i=0; i<m_PlayerNames.Count(); i++)
	{
		pSubSection = new KeyValues("PLAYER");		
		pSubSection->SetString("PlayerName", STRING(m_PlayerNames[i]));
		pSaveKeyValues->AddSubKey(pSubSection);
	}
	// write out player IDs
	for (int i=0; i<m_PlayerIDs.Count(); i++)
	{
		pSubSection = new KeyValues("DATA");		
		pSubSection->SetString("DataBlock", STRING(m_PlayerIDs[i]));
		pSaveKeyValues->AddSubKey(pSubSection);
	}
	// write out last commanders section
	pSubSection = new KeyValues("COMM");
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		char buffer[16];
		Q_snprintf(buffer, sizeof(buffer), "Comm%d", i);
		pSubSection->SetString(buffer, STRING(m_LastCommanders[i]));		
		Q_snprintf(buffer, sizeof(buffer), "Slot%d", i);
		pSubSection->SetInt( buffer, m_LastMarineResourceSlot[i] );
	}
	pSaveKeyValues->AddSubKey(pSubSection);

	if (pSaveKeyValues->SaveToFile(filesystem, szFullFileName))
	{
		if (missionchooser && missionchooser->LocalMissionSource())
		{
			const char *pszNoPathName = Q_UnqualifiedFileName(szFullFileName);
			missionchooser->LocalMissionSource()->OnSaveUpdated(pszNoPathName);
		}
		
		return true;
	}
	return false;
}

void CASW_Campaign_Save::UpdateLastCommanders()
{
	// save which marines the players have selected
	// add which marines he has selected
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return;

	// first check there were some marines selected (i.e. we're not in the campaign lobby map)
	int iNumMarineResources = 0;
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		if (pGameResource->GetMarineResource(i))
			iNumMarineResources++;
	}
	if ( iNumMarineResources <= 0 )
		return;
	
	char buffer[256];
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		// look for a marine info for this marine
		bool bFound = false;
		for (int k=0;k<pGameResource->GetMaxMarineResources();k++)
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(k);
			if (pMR && pMR->GetProfileIndex() == i && pMR->GetCommander())
			{
				CASW_Player *pPlayer = pMR->GetCommander();
				if (pPlayer)
				{
					// store the commander who has this marine
					Q_snprintf(buffer, sizeof(buffer), "%s%s",pPlayer->GetPlayerName(), pPlayer->GetASWNetworkID());
					m_LastCommanders[i] = AllocPooledString(buffer);
					m_LastMarineResourceSlot[i] = k;
					bFound = true;
					break;
				}
			}
		}
		if (!bFound)
		{
			m_LastCommanders[i] = AllocPooledString("");
			m_LastMarineResourceSlot[i] = 0;
		}
	}
}

void CASW_Campaign_Save::DebugInfo()
{
	Msg("Savegame contents:\n");
	Msg(" Version: %d\n", m_iVersion);
	Msg(" Lowest Skill Level Played: %d\n", m_iLowestSkillLevelPlayed);	
	Msg(" Campaign Name: %s\n", m_CampaignName.Get());
	Msg(" Current Position: %d\n", m_iCurrentPosition);
	Msg(" Num Missions Complete: %d\n", m_iNumMissionsComplete);
	Msg(" Initial Num Missions Complete: %d\n", m_iInitialNumMissionsComplete);
	Msg( "Num deaths: %d\n", m_iNumDeaths);
	Msg(" [");
	for (int i=0;i<ASW_MAX_MISSIONS_PER_CAMPAIGN;i++)
	{
		if (m_MissionComplete[i])
			Msg("X");
		else
			Msg("-");
	}
	Msg("]\n");
	Msg("Marine Stats:\n");
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		Msg(" Marine %d:\n", i);
		//Msg("  Health: %d  Speed: %d  Accuracy: %d  Special: %d  Spare: %d\n",
			//m_iMarineHealth[i], m_iMarineSpeed[i], m_iMarineAccuracy[i], m_iMarineSpecial[i], m_iMarineSparePoints[i]);
		Msg("  Missions complete: %s\n", STRING(m_MissionsCompleteNames[i]));
		Msg("  Medals: %s\n", STRING(m_Medals[i]));
	}
	Msg("Multiplayer: %d\n", m_bMultiplayerGame ? 1 : 0);
	Msg("Date/Time: %s\n", m_DateTime.Get());
	Msg("Players: %d\n", m_iNumPlayers);
	Msg("Player names: %d\n", m_PlayerNames.Count());
	for (int i=0;i<m_PlayerNames.Count();i++)
	{
		Msg("  %s\n", STRING(m_PlayerNames[i]));
	}
	Msg("Data blocks: %d\n", m_PlayerIDs.Count());
	for (int i=0;i<m_PlayerIDs.Count();i++)
	{
		Msg("  %s\n", STRING(m_PlayerIDs[i]));
	}
}

void CASW_Campaign_Save::SetMissionComplete(int iMission)
{
	if (iMission>=0 && iMission<ASW_MAX_MISSIONS_PER_CAMPAIGN)
	{
		if (!m_MissionComplete[iMission])
		{
			m_iNumMissionsComplete++;
			m_MissionComplete.Set(iMission, 1);
			// update the lowest skill level played
			int iSkill = ASWGameRules()->GetSkillLevel();
			if (iSkill < m_iLowestSkillLevelPlayed || m_iLowestSkillLevelPlayed == 0)
				m_iLowestSkillLevelPlayed = iSkill;
		}
	}
}

void CASW_Campaign_Save::MoveTo(int iMission)
{
	if (iMission>=0 && iMission<ASW_MAX_MISSIONS_PER_CAMPAIGN)
		m_iCurrentPosition = iMission;
}

const char * CASW_Campaign_Save::GetCampaignName()
{
	return m_CampaignName;
}

bool CASW_Campaign_Save::CreateNewSaveGame(char *szFileName, int iFileNameMaxLen, const char *szCampaignName, bool bMultiplayerGame, const char *szStartingMission)	// szFileName arg is the desired filename or NULL for an autogenerated one.  Function sets szFileName with the filename used.
{
	if (!szFileName)
		return false;

	if (!missionchooser || !missionchooser->LocalMissionSource())
		return false;

	return missionchooser->LocalMissionSource()->ASW_Campaign_CreateNewSaveGame(szFileName, iFileNameMaxLen, szCampaignName, bMultiplayerGame, szStartingMission);
}

int CASW_Campaign_Save::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

void CASW_Campaign_Save::IncreaseRetries()
{
	if (m_iCurrentPosition <0 || m_iCurrentPosition >= ASW_MAX_MISSIONS_PER_CAMPAIGN)
		return;

	int current = m_NumRetries[m_iCurrentPosition];
	m_NumRetries.Set(m_iCurrentPosition, current+1);
}

int CASW_Campaign_Save::GetRetries()
{
	if (m_iCurrentPosition <0 || m_iCurrentPosition >= ASW_MAX_MISSIONS_PER_CAMPAIGN)
		return -1;

	return m_NumRetries[m_iCurrentPosition];
}

void CASW_Campaign_Save::IncreaseMarineSkill( int nProfileIndex, int nSkillSlot )
{
	if (nProfileIndex < 0 || nProfileIndex >= ASW_NUM_MARINE_PROFILES)
		return;

	if (nSkillSlot < 0 || nSkillSlot >= ASW_NUM_SKILL_SLOTS)
		return;

	m_iMarineSkill[nProfileIndex][nSkillSlot]++;	
}

void CASW_Campaign_Save::ReduceMarineSkill(int nProfileIndex, int nSkillSlot)
{
	if (nProfileIndex < 0 || nProfileIndex >= ASW_NUM_MARINE_PROFILES)
		return;

	if (nSkillSlot < 0 || nSkillSlot >= ASW_NUM_SKILL_SLOTS)
		return;

	m_iMarineSkill[nProfileIndex][nSkillSlot]--;	
}

void CASW_Campaign_Save::ReviveMarine(int nProfileIndex)
{
	if (nProfileIndex < 0 || nProfileIndex >= ASW_NUM_MARINE_PROFILES)
		return;

	if (IsMarineAlive(nProfileIndex))	// don't revive ones already alive!
		return;

	// revive
	SetMarineDead(nProfileIndex, false);
	SetMarineWounded(nProfileIndex, false);
	
	// reset skills
	for (int i=0;i<ASW_NUM_SKILL_SLOTS;i++)
	{
		m_iMarineSkill[nProfileIndex][i] = 0;
		m_iPreviousMarineSkill[nProfileIndex][i] = 0;
	}

	// give marine skill points equivalent to a marine that didn't part in any missions
	m_iMarineSkill[nProfileIndex][ASW_SKILL_SLOT_SPARE] = m_iNumMissionsComplete.Get() * 2 + 2;
	m_iPreviousMarineSkill[nProfileIndex][ASW_SKILL_SLOT_SPARE] = m_iNumMissionsComplete.Get() * 2 + 2;

	// misc
	m_iParasitesKilled[nProfileIndex] = 0;	
	m_MissionsCompleteNames.Set(nProfileIndex, NULL_STRING);
	m_Medals.Set(nProfileIndex, NULL_STRING);	
	m_LastCommanders[nProfileIndex] = NULL_STRING;	
	m_LastMarineResourceSlot[nProfileIndex] = 0;
}

int CASW_Campaign_Save::GetParasitesKilled(int nProfileIndex)
{
	if (nProfileIndex < 0 || nProfileIndex >= ASW_NUM_MARINE_PROFILES)
		return 0;

	return m_iParasitesKilled[nProfileIndex];
}

void CASW_Campaign_Save::AddParasitesKilled(int nProfileIndex, int iParasitesKilled)
{
	if (nProfileIndex < 0 || nProfileIndex >= ASW_NUM_MARINE_PROFILES)
		return;

	m_iParasitesKilled[nProfileIndex] += iParasitesKilled;
}

void CASW_Campaign_Save::SetMarineWounded(int nProfileIndex, bool bWounded)
{
	if (nProfileIndex < 0 || nProfileIndex >= ASW_NUM_MARINE_PROFILES)
		return;

	m_bMarineWounded.Set(nProfileIndex, bWounded);
}

void CASW_Campaign_Save::SetMarineDead(int nProfileIndex, bool bDead)
{
	if (nProfileIndex < 0 || nProfileIndex >= ASW_NUM_MARINE_PROFILES)
		return;

	m_bMarineDead.Set(nProfileIndex, bDead);	
}

void CASW_Campaign_Save::RevertSkillsToUndoState(int nProfileIndex)
{
	if (nProfileIndex < 0 || nProfileIndex >= ASW_NUM_MARINE_PROFILES)
		return; 

	for (int k=0;k<ASW_NUM_SKILL_SLOTS;k++)
	{
		m_iMarineSkill[nProfileIndex][k] = m_iPreviousMarineSkill[nProfileIndex][k];
	}	
}

void CASW_Campaign_Save::UpdateSkillUndoState()
{
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		for (int k=0;k<ASW_NUM_SKILL_SLOTS;k++)
		{
			m_iPreviousMarineSkill[i][k] = m_iMarineSkill[i][k];
		}
	}	
}

void CASW_Campaign_Save::SetMoveDestination(int iMission)
{
	if (!ASWGameRules() || !ASWGameRules()->GetCampaignInfo())
		return;

	CASW_Campaign_Info *pCI = ASWGameRules()->GetCampaignInfo();
	if ( !pCI )
		return;

	CASW_Campaign_Info::CASW_Campaign_Mission_t *pMission = pCI->GetMission( iMission );
	if (!pMission)
		return;

	if (iMission != m_iMoveDestination)
	{
		m_iMoveDestination = iMission;
		SetThink( &CASW_Campaign_Save::MoveThink );
		SetNextThink(gpGlobals->curtime + 0.1f);
	}
}

void CASW_Campaign_Save::MoveThink()
{
	if (m_iMoveDestination != -1)
	{
		if (m_iMoveDestination == m_iCurrentPosition)
		{
			Msg("arrived!\n");
			// notify game rules that we've arrived, so it can do any mission launching it desires
			if (ASWGameRules())
				ASWGameRules()->RequestCampaignLaunchMission(m_iCurrentPosition);
		}
		else
		{
			// build a route to the dest from current position
			bool bRoute = BuildCampaignRoute(m_iMoveDestination, m_iCurrentPosition);
			if (!bRoute)
			{
				Msg("Couldn't build route to dest\n");
				SetThink(NULL);
			}
			else
			{
				// we've got a route, move ourselves to the next step in the route
				campaign_route_node_t* pNode = FindMissionInClosedList(m_iRouteDest);
				if (pNode)
					pNode = FindMissionInClosedList(pNode->iParentMission);
				MoveTo(pNode->iMission);
				SetNextThink(gpGlobals->curtime + 1.0f);	// move again in 1 second
			}
		}
	}
}


float CASW_Campaign_Save::EstimateCost(CASW_Campaign_Info *pCI, int iStart, int iEnd)
{
	// heuristic for A*
	CASW_Campaign_Info::CASW_Campaign_Mission_t* pMission = pCI->GetMission(iStart);
	CASW_Campaign_Info::CASW_Campaign_Mission_t* pEndMission = pCI->GetMission(iEnd);
	if (!pMission || !pEndMission)
		return 9000;
	
	float diff_x = pEndMission->m_iLocationX - pMission->m_iLocationX;
	float diff_y = pEndMission->m_iLocationY - pMission->m_iLocationY;
	return (sqrt((diff_x * diff_x) + (diff_y * diff_y)));
}

bool CASW_Campaign_Save::BuildCampaignRoute(int iStart, int iEnd)
{
	if (!ASWGameRules() || !ASWGameRules()->GetCampaignInfo())
		return false;
		
	CASW_Campaign_Info *pCI = ASWGameRules()->GetCampaignInfo();
	if (iStart < 0 || iStart >= pCI->GetNumMissions())
		return false;
	if (iEnd < 0 || iEnd >= pCI->GetNumMissions())
		return false;	

	// do A*
	m_ClosedList.Purge();
	m_OpenList.Purge();
	m_iRouteDest = iEnd;
	m_iRouteStart = iStart;

	// add starting mission to the open list
	campaign_route_node_t start_node;
	start_node.iMission = iStart;
	start_node.iParentMission = iStart;	
	start_node.g = 0;
	start_node.h = EstimateCost(pCI, iStart, iEnd);
	start_node.f = start_node.g + start_node.h;
	m_OpenList.AddToTail(start_node);
	int iOverflow = 0;
	while (iOverflow < 1000)
	{
		// find the lowest node on the open list
		int iLowestF = -1;
		float fLowestF_Value = 99999;
		//Msg("%d nodes in open list\n", m_OpenList.Count());
		for (int i=0;i<m_OpenList.Count();i++)
		{
			if (m_OpenList[i].f < fLowestF_Value)
			{
				//Msg(" and node %d is lower than lowest\n", i);
				fLowestF_Value = m_OpenList[i].f;
				iLowestF = i;
			}
		}
		// if open list is empty, that means we've searched all routes and still didn't arrive at the dest
		if (iLowestF == -1)
		{
			//Msg("Failed to find route\n");
			return false;
		}
		// move it to the closed list
		int iCurrentMission = m_OpenList[iLowestF].iMission;
		float fCurrentCost = m_OpenList[iLowestF].f;
		m_ClosedList.AddToTail(m_OpenList[iLowestF]);
		m_OpenList.Remove(iLowestF);

		if (iCurrentMission == iEnd)		// found the target
		{
			//Msg("current mission is the destination!\n");
			return true;
		}
		//Msg("Current mission is %d, open list size %d closed list size %d\n", iCurrentMission, m_OpenList.Count(), m_ClosedList.Count());
		// check all linked missions
		CASW_Campaign_Info::CASW_Campaign_Mission_t *pMission = pCI->GetMission(iCurrentMission);
		if (!pMission)
		{
			//Msg("Failed to get current mission in BuildCampaignRoute\n");
			return false;
		}
		//Msg("Current mission (%d) has %d links\n", iCurrentMission, pMission->m_Links.Count());
		for (int i=0;i<pMission->m_Links.Count();i++)
		{
			int iOtherMission = pMission->m_Links[i];
			//Msg("other mission[%d] is %d\n", i, iOtherMission);
			CASW_Campaign_Info::CASW_Campaign_Mission_t *pOtherMission = pCI->GetMission(pMission->m_Links[i]);
			if (!pOtherMission)
				continue;

			// check if it's on the open list
			int iOnOpenList = -1;
			for (int k=0;k<m_OpenList.Count();k++)
			{
				if (m_OpenList[k].iMission == iOtherMission)
				{
					//Msg("Other mission (%d) is on the open list\n", iOtherMission);
					iOnOpenList = k;
					break;
				}
			}
			// if not, check if it's already on the closed list
			if (iOnOpenList == -1)
			{
				int iOnClosedList = -1;
				for (int k=0;k<m_ClosedList.Count();k++)
				{
					if (m_ClosedList[k].iMission == iOtherMission)
					{
						//Msg("Other mission (%d) is on the closed list already, so ignoring\n", iOtherMission);
						iOnClosedList = k;
						break;
					}
				}
				if (iOnClosedList != -1)
				{
					continue;
				}
			}
			// if not, add it and calculate costs
			if (iOnOpenList == -1)
			{
				//Msg("other mission %d not on the open list, so adding it...", iOtherMission);
				campaign_route_node_t new_node;
				new_node.iMission = iOtherMission;
				new_node.iParentMission = iCurrentMission;				
				new_node.g = fCurrentCost + EstimateCost(pCI, iCurrentMission, iOtherMission);	// actual cost from start to here
				new_node.h = EstimateCost(pCI, iOtherMission, iEnd);
				new_node.f = new_node.g + new_node.h;
				m_OpenList.AddToTail(new_node);				
			}
			else		// if it is, check if going there from us is cheaper, if so update parent and costs
			{
				//Msg("other mission %d already on open list, checking costs\n", iOtherMission);
				float my_g_cost = fCurrentCost + EstimateCost(pCI, iCurrentMission, iOtherMission);
				if (my_g_cost < m_OpenList[iOnOpenList].g)
				{
					m_OpenList[iOnOpenList].iParentMission = iCurrentMission;
					m_OpenList[iOnOpenList].g = my_g_cost;
					m_OpenList[iOnOpenList].f = m_OpenList[iOnOpenList].g + m_OpenList[iOnOpenList].h;
				}
			}
		}
		iOverflow++;
	}
	//Msg("Error, BuildCampaignRoute overflow!\n");
	return false;
}

CASW_Campaign_Save::campaign_route_node_t* CASW_Campaign_Save::FindMissionInClosedList(int iMission)
{
	for (int i=0;i<m_ClosedList.Count();i++)
	{
		if (m_ClosedList[i].iMission == iMission)
		{
			return &m_ClosedList[i];
		}	
	}
	return NULL;
}

void CASW_Campaign_Save::DebugBuiltRoute()
{
	campaign_route_node_t* pNode = FindMissionInClosedList(m_iRouteDest);
	int iCount = 0;
	while (pNode != NULL && pNode->iMission != m_iRouteStart)
	{
		Msg("Node %d: Mission %d (Cost %f)\n", iCount++, pNode->iMission, pNode->f);
		pNode = FindMissionInClosedList(pNode->iParentMission);
	}	
}

void ASW_TestRoute_cc(const CCommand &args)
{
	if ( args.ArgC() < 3 )
	{
		Warning( "Usage: ASW_TestRoute [start mission index] [end mission index]\n" );
		return;
	}
	if (!ASWGameRules() || !ASWGameRules()->GetCampaignSave())
	{
		Msg("Must be playing a campaign game!\n");
		return;
	}
	int iStart = atoi(args[1]);
	int iEnd = atoi(args[2]);
	CASW_Campaign_Save* pSave = ASWGameRules()->GetCampaignSave();
	bool bFound = pSave->BuildCampaignRoute(iStart, iEnd);
	if (bFound)
	{		
		Msg("Found route:\n");
		pSave->DebugBuiltRoute();
	}
	else
	{
		Msg("No route found!\n");
	}
}
ConCommand ASW_TestRoute( "ASW_TestRoute", ASW_TestRoute_cc, "Tests loading a savegame", FCVAR_CHEAT );

void ASW_TestLoad_cc(const CCommand &args)
{
	if ( args.ArgC() < 2 )
	{
		Warning( "You must specify the name of the saved campaign game to resume playing.\n" );
		return;
	}
	CASW_Campaign_Save *pSave = new CASW_Campaign_Save();
	if (!pSave->LoadGameFromFile(args[1]))
	{
		Msg("LoadGameFromFile failed!\n");
	}
	else
		pSave->DebugInfo();
}
ConCommand ASW_TestLoad( "ASW_TestLoad", ASW_TestLoad_cc, "Tests loading a savegame", FCVAR_CHEAT );

void CASW_Campaign_Save::StartingCampaignVote()
{
	if (!ASWGameRules())
		return;
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	// clear votes and ready
	m_fVoteEndTime = 0;
	m_bNextMissionVoteEnded = false;
	for (int i=0;i<ASW_MAX_MISSIONS_PER_CAMPAIGN;i++)
	{
		m_NumVotes.Set(i, 0);
	}	
	for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
	{
		pGameResource->m_bPlayerReady.Set(i, false);
	}
}

void CASW_Campaign_Save::ForceNextMissionLaunch()
{
	if (!m_fVoteEndTime != 0)
	{	
		m_fVoteEndTime = gpGlobals->curtime + 6.0f;
	}
	SetThink( &CASW_Campaign_Save::VoteEndThink );
	SetNextThink( m_fVoteEndTime );
}

void CASW_Campaign_Save::VoteEndThink()
{
	SetThink(NULL);
	VoteEnded();
}

void CASW_Campaign_Save::VoteEnded()
{
	if (!ASWGameRules())
		return;

	if (!ASWGameRules()->GetCampaignInfo())
		return;
	Msg("CampVote ended\n");

	m_bNextMissionVoteEnded = true;
		
	CASW_Campaign_Info *pCI = ASWGameRules()->GetCampaignInfo();
	// decide which mission to launch
	// find the highest voted launchable,
	int iHighestMission = -1;
	int iHighestVotes = -1;
	CUtlVector<int> Draws;
	for (int i=0;i<pCI->GetNumMissions();i++)
	{
		//CASW_Campaign_Info::CASW_Campaign_Mission_t* pMission = pCI->
		if (m_MissionComplete[i] != 0)	// already complete
			continue;
		if (!IsMissionLinkedToACompleteMission(i, pCI) || i <= 0)	// launchable
			continue;
		if (m_NumVotes[i] > iHighestVotes)
		{
			iHighestVotes = m_NumVotes[i];
			Draws.Purge();
			Draws.AddToTail(i);
			iHighestMission = i;
		}
		else if (m_NumVotes[i] == iHighestVotes)
		{
			Draws.AddToTail(i);
		}
	}
	int iMission = iHighestMission;
	Msg("highest mission is %d draws is %d\n", iMission, Draws.Count());
	if (Draws.Count() > 1)
	{
		// some missions have equal votes, pick one at random
		int iPick = random->RandomInt(0, Draws.Count() - 1);
		iMission = Draws[iPick];
	}

	Msg("Moving to %d\n", iMission);
	// move there
	ASWGameRules()->RequestCampaignMove(iMission);
}

void CASW_Campaign_Save::PlayerDisconnected(CASW_Player *pPlayer)
{
	if (!ASWGameRules() || !pPlayer)
		return;
	if (ASWGameRules()->GetGameState() != ASW_GS_CAMPAIGNMAP)
		return;
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	if (m_bNextMissionVoteEnded)	// don't allow votes if the mission has already been chosen
		return;

	// if player is ready, that means he's already voted or a spectator
	int iPlayer = pPlayer->entindex() -1 ;
	if (iPlayer<0 ||iPlayer>ASW_MAX_READY_PLAYERS-1)
		return;
	if (pGameResource->IsPlayerReady(pPlayer->entindex()))
	{
		// subtract his old vote
		int iVotedFor = pGameResource->m_iCampaignVote[iPlayer];
		if (iVotedFor >=0 && iVotedFor < ASW_MAX_MISSIONS_PER_CAMPAIGN)
		{
			int iVotes = m_NumVotes[iVotedFor] - 1;
			m_NumVotes.Set(iVotedFor, iVotes);
		}
	}

	// check for ending the vote
	if (pGameResource->AreAllOtherPlayersReady(pPlayer->entindex()))
	{
		if ( gpGlobals->maxClients > 1 )
		{
			if (!m_fVoteEndTime != 0)
			{	
				m_fVoteEndTime = gpGlobals->curtime + 4.0f;
			}
			SetThink( &CASW_Campaign_Save::VoteEndThink );
			SetNextThink( m_fVoteEndTime );
		}
		else
		{
			VoteEnded();
		}
	}
}

void CASW_Campaign_Save::PlayerVote(CASW_Player* pPlayer, int iMission)
{
	if (!ASWGameRules() || !pPlayer || pPlayer->m_bRequestedSpectator)
		return;
	if (ASWGameRules()->GetGameState() != ASW_GS_CAMPAIGNMAP)
		return;
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	if (m_bNextMissionVoteEnded)	// don't allow votes if the mission has already been chosen
		return;

	// if player is ready, that means he's already voted or a spectator
	int iPlayer = pPlayer->entindex() -1 ;
	if (iPlayer<0 ||iPlayer>ASW_MAX_READY_PLAYERS-1)
		return;
	if (pGameResource->IsPlayerReady(pPlayer->entindex()))
	{
		// subtract his old vote
		int iVotedFor = pGameResource->m_iCampaignVote[iPlayer];
		if (iVotedFor >=0 && iVotedFor < ASW_MAX_MISSIONS_PER_CAMPAIGN)
		{
			int iVotes = m_NumVotes[iVotedFor] - 1;
			m_NumVotes.Set(iVotedFor, iVotes);
		}
	}
	if (iMission < 0 || iMission >= ASW_MAX_MISSIONS_PER_CAMPAIGN)
		return;

	// don't allow vote for this mission if this mission was already completed, or if it's the dropzone
	if (m_MissionComplete[iMission] != 0 || iMission <= 0)
		return;

	// make sure the mission they want to vote for is reachable
	if (!IsMissionLinkedToACompleteMission(iMission, ASWGameRules()->GetCampaignInfo()))
		return;	

	// add his vote to the chosen map
	pGameResource->m_iCampaignVote[iPlayer] = iMission;
	int iVotes = m_NumVotes[iMission] + 1;
	m_NumVotes.Set(iMission, iVotes);
	// flag him as ready
	pGameResource->m_bPlayerReady.Set(iPlayer, true);

	if (pGameResource->AreAllOtherPlayersReady(pPlayer->entindex()))
	{
		if ( gpGlobals->maxClients > 1 )
		{
			if (!m_fVoteEndTime != 0)
			{	
				m_fVoteEndTime = gpGlobals->curtime + 4.0f;
			}
			SetThink( &CASW_Campaign_Save::VoteEndThink );
			SetNextThink( m_fVoteEndTime );
		}
		else
		{
			VoteEnded();
		}
	}
}

void CASW_Campaign_Save::PlayerSpectating(CASW_Player* pPlayer)
{
	if (!ASWGameRules() || !pPlayer)
		return;
	if (ASWGameRules()->GetGameState() != ASW_GS_CAMPAIGNMAP)
		return;
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	if (m_bNextMissionVoteEnded)	// don't allow votes if the mission has already been chosen
		return;

	pPlayer->m_bRequestedSpectator = true;
	// if player is ready, that means he's already voted or a spectator
	int iPlayer = pPlayer->entindex() -1 ;
	if (iPlayer<0 ||iPlayer>ASW_MAX_READY_PLAYERS-1)
		return;
	if (pGameResource->IsPlayerReady(pPlayer->entindex()))
	{
		// subtract his old vote
		int iVotedFor = pGameResource->m_iCampaignVote[iPlayer];
		if (iVotedFor >=0 && iVotedFor < ASW_MAX_MISSIONS_PER_CAMPAIGN)
		{
			int iVotes = m_NumVotes[iVotedFor] - 1;
			m_NumVotes.Set(iVotedFor, iVotes);
		}
	}
	// clear his chosen mission and flag him as ready
	pGameResource->m_iCampaignVote[iPlayer] = -1;
	pGameResource->m_bPlayerReady.Set(iPlayer, true);

	if (pGameResource->AreAllOtherPlayersReady(pPlayer->entindex()))
	{
		if ( gpGlobals->maxClients > 1 )
		{
			if (!m_fVoteEndTime != 0)
			{	
				m_fVoteEndTime = gpGlobals->curtime + 4.0f;
			}
			SetThink( &CASW_Campaign_Save::VoteEndThink );
			SetNextThink( m_fVoteEndTime );
		}
		else
		{
			VoteEnded();
		}
	}
}

void CASW_Campaign_Save::SelectDefaultNextCampaignMission()
{
	if ( !ASWGameRules() || !ASWGameResource() )
		return;

	CASW_Campaign_Info *pInfo = ASWGameRules()->GetCampaignInfo();
	if ( !pInfo )
		return;

	// find the first valid mission selection in our list
	for ( int i = 1; i < pInfo->GetNumMissions(); i++ )		// skip first dummy entry
	{
		if ( !m_MissionComplete[i] && IsMissionLinkedToACompleteMission( i, pInfo ) )
		{
			ASWGameResource()->m_iNextCampaignMission = i;
			return;
		}
	}
}

void CASW_Campaign_Save::OnMarineKilled()
{
	m_iNumDeaths++;
	SaveGameToFile();
}