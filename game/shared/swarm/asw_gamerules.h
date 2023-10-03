//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: Game rules for Alien Swarm
//
//=============================================================================

#ifndef ASW_GAMERULES_H
#define ASW_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "gamerules.h"
#include "singleplay_gamerules.h"
#include "asw_shareddefs.h"

#ifdef CLIENT_DLL
	#define CAlienSwarm C_AlienSwarm
	#define CAlienSwarmProxy C_AlienSwarmProxy
	#define CASW_Game_Resource C_ASW_Game_Resource
	#define CASW_Marine C_ASW_Marine
	#define CASW_Player C_ASW_Player
	#define CASW_Pickup C_ASW_Pickup
	#define CASW_Powerup C_ASW_Powerup
	#define CASW_Marine_Resource C_ASW_Marine_Resource
	#define CASW_Campaign_Save C_ASW_Campaign_Save
	class CASW_Game_Resource;
#else
	#include "asw_map_reset_filter.h"
	#include "asw_medals.h"
	class CASW_Game_Resource;
	class CASW_Alien;
	class CASW_Mission_Manager;	
	class CASW_Debrief_Stats;
	class CASW_Equip_Req;
	class CASW_Info_Heal;
	class IASW_Map_Builder;
	class CASW_Weapon;
#endif

class CASW_Marine_Resource;
class CASW_Campaign_Info;
class CASW_Campaign_Save;
class CASW_Ammo;

class CAlienSwarmProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CAlienSwarmProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

	int m_iSpeedrunTime;

#ifdef CLIENT_DLL
	virtual void OnDataChanged( DataUpdateType_t updateType );
#endif
};

class CASW_Player;
class CASW_Marine;
class CASW_Pickup;
class CASW_Powerup;

// special game modes
enum
{
	ASW_SM_CARNAGE	=				(1<<0),	// More aliens with less health
	ASW_SM_UBER		=				(1<<1),	// Occasional large slow big health aliens
	ASW_SM_HARDCORE	=				(1<<2),	// Instakill sites, no friendly fire damage reduction, etc.
};

// Faction defines
#define FACTION_MARINES				( LAST_SHARED_FACTION + 1 )
#define FACTION_ALIENS				( LAST_SHARED_FACTION + 2 )
#define FACTION_BAIT				( LAST_SHARED_FACTION + 3 )
#define LAST_ASW_FACTION			(FACTION_BAIT)
#define NUM_ASW_FACTIONS			(LAST_ASW_FACTION + 1)

class CAlienSwarm : public CSingleplayRules
{
public:
	DECLARE_CLASS( CAlienSwarm, CSingleplayRules );

	virtual void LevelInitPostEntity();

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

	CAlienSwarm();
	virtual ~CAlienSwarm();
	
	float GetMarineDeathCamInterp( void );

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.

	CAlienSwarm();
	virtual ~CAlienSwarm();
	
	virtual void			Precache( void );
	virtual void			Think( void );
	virtual const char *GetGameDescription( void ) { return "Alien Swarm"; }
	virtual void			OnServerHibernating();
	
	// briefing roster functions
	virtual bool			RosterSelect( CASW_Player *pPlayer, int RosterIndex, int nPreferredSlot=-1 );
	virtual void			RosterDeselect( CASW_Player *pPlayer, int RosterIndex);
	virtual void			ReassignMarines(CASW_Player *pPlayer);	// reassigns all this player's marines to someone else
	virtual void			RosterDeselectAll( CASW_Player *pPlayer );
	virtual void			SetMaxMarines( CASW_Player *pException = NULL );
	virtual void			ReviveDeadMarines();
	virtual void			EnforceFairMarineRules();
	
	virtual void			ReserveMarines();
	virtual void			UnreserveMarines();
	virtual void			AutoselectMarines(CASW_Player *pPlayer);
	float m_fReserveMarinesEndTime;

