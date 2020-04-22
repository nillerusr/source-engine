//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#include "cbase.h"
#include "hl1_basecombatweapon_shared.h"
#include "effect_dispatch_data.h"
#include "te_effect_dispatch.h"


BEGIN_DATADESC( CBaseHL1CombatWeapon )
	DEFINE_THINKFUNC( FallThink ),
END_DATADESC();


void CBaseHL1CombatWeapon::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "BaseCombatWeapon.WeaponDrop" );
}

bool CBaseHL1CombatWeapon::CreateVPhysics( void )
{
	VPhysicsInitNormal( SOLID_BBOX, GetSolidFlags() | FSOLID_TRIGGER, false );
	IPhysicsObject *pPhysObj = VPhysicsGetObject();
        if ( pPhysObj )
	{
		pPhysObj->SetMass( 30 );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHL1CombatWeapon::FallInit( void )
{
	SetModel( GetWorldModel() );

	if( !CreateVPhysics() )
	{
		SetSolid( SOLID_BBOX );
		SetMoveType( MOVETYPE_FLYGRAVITY );
                SetSolid( SOLID_BBOX );
                AddSolidFlags( FSOLID_TRIGGER );
	}

	SetPickupTouch();

	SetThink( &CBaseHL1CombatWeapon::FallThink );

	SetNextThink( gpGlobals->curtime + 0.1f );

	// HACKHACK - On ground isn't always set, so look for ground underneath
	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector(0,0,256), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

	SetAbsOrigin( tr.endpos );

	if ( tr.fraction < 1.0 )
	{
		SetGroundEntity( tr.m_pEnt );
	}

	SetViewOffset( Vector(0,0,8) );
}


//-----------------------------------------------------------------------------
// Purpose: Items that have just spawned run this think to catch them when 
//			they hit the ground. Once we're sure that the object is grounded, 
//			we change its solid type to trigger and set it in a large box that 
//			helps the player get it.
//-----------------------------------------------------------------------------
void CBaseHL1CombatWeapon::FallThink ( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	bool shouldMaterialize = false;
	IPhysicsObject *pPhysics = VPhysicsGetObject();
	if ( pPhysics )
	{
		shouldMaterialize = pPhysics->IsAsleep();
	}
	else
	{
		shouldMaterialize = (GetFlags() & FL_ONGROUND) ? true : false;
		if( shouldMaterialize )
			SetSize( Vector( -24, -24, 0 ), Vector( 24, 24, 16 ) );
	}

	if ( shouldMaterialize )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the gun is waiting to respawn (if it's waiting, it is invisible!)
		if ( GetOwnerEntity() )
		{
			EmitSound( "BaseCombatWeapon.WeaponDrop" );
		}

		// lie flat
		Materialize();

	}
}

