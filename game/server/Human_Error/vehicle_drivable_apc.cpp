//=================== Half-Life 2: Short Stories Mod 2007 =====================//
//
// Purpose:	Drivable APC
//
//=============================================================================//

#include "cbase.h"
#include "engine/IEngineSound.h"
#include "in_buttons.h"
#include "ammodef.h"
#include "IEffects.h"
#include "beam_shared.h"
#include "soundenvelope.h"
#include "decals.h"
#include "soundent.h"
#include "grenade_ar2.h"
#include "te_effect_dispatch.h"

//for the stuff that kills NPCs from the way of the APC
#include "vphysics/friction.h"
#include "vphysicsupdateai.h"
#include "physics_npc_solver.h"

#include "hl2_player.h"
#include "ndebugoverlay.h"
#include "movevars_shared.h"
#include "bone_setup.h"
#include "ai_basenpc.h"
#include "ai_hint.h"
#include "npc_crow.h"
#include "globalstate.h"
#include "vehicle_drivable_apc.h"
#include "weapon_rpg.h"
#include "rumble_shared.h"

#ifdef EZ
#include "vphysics/constraints.h"
#include "physics_saverestore.h"
#include "weapon_physcannon.h"
#include "eventqueue.h"
#include "hl2_shareddefs.h"
#endif

#ifdef EZ2
#include "ez2/ez2_player.h"
#include "ez2/npc_wilson.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	VEHICLE_HITBOX_DRIVER		1
#define LOCK_SPEED					10
#define APC_GUN_YAW					"vehicle_weapon_yaw"
#define APC_GUN_PITCH				"vehicle_weapon_pitch"
#define APC_LOWER_VIEW				"vehicle_lower_view"

#define CANNON_MAX_UP_PITCH			45
#define CANNON_MAX_DOWN_PITCH		11
#define CANNON_MAX_LEFT_YAW			90
#define CANNON_MAX_RIGHT_YAW		90

#define OVERTURNED_EXIT_WAITTIME	2.0f

#define APC_STEERING_SLOW_ANGLE	50.0f
#define APC_STEERING_FAST_ANGLE	15.0f

#define APC_DELTA_LENGTH_MAX	12.0f			// 1 foot
#define APC_FRAMETIME_MIN		1e-6

//APC defines:

#define MACHINE_GUN_BURST_SIZE		100
#define MACHINE_GUN_BURST_TIME		0.075f
#define MACHINE_GUN_RELOAD_TIME		0.15f

#define ROCKET_SALVO_SIZE				sk_apc_rocket_salvo_size.GetFloat()
#define ROCKET_DELAY_TIME				sk_apc_rocket_delay_time.GetFloat()
#define ROCKET_MIN_BURST_PAUSE_TIME		sk_apc_rocket_min_burst_pause_time.GetFloat()
#define ROCKET_MAX_BURST_PAUSE_TIME		sk_apc_rocket_max_burst_pause_time.GetFloat()
#define ROCKET_SPEED					1500 //Originally 800
#define DEATH_VOLLEY_ROCKET_COUNT		4
#define DEATH_VOLLEY_MIN_FIRE_TIME		0.333
#define DEATH_VOLLEY_MAX_FIRE_TIME		0.166


ConVar	hud_apchint_numentries( "hud_apchint_numentries", "10", FCVAR_NONE );
ConVar	g_apcexitspeed( "g_apcexitspeed", "100", FCVAR_CHEAT );

extern ConVar autoaim_max_dist;

extern ConVar phys_upimpactforcescale;

//ConVar apc_pitch_fix("hlss_apc_pitch_fix", "0" );
#define HLSS_APC_PITCH_FIX -1.5f

ConVar apc_zoomed_pitch_fix("hlss_apc_zoomed_pitch_fix", "0" );
ConVar apc_zoomed_yaw_fix("hlss_apc_zoomed_yaw_fix", "0" );

ConVar apc_no_rpg_while_moving( "hlss_apc_no_rpg_while_moving", "0", FCVAR_CHEAT );
ConVar apc_hull_trace_attack( "hlss_apc_hull_trace_attack", "0", FCVAR_CHEAT );

ConVar	sk_apc_rocket_salvo_size( "sk_apc_rocket_salvo_size", "3" );
ConVar	sk_apc_rocket_delay_time( "sk_apc_rocket_delay_time", "0.75" );
ConVar	sk_apc_rocket_min_burst_pause_time( "sk_apc_rocket_min_burst_pause_time", "3" );
ConVar	sk_apc_rocket_max_burst_pause_time( "sk_apc_rocket_max_burst_pause_time", "4" );

#ifdef EZ
ConVar apc_target_glow( "apc_target_glow", "0", FCVAR_NONE );
ConVar	sk_apc_damage_normal( "sk_apc_damage_normal", "0.15" );
ConVar	sk_apc_damage_blast( "sk_apc_damage_blast", "0.1" );
ConVar	sk_apc_damage_vort( "sk_apc_damage_vort", "0.75" );

ConVar apc_constraint_force_limit( "apc_constraint_force_limit", "3500", FCVAR_NONE );
ConVar apc_constraint_torque_limit( "apc_constraint_torque_limit", "2500", FCVAR_NONE );

// APC Interactions
int	g_interactionAPCConstrain = 0;
int	g_interactionAPCUnconstrain = 0;
int	g_interactionAPCBreak = 0;
#endif

static void SolveBlockingProps( bool bBreakProps, CPropDrivableAPC *pVehicleEntity, IPhysicsObject *pVehiclePhysics );
static void SimpleCollisionResponse( Vector velocityIn, const Vector &normal, float coefficientOfRestitution, Vector *pVelocityOut );
static void KillBlockingEnemyNPCs( CBasePlayer *pPlayer, CBaseEntity *pVehicleEntity, IPhysicsObject *pVehiclePhysics );

int HLSS_SelectTargetType(CBaseEntity *pEntity);

BEGIN_DATADESC( CPropDrivableAPC )
	DEFINE_FIELD( m_flDangerSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_vecGunOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_aimYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_aimPitch, FIELD_FLOAT ),
	DEFINE_FIELD( m_throttleDisableTime, FIELD_TIME ),
	DEFINE_FIELD( m_flHandbrakeTime, FIELD_TIME ),
	DEFINE_FIELD( m_bInitialHandbrake, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flOverturnedTime, FIELD_TIME ),
	DEFINE_FIELD( m_vecLastEyePos, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecLastEyeTarget, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecEyeSpeed, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecTargetSpeed, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_bHeadlightIsOn, FIELD_BOOLEAN ),
#ifdef EZ
	DEFINE_FIELD( m_Spotlight, FIELD_EHANDLE ),

	DEFINE_PHYSPTR( m_pConstraint ),
	DEFINE_FIELD( m_hConstrainedEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flConstrainCooldown, FIELD_TIME ),
#endif

	DEFINE_FIELD( m_iNumberOfEntries, FIELD_INTEGER ),

	DEFINE_FIELD( m_flPlayerExitedTime, FIELD_TIME ),
	DEFINE_FIELD( m_flLastSawPlayerAt, FIELD_TIME ),
	DEFINE_FIELD( m_hLastPlayerInVehicle, FIELD_EHANDLE ),

	DEFINE_FIELD( m_bIsMounted,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hTarget,				FIELD_EHANDLE ),
	DEFINE_FIELD( m_iTargetType,			FIELD_INTEGER ),
	DEFINE_FIELD( m_flTargetSelectTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flLaserTargetTime,		FIELD_TIME ),

	DEFINE_FIELD( m_flMachineGunTime,				FIELD_TIME ),
	DEFINE_FIELD( m_flMachineGunReloadTime,			FIELD_TIME),
	DEFINE_FIELD( m_iMachineGunBurstLeft,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nMachineGunMuzzleAttachment,	FIELD_INTEGER ),
	DEFINE_FIELD( m_nMachineGunBaseAttachment,		FIELD_INTEGER ),

	DEFINE_FIELD( m_hRocketTarget,					FIELD_EHANDLE ),
	DEFINE_FIELD( m_iRocketSalvoLeft,				FIELD_INTEGER ),
	DEFINE_FIELD( m_flRocketTime,					FIELD_TIME ),
	DEFINE_FIELD( m_flRocketReloadTime,				FIELD_TIME ),

	DEFINE_FIELD( m_flNextPropAttackTime,			FIELD_TIME ),
	DEFINE_FIELD( m_flStopBreakTime,				FIELD_TIME ),
	DEFINE_FIELD( m_bShouldAttackProps,				FIELD_BOOLEAN ),

	DEFINE_KEYFIELD( m_bCannotMove,					FIELD_BOOLEAN, "cannotmove" ),

	DEFINE_FIELD( m_nRocketSide,					FIELD_INTEGER ),

	DEFINE_FIELD( m_flViewLowered,					FIELD_FLOAT ),

//	DEFINE_FIELD( m_bForcePlayerOut,				FIELD_BOOLEAN ),

	DEFINE_INPUTFUNC( FIELD_VOID, "ForcePlayerOut",	InputForcePlayerOut ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnableMove", InputEnableMove ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisableMove", InputDisableMove ),

#ifdef EZ
	DEFINE_INPUTFUNC( FIELD_VOID, "HeadlightOn", InputHeadlightOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "HeadlightOff", InputHeadlightOff ),

	DEFINE_INPUTFUNC( FIELD_EHANDLE, "ConstrainEntity", InputConstrainEntity ),
	DEFINE_INPUTFUNC( FIELD_VOID, "UnconstrainEntity", InputUnconstrainEntity ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ConstraintBroken", InputConstraintBroken ),
	DEFINE_INPUTFUNC( FIELD_VOID, "_ResetConstraintToDefault", InputResetConstraintToDefault ),

	DEFINE_OUTPUT( m_onOverturned, "OnOverturned" ),
	DEFINE_OUTPUT( m_onObjectAttached, "OnObjectAttached" ),
#endif

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CPropDrivableAPC, DT_PropDrivableAPC )
	SendPropBool( SENDINFO( m_bHeadlightIsOn ) ),
	SendPropInt( SENDINFO( m_iMachineGunBurstLeft ) ),
	SendPropInt( SENDINFO( m_iRocketSalvoLeft ) ),
	SendPropBool( SENDINFO( m_bIsMounted ) ),
	SendPropEHandle(SENDINFO(m_hTarget)),
	SendPropInt( SENDINFO( m_iTargetType ) ),