	// loadout/equip
	virtual void			LoadoutSelect( CASW_Player *pPlayer, int iRosterIndex, int iInvSlot, int iEquipIndex);
	virtual bool			CanHaveAmmo( CBaseCombatCharacter *pPlayer, int iAmmoIndex );
	void GiveStartingWeaponToMarine(CASW_Marine* pMarine, int iEquipIndex, int iSlot);	// gives the specified marine the specified starting gun and default ammo
	void AddBonusChargesToPickups();
	
	// spawning/connecting
	CBaseEntity* GetMarineSpawnPoint(CBaseEntity *pStartEntity);	
	bool IsValidMarineStart(CBaseEntity *pSpot);
	//virtual bool			ClientCommand( const char *pcmd, CBaseEntity *pEdict );
	virtual void PlayerThink( CBasePlayer *pPlayer );
	virtual void			PlayerSpawn( CBasePlayer *pPlayer );
	virtual bool			ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen );
	virtual void			ClientDisconnected( edict_t *pClient );
	virtual bool			ShouldTimeoutClient( int nUserID, float flTimeSinceLastReceived ); // return true to disconnect client due to timeout (used to do stricter timeouts when the game is sure the client isn't loading a map)
	virtual void			ClientSettingsChanged( CBasePlayer *pPlayer );	
	virtual bool			ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void			ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues );
	void OnPlayerFullyJoined( CASW_Player *pPlayer );

	// powerups
	virtual void DropPowerup( CBaseEntity *pSource, const CTakeDamageInfo &info, const char *pszSourceClass );
	float m_fLastPowerupDropTime;

	// flags the current mission as complete, saves the game and launches the next map
	virtual void CampaignSaveAndShowCampaignMap(CASW_Player* pPlayer, bool bForce);
	virtual bool RequestCampaignMove(int iTargetMission);
	virtual bool RequestCampaignLaunchMission(int iTargetMission);
	virtual void ChangeLevel_Campaign(const char *map);		// issues a changelevel command with the campaign argument and save name

	// marines spending and undoing skills	
	virtual bool SpendSkill(int iProfileIndex, int iSkillIndex);
	virtual bool SkillsUndo(CASW_Player *pPlayer, int iProfileIndex);

	// Resurrection
	void Resurrect( CASW_Marine_Resource * RESTRICT pMR, CASW_Marine *pRespawnNearMarine );

	// cheats
	bool m_bMarineInvuln;
	virtual void MarineInvuln();
	virtual void MarineInvuln( bool bInvuln );
	void AllowBriefing();
	virtual void OnSVCheatsChanged();	// called from the engine when sv_cheats is changed

	// Ammo
	//virtual void			PlayerThink( CBasePlayer *pPlayer );
	//virtual float			GetAmmoDamage( CBaseEntity *pAttacker, CBaseEntity *pVictim, int nAmmoType );
	
	// stim pack and time scaling
	virtual void			StartStim( float duration, CBaseEntity *pSource );	// activates X seconds of slomo
	virtual void			StopStim();		// brings the stim time down to 1 second, so time starts fading back to normal
	void ThinkUpdateTimescale() RESTRICT;

	// AI related
	virtual void			InitDefaultAIRelationships();
	virtual int				NumEntityClasses() const	{ return LAST_ASW_ENTITY_CLASS; }
	virtual int				NumFactions() const	{ return NUM_ASW_FACTIONS; }

	virtual void			MarineKilled( CASW_Marine *pMarine, const CTakeDamageInfo &info );
	virtual void			AlienKilled(CBaseEntity *pAlien, const CTakeDamageInfo &info);

	// mission
	virtual void			RequestStartMission(CASW_Player *pPlayer);
	virtual void			StartMission();
	virtual void			RestartMission( CASW_Player *pPlayer, bool bForce = false );
	virtual void			RestartMissionCountdown( CASW_Player *pPlayer );			// restart a mission mid-game, with a 5 second countdown
	virtual void			CheatCompleteMission();
	CASW_Mission_Manager* GetMissionManager();
	CASW_Mission_Manager* m_pMissionManager;
	virtual void MissionComplete(bool bSuccess);
	virtual void RemoveAllAliens();
	virtual void RemoveNoisyWeapons();
	void ScheduleTechFailureRestart( float flRestartBeginTime ) { if ( m_flTechFailureRestartTime == 0 ) { m_flTechFailureRestartTime = flRestartBeginTime; } }
	void CheckTechFailure();
	float m_fRemoveAliensTime;
	bool m_bShouldStartMission;
	float m_flTechFailureRestartTime;

	virtual void BroadcastMapLine(CASW_Player *pPlayer, int linetype, int world_x, int world_y);

	// used by the chatter class
	virtual void BlipSpeech(int iMarine);
	float GetChatterTime() { return m_fNextChatterTime; }
	float GetIncomingChatterTime() { return m_fNextIncomingChatterTime; }
	float GetNoAmmoChatterTime() { return m_fLastNoAmmoChatterTime; }
	void SetChatterTime(float f) { m_fNextChatterTime = f; }
	void SetIncomingChatterTime(float f) { m_fNextIncomingChatterTime = f; }
	void SetNoAmmoChatterTime(float f) { m_fLastNoAmmoChatterTime = f; }
	float m_fNextChatterTime;
	float m_fNextIncomingChatterTime;
	float m_fLastNoAmmoChatterTime;
	float m_fLastBlipSpeechTime;
	bool m_bPlayedBlipSpeech;

	// map entity filter
	CASW_Map_Reset_Filter m_MapResetFilter;
	int m_iMissionRestartCount;

	// custom version of radius damage to hurt marines a little less
	void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore );
	void FreezeAliensInRadius( CBaseEntity *pInflictor, float flFreezeAmount, const Vector &vecSrcIn, float flRadius );
	void StumbleAliensInRadius( CBaseEntity *pInflictor, const Vector &vecSrcIn, float flRadius );
	void ShockNearbyAliens( CASW_Marine *pMarine, CASW_Weapon *pWeaponSource );
			
	// skill/difficulty level stuff
	virtual bool IsSkillLevel( int iLevel ) { return GetSkillLevel() == iLevel; }
	virtual void OnSkillLevelChanged( int iNewLevel );
	virtual void SetSkillLevel( int iLevel )
	{
		int oldLevel = m_iSkillLevel; 

		if ( iLevel < 1 )
		{
			iLevel = 1;
		}
		else if ( iLevel > 5 )
		{
			iLevel = 5; 
		}

		m_iSkillLevel = iLevel;

		if( m_iSkillLevel != oldLevel )
		{
			OnSkillLevelChanged( m_iSkillLevel );
		}
	}
	void RequestSkill( CASW_Player *pPlayer, int nSkill );
	virtual void RequestSkillDown(CASW_Player *pPlayer);
	virtual void RequestSkillUp(CASW_Player *pPlayer);
	virtual void FindAndModifyAlienHealth(const char *szClass);
	virtual float ModifyAlienDamageBySkillLevel( float flDamage );
	virtual float ModifyAlienHealthBySkillLevel(float health);
	virtual int GetLowestSkillLevelPlayed();	// returns the lowest skill level a mission was completed on, in a campaign game

	// intro
	bool m_bStartedIntro;

	// medals
	CASW_Medals m_Medals;
	int m_iNumGrubs;
	CHandle<CASW_Debrief_Stats> m_hDebriefStats;
	int GetSpeedrunTime( void );	

	// voting
	void ClearLeaderKickVotes(CASW_Player *pPlayer, bool bClearLeader=true, bool bClearKick=true);	// clears out any kick/leader votes aimed at this player	
	void SetLeaderVote(CASW_Player *pPlayer, int iPlayerIndex);
	void SetKickVote(CASW_Player *pPlayer, int iPlayerIndex);
	// campaign/saved/mission voting
	void StartVote(CASW_Player *pPlayer, int iVoteType, const char *szVoteName, int nCampaignIndex = -1);
	void CastVote(CASW_Player *pPlayer, bool bVoteYes);
	void RemoveVote(CASW_Player *pPlayer);
	void UpdateVote();
	CUtlVector<string_t> m_PlayersVoted;	// IDs of players that have already voted
	char m_szCurrentVoteName[128];
	const char* GetCurrentVoteName() { return m_szCurrentVoteName; }

	// forced readiness	
	void SetForceReady(int iForceReadyType);
	void CheckForceReady();
	void FinishForceReady();
	int m_iForceReadyType;
	float m_fForceReadyTime;
	int m_iForceReadyCount;

	// chatter
	bool m_bDoneCrashShieldbugConv;
	float m_fNextWWKillConv;
	float m_fNextCompliment;
	bool m_bSargeAndJaeger;	// are these marines on this mission?
	bool m_bWolfeAndWildcat;	

	void StartAllAmbientSounds();
	void StopAllAmbientSounds();
	virtual bool AllowSoundscapes( void );

	float m_fMissionStartedTime;
	float m_fLaunchOutroMapTime;

	// equip req
	CHandle<CASW_Equip_Req> m_hEquipReq;
	void ReportMissingEquipment();

	void ReportNeedTwoPlayers();

	bool m_bCheckAllPlayersLeft;
	float m_fEmptyServerTime;	

	// stepped launching
	virtual void UpdateLaunching();
	virtual bool SpawnNextMarine();	// spawns a marine for each entry in the marine resource list
	virtual bool SpawnMarineAt( CASW_Marine_Resource *pMR, const Vector &vecPos, const QAngle &angFacing, bool bResurrection = false );
	virtual void VerifySpawnLocation( CASW_Marine *pMarine );
	int m_iMarinesSpawned;
	float m_fNextLaunchingStep;
	CBaseEntity* m_pSpawningSpot;

	// misc
	void ExplodedLevel();
	void GrubSpawned(CBaseEntity *pGrub) { m_iNumGrubs++; }	
	float m_fLastFireTime;	// last time a marine fired a gun (used for avoiding casual chatter in a battle)
	void BroadcastSound( const char *sound );
	virtual const char *GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer ) { return ""; }

	virtual edict_t *DoFindClientInPVS( edict_t *pEdict, unsigned char *pvs, unsigned pvssize );

	virtual int	DefaultFOV( void ) { return 75; }
	void SetInfoHeal( CASW_Info_Heal *pInfoHeal );
	CASW_Info_Heal *GetInfoHeal();
	CHandle<CASW_Info_Heal> m_hInfoHeal;

	float m_fObjectiveSlowDownEndTime;

