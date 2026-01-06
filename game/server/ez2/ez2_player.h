//=============================================================================//
//
// Purpose: 	Bad Cop, a former human bent on killing anyone who stands in his way.
//				His drive in this life is to pacify noncitizens, serve the Combine,
//				and use overly cheesy quips.
// 
// Author: 		Blixibon, 1upD
//
//=============================================================================//

#ifndef EZ2_PLAYER_H
#define EZ2_PLAYER_H
#pragma once

#include "hl2_player.h"
#include "ai_speech.h"
#include "ai_playerally.h"
#include "ai_sensorydummy.h"
#include "ai_concept_response.h"
#include "GameEventListener.h"

class CAI_PlayerNPCDummy;
class CEZ2_Player;
struct SightEvent_t;

#define GLOBAL_PLAYER_ORDER_SURRENDER "player_ordered_surrenders"

// 
// Bad Cop-specific concepts
// 
#define TLK_LAST_ENEMY_DEAD "TLK_LAST_ENEMY_DEAD" // Last enemy was killed after a long engagement (separate from TLK_ENEMY_DEAD to bypass respeak delays)
#define TLK_WOUND_REMARK "TLK_WOUND_REMARK" // Do a long, almost cheesy remark about taking a certain type of damage
#define TLK_THROWGRENADE "TLK_THROWGRENADE" // Grenade was thrown
#define TLK_ALLY_KILLED_NPC "TLK_ALLY_KILLED_NPC" // Ally killed a NPC
#define TLK_KILLED_ALLY "TLK_KILLED_ALLY" // Bad Cop killed an ally (intention ambiguous)
#define TLK_DISPLACER_DISPLACE "TLK_DISPLACER_DISPLACE" // Bad Cop displaced an entity via the displacer pistol
#define TLK_DISPLACER_RELEASE "TLK_DISPLACER_RELEASE" // Bad Cop released an entity via the displacer pistol
#define TLK_SCANNER_FLASH "TLK_SCANNER_FLASH" // Bad Cop was flashed by a scanner
#define TLK_VEHICLE_OVERTURNED "TLK_VEHICLE_OVERTURNED" // Bad Cop's vehicle was overturned
#define TLK_ZOMBIE_SCREAM "TLK_ZOMBIE_SCREAM" // A zombie screams at Bad Cop

//=============================================================================
// >> EZ2_PLAYERMEMORY
// A special component mostly meant to contain memory-related variables for player speech.
// CEZ2_Player is farther below.
//=============================================================================
class CEZ2_PlayerMemory
{
	DECLARE_SIMPLE_DATADESC();
public:

	void InitLastDamage(const CTakeDamageInfo &info);
	void RecordEngagementStart();
	void RecordEngagementEnd();

	// Are we in combat?
	bool			InEngagement() { return m_bInEngagement; }
	float			GetEngagementTime() { return gpGlobals->curtime - m_flEngagementStartTime; }
	int				GetPrevHealth() { return m_iPrevHealth; }

	int				GetHistoricEnemies() { return m_iNumEnemiesHistoric; }
	void			IncrementHistoricEnemies() { m_iNumEnemiesHistoric++; }

	void			KilledEnemy();

	int				GetLastDamageType() { return m_iLastDamageType; }
	int				GetLastDamageAmount() { return m_iLastDamageAmount; }
	CBaseEntity		*GetLastDamageAttacker() { return m_hLastDamageAttacker.Get(); }

	// Criteria sets
	void			AppendLastDamageCriteria( AI_CriteriaSet& set );
	void			AppendKilledEnemyCriteria( AI_CriteriaSet& set );

	CEZ2_Player		*GetOuter() { return m_hOuter.Get(); }
	void            SetOuter( CEZ2_Player *pOuter ) { m_hOuter.Set( pOuter ); }

private:

	// Conditions before combat engagement
	bool	m_bInEngagement;
	float	m_flEngagementStartTime;
	int		m_iPrevHealth;

	int		m_iNumEnemiesHistoric;

