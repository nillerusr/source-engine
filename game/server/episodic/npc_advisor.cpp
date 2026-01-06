//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Advisors. Large sluglike aliens with creepy psychic powers!
//
//=============================================================================

#include "cbase.h"
#include "game.h"
#include "ai_basenpc.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_hint.h"
#include "ai_motor.h"
#include "ai_navigator.h"
#include "beam_shared.h"
#include "hl2_shareddefs.h"
#include "ai_route.h"
#include "npcevent.h"
#include "gib.h"
#include "ai_interactions.h"
#include "ndebugoverlay.h"
#include "physics_saverestore.h"
#include "saverestore_utlvector.h"
#include "soundent.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "movevars_shared.h"
#include "particle_parse.h"
#include "weapon_physcannon.h"
#include "ai_baseactor.h"
// #include "mathlib/noise.h"

// this file contains the definitions for the message ID constants (eg ADVISOR_MSG_START_BEAM etc)
#include "npc_advisor_shared.h"

#include "ai_default.h"
#include "ai_task.h"
#include "npcevent.h"
#include "activitylist.h"

#ifdef EZ2
#include "grenade_hopwire.h"
#include "npc_scanner.h"
#include "bone_setup.h"
#include "npc_turret_floor.h"
#include "IEffects.h"
#include "collisionutils.h"
#include "scripted.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
// Custom activities.
//

//
// Skill settings.
//
ConVar sk_advisor_health( "sk_advisor_health", "0" );
ConVar advisor_use_impact_table("advisor_use_impact_table","1",FCVAR_NONE,"If true, advisor will use her custom impact damage table.");

ConVar sk_advisor_melee_dmg("sk_advisor_melee_dmg", "0");
ConVar sk_advisor_pin_speed( "sk_advisor_pin_speed", "32" );

#ifdef EZ2
ConVar sk_advisor_pin_throw_cooldown( "sk_advisor_pin_throw_cooldown", "3" );

// Think contexts
static const char * ADVISOR_FLY_THINK = "FlyThink";
static const char * ADVISOR_PARTICLE_THINK = "AdvisorParticle";
#endif

#if NPC_ADVISOR_HAS_BEHAVIOR
ConVar advisor_throw_velocity( "advisor_throw_velocity", "1100" );
ConVar advisor_throw_rate( "advisor_throw_rate", "4" );					// Throw an object every 4 seconds.
ConVar advisor_throw_warn_time( "advisor_throw_warn_time", "1.0" );		// Warn players one second before throwing an object.
ConVar advisor_throw_lead_prefetch_time ( "advisor_throw_lead_prefetch_time", "0.66", FCVAR_NONE, "Save off the player's velocity this many seconds before throwing.");
ConVar advisor_throw_stage_distance("advisor_throw_stage_distance","180.0",FCVAR_NONE,"Advisor will try to hold an object this far in front of him just before throwing it at you. Small values will clobber the shield and be very bad.");
// ConVar advisor_staging_num("advisor_staging_num","1",FCVAR_NONE,"Advisor will queue up this many objects to throw at Gordon.");
ConVar advisor_throw_clearout_vel("advisor_throw_clearout_vel","200",FCVAR_NONE,"TEMP: velocity with which advisor clears things out of a throwable's way");
// ConVar advisor_staging_duration("
ConVar advisor_throw_min_mass( "advisor_throw_min_mass", "20" );
ConVar advisor_throw_max_mass( "advisor_throw_max_mass", "220" );

#ifdef EZ2
ConVar advisor_throw_stage_min_mass( "advisor_throw_stage_min_mass", "20" );
ConVar advisor_throw_stage_max_mass( "advisor_throw_stage_max_mass", "220" );

ConVar advisor_head_debounce( "advisor_head_debounce", "0.7" );
ConVar advisor_camera_influence( "advisor_camera_influence", "1.0" );
ConVar advisor_camera_debounce( "advisor_camera_debounce", "0.2" );

ConVar advisor_use_facing_override( "advisor_use_facing_override", "1" );
ConVar advisor_use_flyer_poses( "advisor_use_flyer_poses", "1" );
ConVar advisor_update_yaw( "advisor_update_yaw", "1" );

ConVar advisor_hurt_pose( "advisor_hurt_pose", "0" );
ConVar advisor_camera_flash_health_ratio( "advisor_camera_flash_health_ratio", "0.35" );
//ConVar advisor_bleed_health_ratio( "advisor_bleed_health_ratio", "0.25" );

ConVar advisor_sparks_health_ratio_max( "advisor_sparks_health_ratio_max", "0.5" );
ConVar advisor_sparks_health_ratio_min( "advisor_sparks_health_ratio_min", "0.1" );

//ConVar advisor_push_max_radius( "advisor_push_max_radius", "224.0" );
//ConVar advisor_push_delta( "advisor_push_delta", "4.0" );
//ConVar advisor_push_max_force_scale( "advisor_push_max_force_scale", "0.1" );

Activity ACT_ADVISOR_OBJECT_R_HOLD;
Activity ACT_ADVISOR_OBJECT_R_THROW;
Activity ACT_ADVISOR_OBJECT_L_HOLD;
Activity ACT_ADVISOR_OBJECT_L_THROW;
Activity ACT_ADVISOR_OBJECT_BOTH_HOLD;
Activity ACT_ADVISOR_OBJECT_BOTH_THROW;
#endif

// how long it will take an object to get hauled to the staging point
#define STAGING_OBJECT_FALLOFF_TIME 0.15f
#endif



//
// Spawnflags.
//

//
// Animation events.
//
#define ADVISOR_MELEE_LEFT					( 3 )
#define ADVISOR_MELEE_RIGHT					( 4 )

#ifdef EZ2
int ADVISOR_AE_DETACH_R_ARM;
int ADVISOR_AE_DETACH_L_ARM;
#endif

// Skins
#ifdef EZ2
#define ADVISOR_SKIN_NOSHIELD				0
#define ADVISOR_SKIN_SHIELD					1
#endif


#if NPC_ADVISOR_HAS_BEHAVIOR
//
// Custom schedules.
//
enum
{
	SCHED_ADVISOR_COMBAT = LAST_SHARED_SCHEDULE,
	SCHED_ADVISOR_IDLE_STAND,
	SCHED_ADVISOR_TOSS_PLAYER,
#ifdef EZ2
	SCHED_ADVISOR_STAGGER,
	SCHED_ADVISOR_MELEE_ATTACK1,
#endif
};


//
// Custom tasks.
//
enum 
{
	TASK_ADVISOR_FIND_OBJECTS = LAST_SHARED_TASK,
	TASK_ADVISOR_LEVITATE_OBJECTS,
	TASK_ADVISOR_STAGE_OBJECTS,
	TASK_ADVISOR_BARRAGE_OBJECTS,

	TASK_ADVISOR_PIN_PLAYER,

#ifdef EZ2
	TASK_ADVISOR_STAGGER,

	TASK_ADVISOR_WAIT_FOR_ACTIVITY,
#endif
};

//
// Custom conditions.
//
enum
{
	COND_ADVISOR_PHASE_INTERRUPT = LAST_SHARED_CONDITION,
#ifdef EZ2
	COND_ADVISOR_FORCE_PIN,
	COND_ADVISOR_STAGGER
#endif
};
#endif

class CNPC_Advisor;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CAdvisorLevitate : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();

public:

	// in the absence of goal entities, we float up before throwing and down after
	inline bool OldStyle( void )
	{
		return !(m_vecGoalPos1.IsValid() && m_vecGoalPos2.IsValid());
	}

	virtual simresult_e	Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );

	EHANDLE m_Advisor; ///< handle to the advisor.

	Vector m_vecGoalPos1;
	Vector m_vecGoalPos2;

	float m_flFloat;
};

BEGIN_SIMPLE_DATADESC( CAdvisorLevitate )
	DEFINE_FIELD( m_flFloat, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecGoalPos1, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecGoalPos2, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_Advisor, FIELD_EHANDLE ),
END_DATADESC()

#ifdef EZ2
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CNPC_AdvisorFlyer : public CNPC_CScanner
{
	DECLARE_CLASS( CNPC_AdvisorFlyer, CNPC_CScanner );
public:
	void			Spawn( void );
	void			Precache( void );

	virtual int	 OnTakeDamage( const CTakeDamageInfo &info ) { return 0.0f; };
	
	Disposition_t	IRelationType( CBaseEntity *pTarget );

	bool	UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer = NULL );

	virtual void StartTask( const Task_t *pTask );

	float	GetGoalDistance( void );
	float	MinGroundDist( void );

	Vector		VelocityToAvoidObstacles( float flInterval );
	void		MoveExecute_Alive( float flInterval );

	// The Advisor flyer isn't a real enemy
	bool		CanBeAnEnemyOf( CBaseEntity *pEnemy ) { return false; }
	bool		CanBeSeenBy( CAI_BaseNPC *pNPC ) { return false; }

	// Never displace the flyer - the advisor should kill it if displaced
	virtual bool	IsDisplacementImpossible() { return false; }

	// Use the base class activation function
	void		Activate() { BaseClass::Activate(); }

	virtual char		*GetEngineSound( void ) { return "NPC_AdvisorFlyer.FlyLoop"; }
	virtual char		*GetScannerSoundPrefix( void ) { return "NPC_AdvisorFlyer"; }
};

