//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NPC_MANHACK_H
#define NPC_MANHACK_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_basenpc_physicsflyer.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "player_pickup.h"
#ifdef EZ
#include "hl2_player.h"
#endif

// Start with the engine off and folded up.
#define SF_MANHACK_PACKED_UP			(1 << 16)
#define SF_MANHACK_NO_DAMAGE_EFFECTS	(1 << 17)
#define SF_MANHACK_USE_AIR_NODES		(1 << 18)
#define SF_MANHACK_CARRIED				(1 << 19)	// Being carried by a metrocop
#define SF_MANHACK_NO_DANGER_SOUNDS		(1 << 20)

enum
{
	MANHACK_EYE_STATE_IDLE,
	MANHACK_EYE_STATE_CHASE,
	MANHACK_EYE_STATE_CHARGE,
	MANHACK_EYE_STATE_STUNNED,
};

//-----------------------------------------------------------------------------
// Attachment points.
//-----------------------------------------------------------------------------
#define	MANHACK_GIB_HEALTH				30
#define	MANHACK_INACTIVE_HEALTH			25
#define	MANHACK_MAX_SPEED				500
#define MANHACK_BURST_SPEED				650
#define MANHACK_NPC_BURST_SPEED			800

//-----------------------------------------------------------------------------
// Movement parameters.
//-----------------------------------------------------------------------------
#define MANHACK_WAYPOINT_DISTANCE		25	// Distance from waypoint that counts as arrival.

class CSprite;
class SmokeTrail;
class CSoundPatch;

//-----------------------------------------------------------------------------
// Manhack 
//-----------------------------------------------------------------------------
#ifdef EZ
class CNPC_Manhack : public CAI_SilentSquadMember< CNPCBaseInteractive<CAI_BasePhysicsFlyingBot> >, public CDefaultPlayerPickupVPhysics
{
DECLARE_CLASS( CNPC_Manhack, CAI_SilentSquadMember< CNPCBaseInteractive<CAI_BasePhysicsFlyingBot> > );
#else
class CNPC_Manhack : public CNPCBaseInteractive<CAI_BasePhysicsFlyingBot>, public CDefaultPlayerPickupVPhysics
{
DECLARE_CLASS( CNPC_Manhack, CNPCBaseInteractive<CAI_BasePhysicsFlyingBot> );
#endif
DECLARE_SERVERCLASS();

public:
	CNPC_Manhack();
	~CNPC_Manhack();

	Class_T			Classify(void);

	bool			CorpseGib( const CTakeDamageInfo &info );
	void			Event_Dying(void);
	void			Event_Killed( const CTakeDamageInfo &info );
	int				OnTakeDamage_Alive( const CTakeDamageInfo &info );
	int				OnTakeDamage_Dying( const CTakeDamageInfo &info );
	void			TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	void			TranslateNavGoal( CBaseEntity *pEnemy, Vector &chasePosition );
	float			GetDefaultNavGoalTolerance();

	void			UpdateOnRemove( void );
#ifndef EZ
	void			KillSprites( float flDelay );
#endif

	void			OnStateChange( NPC_STATE OldState, NPC_STATE NewState );

	virtual bool	CreateVPhysics( void );

	virtual void	DeathSound( const CTakeDamageInfo &info );
	virtual bool	ShouldGib( const CTakeDamageInfo &info );

	Activity		NPC_TranslateActivity( Activity baseAct );
	virtual int		TranslateSchedule( int scheduleType );
	int				MeleeAttack1Conditions ( float flDot, float flDist );
	void			HandleAnimEvent( animevent_t *pEvent );

	bool			OverrideMove(float flInterval);
	void			MoveToTarget(float flInterval, const Vector &MoveTarget);
	void			MoveExecute_Alive(float flInterval);
	void			MoveExecute_Dead(float flInterval);
	int				MoveCollisionMask(void);

	void			TurnHeadRandomly( float flInterval );

	void			CrashTouch( CBaseEntity *pOther );

	void			StartEngine( bool fStartSound );

	virtual Vector	BodyTarget( const Vector &posSrc, bool bNoisy = true ) { return WorldSpaceCenter(); }

