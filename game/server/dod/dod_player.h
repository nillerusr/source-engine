//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_PLAYER_H
#define DOD_PLAYER_H
#pragma once


#include "basemultiplayerplayer.h"
#include "server_class.h"
#include "dod_playeranimstate.h"
#include "dod_shareddefs.h"
#include "dod_player_shared.h"
#include "unisignals.h"
#include "dod_statmgr.h"
#include "utlmap.h"
#include "steam/steam_gameserver.h"
#include "hintsystem.h"

// Function table for each player state.
class CDODPlayerStateInfo
{
public:
	DODPlayerState m_iPlayerState;
	const char *m_pStateName;
	
	void (CDODPlayer::*pfnEnterState)();	// Init and deinit the state.
	void (CDODPlayer::*pfnLeaveState)();
	void (CDODPlayer::*pfnPreThink)();	// Do a PreThink() in this state.
};

class CDODPlayer;

//=======================================
//Record of either damage taken or given.
//Contains the player name that we hurt or that hurt us,
//and the total damage
//=======================================
class CDamageRecord
{
public:
	CDamageRecord( const char *pszName, int iLifeID, int iDamage )
	{
		Q_strncpy( m_szPlayerName, pszName, MAX_PLAYER_NAME_LENGTH );
		m_iDamage = iDamage;
		m_iNumHits = 1;
		m_iLifeID = iLifeID;
	}

	void AddDamage( int iDamage )
	{
		m_iDamage += iDamage;
		m_iNumHits++;
	}

	char *GetPlayerName( void ) { return m_szPlayerName; }
	int GetDamage( void ) { return m_iDamage; }
	int GetNumHits( void ) { return m_iNumHits; }
	int GetLifeID( void ) { return m_iLifeID; }

private:
	char m_szPlayerName[MAX_PLAYER_NAME_LENGTH];
	int m_iLifeID;		// life ID of the player when this damage was done
	int m_iDamage;		//how much damage was done
	int m_iNumHits;		//how many hits
};

#define SIGNAL_CAPTUREAREA (1<<0)

class CDODBombTarget;

class CDODPlayerStatProperty
{
	DECLARE_CLASS_NOBASE( CDODPlayerStatProperty );

public:
	CDODPlayerStatProperty()
	{
		m_iCurrentLifePlayerClass = -1;
		m_bRecordingStats = false;
		ResetPerLifeStats();	
	}

	~CDODPlayerStatProperty() {}

	void SetClassAndTeamForThisLife( int iPlayerClass, int iTeam );

	void IncrementPlayerClassStat( DODStatType_t statType, int iValue = 1 );

	void IncrementWeaponStat( DODWeaponID iWeaponID, DODStatType_t statType, int iValue = 1 );

	// reset per life stats
	void ResetPerLifeStats( void );

	// send this life's worth of data to the client
	void SendStatsToPlayer( CDODPlayer *pPlayer );

private:
	
	bool m_bRecordingStats;	// not recording until we get a valid class. stop recording when we join spectator

	int m_iCurrentLifePlayerClass;
	int m_iCurrentLifePlayerTeam;

	// single life's worth of player stats
	dod_stat_accumulator_t m_PlayerStatsPerLife;	

	// single life's worth of weapon stats
	dod_stat_accumulator_t m_WeaponStatsPerLife[WEAPON_MAX];
	bool m_bWeaponStatsDirty[WEAPON_MAX];
};




//=============================================================================
// >> Day of Defeat player
//=============================================================================
class CDODPlayer : public CBaseMultiplayerPlayer
{
public:
	DECLARE_CLASS( CDODPlayer, CBaseMultiplayerPlayer );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CDODPlayer();
	~CDODPlayer();

	static CDODPlayer *CreatePlayer( const char *className, edict_t *ed );
	static CDODPlayer* Instance( int iEnt );