void CNPC_AdvisorFlyer::Spawn( void )
{
	BaseClass::Spawn();

	AddEffects( EF_NODRAW );
	AddEffects( EF_NOSHADOW );
	SetHullType( HULL_LARGE_CENTERED );
	SetHullSizeNormal();

	if (GetOwnerEntity())
	{
		SetMass( GetOwnerEntity()->GetMass() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AdvisorFlyer::Precache( void )
{
	PrecacheScriptSound( "NPC_AdvisorFlyer.Shoot" );
	PrecacheScriptSound( "NPC_AdvisorFlyer.Alert" );
	PrecacheScriptSound( "NPC_AdvisorFlyer.Die" );
	PrecacheScriptSound( "NPC_AdvisorFlyer.Combat" );
	PrecacheScriptSound( "NPC_AdvisorFlyer.Idle" );
	PrecacheScriptSound( "NPC_AdvisorFlyer.Pain" );
	PrecacheScriptSound( "NPC_AdvisorFlyer.TakePhoto" );
	PrecacheScriptSound( "NPC_AdvisorFlyer.AttackFlash" );
	PrecacheScriptSound( "NPC_AdvisorFlyer.DiveBombFlyby" );
	PrecacheScriptSound( "NPC_AdvisorFlyer.DiveBomb" );
	PrecacheScriptSound( "NPC_AdvisorFlyer.DeployMine" );
	PrecacheScriptSound( "NPC_AdvisorFlyer.FlyLoop" );

	BaseClass::Precache();
}

Disposition_t CNPC_AdvisorFlyer::IRelationType( CBaseEntity * pTarget )
{
	if ( GetOwnerEntity() )
	{
		CAI_BaseNPC * pNPC = GetOwnerEntity()->MyNPCPointer();
		if ( pNPC )
		{
			// Ignore targets which are not my advisor's enemy
			if ( pTarget && pNPC->GetEnemy() != pTarget )
				return D_NU;

			return pNPC->IRelationType( pTarget );
		}
	}

	return BaseClass::IRelationType( pTarget );
}

//-----------------------------------------------------------------------------
// Purpose: Update information on my enemy
// Input  :
// Output : Returns true is new enemy, false is known enemy
//-----------------------------------------------------------------------------
bool CNPC_AdvisorFlyer::UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer )
{
	if (BaseClass::UpdateEnemyMemory( pEnemy, position, pInformer ))
	{
		if ( GetOwnerEntity() && GetOwnerEntity()->MyNPCPointer() )
			GetOwnerEntity()->MyNPCPointer()->UpdateEnemyMemory( pEnemy, position, pInformer );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_AdvisorFlyer::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		// Advisor flyers do not dive bomb
		case TASK_SCANNER_SET_FLY_DIVE:
			TaskComplete();
			break;

		default:
		{
			BaseClass::StartTask( pTask );
		}
	}
}

extern void GetAvoidanceForcesForAdvisor( Vector &vecOutForce, CBaseEntity *pEntity, float flEntityRadius, float flAvoidTime );

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
Vector CNPC_AdvisorFlyer::VelocityToAvoidObstacles(float flInterval)
{
	if (!GetOwnerEntity())
		return BaseClass::VelocityToAvoidObstacles( flInterval );

	Vector vBounce = vec3_origin;

	GetAvoidanceForcesForAdvisor( vBounce, this, GetOwnerEntity()->BoundingRadius() * 2.0f, flInterval );

	// --------------------------------
	//  Avoid banging into stuff
	// --------------------------------
	trace_t tr;
	Vector vTravelDir = m_vCurrentVelocity*flInterval*4.0f;
	Vector endPos = GetAbsOrigin() + vTravelDir;
	AI_TraceEntity( GetOwnerEntity(), GetAbsOrigin(), endPos, MASK_NPCSOLID|CONTENTS_WATER, &tr);
	if (tr.fraction != 1.0)
	{	
		// Bounce off in normal 
		vBounce += tr.plane.normal * 0.5 * m_vCurrentVelocity.Length();
		return (vBounce);
	}
	
	// --------------------------------
	// Try to remain above the ground.
	// --------------------------------
	float flMinGroundDist = MinGroundDist();
	AI_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, -flMinGroundDist),
		MASK_NPCSOLID_BRUSHONLY|CONTENTS_WATER, this, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction < 1)
	{
		// Clamp veloctiy
		if (tr.fraction < 0.1)
		{
			tr.fraction = 0.1;
		}

		vBounce += Vector(0, 0, 50/tr.fraction);
	}

	return vBounce;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_AdvisorFlyer::MoveExecute_Alive(float flInterval)
{
	// Amount of noise to add to flying
	float noiseScale = 3.0f;

	// -------------------------------------------
	//  Avoid obstacles
	// -------------------------------------------
	SetCurrentVelocity( GetCurrentVelocity() + VelocityToAvoidObstacles(flInterval) );

	IPhysicsObject *pPhysics = VPhysicsGetObject();

	if ( pPhysics && pPhysics->IsAsleep() )
	{
		pPhysics->Wake();
	}

	if (GetOwnerEntity() && GetOwnerEntity()->MyNPCPointer())
	{
		Vector vecDir;
		AngleVectors( GetAbsAngles(), &vecDir );
		float flYaw = UTIL_VecToYaw( vecDir );

		if (GetOwnerEntity()->MyNPCPointer()->GetMotor()->GetIdealYaw() != flYaw)
		{
			// Ideal yaw is sometimes used by the advisor's base AI functions, like melee attacks
			// Make sure the flyer matches this
			m_fHeadYaw = flYaw;
		}
	}

	// Add time-coherent noise to the current velocity so that it never looks bolted in place.
	AddNoiseToVelocity( noiseScale );

	AdjustScannerVelocity();

	float maxSpeed = GetEnemy() ? ( GetMaxSpeed() * 2.0f ) : GetMaxSpeed();
	if ( m_nFlyMode == SCANNER_FLY_DIVE )
	{
		maxSpeed = -1;
	}

	// Limit fall speed
	LimitSpeed( maxSpeed );

	// Blend out desired velocity when launched by the physcannon
	BlendPhyscannonLaunchSpeed();

	PlayFlySound();
}

LINK_ENTITY_TO_CLASS( npc_advisor_flyer, CNPC_AdvisorFlyer );

//-----------------------------------------------------------------------------
//
// Purpose: A unique motor for the advisor which allows it to rotate on a full XYZ axis.
//			It also sets angular velocity instead of setting angles, which is much smoother for its movetype.
//
//-----------------------------------------------------------------------------
class CAI_AdvisorMotor : public CAI_BlendedMotor
{
public:
	CAI_AdvisorMotor( CAI_BaseNPC *pOuter )
		: CAI_BlendedMotor( pOuter )
	{}

	// Run the current or specified timestep worth of rotation
	virtual	void		UpdateYaw( int speed = -1 );

	void 				SetIdealYawAndPitchToTarget( const Vector &target, float noise = 0.0, float offset = 0.0 );
	
	float 				GetIdealPitch() const					{ return m_IdealPitch; 	}
	void 				SetIdealPitch( float idealYaw)		{ m_IdealPitch = idealYaw; 	}

	float				GetPitchSpeed() const;
	
	CNPC_Advisor			*GetAdvisorOuter();
	const CNPC_Advisor		*GetAdvisorOuter() const;
	CNPC_AdvisorFlyer		*GetAdvisorFlyer();
	const CNPC_AdvisorFlyer	*GetAdvisorFlyer() const;

private:
	float				m_IdealPitch;	// TODO: Save this value?
};

void CAI_AdvisorMotor::UpdateYaw( int yawSpeed )
{
	// Don't do this if we're locked
	if ( IsYawLocked() )
		return;

	// Advisors are designed to face the enemy outside of regular AI facing tasks.
	// However, if the yaw is updated during a facing task, this could result in the yaw being updated
	// multiple times per think, which causes jittering.
	// Under these circumstances, GetLastThink() usually returns as being greater than curtime.
	if (gpGlobals->curtime < GetLastThink())
		return;

	GetOuter()->UpdateTurnGesture( );

	GetOuter()->SetUpdatedYaw();
	
	if ( yawSpeed == -1 )
		yawSpeed = GetYawSpeed();

	int pitchSpeed = GetPitchSpeed();

	// We would normally use UTIL_AngleMod(), but this causes problems when we bring other dimensions into play.
	// There's most likely a specific way of using it which would work, but for now, we can do without it.

	QAngle ideal;
	QAngle current;

	QAngle angles = GetLocalAngles();
	current[0] = angles.x; // UTIL_AngleMod
	current[1] = angles.y; // UTIL_AngleMod
	current[2] = angles.z; // UTIL_AngleMod

	ideal[0] = GetIdealPitch(); // UTIL_AngleMod
	ideal[1] = GetIdealYaw(); // UTIL_AngleMod

	// Base the roll on how quickly we're changing our yaw
	CNPC_AdvisorFlyer *pFlyer = GetAdvisorFlyer();
	ideal[2] = -AngleDistance( ideal[1], current[1] ) * (pFlyer ? 1.0f : 0.5f);

	float dt = MIN( 0.2, gpGlobals->curtime - GetLastThink() );

	for (int i = 0; i < 3; i++)
	{
		// We're setting angular velocity, so don't clamp yaw
		//float newAng = AI_ClampYaw( (float)yawSpeed * 10.0, current[i], ideal[i], dt );

		if (ideal[i] != current[i])
		{
			float speed;
			switch (i)
			{
				// Pitch/Roll
				case 0:
				case 2: speed = (float)pitchSpeed * dt; break;

				// Yaw
				default:
				case 1: speed = (float)yawSpeed * dt; break;
			}

			speed *= 10.0f;

			float move = AngleDiff( ideal[i], current[i] );

			angles[i] = move;

			//if (i == 1)
			//	Msg( "Yaw changed: newAng %f, current %f, ideal %f, dt %f, yawSpeed %i\n", angles[i], current[i], ideal[i], dt, yawSpeed );
			//else if (i == 0)
			//	Msg( "Pitch changed: newAng %f, current %f, ideal %f, dt %f, pitchSpeed %i\n", angles[i], current[i], ideal[i], dt, pitchSpeed );
		}
	}
	
	// MOVETYPE_FLY doesn't interpolate angle changes, so we must change its velocity instead
	float flAdvisorRotateSpeed = pFlyer ? 5.0f : 2.0f;
	GetOuter()->SetLocalAngularVelocity( angles * flAdvisorRotateSpeed );
}

//-----------------------------------------------------------------------------

void CAI_AdvisorMotor::SetIdealYawAndPitchToTarget( const Vector &target, float noise, float offset )
{ 
	float base = CalcIdealYaw( target );
	float baseP = UTIL_VecToPitch( target - GetLocalOrigin() ) * 0.75f;
	base += offset;
	if ( noise > 0 )
	{
		noise *= 0.5;
		base += random->RandomFloat( -noise, noise );
		if ( base < 0 )
			base += 360;
		else if ( base >= 360 )
			base -= 360;
	}
	SetIdealYaw( base );
	SetIdealPitch( baseP );
}
#endif

//-----------------------------------------------------------------------------
// The advisor class.
//-----------------------------------------------------------------------------
class CNPC_Advisor : public CAI_BaseActor
{
	DECLARE_CLASS( CNPC_Advisor, CAI_BaseActor );

#if NPC_ADVISOR_HAS_BEHAVIOR
	DECLARE_SERVERCLASS();
#endif

public:

	//
	// CBaseEntity:
	//
	virtual void Activate();
	virtual void Spawn();
	virtual void Precache();
	virtual void OnRestore();
	virtual void UpdateOnRemove();

	virtual int DrawDebugTextOverlays();

#ifdef EZ2
	virtual void		SetModel( const char *szModelName );
	
	virtual CSprite		*GetGlowSpritePtr( int i );
	virtual void		SetGlowSpritePtr( int i, CSprite *sprite );
	virtual EyeGlow_t	*GetEyeGlowData( int i );
	virtual int			GetNumGlows() { return 2; }

	//-----------------------------------------------------------------------------

	enum AdvisorEyeState_t
	{
		ADVISOR_EYE_NORMAL,		// Regular cyan
		ADVISOR_EYE_CHARGE,		// Turn yellow

		// Eye "flags" (not real flags; can be turned into real flags if needed)
		ADVISOR_EYE_DAMAGED,	// Start flashing
		ADVISOR_EYE_UNDAMAGED,	// Stop flashing
	};

	void				SetEyeState( AdvisorEyeState_t state );

	//-----------------------------------------------------------------------------

	virtual bool		GetGameTextSpeechParams( hudtextparms_t &params );
	virtual	float		GetHeadDebounce( void ); // how much of previous head turn to use
	virtual void		MaintainLookTargets( float flInterval );

	//-----------------------------------------------------------------------------

	CBaseEntity *GetFollowTarget() { return m_hFollowTarget; }
	
	const CNPC_AdvisorFlyer		*GetAdvisorFlyer() const { return static_cast<const CNPC_AdvisorFlyer*>(m_hAdvisorFlyer.Get()); }
	CNPC_AdvisorFlyer			*GetAdvisorFlyer()		 { return static_cast<CNPC_AdvisorFlyer*>(m_hAdvisorFlyer.Get()); }
	
	const CAI_AdvisorMotor *GetAdvisorMotor() const { return assert_cast<const CAI_AdvisorMotor *>(this->GetMotor()); }
	CAI_AdvisorMotor *		GetAdvisorMotor()		{ return assert_cast<CAI_AdvisorMotor *>(this->GetMotor()); }

	CAI_Motor *CreateMotor()
	{
		MEM_ALLOC_CREDIT();
		return new CAI_AdvisorMotor( this );
	}

	virtual float LineOfSightDist( const Vector &vecDir = vec3_invalid, float zEye = FLT_MAX );

	//-----------------------------------------------------------------------------

	virtual CanPlaySequence_t CanPlaySequence( bool fDisregardState, int interruptLevel );

	//-----------------------------------------------------------------------------
	
	inline bool IsPreparingToThrow() { return m_hvStagedEnts.Count() > 0 && m_hvStagedEnts[0]; }

	inline bool IsThisPropStaged( CBaseEntity *pEnt ) { return m_hvStagedEnts.HasElement( pEnt ); }

	inline CBaseEntity *GetFirstStagingPosition() { return m_hvStagingPositions.Count() > 0 ? m_hvStagingPositions[0] : NULL; }

	//-----------------------------------------------------------------------------

	// This is ambiguous since arms are detachable and you can theoretically give them something else
	enum AdvisorAccessory_t
	{
		ADVISOR_ACCESSORY_NONE,

		// Arms
		ADVISOR_ACCESSORY_ARMS,
		ADVISOR_ACCESSORY_R_ARM,
		ADVISOR_ACCESSORY_L_ARM,
		ADVISOR_ACCESSORY_ARMS_STUBBED,
	};

	int GetCurrentAccessories();
	void SetCurrentAccessories( AdvisorAccessory_t iNewAccessory );

	CBaseAnimating *CreateDetachedArm( bool bLeft );
#endif

	//
	// CAI_BaseNPC:
	//
	virtual float MaxYawSpeed() { return 90.0f; } //120.0f 90.0f
	inline float MaxPitchSpeed() const { return 10.0f; }
	
	virtual Class_T Classify();

#ifdef EZ2
	Disposition_t	IRelationType( CBaseEntity *pTarget );
#endif

#if NPC_ADVISOR_HAS_BEHAVIOR
	virtual int GetSoundInterests();
	virtual int SelectSchedule();
#ifdef MAPBASE
	virtual void BuildScheduleTestBits();
	virtual void PrescheduleThink();
#endif
	virtual void StartTask( const Task_t *pTask );
	virtual void RunTask( const Task_t *pTask );
	virtual void OnScheduleChange( void );

	void HandleAnimEvent(animevent_t *pEvent);
	int MeleeAttack1Conditions(float flDot, float flDist);

	void Event_Killed(const CTakeDamageInfo &info);

#ifdef EZ2
	virtual bool IsShieldOn( void );
	virtual void ApplyShieldedEffects( void );

	virtual int	TranslateSchedule( int scheduleType );

	Activity 	NPC_TranslateActivity( Activity activity );

	void StartFlying();
	void StopFlying();
	void FlyThink();

	void UpdateAdvisorFacing();
	
	void			StartSounds( void );
	virtual void	StopLoopingSounds();

	bool	HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt );

	// Override to handle shield
	void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
#endif

#endif

	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void IdleSound();
	virtual void AlertSound();

#if NPC_ADVISOR_HAS_BEHAVIOR
	virtual bool QueryHearSound( CSound *pSound );
	virtual void GatherConditions( void );

	/// true iff I recently threw the given object (not so fast)
	bool DidThrow(const CBaseEntity *pEnt);
#else
	inline bool DidThrow(const CBaseEntity *pEnt) { return false; }
#endif

	virtual bool IsHeavyDamage( const CTakeDamageInfo &info );
	virtual int	 OnTakeDamage( const CTakeDamageInfo &info );

	virtual const impactdamagetable_t &GetPhysicsImpactDamageTable( void );
	COutputInt   m_OnHealthIsNow;

#if NPC_ADVISOR_HAS_BEHAVIOR

	DEFINE_CUSTOM_AI;

	void InputSetThrowRate( inputdata_t &inputdata );
	void InputWrenchImmediate( inputdata_t &inputdata ); ///< immediately wrench an object into the air
	void InputSetStagingNum( inputdata_t &inputdata );
	void InputPinPlayer( inputdata_t &inputdata );
	void InputTurnBeamOn( inputdata_t &inputdata );
	void InputTurnBeamOff( inputdata_t &inputdata );
	void InputElightOn( inputdata_t &inputdata );
	void InputElightOff( inputdata_t &inputdata );

	void StopPinPlayer(inputdata_t &inputdata);

#ifdef EZ2
	void InputStartFlying( inputdata_t &inputdata );
	void InputStopFlying( inputdata_t &inputdata );
	void InputSetFollowTarget( inputdata_t &inputdata );
	void InputStartPsychicShield( inputdata_t &inputdata );
	void InputStopPsychicShield( inputdata_t &inputdata );
	void InputDetachRightArm( inputdata_t &inputdata );
	void InputDetachLeftArm( inputdata_t &inputdata );
	void InputDisablePinFailsafe( inputdata_t &inputdata ) { m_bPinFailsafeActive = false; }
	void InputEnablePinFailsafe( inputdata_t &inputdata ) { m_bPinFailsafeActive = true; }

	COutputEvent m_OnMindBlast;
#endif

	COutputEvent m_OnPickingThrowable, m_OnThrowWarn, m_OnThrow;

	enum { kMaxThrownObjectsTracked = 4 };
#endif

	DECLARE_DATADESC();

protected:

#if NPC_ADVISOR_HAS_BEHAVIOR
	Vector GetThrowFromPos( CBaseEntity *pEnt ); ///< Get the position in which we shall hold an object prior to throwing it
#endif

	bool CanLevitateEntity( CBaseEntity *pEntity, int minMass, int maxMass );
	void StartLevitatingObjects( void );

#ifdef EZ2
	void BeginLevitateObject( CBaseEntity *pEnt );
	void EndLevitateObject( CBaseEntity *pEnt );
#endif


#if NPC_ADVISOR_HAS_BEHAVIOR
	// void PurgeThrownObjects(); ///< clean out the recently thrown objects array
	void AddToThrownObjects(CBaseEntity *pEnt); ///< add to the recently thrown objects array

	void HurlObjectAtPlayer( CBaseEntity *pEnt, const Vector &leadVel );
	void PullObjectToStaging( CBaseEntity *pEnt, const Vector &stagingPos );
	CBaseEntity *ThrowObjectPrepare( void );

	CBaseEntity *PickThrowable( bool bRequireInView ); ///< choose an object to throw at the player (so it can get stuffed in the handle array)

	/// push everything out of the way between an object I'm about to throw and the player.
	void PreHurlClearTheWay( CBaseEntity *pThrowable, const Vector &toPos );
#endif

	CUtlVector<EHANDLE>	m_physicsObjects;
	IPhysicsMotionController *m_pLevitateController;
	CAdvisorLevitate m_levitateCallback;
	
	EHANDLE m_hLevitateGoal1;
	EHANDLE m_hLevitateGoal2;
	EHANDLE m_hLevitationArea;

#if NPC_ADVISOR_HAS_BEHAVIOR
	// EHANDLE m_hThrowEnt;
	CUtlVector<EHANDLE>	m_hvStagedEnts;
	CUtlVector<EHANDLE>	m_hvStagingPositions; 
	// todo: write accessor functions for m_hvStagedEnts so that it doesn't have members added and removed willy nilly throughout
	// code (will make the networking below more reliable)

	void Write_BeamOn(  CBaseEntity *pEnt ); 	///< write a message turning a beam on
	void Write_BeamOff( CBaseEntity *pEnt );   	///< write a message turning a beam off
	void Write_AllBeamsOff( void );				///< tell client to kill all beams

	int beamonce = 1; //Makes sure it only plays once the beam

	// for the pin-the-player-to-something behavior
	EHANDLE m_hPlayerPinPos;
	float  m_playerPinFailsafeTime;

	// keep track of up to four objects after we have thrown them, to prevent oscillation or levitation of recently thrown ammo.
	EHANDLE m_haRecentlyThrownObjects[kMaxThrownObjectsTracked]; 
	float   m_flaRecentlyThrownObjectTimes[kMaxThrownObjectsTracked];
#endif

	string_t m_iszLevitateGoal1;
	string_t m_iszLevitateGoal2;
	string_t m_iszLevitationArea;

#ifdef EZ2
	// Start glow effects for this NPC
	virtual void StartEye( void );
	// Context think function for advisor particle
	void ParticleThink();

	// The way the advisor's throw particle effect works is mostly clientside... awkward. I think in order to make the gameplay match up with the effect, it should not be saved.
	// This will cause some problems saving and loading during an advisor's attack
	bool m_bThrowBeamShield; 
	// "Throw beam shield" is for the effect when the advisor picks up a prop, "Psychic shield" is for the active shield it puts up
	bool m_bPsychicShieldOn;
	bool m_bPinFailsafeActive = true;
	bool m_bAdvisorFlyer;
	EHANDLE m_hAdvisorFlyer;

	// The target entity the advisor should follow. When "flying", this is equivalent to m_hAdvisorFlyer.
	EHANDLE m_hFollowTarget;

	// 0 = right, 1 = left, 2 = both
	int m_iThrowAnimDir;

	float m_fStaggerUntilTime;

	bool m_bBleeding;

	bool m_bLevitatingObjects; // For save/restore

	// Consider m_pEyeGlow to be m_pCameraGlow1
	CSprite *m_pCameraGlow2;

	CSoundPatch *m_pBreathSound;

	Vector m_goalEyeDirection;

	PoseParameter_t		m_ParameterCameraYaw;			// "camera_yaw"
	PoseParameter_t		m_ParameterCameraPitch;			// "camera_pitch"
	PoseParameter_t		m_ParameterFlexHorz;			// "flex_horz"
	PoseParameter_t		m_ParameterFlexVert;			// "flex_vert"
	PoseParameter_t		m_ParameterArmsExtent;			// "arms_extent"
	PoseParameter_t		m_ParameterHurt;				// "hurt"

	static int gm_nMouthAttachment;
	static int gm_nLFrontTopJetAttachment;
	static int gm_nRFrontTopJetAttachment;
	static int gm_nLRearTopJetAttachment;
	static int gm_nRRearTopJetAttachment;
#endif


#if NPC_ADVISOR_HAS_BEHAVIOR
	string_t m_iszStagingEntities;
	string_t m_iszPriorityEntityGroupName;

	float m_flStagingEnd; 
	float m_flThrowPhysicsTime;
	float m_flLastThrowTime;
	float m_flLastPlayerAttackTime; ///< last time the player attacked something. 

	int   m_iStagingNum; ///< number of objects advisor stages at once
	bool  m_bWasScripting;

	// unsigned char m_pickFailures; // the number of times we have tried to pick a throwable and failed 

	Vector m_vSavedLeadVel; ///< save off player velocity for leading a bit before actually pelting them. 
#endif
};

#ifdef EZ2
int CNPC_Advisor::gm_nMouthAttachment = -1;
int CNPC_Advisor::gm_nLFrontTopJetAttachment = -1;
int CNPC_Advisor::gm_nRFrontTopJetAttachment = -1;
int CNPC_Advisor::gm_nLRearTopJetAttachment = -1;
int CNPC_Advisor::gm_nRRearTopJetAttachment = -1;
#endif

LINK_ENTITY_TO_CLASS( npc_advisor, CNPC_Advisor );

BEGIN_DATADESC( CNPC_Advisor )

	DEFINE_KEYFIELD( m_iszLevitateGoal1, FIELD_STRING, "levitategoal_bottom" ),
	DEFINE_KEYFIELD( m_iszLevitateGoal2, FIELD_STRING, "levitategoal_top" ),
	DEFINE_KEYFIELD( m_iszLevitationArea, FIELD_STRING, "levitationarea"), ///< we will float all the objects in this volume

	DEFINE_PHYSPTR( m_pLevitateController ),
	DEFINE_EMBEDDED( m_levitateCallback ),
	DEFINE_UTLVECTOR( m_physicsObjects, FIELD_EHANDLE ),

	DEFINE_FIELD( m_hLevitateGoal1, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hLevitateGoal2, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hLevitationArea, FIELD_EHANDLE ),

#if NPC_ADVISOR_HAS_BEHAVIOR
	DEFINE_KEYFIELD( m_iszStagingEntities, FIELD_STRING, "staging_ent_names"), ///< entities named this constitute the positions to which we stage objects to be thrown
	DEFINE_KEYFIELD( m_iszPriorityEntityGroupName, FIELD_STRING, "priority_grab_name"),
#ifdef EZ2
	DEFINE_KEYFIELD( m_bAdvisorFlyer, FIELD_BOOLEAN, "AdvisorFlyer" ),
	DEFINE_KEYFIELD( m_bPinFailsafeActive, FIELD_BOOLEAN, "pin_failsafe_active"),

	DEFINE_FIELD( m_hAdvisorFlyer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hFollowTarget, FIELD_EHANDLE ),

	DEFINE_FIELD( m_bPsychicShieldOn, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fStaggerUntilTime, FIELD_TIME ),

	DEFINE_FIELD( m_bBleeding, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_bLevitatingObjects, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_iThrowAnimDir, FIELD_INTEGER ),

	DEFINE_FIELD( m_goalEyeDirection, FIELD_VECTOR ),

	DEFINE_FIELD( m_ParameterCameraYaw, FIELD_INTEGER ),
	DEFINE_FIELD( m_ParameterCameraPitch, FIELD_INTEGER ),
	DEFINE_FIELD( m_ParameterFlexHorz, FIELD_INTEGER ),
	DEFINE_FIELD( m_ParameterFlexVert, FIELD_INTEGER ),
	DEFINE_FIELD( m_ParameterArmsExtent, FIELD_INTEGER ),
	DEFINE_FIELD( m_ParameterHurt, FIELD_INTEGER ),
#endif

	DEFINE_UTLVECTOR( m_hvStagedEnts, FIELD_EHANDLE ),
	DEFINE_UTLVECTOR( m_hvStagingPositions, FIELD_EHANDLE ),
	DEFINE_ARRAY( m_haRecentlyThrownObjects, FIELD_EHANDLE, CNPC_Advisor::kMaxThrownObjectsTracked ),
	DEFINE_ARRAY( m_flaRecentlyThrownObjectTimes, FIELD_TIME, CNPC_Advisor::kMaxThrownObjectsTracked ),

	DEFINE_FIELD( m_hPlayerPinPos, FIELD_EHANDLE ),
	DEFINE_FIELD( m_playerPinFailsafeTime, FIELD_TIME ),

	// DEFINE_FIELD( m_hThrowEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flThrowPhysicsTime, FIELD_TIME ),
	DEFINE_FIELD( m_flLastPlayerAttackTime, FIELD_TIME ),
	DEFINE_FIELD( m_flStagingEnd, FIELD_TIME ),
	DEFINE_FIELD( m_iStagingNum, FIELD_INTEGER ),
	DEFINE_FIELD( m_bWasScripting, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_flLastThrowTime, FIELD_TIME ),
	DEFINE_FIELD( m_vSavedLeadVel, FIELD_VECTOR ),

	DEFINE_OUTPUT( m_OnPickingThrowable, "OnPickingThrowable" ),
	DEFINE_OUTPUT( m_OnThrowWarn, "OnThrowWarn" ),
	DEFINE_OUTPUT( m_OnThrow, "OnThrow" ),
	DEFINE_OUTPUT( m_OnHealthIsNow, "OnHealthIsNow" ),
#ifdef EZ2
	DEFINE_OUTPUT( m_OnMindBlast, "OnMindBlast" ),

	DEFINE_SOUNDPATCH( m_pBreathSound ),