	// Kill combo
	int		m_iComboEnemies;
	float	m_flLastEnemyDeadTime;

	// Last damage stuff (for "revenge")
	int		m_iLastDamageType;
	int		m_iLastDamageAmount;
	EHANDLE	m_hLastDamageAttacker;

	CHandle<CEZ2_Player> m_hOuter;
};

//=============================================================================
// >> EZ2_PLAYER
// 
// Bad Cop himself! (by default, that is)
// This class uses advanced criterion and memory tracking to support advanced responses to the player's actions.
// It even includes even a real, invisible dummy NPC to keep track of enemies, AI sounds, etc.
// This class was created by Blixibon and 1upD.
// 
//=============================================================================
class CEZ2_Player : public CAI_ExpresserHost<CHL2_Player>, public CGameEventListener
{
	DECLARE_CLASS(CEZ2_Player, CAI_ExpresserHost<CHL2_Player>);
public:

	CEZ2_Player();

	void			Precache( void );
	void			Spawn( void );
	void			Activate( void );
	void			UpdateOnRemove( void );

	virtual void    ModifyOrAppendCriteria(AI_CriteriaSet& criteriaSet);
	virtual bool	IsAllowedToSpeak(AIConcept_t concept = NULL);
	virtual bool    SpeakIfAllowed(AIConcept_t concept, char *pszOutResponseChosen = NULL, size_t bufsize = 0, IRecipientFilter *filter = NULL);
	virtual bool    SpeakIfAllowed(AIConcept_t concept, AI_CriteriaSet& modifiers, char *pszOutResponseChosen = NULL, size_t bufsize = 0, IRecipientFilter *filter = NULL);
	bool			SelectSpeechResponse( AIConcept_t concept, AI_CriteriaSet *modifiers, CBaseEntity *pTarget, AISpeechSelection_t *pSelection );
	void			PostSpeakDispatchResponse( AIConcept_t concept, AI_Response *response );
	virtual void	PostConstructor(const char * szClassname);
	virtual CAI_Expresser * CreateExpresser(void);
	virtual CAI_Expresser * GetExpresser() { return m_pExpresser;  }

	bool			GetGameTextSpeechParams( hudtextparms_t &params );

	void			ModifyOrAppendDamageCriteria(AI_CriteriaSet & set, const CTakeDamageInfo & info, bool bPlayer = true);
	void			ModifyOrAppendEnemyCriteria(AI_CriteriaSet & set, CBaseEntity * pEnemy);
	void			ModifyOrAppendSquadCriteria(AI_CriteriaSet & set);
	void			ModifyOrAppendWeaponCriteria(AI_CriteriaSet & set, CBaseEntity * pWeapon = NULL);
	void			ModifyOrAppendSoundCriteria(AI_CriteriaSet & set, CSound *pSound, float flDist);

	void			ModifyOrAppendFinalEnemyCriteria(AI_CriteriaSet & set, CBaseEntity * pEnemy, const CTakeDamageInfo & info);
	void			ModifyOrAppendAICombatCriteria(AI_CriteriaSet & set);

	// "Speech target" is a thing from CAI_PlayerAlly mostly used for things like Q&A.
	// I'm using it here to refer to the player's allies in player dialogue. (shouldn't be used for enemies)
	void			ModifyOrAppendSpeechTargetCriteria(AI_CriteriaSet &set, CBaseEntity *pTarget);
	CBaseEntity		*FindSpeechTarget();
	void			SetSpeechTarget( CBaseEntity *pEntity ) { m_hSpeechTarget.Set( pEntity ); }
	CBaseEntity		*GetSpeechTarget() { return m_hSpeechTarget.Get(); }

	void			InputAnswerConcept( inputdata_t &inputdata );
	
	bool			CanAutoSwitchToNextBestWeapon( CBaseCombatWeapon *pWeapon );