#endif

	// stim music
	bool	ShouldPlayStimMusic();
	float	GetStimEndTime() { return m_flStimEndTime; }
	float   GetStartStimTime() { return m_flStimStartTime; }
	CBaseEntity* GetStimSource() { return m_hStartStimPlayer.Get(); }
	bool	ShouldForceStylinCam() { return m_bForceStylinCam; }
	bool	ShouldShowCommanderFace() { return m_bShowCommanderFace; }
	float	GetRestartingMissionTime() { return m_flRestartingMissionTime; }
	CNetworkVar(EHANDLE, m_hStartStimPlayer);
	CNetworkVar(float, m_flStimEndTime);		// time at which stims will end
	CNetworkVar(float, m_flStimStartTime);
	CNetworkVar(float, m_fPreventStimMusicTime);	// while under this time, clients won't play stim music
	CNetworkVar(bool, m_bForceStylinCam);
	CNetworkVar(bool, m_bShowCommanderFace);
	CNetworkVar(float, m_flRestartingMissionTime);

	// marine death cams
	CNetworkVar( float, m_fMarineDeathTime );
	CNetworkVar( Vector, m_vMarineDeathPos );
	CNetworkVar( int, m_nMarineForDeathCam );
#ifdef CLIENT_DLL
	int m_nOldMarineForDeathCam;
	int m_nMarineDeathCamStep;
	float m_fMarineDeathCamRealtime;
	float m_fDeathCamYawAngleOffset;
	CHandle< C_BaseAnimating > m_hMarineDeathRagdoll;