#endif

	DEFINE_INPUTFUNC( FIELD_FLOAT,   "SetThrowRate",    InputSetThrowRate ),
	DEFINE_INPUTFUNC( FIELD_STRING,  "WrenchImmediate", InputWrenchImmediate ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetStagingNum",   InputSetStagingNum),
	DEFINE_INPUTFUNC( FIELD_STRING,  "PinPlayer",       InputPinPlayer ),
	DEFINE_INPUTFUNC( FIELD_STRING,  "BeamOn",          InputTurnBeamOn ),
	DEFINE_INPUTFUNC( FIELD_STRING,  "BeamOff",         InputTurnBeamOff ),
	DEFINE_INPUTFUNC( FIELD_STRING,  "ElightOn",         InputElightOn ),
	DEFINE_INPUTFUNC( FIELD_STRING,  "ElightOff",         InputElightOff ),

	DEFINE_INPUTFUNC(FIELD_VOID, "StopPinPlayer", StopPinPlayer),

#ifdef EZ2
	DEFINE_INPUTFUNC( FIELD_VOID, "StartFlying", InputStartFlying ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StopFlying", InputStopFlying ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "SetFollowTarget", InputSetFollowTarget ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnablePinFailsafe", InputEnablePinFailsafe ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisablePinFailsafe", InputDisablePinFailsafe ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StartPsychicShield", InputStartPsychicShield ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StopPsychicShield", InputStopPsychicShield ),

	DEFINE_INPUTFUNC( FIELD_VOID, "DetachRightArm", InputDetachRightArm ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DetachLeftArm", InputDetachLeftArm ),

	DEFINE_THINKFUNC( FlyThink )
#endif

#endif

END_DATADESC()



#if NPC_ADVISOR_HAS_BEHAVIOR
IMPLEMENT_SERVERCLASS_ST(CNPC_Advisor, DT_NPC_Advisor)

END_SEND_TABLE()
#endif


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Advisor::Spawn()
{
	BaseClass::Spawn();

#ifdef _XBOX
	// Always fade the corpse
	AddSpawnFlags( SF_NPC_FADE_CORPSE );
#endif // _XBOX

	Precache();

	GetAbsOrigin();

	SetModel( STRING( GetModelName() ) );

	m_iHealth = sk_advisor_health.GetFloat();
	m_takedamage = DAMAGE_NO;

	SetHullType( HULL_LARGE_CENTERED );
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags(FSOLID_NOT_STANDABLE); //FSOLID_NOT_SOLID

	SetMoveType( MOVETYPE_FLY );
#ifdef EZ2
	// Advisors cannot move unassisted
	CapabilitiesRemove( bits_CAP_MOVE_GROUND );

	// Advisors are slugs with CHARACTER!
	CapabilitiesAdd( bits_CAP_TURN_HEAD | bits_CAP_ANIMATEDFACE );
#endif

	m_flFieldOfView = VIEW_FIELD_FULL;
	SetViewOffset( Vector( 0, 0, 80 ) );		// Position of the eyes relative to NPC's origin.

	SetBloodColor( BLOOD_COLOR_GREEN );
	m_NPCState = NPC_STATE_NONE;

	//CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);

	NPCInit();

	SetGoalEnt( NULL );

	AddEFlags( EFL_NO_DISSOLVE );

#ifdef EZ2
	GetMotor()->RecalculateYawSpeed();

	SetActivity( ACT_IDLE );
#endif
}


#if NPC_ADVISOR_HAS_BEHAVIOR
//-----------------------------------------------------------------------------
// comparison function for qsort used below. Compares "StagingPriority" keyfield
//-----------------------------------------------------------------------------
int __cdecl AdvisorStagingComparator(const EHANDLE *pe1, const EHANDLE *pe2)
{
	// bool ReadKeyField( const char *varName, variant_t *var );

	variant_t var;
	int val1 = 10, val2 = 10; // default priority is ten
	
	// read field one
	if ( pe1->Get()->ReadKeyField( "StagingPriority", &var ) )
	{
		val1 = var.Int();
	}
	
	// read field two
	if ( pe2->Get()->ReadKeyField( "StagingPriority", &var ) )
	{
		val2 = var.Int();
	}

	// return comparison (< 0 if pe1<pe2) 
	return( val1 - val2 );
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable : 4706)

void CNPC_Advisor::Activate()
{
	BaseClass::Activate();
	
	m_hLevitateGoal1  = gEntList.FindEntityGeneric( NULL, STRING( m_iszLevitateGoal1 ),  this );
	m_hLevitateGoal2  = gEntList.FindEntityGeneric( NULL, STRING( m_iszLevitateGoal2 ),  this );
	m_hLevitationArea = gEntList.FindEntityGeneric( NULL, STRING( m_iszLevitationArea ), this );

	m_levitateCallback.m_Advisor = this;

#if NPC_ADVISOR_HAS_BEHAVIOR
	// load the staging positions
	CBaseEntity *pEnt = NULL;
	m_hvStagingPositions.EnsureCapacity(6); // reserve six

	// conditional assignment: find an entity by name and save it into pEnt. Bail out when none are left.
	while ( pEnt = gEntList.FindEntityByName(pEnt,m_iszStagingEntities) )
	{
		m_hvStagingPositions.AddToTail(pEnt);
	}

	// sort the staging positions by their staging number.
	m_hvStagingPositions.Sort( AdvisorStagingComparator );

	// positions loaded, null out the m_hvStagedEnts array with exactly as many null spaces
	m_hvStagedEnts.SetCount( m_hvStagingPositions.Count() );

	m_iStagingNum = 1;

	AssertMsg(m_hvStagingPositions.Count() > 0, "You did not specify any staging positions in the advisor's staging_ent_names !");
#endif

#ifdef EZ2
	if ( m_bAdvisorFlyer )
	{
		StartFlying();
	}

	// Start looping sounds
	StartSounds();
#endif
}
#pragma warning(pop)


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Advisor::UpdateOnRemove()
{
	if ( m_pLevitateController )
	{
		physenv->DestroyMotionController( m_pLevitateController );

#ifdef EZ2
		for (int i = m_physicsObjects.Count()-1; i >= 0; i--)
		{
			EndLevitateObject( m_physicsObjects[i] );
		}
#endif
	}
	
	BaseClass::UpdateOnRemove();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Advisor::OnRestore()
{
	BaseClass::OnRestore();
#ifdef EZ2
	if (m_bLevitatingObjects)
#endif
	StartLevitatingObjects();
}

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Advisor::SetModel( const char *szModelName )
{
	BaseClass::SetModel( szModelName );

	Init( m_ParameterCameraYaw, "camera_yaw" );
	Init( m_ParameterCameraPitch, "camera_pitch" );
	Init( m_ParameterFlexHorz, "flex_horz" );
	Init( m_ParameterFlexVert, "flex_vert" );
	Init( m_ParameterArmsExtent, "arms_extent" );
	Init( m_ParameterHurt, "hurt" );

	gm_nMouthAttachment = LookupAttachment( "attach_mouth" );
	gm_nLFrontTopJetAttachment = LookupAttachment( "attach_l_front_top_jet" );
	gm_nRFrontTopJetAttachment = LookupAttachment( "attach_r_front_top_jet" );
	gm_nLRearTopJetAttachment = LookupAttachment( "attach_l_front_top_jet1" );
	gm_nRRearTopJetAttachment = LookupAttachment( "attach_r_rear_top_jet" );
}

//-----------------------------------------------------------------------------
// Purpose: Return the pointer for a given sprite given an index
//-----------------------------------------------------------------------------
CSprite	*CNPC_Advisor::GetGlowSpritePtr( int index )
{
	switch (index)
	{
		case 0:
			return m_pEyeGlow;
		case 1:
			return m_pCameraGlow2;
	}

	return BaseClass::GetGlowSpritePtr( index );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the glow sprite at the given index
//-----------------------------------------------------------------------------
void CNPC_Advisor::SetGlowSpritePtr( int index, CSprite *sprite )
{
	switch (index)
	{
		case 0:
			m_pEyeGlow = sprite;
			break;
		case 1:
			m_pCameraGlow2 = sprite;
			break;
	}

	BaseClass::SetGlowSpritePtr( index, sprite );
}

//-----------------------------------------------------------------------------
// Purpose: Return the glow attributes for a given index
//-----------------------------------------------------------------------------
EyeGlow_t *CNPC_Advisor::GetEyeGlowData( int index )
{
	EyeGlow_t * eyeGlow = new EyeGlow_t();

	switch (index){
	case 0:
		eyeGlow->spriteName = MAKE_STRING( "sprites/light_glow02.vmt" );
		eyeGlow->attachment = MAKE_STRING( "camera_bottom_lens" );

		eyeGlow->alpha = 255;
		eyeGlow->brightness = 224;

		eyeGlow->red = 0;
		eyeGlow->green = 255;
		eyeGlow->blue = 255;

		eyeGlow->renderMode = kRenderWorldGlow;
		eyeGlow->scale = 0.1f;
		eyeGlow->proxyScale = 2.0f;

		return eyeGlow;
	case 1:
		eyeGlow->spriteName = MAKE_STRING( "sprites/light_glow02.vmt" );
		eyeGlow->attachment = MAKE_STRING( "camera_top_lens" );

		eyeGlow->alpha = 255;
		eyeGlow->brightness = 224;

		eyeGlow->red = 0;
		eyeGlow->green = 255;
		eyeGlow->blue = 255;

		eyeGlow->renderMode = kRenderWorldGlow;
		eyeGlow->scale = 0.1f;
		eyeGlow->proxyScale = 2.0f;

		return eyeGlow;
	}

	return BaseClass::GetEyeGlowData(index);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Advisor::SetEyeState( AdvisorEyeState_t state )
{
	if (!m_pEyeGlow || !m_pCameraGlow2)
		return;

	switch (state)
	{
		case ADVISOR_EYE_NORMAL:
			{
				if (IsShieldOn())
				{
					m_pEyeGlow->SetRenderColor( 255, 255, 255 );
					m_pCameraGlow2->SetRenderColor( 255, 255, 255 );

					m_pEyeGlow->SetScale( 0.25 );
					m_pCameraGlow2->SetScale( 0.25 );
				}
				else
				{
					m_pEyeGlow->SetRenderColor( 0, 255, 255 );
					m_pCameraGlow2->SetRenderColor( 0, 255, 255 );

					m_pEyeGlow->SetScale( 0.1 );
					m_pCameraGlow2->SetScale( 0.1 );
				}
			} break;
			
		case ADVISOR_EYE_CHARGE:
			{
				// Turn yellow
				m_pEyeGlow->SetRenderColor( 255, 255, 0 );
				m_pCameraGlow2->SetRenderColor( 255, 255, 0 );
			} break;
			
		case ADVISOR_EYE_DAMAGED:
			{
				// Start flashing
				m_pEyeGlow->m_nRenderFX = kRenderFxFlickerSlow;
				m_pCameraGlow2->m_nRenderFX = kRenderFxFlickerSlow;
			} break;
			
		case ADVISOR_EYE_UNDAMAGED:
			{
				// Stop flashing
				m_pEyeGlow->m_nRenderFX = kRenderFxNone;
				m_pCameraGlow2->m_nRenderFX = kRenderFxNone;
			} break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parameters for scene event AI_GameText
//-----------------------------------------------------------------------------
bool CNPC_Advisor::GetGameTextSpeechParams( hudtextparms_t &params )
{
	params.channel = 3;
	params.x = -1;
	params.y = 0.6;
	params.effect = 0;

	params.r1 = 136;
	params.g1 = 180;
	params.b1 = 185;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_Advisor::GetHeadDebounce( void )
{
	return advisor_head_debounce.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Maintain eye, head, body postures, etc.
//-----------------------------------------------------------------------------
void CNPC_Advisor::MaintainLookTargets( float flInterval )
{
	BaseClass::MaintainLookTargets( flInterval );

	// Turn our eye
	if (HasCondition(COND_IN_PVS))
	{
		// Determine eye target position
		Vector vecEyeTarget = EyePosition() + (EyeDirection3D() * 100.0f);
		if (GetEnemy() && m_flThrowPhysicsTime + advisor_throw_warn_time.GetFloat() >= gpGlobals->curtime)
		{
			vecEyeTarget = GetEnemy()->EyePosition();
		}
		else if (GetLooktarget())
		{
			vecEyeTarget = GetLooktarget()->EyePosition();
		}

		// calc current animation head bias, movement needs to clamp accumulated with this
		QAngle angBias;
		QAngle vTargetAngles;

		int iCamera = LookupAttachment( "camera" );
		int iForward = LookupAttachment( "forward" );

		matrix3x4_t eyesToWorld;
		matrix3x4_t forwardToWorld, worldToForward;

		GetAttachment( iCamera, eyesToWorld );

		GetAttachment( iForward, forwardToWorld );
		MatrixInvert( forwardToWorld, worldToForward );

		MatrixInvert( forwardToWorld, worldToForward );

		matrix3x4_t targetXform;
		targetXform = forwardToWorld;
		Vector vTargetDir = vecEyeTarget - EyePosition();

		Studio_AlignIKMatrix( targetXform, vTargetDir );

		matrix3x4_t headXform;
		ConcatTransforms( worldToForward, targetXform, headXform );
		MatrixAngles( headXform, vTargetAngles );

		// partially debounce head goal
		float s0 = 1.0 - advisor_camera_influence.GetFloat() + advisor_camera_debounce.GetFloat() * advisor_camera_influence.GetFloat();
		float s1 = (1.0 - s0);
		// limit velocity of head turns
		m_goalEyeDirection.x = UTIL_Approach( m_goalEyeDirection.x * s0 + vTargetAngles.x * s1, m_goalEyeDirection.x, 10.0 );
		m_goalEyeDirection.y = UTIL_Approach( m_goalEyeDirection.y * s0 + vTargetAngles.y * s1, m_goalEyeDirection.y, 30.0 );
		//m_goalEyeDirection.z = UTIL_Approach( m_goalEyeDirection.z * s0 + vTargetAngles.z * s1, m_goalEyeDirection.z, 10.0 );

		/*
		if ( (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT) )
		{
			// Msg( "yaw %.1f (%f) pitch %.1f (%.1f)\n", m_goalEyeDirection.y, vTargetAngles.y, vTargetAngles.x, m_goalEyeDirection.x );
			// Msg( "yaw %.2f (goal %.2f) (influence %.2f) (flex %.2f)\n", flLimit, m_goalEyeDirection.y, flHeadInfluence, Get( m_FlexweightHeadRightLeft ) );
		}
		*/

		float flTarget;

		flTarget = ClampWithBias( m_ParameterCameraYaw, m_goalEyeDirection.y, angBias.y );
		if ( (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT) )
		{
			Msg( "yaw  %5.1f : (%5.1f : 0.0) %5.1f (%5.1f)\n", flTarget, m_goalEyeDirection.y/*, Get( m_FlexweightHeadRightLeft )*/, angBias.y, Get( m_ParameterCameraYaw ) );
		}
		Set( m_ParameterCameraYaw, flTarget );

		flTarget = ClampWithBias( m_ParameterCameraPitch, m_goalEyeDirection.x, angBias.x );
		if ( (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT) )
		{
			Msg( "pitch %5.1f : (%5.1f : 0.0) %5.1f (%5.1f)\n", flTarget, m_goalEyeDirection.x/*, Get( m_FlexweightHeadUpDown )*/, angBias.x, Get( m_ParameterCameraPitch ) );
		}
		Set( m_ParameterCameraPitch, flTarget );
	}
}

//---------------------------------------------------------
// Pass a direction to get how far an NPC would see if facing
// that direction. Pass nothing to get the length of the NPC's
// current line of sight.
//---------------------------------------------------------
float CNPC_Advisor::LineOfSightDist( const Vector &vecDir, float zEye )
{
	Vector testDir;
	if( vecDir == vec3_invalid )
	{
		testDir = EyeDirection3D();
		testDir.x = 0;
	}
	else
	{
		testDir = vecDir;
	}

	if ( zEye == FLT_MAX ) 
		zEye = EyePosition().z;

	CTraceFilterLOS traceFilter( this, COLLISION_GROUP_NONE, m_hAdvisorFlyer );
	trace_t tr;

	// Need to center trace so don't get erratic results based on orientation
	Vector testPos( GetAbsOrigin().x, GetAbsOrigin().y, zEye ); 
	AI_TraceLOS( testPos, testPos + testDir * MAX_COORD_RANGE, this, &tr, &traceFilter );
	return (tr.startpos - tr.endpos ).Length();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CanPlaySequence_t CNPC_Advisor::CanPlaySequence( bool fDisregardNPCState, int interruptLevel )
{
	CanPlaySequence_t eReturn = BaseClass::CanPlaySequence( fDisregardNPCState, interruptLevel );

	// Allows the advisor to play scripted sequences while dying.
	// Mainly for death animations.
	if ( eReturn == CANNOT_PLAY && !IsAlive() && !m_hCine )
	{
		eReturn = CAN_PLAY_NOW;
	}

	return eReturn;
}
#endif

//-----------------------------------------------------------------------------
//  Returns this monster's classification in the relationship table.
//-----------------------------------------------------------------------------
Class_T	CNPC_Advisor::Classify()
{
	return CLASS_COMBINE;
}


#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Disposition_t CNPC_Advisor::IRelationType( CBaseEntity * pTarget )
{
	// Don't attack entities we're about to throw
	if ( m_hvStagedEnts.HasElement(pTarget) )
	{
		return D_NU;
	}

	return BaseClass::IRelationType( pTarget );
}
#endif


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Advisor::IsHeavyDamage( const CTakeDamageInfo &info )
{
	return (info.GetDamage() > 0);
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Advisor::StartLevitatingObjects()
{
	if ( !m_pLevitateController )
	{
		m_pLevitateController = physenv->CreateMotionController( &m_levitateCallback );
	}

	m_pLevitateController->ClearObjects();
	
	int nCount = m_physicsObjects.Count();
	for ( int i = 0; i < nCount; i++ )
	{
		CBaseEntity *pEnt = m_physicsObjects.Element( i );
		if ( !pEnt )
			continue;

		//NDebugOverlay::Box( pEnt->GetAbsOrigin(), pEnt->CollisionProp()->OBBMins(), pEnt->CollisionProp()->OBBMaxs(), 0, 255, 0, 1, 0.1 );

		IPhysicsObject *pPhys = pEnt->VPhysicsGetObject();
		if ( pPhys && pPhys->IsMoveable() )
		{
			m_pLevitateController->AttachObject( pPhys, false );
			pPhys->Wake();

#ifdef EZ2
			BeginLevitateObject( pEnt );
#endif
		}
	}
#ifdef EZ2
	m_bLevitatingObjects = true;
#endif
}

// This function is used by both version of the entity finder below 
bool CNPC_Advisor::CanLevitateEntity( CBaseEntity *pEntity, int minMass, int maxMass )
{
#ifdef EZ2
	// Allowed to pick up physics NPCs which aren't our flyer
	if (!pEntity || pEntity == m_hAdvisorFlyer)
#else
	if (!pEntity || pEntity->IsNPC()) 
#endif
		return false;

	IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
	if (!pPhys)
		return false;

	float mass = pPhys->GetMass();

	return ( mass >= minMass && 
			 mass <= maxMass && 
			 //pEntity->VPhysicsGetObject()->IsAsleep() && 
			 pPhys->IsMoveable() /* &&
			!DidThrow(pEntity) */ );
}

#ifdef EZ2
void CNPC_Advisor::BeginLevitateObject( CBaseEntity *pEnt )
{
	if (pEnt && pEnt->IsNPC())
	{
		// Important for stopping turrets from being annoying and strange while under no gravity
		ITippableTurret *pTippable = dynamic_cast<ITippableTurret*>(pEnt);
		if (pTippable)
		{
			//Msg( "Setting low gravity on %s\n", pEnt->GetDebugName() );
			pTippable->SetLowGravity( true );
		}
	}
}

void CNPC_Advisor::EndLevitateObject( CBaseEntity *pEnt )
{
	if (pEnt && pEnt->IsNPC())
	{
		// Important for stopping turrets from being annoying and strange while under no gravity
		ITippableTurret *pTippable = dynamic_cast<ITippableTurret*>(pEnt);
		if (pTippable)
		{
			//Msg( "Deactivating low gravity on %s\n", pEnt->GetDebugName() );
			pTippable->SetLowGravity( false );
		}
	}
}
#endif

#if NPC_ADVISOR_HAS_BEHAVIOR
// find an object to throw at the player and start the warning on it. Return object's 
// pointer if we got something. Otherwise, return NULL if nothing left to throw. Will
// always leave the prepared object at the head of m_hvStagedEnts
CBaseEntity *CNPC_Advisor::ThrowObjectPrepare()
{

	CBaseEntity *pThrowable = NULL;
	while (m_hvStagedEnts.Count() > 0)
	{
		pThrowable = m_hvStagedEnts[0];

		if (pThrowable)
		{
			IPhysicsObject *pPhys = pThrowable->VPhysicsGetObject();
			if ( !pPhys )
			{
				// reject!
				
				Write_BeamOff(m_hvStagedEnts[0]);
				pThrowable = NULL;
			}
		}

		// if we still have pThrowable...
		if (pThrowable)
		{
			// we're good
			break;
		}
		else
		{
			m_hvStagedEnts.Remove(0);
		}
	}

	if (pThrowable)
	{
		Assert( pThrowable->VPhysicsGetObject() );

		// play the sound, attach the light, fire the trigger
		EmitSound( "NPC_Advisor.ObjectChargeUp" );

#ifdef EZ2
		// TODO: Show an effect on the throwable object
		//DispatchParticleEffect( "Advisor_Throw_Obj_Charge", PATTACH_ABSORIGIN_FOLLOW, pThrowable );
#endif

		m_OnThrowWarn.FireOutput(pThrowable,this); 
		m_flThrowPhysicsTime = gpGlobals->curtime + advisor_throw_warn_time.GetFloat();

		if ( GetEnemy() )
		{
			PreHurlClearTheWay( pThrowable, GetEnemy()->EyePosition() );
		}

		return pThrowable;
	}
	else // we had nothing to throw
	{
		return NULL;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Advisor::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		// DVS: TODO: if this gets expensive we can start caching the results and doing it less often.
		case TASK_ADVISOR_FIND_OBJECTS:
		{
			// if we have a trigger volume, use the contents of that. If not, use a hardcoded box (for debugging purposes)
			// in both cases we validate the objects using the same helper funclet just above. When we can count on the
			// trigger vol being there, we can elide the else{} clause here.

			CBaseEntity *pVolume = m_hLevitationArea;
			AssertMsg(pVolume, "Combine advisor needs 'levitationarea' key pointing to a trigger volume." );

			if (!pVolume)
			{
				TaskFail( "No levitation area found!" );
				break;
			}

			touchlink_t *touchroot = ( touchlink_t * )pVolume->GetDataObject( TOUCHLINK );
			if ( touchroot )
			{
#ifdef EZ2
				for (int i = m_physicsObjects.Count()-1; i >= 0; i--)
				{
					EndLevitateObject( m_physicsObjects[i] );

					m_physicsObjects.FastRemove( i );
				}
#else
				m_physicsObjects.RemoveAll();
#endif

				for ( touchlink_t *link = touchroot->nextLink; link != touchroot; link = link->nextLink )
				{
					CBaseEntity *pTouch = link->entityTouched;
					if ( CanLevitateEntity( pTouch, advisor_throw_min_mass.GetInt(), advisor_throw_max_mass.GetInt() ) )
					{
						if ( pTouch->GetMoveType() == MOVETYPE_VPHYSICS )
						{
							//Msg( "   %d added %s\n", m_physicsObjects.Count(), STRING( list[i]->GetModelName() ) );
							m_physicsObjects.AddToTail( pTouch );

#ifdef EZ2
							BeginLevitateObject( pTouch );
#endif
						}
					}
				}
			}

			/*
			// this is the old mechanism, using a hardcoded box and an entity enumerator. 
			// since deprecated.

			else
			{
				CBaseEntity *list[128];
				
				m_physicsObjects.RemoveAll();

				//NDebugOverlay::Box( GetAbsOrigin(), Vector( -408, -368, -188 ), Vector( 92, 208, 168 ), 255, 255, 0, 1, 5 );
				
				// one-off class used to determine which entities we want from the UTIL_EntitiesInBox
				class CAdvisorLevitateEntitiesEnum : public CFlaggedEntitiesEnum
				{
				public:
					CAdvisorLevitateEntitiesEnum( CBaseEntity **pList, int listMax, int nMinMass, int nMaxMass )
					:	CFlaggedEntitiesEnum( pList, listMax, 0 ),
						m_nMinMass( nMinMass ),
						m_nMaxMass( nMaxMass )
					{
					}

					virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
					{
						CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
						if ( AdvisorCanLevitateEntity( pEntity, m_nMinMass, m_nMaxMass ) )
						{
							return CFlaggedEntitiesEnum::EnumElement( pHandleEntity );
						}
						return ITERATION_CONTINUE;
					}

					int m_nMinMass;
					int m_nMaxMass;
				};

				CAdvisorLevitateEntitiesEnum levitateEnum( list, ARRAYSIZE( list ), 10, 220 );

				int nCount = UTIL_EntitiesInBox( GetAbsOrigin() - Vector( 554, 368, 188 ), GetAbsOrigin() + Vector( 92, 208, 168 ), &levitateEnum );
				for ( int i = 0; i < nCount; i++ )
				{
					//Msg( "%d found %s\n", m_physicsObjects.Count(), STRING( list[i]->GetModelName() ) );
					if ( list[i]->GetMoveType() == MOVETYPE_VPHYSICS )
					{
						//Msg( "   %d added %s\n", m_physicsObjects.Count(), STRING( list[i]->GetModelName() ) );
						m_physicsObjects.AddToTail( list[i] );
					}
				}
			}
			*/

			if ( m_physicsObjects.Count() > 0 )
			{
				TaskComplete();
			}
			else
			{
				TaskFail( "No physics objects found!" );
			}

			break;
		}

		case TASK_ADVISOR_LEVITATE_OBJECTS:
		{
			StartLevitatingObjects();

			m_flThrowPhysicsTime = gpGlobals->curtime + advisor_throw_rate.GetFloat();
			
			break;
		}
		
		case TASK_ADVISOR_STAGE_OBJECTS:
		{
            // m_pickFailures = 0;
			// clear out previously staged throwables
			/*
			for (int ii = m_hvStagedEnts.Count() - 1; ii >= 0 ; --ii)
			{
				m_hvStagedEnts[ii] = NULL;
			}
			*/
			Write_AllBeamsOff();
			m_hvStagedEnts.RemoveAll();

			m_OnPickingThrowable.FireOutput(NULL,this);
			m_flStagingEnd = gpGlobals->curtime + pTask->flTaskData;

			break;
		}
	
		// we're about to pelt the player with everything. Start the warning effect on the first object.
		case TASK_ADVISOR_BARRAGE_OBJECTS:
		{

			CBaseEntity *pThrowable = ThrowObjectPrepare();

			if (!pThrowable || m_hvStagedEnts.Count() < 1)
			{
				TaskFail( "Nothing to throw!" );
				return;
			}
			
			m_vSavedLeadVel.Invalidate();

			break;
		}
		
		case TASK_ADVISOR_PIN_PLAYER:
		{

			// should never be here
			
			Assert( m_hPlayerPinPos.IsValid() );
			m_playerPinFailsafeTime = gpGlobals->curtime + 10.0f;

			break;
			
		}
#ifdef EZ2
		case TASK_ADVISOR_STAGGER:
		case TASK_ADVISOR_WAIT_FOR_ACTIVITY:
			break;
#endif
		default:
		{
			BaseClass::StartTask( pTask );
		}
	}
}


//-----------------------------------------------------------------------------
// todo: find a way to guarantee that objects are made pickupable again when bailing out of a task
//-----------------------------------------------------------------------------
void CNPC_Advisor::RunTask( const Task_t *pTask )
{
#ifndef EZ2
	//Needed for the npc face constantly at player
	GetMotor()->SetIdealYawToTargetAndUpdate(GetEnemyLKP(), AI_KEEP_YAW_SPEED);
#else
	if ( !m_bAdvisorFlyer )
	{
		UpdateAdvisorFacing();
	}
#endif

	// Used by TASK_ADVISOR_STAGGER
	CTakeDamageInfo info( this, this, vec3_origin, GetAbsOrigin(), 20, DMG_BLAST );

	switch ( pTask->iTask )
	{
		// Raise up the objects that we found and then hold them.
		case TASK_ADVISOR_LEVITATE_OBJECTS:
		{
			float flTimeToThrow = m_flThrowPhysicsTime - gpGlobals->curtime;
			if ( flTimeToThrow < 0 )
			{
				TaskComplete();
				return;
			}
						
			// set the top and bottom on the levitation volume from the entities. If we don't have
			// both, zero it out so that we can use the old-style simpler mechanism.
			if ( m_hLevitateGoal1 && m_hLevitateGoal2 )
			{
				m_levitateCallback.m_vecGoalPos1 = m_hLevitateGoal1->GetAbsOrigin();
				m_levitateCallback.m_vecGoalPos2 = m_hLevitateGoal2->GetAbsOrigin();
				// swap them if necessary (1 must be the bottom)
				if (m_levitateCallback.m_vecGoalPos1.z > m_levitateCallback.m_vecGoalPos2.z)
				{
					//swap(m_levitateCallback.m_vecGoalPos1,m_levitateCallback.m_vecGoalPos2);
				}

				m_levitateCallback.m_flFloat = 0.06f; // this is an absolute accumulation upon gravity
			}
			else
			{
				m_levitateCallback.m_vecGoalPos1.Invalidate(); 
				m_levitateCallback.m_vecGoalPos2.Invalidate(); 

				// the below two stanzas are used for old-style floating, which is linked 
				// to float up before thrown and down after
				if ( flTimeToThrow > 2.0f )
				{
					m_levitateCallback.m_flFloat = 1.06f; 
				}
				else
				{
					m_levitateCallback.m_flFloat = 0.94f; 
				}
			}

			/*
			// Draw boxes around the objects we're levitating.
			for ( int i = 0; i < m_physicsObjects.Count(); i++ )
			{
				CBaseEntity *pEnt = m_physicsObjects.Element( i );
				if ( !pEnt )
					continue;	// The prop has been broken!

				IPhysicsObject *pPhys = pEnt->VPhysicsGetObject();
				if ( pPhys && pPhys->IsMoveable() )
				{
					NDebugOverlay::Box( pEnt->GetAbsOrigin(), pEnt->CollisionProp()->OBBMins(), pEnt->CollisionProp()->OBBMaxs(), 0, 255, 0, 1, 0.1 );
				}
			}*/

			break;
		}	
		
		// Pick a random object that we are levitating. If we have a clear LOS from that object
		// to our enemy's eyes, choose that one to throw. Otherwise, keep looking.
		case TASK_ADVISOR_STAGE_OBJECTS:
		{	
			if (m_iStagingNum > m_hvStagingPositions.Count())
			{
				Warning( "Advisor tries to stage %d objects but only has %d positions named %s! Overriding.\n", m_iStagingNum, m_hvStagingPositions.Count(), STRING( m_iszStagingEntities ) );
				m_iStagingNum = m_hvStagingPositions.Count() ;
			}


// advisor_staging_num

			// in the future i'll distribute the staging chronologically. For now, yank all the objects at once.
			if (m_hvStagedEnts.Count() < m_iStagingNum)
			{	
				// pull another object
				bool bDesperate = m_flStagingEnd - gpGlobals->curtime < 0.50f; // less than one half second left
				CBaseEntity *pThrowable = PickThrowable(!bDesperate);
				if (pThrowable)
				{
					// don't let the player take it from me
					IPhysicsObject *pPhys = pThrowable->VPhysicsGetObject();
					if ( pPhys )
					{
						// no pickup!
						pPhys->SetGameFlags(pPhys->GetGameFlags() | FVPHYSICS_NO_PLAYER_PICKUP); 
					}

					m_hvStagedEnts.AddToTail( pThrowable );
					Write_BeamOn(pThrowable);

					
					DispatchParticleEffect( "advisor_object_charge", PATTACH_ABSORIGIN_FOLLOW, 
							pThrowable, 0, 
							false  );

#ifdef EZ2
					SetEyeState( ADVISOR_EYE_CHARGE );
#endif
				}
			}


			Assert(m_hvStagedEnts.Count() <= m_hvStagingPositions.Count());

			// yank all objects into place
			for (int ii = m_hvStagedEnts.Count() - 1 ; ii >= 0 ; --ii)
			{

				// just ignore lost objects (if the player destroys one, that's fine, leave a hole)
				CBaseEntity *pThrowable = m_hvStagedEnts[ii];
				if (pThrowable)
				{
					PullObjectToStaging(pThrowable, m_hvStagingPositions[ii]->GetAbsOrigin());
				}
			}

			// are we done yet?
			if (gpGlobals->curtime > m_flStagingEnd)
			{
				TaskComplete();
				break;
			}
			
			break;
		}

		// Fling the object that we picked at our enemy's eyes!
		case TASK_ADVISOR_BARRAGE_OBJECTS:
		{
			Assert(m_hvStagedEnts.Count() > 0);

			// do I still have an enemy?
			if ( !GetEnemy() )
			{
				// no? bail all the objects. 
				for (int ii = m_hvStagedEnts.Count() - 1 ; ii >=0 ; --ii)
				{

					IPhysicsObject *pPhys = m_hvStagedEnts[ii]->VPhysicsGetObject();
					if ( pPhys )
					{  
						//Removes the nopickup flag from the prop
						pPhys->SetGameFlags(pPhys->GetGameFlags() & (~FVPHYSICS_NO_PLAYER_PICKUP)); 
					}
				}

				Write_AllBeamsOff();
				m_hvStagedEnts.RemoveAll();

#ifdef EZ2
				SetEyeState( ADVISOR_EYE_NORMAL );
#endif

				TaskFail( "Lost enemy" );
				return;
			}

			// do I still have something to throw at the player?
			CBaseEntity *pThrowable = m_hvStagedEnts[0];
			while (!pThrowable) 
			{	// player has destroyed whatever I planned to hit him with, get something else
                if (m_hvStagedEnts.Count() > 0)
				{
					pThrowable = ThrowObjectPrepare();
				}
				else
				{
					TaskComplete();
					break;
				}
			}

			// If we've gone NULL, then opt out
			if ( pThrowable == NULL )
			{
				TaskComplete();
				break;
			}

			if ( (gpGlobals->curtime > m_flThrowPhysicsTime - advisor_throw_lead_prefetch_time.GetFloat()) && 
				!m_vSavedLeadVel.IsValid() )
			{
				// save off the velocity we will use to lead the player a little early, so that if he jukes 
				// at the last moment he'll have a better shot of dodging the object.
				m_vSavedLeadVel = GetEnemy()->GetAbsVelocity();
			}

			// if it's time to throw something, throw it and go on to the next one. 
			if (gpGlobals->curtime > m_flThrowPhysicsTime)
			{
				IPhysicsObject *pPhys = pThrowable->VPhysicsGetObject();
				Assert(pPhys);

				//Removes the nopickup flag from the prop
				pPhys->SetGameFlags(pPhys->GetGameFlags() & (~FVPHYSICS_NO_PLAYER_PICKUP));
				HurlObjectAtPlayer(pThrowable,Vector(0,0,0)/*m_vSavedLeadVel*/);
				m_flLastThrowTime = gpGlobals->curtime;
				m_flThrowPhysicsTime = gpGlobals->curtime + 0.75f;
				// invalidate saved lead for next time
				m_vSavedLeadVel.Invalidate();

				EmitSound( "NPC_Advisor.Blast" );

#ifdef EZ2
				// Throw animation
				Activity actThrows[]	{ ACT_ADVISOR_OBJECT_R_THROW,	ACT_ADVISOR_OBJECT_L_THROW,	ACT_ADVISOR_OBJECT_BOTH_THROW };
				SetIdealActivity( actThrows[m_iThrowAnimDir] );

				SetEyeState( ADVISOR_EYE_NORMAL );
#endif

				Write_BeamOff(m_hvStagedEnts[0]);
				m_hvStagedEnts.Remove(0);
				if (!ThrowObjectPrepare())
				{
					TaskComplete();
					break;
				}
			}
			else
			{	
				// wait, bide time
				// PullObjectToStaging(pThrowable, m_hvStagingPositions[ii]->GetAbsOrigin());

#ifdef EZ2
				// Telegraph our progress in animation
				Activity actHolds[]	{ ACT_ADVISOR_OBJECT_R_HOLD,	ACT_ADVISOR_OBJECT_L_HOLD,	ACT_ADVISOR_OBJECT_BOTH_HOLD };

				// Determine where our object is
				m_iThrowAnimDir = 2;
				//if (pThrowable->GetMass() < 5000.0f)
				{
					//Vector vecForward;
					//GetVectors( &vecForward, NULL, NULL );

					Vector2D vec2DDelta = (pThrowable->GetAbsOrigin().AsVector2D() - GetAbsOrigin().AsVector2D());
					Vector2DNormalize( vec2DDelta );

					QAngle angAngles;
					VectorAngles( Vector(vec2DDelta.x, vec2DDelta.y, 0), angAngles );

					float flObjectAngle = AngleNormalizePositive( AngleDistance( angAngles.y, GetAbsAngles().y ) );
					//if (flObjectAngle <= 160.0f || flObjectAngle >= 200.0f)
					{
						if (flObjectAngle > 180.0f)
							m_iThrowAnimDir = 0; // right
						else
							m_iThrowAnimDir = 1; // left
					}
					Msg( "flObjectAngle = %f\n", flObjectAngle );

					//float flDot = DotProduct2D( vec2DDelta, vecForward.AsVector2D() );
					//if (abs(flDot) < 0.75f)
					//{
					//	if (flDot < 0)
					//		m_iThrowAnimDir = 0; // right
					//	else
					//		m_iThrowAnimDir = 1; // left
					//}
					//Msg( "Dot: %f\n", flDot );
				}

				SetIdealActivity( actHolds[m_iThrowAnimDir] );

				//if (GetActivity() == actStarts[m_iThrowAnimDir])
				//{
				//	if (IsActivityFinished())
				//		SetActivity( actHolds[m_iThrowAnimDir] );
				//}
				//else if (GetActivity() != actHolds[m_iThrowAnimDir])
				//{
				//	SetActivity( actStarts[m_iThrowAnimDir] );
				//}
#endif
			}
		
			break;
		}

		case TASK_ADVISOR_PIN_PLAYER:
		{
			
#ifdef EZ2
			CBaseEntity * pEnemy = AI_GetSinglePlayer();
#else
			CBaseEntity * pEnemy = GetEnemy();
#endif

			// bail out if the pin entity went away.
			CBaseEntity *pPinEnt = m_hPlayerPinPos;
			if (!pPinEnt)
			{
				pEnemy->SetGravity(1.0f);
				pEnemy->SetMoveType( MOVETYPE_WALK );
				Write_BeamOff(GetEnemy());
				beamonce = 1;
#ifdef EZ2
				if (GetTask() == pTask)
#endif
				TaskComplete();
				break;
			}

			// failsafe: don't do this for more than ten seconds.
			if ( gpGlobals->curtime > m_playerPinFailsafeTime && m_bPinFailsafeActive )
			{
				pEnemy->SetGravity(1.0f);
				pEnemy->SetMoveType( MOVETYPE_WALK );
				Write_BeamOff(GetEnemy());
				beamonce = 1;
				Warning( "Advisor did not leave PIN PLAYER mode. Aborting due to ten second failsafe!\n" );
#ifdef EZ2
				if (GetTask() == pTask)
#endif
				TaskFail("Advisor did not leave PIN PLAYER mode. Aborting due to ten second failsafe!\n");
				break;
			}

#ifndef EZ2
			// if the player isn't the enemy, bail out.
			if ( !GetEnemy()->IsPlayer() )
			{
				GetEnemy()->SetGravity(1.0f);
				GetEnemy()->SetMoveType( MOVETYPE_WALK );
				Write_BeamOff(GetEnemy());
				beamonce = 1;
				TaskFail( "Player is not the enemy?!" );
				break;
			}
#endif

			//FUgly, yet I can't think right now a quicker solution to only make one beam on this loop case
			if (beamonce == 1)
			{
				Write_BeamOn(GetEnemy());
				beamonce++;
			}
			
			pEnemy->SetMoveType( MOVETYPE_FLY );
			pEnemy->SetGravity( 0 );

			// use exponential falloff to peg the player to the pin point
			const Vector &desiredPos = pPinEnt->GetAbsOrigin();
			const Vector &playerPos = pEnemy->GetAbsOrigin();

			Vector displacement = desiredPos - playerPos;

			float desiredDisplacementLen = ExponentialDecay(0.250f,gpGlobals->frametime);// * sqrt(displacementLen);			

			pEnemy->SetAbsVelocity( displacement * (1.0f - desiredDisplacementLen) * sk_advisor_pin_speed.GetFloat() );

			break;
			
		}
#ifdef EZ2
		case TASK_ADVISOR_STAGGER:
			SetCondition( COND_ADVISOR_STAGGER );
			// Make it look like we're taking damage even though we're taking very little
			m_iHealth += 19;
			OnTakeDamage( info );
			TaskComplete();
			break;

		case TASK_ADVISOR_WAIT_FOR_ACTIVITY:
			if (GetIdealActivity() != ACT_IDLE && IsActivityFinished())
			{
				SetIdealActivity( ACT_IDLE );
				TaskComplete();
				return;
			}
			break;
			
		// HACKHACK: If we're running a script, make sure we still pin the player
		case TASK_PRE_SCRIPT:
		case TASK_ENABLE_SCRIPT:
		case TASK_WAIT_FOR_SCRIPT:
		case TASK_FACE_SCRIPT:
		case TASK_PLAY_SCRIPT:
		case TASK_PLAY_SCRIPT_POST_IDLE:
		{
			if (m_hPlayerPinPos)
			{
				m_playerPinFailsafeTime = gpGlobals->curtime + 10.0f;
				ChainRunTask( TASK_ADVISOR_PIN_PLAYER );
			}

			BaseClass::RunTask( pTask );
			break;
		}
#endif
		default:
		{
			BaseClass::RunTask( pTask );
		}
	}
}

//----------------------------------------------------------------------------------------------
// Failsafe for restoring mobility to the player if the Advisor gets killed while PinPlayer task
//----------------------------------------------------------------------------------------------
void CNPC_Advisor::Event_Killed(const CTakeDamageInfo &info)
{
	m_OnDeath.FireOutput(info.GetAttacker(), this);
#ifndef EZ2
	if (info.GetAttacker())
	{
		info.GetAttacker()->SetGravity(1.0f);
		info.GetAttacker()->SetMoveType(MOVETYPE_WALK);
		Write_BeamOff(info.GetAttacker());
		beamonce = 1;
	}
#else
	CBasePlayer *pLocalPlayer = AI_GetSinglePlayer();
	if ( pLocalPlayer )
	{
		pLocalPlayer->SetGravity( 1.0f );
		pLocalPlayer->SetMoveType( MOVETYPE_WALK );
		Write_BeamOff( pLocalPlayer );
		beamonce = 1;
	}

	StopFlying();
#endif

	BaseClass::Event_Killed(info);
}

#ifdef EZ2
bool CNPC_Advisor::IsShieldOn( void )
{
	return m_bThrowBeamShield || ( m_bPsychicShieldOn && !HasCondition( COND_ADVISOR_STAGGER ) );
}

void CNPC_Advisor::ApplyShieldedEffects(  )
{
	// Modulus with the skin allows multiple sets of shielded / unshielded skins
	if ( IsAlive() && IsShieldOn() && m_nSkin % 2 == ADVISOR_SKIN_NOSHIELD )
	{
		EmitSound( "NPC_Advisor.shieldup" );
		SetBloodColor( DONT_BLEED );
		SetEyeState( ADVISOR_EYE_NORMAL ); // Refresh eye state
		m_nSkin++;
	}
	else if ( !IsShieldOn() && m_nSkin % 2 == ADVISOR_SKIN_SHIELD)
	{
		EmitSound( "NPC_Advisor.shielddown" );
		SetBloodColor( BLOOD_COLOR_GREEN );
		SetEyeState( ADVISOR_EYE_NORMAL ); // Refresh eye state
		m_nSkin--;
	}
}

int CNPC_Advisor::TranslateSchedule( int scheduleType )
{
	switch (scheduleType)
	{

	case SCHED_IDLE_STAND:
	{
		return SCHED_ADVISOR_IDLE_STAND;
		break;
	}

	case SCHED_MELEE_ATTACK1:
	{
		return SCHED_ADVISOR_MELEE_ATTACK1;
		break;
	}

	case SCHED_CHASE_ENEMY:
	{
		return SCHED_STANDOFF;
		break;
	}

	}

	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_Advisor::NPC_TranslateActivity( Activity activity )
{
	if (activity == ACT_MELEE_ATTACK1)
	{
		// Determine activity based on arm type
		switch (GetCurrentAccessories())
		{
			case ADVISOR_ACCESSORY_ARMS_STUBBED: // Try to attack fruitlessly
			case ADVISOR_ACCESSORY_ARMS:
				{
					// Alternate between left, right, and both
					switch (RandomInt( 0, 2 ))
					{
						case 0: break;
						case 1: activity = ACT_MELEE_ATTACK2; break;
						case 2: activity = ACT_MELEE_ATTACK_SWING; break;
					}
				}
				break;
			case ADVISOR_ACCESSORY_R_ARM:
				// Maintain ACT_MELEE_ATTACK1
				break;
			case ADVISOR_ACCESSORY_L_ARM:
				activity = ACT_MELEE_ATTACK2;
				break;
		}
	}

	return BaseClass::NPC_TranslateActivity( activity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Advisor::GetCurrentAccessories()
{
	// For now, try to identify from the bodygroup
	int iArms = FindBodygroupByName( "arms" );
	if (iArms == -1)
		return ADVISOR_ACCESSORY_NONE;

	int iBody = GetBodygroup( iArms );

	// The advisor model starts with arms as the first and no arms as the second.
	// Beyond that, just use the value directly.
	if (iBody == 0)
		return ADVISOR_ACCESSORY_ARMS;
	else if (iBody == 1)
		return ADVISOR_ACCESSORY_NONE;

	return iBody;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Advisor::SetCurrentAccessories( AdvisorAccessory_t iNewAccessory )
{
	// For now, try to identify from the bodygroup
	int iArms = FindBodygroupByName( "arms" );
	if (iArms == -1)
		return;

	// The advisor model starts with arms as the first and no arms as the second.
	// Beyond that, just use the value directly.
	if (iNewAccessory == ADVISOR_ACCESSORY_NONE)
		iNewAccessory = ADVISOR_ACCESSORY_ARMS;
	else if (iNewAccessory == ADVISOR_ACCESSORY_ARMS)
		iNewAccessory = ADVISOR_ACCESSORY_NONE;

	SetBodygroup( iArms, iNewAccessory );
}

extern CBaseAnimating *CreateServerRagdollSubmodel( CBaseAnimating *pOwner, const char *pModelName, const Vector &position, const QAngle &angles, int collisionGroup );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseAnimating *CNPC_Advisor::CreateDetachedArm( bool bLeft )
{
	const char *pszModelName = bLeft ? "models/advisor_combat_arm_l_detached.mdl" : "models/advisor_combat_arm_r_detached.mdl";

	CBaseAnimating *pAnimating = CreateServerRagdollSubmodel( this, pszModelName, GetAbsOrigin(), GetAbsAngles(), COLLISION_GROUP_DEBRIS );
	if (!pAnimating)
		return NULL;

	pAnimating->m_nSkin = m_nSkin;
	if (IsShieldOn())
		pAnimating->m_nSkin--; // Don't use shield skin

	// For now, fade out after about 3 minutes
	pAnimating->SUB_StartFadeOut( 180.0f, false );

	Vector vecVelocity;
	AngularImpulse vecAngVelocity;
	GetVelocity( &vecVelocity, &vecAngVelocity );

	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = pAnimating->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
	if ( count )
	{
		for ( int i = 0; i < count; i++ )
		{
			pList[i]->SetVelocity( &vecVelocity, &vecAngVelocity );
		}
	}
	else
	{
		// failed to create a physics object
		UTIL_Remove( pAnimating );
		return NULL;
	}

	return pAnimating;
}

//-----------------------------------------------------------------------------
// Create an advisor flyer and start the think function
//-----------------------------------------------------------------------------
void CNPC_Advisor::StartFlying()
{
	if ( m_hAdvisorFlyer == NULL )
	{
		m_hAdvisorFlyer = CreateNoSpawn( "npc_advisor_flyer", GetAbsOrigin(), GetAbsAngles(), this );
		m_hAdvisorFlyer->KeyValue( "SpotlightDisabled", "1" );
		m_hAdvisorFlyer->Spawn();
		m_hAdvisorFlyer->AcceptInput( "DisablePhotos", this, this, variant_t(), 0 );

		if ( GetEnemy() )
		{
			m_hAdvisorFlyer->MyNPCPointer()->UpdateEnemyMemory( GetEnemy(), GetEnemyLKP() );
		}
	}

	m_hFollowTarget = m_hAdvisorFlyer;

	SetContextThink( &CNPC_Advisor::FlyThink, gpGlobals->curtime + TICK_INTERVAL, ADVISOR_FLY_THINK );
}

//-----------------------------------------------------------------------------
// Destroy the advisor flyer and stop think function
//-----------------------------------------------------------------------------
void CNPC_Advisor::StopFlying()
{
	if ( m_hAdvisorFlyer )
	{
		m_hAdvisorFlyer->SUB_Remove();
		m_hAdvisorFlyer = NULL;

		m_hFollowTarget = NULL;

		SetContextThink( NULL, 0.0f, ADVISOR_FLY_THINK );
	}
}

void CNPC_Advisor::FlyThink()
{
	UpdateAdvisorFacing();

	if ( m_hFollowTarget )
	{
		// Something went bad! The flyer is too far, let's teleport
		if ( m_hFollowTarget->GetAbsOrigin().DistTo( GetAbsOrigin() ) > 512.0f )
		{
			// Fire a mind blast effect to blind the player while we do something janky
			m_OnMindBlast.FireOutput( this, this, 0.0f );

			DispatchParticleEffect( "tele_warp", PATTACH_ABSORIGIN_FOLLOW, this, 0, false );
			Teleport( &m_hFollowTarget->GetAbsOrigin(), &GetAbsAngles(), &GetAbsVelocity() );
		}
		// Move towards the advisor flyer
		else
		{
			// Gotta go fast!
			float flAdvisorFlightSpeed = m_hFollowTarget == m_hAdvisorFlyer ? 128.0f : 1.0f;
			SetAbsVelocity( ( m_hFollowTarget->GetAbsOrigin() - GetAbsOrigin() ) * flAdvisorFlightSpeed );
		}
	}

	SetContextThink( &CNPC_Advisor::FlyThink, gpGlobals->curtime + TICK_INTERVAL, ADVISOR_FLY_THINK );
}

//-----------------------------------------------------------------------------
// Face the target
// In the original Episode 2 code, advisors would always face towards the player
//-----------------------------------------------------------------------------
void CNPC_Advisor::UpdateAdvisorFacing()
{
	/*if (m_hAdvisorFlyer)
	{
		Vector vecDir;
		AngleVectors( m_hAdvisorFlyer->GetAbsAngles(), &vecDir );
		float flYaw = UTIL_VecToYaw( vecDir );
		float flPitch = -UTIL_VecToPitch( vecDir ); // Invert flyer pitch
		
		GetAdvisorMotor()->SetIdealYaw( flYaw );
		GetAdvisorMotor()->SetIdealPitch( flPitch );
	}
	else*/ if (advisor_use_facing_override.GetBool())
	{
		switch ( GetState() )
		{
		case NPC_STATE_COMBAT:
			GetAdvisorMotor()->SetIdealYawAndPitchToTarget( GetEnemyLKP()/*, AI_KEEP_YAW_SPEED*/ );
			break;
		case NPC_STATE_SCRIPT:
			GetAdvisorMotor()->SetIdealPitch( 0.0f );
			break;
		default:
			if ( GetLooktarget() )
			{
				GetAdvisorMotor()->SetIdealYawAndPitchToTarget( GetLooktarget()->GetAbsOrigin()/*, AI_KEEP_YAW_SPEED*/ );
			}
			else if ( HasCondition( COND_SEE_PLAYER ) )
			{
				CBasePlayer * pPlayer = AI_GetSinglePlayer();
				GetAdvisorMotor()->SetIdealYawAndPitchToTarget( pPlayer->GetAbsOrigin()/*, AI_KEEP_YAW_SPEED*/ );
			}
			break;
		}
	}

	if (advisor_update_yaw.GetBool())
	{
		//GetMotor()->SetIdealYaw( CalcReasonableFacing( true ) );
		GetMotor()->UpdateYaw( -1 );
	}

	if (GetState() != NPC_STATE_SCRIPT)
	{
		if (advisor_use_flyer_poses.GetBool())
		{
			Vector vecForward;
			GetVectors( &vecForward, NULL, NULL );
			float flCurrentYaw = UTIL_VecToYaw( vecForward );
			float flCurrentPitch = UTIL_VecToPitch( vecForward );

			float flYawDiff = RemapValClamped( -AngleDistance(flCurrentYaw, GetMotor()->GetIdealYaw()), -30.0, 30.0f, -75.0f, 75.0f );
			float flPitchDiff = RemapValClamped( -AngleDistance(flCurrentPitch, GetAdvisorMotor()->GetIdealPitch()), -30.0, 30.0f, -75.0f, 75.0f );

			float yaw = GetPoseParameter( m_ParameterFlexHorz );
			float pitch = GetPoseParameter( m_ParameterFlexVert );

			SetPoseParameter( m_ParameterFlexHorz, UTIL_Approach( flYawDiff, yaw, MaxYawSpeed() ) );
			SetPoseParameter( m_ParameterFlexVert, UTIL_Approach( flPitchDiff, pitch, MaxPitchSpeed() ) );

			// Set arms extent as combination of velocity and distance to enemy
			// (higher up = more constricted)
			float flArmsExtent;
			if (GetEnemy())
			{
				Vector vecDelta = (GetAbsOrigin() - GetEnemy()->GetAbsOrigin());
				float flVelocityLengthSqr = GetLocalVelocity().LengthSqr() + GetLocalAngularVelocity().LengthSqr();
				flArmsExtent = RemapVal( (flVelocityLengthSqr + vecDelta.LengthSqr()), 0, Square( 1000 ), 0.0f, 1.0f );
			}
			else
			{
				flArmsExtent = (GetAbsVelocity().Length()) * 0.005f;
			}
			//Msg( "flYawDiff: %f, flArmsExtent: %f (yaw: %f, ideal yaw: %f)\n", flYawDiff, flArmsExtent, flCurrentYaw, GetMotor()->GetIdealYaw() );
			SetPoseParameter( m_ParameterArmsExtent, flArmsExtent );
		}
	}
	else
	{
		SetPoseParameter( m_ParameterFlexHorz, 0.0f );
		SetPoseParameter( m_ParameterFlexVert, 0.0f );
		SetPoseParameter( m_ParameterArmsExtent, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Advisor::StartSounds( void )
{
	//Initialize the additive sound channels
	CPASAttenuationFilter filter( this );

	if (m_pBreathSound == NULL)
	{
		m_pBreathSound	= CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), CHAN_ITEM, "NPC_Advisor.BreatheLoop", ATTN_NORM );

		if (m_pBreathSound)
		{
			CSoundEnvelopeController::GetController().Play( m_pBreathSound, 0.5f, 100 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Advisor::StopLoopingSounds()
{
	//Stop all sounds
	CSoundEnvelopeController::GetController().SoundDestroy( m_pBreathSound );

	m_pBreathSound		= NULL;

	BaseClass::StopLoopingSounds();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Advisor::HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt )
{
	if ( interactionType == g_interactionXenGrenadeConsume )
	{
		DevMsg( "Advisor %s has been displaced by the Xen grenade or displacer pistol!\n", GetDebugName() );
		// "Don't be sucked up" case, which is default

		// Invoke effects similar to that of being lifted by a barnacle.
		AddEFlags( EFL_IS_BEING_LIFTED_BY_BARNACLE );
		AddSolidFlags( FSOLID_NOT_SOLID );
		AddEffects( EF_NODRAW );
		KillSprites( 0.0f );
		SetSleepState( AISS_IGNORE_INPUT );
		Sleep();

		Write_AllBeamsOff();

		IPhysicsObject *pPhys = VPhysicsGetObject();
		if (pPhys)
		{
			pPhys->EnableMotion( false );
		}

		if ( m_bAdvisorFlyer )
		{
			StopFlying();
		}

		SetAbsVelocity( vec3_origin );

		// Make all children nodraw
		CBaseEntity *pChild = FirstMoveChild();
		while (pChild)
		{
			pChild->AddEffects( EF_NODRAW );
			pChild = pChild->NextMovePeer();
		}

		SetNextThink( TICK_NEVER_THINK );

		// Return true to indicate we shouldn't be sucked up 
		return true;
	}

	if ( interactionType == g_interactionXenGrenadeRelease )
	{
		DevMsg( "Advisor %s has been released by the Xen grenade or displacer pistol!\n", GetDebugName() );
		DisplacementInfo_t *info = static_cast<DisplacementInfo_t*>( data );
		if ( !info )
			return false;

		Teleport( info->vecTargetPos, &info->pDisplacer->GetAbsAngles(), NULL );


		Wake();
		StartEye();
		RemoveEFlags( EFL_IS_BEING_LIFTED_BY_BARNACLE );
		RemoveSolidFlags( FSOLID_NOT_SOLID );
		RemoveEffects( EF_NODRAW );

		IPhysicsObject *pPhys = VPhysicsGetObject();
		if (pPhys)
		{
			pPhys->EnableMotion( true );
			pPhys->Wake();
		}

		// Make all children draw
		CBaseEntity *pChild = FirstMoveChild();
		while (pChild)
		{
			pChild->RemoveEffects( EF_NODRAW );
			pChild = pChild->NextMovePeer();
		}

		if ( m_bAdvisorFlyer )
		{
			StartFlying();
		}

		CBaseEntity * pAttacker = info->pOwner == NULL ? this : info->pOwner;
		CTakeDamageInfo damageInfo( pAttacker, pAttacker, vec3_origin, GetAbsOrigin(), 100, DMG_BLAST );
		OnTakeDamage( damageInfo );

		// Pretend we spawned from a recipe
		info->pSink->SpawnEffects( this );

		m_fStaggerUntilTime = gpGlobals->curtime + 3.0f;

		SetNextThink( gpGlobals->curtime );

		return true;
	}

	if ( interactionType == g_interactionXenGrenadePull )
	{
		DevMsg( "Advisor being pulled!\n" );
		m_fStaggerUntilTime = gpGlobals->curtime + 1.0f;

		// Let the advisor be pulled normally
		return false;
	}

	if ( interactionType == g_interactionXenGrenadeRagdoll )
	{
		// Don't.
		return true;
	}

	// TODO - Add an interaction for telefragging

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//=========================================================
// TraceAttack is overridden here to handle shield impacts.
//=========================================================
void CNPC_Advisor::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	m_fNoDamageDecal = false;

	QAngle vecAngles;
	VectorAngles( ptr->plane.normal, vecAngles );

	// Shield particle
	if ( IsShieldOn() )
	{
		DispatchParticleEffect( "warp_shield_impact_BMan", ptr->endpos, vecAngles, this );

		// Make a blocked sound
		EmitSound( "NPC_Advisor.shieldblock" );

		// Do not handle trace
		m_fNoDamageDecal = true;

		// Fire a fake bullet in the normal's direction
		//FireBulletsInfo_t bulletsInfo;
		//bulletsInfo.m_iShots = 1;
		//bulletsInfo.m_vecSrc = ptr->endpos;
		//bulletsInfo.m_vecDirShooting = ptr->plane.normal;
		//bulletsInfo.m_vecSpread = VECTOR_CONE_5DEGREES;
		//bulletsInfo.m_flDistance = MAX_TRACE_LENGTH;
		//bulletsInfo.m_iAmmoType = 0;
		//bulletsInfo.m_flDamage = 0;
		//bulletsInfo.m_iTracerFreq = 1;
		//FireBullets( bulletsInfo );

		// Make one spark fly off to represent the bullet that hit the shield
		g_pEffects->MetalSparks( ptr->endpos, ptr->plane.normal );

		return;
	}
	else
	{
		// Dispatch a bleeding particle effect
		DispatchParticleEffect( RandomInt(0,1) ? "blood_advisor_shrapnel_spurt_1" : "blood_advisor_shrapnel_spurt_2", ptr->endpos, vecAngles, this );
		//DispatchParticleEffect( RandomInt(0,1) ? "blood_advisor_shrapnel_spurt_1" : "blood_advisor_shrapnel_spurt_2", ptr->endpos, vecAngles, this, PATTACH_ABSORIGIN_FOLLOW );
	}

	BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );
}

//-----------------------------------------------------------------------------
// Purpose: Custom trace filter used for advisor staging LOS traces
//-----------------------------------------------------------------------------
class CTraceFilterAdvisorStage : public CTraceFilterSkipTwoEntities
{
public:
	CTraceFilterAdvisorStage( IHandleEntity *pHandleEntity, int collisionGroup, IHandleEntity *pHandleEntity2 = NULL )
		: CTraceFilterSkipTwoEntities( pHandleEntity, pHandleEntity2, collisionGroup )
	{
		m_pAdvisor = static_cast<CNPC_Advisor *>(pHandleEntity);
	}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );

		if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
		{
			// Don't hit staged props
			if ( m_pAdvisor->IsThisPropStaged( pEntity ) )
				return false;
		}

		return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
	}

private:
	CNPC_Advisor *m_pAdvisor;
};
#endif


#endif

// helper function for testing whether or not an avisor is allowed to grab an object
static bool AdvisorCanPickObject( CBasePlayer *pPlayer, CBaseEntity *pEnt, CNPC_Advisor *pAdvisor = NULL )
{
#ifdef EZ2
	if (pPlayer != NULL)
	{
#endif
	Assert( pPlayer != NULL );

	// Is the player carrying something?
	CBaseEntity *pHeldObject = GetPlayerHeldEntity(pPlayer);

	if( !pHeldObject )
	{
		pHeldObject = PhysCannonGetHeldEntity( pPlayer->GetActiveWeapon() );
	}

	if( pHeldObject == pEnt )
	{
		return false;
	}
#ifdef EZ2
	}

	// Is this object within mass thresholds?
	if ( pEnt->GetMass() < advisor_throw_stage_min_mass.GetInt() || pEnt->GetMass() > advisor_throw_stage_max_mass.GetInt() )
	{
		return false;
	}
#endif

	if ( pEnt->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
	{
		return false;
	}

#ifdef EZ2
	if (pAdvisor)
	{
		// Can the prop reach our first staging position?
		// TODO: Check all staging positions
		CBaseEntity *pStagingPos = pAdvisor->GetFirstStagingPosition();
		if (pStagingPos)
		{
			CTraceFilterAdvisorStage traceFilter( pAdvisor, COLLISION_GROUP_NONE, pEnt );
			trace_t tr;

			Vector stagingPos = pStagingPos->GetAbsOrigin();
			AI_TraceLine( pEnt->GetAbsOrigin(), stagingPos, MASK_SOLID, &traceFilter, &tr );

			if (tr.fraction != 1.0f)
				return false;
		}
	}
#endif

	return true;
}


#if NPC_ADVISOR_HAS_BEHAVIOR
//-----------------------------------------------------------------------------
// Choose an object to throw.
// param bRequireInView : if true, only accept objects that are in the player's fov.
//
// Can always return NULL.
// todo priority_grab_name
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_Advisor::PickThrowable( bool bRequireInView )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetEnemy() );
#ifndef EZ2
	Assert(pPlayer);
	if (!pPlayer)
		return NULL;
#endif

	const int numObjs = m_physicsObjects.Count(); ///< total number of physics objects in my system
	if (numObjs < 1) 
		return NULL; // bail out if nothing available

	
	// used for require-in-view
	Vector eyeForward, eyeOrigin;
	if (pPlayer)
	{
		eyeOrigin = pPlayer->EyePosition();
		pPlayer->EyeVectors(&eyeForward);
	}
	else
	{
		bRequireInView = false;
	}

	// filter-and-choose algorithm:
	// build a list of candidates
	Assert(numObjs < 128); /// I'll come back and utlvector this shortly -- wanted easier debugging
	unsigned int candidates[128];
	unsigned int numCandidates = 0;

	if (!!m_iszPriorityEntityGroupName) // if the string isn't null
	{
		// first look to see if we have any priority objects.
		for (int ii = 0 ; ii < numObjs ; ++ii )
		{
			CBaseEntity *pThrowEnt = m_physicsObjects[ii];
			// Assert(pThrowEnt); 
			if (!pThrowEnt)
				continue;

			if (!pThrowEnt->NameMatches(m_iszPriorityEntityGroupName)) // if this is not a priority object
				continue;

#ifdef EZ2
			if (pThrowEnt == GetEnemy()) // Don't pick our enemy
				continue;
#endif

			bool bCanPick = AdvisorCanPickObject( pPlayer, pThrowEnt ) && !m_hvStagedEnts.HasElement( m_physicsObjects[ii] );
			if (!bCanPick)
				continue;

			// bCanPick guaranteed true here

			if ( bRequireInView )
			{
				bCanPick = (pThrowEnt->GetAbsOrigin() - eyeOrigin).Dot(eyeForward) > 0;
			}

			if ( bCanPick )
			{
				candidates[numCandidates++] = ii; 
			}
		}
	}

	// if we found no priority objects (or don't have a priority), just grab whatever
	if (numCandidates == 0)
	{
		for (int ii = 0 ; ii < numObjs ; ++ii )
		{
			CBaseEntity *pThrowEnt = m_physicsObjects[ii];
			// Assert(pThrowEnt); 
			if (!pThrowEnt)
				continue;

			bool bCanPick = AdvisorCanPickObject( pPlayer, pThrowEnt ) && !m_hvStagedEnts.HasElement( m_physicsObjects[ii] );
			if (!bCanPick)
				continue;

			// bCanPick guaranteed true here

			if ( bRequireInView )
			{
				bCanPick = (pThrowEnt->GetAbsOrigin() - eyeOrigin).Dot(eyeForward) > 0;
			}

			if ( bCanPick )
			{
				candidates[numCandidates++] = ii; 
			}
		}
	}

	if ( numCandidates == 0 )
		return NULL; // must have at least one candidate

	// pick a random candidate.
	int nRandomIndex = random->RandomInt( 0, numCandidates - 1 );
	return m_physicsObjects[candidates[nRandomIndex]];

}

/*! \TODO
	Correct bug where Advisor seemed to be throwing stuff at people's feet. 
	This is because the object was falling slightly in between the staging 
	and when he threw it, and that downward velocity was getting accumulated 
	into the throw speed. This is temporarily fixed here by using SetVelocity 
	instead of AddVelocity, but the proper fix is to pin the object to its 
	staging point during the warn period. That will require maintaining a map
	of throwables to their staging points during the throw task.
*/
//-----------------------------------------------------------------------------
// Impart necessary force on any entity to make it clobber Gordon.
// Also detaches from levitate controller.
// The optional lead velocity parameter is for cases when we pre-save off the 
// player's speed, to make last-moment juking more effective
//-----------------------------------------------------------------------------
void CNPC_Advisor::HurlObjectAtPlayer( CBaseEntity *pEnt, const Vector &leadVel )
{
	IPhysicsObject *pPhys = pEnt->VPhysicsGetObject();

	//
	// Lead the target accurately. This encourages hiding behind cover
	// and/or catching the thrown physics object!
	//
	Vector vecObjOrigin = pEnt->CollisionProp()->WorldSpaceCenter();
	Vector vecEnemyPos = GetEnemy()->EyePosition();
	// disabled -- no longer compensate for gravity: // vecEnemyPos.y += 12.0f;

//	const Vector &leadVel = pLeadVelocity ? *pLeadVelocity : GetEnemy()->GetAbsVelocity();

	Vector vecDelta = vecEnemyPos - vecObjOrigin;
	float flDist = vecDelta.Length();

	float flVelocity = advisor_throw_velocity.GetFloat();

	if ( flVelocity == 0 )
	{
		flVelocity = 1000;
	}
		
	float flFlightTime = flDist / flVelocity;

	Vector vecThrowAt = vecEnemyPos + flFlightTime * leadVel;
	Vector vecThrowDir = vecThrowAt - vecObjOrigin;
	VectorNormalize( vecThrowDir );
	
	Vector vecVelocity = flVelocity * vecThrowDir;
	pPhys->SetVelocity( &vecVelocity, NULL );

	AddToThrownObjects(pEnt);

	m_OnThrow.FireOutput(pEnt,this);

}


//-----------------------------------------------------------------------------
// do a sweep from an object I'm about to throw, to the target, pushing aside 
// anything floating in the way.
// TODO: this is probably a good profiling candidate.
//-----------------------------------------------------------------------------
void CNPC_Advisor::PreHurlClearTheWay( CBaseEntity *pThrowable, const Vector &toPos )
{
	// look for objects in the way of chucking.
	CBaseEntity *list[128];
	Ray_t ray;

	
	float boundingRadius = pThrowable->BoundingRadius();
	
	ray.Init( pThrowable->GetAbsOrigin(), toPos,
			  Vector(-boundingRadius,-boundingRadius,-boundingRadius),
		      Vector( boundingRadius, boundingRadius, boundingRadius) );

	int nFoundCt = UTIL_EntitiesAlongRay( list, 128, ray, 0 );
	AssertMsg(nFoundCt < 128, "Found more than 128 obstructions between advisor and Gordon while throwing. (safe to continue)\n");

	// for each thing in the way that I levitate, but is not something I'm staging
	// or throwing, push it aside.
	for (int i = 0 ; i < nFoundCt ; ++i )
	{
		CBaseEntity *obstruction = list[i];
		if (  obstruction != pThrowable                  &&
			  m_physicsObjects.HasElement( obstruction ) && // if it's floating
			 !m_hvStagedEnts.HasElement( obstruction )   && // and I'm not staging it
			 !DidThrow( obstruction ) )						// and I didn't just throw it
		{
            IPhysicsObject *pPhys = obstruction->VPhysicsGetObject();
			Assert(pPhys); 

			// If pPhys is NULL, skip
			if (pPhys == NULL)
			{
				DevWarning("WARNING: Advisor attempted to push NULL physics object out of the way of an object.\n");
				continue;
			}

			// this is an object we want to push out of the way. Compute a vector perpendicular
			// to the path of the throwables's travel, and thrust the object along that vector.
			Vector thrust;
			CalcClosestPointOnLine( obstruction->GetAbsOrigin(),
									pThrowable->GetAbsOrigin(), 
									toPos,
									thrust );
			// "thrust" is now the closest point on the line to the obstruction. 
			// compute the difference to get the direction of impulse
			thrust = obstruction->GetAbsOrigin() - thrust;

			// and renormalize it to equal a giant kick out of the way
			// (which I'll say is about ten feet per second -- if we want to be
			//  more precise we could do some kind of interpolation based on how
			//  far away the object is)
			float thrustLen = thrust.Length();
			if (thrustLen > 0.0001f)
			{
				thrust *= advisor_throw_clearout_vel.GetFloat() / thrustLen; 
			}

			// heave!
			pPhys->AddVelocity( &thrust, NULL );
		}
	}

/*

	// Otherwise only help out a little
	Vector extents = Vector(256, 256, 256);
	Ray_t ray;
	ray.Init( vecStartPoint, vecStartPoint + 2048 * vecVelDir, -extents, extents );
	int nCount = UTIL_EntitiesAlongRay( list, 1024, ray, FL_NPC | FL_CLIENT );
	for ( int i = 0; i < nCount; i++ )
	{
		if ( !IsAttractiveTarget( list[i] ) )
			continue;

		VectorSubtract( list[i]->WorldSpaceCenter(), vecStartPoint, vecDelta );
		distance = VectorNormalize( vecDelta );
		flDot = DotProduct( vecDelta, vecVelDir );
		
		if ( flDot > flMaxDot )
		{
			if ( distance < flBestDist )
			{
				pBestTarget = list[i];
				flBestDist = distance;
			}
		}
	}

*/

}

/* 
// commented out because unnecessary: we will do this during the DidThrow check

//-----------------------------------------------------------------------------
// clean out the recently thrown objects array
//-----------------------------------------------------------------------------
void CNPC_Advisor::PurgeThrownObjects() 
{
	float threeSecondsAgo = gpGlobals->curtime - 3.0f; // two seconds ago

	for (int ii = 0 ; ii < kMaxThrownObjectsTracked ; ++ii)
	{
		if ( m_haRecentlyThrownObjects[ii].IsValid() && 
			 m_flaRecentlyThrownObjectTimes[ii] < threeSecondsAgo )
		{
			 m_haRecentlyThrownObjects[ii].Set(NULL);
		}
	}
	
}
*/


//-----------------------------------------------------------------------------
// true iff an advisor threw the object in the last three seconds
//-----------------------------------------------------------------------------
bool CNPC_Advisor::DidThrow(const CBaseEntity *pEnt)
{
	// look through all my objects and see if they match this entity. Incidentally if 
	// they're more than three seconds old, purge them.
	float threeSecondsAgo = gpGlobals->curtime - 3.0f; 

	for (int ii = 0 ; ii < kMaxThrownObjectsTracked ; ++ii)
	{
		// if object is old, skip it.
		CBaseEntity *pTestEnt = m_haRecentlyThrownObjects[ii];

		if ( pTestEnt ) 
		{
			if ( m_flaRecentlyThrownObjectTimes[ii] < threeSecondsAgo )
			{
				m_haRecentlyThrownObjects[ii].Set(NULL);
				continue;
			}
			else if (pTestEnt == pEnt)
			{
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Advisor::AddToThrownObjects(CBaseEntity *pEnt)
{
	Assert(pEnt);

	// try to find an empty slot, or if none exists, the oldest object
	int oldestThrownObject = 0;
	for (int ii = 0 ; ii < kMaxThrownObjectsTracked ; ++ii)
	{
		if (m_haRecentlyThrownObjects[ii].IsValid())
		{
			if (m_flaRecentlyThrownObjectTimes[ii] < m_flaRecentlyThrownObjectTimes[oldestThrownObject])
			{
				oldestThrownObject = ii;
			}
		}
		else
		{	// just use this one
			oldestThrownObject = ii;
			break;
		}
	}

	m_haRecentlyThrownObjects[oldestThrownObject] = pEnt;
	m_flaRecentlyThrownObjectTimes[oldestThrownObject] = gpGlobals->curtime;

}


//-----------------------------------------------------------------------------
// Drag a particular object towards its staging location.
//-----------------------------------------------------------------------------
void CNPC_Advisor::PullObjectToStaging( CBaseEntity *pEnt, const Vector &stagingPos )
{
	IPhysicsObject *pPhys = pEnt->VPhysicsGetObject();
	Assert(pPhys);

	Vector curPos = pEnt->CollisionProp()->WorldSpaceCenter();
	Vector displacement = stagingPos - curPos;

	// quick and dirty -- use exponential decay to haul the object into place
	// ( a better looking solution would be to use a spring system )

	float desiredDisplacementLen = ExponentialDecay(STAGING_OBJECT_FALLOFF_TIME, gpGlobals->frametime);// * sqrt(displacementLen);
	
	Vector vel; AngularImpulse angimp;
	pPhys->GetVelocity(&vel,&angimp);

	vel = (1.0f / gpGlobals->frametime)*(displacement * (1.0f - desiredDisplacementLen));
	pPhys->SetVelocity(&vel,&angimp);
}



#endif

int	CNPC_Advisor::OnTakeDamage( const CTakeDamageInfo &info )
{
#ifdef EZ2
	// I AM BULLETPROOF
	if ( IsShieldOn() )
	{
		return 0;
	}
#endif

	// Clip our max 
	CTakeDamageInfo newInfo = info;
	if ( newInfo.GetDamage() > 20.0f )
	{
		newInfo.SetDamage( 20.0f );
	}

	// Hack to make him constantly flinch
	m_flNextFlinchTime = gpGlobals->curtime;

	const float oldLastDamageTime = m_flLastDamageTime;
	int retval = BaseClass::OnTakeDamage(newInfo);

	// we have a special reporting output 
	if ( oldLastDamageTime != gpGlobals->curtime )
	{
		// only fire once per frame

		m_OnHealthIsNow.Set( GetHealth(), newInfo.GetAttacker(), this);
	}

#ifdef EZ2
	float flHealthRatio = (float)GetHealth() / (float)GetMaxHealth();
	if ( flHealthRatio < advisor_camera_flash_health_ratio.GetFloat() )
	{
		SetEyeState( ADVISOR_EYE_DAMAGED );
	}
	else
	{
		SetEyeState( ADVISOR_EYE_UNDAMAGED );
	}

	// TODO: This needs an elaborate clientside implementation in order to function
	//if ( flHealthRatio < advisor_bleed_health_ratio.GetFloat() )
	//{
	//	if (!m_bBleeding)
	//	{
	//		//DispatchParticleEffect( "blood_antlionguard_injured_heavy", PATTACH_POINT_FOLLOW, this, gm_nMouthAttachment );
	//		//DispatchParticleEffect( "blood_drip_synth_01", PATTACH_ABSORIGIN_FOLLOW, this );
	//		m_bBleeding = true;
	//	}
	//}
	//else if (m_bBleeding)
	//{
	//	// HACKHACK
	//	StopParticleEffects( this );
	//	m_bBleeding = false;
	//}

	if (advisor_hurt_pose.GetBool())
	{
		SetPoseParameter( m_ParameterHurt, 1.0f - flHealthRatio );
	}
#endif

	return retval;
}




#if NPC_ADVISOR_HAS_BEHAVIOR
//-----------------------------------------------------------------------------
// Purpose: For innate melee attack
// Input :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Advisor::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (flDist > 128)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.7)
	{
		return COND_NOT_FACING_ATTACK;
	}
	return COND_CAN_MELEE_ATTACK1;
}

//-----------------------------------------------------------------------------
//  Returns the best new schedule for this NPC based on current conditions.
//-----------------------------------------------------------------------------
int CNPC_Advisor::SelectSchedule()
{
#ifndef EZ2
    if ( IsInAScript() )
        return SCHED_ADVISOR_IDLE_STAND;
#else
	if ( HasCondition( COND_ADVISOR_STAGGER ) )
	{
		Write_AllBeamsOff();
		ApplyShieldedEffects();
		return SCHED_ADVISOR_STAGGER;
	}
#endif

	// Kick attack?
	if (HasCondition(COND_CAN_MELEE_ATTACK1))
	{
		return SCHED_MELEE_ATTACK1;
	}

#ifdef EZ2
	// Don't pin the player immediately after throwing an object!
	if ( IsAlive() && m_hPlayerPinPos.IsValid() && gpGlobals->curtime - m_flLastThrowTime > sk_advisor_pin_throw_cooldown.GetFloat() )
	{
		ClearCondition( COND_ADVISOR_FORCE_PIN );
		return SCHED_ADVISOR_TOSS_PLAYER;
	}
#endif

	switch ( m_NPCState )
	{
#ifndef EZ2
		case NPC_STATE_IDLE:
		case NPC_STATE_ALERT:
		{

			return SCHED_ADVISOR_IDLE_STAND;
		} 
#endif

		case NPC_STATE_COMBAT:
		{
			if (GetEnemy() && GetEnemy()->IsAlive() && HasCondition(COND_HAVE_ENEMY_LOS))
			{
#ifndef EZ2
				if (   m_hPlayerPinPos.IsValid()  )//false
					return SCHED_ADVISOR_TOSS_PLAYER;
				else
#endif
					return SCHED_ADVISOR_COMBAT;
				
			}
			
#ifndef EZ2
			return SCHED_ADVISOR_IDLE_STAND;
#endif
		}
	}

	return BaseClass::SelectSchedule();
}

#ifdef EZ2
//-----------------------------------------------------------------------------
// TASK_RANGE_ATTACK1
//-----------------------------------------------------------------------------
void CNPC_Advisor::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();

	// Don't interrupt scripts. RunTask() allows pinning to occur simultaneously
	if ( GetState() != NPC_STATE_SCRIPT )
	{
		SetCustomInterruptCondition( COND_ADVISOR_FORCE_PIN );
	}
	
	if ( IsCurSchedule( SCHED_ADVISOR_COMBAT, false ) )
	{
		// Don't interrupt during barrage task
		if (!GetTask() || GetTask()->iTask != TASK_ADVISOR_BARRAGE_OBJECTS)
			SetCustomInterruptCondition( COND_CAN_MELEE_ATTACK1 );
	}
	//else if ( IsCurSchedule( SCHED_MELEE_ATTACK1, false ) )
	//{
	//	// Don't get stuck trying to face
	//	if (GetTask() && GetTask()->iTask == TASK_FACE_ENEMY)
	//		SetCustomInterruptCondition( COND_TOO_FAR_TO_ATTACK );
	//}
	else if ( IsCurSchedule( SCHED_STANDOFF, false ) )
	{
		// Interrupt when we can use our combat schedule again
		SetCustomInterruptCondition( COND_HAVE_ENEMY_LOS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Advisor::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

	if ( IsPreparingToThrow() && m_iThrowAnimDir != 2 )
	{
		Vector vecLookOrigin = m_hvStagedEnts[0]->GetAbsOrigin();

		if (m_hvStagedEnts.Count() > 1)
		{
			// Get a vague center
			for (int i = 1; i < m_hvStagedEnts.Count(); i++)
			{
				if (m_hvStagedEnts[i])
					vecLookOrigin += ((vecLookOrigin - m_hvStagedEnts[i]->GetAbsOrigin()) * 0.5f);
			}
		}

		AddLookTarget( vecLookOrigin, 1.0, 0.5 );
	}
	else if ( IsCurSchedule( SCHED_ADVISOR_COMBAT, false ) )
	{
		AddLookTarget( GetEnemy(), 1.0, 0.5/*, 0.2*/ );
	}
}
#endif

void CNPC_Advisor::HandleAnimEvent(animevent_t *pEvent)
{
	if (pEvent->type & AE_TYPE_NEWEVENTSYSTEM)
	{
		if ( pEvent->event == ADVISOR_AE_DETACH_R_ARM )
		{
			inputdata_t dummy;
			InputDetachRightArm( dummy );
		}
		else if ( pEvent->event == ADVISOR_AE_DETACH_L_ARM )
		{
			inputdata_t dummy;
			InputDetachLeftArm( dummy );
		}
		else
		{
			BaseClass::HandleAnimEvent( pEvent );
		}
	}
	else
	{
		switch (pEvent->event)
		{
		case ADVISOR_MELEE_LEFT:
		{
#ifdef EZ2
			// If we don't have a left arm, early out
			int iAccessory = GetCurrentAccessories();
			if (iAccessory != ADVISOR_ACCESSORY_ARMS && iAccessory != ADVISOR_ACCESSORY_L_ARM)
				break;
#endif

			CBaseEntity *pHurt = CheckTraceHullAttack(70, -Vector(128, 128, 224), Vector(128, 128, 64), sk_advisor_melee_dmg.GetFloat(), DMG_SLASH);
			if (pHurt)
			{
				Vector forward, up;
				AngleVectors(GetLocalAngles(), &forward, NULL, &up);

				if (pHurt->GetFlags() & (FL_NPC | FL_CLIENT))
				{
					pHurt->ViewPunch(QAngle(5, 0, random->RandomInt(-10, 10)));
				}
				// Spawn some extra blood if we hit a BCC
				CBaseCombatCharacter* pBCC = ToBaseCombatCharacter(pHurt);
				if (pBCC)
				{
					SpawnBlood(pBCC->EyePosition(), g_vecAttackDir, pBCC->BloodColor(), sk_advisor_melee_dmg.GetFloat());
				}
				// Play a attack hit sound
#ifdef EZ2
				EmitSound("NPC_Advisor.Melee_Hit");
#else
				EmitSound("NPC_Stalker.Hit");
#endif
			}
#ifdef EZ2
			else
			{
				EmitSound( "NPC_Advisor.Melee_Miss" );
			}
#endif
			break;
		}

		case ADVISOR_MELEE_RIGHT:
		{
#ifdef EZ2
			// If we don't have a right arm, early out
			int iAccessory = GetCurrentAccessories();
			if (iAccessory != ADVISOR_ACCESSORY_ARMS && iAccessory != ADVISOR_ACCESSORY_R_ARM)
				break;
#endif

			CBaseEntity *pHurt = CheckTraceHullAttack(70, -Vector(128, 128, 224), Vector(128, 128, 64), sk_advisor_melee_dmg.GetFloat(), DMG_SLASH);
			if (pHurt)
			{
				Vector forward, up;
				AngleVectors(GetLocalAngles(), &forward, NULL, &up);

				if (pHurt->GetFlags() & (FL_NPC | FL_CLIENT))
				{
					pHurt->ViewPunch(QAngle(5, 0, random->RandomInt(-10, 10)));
				}
				// Spawn some extra blood if we hit a BCC
				CBaseCombatCharacter* pBCC = ToBaseCombatCharacter(pHurt);
				if (pBCC)
				{
					SpawnBlood(pBCC->EyePosition(), g_vecAttackDir, pBCC->BloodColor(), sk_advisor_melee_dmg.GetFloat());
				}
				// Play a attack hit sound
#ifdef EZ2
				EmitSound("NPC_Advisor.Melee_Hit");
#else
				EmitSound("NPC_Stalker.Hit");
#endif
			}
#ifdef EZ2
			else
			{
				EmitSound( "NPC_Advisor.Melee_Miss" );
			}
#endif
			break;
		}

		default:
			BaseClass::HandleAnimEvent(pEvent);
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// return the position where an object should be staged before throwing
//-----------------------------------------------------------------------------
Vector CNPC_Advisor::GetThrowFromPos( CBaseEntity *pEnt )
{
	Assert(pEnt);
	Assert(pEnt->VPhysicsGetObject());
	const CCollisionProperty *cProp = pEnt->CollisionProp();
	Assert(cProp);

	float effecRadius = cProp->BoundingRadius(); // radius of object (important for kickout)
	float howFarInFront = advisor_throw_stage_distance.GetFloat() + effecRadius * 1.43f;// clamp(lenToPlayer - posDist + effecRadius,effecRadius*2,90.f + effecRadius);
	
	Vector fwd;
	GetVectors(&fwd,NULL,NULL);
	
	return GetAbsOrigin() + fwd*howFarInFront;
}
#endif


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Advisor::Precache()
{
	BaseClass::Precache();
	
#ifdef EZ2
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/advisor_combat.mdl" ) );
	}

	UTIL_PrecacheOther( "npc_advisor_flyer" );

	PrecacheScriptSound( "NPC_Advisor.BreatheLoop" );
	PrecacheScriptSound( "NPC_Advisor.shieldblock" );
	PrecacheScriptSound( "NPC_Advisor.shieldup" );
	PrecacheScriptSound( "NPC_Advisor.shielddown" );
	PrecacheScriptSound( "DoSpark" );

	PrecacheParticleSystem( "Advisor_Psychic_Shield_Idle" );
	//PrecacheParticleSystem( "Advisor_Throw_Obj_Charge" );
	PrecacheParticleSystem( "tele_warp" );

	PrecacheParticleSystem( "blood_advisor_shrapnel_impact" );
	PrecacheParticleSystem( "blood_advisor_shrapnel_spurt_1" );
	PrecacheParticleSystem( "blood_advisor_shrapnel_spurt_2" );
	//PrecacheParticleSystem( "blood_antlionguard_injured_heavy" );
	//PrecacheParticleSystem( "blood_drip_synth_01" );
#endif
		
	PrecacheModel( STRING( GetModelName() ) );

#if NPC_ADVISOR_HAS_BEHAVIOR
	PrecacheModel( "sprites/lgtning.vmt" );
#endif

#ifdef EZ2
	PrecacheModel( "models/advisor_combat_arm_r_detached.mdl" );
	PrecacheModel( "models/advisor_combat_arm_l_detached.mdl" );
#endif

	PrecacheScriptSound( "NPC_Advisor.Blast" );
	PrecacheScriptSound( "BaseCombatCharacter.CorpseGib" );	//NPC_Advisor.Gib
	PrecacheScriptSound( "NPC_Advisor.Idle" );
	PrecacheScriptSound( "NPC_Advisor.Alert" );
	PrecacheScriptSound( "NPC_Advisor.Die" );
	PrecacheScriptSound( "NPC_Advisor.Pain" );
	PrecacheScriptSound( "NPC_Advisor.ObjectChargeUp" );
	PrecacheParticleSystem( "Advisor_Psychic_Beam" );
	PrecacheParticleSystem( "advisor_object_charge" );
	PrecacheModel("sprites/greenglow1.vmt");

#ifdef EZ2
	PrecacheScriptSound( "NPC_Advisor.Melee_Hit" );
	PrecacheScriptSound( "NPC_Advisor.Melee_Miss" );

	PrecacheScriptSound( "NPC_Advisor.ArmDetach" );
#else
	PrecacheScriptSound("NPC_Stalker.Hit");
#endif
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Advisor::IdleSound()
{
	EmitSound( "NPC_Advisor.Idle" );
}


void CNPC_Advisor::AlertSound()
{
	EmitSound( "NPC_Advisor.Alert" );
}


void CNPC_Advisor::PainSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_Advisor.Pain" );
}


void CNPC_Advisor::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_Advisor.Die" );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Advisor::DrawDebugTextOverlays()
{
	int nOffset = BaseClass::DrawDebugTextOverlays();
	return nOffset;
}


#if NPC_ADVISOR_HAS_BEHAVIOR
//-----------------------------------------------------------------------------
// Determines which sounds the advisor cares about.
//-----------------------------------------------------------------------------
int CNPC_Advisor::GetSoundInterests()
{
	return SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_DANGER;
}


//-----------------------------------------------------------------------------
// record the last time we heard a combat sound
//-----------------------------------------------------------------------------
bool CNPC_Advisor::QueryHearSound( CSound *pSound )
{
	// Disregard footsteps from our own class type
	CBaseEntity *pOwner = pSound->m_hOwner;
	if ( pOwner && pSound->IsSoundType( SOUND_COMBAT ) && pSound->SoundChannel() != SOUNDENT_CHANNEL_NPC_FOOTSTEP && pSound->m_hOwner.IsValid() && pOwner->IsPlayer() )
	{
		// Msg("Heard player combat.\n");
		m_flLastPlayerAttackTime = gpGlobals->curtime;
	}

	return BaseClass::QueryHearSound(pSound);
}

//-----------------------------------------------------------------------------
// designer hook for setting throw rate
//-----------------------------------------------------------------------------
void CNPC_Advisor::InputSetThrowRate( inputdata_t &inputdata )
{
	advisor_throw_rate.SetValue(inputdata.value.Float());
}

//-----------------------------------------------------------------------------
// Sets the number of staging points to levitate the props before being launched
//-----------------------------------------------------------------------------
void CNPC_Advisor::InputSetStagingNum( inputdata_t &inputdata )
{
	m_iStagingNum = inputdata.value.Int();
}

//-----------------------------------------------------------------------------
//  cause the player to be pinned to a point in space
//-----------------------------------------------------------------------------
void CNPC_Advisor::InputPinPlayer( inputdata_t &inputdata )
{
	string_t targetname = inputdata.value.StringID();

	// null string means designer is trying to unpin the player
	if (!targetname)
	{
		m_hPlayerPinPos = NULL;
	}

	// otherwise try to look up the entity and make it a target.
	CBaseEntity *pEnt = gEntList.FindEntityByName(NULL,targetname);

	if (pEnt)
	{
		m_hPlayerPinPos = pEnt;
#ifdef EZ2
		SetCondition( COND_ADVISOR_FORCE_PIN );
#endif
	}
	else
	{
		// if we couldn't find the target, just bail on the behavior.
		Warning("Advisor tried to pin player to %s but that does not exist.\n", targetname.ToCStr());
		m_hPlayerPinPos = NULL;
	}
}

//-----------------------------------------------------------------------------
//Stops the Advisor to try pin the player into a point in space
//-----------------------------------------------------------------------------
void CNPC_Advisor::StopPinPlayer(inputdata_t &inputdata)
{
#ifdef EZ2
	ClearCondition( COND_ADVISOR_FORCE_PIN );
#endif
	m_hPlayerPinPos = NULL;
}

#ifdef EZ2
void CNPC_Advisor::InputStartFlying( inputdata_t & inputdata )
{
	// Make sure to persist the "Advisor Flying" key value if the advisor receives an input to start or stop flying
	// This is important for displacement via the xen grenade or displacer pistol
	m_bAdvisorFlyer = true;
	StartFlying();
}

void CNPC_Advisor::InputStopFlying( inputdata_t & inputdata )
{
	// Make sure to persist the "Advisor Flying" key value if the advisor receives an input to start or stop flying
	// This is important for displacement via the xen grenade or displacer pistol
	m_bAdvisorFlyer = false;
	StopFlying();
}

void CNPC_Advisor::InputSetFollowTarget( inputdata_t & inputdata )
{
	m_hFollowTarget = inputdata.value.Entity();

	if (m_hFollowTarget)
		SetContextThink( &CNPC_Advisor::FlyThink, gpGlobals->curtime + TICK_INTERVAL, ADVISOR_FLY_THINK );
	else
		SetContextThink( NULL, 0.0f, ADVISOR_FLY_THINK );
}

void CNPC_Advisor::InputStartPsychicShield( inputdata_t & inputdata )
{
	m_bPsychicShieldOn = true;
	ApplyShieldedEffects();
}

void CNPC_Advisor::InputStopPsychicShield( inputdata_t & inputdata )
{
	m_bPsychicShieldOn = false;
	ApplyShieldedEffects();
}

void CNPC_Advisor::InputDetachRightArm( inputdata_t &inputdata )
{
	int iCurAccessory = GetCurrentAccessories();
	AdvisorAccessory_t iNewBody = ADVISOR_ACCESSORY_NONE;

	switch (iCurAccessory)
	{
		case ADVISOR_ACCESSORY_ARMS:
			iNewBody = ADVISOR_ACCESSORY_L_ARM;
			break;

		case ADVISOR_ACCESSORY_R_ARM:
			iNewBody = ADVISOR_ACCESSORY_ARMS_STUBBED;
			break;
	}

	if (iNewBody != ADVISOR_ACCESSORY_NONE)
	{
		SetCurrentAccessories( iNewBody );

		EmitSound( "NPC_Advisor.ArmDetach" );

		CreateDetachedArm( false );
	}
}

void CNPC_Advisor::InputDetachLeftArm( inputdata_t &inputdata )
{
	int iCurAccessory = GetCurrentAccessories();
	AdvisorAccessory_t iNewBody = ADVISOR_ACCESSORY_NONE;

	switch (iCurAccessory)
	{
		case ADVISOR_ACCESSORY_ARMS:
			iNewBody = ADVISOR_ACCESSORY_R_ARM;
			break;

		case ADVISOR_ACCESSORY_L_ARM:
			iNewBody = ADVISOR_ACCESSORY_ARMS_STUBBED;
			break;
	}

	if (iNewBody != ADVISOR_ACCESSORY_NONE)
	{
		SetCurrentAccessories( iNewBody );

		CreateDetachedArm( true );
	}
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Advisor::OnScheduleChange( void )
{
	Write_AllBeamsOff();
	m_hvStagedEnts.RemoveAll();

#ifdef EZ2
	if (!m_hPlayerPinPos)
	{
		// Make sure we're not pinning
		CBasePlayer *pLocalPlayer = AI_GetSinglePlayer();
		if (pLocalPlayer && pLocalPlayer->GetMoveType() == MOVETYPE_FLY)
		{
			inputdata_t dummy;
			StopPinPlayer( dummy );
			pLocalPlayer->SetGravity( 1.0f );
			pLocalPlayer->SetMoveType( MOVETYPE_WALK );
			Write_BeamOff( pLocalPlayer );
			beamonce = 1;
		}
	}

	SetEyeState( ADVISOR_EYE_NORMAL );

	SetIdealActivity( ACT_IDLE );
#endif

	BaseClass::OnScheduleChange();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Advisor::GatherConditions( void )
{
	BaseClass::GatherConditions();

	// Handle script state changes
	bool bInScript = IsInAScript();
	if ( ( m_bWasScripting && bInScript == false ) || ( m_bWasScripting == false && bInScript ) )
	{
		SetCondition( COND_ADVISOR_PHASE_INTERRUPT );
	}
	
	// Retain this
	m_bWasScripting = bInScript;

#ifdef EZ2
	if (m_fStaggerUntilTime > gpGlobals->curtime)
	{
		// Pause flight
		if ( m_bAdvisorFlyer && m_hAdvisorFlyer )
		{
			StopFlying();
		}

		// Stop pinning
		inputdata_t dummy;
		StopPinPlayer( dummy );
		CBasePlayer *pLocalPlayer = AI_GetSinglePlayer();
		if (pLocalPlayer)
		{
			pLocalPlayer->SetGravity( 1.0f );
			pLocalPlayer->SetMoveType( MOVETYPE_WALK );
			Write_BeamOff( pLocalPlayer );
			beamonce = 1;
		}

		SetCondition( COND_ADVISOR_STAGGER );
	}
	else
	{
		// Resume flight
		if ( m_bAdvisorFlyer && !m_hAdvisorFlyer )
		{
			StartFlying();
		}
		ClearCondition( COND_ADVISOR_STAGGER );
	}

	// Don't fall to ground!
	ClearCondition( COND_FLOATING_OFF_GROUND );
#endif
}

//-----------------------------------------------------------------------------
// designer hook for yanking an object into the air right now
//-----------------------------------------------------------------------------
void CNPC_Advisor::InputWrenchImmediate( inputdata_t &inputdata )
{
	string_t groupname = inputdata.value.StringID();

	Assert(!!groupname);

	// for all entities with that name that aren't floating, punt them at me and add them to the levitation

	CBaseEntity *pEnt = NULL;

	const Vector &myPos = GetAbsOrigin() + Vector(0,36.0f,0);

	// conditional assignment: find an entity by name and save it into pEnt. Bail out when none are left.
	while ( ( pEnt = gEntList.FindEntityByName(pEnt,groupname) ) != NULL )
	{
		// if I'm not already levitating it, and if I didn't just throw it
		if (!m_physicsObjects.HasElement(pEnt) )
		{
			// add to levitation
			IPhysicsObject *pPhys = pEnt->VPhysicsGetObject();
			if ( pPhys )
			{
				// if the object isn't moveable, make it so.
                if ( !pPhys->IsMoveable() )
				{
					pPhys->EnableMotion( true );
				}

				// first, kick it at me
				Vector objectToMe;
				pPhys->GetPosition(&objectToMe,NULL);
                objectToMe = myPos - objectToMe;
				// compute a velocity that will get it here in about a second
				objectToMe /= (1.5f * gpGlobals->frametime);

				objectToMe *= random->RandomFloat(0.25f,1.0f);

				pPhys->SetVelocity( &objectToMe, NULL );

				// add it to tracked physics objects
				m_physicsObjects.AddToTail( pEnt );

				m_pLevitateController->AttachObject( pPhys, false );
				pPhys->Wake();
			}
			else
			{
				Warning( "Advisor tried to wrench %s, but it is not moveable!", pEnt->GetEntityName().ToCStr());
			}
		}
	}

}



//-----------------------------------------------------------------------------
// write a message turning a beam on
//-----------------------------------------------------------------------------
void CNPC_Advisor::Write_BeamOn(  CBaseEntity *pEnt )
{
	Assert( pEnt );

	// Protect against null pointer exceptions
	if ( pEnt == NULL )
		return;

#ifdef EZ2
	m_bThrowBeamShield = true;
	ApplyShieldedEffects();
#endif

	EntityMessageBegin( this, true );
		WRITE_BYTE( ADVISOR_MSG_START_BEAM );
		WRITE_LONG( pEnt->entindex() );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// write a message turning a beam off
//-----------------------------------------------------------------------------
void CNPC_Advisor::Write_BeamOff( CBaseEntity *pEnt )
{
	Assert( pEnt );

	// Protect against null pointer exceptions
	if ( pEnt == NULL )
		return;

#ifdef EZ2
	m_bThrowBeamShield = false;
	ApplyShieldedEffects();
#endif

	EntityMessageBegin( this, true );
		WRITE_BYTE( ADVISOR_MSG_STOP_BEAM );
		WRITE_LONG( pEnt->entindex() );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// tell client to kill all beams
//-----------------------------------------------------------------------------
void CNPC_Advisor::Write_AllBeamsOff( void )
{
#ifdef EZ2
	m_bThrowBeamShield = false;
	ApplyShieldedEffects();
#endif

	EntityMessageBegin( this, true );
		WRITE_BYTE( ADVISOR_MSG_STOP_ALL_BEAMS );
	MessageEnd();
}

#ifdef EZ2
//-----------------------------------------------------------------------------
//  Purpose: Overridden to handle particles
//  Weird place to start this, but it worked great for Zombigaunts
//-----------------------------------------------------------------------------
void CNPC_Advisor::StartEye( void )
{
	// Start particle
	if (GetSleepState() == AISS_AWAKE)
	{
		SetContextThink( &CNPC_Advisor::ParticleThink, gpGlobals->curtime + 0.1, ADVISOR_PARTICLE_THINK );
	}

	BaseClass::StartEye();
}


void CNPC_Advisor::ParticleThink()
{
	if ( m_bPsychicShieldOn && !HasCondition( COND_ADVISOR_STAGGER ) )
	{
		DispatchParticleEffect( "Advisor_Psychic_Shield_Idle", PATTACH_ABSORIGIN_FOLLOW, this, 0, false );
	}

	ApplyShieldedEffects();
	
	float flHealthRatio = (float)GetHealth() / (float)GetMaxHealth();
	if ( flHealthRatio < advisor_sparks_health_ratio_max.GetFloat() )
	{
		// Random chance of sparks coming from one of our jets, increases with lower health
		float flChance = RandomFloat( 0, flHealthRatio );
		if (flChance <= advisor_sparks_health_ratio_min.GetFloat())
		{
			Vector vecSparkPosition, vecSparkDir;
			switch (RandomInt( 0, 3 ))
			{
				case 0: GetAttachment( gm_nLFrontTopJetAttachment, vecSparkPosition, &vecSparkDir ); break;
				case 1: GetAttachment( gm_nRFrontTopJetAttachment, vecSparkPosition, &vecSparkDir ); break;
				case 2: GetAttachment( gm_nLRearTopJetAttachment, vecSparkPosition, &vecSparkDir ); break;
				case 3: GetAttachment( gm_nRRearTopJetAttachment, vecSparkPosition, &vecSparkDir ); break;
			}

			g_pEffects->Sparks( vecSparkPosition, (1.0f / flHealthRatio), RandomInt( 1, 4 ), &vecSparkDir );
			EmitSound( "DoSpark" );
		}
	}

	SetNextThink( gpGlobals->curtime + 1.0f, ADVISOR_PARTICLE_THINK );
}
#endif

//-----------------------------------------------------------------------------
// input wrapper around Write_BeamOn
//-----------------------------------------------------------------------------
void CNPC_Advisor::InputTurnBeamOn( inputdata_t &inputdata )
{
	// inputdata should specify a target
	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, inputdata.value.StringID() );
	if ( pTarget )
	{
		Write_BeamOn( pTarget );
	}
	else
	{
		Warning("InputTurnBeamOn could not find object %s", inputdata.value.String() );
	}
}


//-----------------------------------------------------------------------------
// input wrapper around Write_BeamOff
//-----------------------------------------------------------------------------
void CNPC_Advisor::InputTurnBeamOff( inputdata_t &inputdata )
{
	// inputdata should specify a target
	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, inputdata.value.StringID() );
	if ( pTarget )
	{
		Write_BeamOff( pTarget );
	}
	else
	{
		Warning("InputTurnBeamOn could not find object %s", inputdata.value.String() );
	}
}


void CNPC_Advisor::InputElightOn( inputdata_t &inputdata )
{
	EntityMessageBegin( this, true );
	WRITE_BYTE( ADVISOR_MSG_START_ELIGHT );
	MessageEnd();
}

void CNPC_Advisor::InputElightOff( inputdata_t &inputdata )
{
	EntityMessageBegin( this, true );
	WRITE_BYTE( ADVISOR_MSG_STOP_ELIGHT );
	MessageEnd();
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
float CNPC_AdvisorFlyer::GetGoalDistance()
{
	if (GetOwnerEntity())
	{
		CNPC_Advisor *pAdvisor = static_cast<CNPC_Advisor *>(GetOwnerEntity());

		// If preparing to throw or have no defenses, get away
		if (pAdvisor->IsPreparingToThrow() || !(pAdvisor->CapabilitiesGet() & bits_CAP_INNATE_MELEE_ATTACK1))
		{
			return BaseClass::GetGoalDistance() * 2.5f;
		}
	}

	return BaseClass::GetGoalDistance();
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
float CNPC_AdvisorFlyer::MinGroundDist()
{
	if (GetOwnerEntity())
	{
		CNPC_Advisor *pAdvisor = static_cast<CNPC_Advisor *>(GetOwnerEntity());

		// If the advisor is in melee range, get closer
		if (pAdvisor->HasCondition( COND_CAN_MELEE_ATTACK1 ))
		{
			return 72.0f;
		}
	}

	return 96.0f;
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
float CAI_AdvisorMotor::GetPitchSpeed() const
{
	if (GetAdvisorOuter())
	{
		return GetAdvisorOuter()->MaxPitchSpeed();
	}

	return 18.0f;
}

inline CNPC_Advisor *CAI_AdvisorMotor::GetAdvisorOuter()
{
	return static_cast<CNPC_Advisor*>(GetOuter());
}

inline const CNPC_Advisor *CAI_AdvisorMotor::GetAdvisorOuter() const
{
	return static_cast<const CNPC_Advisor*>(GetOuter());
}

inline CNPC_AdvisorFlyer *CAI_AdvisorMotor::GetAdvisorFlyer()
{
	return GetAdvisorOuter()->GetAdvisorFlyer();
}

inline const CNPC_AdvisorFlyer *CAI_AdvisorMotor::GetAdvisorFlyer() const
{
	return GetAdvisorOuter()->GetAdvisorFlyer();
}
#endif


//==============================================================================================
// MOTION CALLBACK
//==============================================================================================
CAdvisorLevitate::simresult_e	CAdvisorLevitate::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
   	// this function can be optimized to minimize branching if necessary (PPE branch prediction) 
	CNPC_Advisor *pAdvisor = static_cast<CNPC_Advisor *>(m_Advisor.Get());
	Assert(pAdvisor);

	if ( !OldStyle() )
	{	// independent movement of all objects
		// if an object was recently thrown, just zero out its gravity.
		CBaseEntity *pEnt = static_cast<CBaseEntity *>(pObject->GetGameData());
		if (pAdvisor->DidThrow(pEnt))
		{
			linear = Vector( 0, 0, GetCurrentGravity() );
			
			return SIM_GLOBAL_ACCELERATION;
		}
		else
		{
			Vector vel; AngularImpulse angvel;
			pObject->GetVelocity(&vel,&angvel);
			Vector pos;
			pObject->GetPosition(&pos,NULL);
			bool bMovingUp = vel.z > 0;

			// if above top limit and moving up, move down. if below bottom limit and moving down, move up.
			if (bMovingUp)
			{
				if (pos.z > m_vecGoalPos2.z)
				{
					// turn around move down
					linear = Vector( 0, 0, Square((1.0f - m_flFloat)) * GetCurrentGravity() );
					angular = Vector( 0, -5, 0 );
				}
				else
				{	// keep moving up 
					linear = Vector( 0, 0, (1.0f + m_flFloat) * GetCurrentGravity() );
					angular = Vector( 0, 0, 10 );
				}
			}
			else
			{
				if (pos.z < m_vecGoalPos1.z)
				{
					// turn around move up
					linear = Vector( 0, 0, Square((1.0f + m_flFloat)) * GetCurrentGravity() );
					angular = Vector( 0, 5, 0 );
				}
				else
				{	// keep moving down
					linear = Vector( 0, 0, (1.0f - m_flFloat) * GetCurrentGravity() );
					angular = Vector( 0, 0, 10 );
				}
			}

#ifdef EZ2
			// DEACTIVATED FOR NOW
			// We could really use a solution which pushes props we aren't staging
			/*
			float flOurRadius = 64.0f;
			if (pEnt)
				flOurRadius = pEnt->BoundingRadius() * 1.5f;

			Vector vecAdvisorCenter = pAdvisor->GetAbsOrigin();
			float flAdvisorDistSqr = (vecAdvisorCenter - pos).LengthSqr() - Square( flOurRadius );
			if (flAdvisorDistSqr <= Square(advisor_push_max_radius.GetFloat()))
			{
				Vector vecAdvisorVel;
				VectorMultiply( pAdvisor->GetAbsVelocity(), advisor_push_delta.GetFloat(), vecAdvisorVel );
			
				float flTotalRadius = flOurRadius + pAdvisor->BoundingRadius() * 2.0f;
				float t1, t2;
				if ( IntersectRayWithSphere( vecAdvisorCenter, vecAdvisorVel,
					pos, flTotalRadius, &t1, &t2 ) )
				{
					// Push it away from the advisor's path
					Vector vecClosest;
					CalcClosestPointOnLineSegment( pos, vecAdvisorCenter, vecAdvisorCenter + vecAdvisorVel, vecClosest, NULL );

					Vector vecForceDir = (pos - vecClosest);
					VectorNormalize( vecForceDir );
					vecForceDir *= pObject->GetMass();

					//angular += vecForceDir;

					// The closer we are to the advisor, the more force we'll get
					vecForceDir *= RemapVal( flAdvisorDistSqr, Square(advisor_push_max_radius.GetFloat()), 0.0f, 0.0f, advisor_push_max_force_scale.GetFloat() );
					
					linear += vecForceDir;
				}
			}
			*/
#endif
			
			return SIM_GLOBAL_ACCELERATION;
		}

		//NDebugOverlay::Cross3D(pos,24.0f,255,255,0,true,0.04f);
		
	}
	else // old stateless technique
	{
		Warning("Advisor using old-style object movement!\n");

		/* // obsolete
		CBaseEntity *pEnt = (CBaseEntity *)pObject->GetGameData();
		Vector vecDir1 = m_vecGoalPos1 - pEnt->GetAbsOrigin();
		VectorNormalize( vecDir1 );
		
		Vector vecDir2 = m_vecGoalPos2 - pEnt->GetAbsOrigin();
		VectorNormalize( vecDir2 );
		*/
	
		linear = Vector( 0, 0, m_flFloat * GetCurrentGravity() );// + m_flFloat * 0.5 * ( vecDir1 + vecDir2 );
		angular = Vector( 0, 0, 10 );
		
		return SIM_GLOBAL_ACCELERATION;
	}

}


//==============================================================================================
// ADVISOR PHYSICS DAMAGE TABLE
//==============================================================================================
static impactentry_t advisorLinearTable[] =
{
	{ 100*100,	10 },
	{ 250*250,	25 },
	{ 350*350,	50 },
	{ 500*500,	75 },
	{ 1000*1000,100 },
};

static impactentry_t advisorAngularTable[] =
{
	{  50* 50, 10 },
	{ 100*100, 25 },
	{ 150*150, 50 },
	{ 200*200, 75 },
};

static impactdamagetable_t gAdvisorImpactDamageTable =
{
	advisorLinearTable,
	advisorAngularTable,
	
	ARRAYSIZE(advisorLinearTable),
	ARRAYSIZE(advisorAngularTable),

	200*200,// minimum linear speed squared
	180*180,// minimum angular speed squared (360 deg/s to cause spin/slice damage)
	15,		// can't take damage from anything under 15kg

	10,		// anything less than 10kg is "small"
	5,		// never take more than 1 pt of damage from anything under 15kg
	128*128,// <15kg objects must go faster than 36 in/s to do damage

	45,		// large mass in kg 
	2,		// large mass scale (anything over 500kg does 4X as much energy to read from damage table)
	1,		// large mass falling scale
	0,		// my min velocity
};

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const impactdamagetable_t
//-----------------------------------------------------------------------------
const impactdamagetable_t &CNPC_Advisor::GetPhysicsImpactDamageTable( void )
{
	return advisor_use_impact_table.GetBool() ? gAdvisorImpactDamageTable : BaseClass::GetPhysicsImpactDamageTable();
}



#if NPC_ADVISOR_HAS_BEHAVIOR
//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_advisor, CNPC_Advisor )

#ifdef EZ2
	DECLARE_ACTIVITY( ACT_ADVISOR_OBJECT_R_HOLD )
	DECLARE_ACTIVITY( ACT_ADVISOR_OBJECT_R_THROW )
	DECLARE_ACTIVITY( ACT_ADVISOR_OBJECT_L_HOLD )
	DECLARE_ACTIVITY( ACT_ADVISOR_OBJECT_L_THROW )
	DECLARE_ACTIVITY( ACT_ADVISOR_OBJECT_BOTH_HOLD )
	DECLARE_ACTIVITY( ACT_ADVISOR_OBJECT_BOTH_THROW )

	DECLARE_ANIMEVENT( ADVISOR_AE_DETACH_R_ARM )
	DECLARE_ANIMEVENT( ADVISOR_AE_DETACH_L_ARM )
#endif

	DECLARE_TASK( TASK_ADVISOR_FIND_OBJECTS )
	DECLARE_TASK( TASK_ADVISOR_LEVITATE_OBJECTS )
	/*
	DECLARE_TASK( TASK_ADVISOR_PICK_THROW_OBJECT )
	DECLARE_TASK( TASK_ADVISOR_THROW_OBJECT )
	*/

	DECLARE_CONDITION( COND_ADVISOR_PHASE_INTERRUPT )	// A stage has interrupted us
#ifdef EZ2
	DECLARE_CONDITION( COND_ADVISOR_FORCE_PIN )
	DECLARE_CONDITION( COND_ADVISOR_STAGGER )
#endif

	DECLARE_TASK( TASK_ADVISOR_STAGE_OBJECTS ) // haul all the objects into the throw-from slots
	DECLARE_TASK( TASK_ADVISOR_BARRAGE_OBJECTS ) // hurl all the objects in sequence

	DECLARE_TASK( TASK_ADVISOR_PIN_PLAYER ) // pinion the player to a point in space

#ifdef EZ2
	DECLARE_TASK( TASK_ADVISOR_STAGGER )
	DECLARE_TASK( TASK_ADVISOR_WAIT_FOR_ACTIVITY )
#endif
	
#ifndef EZ2
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ADVISOR_COMBAT,

		"	Tasks"
		"		TASK_ADVISOR_FIND_OBJECTS			0"
		"		TASK_ADVISOR_LEVITATE_OBJECTS		0"
		"		TASK_ADVISOR_STAGE_OBJECTS			1"
		"		TASK_FACE_ENEMY						0"
		"		TASK_ADVISOR_BARRAGE_OBJECTS		0"
		"	"
		"	Interrupts"
		"		COND_ADVISOR_PHASE_INTERRUPT"
		"		COND_ENEMY_DEAD"
		"		COND_CAN_MELEE_ATTACK1"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
	SCHED_ADVISOR_IDLE_STAND,

		"	Tasks"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
		"		TASK_WAIT				3"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_ADVISOR_PHASE_INTERRUPT"
	)
	
	DEFINE_SCHEDULE
	(
		SCHED_ADVISOR_TOSS_PLAYER,

		"	Tasks"
		"		TASK_FACE_ENEMY						0"
		"		TASK_ADVISOR_PIN_PLAYER				0"
		"	"
		"	Interrupts"
	)
#else
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ADVISOR_COMBAT,

		"	Tasks"
		"		TASK_ADVISOR_FIND_OBJECTS			0"
		"		TASK_ADVISOR_LEVITATE_OBJECTS		0"
		"		TASK_ADVISOR_STAGE_OBJECTS			1"
		"		TASK_ADVISOR_BARRAGE_OBJECTS		0"
		"		TASK_ADVISOR_WAIT_FOR_ACTIVITY		0"
		"	"
		"	Interrupts"
		"		COND_ADVISOR_PHASE_INTERRUPT"
		"		COND_ENEMY_DEAD"
		//"		COND_CAN_MELEE_ATTACK1" // See BuildScheduleTestBits()
		"       COND_ADVISOR_STAGGER"
	)

		//=========================================================
		DEFINE_SCHEDULE
		(
			SCHED_ADVISOR_IDLE_STAND,

			"	Tasks"
			"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
			"		TASK_WAIT				3"
			""
			"	Interrupts"
			"		COND_NEW_ENEMY"
			"		COND_SEE_FEAR"
			"		COND_ADVISOR_PHASE_INTERRUPT"
		)

		DEFINE_SCHEDULE
		(
			SCHED_ADVISOR_TOSS_PLAYER,

			"	Tasks"
			//"		TASK_FACE_ENEMY						0"
			"		TASK_ADVISOR_PIN_PLAYER				0"
			"	"
			"	Interrupts"
			"       COND_ADVISOR_STAGGER"
		)

		DEFINE_SCHEDULE
		(
			SCHED_ADVISOR_STAGGER,

			"	Tasks"
			"		TASK_ADVISOR_STAGGER	0"
			"		TASK_BIG_FLINCH			0"
			"		TASK_WAIT				1"
			""
			"	Interrupts"
			"		COND_ADVISOR_PHASE_INTERRUPT"
		)

		DEFINE_SCHEDULE
		(
			SCHED_ADVISOR_MELEE_ATTACK1,

			"	Tasks"
			"		TASK_MELEE_ATTACK1		0"
			""
			"	Interrupts"
			"		COND_NEW_ENEMY"
			"		COND_ENEMY_DEAD"
			"		COND_LIGHT_DAMAGE"
			"		COND_HEAVY_DAMAGE"
		)
#endif

AI_END_CUSTOM_NPC()
#endif
