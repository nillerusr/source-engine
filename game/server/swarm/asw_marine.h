#ifndef ASW_MARINE_H
#define ASW_MARINE_H
#pragma once

#include "asw_marine_shared.h"
#include "asw_vphysics_npc.h"
#include "asw_shareddefs.h"
#include "asw_playeranimstate.h"
#include "asw_lag_compensation.h"

class CASW_Player;
class CASW_Marine_Resource;
class CASW_Marine_Profile;
class CASW_Weapon;
class CASW_MarineSpeech;
class CASW_SimpleAI;
class CASW_Pickup;
class CASW_Pickup_Weapon;
class CASW_Alien;
class CASW_Ammo;
class CASW_Ammo_Drop;
class CMoveData;
class IASW_Vehicle;
class CASW_Remote_Turret;
class CRagdollProp;
class CASW_Hack;
class CBeam;
class CASW_Melee_Attack;
class CASW_Use_Area;
class CASW_SquadFormation;
class CAI_Hint;
class CASW_Alien_Goo;
class CASW_BuffGrenade_Projectile;
class CBaseTrigger;

#define ASW_MAX_FOLLOWERS 6
#define ASW_INVALID_FOLLOW_SLOT -1

// marine sight cone when holding position
#define ASW_HOLD_POSITION_FOV_DOT 0.5f
#define ASW_HOLD_POSITION_SIGHT_RANGE 768.0f

// marine sight cone when in follow mode
#define ASW_FOLLOW_MODE_FOV_DOT 0.7f
#define ASW_FOLLOW_MODE_SIGHT_RANGE 525.0f
#define ASW_FOLLOW_MODE_SIGHT_HEIGHT 64.0f

// marine sight cone using the old follow mode
#define ASW_DUMB_FOLLOW_MODE_SIGHT_RANGE 200.0f
#define ASW_DUMB_FOLLOW_MODE_SIGHT_HEIGHT 64.0f

// AI marines can always see aliens within the close combat sight range
#define ASW_CLOSE_COMBAT_SIGHT_RANGE_EASY 256.0f
#define ASW_CLOSE_COMBAT_SIGHT_RANGE_NORMAL 214.0f
#define ASW_CLOSE_COMBAT_SIGHT_RANGE_HARD 171.0f
#define ASW_CLOSE_COMBAT_SIGHT_RANGE_INSANE 128.0f

#define ASW_MOB_VICTIM_SIZE 3
#define ASW_MARINE_HISTORY_POSITIONS 6

class CASW_Marine : public CASW_VPhysics_NPC, public IASWPlayerAnimStateHelpers
{
public:
	DECLARE_CLASS( CASW_Marine, CASW_VPhysics_NPC );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;

	CASW_Marine();
	virtual ~CASW_Marine();

	// Use this in preference to CASW_Marine::AsMarine( pEnt ) :
	static inline CASW_Marine *AsMarine( CBaseEntity *pEnt );

	virtual void Precache();
	virtual void PrecacheSpeech();
	
	virtual void	Spawn( void );
	virtual void	NPCInit();
	virtual void	UpdateOnRemove();
	void	SetModelFromProfile();
	void	SelectModelFromProfile();
	void	SelectModel();
	
	CAI_Senses *CreateSenses();

	void SetHeightLook( float flHeightLook );

	// Thinking
	virtual void Think(void);
	void PostThink();	
	virtual void ASWThinkEffects();
	