	// This passes the event to the client's and server's CPlayerAnimState.
	void			DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );
	void			SetupBones( matrix3x4_t *pBoneToWorld, int boneMask );

	virtual void	Precache();
	void			PrecachePlayerModel( const char *szPlayerModel );
	virtual void	Spawn();
	virtual void	InitialSpawn( void );
		
	virtual void	CheatImpulseCommands( int iImpulse );
	virtual void	PlayerRunCommand( CUserCmd *ucmd, IMoveHelper *moveHelper );

	virtual void	PreThink();
	virtual void	PostThink();

	virtual int		OnTakeDamage( const CTakeDamageInfo &inputInfo );
	virtual int		OnTakeDamage_Alive( const CTakeDamageInfo &info );

	virtual void	Event_Killed( const CTakeDamageInfo &info );
	virtual void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	void			Pain( void );
	void			OnDamagedByExplosion( const CTakeDamageInfo &info );
	void			OnDamageByStun( const CTakeDamageInfo &info );
	void			DeafenThink( void );

	virtual void	UpdateGeigerCounter( void ) {}
	virtual void	CheckTrainUpdate( void ) {}

	virtual void	CreateViewModel( int viewmodelindex = 0 );

	virtual bool	SetObserverMode(int mode); // sets new observer mode, returns true if successful
	virtual bool	ModeWantsSpectatorGUI( int iMode ) { return ( iMode != OBS_MODE_DEATHCAM && iMode != OBS_MODE_FREEZECAM ); }

		
	// from CBasePlayer
	void			SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize );

	CBaseEntity*	EntSelectSpawnPoint();

	void			ChangeTeam( int iTeamNum );

	bool			CanMove( void ) const;

	virtual void	SharedSpawn();

	void CheckProneMoveSound( int groundspeed, bool onground );
	virtual void UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  );
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );

	virtual const Vector	GetPlayerMins( void ) const; // uses local player
	virtual const Vector	GetPlayerMaxs( void ) const; // uses local player

	void			DODRespawn( void );

	virtual void	SetAnimation( PLAYER_ANIM playerAnim );

	CBaseEntity	*	GiveNamedItem( const char *pszName, int iSubType = 0 );

	bool			Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon );

	void			SetScore( int score );
	void			AddScore( int num );
	int				GetScore( void ) { return m_iScore; }
	int				m_iScore;

	// Simulates a single frame of movement for a player
	void RunPlayerMove( const QAngle& viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, float frametime );


	//Damage record functions
	void			RecordDamageTaken( CDODPlayer *pAttacker, int iDamageTaken );
	void			RecordWorldDamageTaken( int iDamageTaken );
	void			RecordDamageGiven( CDODPlayer *pVictim, int iDamageGiven );
	void			ResetDamageCounters();	//Reset all lists

	void			OutputDamageTaken( void );
	void			OutputDamageGiven( void );

	// Voice Commands
	//============
	void HandleCommand_Voice( const char *pcmd );			// player submitted a raw voice_ command
	void HandleCommand_HandSignal( const char *pcmd );		// player wants to show a hand signal
	void VoiceCommand( int iVoiceCommand );					// internal voice command function
	void HandSignal( int iSignal );							// same for hand signals

	float m_flNextVoice;
	float m_flNextHandSignal;

	void PopHelmet( Vector vecDir, Vector vecForceOrigin );

	bool DropActiveWeapon( void );
	bool DropPrimaryWeapon( void );
	bool DODWeaponDrop( CBaseCombatWeapon *pWeapon, bool bThrowForward );
	bool BumpWeapon( CBaseCombatWeapon *pBaseWeapon );

	CWeaponDODBase* GetActiveDODWeapon() const;

	virtual void AttemptToExitFreezeCam( void );

	//Generic Ammo
	//============
	void DropGenericAmmo( void );
	void ReturnGenericAmmo( void );
	bool GiveGenericAmmo( void );
	bool m_bHasGenericAmmo;

	void ResetBleeding( void );
	void Bandage( void );		// stops the bleeding
	void SetBandager( CDODPlayer *pPlayer );
	bool IsBeingBandaged( void );
	EHANDLE m_hBandager;

	//Area Signals
	//============
	//to determine if the player is in a sandbag trigger
	CUnifiedSignals		m_signals;				// Player signals (buy zone, bomb zone, etc.)

	int					m_iCapAreaIconIndex;	//which area's icon to show - we are not necessarily capping it.
	int					m_iObjectAreaIndex;		//if the player is in an object cap area, which one?

	void				SetCapAreaIndex( int index );
	int					GetCapAreaIndex( void );
	void				ClearCapAreaIndex() { SetCapAreaIndex(-1); }

	void				SetCPIndex( int index );
		
	float				m_fHandleSignalsTime;	//time to next check the area signals
	void				HandleSignals( void );	//check if signals need to do anything, like turn icons on or off

	bool				ShouldAutoReload( void ) { return m_bAutoReload; }
	void				SetAutoReload( bool bAutoReload ) { m_bAutoReload = bAutoReload; }

	bool				ShouldAutoRezoom( void ) { return m_bAutoRezoom; }
	void				SetAutoRezoom( bool bAutoRezoom ) { m_bAutoRezoom = bAutoRezoom; }

	// Hints
	virtual CHintSystem	*Hints( void ) { return &m_Hints; }
	
	// Reset all scores
	void				ResetScores( void );

	int GetHealthAsString( char *pDest, int iDestSize );
	int GetLastPlayerIDAsString( char *pDest, int iDestSize );
	int GetClosestPlayerHealthAsString( char *pDest, int iDestSize );
	int GetPlayerClassAsString( char *pDest, int iDestSize );
	int GetNearestLocationAsString( char *pDest, int iDestSize );
	int GetTimeleftAsString( char *pDest, int iDestSize );
	int GetStringForEscapeSequence( char c,  char *pDest, int iDestSize );
	virtual void		CheckChatText( char *p, int bufsize );
	
	void PushawayThink();
	
	void DestroyRagdoll( void );

	virtual bool CanHearChatFrom( CBasePlayer *pPlayer );

	virtual void CommitSuicide( bool bExplode = false, bool bForce = false );
	virtual void CommitSuicide( const Vector &vecForce, bool bExplode = false, bool bForce = false );

	virtual bool StartReplayMode( float fDelay, float fDuration, int iEntity  );
	virtual void StopReplayMode();

	void PickUpWeapon( CWeaponDODBase *pWeapon );

	int GetPriorityForPickUpEnt( CBaseEntity *pEnt );
	virtual CBaseEntity *FindUseEntity();

	virtual void ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	bool ShouldCollide( int collisionGroup, int contentsMask ) const;

	void SetDeathFlags( int iDeathFlags ) { m_iDeathFlags = iDeathFlags; }
	int	GetDeathFlags() { return m_iDeathFlags; }

	void RemoveNemesisRelationships();

	virtual void OnAchievementEarned( int iAchievement );
	void RecalculateAchievementAwardsMask();

	bool ShouldInstantRespawn( void );

	void StatEvent_UploadStats( void );
	void StatEvent_KilledPlayer( DODWeaponID iKillingWeapon );
	void StatEvent_WasKilled( void );
	void StatEvent_RoundWin( void );
	void StatEvent_RoundLoss( void );
	void StatEvent_PointCaptured( void );
	void StatEvent_CaptureBlocked( void );
	void StatEvent_BombPlanted( void );
	void StatEvent_BombDefused( void );
	void StatEvent_ScoredDomination( void );
	void StatEvent_ScoredRevenge( void );
	void StatEvent_WeaponFired( DODWeaponID iWeaponID );
	void StatEvent_WeaponHit( DODWeaponID iWeaponID, bool bWasHeadshot );