	void			OnUseEntity( CBaseEntity *pEntity );
	bool			HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt );

	void			OnOrderSurrender( CAI_BaseNPC *pNPC );

	Disposition_t	IRelationType( CBaseEntity *pTarget );

	bool			GetInVehicle( IServerVehicle *pVehicle, int nRole );

	// Override impulse commands for new detonation command
	virtual void		CheatImpulseCommands( int iImpulse );
	virtual void		DetonateExplosives();
	virtual void		UseHealthVial();

	// For more accurate representations of whether the player actually sees something
	// (3D dot calculations instead of 2D dot calculations)
	bool			FInTrueViewCone( const Vector &vecSpot );

	virtual int		OnTakeDamage_Alive(const CTakeDamageInfo &info);
	virtual int		TakeHealth( float flHealth, int bitsDamageType );
	void			TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	void			Event_KilledOther(CBaseEntity * pVictim, const CTakeDamageInfo & info);
	void			Event_KilledEnemy(CBaseCombatCharacter * pVictim, const CTakeDamageInfo & info);
	void			Event_Killed( const CTakeDamageInfo &info );
	bool			CommanderExecuteOne(CAI_BaseNPC *pNpc, const commandgoal_t &goal, CAI_BaseNPC **Allies, int numAllies);

	void			Event_NPCKilled( CAI_BaseNPC *pVictim, const CTakeDamageInfo &info );
	void			Event_NPCIgnited( CAI_BaseNPC *pVictim );
	void			AllyDied( CAI_BaseNPC *pVictim, const CTakeDamageInfo &info );
	void			AllyKilledEnemy( CBaseEntity *pAlly, CAI_BaseNPC *pVictim, const CTakeDamageInfo &info );

	void			Event_SeeEnemy( CBaseEntity *pEnemy );
	bool			HandleAddToPlayerSquad( CAI_BaseNPC *pNPC );
	bool			HandleRemoveFromPlayerSquad( CAI_BaseNPC *pNPC );

	void			Weapon_HandleEquip( CBaseCombatWeapon *pWeapon );

	void			Event_FirstDrawWeapon( CBaseCombatWeapon *pWeapon );
	void			Event_ThrewGrenade( CBaseCombatWeapon *pWeapon );
	void			Event_DisplacerPistolDisplace( CBaseCombatWeapon *pWeapon, CBaseEntity *pVictimEntity );
	void			Event_DisplacerPistolRelease( CBaseCombatWeapon *pWeapon, CBaseEntity *pReleaseEntity, CBaseEntity *pVictimEntity );

	void			FireGameEvent( IGameEvent *event );

	void			InputFinishBonusChallenge( inputdata_t &inputdata );
	bool			HasCheated();

	void			HUDMaskInterrupt();
	void			HUDMaskRestore();

	bool			HidingBonusProgressHUD();

	void			SetWarningTarget( CBaseEntity *pTarget ) { m_hWarningTarget = pTarget; }

	void			FireBullets( const FireBulletsInfo_t &info );

	// Blixibon - StartScripting for gag replacement
	bool				IsInAScript( void ) { return m_bInAScript; }
	inline void			SetInAScript( bool bScript ) { m_bInAScript = bScript; }
	void				InputStartScripting( inputdata_t &inputdata );
	void				InputStopScripting( inputdata_t &inputdata );

	// Blixibon - Speech thinking implementation
	void				DoSpeechAI();
	bool				DoIdleSpeech();
	bool				DoCombatSpeech();

	void				MeasureEnemies(int &iVisibleEnemies, int &iCloseEnemies);

	bool				ReactToSound( CSound *pSound, float flDist );

	CBaseEntity*		GetStaringEntity() { return m_hStaringEntity.Get(); }
	void				SetStaringEntity(CBaseEntity *pEntity) { return m_hStaringEntity.Set(pEntity); }

	void				SetSpeechFilter( CAI_SpeechFilter *pFilter )	{ m_hSpeechFilter = pFilter; }
	CAI_SpeechFilter	*GetSpeechFilter( void )						{ return m_hSpeechFilter; }

	CAI_PlayerNPCDummy	*GetNPCComponent() { return m_hNPCComponent.Get(); }
	void				CreateNPCComponent();
	void				RemoveNPCComponent();

	CEZ2_PlayerMemory	*GetMemoryComponent() { return &m_MemoryComponent; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_ENT_SCRIPTDESC();

public:
#ifdef VSCRIPT
	// VScript functions
	HSCRIPT				ScriptGetNPCComponent();
	HSCRIPT				ScriptGetStaringEntity();

	HSCRIPT				VScriptGetEnemy() { return ToHScript( GetEnemy() ); }

	int					GetVisibleEnemies() { return m_iVisibleEnemies; }
	int					GetCloseEnemies() { return m_iCloseEnemies; }
#endif

public:

	// Expresser shortcuts
	bool IsSpeaking()				{ return GetExpresser()->IsSpeaking(); }

	// NPC component shortcuts
	CBaseEntity*		GetEnemy();
	NPC_STATE			GetState();

	void AddSightEvent( SightEvent_t &sightEvent );
	CUtlVector<SightEvent_t*>	*GetSightEvents() { return &m_SightEvents; }

protected:
	virtual	void	PostThink(void);

	// A lot of combat-related concepts modify categorized criteria directly for specific subjects.
	// 
	// Modifiers will always overwrite automatic criteria, so we don't have to worry about this overwriting the desired "enemy",
	// but we do have to worry about doing a bunch of useless measurements in general criteria that are just gonna be overwritten.
	// 
	// As a result, each relevant category marks itself as "used" until the next time the original ModifyOrAppendCriteria() is called.
	enum PlayerCriteria_t
	{
		PLAYERCRIT_DAMAGE,
		PLAYERCRIT_ENEMY,
		PLAYERCRIT_SQUAD,
		PLAYERCRIT_WEAPON,
		PLAYERCRIT_SPEECHTARGET,
		PLAYERCRIT_SOUND,
	};

	inline void			MarkCriteria(PlayerCriteria_t crit) { m_iCriteriaAppended |= (1 << crit); }
	inline bool			IsCriteriaModified(PlayerCriteria_t crit) { return (m_iCriteriaAppended & (1 << crit)) != 0; }
	inline void			ResetPlayerCriteria() { m_iCriteriaAppended = 0; }

private:
	CAI_Expresser * m_pExpresser;

	bool			m_bInAScript;

	EHANDLE			m_hStaringEntity;
	float			m_flCurrentStaringTime;
	QAngle			m_angLastStaringEyeAngles;

	CUtlVector<SightEvent_t*>	m_SightEvents;

	// For speech purposes
	Vector			m_vecLastCommandGoal;

	CHandle<CAI_PlayerNPCDummy> m_hNPCComponent;
	float			m_flNextSpeechTime;
	CHandle<CAI_SpeechFilter>	m_hSpeechFilter;

	CEZ2_PlayerMemory	m_MemoryComponent;

	EHANDLE			m_hSpeechTarget;

	int				m_iVisibleEnemies;
	int				m_iCloseEnemies;

	// See PlayerCriteria_t
	int				m_iCriteriaAppended;

	// These don't need to be saved
	CNetworkVar( bool, m_bBonusChallengeUpdate );
	bool m_bMaskInterrupt;

	CNetworkHandle( CBaseEntity, m_hWarningTarget );
};

