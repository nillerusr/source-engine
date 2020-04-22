//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A moving vehicle that is used as a battering ram
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_vehicle_tank.h"
#include "engine/IEngineSound.h"
#include "VGuiScreen.h"
#include "ammodef.h"
#include "in_buttons.h"
#include "vehicle_mortar_shared.h"
#include "movevars_shared.h"
#include "mortar_round.h"
#include "explode.h"
#include "tf_gamerules.h"
#include "shake.h"
#include "basetempentity.h"
#include "weapon_grenade_rocket.h"


#define TANK_MINS			Vector(-30, -50, -10)
#define TANK_MAXS			Vector( 30,  50, 55)
#define TANK_MODEL			"models/objects/vehicle_tank.mdl"

// N seconds between tank shots.
#define TANK_FIRE_INTERVAL	2

#define TANK_MAX_RANGE	1800


const char *g_pTurretThinkContextName = "TurretThinkContext";

ConVar vehicle_tank_damage( "vehicle_tank_damage","150", FCVAR_NONE, "Tank Rocket damage" );
ConVar vehicle_tank_range( "vehicle_tank_range","1000", FCVAR_NONE, "Tank Rocket range" );
ConVar vehicle_tank_radius( "vehicle_tank_radius","128", FCVAR_NONE, "Tank rocket explosive radius" );


// How fast the turret rotates to where the driver is facing.
#define TURRET_DEGREES_PER_SEC	30


IMPLEMENT_SERVERCLASS_ST( CVehicleTank, DT_VehicleTank )
	SendPropAngle( SENDINFO( m_flTurretYaw ), 11 ),
	SendPropAngle( SENDINFO( m_flTurretPitch ), 11 ),
END_SEND_TABLE()


LINK_ENTITY_TO_CLASS( vehicle_tank, CVehicleTank );
PRECACHE_REGISTER( vehicle_tank );


// CVars
ConVar	vehicle_tank_health( "vehicle_tank_health","800", FCVAR_NONE, "Tank health" );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVehicleTank::CVehicleTank()
{
	m_flClientYaw = 0;
	m_flClientPitch = 0;

	m_flTurretYaw = 0;
	m_flTurretPitch = 0;

	m_flNextFireTime = 0;

	UseClientSideAnimation();
}