	// Networking
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual int	UpdateTransmitState();	
	void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );

	// Camera
	virtual const QAngle& ASWEyeAngles( void );
	Vector EyePosition(void);

	// Classification
	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_MARINE; }
	virtual bool IsPlayerAlly( CBasePlayer *pPlayer );

	// Animation
	IASWPlayerAnimState *m_PlayerAnimState;
	void DoAnimationEvent( PlayerAnimEvent_t event );
	void DoAnimationEventToAll( PlayerAnimEvent_t event );
	void HandleAnimEvent( animevent_t *pEvent );
	
	// Marine resource
	void SetMarineResource(CASW_Marine_Resource *pMR);
	CASW_Marine_Resource* GetMarineResource() const;
	CASW_Marine_Profile* GetMarineProfile();
	EHANDLE m_MarineResource;

	// Commander/Inhabiting	
	void SetCommander(CASW_Player *player);		// sets which player commands this marine
	CASW_Player* GetCommander() const;
	bool IsInhabited();
	void SetInhabited(bool bInhabited);
	void InhabitedBy(CASW_Player *player);		// called when a player takes direct control of this marine
	void UninhabitedBy(CASW_Player *player);	// called when a player stops direct control of this marine	
	CNetworkHandle (CASW_Player, m_Commander); 	// the player in charge of this marine
	void SetInitialCommander(CASW_Player *player);
	char m_szInitialCommanderNetworkID[64];		// ASWNetworkID of the first commander for this marine in this mission
	const char *GetPlayerName() const;

	// Alien related
	bool IsAlienNear();	// is an alien nearby? (used by speech to know if we should shout urgent lines)
	void HurtAlien(CBaseEntity *pAlien, const CTakeDamageInfo &info);
	void HurtJunkItem(CBaseEntity *pAlien, const CTakeDamageInfo &info);
	float m_fMadFiringCounter;
	static float s_fNextMadFiringChatter;
	float m_fNextAlienWalkDamage;	// timer for pain from walking on aliens

	// Sound, speech
	CASW_MarineSpeech* GetMarineSpeech() { return m_MarineSpeech; }		
	CASW_MarineSpeech* m_MarineSpeech;
	void		ModifyOrAppendCriteria( AI_CriteriaSet& set );
	const char *GetResponseRulesName(); ///< returns the name used for the 'who' clause in a response rules query
	float m_fLastStillTime;
	float m_fIdleChatterDelay;
	static float s_fNextIdleChatterTime;	// team next chatter time
	
	// Light related
	void FlashlightTurnOn();
	void FlashlightTurnOff();
	void FlashlightToggle();
	bool HasFlashlight();

	// Movement
	// these stop movement, doesn't actually do anything here on the server! but used for melee anims to keep in sync.
	virtual bool IsCurTaskContinuousMove();
	float GetStopTime() { return m_fStopMarineTime; }
	void SetStopTime(float fTime) { m_fStopMarineTime = fTime; }
	CNetworkVar(float, m_fStopMarineTime);
	virtual bool ASWAnim_CanMove();
	virtual float MaxSpeed();
	void AvoidPhysicsProps( CUserCmd *pCmd );
	void    PhysicsSimulate( void );		
	virtual void InhabitedPhysicsSimulate();
	int m_nOldButtons;
	virtual bool ShouldPlayerAvoid( void );
	virtual float GetIdealSpeed() const;
	float m_fCachedIdealSpeed;
	virtual float GetIdealAccel( ) const;
	virtual float MaxYawSpeed( void );
	float	m_fLastStuckTime;	// gets set when we're stuck (used to turn on our phys shadow to push away phys objects)
	float m_flFirstStuckTime;	// gets set the first frame we're stuck
	CNetworkVar(bool, m_bPreventMovement);	// if true, the player will send zeroed movement commands (used by queen tentacles)
	virtual void SetPlayerAvoidState();
	virtual unsigned int PhysicsSolidMaskForEntity() const;
	bool TeleportStuckMarine();
	bool TeleportToFreeNode();
	CNetworkVar( bool, m_bWalking );

	CASW_Lag_Compensation m_LagCompensation;

	// Texture names and surface data, used by CASW_MarineGameMovement
	int				m_surfaceProps;
	surfacedata_t*	m_pSurfaceData;
	float			m_surfaceFriction;
	char			m_chTextureType;
	char			m_chPreviousTextureType;	// Separate from m_chTextureType. This is cleared if the player's not on the ground.

	// melee
	void PhysicsShove();
	void DoMeleeDamageTrace( float flYawStart, float flYawEnd );
	void PlayMeleeImpactEffects( CBaseEntity *pEntity, trace_t *tr );
	void ApplyMeleeDamage( CBaseEntity *pHitEntity, CTakeDamageInfo &dmgInfo, Vector &vecAttackDir, trace_t *tr );
	CBaseEntity *MeleeTraceHullAttack( const Vector &vecStart, const Vector &vecEnd, const Vector &vecMins, const Vector &vecMaxs, bool bHitBehindMarine, float flAttackCone );
	float m_fKickTime;
	// Keep track of recent melee hits so we can perform wide area of effect melee attacks without double-damaging entities
	CUtlVector<CBaseEntity*>	m_RecentMeleeHits;
	CNetworkVar(float, m_fNextMeleeTime);
	int GetAlienMeleeFlinch();
	CNetworkVar( int, m_iMeleeAttackID );
	CNetworkVar( float, m_flKnockdownYaw );
	CASW_Melee_Attack *GetCurrentMeleeAttack();
	Vector m_vecMeleeStartPos;
	float m_flMeleeStartTime, m_flMeleeLastCycle;
	CNetworkVar( float, m_flMeleeYaw );
	CNetworkVar( bool, m_bFaceMeleeYaw );
	bool m_bMeleeCollisionDamage;
	bool m_bMeleeComboKeypressAllowed;
	bool m_bMeleeComboKeyPressed;
	bool m_bMeleeComboTransitionAllowed;
	bool m_bMeleeMadeContact;
	int m_iMeleeAllowMovement;
	bool m_bMeleeKeyReleased; 