//-----------------------------------------------------------------------------
// Purpose: Sensory dummy for Bad Cop component
// 
// NPC created by Blixibon.
//-----------------------------------------------------------------------------
class CAI_PlayerNPCDummy : public CAI_SensingDummy<CAI_BaseNPC>
{
	DECLARE_CLASS(CAI_PlayerNPCDummy, CAI_SensingDummy<CAI_BaseNPC>);
	DECLARE_DATADESC();
public:

	// Don't waste CPU doing sensing code.
	// We now need hearing for state changes and stuff, but sight isn't necessary at the moment.
	//int		GetSensingFlags( void ) { return SENSING_FLAGS_DONT_LOOK /*| SENSING_FLAGS_DONT_LISTEN*/; }

	void	Spawn();

	void	ModifyOrAppendOuterCriteria(AI_CriteriaSet & set);

	void	RunAI( void );
	void	GatherEnemyConditions( CBaseEntity *pEnemy );
	int 	TranslateSchedule( int scheduleType );
	void 	OnStateChange( NPC_STATE OldState, NPC_STATE NewState );

	// Base class's sound interests include combat and danger, add relevant scents onto it
	int		GetSoundInterests( void ) { return BaseClass::GetSoundInterests() | SOUND_PHYSICS_DANGER | SOUND_CARCASS | SOUND_MEAT; }
	bool	QueryHearSound( CSound *pSound );
	bool	QuerySeeEntity( CBaseEntity *pEntity, bool bOnlyHateOrFearIfNPC = false );