	virtual float	GetHeadTurnRate( void ) { return 45.0f; } // Degrees per second

	virtual void	CheckCollisions(float flInterval);
	virtual void	GatherEnemyConditions( CBaseEntity *pEnemy );
	virtual void	PlayFlySound(void);
	virtual void	PlayAttackSound(bool bHostile);
	virtual void	PlayDamagedSound(void);
	virtual void	StopLoopingSounds(void);

	void			Precache(void);
	void			RunTask( const Task_t *pTask );
	void			Spawn(void);
	void			Activate();
	void			StartTask( const Task_t *pTask );

	virtual void			BladesInit();
	virtual void			SoundInit( void );
#ifdef EZ
	virtual CSprite		* GetGlowSpritePtr(int i);
	virtual void		  SetGlowSpritePtr(int i, CSprite * sprite);
	virtual EyeGlow_t	* GetEyeGlowData(int i);
	virtual int			  GetNumGlows() { return 2; };
#else
	virtual void			StartEye( void );
#endif

	bool			HandleInteraction(int interactionType, void* data, CBaseCombatCharacter* sourceEnt);

	void			PostNPCInit( void );

	void			GatherConditions();
	void			PrescheduleThink( void );

	void			SpinBlades(float flInterval);

	virtual void	Slice( CBaseEntity *pHitEntity, float flInterval, trace_t &tr );
	void			Bump( CBaseEntity *pHitEntity, float flInterval, trace_t &tr );
	void			Splash( const Vector &vecSplashPos );

	float			ManhackMaxSpeed( void );
	virtual void	VPhysicsShadowCollision( int index, gamevcollisionevent_t *pEvent );
	void			VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );
	void			HitPhysicsObject( CBaseEntity *pOther );
	virtual void	ClampMotorForces( Vector &linear, AngularImpulse &angular );
	unsigned int	PhysicsSolidMaskForEntity( void ) const;

	// Create smoke trail!
	void CreateSmokeTrail();
	void DestroySmokeTrail();
		
	void Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner ) { return; }

	void			InputDisableSwarm( inputdata_t &inputdata );
	void			InputUnpack( inputdata_t &inputdata );
#ifdef MAPBASE
	void			InputEnableSprites( inputdata_t &inputdata );
	void			InputDisableSprites( inputdata_t &inputdata );
#endif

	// 	CDefaultPlayerPickupVPhysics
	virtual void	OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void	OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason );

	CBasePlayer *HasPhysicsAttacker( float dt );

	float GetMaxEnginePower();

	// INPCInteractive Functions
#ifdef MAPBASE
	virtual bool	CanInteractWith( CAI_BaseNPC *pUser );
#else
	virtual bool	CanInteractWith( CAI_BaseNPC *pUser ) { return false; } // Disabled for now (sjb)
#endif
	virtual	bool	HasBeenInteractedWith()	{ return m_bHackedByAlyx; }
	virtual void	NotifyInteraction( CAI_BaseNPC *pUser )
	{
		// Turn the sprites off and on again so their colors will change.
		KillSprites(0.0f);
		m_bHackedByAlyx = true; 
		StartEye();
#ifdef MAPBASE
		m_OnHacked.FireOutput(pUser, this);
#endif
	}

	virtual void	InputPowerdown( inputdata_t &inputdata )
	{
		m_iHealth = 0;
	}

	virtual void SetEyeState(int state);

#ifdef EZ2
	void		TurnIntoNemesis() { m_bNemesis = true; }

	void		InputSetNemesisManhack( inputdata_t &inputdata )
	{
		// Turn the sprites off and on again so their colors will change.
		KillSprites(0.0f);
		m_bNemesis = inputdata.value.Bool();
		StartEye();
	}
#endif

	DEFINE_CUSTOM_AI;

	DECLARE_DATADESC();