#ifdef MELEE_CHARGE_ATTACKS
	bool m_bMeleeHeavyKeyHeld;
	CNetworkVar( float, m_flMeleeHeavyKeyHoldStart );
#endif
	bool m_bMeleeChargeActivate;
	int m_iUsableItemsOnMeleePress;
	virtual void HandlePredictedAnimEvent( int event, const char* options );
	int m_iPredictedEvent[ASW_MAX_PREDICTED_MELEE_EVENTS];
	float m_flPredictedEventTime[ASW_MAX_PREDICTED_MELEE_EVENTS];
	const char* m_szPredictedEventOptions[ASW_MAX_PREDICTED_MELEE_EVENTS];
	int m_iNumPredictedEvents;
	int m_iOnLandMeleeAttackID;
	float GetBaseMeleeDamage() { return m_flBaseMeleeDamage; }
	float m_flBaseMeleeDamage;
	bool m_bPlayedMeleeHitSound;
	bool HasPowerFist();

	CNetworkVar( bool, m_bNoAirControl );

	// jump jets
	CNetworkVar( int, m_iJumpJetting );
	Vector m_vecJumpJetStart;
	Vector m_vecJumpJetEnd;
	float m_flJumpJetStartTime;
	float m_flJumpJetEndTime;

	// powerups are designed so that you can only have one at a time
	void AddPowerup( int iType, float flExpireTime = -1 );
	bool HasPowerup( int iType );
	void RemoveWeaponPowerup( CASW_Weapon* pWeapon );
	void RemoveAllPowerups( void );
	void UpdatePowerupDuration( void );
	CNetworkVar( int, m_iPowerupType );
	CNetworkVar( float, m_flPowerupExpireTime );
	CNetworkVar( bool, m_bPowerupExpires );
	//CNetworkVar( int, m_iPowerupCount );

	void AddDamageBuff( CASW_BuffGrenade_Projectile *pBuffGrenade, float flDuration ) { m_hLastBuffGrenade = pBuffGrenade; m_flDamageBuffEndTime = MAX( GetDamageBuffEndTime(), gpGlobals->curtime + flDuration ); }
	void RemoveDamageBuff() { m_flDamageBuffEndTime = 0.0f; }
	float GetDamageBuffEndTime() { return m_flDamageBuffEndTime.Get(); }
	CNetworkVar( float, m_flDamageBuffEndTime );
	CHandle<CASW_BuffGrenade_Projectile> m_hLastBuffGrenade;

	void AddElectrifiedArmor( float flDuration ) { m_flElectrifiedArmorEndTime = MAX( GetElectrifiedArmorEndTime(), gpGlobals->curtime + flDuration ); }
	float GetElectrifiedArmorEndTime() { return m_flElectrifiedArmorEndTime.Get(); }
	bool IsElectrifiedArmorActive() { return GetElectrifiedArmorEndTime() > gpGlobals->curtime; }
	CNetworkVar( float, m_flElectrifiedArmorEndTime );
	
	void ApplyPassiveArmorEffects( CTakeDamageInfo &dmgInfo ) RESTRICT ;
	void ApplyPassiveMeleeDamageEffects( CTakeDamageInfo &dmgInfo );

	CBaseTrigger* IsInEscapeVolume();		// returns pointer to current escape volume, if marine is in the exit

	// Custom conditions, schedules and tasks
	enum
	{
		COND_ASW_NEW_ORDERS = BaseClass::NEXT_CONDITION,
		COND_ASW_BEGIN_RAPPEL,
		COND_SQUADMATE_WANTS_AMMO,
		COND_SQUADMATE_NEEDS_AMMO,	// out of ammo
		COND_PATH_BLOCKED_BY_PHYSICS_PROP,
		COND_PROP_DESTROYED,
		COND_COMPLETELY_OUT_OF_AMMO,
		COND_ASW_ORDER_TIME_OUT,
		NEXT_CONDITION,

		SCHED_ASW_HOLD_POSITION = BaseClass::NEXT_SCHEDULE,
		SCHED_ASW_RANGE_ATTACK1,
		SCHED_ASW_USING_OVER_TIME,
		SCHED_ASW_USE_AREA,
		SCHED_ASW_OVERKILL_SHOOT,
		SCHED_ASW_MOVE_TO_ORDER_POS,
		SCHED_ASW_FOLLOW_MOVE,
		SCHED_ASW_FOLLOW_WAIT,
		SCHED_ASW_PICKUP_WAIT,
		SCHED_ASW_FOLLOW_BACK_OFF,
		SCHED_ASW_RAPPEL_WAIT,
		SCHED_ASW_RAPPEL,
		SCHED_ASW_CLEAR_RAPPEL_POINT, // Get out of the way for the next guy		
		SCHED_ASW_GIVE_AMMO, // ammo bag
		SCHED_MELEE_ATTACK_PROP1, // melee a physics prop, typically because it's obstructing
		SCHED_ENGAGE_AND_MELEE_ATTACK1,
		SCHED_ASW_HEAL_MARINE,
		NEXT_SCHEDULE,

		TASK_ASW_TRACK_AND_FIRE = BaseClass::NEXT_TASK,		
		TASK_ASW_FACE_HOLDING_YAW,
		TASK_ASW_FACE_USING_ITEM,
		TASK_ASW_START_USING_AREA,
		TASK_ASW_FACE_ENEMY_WITH_ERROR,
		TASK_ASW_OVERKILL_SHOOT,
		TASK_ASW_GET_PATH_TO_ORDER_POS,
		TASK_ASW_GET_PATH_TO_FOLLOW_TARGET,
		TASK_ASW_GET_PATH_TO_PROP,
		TASK_ASW_FACE_RANDOM_WAIT,
		TASK_ASW_FACE_FOLLOW_WAIT,
		TASK_ASW_GET_BACK_OFF_PATH,
		TASK_ASW_WAIT_FOR_FOLLOW_MOVEMENT,
		TASK_ASW_WAIT_FOR_MOVEMENT,
		TASK_ASW_CHATTER_CONFIRM,
		TASK_ASW_RAPPEL,
		TASK_ASW_HIT_GROUND,
		TASK_ASW_ORDER_TO_DEPLOY_SPOT,
		TASK_ASW_GET_PATH_TO_GIVE_AMMO,
		TASK_ASW_MOVE_TO_GIVE_AMMO,
		TASK_ASW_SWAP_TO_AMMO_BAG,
		TASK_ASW_GIVE_AMMO_TO_MARINE,
		TASK_ASW_GET_PATH_TO_HEAL,
		TASK_ASW_MOVE_TO_HEAL,
		TASK_ASW_SWAP_TO_HEAL_GUN,
		TASK_ASW_HEAL_MARINE,
		NEXT_TASK,
	};

	// asw schedule stuff
	virtual bool OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult );
	virtual void GatherConditions();
	virtual void BuildScheduleTestBits();
	virtual int SelectSchedule();
	virtual int SelectMeleeSchedule();
	virtual int SelectFollowSchedule();
	virtual int TranslateSchedule( int scheduleType );
	int SelectHackingSchedule();
	virtual void RunTaskRangeAttack1( const Task_t *pTask );
	virtual void StartTaskRangeAttack1( const Task_t *pTask );
	virtual void RunTask( const Task_t *pTask );
	virtual void StartTask(const Task_t *pTask);
	virtual void UpdateEfficiency( bool bInPVS );
	//virtual void TaskFail( AI_TaskFailureCode_t );
	virtual void		TaskFail( AI_TaskFailureCode_t );
	void				TaskFail( const char *pszGeneralFailText )	{ TaskFail( MakeFailCode( pszGeneralFailText ) ); }
	void CheckForAIWeaponSwitch();