CVehicleTank::~CVehicleTank()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleTank::Precache()
{
	PrecacheModel( TANK_MODEL );
	PrecacheVGuiScreen( "screen_vulnerable_point" );

	PrecacheScriptSound( "VehicleTank.FireSound" );


	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleTank::Spawn()
{
	SetModel( TANK_MODEL );
	
	// This size is used for placement only...
	SetSize( TANK_MINS, TANK_MAXS );
	m_takedamage = DAMAGE_YES;
	m_iHealth = vehicle_tank_health.GetInt();

	SetType( OBJ_VEHICLE_TANK );
	SetMaxPassengerCount( 3 );

	// This can go away when the tank is using a real tank model.
	SetActivity( ACT_DEPLOY );

	SetContextThink( TurretThink, gpGlobals->curtime, g_pTurretThinkContextName );

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CVehicleTank::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	//pPanelName = "screen_vehicle_siege_tower";
	pPanelName = "screen_vulnerable_point";
}

float MoveAngleTowards( float flAngle, float flTowards, float flRate )
{
	// Translate so flTowards is 0, then interpolate towards 0 or 360.
	flAngle -= flTowards; 

	// Move yaw to the client yaw using the shortest path.
	flAngle = anglemod( flAngle );
	if ( flAngle > 180 )
	{
		flAngle = MIN( 360, flAngle + flRate );
	}
	else
	{
		flAngle = MAX( 0, flAngle - flRate );
	}

	// Translate back.
	flAngle += flTowards;
	return flAngle;
}


void CVehicleTank::TurretThink()
{
	float flStartYaw = m_flTurretYaw;
	float flStartPitch = m_flTurretPitch;

	m_flTurretYaw = MoveAngleTowards( m_flTurretYaw, m_flClientYaw, gpGlobals->frametime * TURRET_DEGREES_PER_SEC );
	m_flTurretPitch = MoveAngleTowards( m_flTurretPitch, m_flClientPitch, gpGlobals->frametime * TURRET_DEGREES_PER_SEC );

	SetBoneController( 0, m_flTurretYaw );
	SetBoneController( 1, m_flTurretPitch );

	SetContextThink( TurretThink, gpGlobals->curtime, g_pTurretThinkContextName );
}


//-----------------------------------------------------------------------------
// Here's where we deal with weapons
//-----------------------------------------------------------------------------
void CVehicleTank::OnItemPostFrame( CBaseTFPlayer *pDriver )
{
	// I can't do anything if I'm not active
	if ( !ShouldBeActive() )
		return;

	// Make sure we're allowed to fire here
	if ( !IsReadyToDrive() )
		return;

	if ( GetPassengerRole( pDriver ) != VEHICLE_ROLE_DRIVER )
		return;

	if ( pDriver->m_nButtons & IN_ATTACK )
	{
		// Time to fire?
		if ( gpGlobals->curtime >= m_flNextFireTime )
		{
			Fire();
		}
	}
	else if ( pDriver->m_nButtons & (IN_ATTACK2 | IN_SPEED) )
	{
		//StartBoost();
		BaseClass::OnItemPostFrame( pDriver );
	}
}


void CVehicleTank::Fire()
{
	// NOTE: this code reverses the steps taken in C_VehicleTank::ClientThink to 
	// reconstruct the fire direction.

	// Build local angles.
	QAngle angles;
	angles[YAW] = m_flTurretYaw + 90;
	angles[PITCH] = -m_flTurretPitch;
	angles[ROLL] = 0;

	// Convert to a forward vector.
	Vector vForward, vLocalForward;
	AngleVectors( angles, &vLocalForward );

	// Move the vector to world space.
	matrix3x4_t tankToWorld;
	AngleMatrix( GetAbsAngles(), tankToWorld );
	VectorTransform( vLocalForward, tankToWorld, vForward );


	Vector vFireOrigin;
	QAngle dummy;
	GetAttachment( LookupAttachment( "muzzle" ), vFireOrigin, dummy );
	
	// Create the rocket.
	CWeaponGrenadeRocket *pRocket = CWeaponGrenadeRocket::Create( vFireOrigin, vForward, vehicle_tank_range.GetFloat(), this );
	if ( pRocket )
	{
		pRocket->SetRealOwner( GetDriverPlayer() );
		pRocket->SetDamage( vehicle_tank_damage.GetFloat() );
		pRocket->SetDamageRadius( vehicle_tank_radius.GetFloat() );

		// Apply a screen shake.
		UTIL_ScreenShake( vFireOrigin, 12, 60, 1.5, 512, SHAKE_START, true );

		// BOOM!
		CPASAttenuationFilter pasAttenuation( this, "VehicleTank.FireSound" );
		EmitSound( pasAttenuation, entindex(), "VehicleTank.FireSound" );
	}

	m_flNextFireTime = gpGlobals->curtime + TANK_FIRE_INTERVAL;
}


bool CVehicleTank::ClientCommand( CBaseTFPlayer *pPlayer, const char *pCmd, ICommandArguments *pArg )
{
	ResetDeteriorationTime();
	
	if ( FStrEq( pCmd, "TurretAngles" ) )
	{
		m_flClientYaw = atof( pArg->Argv(1) );
		m_flClientPitch = atof( pArg->Argv(2) );
		return true;
	}

	return BaseClass::ClientCommand( pPlayer, pCmd, pArg );
}


bool CVehicleTank::IsPassengerUsingStandardWeapons( int nRole )
{
	return ( nRole != VEHICLE_ROLE_DRIVER );
}