#endif

	// voting
	CNetworkString(m_szCurrentVoteDescription, 128);
	CNetworkString(m_szCurrentVoteMapName, 128);
	CNetworkString(m_szCurrentVoteCampaignName, 128);
	CNetworkVar(int, m_iCurrentVoteYes);
	CNetworkVar(int, m_iCurrentVoteNo);
	CNetworkVar(int, m_iCurrentVoteType);
	CNetworkVar(float, m_fVoteEndTime);
	
	int GetCurrentVoteType() { return m_iCurrentVoteType; }
	int GetCurrentVoteYes() { return m_iCurrentVoteYes; }
	int GetCurrentVoteNo() { return m_iCurrentVoteNo; }
	const char* GetCurrentVoteMapName() { return m_szCurrentVoteMapName; }
	const char* GetCurrentVoteDescription() { return m_szCurrentVoteDescription; }
	const char* GetCurrentVoteCampaignName() { return m_szCurrentVoteCampaignName; }
	int GetCurrentVoteTimeLeft() { return m_fVoteEndTime - gpGlobals->curtime; }

	CNetworkVar( float, m_fBriefingStartedTime );

	// skills
	virtual bool CanSpendPoint(CASW_Player *pPlayer, int iProfileIndex, int iSkillIndex);
	virtual void RefreshSkillData ( bool forceUpdate );

	// difficulty
	virtual int GetSkillLevel() { return m_iSkillLevel; }		// skill level (expanded HL2 style: 1 = easy, 2 = normal, 3 = hard, 4 = insane, 5 = imba )
	CNetworkVar(int, m_iSkillLevel);	// 1 = easy, 2 = normal, 3 = hard, 4 = insane, 5 = imba
	int GetMissionDifficulty() { return m_iMissionDifficulty; }	// overall difficulty of the mission from 2-10, based on skill level and campaign modifier	
	CNetworkVar(int, m_iMissionDifficulty);	
	CNetworkVar(bool, m_bCheated);
	
	// pickups
	virtual bool MarineCanPickup(CASW_Marine_Resource* pMarineResource, const char* szWeaponClass, const char* szSwappingClass=NULL);
	bool MarineCanPickupAmmo(CASW_Marine *pMarine, CASW_Ammo *pAmmo);
	bool MarineCanPickupPowerup(CASW_Marine *pMarine, CASW_Powerup *pPowerup);
	const char* GetPickupDenial() { return m_szPickupDenial; }
	virtual bool MarineHasRoomInAmmoBag(CASW_Marine *pMarine, int iAmmoIndex);

	// game state
	virtual int	GetGameState() { return m_iGameState; }
	virtual void SetGameState(int iNewState) { m_iGameState = iNewState; }
	CNetworkVar(unsigned char, m_iGameState);