//	SendPropBool( SENDINFO( m_bHasTarget ) ),
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( prop_vehicle_drivable_apc, CPropDrivableAPC );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPropDrivableAPC::CPropDrivableAPC( void )
{
	m_flOverturnedTime = 0.0f;
	m_iNumberOfEntries = 0;

	m_vecEyeSpeed.Init();

	m_bUnableToFire = true;

	m_bCannotMove	= false;

	//TERO: lets start with full ammo
	m_iMachineGunBurstLeft = MACHINE_GUN_BURST_SIZE;
	m_iRocketSalvoLeft = ROCKET_SALVO_SIZE;

#ifdef EZ
	m_pConstraint = NULL;
	m_hConstrainedEntity = NULL;
#endif

	m_flNextPropAttackTime = 0 ;
	m_flStopBreakTime = 0;
	m_bShouldAttackProps = false;
}

void CPropDrivableAPC::InputDisableMove( inputdata_t &inputdata )
{
	m_bCannotMove = true;
}
void CPropDrivableAPC::InputEnableMove( inputdata_t &inputdata )
{
	m_bCannotMove = false;
}

#ifdef EZ
void CPropDrivableAPC::InputHeadlightOn( inputdata_t &inputdata )
{
	HeadlightTurnOn();
}

void CPropDrivableAPC::InputHeadlightOff( inputdata_t &inputdata )
{
	HeadlightTurnOff();
}

void CPropDrivableAPC::InputConstrainEntity( inputdata_t &inputdata )
{
	if (inputdata.value.Entity())
	{
		ConstrainEntity( inputdata.value.Entity(), NULL );
	}
}

void CPropDrivableAPC::InputUnconstrainEntity( inputdata_t &inputdata )
{
	UnconstrainEntity( false, ToBasePlayer(inputdata.pActivator) );
}

void CPropDrivableAPC::InputConstraintBroken( inputdata_t &inputdata )
{
	UnconstrainEntity( true );
}

