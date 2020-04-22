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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHL1CombatWeapon::FallInit( void )
{
	SetModel( GetWorldModel() );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_TRIGGER );
	AddSolidFlags( FSOLID_NOT_SOLID );

	SetPickupTouch();
	
	SetThink( &CBaseHL1CombatWeapon::FallThink );

	SetNextThink( gpGlobals->curtime + 0.1f );

	// HACKHACK - On ground isn't always set, so look for ground underneath
	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector(0,0,2), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

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

	if ( GetFlags() & FL_ONGROUND )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the gun is waiting to respawn (if it's waiting, it is invisible!)
		if ( GetOwnerEntity() )
		{
			EmitSound( "BaseCombatWeapon.WeaponDrop" );
		}

		// lie flat
		QAngle ang = GetAbsAngles();
		ang.x = 0;
		ang.z = 0;
		SetAbsAngles( ang );

		Materialize(); 

		SetSize( Vector( -24, -24, 0 ), Vector( 24, 24, 16 ) );
	}
}