	bool	UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer = NULL );

	void	DrawDebugGeometryOverlays( void );

	// Special handling for info_remarkable
	void	OnSeeEntity( CBaseEntity *pEntity );

	bool	IsValidEnemy( CBaseEntity *pEnemy );

	//---------------------------------------------------------------------------------------------
	// Override a bunch of stuff to redirect to our outer.
	//---------------------------------------------------------------------------------------------
	Vector				EyePosition() { return GetOuter()->EyePosition(); }
	const QAngle		&EyeAngles() { return GetOuter()->EyeAngles(); }
	void				EyePositionAndVectors( Vector *pPosition, Vector *pForward, Vector *pRight, Vector *pUp ) { GetOuter()->EyePositionAndVectors(pPosition, pForward, pRight, pUp); }
	const QAngle		&LocalEyeAngles() { return GetOuter()->LocalEyeAngles(); }
	void				EyeVectors( Vector *pForward, Vector *pRight = NULL, Vector *pUp = NULL ) { GetOuter()->EyeVectors(pForward, pRight, pUp); }

	Vector				HeadDirection2D( void ) { return GetOuter()->HeadDirection2D(); }
	Vector				HeadDirection3D( void ) { return GetOuter()->HeadDirection3D(); }
	Vector				EyeDirection2D( void ) { return GetOuter()->EyeDirection2D(); }
	Vector				EyeDirection3D( void ) { return GetOuter()->EyeDirection3D(); }

	Disposition_t		IRelationType( CBaseEntity *pTarget )		{ return GetOuter()->IRelationType(pTarget); }
	int					IRelationPriority( CBaseEntity *pTarget );	//{ return GetOuter()->IRelationPriority(pTarget); }

	// NPCs seem to be able to see the player inappropriately with these overrides to FVisible()
	//bool				FVisible ( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL ) { return GetOuter()->FVisible(pEntity, traceMask, ppBlocker); }
	//bool				FVisible( const Vector &vecTarget, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL )	{ return GetOuter()->FVisible( vecTarget, traceMask, ppBlocker ); }

	bool				FInViewCone( CBaseEntity *pEntity ) { return GetOuter()->FInViewCone(pEntity); }
	bool				FInViewCone( const Vector &vecSpot ) { return GetOuter()->FInViewCone(vecSpot); }

	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	bool		IsPlayerAlly(CBasePlayer *pPlayer = NULL) { return false; }
	bool		IsSilentSquadMember() const 	{ return true; }

	// The player's dummy can only sense, it isn't a real enemy
	bool		CanBeAnEnemyOf( CBaseEntity *pEnemy ) { return false; }
	bool		CanBeSeenBy( CAI_BaseNPC *pNPC ) { return false; }

	Class_T	Classify( void ) { return CLASS_NONE; }

	CEZ2_Player		*GetOuter() { return m_hOuter.Get(); }
	void			SetOuter(CEZ2_Player *pOuter) { m_hOuter.Set(pOuter); }

