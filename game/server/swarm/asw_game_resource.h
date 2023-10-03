#ifndef ASW_GAME_RESOURCE_H
#define ASW_GAME_RESOURCE_H
#pragma once

#include "baseentity.h"
#include "asw_shareddefs.h"
#include "asw_marine_profile.h"

class CASW_Marine_Resource;
class CASW_Objective;
class CASW_Player;
class CASW_Scanner_Info;
class CASW_Campaign_Info;
class CASW_Campaign_Save;
class CASW_Marine_Profile;
class CASW_Marine;

#define ASW_LEADERID_LEN 128

// This class holds central game data that all clients should know about

class CASW_Game_Resource : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Game_Resource, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Game_Resource();
	virtual ~CASW_Game_Resource();	

	// info about each marine
	CNetworkArray( EHANDLE, m_MarineResources, ASW_MAX_MARINE_RESOURCES );

	// mission objectives for this level
	CNetworkArray( EHANDLE, m_Objectives, ASW_MAX_OBJECTIVES );

	// which marines from the roster have been selected	
	CNetworkVar( bool, m_bOneMarineEach );
	CNetworkVar( int, m_iMaxMarines );
	void SetMaxMarines(int iMaxMarines, bool bOneEach) { m_iMaxMarines = iMaxMarines; m_bOneMarineEach = bOneEach; }

	// ready status of players
	CNetworkArray( bool, m_bPlayerReady, ASW_MAX_READY_PLAYERS );
	bool IsPlayerReady(CASW_Player *pPlayer);
	bool IsPlayerReady(int iPlayerEntIndex);
	int m_iCampaignVote[ASW_MAX_READY_PLAYERS];	// which campaign map this player has chosen, if any

	// checking if we can start the game yet or not
	bool AreAllOtherPlayersReady(int iPlayerEntIndex);

	// which player is the leader
	CNetworkHandle (CASW_Player, m_Leader);
	CNetworkVar(int, m_iLeaderIndex);
	void RememberLeaderID();
	const char* GetLastLeaderNetworkID();
	void SetLastLeaderNetworkID(const char* szID);

	CASW_Player* GetLeader();

	CASW_Scanner_Info* GetScannerInfo();
	CNetworkHandle(CASW_Scanner_Info, m_hScannerInfo);

	virtual int	ShouldTransmit( const CCheckTransmitInfo *pInfo );
	
	CASW_Objective* GetObjective(int i);
	void FindObjectivesOfClass(const char *szClass);
	void FindObjectives();		// searches through all entities to find the objectives
	int GetMarineResourceIndex( CASW_Marine_Resource *pMR );
	CASW_Marine_Resource* GetFirstMarineResourceForPlayer( CASW_Player *pPlayer );	// returns the first marine resource controlled by this player

	bool IsRosterSelected(int i);
	bool IsRosterReserved(int i);
	void SetRosterSelected(int i, int iSelected);	// 0 = unselected 1 = selected 2 = reserved
	bool AtLeastOneMarine();	// is at least one marine selected?
	void RemoveAMarine();	// deselects a marine
	void RemoveAMarineFor(CASW_Player *pPlayer);	// deselects a marine
	int m_iNumMarinesSelected;

	void SetLeader(CASW_Player *pPlayer);

	CASW_Marine_Resource* GetMarineResource(int i);
	bool AddMarineResource( CASW_Marine_Resource* m, int nPreferredSlot=-1 );
	void DeleteMarineResource( CASW_Marine_Resource *m );		

	int GetMaxMarineResources() { return ASW_MAX_MARINE_RESOURCES; }
	int GetNumMarines(CASW_Player *pPlayer, bool bAliveOnly=false);	// returns how many marines this player has selected (if player is null, it'll return the total)

	int m_NumObjectives;

	bool IsOfflineGame() const { return m_bOfflineGame || ( gpGlobals->maxClients == 1 ); }
	CNetworkVar( bool, m_bOfflineGame );

	int IsCampaignGame() { return m_iCampaignGame; }
	CNetworkVar(int, m_iCampaignGame);	// is this a campaign game?  -1 = unknown, 0 = single mission, 1 = campaign
	CASW_Campaign_Save* GetCampaignSave();
	CASW_Campaign_Save* CreateCampaignSave();
	const char* GetCampaignSaveName() { return m_szCampaignSaveName; }
	CNetworkHandle(CASW_Campaign_Save, m_hCampaignSave);
	char m_szCampaignSaveName[64]; // save name of our current campaign game

	CASW_Marine* FindMarineByVoiceType( ASW_Voice_Type voice );

	// skills
	int GetSlotForSkill( int nProfileIndex, ASW_Skill nSkillIndex );
	int GetMarineSkill( int nProfileIndex, int nSkillSlot );
	int GetMarineSkill( CASW_Marine_Resource *m, int nSkillSlot );
	void UpdateMarineSkills( CASW_Campaign_Save *pCampaign );
	bool SetMarineSkill( int nProfileIndex, int nSkillSlot, int nValue );

	// player medals
	CNetworkArray( string_t, m_iszPlayerMedals, ASW_MAX_READY_PLAYERS );

	// voting
	CNetworkArray(int, m_iKickVotes, ASW_MAX_READY_PLAYERS);
	CNetworkArray(int, m_iLeaderVotes, ASW_MAX_READY_PLAYERS);

	// returns current number of alive (non-KOed players)
	int CountAllAliveMarines( void );

	// returns count of all marines in these bounds
	int EnumerateMarinesInBox(Vector &mins, Vector &maxs);
	CASW_Marine* EnumeratedMarine(int i);
	CASW_Marine* m_pEnumeratedMarines[ASW_MAX_MARINE_RESOURCES];
	int m_iNumEnumeratedMarines;

	int GetAliensKilledInThisMission();

	CASW_Campaign_Info* m_pCampaignInfo;

	// money
	int  GetMoney() { return m_iMoney; }
	void SetMoney( int iAmount ) { m_iMoney += iAmount; }
	void IncrementMoney( int iAmount ) { m_iMoney += iAmount; }
	CNetworkVar( int, m_iMoney );

	// random map generation
	CNetworkVar( float, m_fMapGenerationProgress );
	CNetworkString( m_szMapGenerationStatus, 128 );
	CNetworkVar( int, m_iRandomMapSeed );					// if set clients, will begin generating a random map based on this seed

	CNetworkVar( int, m_iNextCampaignMission );

	CNetworkVar( int, m_nDifficultySuggestion );	// 0 = none, 1 = harder, 2 = easier

	int m_iStartingEggsInMap;
	int m_iEggsKilled;
	int m_iEggsHatched;		// how many eggs have hatched so far
	int m_iAliensKilledWithDamageAmp;
	bool m_bAwardedDamageAmpAchievement;
	int m_iElectroStunnedAliens;

	void OnMissionFailed( void );
	void OnMissionCompleted( bool bWellDone );
	static int s_nNumConsecutiveFailures;
	static bool s_bLeaderGivenDifficultySuggestion;

private:
	CNetworkArray( int, m_iRosterSelected, ASW_NUM_MARINE_PROFILES );	

	// skillslots
	CNetworkArray(int, m_iSkillSlot0, ASW_NUM_MARINE_PROFILES);
	CNetworkArray(int, m_iSkillSlot1, ASW_NUM_MARINE_PROFILES);
	CNetworkArray(int, m_iSkillSlot2, ASW_NUM_MARINE_PROFILES);
	CNetworkArray(int, m_iSkillSlot3, ASW_NUM_MARINE_PROFILES);
	CNetworkArray(int, m_iSkillSlot4, ASW_NUM_MARINE_PROFILES);
	CNetworkArray(int, m_iSkillSlotSpare, ASW_NUM_MARINE_PROFILES);
};

extern CASW_Game_Resource *g_pASWGameResource;

inline CASW_Game_Resource* ASWGameResource()
{
	return g_pASWGameResource;
}

#endif /* ASW_GAME_RESOURCE_H */