private:
	EHANDLE m_hAlienGooTarget;
	EHANDLE m_hPhysicsPropTarget;
public:
	float m_flLastGooScanTime;
	CBaseEntity *BestAlienGooTarget();
	void SetAlienGooTarget( CBaseEntity *pAlienGooTarget )	{ m_hAlienGooTarget = pAlienGooTarget; }
	EHANDLE GetAlienGooTarget() { return m_hAlienGooTarget.Get(); }
	EHANDLE GetAlienGooTarget() const { return m_hAlienGooTarget.Get(); }
	bool EngageNewAlienGooTarget();

	void SetPhysicsPropTarget( CBaseEntity *pPhysicsPropTarget ) { m_hPhysicsPropTarget = pPhysicsPropTarget; }
	EHANDLE GetPhysicsPropTarget() { return m_hPhysicsPropTarget.Get(); }
	EHANDLE GetPhysicsPropTarget() const { return m_hPhysicsPropTarget.Get(); }
	void CheckForDisablingAICollision( CBaseEntity *pEntity );

	virtual const Vector &		GetEnemyLKP() const;

	// hacking
	CHandle<CASW_Use_Area> m_hAreaToUse;
	void OnWeldStarted();
	void OnWeldFinished();
	bool m_bWaitingForWeld;
	float m_flBeginWeldTime;
	
	// orders and holding position	
	virtual bool OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval );
	void UpdateFacing();
	float GetHoldingYaw() { return m_fHoldingYaw; }
	float m_fHoldingYaw;
	CNetworkVar(int, m_ASWOrders);
	ASW_Orders GetASWOrders() { return (ASW_Orders) m_ASWOrders.Get(); }
	void SetASWOrders(ASW_Orders NewOrders, float fHoldingYaw=-1, const Vector *pOrderPos=NULL);
	void OrdersFromPlayer(CASW_Player* pPlayer, ASW_Orders NewOrders, CBaseEntity *pMarine, bool bChatter, float fHoldingYaw=-1, Vector *pVecOrderPos = NULL);	// called by the player when ordering this marine about	
	virtual bool CreateBehaviors();
	void ProjectBeam( const Vector &vecStart, const Vector &vecDir, int width, int brightness, float duration );
	void Scan();
	float m_flTimeNextScanPing;
	Vector m_vecMoveToOrderPos;
	bool m_bDoneOrderChatter;
	CASW_Marine* TooCloseToAnotherMarine();	// returns a marine we're standing too close to
	float m_fRandomFacing;
	float m_fNewRandomFacingTime;
	bool NeedToUpdateSquad(); // whether I am a squad leader and should update the follow positions for my squad
	bool NeedToFollowMove();
	bool m_bWasFollowing;	// were we previously doing follow orders?

	void RecalculateAIYawOffset();
	float m_flAIYawOffset;	// AI marine will offset his yaw by this amount.  this value changes periodically to make the marine seem less robotic
	float m_flNextYawOffsetTime;	// time at which to recalculate the yaw offset
	CNetworkVar( bool, m_bAICrouch );		// if set, the AI will appear crouched when still
	float m_flLastEnemyYaw;
	float m_flLastEnemyYawTime;

	void OrderHackArea( CASW_Use_Area *pArea );

	// AI taking ammo
	virtual int SelectTakeAmmoSchedule();
	float m_flNextAmmoScanTime;
	CHandle<CASW_Ammo> m_hTakeAmmo;
	CHandle<CASW_Ammo_Drop> m_hTakeAmmoDrop;
	float m_flResetAmmoIgnoreListTime;
	CUtlVector<EHANDLE> m_hIgnoreAmmo;

	// detecting squad combat state
	void UpdateCombatStatus();
	bool IsInCombat();
	float m_flLastHurtAlienTime;
	float m_flLastSquadEnemyTime;
	float m_flLastSquadShotAlienTime;
	bool m_bInCombat;

	virtual bool FValidateHintType( CAI_Hint *pHint );

	inline CASW_SquadFormation *GetSquadFormation(); // get the formation in which I participate
	CASW_Marine *GetSquadLeader();  // If I'm following in a squad, get the leader I'm following

	// tracking movement through the level
	void AddPositionHistory();
	float GetOverallMovementDirection();

	struct PositionHistory_t
	{
		Vector vecPosition;
		float flTime;
	};
	int m_nPositionHistoryTail;
	PositionHistory_t m_PositionHistory[ ASW_MARINE_HISTORY_POSITIONS ];
	float m_flNextPositionHistoryTime;