#ifdef CLIENT_DLL

	virtual void OnDataChanged( DataUpdateType_t updateType );
	unsigned char m_iPreviousGameState;
#endif

	// misc
	virtual void CreateStandardEntities( void );	
	virtual bool IsMultiplayer();	
	bool IsOfflineGame();
	bool CanFlareAutoaimAt(CASW_Marine* pMarine, CBaseEntity *pEntity);
	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );
	
	// mission
	virtual bool GetMissionSuccess() { return m_bMissionSuccess; }
	virtual bool GetMissionFailed() { return m_bMissionFailed; }
	CNetworkVar(bool, m_bMissionRequiresTech);
	CNetworkVar(bool, m_bMissionSuccess);
	CNetworkVar(bool, m_bMissionFailed);

	// fail advice
	CNetworkVar(int, m_nFailAdvice);

	const char* GetFailAdviceText( void );
	
	// total amount of damage given to a marine by infestation (based on difficulty)
	int TotalInfestDamage();

	virtual bool	Damage_IsTimeBased( int iDmgType );			// Damage types that are time-based.
	virtual bool	Damage_ShouldGibCorpse( int iDmgType );		// Damage types that gib the corpse.
	virtual bool	Damage_NoPhysicsForce( int iDmgType );		// Damage types that don't have to supply a physics force & position.
	virtual bool	Damage_ShouldNotBleed( int iDmgType );		// Damage types that don't make the player bleed.
	//Temp: These will go away once DamageTypes become enums.
	virtual int		Damage_GetTimeBased( void );				// Actual bit-fields.
	virtual int		Damage_GetShouldGibCorpse( void );
	virtual int		Damage_GetNoPhysicsForce( void );
	virtual int		Damage_GetShouldNotBleed( void );

	// campaign related
	int IsCampaignGame();	// -1 = unknown, 0 = single mission, 1 = campaign game
	int CampaignMissionsLeft();	
	CASW_Campaign_Info* GetCampaignInfo();
	CASW_Campaign_Save* GetCampaignSave();	

	// special game modes