void CPropDrivableAPC::InputResetConstraintToDefault( inputdata_t &inputdata )
{
	if ( m_hConstrainedEntity && m_pConstraint )
	{
		// Remove constraint and re-add it with default values
		physenv->DestroyConstraint( m_pConstraint );
		m_pConstraint = NULL;

		IPhysicsObject *pPhysics = VPhysicsGetObject();
		IPhysicsObject *pOtherPhysics = m_hConstrainedEntity->VPhysicsGetObject();

		constraint_fixedparams_t fixed;
		fixed.Defaults();
		fixed.InitWithCurrentObjectState( pPhysics, pOtherPhysics );
		fixed.constraint.Defaults();
		fixed.constraint.forceLimit	= ImpulseScale( pOtherPhysics->GetMass(), apc_constraint_force_limit.GetFloat() );
		fixed.constraint.torqueLimit = ImpulseScale( pOtherPhysics->GetMass(), apc_constraint_torque_limit.GetFloat() );
		m_pConstraint = physenv->CreateFixedConstraint( pPhysics, pOtherPhysics, NULL, fixed );
		m_pConstraint->SetGameData( (void *)this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::ConstrainEntity( CBaseEntity *pEntity, CBasePlayer *pPlayer )
{
	IPhysicsObject *pOtherPhysics = pEntity->VPhysicsGetObject();
	if ( !pOtherPhysics )
		return;

	// Dispatch NPC-specific interaction, returning true means it overrides this behavior
	if (pEntity->DispatchInteraction( g_interactionAPCConstrain, this, pPlayer ))
		return;

	if (pPlayer)
		pPlayer->ForceDropOfCarriedPhysObjects( pEntity );

	PhysDisableEntityCollisions( this, pEntity );

	IPhysicsObject *pPhysics = VPhysicsGetObject();

	Vector vecTopOrigin;
	QAngle vecTopAngles;
	matrix3x4_t matTopAttach;
	GetAttachment( m_nTopAttachAttachment, matTopAttach );
	MatrixAngles( matTopAttach, vecTopAngles, vecTopOrigin );

	// Teleport the entity to the point
	pEntity->Teleport( &vecTopOrigin, &vecTopAngles, NULL );

	matrix3x4_t matMinusOne;
	AngleMatrix( vec3_angle, Vector(0,0,-1), matMinusOne );

	Vector vecNewOrigin;
	trace_t tr;
	int i = 0;
	while (i < 32)
	{
		MatrixAngles( matTopAttach, vecTopAngles, vecNewOrigin );
		UTIL_TraceLine( vecNewOrigin, vecNewOrigin, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
		if (!tr.startsolid)
			break;
		else
		{
			ConcatTransforms( matTopAttach, matMinusOne, matTopAttach );
		}
		i++;
	}

	// Teleport the entity to the corrected point
	vecTopOrigin += (vecTopOrigin - vecNewOrigin);
	pEntity->Teleport( &vecTopOrigin, &vecTopAngles, NULL );

	// Test collision. Allow entity to move up to 32 units
	//matrix3x4_t matTopAttach;
	//GetAttachment( m_nTopAttachAttachment, matTopAttach );
	//
	//matrix3x4_t matPlusOne;
	//AngleMatrix( vec3_angle, Vector(0,0,1), matPlusOne );
	//
	//race_t tr;
	//nt i = 0;
	//hile (i < 32)
	//
	//	MatrixAngles( matTopAttach, vecTopAngles, vecTopOrigin );
	//	physcollision->TraceCollide( vecTopOrigin, vecTopOrigin, pOtherPhysics->GetCollide(), vecTopAngles, pPhysics->GetCollide(), GetAbsOrigin(), GetAbsAngles(), &tr );
	//	if (!tr.startsolid)
	//		break;
	//	else
	//	{
	//		ConcatTransforms( matTopAttach, matPlusOne, matTopAttach );
	//	}
	//	i++;
	//

	// Constrain it to the vehicle
	constraint_fixedparams_t fixed;
	fixed.Defaults();
	fixed.InitWithCurrentObjectState( pPhysics, pOtherPhysics );
	fixed.constraint.Defaults();
	fixed.constraint.forceLimit	= ImpulseScale( pOtherPhysics->GetMass(), apc_constraint_force_limit.GetFloat() );
	fixed.constraint.torqueLimit = ImpulseScale( pOtherPhysics->GetMass(), apc_constraint_torque_limit.GetFloat() );
	m_pConstraint = physenv->CreateFixedConstraint( pPhysics, pOtherPhysics, NULL, fixed );
	m_pConstraint->SetGameData( (void *)this );

	m_hConstrainedEntity = pEntity;

	// Need to compromise for lack of entity name
	if (GetEntityName() == NULL_STRING)
		SetName( AllocPooledString(UTIL_VarArgs("apc%i", entindex())) );

	// Not all entities have OnPhysGunPickup, but there's not much else we can do
	pEntity->KeyValue( "OnPhysGunPickup", UTIL_VarArgs("%s,UnconstrainEntity,,0,1", STRING(GetEntityName())) );

	pEntity->EmitSound( "PropAPC.AttachEntity" );

	m_onObjectAttached.FireOutput( pEntity, this );
}

void CPropDrivableAPC::UnconstrainEntity( bool bBroken, CBasePlayer *pPlayer )
{
	if (m_hConstrainedEntity)
	{
		PhysEnableEntityCollisions( this, m_hConstrainedEntity );

		// Dispatch NPC-specific interaction, returning true means it overrides this behavior
		if (m_hConstrainedEntity->DispatchInteraction( bBroken ? g_interactionAPCBreak : g_interactionAPCUnconstrain, this, pPlayer ))
			return;

		if (bBroken)
			EmitSound( "PropAPC.RipEntityOff" ); // Emit from the APC itself so that the driver can always hear it
		else
			m_hConstrainedEntity->EmitSound( "PropAPC.DetachEntity" );

		m_hConstrainedEntity = NULL;
		m_flConstrainCooldown = gpGlobals->curtime;
	}

	if ( m_pConstraint )
	{
		physenv->DestroyConstraint( m_pConstraint );
		m_pConstraint = NULL;
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::CreateServerVehicle( void )
{
	// Create our armed server vehicle
	m_pServerVehicle = new CDrivableAPCFourWheelServerVehicle();
	m_pServerVehicle->SetVehicle( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::Precache( void )
{
	PrecacheScriptSound( "Weapon_AR2.Single" );
	PrecacheScriptSound( "PropAPC.FireRocket" );
	PrecacheScriptSound( "combine.door_lock" );

	PrecacheScriptSound( "Airboat_headlight_on" );
	PrecacheScriptSound( "Airboat_headlight_off" );

	PrecacheModel( "sprites/redglow1.vmt" );

#ifdef EZ // Blixibon - APC spotlight
	UTIL_PrecacheOther( "point_spotlight" );

	PrecacheScriptSound( "PropAPC.AttachEntity" );
	PrecacheScriptSound( "PropAPC.DetachEntity" );
	PrecacheScriptSound( "PropAPC.RipEntityOff" );

	// Interactions
	if (g_interactionAPCConstrain == 0)
	{
		g_interactionAPCConstrain = CBaseCombatCharacter::GetInteractionID();
		g_interactionAPCUnconstrain = CBaseCombatCharacter::GetInteractionID();
		g_interactionAPCBreak = CBaseCombatCharacter::GetInteractionID();
	}
#endif

	BaseClass::Precache();
}

//------------------------------------------------
// Spawn
//------------------------------------------------
void CPropDrivableAPC::Spawn( void )
{
	// Setup vehicle as a real-wheels car.
	SetVehicleType( VEHICLE_TYPE_CAR_WHEELS );
	SetCollisionGroup( COLLISION_GROUP_VEHICLE );

#ifdef EZ2
	// Necessary to make vehicles visible to the player NPC component
	AddFlag( FL_OBJECT );
	SetViewOffset( Vector(0,0,96) );
#endif

	BaseClass::Spawn();
	m_flHandbrakeTime = gpGlobals->curtime + 0.1;
	m_bInitialHandbrake = false;

	m_flMinimumSpeedToEnterExit = LOCK_SPEED;


	// Initialize pose parameters
	SetPoseParameter( APC_GUN_YAW, 0 );
	SetPoseParameter( APC_GUN_PITCH, 0 );
	m_aimYaw = 0;
	m_aimPitch = 0;

	m_flViewLowered = 0;
	SetPoseParameter( APC_LOWER_VIEW, 0 );

	m_flTargetSelectTime = 0;
	m_hTarget = NULL;

	CreateAPCLaserDot();

	AddSolidFlags( FSOLID_NOT_STANDABLE );

//	m_bForcePlayerOut = false;
}

//-----------------------------------------------------------------------------
// Purpose: Create a laser
//-----------------------------------------------------------------------------
void CPropDrivableAPC::CreateAPCLaserDot( void )
{
	// Create a laser if we don't have one
#ifdef EZ
	if ( m_hLaserDot == NULL && GetDriver() )
	{
		// Rockets are now owned by the driver, so the laser dot must be owned by the driver as well
		m_hLaserDot = CreateLaserDot( GetAbsOrigin(), GetDriver(), false );
#else
	if ( m_hLaserDot == NULL )
	{
		m_hLaserDot = CreateLaserDot( GetAbsOrigin(), this, false );
#endif
	}
}

//-----------------------------------------------------------------------------
// Aims the secondary weapon at a target 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::AimSecondaryWeaponAt( CBaseEntity *pTarget )
{
	m_hRocketTarget = pTarget;

	// Update the rocket target
	CreateAPCLaserDot();

	if ( m_hRocketTarget )
	{
		m_hLaserDot->SetAbsOrigin( m_hRocketTarget->BodyTarget( WorldSpaceCenter(), false ) );
	}
	if (m_hLaserDot)
	{
		SetLaserDotTarget( m_hLaserDot, m_hRocketTarget );
		EnableLaserDot( m_hLaserDot, m_hRocketTarget != NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::UpdateOnRemove( void )
{
	if ( m_hLaserDot )
	{
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}
#ifdef EZ
	if ( m_Spotlight )
	{
		UTIL_Remove( m_Spotlight );
		m_Spotlight = NULL;
	}
#endif
	BaseClass::UpdateOnRemove();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CPropDrivableAPC::Activate()
{
	BaseClass::Activate();

	CBaseServerVehicle *pServerVehicle = dynamic_cast<CBaseServerVehicle *>(GetServerVehicle());
	if ( pServerVehicle )
	{
		if( pServerVehicle->GetPassenger() )
		{
			// If a jeep comes back from a save game with a driver, make sure the engine rumble starts up.
			pServerVehicle->StartEngineRumble();
		}
	}

	m_nRocketAttachment = LookupAttachment( "cannon_muzzle" );
	m_nMachineGunMuzzleAttachment = LookupAttachment( "Muzzle" );
	m_nMachineGunBaseAttachment = LookupAttachment( "gun_base" );
#ifdef EZ
	m_nSpotlightAttachment = LookupAttachment( "headlight" );
	m_nTopAttachAttachment = LookupAttachment( "top_attach" );

	// HACKHACK: Make the constraint invincible for 2 seconds
	if ( m_hConstrainedEntity && m_pConstraint )
	{
		PhysDisableEntityCollisions( this, m_hConstrainedEntity );

		physenv->DestroyConstraint( m_pConstraint );
		m_pConstraint = NULL;

		IPhysicsObject *pPhysics = VPhysicsGetObject();
		IPhysicsObject *pOtherPhysics = m_hConstrainedEntity->VPhysicsGetObject();

		// Constrain it to the vehicle
		constraint_fixedparams_t fixed;
		fixed.Defaults();
		fixed.InitWithCurrentObjectState( pPhysics, pOtherPhysics );
		fixed.constraint.Defaults();
		fixed.constraint.forceLimit	= 0;
		fixed.constraint.torqueLimit = 0;
		m_pConstraint = physenv->CreateFixedConstraint( pPhysics, pOtherPhysics, NULL, fixed );
		m_pConstraint->SetGameData( (void *)this );

		g_EventQueue.AddEvent( this, "_ResetConstraintToDefault", 2.0f, this, this );
	}

	// HACKHACK: Halve the vehicle's velocity.
	if (VPhysicsGetObject())
	{
		Vector velocity;
		AngularImpulse angularVelocity;
		VPhysicsGetObject()->GetVelocity( &velocity, &angularVelocity );
		velocity *= 0.5f;
		angularVelocity *= 0.5f;
		VPhysicsGetObject()->SetVelocityInstantaneous( &velocity, &angularVelocity );
	}

	// Reset the next think time on Activate()
	SetNextThink( gpGlobals->curtime );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr )
{
	CTakeDamageInfo info = inputInfo;

	if (info.GetAttacker() && info.GetAttacker()->MyNPCPointer() && info.GetAttacker()->IsAlive() &&
		CBaseCombatCharacter::GetDefaultRelationshipDispositionBetweenClasses( CLASS_PLAYER, info.GetAttacker()->Classify() ) != D_LI )
	{
		if (!m_hTarget || m_flTargetSelectTime < gpGlobals->curtime)
		{
#ifdef EZ
			if ( m_hTarget != NULL && m_hTarget->MyNPCPointer() != NULL )
			{
				m_hTarget->MyNPCPointer()->RemoveGlowEffect();
			}
			if ( apc_target_glow.GetBool() && info.GetAttacker()->MyNPCPointer() != NULL )
			{
				info.GetAttacker()->MyNPCPointer()->AddGlowEffect();
			}
#endif
			m_hTarget = info.GetAttacker();
			m_iTargetType = HLSS_SelectTargetType(m_hTarget);
		}
	}

	if ( ptr->hitbox != VEHICLE_HITBOX_DRIVER )
	{
		if ( inputInfo.GetDamageType() & DMG_BULLET )
		{
			info.ScaleDamage( 0.0001 );
		}
	}

	BaseClass::TraceAttack( info, vecDir, ptr, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPropDrivableAPC::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	//Do scaled up physics damage to the car
	CTakeDamageInfo info = inputInfo;
	/*info.ScaleDamage( 5 ); //25
	
	// HACKHACK: Scale up grenades until we get a better explosion/pressure damage system
	if ( inputInfo.GetDamageType() & DMG_BLAST )
	{
		info.SetDamageForce( inputInfo.GetDamageForce() * 10 );
	}
	VPhysicsTakeDamage( info );

	// reset the damage
	info.SetDamage( inputInfo.GetDamage() );*/

	// small amounts of shock damage disrupt the car, but aren't transferred to the player
	/*if ( info.GetDamageType() == DMG_SHOCK )
	{
		if ( info.GetDamage() <= 10 )
		{
			// take 10% damage and make the engine stall
			info.ScaleDamage( 0.1 );
			m_throttleDisableTime = gpGlobals->curtime + 2;
		}
	}*/

	//Check to do damage to driver
	if ( GetDriver() )
	{
		float flNormalDamageModifier = sk_apc_damage_normal.GetFloat();

#ifdef EZ
		// In Human Error, the view hide damage reduce percent modifier was half of the normal damage modifier divided by the maximum flViewLowered distance, 10
		// 0.15 / 2 / 10 = 0.0075
		// In Entropy : Zero, we want it to have half of the normal damage modifier divided by 10 
		float flViewHideDamageReduce = flNormalDamageModifier / 20.0f;
#else
		float flViewHideDamageReduce = (float)(m_flViewLowered * 0.0075);
#endif

		//Take no damage from physics damages
		if ( info.GetDamageType() & DMG_CRUSH )
			return 0;

#ifdef EZ
		// Take no damage from goo puddles
		if ( info.GetInflictor() && FClassnameIs(info.GetInflictor(), "zombie_goo_puddle") )
			return 0;
#endif

		//We want to get more damage from vortigaunts
		if ( info.GetDamageType() == DMG_SHOCK )
		{
#ifdef EZ
			info.ScaleDamage( sk_apc_damage_vort.GetFloat() );
#else
			info.ScaleDamage( 0.75f );
#endif
		}
		else if (!m_bExitAnimOn && !m_bEnterAnimOn)
		{
#ifdef EZ
			if (info.GetDamageType() & DMG_BLAST)
				info.ScaleDamage( MAX( sk_apc_damage_blast.GetFloat() + flViewHideDamageReduce, 0.0f) );
			else
				info.ScaleDamage( MAX( flNormalDamageModifier + flViewHideDamageReduce, 0.0f) );
#else
			if ( info.GetDamageType() & DMG_BLAST )
				info.ScaleDamage( 0.1 + flViewHideDamageReduce );
			else
				info.ScaleDamage( 0.15 + flViewHideDamageReduce );
#endif
		}

		// Take the damage (strip out the DMG_BLAST)
		info.SetDamageType( info.GetDamageType() & (~DMG_BLAST) );
		GetDriver()->TakeDamage( info );
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CPropDrivableAPC::BodyTarget( const Vector &posSrc, bool bNoisy )
{
	Vector	shotPos;
	matrix3x4_t	matrix;

	int eyeAttachmentIndex = LookupAttachment("vehicle_driver_eyes");
	GetAttachment( eyeAttachmentIndex, matrix );
	MatrixGetColumn( matrix, 3, shotPos );

	if ( bNoisy )
	{
		shotPos[0] += random->RandomFloat( -8.0f, 8.0f );
		shotPos[1] += random->RandomFloat( -8.0f, 8.0f );
		shotPos[2] += random->RandomFloat( -8.0f, 8.0f );
	}

	return shotPos;
}

//-----------------------------------------------------------------------------
// Purpose: Aim Gun at a target
//-----------------------------------------------------------------------------
void CPropDrivableAPC::AimGunAt( Vector *endPos, float flInterval )
{
	Vector	aimPos = *endPos;

	// See if the gun should be allowed to aim
	if ( IsOverturned() || m_bEngineLocked )
	{
		SetPoseParameter( APC_GUN_YAW, 0 );
		SetPoseParameter( APC_GUN_PITCH, 0 );
		return;

		// Make the gun go limp and look "down"
		Vector	v_forward, v_up;
		AngleVectors( GetLocalAngles(), NULL, &v_forward, &v_up );
		aimPos = WorldSpaceCenter() + ( v_forward * -32.0f ) - Vector( 0, 0, 128.0f );
	}

	matrix3x4_t gunMatrix;
	GetAttachment( LookupAttachment("gun_ref"), gunMatrix );

	// transform the enemy into gun space
	Vector localEnemyPosition;
	VectorITransform( aimPos, gunMatrix, localEnemyPosition );

	// do a look at in gun space (essentially a delta-lookat)
	QAngle localEnemyAngles;
	VectorAngles( localEnemyPosition, localEnemyAngles );
	
	// convert to +/- 180 degrees
	localEnemyAngles.x = UTIL_AngleDiff( localEnemyAngles.x, 0 );	
	localEnemyAngles.y = UTIL_AngleDiff( localEnemyAngles.y, 0 );

	float targetYaw = m_aimYaw + localEnemyAngles.y;
	float targetPitch = m_aimPitch + localEnemyAngles.x;

	//TERO: a silly trick to able different pose parameters
	float max_down_pitch = 4;
	float yaw = GetPoseParameter( APC_GUN_YAW );
	if (yaw < 0) 
	{
		yaw = -yaw;
		
		float yaw_fix = ((yaw - 90)/90);
		max_down_pitch += ((1 - (yaw_fix * yaw_fix)) * 37); //this sets the pitch down max to 45 if yaw is -90
	}

	max_down_pitch += (8 - (yaw / 22.5));	//from 0 to 8, this sets the original pitch down max of 4 to 12 if yaw is -180 or 180
	
	
	// Constrain our angles
	float newTargetYaw	=  targetYaw;
	float newTargetPitch = HLSS_APC_PITCH_FIX + clamp( targetPitch, -CANNON_MAX_UP_PITCH, max_down_pitch );

	//newTargetYaw += apc_zoomed_yaw_fix.GetFloat();
	//newTargetPitch += apc_zoomed_pitch_fix.GetFloat(); 

	//DevMsg("newTargetPitch: %f\n", newTargetPitch);

	// If the angles have been clamped, we're looking outside of our valid range
	if ( fabs(newTargetYaw-targetYaw) > 1e-4 || fabs(newTargetPitch-targetPitch) > 1e-4 )
	{
		m_bUnableToFire = true;
	}

	targetYaw = newTargetYaw;
	targetPitch = newTargetPitch;

	// Exponentially approach the target
	float yawSpeed = 8;
	float pitchSpeed = 8;

	m_aimYaw = UTIL_Approach( targetYaw, m_aimYaw, yawSpeed );
	m_aimPitch = UTIL_Approach( targetPitch, m_aimPitch, pitchSpeed );

	SetPoseParameter( APC_GUN_YAW, -m_aimYaw);
	SetPoseParameter( APC_GUN_PITCH, -m_aimPitch );

	InvalidateBoneCache();

	// read back to avoid drift when hitting limits
	// as long as the velocity is less than the delta between the limit and 180, this is fine.
	m_aimPitch = -GetPoseParameter( APC_GUN_PITCH );
	m_aimYaw = -GetPoseParameter( APC_GUN_YAW );

	// Now draw crosshair for actual aiming point
	Vector	vecMuzzle, vecMuzzleDir;
	QAngle	vecMuzzleAng;

	GetAttachment( "Muzzle", vecMuzzle, vecMuzzleAng );
	AngleVectors( vecMuzzleAng, &vecMuzzleDir );

	trace_t	tr;
	UTIL_TraceLine( vecMuzzle, vecMuzzle + (vecMuzzleDir * MAX_TRACE_LENGTH), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	// see if we hit something, if so, adjust endPos to hit location
	if ( m_hLaserDot && tr.fraction < 1.0 )
	{
		m_vecGunCrosshair = vecMuzzle + ( vecMuzzleDir * MAX_TRACE_LENGTH * tr.fraction );

		m_hLaserDot->SetAbsOrigin( tr.endpos );
		EnableLaserDot( m_hLaserDot, true );

		/*if ( tr.m_pEnt && tr.m_pEnt->IsNPC() && 
			 tr.m_pEnt->MyNPCPointer() && 
			 m_hPlayer && 
			 tr.m_pEnt->MyNPCPointer()->IsValidEnemy(m_hPlayer) )
		{	
				SetLaserDotTarget( m_hLaserDot, tr.m_pEnt );
				m_bHasTarget = true;
		} else
		{
			SetLaserDotTarget( m_hLaserDot, NULL );
		}*/

		if (tr.m_pEnt && tr.m_pEnt->MyNPCPointer() && CBaseCombatCharacter::GetDefaultRelationshipDispositionBetweenClasses( CLASS_PLAYER, tr.m_pEnt->Classify() ) == D_HT )
		{
			SetLaserDotTarget( m_hLaserDot, tr.m_pEnt );

			m_flLaserTargetTime = gpGlobals->curtime + 0.6f;

			if (tr.m_pEnt->IsAlive())
			{
#ifdef EZ
				if ( m_hTarget != NULL && m_hTarget->MyNPCPointer() != NULL )
				{
					m_hTarget->MyNPCPointer()->RemoveGlowEffect( );
				}
				if ( apc_target_glow.GetBool() && tr.m_pEnt->MyNPCPointer() != NULL )
				{
					tr.m_pEnt->MyNPCPointer()->AddGlowEffect();
				}
#endif
				m_hTarget = tr.m_pEnt;

				//TERO: don't change from this target for a moment
				m_flTargetSelectTime = gpGlobals->curtime + 0.6f;
				m_iTargetType = HLSS_SelectTargetType(m_hTarget);
			}
		}
		else if (m_flLaserTargetTime < gpGlobals->curtime)
		{
			SetLaserDotTarget( m_hLaserDot, NULL );
		}

		//m_hLaserDot->SetLaserPosition( tr.endpos, tr.plane.normal );
	}

	// Update the rocket target
	CreateAPCLaserDot();

	if (!DoesLaserDotHaveTarget( m_hLaserDot ))
	{
		m_hLaserDot->SetAbsOrigin( *endPos );
	}

	if (m_hLaserDot)
		EnableLaserDot( m_hLaserDot, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::Think(void)
{
	BaseClass::Think();

	CBasePlayer	*pPlayer = UTIL_GetLocalPlayer();

	if ( m_bEngineLocked )
	{
		m_bUnableToFire = true;
		
		if ( pPlayer != NULL )
		{
			pPlayer->m_Local.m_iHideHUD |= HIDEHUD_VEHICLE_CROSSHAIR;
		}
	}
	else
	{
		// Start this as false and update it again each frame
		m_bUnableToFire = false;

		if ( pPlayer != NULL )
		{
			pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_VEHICLE_CROSSHAIR;
		}
	}


	SetSimulationTime( gpGlobals->curtime );
	
	SetNextThink( gpGlobals->curtime );
	SetAnimatedEveryTick( true );

    if ( !m_bInitialHandbrake )	// after initial timer expires, set the handbrake
	{
		m_bInitialHandbrake = true;
		m_VehiclePhysics.SetHandbrake( true );
		m_VehiclePhysics.Think();
	}

	// Check overturned status.
	if ( !IsOverturned() )
	{
		m_flOverturnedTime = 0.0f;

#ifdef EZ
		if (!m_pConstraint && gpGlobals->curtime - m_flConstrainCooldown > 2.0f && m_nTopAttachAttachment != 0)
		{
			// Check if the player is holding something
			if (pPlayer && pPlayer->GetUseEntity())
			{
				CBaseEntity *pEntity = GetPlayerHeldEntity( pPlayer );
				if (!pEntity)
					PhysCannonGetHeldEntity( pPlayer->GetActiveWeapon() );

				if (pEntity)
				{
					Vector vecTop;
					GetAttachment( m_nTopAttachAttachment, vecTop );
					if (vecTop.DistToSqr(pEntity->GetAbsOrigin()) <= Square(32.0f))
					{
						ConstrainEntity( pEntity, pPlayer );
					}
				}
			}
		}
#endif
	}
	else
	{
#ifdef EZ
		if (m_flOverturnedTime == 0.0f )
		{
			// Fire an output
			m_onOverturned.FireOutput( this, this, 0 );

#ifdef EZ2
			IGameEvent *event = gameeventmanager->CreateEvent( "vehicle_overturned" );
			if (event && pPlayer->IsAlive())
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				event->SetInt( "vehicle", entindex() );
				gameeventmanager->FireEvent( event );
			}
#endif
		}
#endif

		m_flOverturnedTime += gpGlobals->frametime;
	}

	

	// Aim gun based on the player view direction.
	if ( m_hPlayer && !m_bExitAnimOn && !m_bEnterAnimOn )
	{
		if (m_flMachineGunReloadTime < gpGlobals->curtime && m_iMachineGunBurstLeft < MACHINE_GUN_BURST_SIZE)
		{
			m_iMachineGunBurstLeft++;
			m_flMachineGunReloadTime = gpGlobals->curtime + MACHINE_GUN_RELOAD_TIME;
		}

		if (m_flRocketReloadTime < gpGlobals->curtime && m_iRocketSalvoLeft < ROCKET_SALVO_SIZE)
		{
			m_iRocketSalvoLeft++;
			m_flRocketReloadTime = gpGlobals->curtime + random->RandomFloat( ROCKET_MIN_BURST_PAUSE_TIME + 1, ROCKET_MAX_BURST_PAUSE_TIME + 1 );
		}

		/*Vector vecEyeDir, vecEyePos;
		m_hPlayer->EyePositionAndVectors( &vecEyePos, NULL, NULL, NULL ); //second one used to be &vecEyeDir
		QAngle angEyeDir = m_hPlayer->EyeAngles();

		//TERO: attempt to fix the zoom aiming
		float angleFix = (m_hPlayer->GetDefaultFOV() / m_hPlayer->GetFOV()) - 1;
		angEyeDir.x -= angleFix; // * 0.5;


		AngleVectors(angEyeDir, &vecEyeDir);

		// Trace out from the player's eye point.
		Vector	vecEndPos = vecEyePos + ( vecEyeDir * MAX_TRACE_LENGTH );
		trace_t	trace;
		UTIL_TraceLine( vecEyePos, vecEndPos, MASK_SHOT, this, COLLISION_GROUP_NONE, &trace );*/

		Vector vecEndPos, vecEyePos, vecEyeDirection;
		m_hPlayer->EyePositionAndVectors( &vecEyePos, &vecEyeDirection, NULL, NULL );
		vecEndPos = vecEyePos + ( vecEyeDirection * MAX_TRACE_LENGTH );
		trace_t	trace;
		UTIL_TraceLine( vecEyePos, vecEndPos, MASK_SHOT, this, COLLISION_GROUP_NONE, &trace );
		vecEndPos = trace.endpos;


		// See if we hit something, if so, adjust end position to hit location.
		/*if ( trace.fraction < 1.0 )
		{
   			vecEndPos = vecEyePos + ( vecEyeDir * MAX_TRACE_LENGTH * trace.fraction );
		}*/

		if ( m_hTarget && !m_hTarget->IsAlive() )
		{
#ifdef EZ
			if ( m_hTarget->MyNPCPointer() != NULL )
			{
				m_hTarget->MyNPCPointer()->RemoveGlowEffect();
			}
#endif
			m_hTarget = NULL;
		}

		//NDebugOverlay::Box(vecEndPos, Vector(-1,-1,-1),Vector(1,1,1), 255, 0, 255, 0, 0.1);

		//m_vecLookCrosshair = vecEndPos;
		AimGunAt( &vecEndPos, 0.1f );
//		AimSecondaryWeaponAt( &vecEndPos, 0.1f);
	}

	StudioFrameAdvance();

	// If the enter or exit animation has finished, tell the server vehicle
	if ( IsSequenceFinished() && (m_bExitAnimOn || m_bEnterAnimOn) )
	{
		if ( m_bEnterAnimOn )
		{
			m_VehiclePhysics.ReleaseHandbrake();
			StartEngine();

			// HACKHACK: This forces the jeep to play a sound when it gets entered underwater
			if ( m_VehiclePhysics.IsEngineDisabled() )
			{
				CBaseServerVehicle *pServerVehicle = dynamic_cast<CBaseServerVehicle *>(GetServerVehicle());
				if ( pServerVehicle )
				{
					pServerVehicle->SoundStartDisabled();
				}
			}

			// The first few time we get into the jeep, print the jeep help
			if ( m_iNumberOfEntries < hud_apchint_numentries.GetInt() )
			{
				UTIL_HudHintText( m_hPlayer, "#HLSS_Hint_APC" );
				m_iNumberOfEntries++;
			}
		}

		// If we're exiting and have had the tau cannon removed, we don't want to reset the animation
		GetServerVehicle()->HandleEntryExitFinish( m_bExitAnimOn, !m_bExitAnimOn );
	}

	/*if (m_bForcePlayerOut)
	{
		GetServerVehicle()->HandlePassengerExit( m_hPlayer );
	}*/
}



//-----------------------------------------------------------------------------
// Purpose: If the player uses the jeep while at the back, he gets ammo from the crate instead
//-----------------------------------------------------------------------------
void CPropDrivableAPC::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	/*CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	
	if ( pPlayer == NULL)
		return;

	// Find out if the player's looking at our ammocrate hitbox 
	Vector vecForward;
	pPlayer->EyeVectors( &vecForward, NULL, NULL );

	trace_t tr;
	Vector vecStart = pPlayer->EyePosition();
	UTIL_TraceLine( vecStart, vecStart + vecForward * 1024, MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_HITBOX, pPlayer, COLLISION_GROUP_NONE, &tr );
	
	if ( tr.m_pEnt == this && tr.hitgroup == JEEP_AMMOCRATE_HITGROUP )
	{
		// Player's using the crate.
		// Fill up his SMG ammo.
		pPlayer->GiveAmmo( 300, "SMG1");
		
		if ( ( GetSequence() != LookupSequence( "ammo_open" ) ) && ( GetSequence() != LookupSequence( "ammo_close" ) ) )
		{
			// Open the crate
			m_flAnimTime = gpGlobals->curtime;
			m_flPlaybackRate = 0.0;
			SetCycle( 0 );
			ResetSequence( LookupSequence( "ammo_open" ) );
			
			CPASAttenuationFilter sndFilter( this, "PropJeep.AmmoOpen" );
			EmitSound( sndFilter, entindex(), "PropJeep.AmmoOpen" );
		}

		m_flAmmoCrateCloseTime = gpGlobals->curtime + JEEP_AMMO_CRATE_CLOSE_DELAY;
		return;
	}*/

	// Fall back and get in the vehicle instead
	BaseClass::Use( pActivator, pCaller, useType, value );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropDrivableAPC::CanExitVehicle( CBaseEntity *pEntity )
{
	return ( !m_bEnterAnimOn && !m_bExitAnimOn && !m_bLocked && (m_nSpeed <= g_apcexitspeed.GetFloat() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::DampenEyePosition( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles )
{
	// Get the frametime. (Check to see if enough time has passed to warrent dampening).
	float flFrameTime = gpGlobals->frametime;
	if ( flFrameTime < APC_FRAMETIME_MIN )
	{
		vecVehicleEyePos = m_vecLastEyePos;
		DampenUpMotion( vecVehicleEyePos, vecVehicleEyeAngles, 0.0f );
		return;
	}

	// Keep static the sideways motion.

	// Dampen forward/backward motion.
	DampenForwardMotion( vecVehicleEyePos, vecVehicleEyeAngles, flFrameTime );

	// Blend up/down motion.
	DampenUpMotion( vecVehicleEyePos, vecVehicleEyeAngles, flFrameTime );
}

//-----------------------------------------------------------------------------
// Use the controller as follows:
// speed += ( pCoefficientsOut[0] * ( targetPos - currentPos ) + pCoefficientsOut[1] * ( targetSpeed - currentSpeed ) ) * flDeltaTime;
//-----------------------------------------------------------------------------
void CPropDrivableAPC::ComputePDControllerCoefficients( float *pCoefficientsOut,
												  float flFrequency, float flDampening,
												  float flDeltaTime )
{
	float flKs = 9.0f * flFrequency * flFrequency;
	float flKd = 4.5f * flFrequency * flDampening;

	float flScale = 1.0f / ( 1.0f + flKd * flDeltaTime + flKs * flDeltaTime * flDeltaTime );

	pCoefficientsOut[0] = flKs * flScale;
	pCoefficientsOut[1] = ( flKd + flKs * flDeltaTime ) * flScale;
}
 
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPropDrivableAPC::DampenForwardMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime )
{
	// Get forward vector.
	Vector vecForward;
	AngleVectors( vecVehicleEyeAngles, &vecForward);

	// Simulate the eye position forward based on the data from last frame
	// (assumes no acceleration - it will get that from the "spring").
	Vector vecCurrentEyePos = m_vecLastEyePos + m_vecEyeSpeed * flFrameTime;

	// Calculate target speed based on the current vehicle eye position and the last vehicle eye position and frametime.
	Vector vecVehicleEyeSpeed = ( vecVehicleEyePos - m_vecLastEyeTarget ) / flFrameTime;
	m_vecLastEyeTarget = vecVehicleEyePos;	

	// Calculate the speed and position deltas.
	Vector vecDeltaSpeed = vecVehicleEyeSpeed - m_vecEyeSpeed;
	Vector vecDeltaPos = vecVehicleEyePos - vecCurrentEyePos;

	// Clamp.
	if ( vecDeltaPos.Length() > APC_DELTA_LENGTH_MAX )
	{
		float flSign = vecForward.Dot( vecVehicleEyeSpeed ) >= 0.0f ? -1.0f : 1.0f;
		vecVehicleEyePos += flSign * ( vecForward * APC_DELTA_LENGTH_MAX );
		m_vecLastEyePos = vecVehicleEyePos;
		m_vecEyeSpeed = vecVehicleEyeSpeed;
		return;
	}

	// Generate an updated (dampening) speed for use in next frames position extrapolation.
	float flCoefficients[2];
	ComputePDControllerCoefficients( flCoefficients, r_JeepViewDampenFreq.GetFloat(), r_JeepViewDampenDamp.GetFloat(), flFrameTime );
	m_vecEyeSpeed += ( ( flCoefficients[0] * vecDeltaPos + flCoefficients[1] * vecDeltaSpeed ) * flFrameTime );

	// Save off data for next frame.
	m_vecLastEyePos = vecCurrentEyePos;

	// Move eye forward/backward.
	Vector vecForwardOffset = vecForward * ( vecForward.Dot( vecDeltaPos ) );
	vecVehicleEyePos -= vecForwardOffset;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPropDrivableAPC::DampenUpMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime )
{
	// Get up vector.
	Vector vecUp;
	AngleVectors( vecVehicleEyeAngles, NULL, NULL, &vecUp );
	vecUp.z = clamp( vecUp.z, 0.0f, vecUp.z );
	vecVehicleEyePos.z += r_JeepViewZHeight.GetFloat() * vecUp.z;

	// NOTE: Should probably use some damped equation here.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	// If we are overturned and hit any key - leave the vehicle (IN_USE is already handled!).
	if ( m_flOverturnedTime > OVERTURNED_EXIT_WAITTIME )
	{
		if ( (ucmd->buttons & (IN_FORWARD|IN_BACK|IN_MOVELEFT|IN_MOVERIGHT|IN_SPEED|IN_JUMP|IN_ATTACK|IN_ATTACK2) ) && !m_bExitAnimOn )
		{
			// Can't exit yet? We're probably still moving. Swallow the keys.
			if ( !CanExitVehicle(player) )
				return;

			if ( !GetServerVehicle()->HandlePassengerExit( m_hPlayer ) && ( m_hPlayer != NULL ) )
			{
				m_hPlayer->PlayUseDenySound();
			}
			return;
		}
	}

	// If the throttle is disabled or we're upside-down, don't allow throttling (including turbo)
	CUserCmd tmp;
	if ( ( m_throttleDisableTime > gpGlobals->curtime ) || ( IsOverturned() ) )
	{
		m_bUnableToFire = true;
		
		tmp = (*ucmd);
		tmp.buttons &= ~(IN_FORWARD|IN_BACK|IN_SPEED);
		ucmd = &tmp;
	}

	//TERO: if we are pressing use, stop the vehicle first
	if ( ucmd->buttons & IN_USE ) //|| m_bForcePlayerOut )
	{
		ucmd->buttons |= IN_JUMP;
	}

	if (m_bCannotMove)
	{
		ucmd->forwardmove = 0;

		tmp = (*ucmd);
		tmp.buttons &= ~(IN_FORWARD|IN_BACK|IN_SPEED|IN_MOVELEFT|IN_MOVERIGHT);
		ucmd = &tmp;
	}
	
	BaseClass::SetupMove( player, ucmd, pHelper, move );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::HeadlightTurnOn( void )
{
	EmitSound( "Airboat_headlight_on" );
	m_bHeadlightIsOn = true;

#ifdef EZ // Blixibon - APC spotlight
	if (!m_Spotlight)
	{
		// CAI_Spotlight is designed for NPCs and CPointSpotlight has no header file,
		// so we have to create the entity anonymously and tweak it through keyvalues.
		Vector vecOrigin;
		QAngle angAngles;
		GetAttachment( m_nSpotlightAttachment, vecOrigin, angAngles );
		m_Spotlight = CreateNoSpawn( "point_spotlight", vecOrigin, angAngles, this );
		if (m_Spotlight)
		{
			m_Spotlight->AddSpawnFlags( 3 ); // Start on, no dynamic light
			m_Spotlight->KeyValue( "spotlightlength", "125" );
			m_Spotlight->KeyValue( "spotlightwidth", "50" );

			m_Spotlight->SetRenderColor(255, 255, 255, 128);

			DispatchSpawn(m_Spotlight);

			m_Spotlight->SetParent( this, m_nSpotlightAttachment );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::HeadlightTurnOff( void )
{
	EmitSound( "Airboat_headlight_off" );
	m_bHeadlightIsOn = false;

#ifdef EZ // Blixibon - APC spotlight
	if (m_Spotlight)
	{
		UTIL_Remove( m_Spotlight );
		m_Spotlight = NULL;
	}
#endif
}

float CPropDrivableAPC::GetUprightStrength( void )
{
	// Lesser if overturned
	if ( IsOverturned() )
		return 4.0f;
	
	return 2.0f; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::DriveVehicle( float flFrameTime, CUserCmd *ucmd, int iButtonsDown, int iButtonsReleased )
{
	int iButtons = ucmd->buttons;

	if ( ucmd->impulse == 100 )
	{
		if (HeadlightIsOn())
		{
			HeadlightTurnOff();
		}
        else 
		{
			HeadlightTurnOn();
		}
	}

	//int iDangerRadius = 200;

	if ( ucmd->forwardmove != 0.0f )
	{
		//iDangerRadius = 300;

		//Msg("Push V: %.2f, %.2f, %.2f\n", ucmd->forwardmove, carState->engineRPM, carState->speed );
		CBasePlayer *pPlayer = ToBasePlayer(GetDriver());

		if ( pPlayer && VPhysicsGetObject() )
		{
			bool bBreakProps = false;

			Vector velocity;
			VPhysicsGetObject()->GetVelocity( &velocity, NULL );
			float flAttackScale = velocity.Length() / 100.0f;

			//DevMsg("velocity %f, scale %f\n", velocity.Length(), flAttackScale);

			if (flAttackScale >= 2.0f)
			{
				m_bShouldAttackProps = true;
			}
			else if (flAttackScale < 1.0f)
			{
				if (m_bShouldAttackProps)
				{
					m_flNextPropAttackTime = gpGlobals->curtime;
				}

				m_bShouldAttackProps = false;
			}

			if (flAttackScale >= 1.0f)
			{
				m_flStopBreakTime = gpGlobals->curtime + 0.6f;
			}

			if ((m_flStopBreakTime > gpGlobals->curtime && m_flNextPropAttackTime < gpGlobals->curtime) )
			{
				m_flNextPropAttackTime = gpGlobals->curtime + 0.3f;

				bBreakProps = true;
			}

			KillBlockingEnemyNPCs( pPlayer, this, VPhysicsGetObject() );
			SolveBlockingProps( bBreakProps, this, VPhysicsGetObject() );

			if ( apc_hull_trace_attack.GetBool() ) 
			{
				QAngle angHeadlight;
				Vector vecHeadlight, vecDir;
				GetAttachment( "headlight", vecHeadlight, angHeadlight );
				AngleVectors( angHeadlight, &vecDir );

				vecDir = vecHeadlight + (vecDir);	//TERO: only stepping one

				NDebugOverlay::Box( vecHeadlight, -Vector(10,10,10), Vector(10,10,10), 255,0,0, 8, 0.1 );
				NDebugOverlay::Box( vecDir, -Vector(10,10,10), Vector(10,10,10), 0,255,0, 8, 0.1 );
			}
		}
		m_bIsMounted = false;

		//Disable LaserDot
		if (m_hLaserDot && apc_no_rpg_while_moving.GetBool() )
			EnableLaserDot( m_hLaserDot, false );
	} 
	else
	{
		m_bIsMounted = true;
		if (m_hLaserDot)
			EnableLaserDot( m_hLaserDot, true );
	}

	//CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin(), iDangerRadius, flFrameTime, NULL );

	if ( iButtons & IN_ATTACK )
	{
		FireMachineGun();
	}
	else if ( (m_bIsMounted || !apc_no_rpg_while_moving.GetBool()  ) && iButtons & IN_ATTACK2 )
	{
		FireRocket();
	}

	//TERO: lowering the view - ability

	m_flViewLowered = GetPoseParameter( APC_LOWER_VIEW );
	if ( iButtons & IN_DUCK )
	{
		m_flViewLowered = UTIL_Approach( -10.0, m_flViewLowered, 1.0);
	}
	else
	{
		m_flViewLowered = UTIL_Approach( 0, m_flViewLowered, 2.0);
	}
	//DevMsg("vehicle_drivable_apc, m_flViewLowered: %f\n", m_flViewLowered);
	SetPoseParameter( APC_LOWER_VIEW, m_flViewLowered );
	

	BaseClass::DriveVehicle( flFrameTime, ucmd, iButtonsDown, iButtonsReleased );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			*pMoveData - 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData )
{
	BaseClass::ProcessMovement( pPlayer, pMoveData );

	// Update the steering angles based on speed.
	UpdateSteeringAngle();

	// Create dangers sounds in front of the vehicle.
	CreateDangerSounds();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::UpdateSteeringAngle( void )
{
	float flMaxSpeed = m_VehiclePhysics.GetMaxSpeed();
	float flSpeed = m_VehiclePhysics.GetSpeed();

	float flRatio = 1.0f - ( flSpeed / flMaxSpeed );
	float flSteeringDegrees = APC_STEERING_FAST_ANGLE + ( ( APC_STEERING_SLOW_ANGLE - APC_STEERING_FAST_ANGLE ) * flRatio );
	flSteeringDegrees = clamp( flSteeringDegrees, APC_STEERING_FAST_ANGLE, APC_STEERING_SLOW_ANGLE );
	m_VehiclePhysics.SetSteeringDegrees( flSteeringDegrees );
}

//-----------------------------------------------------------------------------
// Purpose: Create danger sounds in front of the vehicle.
//-----------------------------------------------------------------------------
void CPropDrivableAPC::CreateDangerSounds( void )
{
//	QAngle dummy;
//	GetAttachment( "Muzzle", m_vecGunOrigin, dummy );

	if ( m_flDangerSoundTime > gpGlobals->curtime )
		return;

	QAngle vehicleAngles = GetLocalAngles();
	Vector vecStart = GetAbsOrigin();
	Vector vecDir, vecRight;

	GetVectors( &vecDir, &vecRight, NULL );

	const float soundDuration = 0.25;
	float speed = m_VehiclePhysics.GetHLSpeed();
	// Make danger sounds ahead of the jeep
	if ( fabs(speed) > 120 )
	{
		Vector	vecSpot;

		float steering = m_VehiclePhysics.GetSteering();
		if ( steering != 0 )
		{
			if ( speed > 0 )
			{
				vecDir += vecRight * steering * 0.5;
			}
			else
			{
				vecDir -= vecRight * steering * 0.5;
			}
			VectorNormalize(vecDir);
		}
		const float radius = speed * 1.1;
		// 0.3 seconds ahead of the jeep
		vecSpot = vecStart + vecDir * (speed * 0.3f);
#ifdef EZ
		// Blixibon - Corrected APC danger sounds
		CSoundEnt::InsertSound( SOUND_DANGER, vecSpot, radius, soundDuration, this, 0 );
		CSoundEnt::InsertSound( SOUND_PHYSICS_DANGER, vecSpot, radius, soundDuration, this, 1 );
#else
		CSoundEnt::InsertSound( SOUND_DANGER, vecSpot, radius, soundDuration, NULL, 0 );
		CSoundEnt::InsertSound( SOUND_PHYSICS_DANGER, vecSpot, radius, soundDuration, NULL, 1 );
#endif
		//NDebugOverlay::Box(vecSpot, Vector(-radius,-radius,-radius),Vector(radius,radius,radius), 255, 0, 255, 0, soundDuration);

#if 0
		trace_t	tr;
		// put sounds a bit to left and right but slightly closer to Jeep to make a "cone" of sound 
		// in front of it
		vecSpot = vecStart + vecDir * (speed * 0.5f) - vecRight * speed * 0.5;
		UTIL_TraceLine( vecStart, vecSpot, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		CSoundEnt::InsertSound( SOUND_DANGER, vecSpot, 400, soundDuration, NULL, 1 );

		vecSpot = vecStart + vecDir * (speed * 0.5f) + vecRight * speed * 0.5;
		UTIL_TraceLine( vecStart, vecSpot, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		CSoundEnt::InsertSound( SOUND_DANGER, vecSpot, 400, soundDuration, NULL, 2);
#endif
	}
	else
	{
		//TERO: added by me
#ifdef EZ
		// Blixibon - Corrected APC danger sounds
		CSoundEnt::InsertSound( SOUND_MOVE_AWAY, GetAbsOrigin(), 250, soundDuration, this );
#else
		CSoundEnt::InsertSound( SOUND_MOVE_AWAY, GetAbsOrigin(), 250, soundDuration, NULL );
#endif
	}

	m_flDangerSoundTime = gpGlobals->curtime + 0.1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::EnterVehicle( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	BaseClass::EnterVehicle( pPlayer );

	//TERO: 
	m_flViewLowered = 0;
	SetPoseParameter( APC_LOWER_VIEW, 0 );

	// Start looking for seagulls to land
	m_hLastPlayerInVehicle = m_hPlayer;
//	SetContextThink( NULL, 0, g_pApcThinkContext );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::ExitVehicle( int nRole )
{
#ifndef EZ // Blixibon - The headlight stays on in E:Z
	HeadlightTurnOff();
#endif

	//m_bForcePlayerOut = false;

#ifdef EZ
	// If we have a target, remove the glow effect on that target
	// and reset to NULL
	if (m_hTarget != NULL && m_hTarget->MyNPCPointer() != NULL)
	{
		m_hTarget->MyNPCPointer()->RemoveGlowEffect();
		m_hTarget = NULL;
	}

	// Remove our laser dot when we exit the vehicle
	if ( m_hLaserDot )
	{
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}
#endif

	BaseClass::ExitVehicle( nRole );

	m_flViewLowered = 0;
	SetPoseParameter( APC_LOWER_VIEW, 0 );

	// Remember when we last saw the player
	m_flPlayerExitedTime = gpGlobals->curtime;
	m_flLastSawPlayerAt = gpGlobals->curtime;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CPropDrivableAPC::GetTracerType( void ) 
{
	return "HelicopterTracer"; 
}


//-----------------------------------------------------------------------------
// Allows the shooter to change the impact effect of his bullets
//-----------------------------------------------------------------------------
void CPropDrivableAPC::DoImpactEffect( trace_t &tr, int nDamageType )
{
	UTIL_ImpactTrace( &tr, nDamageType, "HelicopterImpact" );
} 


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::DoMuzzleFlash( void )
{
	CEffectData data;
	data.m_nEntIndex = entindex();
	data.m_nAttachmentIndex = m_nMachineGunMuzzleAttachment;
	data.m_flScale = 1.0f;
	DispatchEffect( "AirboatMuzzleFlash", data ); //used to be ChopperMuzzleFlash

	BaseClass::DoMuzzleFlash();
}


/*Class_T CPropDrivableAPC::Classify() 
{ 
	if (GetDriver()) 
		return CLASS_METROPOLICE; 
	
	return CLASS_NONE; 
}*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::FireMachineGun( void )
{
	if ( m_flMachineGunTime > gpGlobals->curtime )
		return;

	// If we're still firing the salvo, fire quickly
	m_iMachineGunBurstLeft--;
	if ( m_iMachineGunBurstLeft > 0 )
	{
		m_flMachineGunTime = gpGlobals->curtime + MACHINE_GUN_BURST_TIME;
		m_flMachineGunReloadTime = gpGlobals->curtime + MACHINE_GUN_RELOAD_TIME;
	}
	else
	{
		// Reload the salvo
		m_iMachineGunBurstLeft = 0;
		m_flMachineGunTime = gpGlobals->curtime + MACHINE_GUN_RELOAD_TIME * 2;
	}

	Vector vecMachineGunShootPos;
	Vector vecMachineGunDir;
	GetAttachment( m_nMachineGunMuzzleAttachment, vecMachineGunShootPos, &vecMachineGunDir );
	
	// Fire the round
	//int	bulletType = GetAmmoDef()->Index("AR2");
	//FireBullets( 1, vecMachineGunShootPos, vecMachineGunDir, VECTOR_CONE_5DEGREES, MAX_TRACE_LENGTH, bulletType, 1 );

	FireBulletsInfo_t BulletInfo;
	BulletInfo.m_iShots = 1;
	BulletInfo.m_vecSrc = vecMachineGunShootPos;
	BulletInfo.m_vecDirShooting = vecMachineGunDir;
	BulletInfo.m_vecSpread = VECTOR_CONE_1DEGREES; //vec3_origin;
	BulletInfo.m_flDistance = MAX_TRACE_LENGTH;
	BulletInfo.m_iAmmoType = GetAmmoDef()->Index("AirboatGun");
	//BulletInfo.m_iTracerFreq = info.m_iTracerFreq;
	//BulletInfo.m_iDamage = info.m_iDamage;
	BulletInfo.m_pAttacker = UTIL_GetLocalPlayer();

	FireBullets( BulletInfo );

	Classify();

	DoMuzzleFlash();

	EmitSound( "Weapon_AR2.Single" );

#ifdef EZ
	if (GetDriver() && GetDriver()->IsPlayer())
	{
		CBasePlayer *pPlayer = ((CBasePlayer*)GetDriver());
		if (pPlayer->GetBonusChallenge() == EZ_CHALLENGE_BULLETS)
		{
			pPlayer->SetBonusProgress( pPlayer->GetBonusProgress() + BulletInfo.m_iShots );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::GetRocketShootPosition( Vector *pPosition )
{
	GetAttachment( m_nRocketAttachment, *pPosition );
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::OnRestore( void )
{
	IServerVehicle *pServerVehicle = GetServerVehicle();
	if ( pServerVehicle != NULL )
	{
		// Restore the passenger information we're holding on to
		pServerVehicle->RestorePassengerInfo();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropDrivableAPC::FireRocket( void )
{
	if ( m_flRocketTime > gpGlobals->curtime )
		return;

	// If we're still firing the salvo, fire quickly
	m_iRocketSalvoLeft--;
	if ( m_iRocketSalvoLeft > 0 )
	{
		m_flRocketTime = gpGlobals->curtime + ROCKET_DELAY_TIME;
		m_flRocketReloadTime = gpGlobals->curtime + random->RandomFloat( ROCKET_MIN_BURST_PAUSE_TIME, ROCKET_MAX_BURST_PAUSE_TIME );
	}
	else
	{
		// Reload the salvo
		m_iRocketSalvoLeft = 0;
		m_flRocketReloadTime = gpGlobals->curtime + random->RandomFloat( ROCKET_MIN_BURST_PAUSE_TIME, ROCKET_MAX_BURST_PAUSE_TIME );
		m_flRocketTime = m_flRocketReloadTime + 0.1f;
	}

	Vector vecRocketOrigin;
	GetRocketShootPosition(	&vecRocketOrigin );

	//static float s_pSide[] = { 0.966, 0.866, 0.5, -0.5, -0.866, -0.966 };
	/*static float s_pSide[] = { 0.5, 0.3, 0, -0.3, -0.5 };

	Vector forward, right;
	GetVectors( &forward, &right, NULL );

	float flZ = GetPoseParameter( APC_GUN_YAW );
	flZ = clamp( flZ * 0.02f, 0, 1); // 1/45 * 0.7 = 0.02 (approx)

	Vector vecDir;
	//CrossProduct( Vector( 0, 0, 1 ), forward, vecDir );
	vecDir = forward + (right * s_pSide[m_nRocketSide]);
	vecDir.z = 0.3f + flZ;
	if ( ++m_nRocketSide >= 5 )
	{
		m_nRocketSide = 0;
	}

	//TERO: if you want the Side thing to work uncomment the above and comment the following
	//Vector vecDir = forward;

	VectorNormalize( vecDir );*/


	//start new way of shooting rocket

	Vector	vecMuzzle, vecDir;
	QAngle	vecMuzzleAng;

	GetAttachment( "Muzzle", vecMuzzle, vecMuzzleAng );
	AngleVectors( vecMuzzleAng, &vecDir );

	//end new way of shooting rocket

	Vector vecVelocity;
	VectorMultiply( vecDir, ROCKET_SPEED, vecVelocity );

	QAngle angles;
	VectorAngles( vecDir, angles );

#ifdef EZ
	CAPCMissile *pRocket = (CAPCMissile *)CAPCMissile::Create( vecRocketOrigin, angles, vecVelocity, GetDriver() );
#else
	CAPCMissile *pRocket = (CAPCMissile *)CAPCMissile::Create( vecRocketOrigin, angles, vecVelocity, this );
#endif
	pRocket->IgniteDelay();
#ifdef EZ
	// If the APC has a target, disable guiding and aim at that target instead
	if ( m_hTarget != NULL )
	{
		pRocket->AimAtSpecificTarget( m_hTarget );
	}
	else
	{
		// If there is no target, disable guiding
		pRocket->DisableGuiding();
	}
#endif

	EmitSound( "PropAPC.FireRocket" );


/*	Vector	vecMuzzle, vecMuzzleDir;
	QAngle	vecMuzzleAng;

	GetAttachment( "Muzzle", vecMuzzle, vecMuzzleAng );
	AngleVectors( vecMuzzleAng, &vecMuzzleDir );*/


}

void CPropDrivableAPC::InputForcePlayerOut( inputdata_t &inputdata )
{
	if ( !m_hPlayer)
		return;

	//m_bForcePlayerOut = true;

	m_VehiclePhysics.SetHandbrake( true );

	GetServerVehicle()->HandlePassengerExit( m_hPlayer );
}



//========================================================================================================================================
// JEEP FOUR WHEEL PHYSICS VEHICLE SERVER VEHICLE
//========================================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDrivableAPCFourWheelServerVehicle::NPC_AimPrimaryWeapon( Vector vecTarget )
{
	((CPropDrivableAPC*)m_pVehicle)->AimGunAt( &vecTarget, 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecEyeExitEndpoint - 
// Output : int
//-----------------------------------------------------------------------------
int CDrivableAPCFourWheelServerVehicle::GetExitAnimToUse( Vector &vecEyeExitEndpoint, bool &bAllPointsBlocked )
{
	bAllPointsBlocked = false;

	if ( !m_bParsedAnimations )
	{
		// Load the entry/exit animations from the vehicle
		ParseEntryExitAnims();
		m_bParsedAnimations = true;
	}

//	CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating *>(m_pVehicle);

	return BaseClass::GetExitAnimToUse( vecEyeExitEndpoint, bAllPointsBlocked );
}
// Moved here from vehicle_manhack.cpp
int HLSS_SelectTargetType( CBaseEntity *pEntity )
{
	if (pEntity)
	{
		switch (pEntity->Classify())
		{
		case CLASS_CITIZEN_REBEL:
			return 1;
			break;
		//case CLASS_ALIENCONTROLLER:
		//case CLASS_ALIENGRUNT:
		case CLASS_VORTIGAUNT:
		//case CLASS_MANTARAY_TELEPORTER:
			return 2;
			break;
		case CLASS_HEADCRAB:
		case CLASS_ZOMBIE:
		case CLASS_ANTLION:
			return 3;
			break;
		//case CLASS_MILITARY_HACKED:
		//case CLASS_COMBINE_HACKED:
		//case CLASS_AIR_DEFENSE_HACKED:
		case CLASS_HACKED_ROLLERMINE:
			return 4;
			break;
		default:
			return 0;
			break;
		}
	}

	return 0;
}





// adds a collision solver for any small props that are stuck under the vehicle
static void SolveBlockingProps( bool bBreakProps, CPropDrivableAPC *pVehicleEntity, IPhysicsObject *pVehiclePhysics )
{
	//TERO: added
	Vector velocity;
	pVehiclePhysics->GetVelocity( &velocity, NULL );

	CUtlVector<CBaseEntity *> solveList;
	float vehicleMass = pVehiclePhysics->GetMass();
	Vector vehicleUp;
	pVehicleEntity->GetVectors( NULL, NULL, &vehicleUp );
	IPhysicsFrictionSnapshot *pSnapshot = pVehiclePhysics->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject(1);
		float otherMass = pOther->GetMass();
		CBaseEntity *pOtherEntity = static_cast<CBaseEntity *>(pOther->GetGameData());
		Assert(pOtherEntity);
		if ( pOtherEntity && pOtherEntity->GetMoveType() == MOVETYPE_VPHYSICS && pOther->IsMoveable() && (otherMass*2.0f) < vehicleMass )
		{
			Vector normal;
			pSnapshot->GetSurfaceNormal(normal);
			// this points down in the car's reference frame, then it's probably trapped under the car
			if ( DotProduct(normal, vehicleUp) < -0.9f )
			{
				Vector point, pointLocal;
				pSnapshot->GetContactPoint(point);
				VectorITransform( point, pVehicleEntity->EntityToWorldTransform(), pointLocal );
				Vector bottomPoint = physcollision->CollideGetExtent( pVehiclePhysics->GetCollide(), vec3_origin, vec3_angle, Vector(0,0,-1) );
				// make sure it's under the bottom of the car
				float bottomPlane = DotProduct(bottomPoint,vehicleUp)+8;	// 8 inches above bottom
				if ( DotProduct( pointLocal, vehicleUp ) <= bottomPlane )
				{
					//Msg("Solved %s\n", pOtherEntity->GetClassname());
					if ( solveList.Find(pOtherEntity) < 0 )
					{
						solveList.AddToTail(pOtherEntity);
					}
				}
			}
		}
		
		//TERO: added
		if ( bBreakProps && pOtherEntity && ( (pOtherEntity->GetMoveType() == MOVETYPE_VPHYSICS && ((otherMass*2.0f) < vehicleMass )) || 
							   pOtherEntity->GetMoveType() == MOVETYPE_PUSH))
		{
			//TERO: it's okay to have pVehicleEntity as the attacker instead of player, since dey arr props
			CTakeDamageInfo dmgInfo( pVehicleEntity, pVehicleEntity, velocity, pOtherEntity->WorldSpaceCenter(), 25.0f, DMG_CLUB );
			pOtherEntity->TakeDamage( dmgInfo );
		}

		//DevMsg("other mass %f, vehicleMass %f\n", otherMass, vehicleMass);


		pSnapshot->NextFrictionData();
	}

	pVehiclePhysics->DestroyFrictionSnapshot( pSnapshot );
	if ( solveList.Count() )
	{
		for ( int i = 0; i < solveList.Count(); i++ )
		{
			EntityPhysics_CreateSolver( pVehicleEntity, solveList[i], true, 6.0f );
		}
		pVehiclePhysics->RecheckContactPoints();
	}
}

static void SimpleCollisionResponse( Vector velocityIn, const Vector &normal, float coefficientOfRestitution, Vector *pVelocityOut )
{
	Vector Vn = DotProduct(velocityIn,normal) * normal;
	Vector Vt = velocityIn - Vn;
	*pVelocityOut = Vt - coefficientOfRestitution * Vn;
}

static void KillBlockingEnemyNPCs( CBasePlayer *pPlayer, CBaseEntity *pVehicleEntity, IPhysicsObject *pVehiclePhysics )
{
	Vector velocity;
	pVehiclePhysics->GetVelocity( &velocity, NULL );
	float vehicleMass = pVehiclePhysics->GetMass();

	// loop through the contacts and look for enemy NPCs that we're pushing on
	CUtlVector<CAI_BaseNPC *> npcList;
	CUtlVector<Vector> forceList;
	CUtlVector<Vector> contactList;
	IPhysicsFrictionSnapshot *pSnapshot = pVehiclePhysics->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject(1);
		float otherMass = pOther->GetMass();
		CBaseEntity *pOtherEntity = static_cast<CBaseEntity *>(pOther->GetGameData());
		CAI_BaseNPC *pNPC = pOtherEntity ? pOtherEntity->MyNPCPointer() : NULL;
		// Is this an enemy NPC with a small enough mass?
		if ( pNPC && pPlayer->IRelationType(pNPC) != D_LI && ((otherMass*2.0f) < vehicleMass) )
		{
			// accumulate the stress force for this NPC in the lsit
			float force = pSnapshot->GetNormalForce();
			Vector normal;
			pSnapshot->GetSurfaceNormal(normal);
			normal *= force;
			int index = npcList.Find(pNPC);
			if ( index < 0 )
			{
				vphysicsupdateai_t *pUpdate = NULL;
				if ( pNPC->VPhysicsGetObject() && pNPC->VPhysicsGetObject()->GetShadowController() && pNPC->GetMoveType() == MOVETYPE_STEP )
				{
					if ( pNPC->HasDataObjectType(VPHYSICSUPDATEAI) )
					{
						pUpdate = static_cast<vphysicsupdateai_t *>(pNPC->GetDataObject(VPHYSICSUPDATEAI));
						// kill this guy if I've been pushing him for more than half a second and I'm 
						// still pushing in his direction
						if ( (gpGlobals->curtime - pUpdate->startUpdateTime) > 0.5f && DotProduct(velocity,normal) > 0)
						{
							index = npcList.AddToTail(pNPC);
							forceList.AddToTail( normal );
							Vector pos;
							pSnapshot->GetContactPoint(pos);
							contactList.AddToTail(pos);
						}
					}
					else
					{
						pUpdate = static_cast<vphysicsupdateai_t *>(pNPC->CreateDataObject( VPHYSICSUPDATEAI ));
						pUpdate->startUpdateTime = gpGlobals->curtime;
					}
					// update based on vphysics for the next second
					// this allows the car to push the NPC
					pUpdate->stopUpdateTime = gpGlobals->curtime + 1.0f;
					float maxAngular;
					pNPC->VPhysicsGetObject()->GetShadowController()->GetMaxSpeed( &pUpdate->savedShadowControllerMaxSpeed, &maxAngular );
					pNPC->VPhysicsGetObject()->GetShadowController()->MaxSpeed( 1.0f, maxAngular );
				}
			}
			else
			{
				forceList[index] += normal;
			}
		}
		pSnapshot->NextFrictionData();
	}
	pVehiclePhysics->DestroyFrictionSnapshot( pSnapshot );
	// now iterate the list and check each cumulative force against the threshold
	if ( npcList.Count() )
	{
		for ( int i = npcList.Count(); --i >= 0; )
		{
			Vector damageForce;
			if (npcList[i]->VPhysicsGetObject() == NULL)
			{
				DevMsg( "APC tried to access NULL physics object\n" );
				continue;
			}
			npcList[i]->VPhysicsGetObject()->GetVelocity( &damageForce, NULL );
			Vector vel;
			pVehiclePhysics->GetVelocityAtPoint( contactList[i], &vel );
			vel *= 12; //TERO: added by because the APC is generally slow
			damageForce -= vel;
			Vector normal = forceList[i];
			VectorNormalize(normal);
			SimpleCollisionResponse( damageForce, normal, 1.0, &damageForce );
			damageForce += (normal * 10.0f);
			damageForce *= npcList[i]->VPhysicsGetObject()->GetMass();
			float len = damageForce.Length();
			damageForce.z += len*phys_upimpactforcescale.GetFloat();
			Vector vehicleForce = -damageForce;
			
			//TERO: next bit added by me
			//damageForce *= 10.0f;

			//DevMsg("KillBlockingEnemyNPCs %f\n", len );

			CTakeDamageInfo dmgInfo( pVehicleEntity, pPlayer, damageForce, contactList[i], 600.0f, DMG_CRUSH|DMG_VEHICLE );
			npcList[i]->TakeDamage( dmgInfo );
			pVehiclePhysics->ApplyForceOffset( vehicleForce, contactList[i] );

			if (npcList[i]->VPhysicsGetObject() == NULL)
			{
				DevMsg( "APC tried to access NULL physics object\n" );
				continue;
			}

			PhysCollisionSound( pVehicleEntity, npcList[i]->VPhysicsGetObject(), CHAN_BODY, pVehiclePhysics->GetMaterialIndex(), npcList[i]->VPhysicsGetObject()->GetMaterialIndex(), gpGlobals->frametime, 200.0f );
		}
	}
}