protected:
	inline const Vector &GetFollowPos();
	// this is not authoritative -- it's a bit of cruft because some old code in the client needs
	// to know if this marine is following someone. this just caches the results of a GetSquadLeader() call.
	CNetworkHandle(CBaseEntity, m_hMarineFollowTarget);   // the marine we're currently ordered to follow, to be shown on the scanner and used by custom follow code

public:

	void OrderUseOffhandItem( int iInventorySlot, const Vector &vecDest );
	int SelectOffhandItemSchedule();
	void FinishedUsingOffhandItem( bool bItemThrown = false, bool bFailed = false );
	bool CanThrowOffhand( CASW_Weapon *pWeapon, const Vector &vecSrc, const Vector &vecDest, bool bDrawArc = false );
	const Vector& GetOffhandItemSpot() { return m_vecOffhandItemSpot; }
	Vector GetOffhandThrowSource( const Vector *vecStandingPos = NULL );
	int FindThrowNode( const Vector &vThreatPos, float flMinThreatDist, float flMaxThreatDist, float flBlockTime );
	Vector m_vecOffhandItemSpot;					// the place we want to throw/deploy our offhand item
	CHandle<CASW_Weapon> m_hOffhandItemToUse;		// ordering the marine to use an offhand item
			
	// Hacking
	CNetworkVar( bool,  m_bHacking );			// is this marine currently hacking a button panel or computer?
	CNetworkHandle( CASW_Hack, m_hCurrentHack );		// the current hack object (not the computer console itself) we're hacking
	bool IsHacking( void );

	// ammo
	bool CarryingAGunThatUsesAmmo( int iAmmoIndex);
	virtual int	GiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound = false );
	virtual int	GiveAmmoToAmmoBag( int iCount, int iAmmoIndex, bool bSuppressSound = false );
	void Weapon_Equip( CBaseCombatWeapon *pWeapon );
	void Weapon_Equip_In_Index( CBaseCombatWeapon *pWeapon, int index );	// like weapon_equip, but tries to put the weapon in the specific index
	void Weapon_Equip_Post( CBaseCombatWeapon *pWeapon);
	int GetWeaponIndex( CBaseCombatWeapon *pWeapon ) const;		// returns weapon's position in our myweapons array
	int GetNumberOfWeaponsUsingAmmo(int iAmmoType);
	bool CanPickupPrimaryAmmo();
	virtual void TookAmmoPickup( CBaseEntity* pAmmoPickup );
	int GetAllAmmoCount( void );
	int GetTotalAmmoCount(int iAmmoIndex);
	int GetTotalAmmoCount(char *szName);
	int GetWeaponAmmoCount(int iAmmoIndex);
	int GetWeaponAmmoCount(char *szName);
	void ThrowAmmo(int iInventorySlot, int iTargetMarine, int iAmmoType);	 // throwing ammo from the ammo bag
	bool CheckAutoWeaponSwitch();	// AI switching weapons automatically if hurt while using a non-offensive weapon
	int m_iHurtWithoutOffensiveWeapon;	// tracks how many times the AI marine's been hurt without a weapon selected
	void CheckAndRequestAmmo();
	bool IsOutOfAmmo();
	virtual void OnWeaponOutOfAmmo(bool bChatter);

	// ammo bag/give ammo
	bool CanGiveAmmoTo( CASW_Marine* pMarine );
	int SelectGiveAmmoSchedule( void );
	CHandle<CASW_Marine> m_hGiveAmmoTarget;

	CHandle<CASW_Marine> m_hHealTarget;
	bool CanHeal() const;
	int SelectHealSchedule();

	// weapons
	virtual bool Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex=0 ) ;
	virtual bool Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon );
	CASW_Weapon* GetASWWeapon(int index) const;	
	int GetWeaponPositionForPickup(const char* szWeaponClass);	// returns which slot in the m_hWeapons array this pickup should go in
	bool TakeWeaponPickup(CASW_Weapon* pWeapon);
	bool TakeWeaponPickup(CASW_Pickup_Weapon* pPickup);				// takes a weapon	
	bool DropWeapon(int iWeaponIndex, bool bNoSwap = false);		// drops the weapon on the ground as a weapon pickup
	bool DropWeapon(CASW_Weapon* pWeapon, bool bNoSwap, const Vector *pvecTarget=NULL, const Vector *pVelocity=NULL);
	virtual void Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ );	// HL version	
	virtual CBaseCombatWeapon* ASWAnim_GetActiveWeapon();
	CASW_Weapon* GetActiveASWWeapon( void ) const;	
	virtual Vector Weapon_ShootPosition();					// source point for firing weapons	
	virtual bool IsFiring();

	void DoDamagePowerupEffects( CBaseEntity *pTarget, CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	int m_iDamageAttributeEffects;
	virtual void FireBullets( const FireBulletsInfo_t &info );
	virtual void FireRegularBullets( const FireBulletsInfo_t &info );
	virtual void FirePenetratingBullets( const FireBulletsInfo_t &info, int iMaxPenetrate, float fPenetrateChance, int iSeedPlus, bool bAllowChange=true, Vector *pPiercingTracerEnd=NULL, bool bSegmentTracer = true );
	virtual void FireBouncingBullets( const FireBulletsInfo_t &info, int iMaxBounce, int iSeedPlus=0 );
	CBaseCombatWeapon* GetLastWeaponSwitchedTo();
	EHANDLE m_hLastWeaponSwitchedTo;
	float m_fStartedFiringTime;
	virtual void AimGun();
	float m_fLastShotAlienTime;
	float m_fLastShotJunkTime;
	virtual void DoMuzzleFlash();
	virtual void DoImpactEffect( trace_t &tr, int nDamageType );
	void MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );
	void MakeUnattachedTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );
	void OnWeaponFired( const CBaseEntity *pWeapon, int nShotsFired, bool bIsSecondary = false );
	float m_flLastAttributeExplosionSound;
	int m_nFastReloadsInARow;
	CNetworkVar( float, m_flPreventLaserSightTime );

	// AI control of firing
	bool AIWantsToFire();
	bool AIWantsToFire2();
	bool AIWantsToReload();
	bool m_bWantsToFire, m_bWantsToFire2;
	float m_fMarineAimError;
	CNetworkVar(float, m_fAIPitch);	// pitch aim of the AI, so it can be shown by the clientside anims
	virtual bool SetNewAimError(CBaseEntity *pTarget);
	virtual void AddAimErrorToTarget(Vector &vecTarget);
	virtual Vector GetActualShootTrajectory( const Vector &shootOrigin );
	virtual Vector GetActualShootPosition( const Vector &shootOrigin );
	virtual bool FInViewCone( const Vector &vecSpot );
	virtual bool FInViewCone( CBaseEntity *pEntity ) { return FInViewCone( pEntity->WorldSpaceCenter() ); }
	virtual bool FInAimCone( const Vector &vecSpot );
	virtual bool WeaponLOSCondition(const Vector &ownerPos, const Vector &targetPos, bool bSetConditions );
	virtual float GetFollowSightRange();
	virtual float GetCloseCombatSightRange();

	// overkill shooting (the AI marine firing for a period longer than strictly necessary)
	float m_fOverkillShootTime;
	Vector m_vecOverkillPos;

	// health related
	void AddSlowHeal(int iHealAmount, float flHealRateScale, CASW_Marine *pMedic, CBaseEntity* pHealingWeapon = NULL );
	CNetworkVar(bool, m_bSlowHeal);
	CNetworkVar(int, m_iSlowHealAmount);
	float m_flHealRateScale;
	float m_fNextSlowHealTick;
	void MeleeBleed(CTakeDamageInfo* info);
	void BecomeInfested(CASW_Alien* pAlien);
	void CureInfestation(CASW_Marine *pHealer, float fCureFraction);
	bool m_bPlayedCureScream;	// have we played a scream sound for our parasite?
	bool IsInfested();
	CNetworkVar(float, m_fInfestedTime);	// how much time left on the infestation
	CNetworkVar(float, m_fInfestedStartTime);	// when the marine first got infested
	int m_iInfestCycle;	
	virtual void ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon = NULL );
	virtual void Ignite( float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false );
	virtual void Extinguish();
	virtual	bool		AllowedToIgnite( void );
	float m_flFirstBurnTime;
	float m_flLastBurnTime;
	float m_flLastBurnSoundTime;
	float m_fNextPainSoundTime;
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual bool BecomeRagdollOnClient( const Vector &force );
	void Suicide();
	bool IsWounded() const;	// less than 60% health
	int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual bool CorpseGib( const CTakeDamageInfo &info );
	virtual bool ShouldGib( const CTakeDamageInfo &info );
	virtual bool Event_Gibbed( const CTakeDamageInfo &info );
	virtual bool HasHumanGibs() { return true; }
	//CNetworkVar(float, m_fDieHardTime);	// If set, marine is in DIE HARD mode, this is the time at which marine will die
	virtual float GetReceivedDamageScale( CBaseEntity *pAttacker );
	CNetworkVar(float, m_fFFGuardTime);	// if set, marine cannot fire any weapons for a certain amount of time
	virtual void ActivateFriendlyFireGuard(CASW_Marine *pVictim);
	virtual float GetFFAbsorptionScale();
	float m_fLastFriendlyFireTime;
	float m_fFriendlyFireAbsorptionTime;				// timer for friendly fire damage
	float m_fLastAmmoCheckTime;
	bool m_bDoneWoundedRebuke;
	float m_fFriendlyFireDamage;	// keeps track of friendly fire damage done to this marine (so healing medals don't reward healing of FF damage)
	int m_pRecentAttackers[ ASW_MOB_VICTIM_SIZE ];
	float m_fLastMobDamageTime;
	bool m_bHasBeenMobAttacked;
	CHandle<CASW_Marine> m_hInfestationCurer;	// the last medic to cure us of some infestation - give him some stats if I survive the infestation
	CNetworkVar(bool, m_bOnFire);
	virtual bool TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr );
	void PerformResurrectionEffect( void );		///< issue any special effects or sounds on resurrection
	// we want to no part of this freezing business!
	void Freeze( float flFreezeAmount, CBaseEntity *pFreezer, Ray_t *pFreezeRay ) { }

	// using entities over time
	virtual bool StartUsing(CBaseEntity *pEntity);
	virtual void StopUsing();
	inline CBaseEntity *GetUsingEntity() const { return m_hUsingEntity.Get(); }
	CNetworkHandle( CBaseEntity, m_hUsingEntity );
	void SetFacingPoint(const Vector &vec, float fDuration);
	CNetworkVar(Vector, m_vecFacingPointFromServer);
	float m_fStopFacingPointTime;
	float m_fLastASWThink;
	virtual int DrawDebugTextOverlays();
	virtual void DrawDebugGeometryOverlays();
	float m_fUsingEngineeringAura;	// last time this tech marine had his engineering aura used

	// emote system
	void TickEmotes(float d);
	bool TickEmote(float d, bool bEmote, bool& bClientEmote, float& fEmoteTime);
	void DoEmote(int iEmote);
	CNetworkVar(bool, bEmoteMedic);
	CNetworkVar(bool, bEmoteAmmo);
	CNetworkVar(bool, bEmoteSmile);
	CNetworkVar(bool, bEmoteStop);
	CNetworkVar(bool, bEmoteGo);
	CNetworkVar(bool, bEmoteExclaim);
	CNetworkVar(bool, bEmoteAnimeSmile);
	CNetworkVar(bool, bEmoteQuestion);
	bool bClientEmoteMedic, bClientEmoteAmmo, bClientEmoteSmile, bClientEmoteStop,
		bClientEmoteGo, bClientEmoteExclaim, bClientEmoteAnimeSmile, bClientEmoteQuestion; 		// these are unused by the server.dll but are here for shared code purposes
	float fEmoteMedicTime, fEmoteAmmoTime, fEmoteSmileTime, fEmoteStopTime,
		fEmoteGoTime, fEmoteExclaimTime, fEmoteAnimeSmileTime, fEmoteQuestionTime;

	// driving
	virtual void StartDriving(IASW_Vehicle* pVehicle);
	virtual void StopDriving(IASW_Vehicle* pVehicle);
	IASW_Vehicle* GetASWVehicle();
	bool IsDriving() { return m_bDriving; }
	bool IsInVehicle() { return m_bIsInVehicle; }
	CNetworkHandle( CBaseEntity, m_hASWVehicle );
	CNetworkVar( bool, m_bDriving );
	CNetworkVar( bool, m_bIsInVehicle );

	// controlling a turret
	bool IsControllingTurret();
	CASW_Remote_Turret* GetRemoteTurret();
	CNetworkHandle( CASW_Remote_Turret, m_hRemoteTurret );

	// falling over
	void SetKnockedOut(bool bKnockedOut);
	CNetworkVar( bool, m_bKnockedOut );
	//CRagdollProp * m_pKnockedOutRagdoll;
	CRagdollProp* GetRagdollProp();
	EHANDLE m_hKnockedOutRagdoll;
	float m_fUnfreezeTime;

	// custom rappeling
	bool IsWaitingToRappel() { return m_bWaitingToRappel; }
	void BeginRappel();
	void SetDescentSpeed();
	void CreateZipline();
	void CutZipline();
	virtual void CleanupOnDeath( CBaseEntity *pCulprit, bool bFireDeathOutput );

	// We want to do this on our marines even when they're not using move type step (i.e. inhabited by the player)
	virtual bool ShouldCheckPhysicsContacts( void ) { return ( (GetMoveType() == MOVETYPE_STEP || Classify() == CLASS_ASW_MARINE) && VPhysicsGetObject() ); }

	virtual float		StepHeight() const			{ return 24.0f; }

	bool	m_bWaitingToRappel;
	bool	m_bOnGround;
	CHandle<CBeam> m_hLine;
	Vector	m_vecRopeAnchor;

	virtual void PhysicsLandedOnGround( float fFallSpeed );
	
	// vars from parent that we're networking
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_hGroundEntity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iHealth );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iMaxHealth );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecBaseVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_fFlags );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iAmmo );

	int m_iLightLevel;

	// skill related
	bool IsReflectingProjectiles() { return m_bReflectingProjectiles.Get(); }
	CNetworkVar( bool, m_bReflectingProjectiles );

	// stumbling
	void Stumble( CBaseEntity *pSource, const Vector &vecStumbleDir, bool bShort );
	void Knockdown( CBaseEntity *pSource, const Vector &vecKnockdownDir, bool bForce = false );
	int GetForcedActionRequest() { return m_iForcedActionRequest.Get(); }
	void ClearForcedActionRequest() { m_iForcedActionRequest = 0; }
	bool CanDoForcedAction( int iForcedAction );		// check if we're allowed to perform a forced action (certain abilities limit this)
	void RequestForcedAction( int iForcedAction ) { m_iForcedActionRequest = iForcedAction; }
	CNetworkVar( int, m_iForcedActionRequest );

	void SetNextStumbleTime( float flStumbleTime ) { m_flNextStumbleTime = flStumbleTime; }
	float m_flNextStumbleTime;
private:
	float m_flNextBreadcrumbTime;

};


#include "asw_squadformation.h"


inline CASW_SquadFormation *CASW_Marine::GetSquadFormation() // get the formation in which I participate; in the future there may be more than one per universe
{
	return &g_ASWSquadFormation;
}


inline CASW_Marine *CASW_Marine::AsMarine( CBaseEntity *pEnt )
{
	return ( pEnt && pEnt->Classify() == CLASS_ASW_MARINE ) ? assert_cast<CASW_Marine *>(pEnt) : NULL;
}

#endif /* ASW_MARINE_H */