#ifndef CLIENT_DLL
	void SetInitialGameMode();
	void SetCarnageMode(bool bCarnageMode);
	void SetUberMode(bool bUberMode);
	void SetHardcoreMode(bool bHardcoreMode);

	void StartTutorial(CASW_Player *pPlayer);

	bool ShouldQuickStart() { return m_bQuickStart; }
	bool m_bQuickStart;

#endif
	bool IsCarnageMode() { return (m_iSpecialMode & ASW_SM_CARNAGE) != 0; }	
	bool IsUberMode() { return (m_iSpecialMode & ASW_SM_UBER) != 0; }	
	bool IsHardcoreMode() { return (m_iSpecialMode & ASW_SM_HARDCORE) != 0; }	
	int GetUnlockedModes() { return m_iUnlockedModes; }
	CNetworkVar(int, m_iSpecialMode);
	CNetworkVar(int, m_iUnlockedModes);

	virtual bool IsTopDown() { return true; }
	virtual const QAngle& GetTopDownMovementAxis();

	// special maps
	bool IsTutorialMap() { return m_bIsTutorial; }
	bool IsIntroMap() { return m_bIsIntro; }
	bool IsOutroMap() { return m_bIsOutro; }
	bool IsLobbyMap() { return m_bIsLobby; }
	static bool IsHardcoreFF();
	static bool IsOnslaught();

	bool m_bIsTutorial;
	bool m_bIsIntro;
	bool m_bIsOutro;
	bool m_bIsLobby;		// lobby map is a temporary map that dedicated servers load into.  We detect that and start a new campaign game.
	
private:
	char m_szPickupDenial[128];

#ifndef CLIENT_DLL
	IASW_Map_Builder *m_pMapBuilder;
#endif
};

enum ASW_GameState
{
	ASW_GS_NONE = 0,
	ASW_GS_BRIEFING = 1,		// causes clients to launch their briefing frame
	ASW_GS_LAUNCHING = 2,		// server is spawning and equipping marines
	ASW_GS_INGAME = 3,
	ASW_GS_DEBRIEF = 4,			// causes clients to launch their debrief frame
	ASW_GS_CAMPAIGNMAP = 5,		// causes clients to launch their campaign map
	ASW_GS_OUTRO = 6,			// todo: make clients show the credits scroller
};

enum ASW_Force_Ready
{
	ASW_FR_NONE = 0,
	ASW_FR_BRIEFING = 1,
	ASW_FR_CONTINUE = 2,
	ASW_FR_RESTART = 3,
	ASW_FR_CAMPAIGN_MAP = 4,
	ASW_FR_INGAME_RESTART = 5,
};

//-----------------------------------------------------------------------------
// Gets us at the Alien Swarm game rules
//-----------------------------------------------------------------------------
inline CAlienSwarm* ASWGameRules()
{
	return static_cast<CAlienSwarm*>(g_pGameRules);
}



#endif // ASW_GAMERULES_H