private:

	bool IsInEffectiveTargetZone( CBaseEntity *pTarget );
	void MaintainGroundHeight( void );

	void StartBurst( const Vector &vecDirection );
	void StopBurst( bool bInterruptSchedule = false );

	void UpdatePanels( void );



	bool IsFlyingActivity( Activity baseAct );

	// Computes the slice bounce velocity
	void ComputeSliceBounceVelocity( CBaseEntity *pHitEntity, trace_t &tr );

	// Take damage from being thrown by a physcannon 
	void TakeDamageFromPhyscannon( CBasePlayer *pPlayer );

	// Take damage from a vehicle: 
	void TakeDamageFromVehicle( int index, gamevcollisionevent_t *pEvent );

	// Take damage from physics impacts
	void TakeDamageFromPhysicsImpact( int index, gamevcollisionevent_t *pEvent );

	// Are we being held by the physcannon?
	bool IsHeldByPhyscannon( );

	void ShowHostile(bool hostile = true);
	void StartLoitering( const Vector &vecLoiterPosition );
	void StopLoitering() { m_vecLoiterPosition = vec3_invalid; m_fTimeNextLoiterPulse = gpGlobals->curtime; }
	bool IsLoitering() { return m_vecLoiterPosition != vec3_invalid; }
	void Loiter();

	//
	// Movement variables.
	//

	Vector			m_vForceVelocity;		// Someone forced me to move

	Vector			m_vTargetBanking;

	Vector			m_vForceMoveTarget;		// Will fly here
	float			m_fForceMoveTime;		// If time is less than this
	Vector			m_vSwarmMoveTarget;		// Will fly here
	float			m_fSwarmMoveTime;		// If time is less than this
	float			m_fEnginePowerScale;	// scale all thrust by this amount (usually 1.0!)

	float			m_flNextEngineSoundTime;
	float			m_flEngineStallTime;

	float			m_flNextBurstTime;
	float			m_flBurstDuration;
	Vector			m_vecBurstDirection;

	float			m_flWaterSuspendTime;
	int				m_nLastSpinSound;

	// physics influence
	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;

	// Death
	float			m_fSparkTime;
	float			m_fSmokeTime;

	bool			m_bDirtyPitch; // indicates whether we want the sound pitch updated.(sjb)
	bool			m_bShowingHostile;

	bool			m_bBladesActive;
	bool			m_bIgnoreClipbrushes;

	float			m_flBladeSpeed;
#ifndef EZ // Declared in BaseNPC
	CSprite			*m_pEyeGlow;
#endif
	CSprite			*m_pLightGlow;
	
	CHandle<SmokeTrail>	m_hSmokeTrail;
#ifdef MAPBASE
	EHANDLE			m_hPrevOwner;

	bool			m_bNoSprites;
#endif

	int				m_iPanel1;
	int				m_iPanel2;
	int				m_iPanel3;
	int				m_iPanel4;

	int				m_nLastWaterLevel;
	bool			m_bDoSwarmBehavior;
	bool			m_bGib;

	bool			m_bHeld;
	bool			m_bHackedByAlyx;
#ifdef EZ2
	bool			m_bNemesis;
#endif
	Vector			m_vecLoiterPosition;
	float			m_fTimeNextLoiterPulse;

	float			m_flBumpSuppressTime;

	CNetworkVar( int,	m_nEnginePitch1 );
	CNetworkVar( int,	m_nEnginePitch2 );
	CNetworkVar( float,	m_flEnginePitch1Time );
	CNetworkVar( float,	m_flEnginePitch2Time );

	//NEW CONTROLLABLE STUFF:
	bool m_bControllable;
	bool m_bShouldFollowPlayer;

public:
	void		SetControllable(bool bControllable);
	void		ShouldFollowPlayer(bool bFollow) { m_bShouldFollowPlayer = bFollow; }
	void		ShowRedGlow(bool bHide);

	//void		SetHeadYaw(float HeadYaw);

	void		MoveUpDown(float direction);
	void		MoveForwardBack(float direction, QAngle angManhackEye);
	//void TurnLeft();
	//void TurnRight();

	Vector		GetManhackView();
	//Vector		m_vCollisionView;		//NOTE: I think this is not even used at the moment

	void		ComeBackToPlayer(CBasePlayer *pPlayer, float fCallBackTime);
	void		GoThere(CBasePlayer *pPlayer, float fGoHereTime);

};

#endif	//NPC_MANHACK_H
