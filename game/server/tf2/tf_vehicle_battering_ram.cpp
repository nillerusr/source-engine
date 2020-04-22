//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A moving vehicle that is used as a battering ram
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_vehicle_battering_ram.h"
#include "engine/IEngineSound.h"
#include "VGuiScreen.h"
#include "ammodef.h"
#include "in_buttons.h"
#include "shake.h"

#define BATTERING_RAM_MINS			Vector(-30, -50, -10)
#define BATTERING_RAM_MAXS			Vector( 30,  50, 55)
#define BATTERING_RAM_MODEL			"models/objects/vehicle_battering_ram.mdl"

IMPLEMENT_SERVERCLASS_ST(CVehicleBatteringRam, DT_VehicleBatteringRam)
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(vehicle_battering_ram, CVehicleBatteringRam);
PRECACHE_REGISTER(vehicle_battering_ram);

// CVars
ConVar	vehicle_battering_ram_health( "vehicle_battering_ram_health","800", FCVAR_NONE, "Battering ram health" );
ConVar	vehicle_battering_ram_damage( "vehicle_battering_ram_damage","300", FCVAR_NONE, "Battering ram damage" );
ConVar	vehicle_battering_ram_damage_boostmod( "vehicle_battering_ram_damage_boostmod", "1.5", FCVAR_NONE, "Battering ram boosted damage modifier" );
ConVar	vehicle_battering_ram_mindamagevel( "vehicle_battering_ram_mindamagevel","100", FCVAR_NONE, "Battering ram velocity for min damage" );
ConVar	vehicle_battering_ram_maxdamagevel( "vehicle_battering_ram_maxdamagevel","260", FCVAR_NONE, "Battering ram velocity for max damage" );
ConVar  vehicle_battering_ram_impact_time( "vehicle_battering_ram_impact_time", "1.0", FCVAR_NONE, "Battering ram impact wait time." );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CVehicleBatteringRam::CVehicleBatteringRam()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleBatteringRam::Precache()
{
	PrecacheModel( BATTERING_RAM_MODEL );

	PrecacheVGuiScreen( "screen_vehicle_battering_ram" );
	PrecacheVGuiScreen( "screen_vulnerable_point");

	PrecacheScriptSound( "VehicleBatteringRam.BashSound" );

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleBatteringRam::Spawn()
{
	SetModel( BATTERING_RAM_MODEL );
	
	// This size is used for placement only...
	UTIL_SetSize(this, BATTERING_RAM_MINS, BATTERING_RAM_MAXS);
	m_takedamage = DAMAGE_YES;
	m_iHealth = vehicle_battering_ram_health.GetInt();
	m_nBarrelAttachment = LookupAttachment( "barrel" );

	SetType( OBJ_BATTERING_RAM );
	SetMaxPassengerCount( 4 );
//	SetTouch( BashTouch );
	
	m_flNextBashTime = 0.0f;

	BaseClass::Spawn();
}


//-----------------------------------------------------------------------------
// Does the player use his normal weapons while in this mode?
//-----------------------------------------------------------------------------
bool CVehicleBatteringRam::IsPassengerUsingStandardWeapons( int nRole )
{
	return (nRole > 1);
}


//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CVehicleBatteringRam::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_vulnerable_point";
}