// ------------------------------------------------------------------------------------------------ //
// Player state management.
// ------------------------------------------------------------------------------------------------ //
public:

	void State_Transition( DODPlayerState newState );
	DODPlayerState State_Get() const;				// Get the current state.

	void MoveToNextIntroCamera();	//Cycle view through available intro cameras
 
	bool ClientCommand( const CCommand &args );
	
	virtual bool IsReadyToPlay( void );

	void FireBullets( const FireBulletsInfo_t &info );

	bool CanAttack( void );

	void SetBazookaDeployed( bool bDeployed ) { m_bBazookaDeployed = bDeployed; }

	// from cbasecombatcharacter
	virtual void InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity );
	virtual void VPhysicsShadowUpdate( IPhysicsObject *pPhysics );

	void DeathSound( const CTakeDamageInfo &info );

	Activity TranslateActivity( Activity baseAct, bool *pRequired = NULL );

	CNetworkVar( float, m_flStunDuration );
	CNetworkVar( float, m_flStunMaxAlpha );

	// Stats Functions

	void Stats_WeaponFired( int weaponID );
	void Stats_WeaponHit( CDODPlayer *pVictim, int weaponID, int iDamage, int iDamageGiven, int hitgroup, float flHitDistance );
	void Stats_HitByWeapon( CDODPlayer *pAttacker, int weaponID, int iDamage, int iDamageGiven, int hitgroup );
	void Stats_KilledPlayer( CDODPlayer *pVictim, int weaponID );
	void Stats_KilledByPlayer( CDODPlayer *pAttacker, int weaponID );
	void Stats_AreaDefended( void );
	void Stats_AreaCaptured( void );
	void Stats_BonusRoundKill( void );
	void Stats_BombDetonated( void );

	void PrintLifetimeStats( void );

	// Called whenever this player fires a shot.
	void NoteWeaponFired();
	virtual bool WantsLagCompensationOnEntity( const CBasePlayer *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const;

	void TallyLatestTimePlayedPerClass( int iOldTeam, int iOldClass );

	void ResetProgressBar( void );
	void SetProgressBarTime( int barTime );

	void StoreCaptureBlock( int iAreaIndex, int iCapAttempt );
	int GetLastBlockCapAttempt( void );
	int GetLastBlockAreaIndex( void );

public:
	
	CNetworkVarEmbedded( CDODPlayerShared, m_Shared );
	
	int m_flNextTimeCheck;		// Next time the player can execute a "timeleft" command

	Vector m_lastStandingPos; // used by the gamemovement code for finding ladders

	void SetSprinting( bool bIsSprinting );

	void SetDefusing( CDODBombTarget *pTarget );
	bool m_bIsDefusing;
	CHandle<CDODBombTarget> m_pDefuseTarget;

	void SetPlanting( CDODBombTarget *pTarget );
	bool m_bIsPlanting;
	CHandle<CDODBombTarget> m_pPlantTarget;

	// Achievements
	void HandleHeadshotAchievement( int iNumHeadshots );
	void HandleDeployedMGKillCount( int iNumDeployedKills );
	int GetDeployedKillStreak( void );
	void HandleEnemyWeaponsAchievement( int iNumEnemyWpnKills );

	void ResetComboWeaponKill( void );
	void HandleComboWeaponKill( int iWeaponType );

	virtual void	PlayUseDenySound();

	int iNumKilledByUnanswered[MAX_PLAYERS+1];		// how many unanswered kills this player has been dealt by every other player

#if !defined(NO_STEAM)
	STEAM_GAMESERVER_CALLBACK( CDODPlayer, OnGSStatsReceived, GSStatsReceived_t, m_CallbackGSStatsReceived );
#endif

private:
	bool SelectSpawnSpot( const char *pEntClassName, CBaseEntity* &pSpot );

	CBaseEntity *SelectSpawnSpot( CUtlVector<EHANDLE> *pSpawnPoints, int &iLastSpawnIndex );

	// Copyed from EyeAngles() so we can send it to the client.
	CNetworkQAngle( m_angEyeAngles );

	IDODPlayerAnimState *m_PlayerAnimState;

	int FlashlightIsOn( void );
	void FlashlightTurnOn( void );
	void FlashlightTurnOff( void );

	void ShowClassSelectMenu();

	void CheckRotateIntroCam( void );

	void State_Enter( DODPlayerState newState );	// Initialize the new state.
	void State_Leave();								// Cleanup the previous state.
	void State_PreThink();							// Update the current state.

	// Specific state handler functions.
	void State_Enter_WELCOME();
	void State_PreThink_WELCOME();

	void State_Enter_PICKINGTEAM();
	void State_Enter_PICKINGCLASS();

	void State_PreThink_PICKING();

	void State_Enter_ACTIVE();
	void State_PreThink_ACTIVE();

	void State_Enter_OBSERVER_MODE();
	void State_PreThink_OBSERVER_MODE();

	void State_Enter_DEATH_ANIM();
	void State_PreThink_DEATH_ANIM();

	virtual void PlayerDeathThink();
	
	// When the player joins, it cycles their view between trigger_camera entities.
	// This is the current camera, and the time that we'll switch to the next one.
	EHANDLE m_pIntroCamera;
	float m_fIntroCamTime;

	// Find the state info for the specified state.
	static CDODPlayerStateInfo* State_LookupInfo( DODPlayerState state );

	// This tells us which state the player is currently in (joining, observer, dying, etc).
	// Each state has a well-defined set of parameters that go with it (ie: observer is movetype_noclip, non-solid,
	// invisible, etc).
	CNetworkVar( DODPlayerState, m_iPlayerState );
	// Tracks our ragdoll entity.
	CNetworkHandle( CBaseEntity, m_hRagdoll );	// networked entity handle 

	float m_flLastMovement;	// Time the player last moved, used for mp_autokick

	void InitProne( void );

	void InitSprinting( void );
	bool IsSprinting( void );
	bool CanSprint( void );

	int m_iDeathFlags;		// death notice flags related to domination/revenge

	CNetworkVar( int, m_iAchievementAwardsMask );

protected:

	void CreateRagdollEntity();


	void PhysObjectSleep();
	void PhysObjectWake();


private:

	friend void Bot_Think( CDODPlayer *pBot ); // needs to use the HandleCommand_ stuff.
	bool HandleCommand_JoinTeam( int iTeam );
	bool HandleCommand_JoinClass( int iClass );
	
	CDODPlayerStateInfo *m_pCurStateInfo;			// This can be NULL if no state info is defined for m_iPlayerState.

	bool	m_bTeamChanged;		//have we changed teams this spawn? Used to enforce one team switch per death rule

	float	m_flNextStaminaThink;	//time to do next stamina gain
	CNetworkVar( float, m_flStamina );	//stamina for sprinting, jumping etc

	Vector	m_vecTotalBulletForce;	//Accumulator for bullet force in a single frame

	bool	m_bBazookaDeployed;

	//A list of damage given
	CUtlLinkedList< CDamageRecord *, int >		m_DamageGivenList;

	//A list of damage taken
	CUtlLinkedList< CDamageRecord *, int >		m_DamageTakenList;

	bool	m_bSlowedByHit;
	float	m_flUnslowTime;
	int		m_iPlayerSpeed;	//last updated player max speed

	bool	SetSpeed( int speed );
	
	bool	m_bAutoReload;	// does the player want to autoreload their weapon when empty

	bool	m_bAutoRezoom;	// does the player want to re-zoom after each shot for sniper rifles and bazookas

	float	m_flIdleTime;	// next time we should do a deep idle

	bool	m_bIsSprinting;

	CNetworkVar( bool, m_bSpawnInterpCounter );

	CHintSystem m_Hints;

	float m_flMinNextStepSoundTime;
	
	int m_LastHitGroup;			// the last body region that took damage
	int m_LastDamageType;		// the type of damage we last took

	bool m_bPlayingProneMoveSound;

	int m_iCapAreaIndex;

	// Last usercmd we shot a bullet on.
	int m_iLastWeaponFireUsercmd;

	CNetworkVar( float, m_flProgressBarStartTime );
	CNetworkVar( int, m_iProgressBarDuration );

	// blocking abuse protection
	int m_iLastBlockAreaIndex;
	int m_iLastBlockCapAttempt;

	// Achievements Data
	int m_iComboWeaponKillMask;

	bool m_bAbortFreezeCam;
	bool m_bPlayedFreezeCamSound;

	CDODPlayerStatProperty m_StatProperty;

	EHANDLE m_hLastDroppedWeapon;
	EHANDLE m_hLastDroppedAmmoBox;

	float m_flTimeAsClassAccumulator;

public:

	// LifeID is a unique int assigned to a player each time they spawn
	int GetLifeID() { return m_iLifeID; }
	int m_iLifeID;	

	// Stats variables
	//==================

	// stats related to each weapon ( shots taken and given )
	weaponstat_t m_WeaponStats[MAX_WEAPONS];

	// a list of players I have killed ( by userid )
	CUtlMap<int, playerstat_t, int>	m_KilledPlayers;

	// a list of players that have killed me ( by userid )
	CUtlMap<int, playerstat_t, int>	m_KilledByPlayers;

	// start time - used to calc total time played

	// time played per class
	float m_flTimePlayedPerClass_Allies[7];		//0-5, 6 is random
	float m_flTimePlayedPerClass_Axis[7];		//0-5, 6 is random
	float m_flLastClassChangeTime;

	// area cap stats
	int m_iNumAreaDefenses;
	int	m_iNumAreaCaptures;
	int m_iNumBonusRoundKills;


	// Per-Round Stats
	//================

	virtual void ResetPerRoundStats( void )
	{
		m_iPerRoundCaptures = 0;
		m_iPerRoundDefenses = 0;
		m_iPerRoundBombsDetonated = 0;
		m_iPerRoundKills = 0;
	}

	int GetPerRoundCaps( void ) 
	{
		return m_iPerRoundCaptures;
	}

	int GetPerRoundDefenses( void )
	{
		return m_iPerRoundDefenses;
	}

	int GetPerRoundBombsDetonated( void )
	{
		return m_iPerRoundBombsDetonated;
	}

	int GetPerRoundKills( void )
	{
		return m_iPerRoundKills;
	}

	int m_iPerRoundCaptures;		// how many caps this round
	int m_iPerRoundDefenses;		// how many defenses this round	
	int m_iPerRoundBombsDetonated;
	int m_iPerRoundKills;
};


inline CDODPlayer *ToDODPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

#ifdef _DEBUG
	Assert( dynamic_cast<CDODPlayer*>( pEntity ) != 0 );
#endif
	return static_cast< CDODPlayer* >( pEntity );
}

inline DODPlayerState CDODPlayer::State_Get() const
{
	return m_iPlayerState;
}

#endif	//DOD_PLAYER_H