protected:
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		//COND_EXAMPLE = BaseClass::NEXT_CONDITION,
		//NEXT_CONDITION,

		SCHED_PLAYERDUMMY_ALERT_STAND = BaseClass::NEXT_SCHEDULE,
		NEXT_SCHEDULE,

		//TASK_EXAMPLE = BaseClass::NEXT_TASK,
		//NEXT_TASK,

		//AE_EXAMPLE = LAST_SHARED_ANIMEVENT

	};

	CHandle<CEZ2_Player> m_hOuter;

	DEFINE_CUSTOM_AI;
};

//-----------------------------------------------------------------------------
// Purpose: Mapper-controlled detonatable object
//-----------------------------------------------------------------------------
class CPointDetonatable : public CBaseEntity
{
public:
	DECLARE_CLASS( CPointDetonatable, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPointDetonatable();
	~CPointDetonatable();

	void			Spawn();
	int				UpdateTransmitState();
	void			VerifyGlowTarget( CBaseEntity *pActivator, CBaseEntity *pCaller );

	void			InputEnable( inputdata_t &inputdata );
	void			InputDisable( inputdata_t &inputdata );
	void			InputSetGlowTarget( inputdata_t &inputdata );
	void			InputDetonate( inputdata_t &inputdata );

	CNetworkVar( bool, m_bDisabled );

	string_t		m_iszThrower;
	CNetworkHandle( CBaseCombatCharacter, m_hThrower );

	string_t		m_iszGlowTarget;
	CNetworkHandle( CBaseEntity, m_hGlowTarget );

	COutputEvent	m_OnDetonate;
};

//-----------------------------------------------------------------------------
// Purpose: Kick data for interaction.
// (Blixibon)
//-----------------------------------------------------------------------------
struct KickInfo_t
{
	KickInfo_t( trace_t *_tr, CTakeDamageInfo *_dmgInfo )
	{
		tr = _tr;
		dmgInfo = _dmgInfo;
		success = true;
	}

	trace_t *tr;
	CTakeDamageInfo *dmgInfo;
	bool success; // Can be set by interactions to determine if a kick was "successful" (whether it should be counted by kick trackers)
};

//-----------------------------------------------------------------------------
// Purpose: Sight events for hints and stuff.
// (Blixibon)
//-----------------------------------------------------------------------------
typedef bool (*SIGHTEVENTPTR)(CEZ2_Player *pPlayer, CBaseEntity *pActivator);

struct SightEvent_t
{
	//DECLARE_SIMPLE_DATADESC();

	SightEvent_t( const char *_pName, float _flCooldown, SIGHTEVENTPTR _pTestFunc, SIGHTEVENTPTR _pMainFunc, float _flFailedCooldown = 2.0f )
	{
		pName = _pName;
		flCooldown = _flCooldown;
		pTestFunc = _pTestFunc;
		pMainFunc = _pMainFunc;
		flNextHintTime = 0.0f;
		flLastHintTime = 0.0f;
		flFailedCooldown = _flFailedCooldown;
	}

	bool Test( CEZ2_Player *pPlayer, CBaseEntity *pActivator ) { return (*pTestFunc)(pPlayer, pActivator); }
	bool DoEvent( CEZ2_Player *pPlayer, CBaseEntity *pActivator ) { return (*pMainFunc)(pPlayer, pActivator); }

	const char *pName;

	float	flNextHintTime;
	float	flLastHintTime;

	float	flCooldown;
	float	flFailedCooldown; // When to check again when test fails

private:

	SIGHTEVENTPTR pTestFunc;
	SIGHTEVENTPTR pMainFunc;
};

#define SIGHT_EVENT_HINT_START( funcName, eventName ) \
	bool funcName( CEZ2_Player *pPlayer, CBaseEntity *pActivator ) \
	{ \
		if (player_use_instructor.GetBool()) \
		{ \
			IGameEvent *event = gameeventmanager->CreateEvent( eventName ); \
			if ( event ) \
			{

#define SIGHT_EVENT_HINT_END( hintName ) \
			} \
		} \
		else \
		{ \
			UTIL_HudHintText( pPlayer, hintName ); \
		} \
		return true; \
	}

#endif	//EZ2_PLAYER_H