//-----------------------------------------------------------------------------
// Purpose: Ram!!!
//-----------------------------------------------------------------------------
void CVehicleBatteringRam::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	int otherIndex = !index;
	CBaseEntity *pEntity = pEvent->pEntities[otherIndex];

    // We only damage objects...
	// And only if we're travelling fast enough...
	if ( !pEntity->IsSolid( ) )
		return;

	// Ignore shields..
	if ( pEntity->GetCollisionGroup() == TFCOLLISION_GROUP_SHIELD )
		return;

	// Ignore anything that's not an object
	if ( pEntity->Classify() != CLASS_MILITARY && !pEntity->IsPlayer() )
		return;

	// Ignore teammates
	if ( InSameTeam( pEntity ))
		return;

	// Ignore invulnerable stuff
	if ( pEntity->m_takedamage == DAMAGE_NO )
		return;

	// See if we can damage again? (Time-based)
	if ( m_flNextBashTime > gpGlobals->curtime )
		return;

	// Use the attachment point to make sure we damage stuff that hit our front
	// FIXME: Should we be using hitboxes here?
	// And damage them all
    // We only damage objects...
 	QAngle vecAng;
	Vector vecSrc, vecAim;
	GetAttachment( m_nBarrelAttachment, vecSrc, vecAng );

	// Check the forward direction
	Vector forward;
	AngleVectors( vecAng, &forward );

	// Did we hit it in front of the vehicle?
	Vector vecDelta;
	VectorSubtract( pEntity->WorldSpaceCenter(), vecSrc, vecDelta );
	if ( ( DotProduct( vecDelta, forward ) <= 0 ) || pEntity->IsPlayer() )
	{
		BaseClass::VPhysicsCollision( index, pEvent );
		return;
	}

	// Call our parents base class.
	BaseClass::BaseClass::VPhysicsCollision( index, pEvent );

	// Do damage based on velocity (and only if we were heading forward)
	Vector vecVelocity = pEvent->preVelocity[index];
	if (DotProduct( vecVelocity, forward ) <= 0)
		return;

	CTakeDamageInfo info;
	info.SetInflictor( this );
	info.SetAttacker( GetDriverPlayer() );
	info.SetDamageType( DMG_CLUB );

	float flMaxDamage = vehicle_battering_ram_damage.GetFloat();
	float flMaxDamageVel = vehicle_battering_ram_maxdamagevel.GetFloat();
	float flMinDamageVel = vehicle_battering_ram_mindamagevel.GetFloat();

	float flVel = vecVelocity.Length();
	if (flVel < flMinDamageVel)
		return;

	// FIXME: Play a sound here...
	EmitSound( "VehicleBatteringRam.BashSound" );

	// Apply the damage
	float flDamageFactor = flMaxDamage;
	if ( IsBoosting() )
	{
		flDamageFactor *= vehicle_battering_ram_damage_boostmod.GetFloat();
	}

	if ((flMaxDamageVel > flMinDamageVel) && (flVel < flMaxDamageVel))
	{
		// Use less damage when we're not moving fast enough
		float flVelocityFactor = (flVel - flMinDamageVel) / (flMaxDamageVel - flMinDamageVel);
		flVelocityFactor *= flVelocityFactor;
		flDamageFactor *= flVelocityFactor;
	}

	UTIL_ScreenShakeObject( pEntity, vecSrc, 45.0f * flDamageFactor / flMaxDamage, 30.0f, 0.5f, 250.0f, SHAKE_START );

	info.SetDamage( flDamageFactor );

	Vector damagePos;
	pEvent->pInternalData->GetContactPoint( damagePos );
	Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass();
	info.SetDamageForce( damageForce );
	info.SetDamagePosition( damagePos );

	PhysCallbackDamage( pEntity, info, *pEvent, index );

	// Set next time ram time
	m_flNextBashTime = gpGlobals->curtime + vehicle_battering_ram_impact_time.GetFloat();
}

//-----------------------------------------------------------------------------
// Here's where we deal with weapons, ladders, etc.
//-----------------------------------------------------------------------------
void CVehicleBatteringRam::OnItemPostFrame( CBaseTFPlayer *pDriver )
{
	// I can't do anything if I'm not active
	if ( !ShouldBeActive() )
		return;

	if ( GetPassengerRole( pDriver ) != VEHICLE_ROLE_DRIVER )
		return;

	if ( pDriver->m_nButtons & (IN_ATTACK2 | IN_SPEED) )
	{
		StartBoost();
	}

	BaseClass::OnItemPostFrame( pDriver );
}
