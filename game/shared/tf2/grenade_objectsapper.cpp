//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "player.h"
#include "tf_obj.h"
#include "basegrenade_shared.h"
#include "grenade_objectsapper.h"
#include "engine/IEngineSound.h"

// Damage CVars
static ConVar	weapon_objectsapper_damage( "weapon_objectsapper_damage","50", FCVAR_NONE, "Damage done, per second, by the object sapper." );

// Global Savedata for friction modifier
BEGIN_DATADESC( CGrenadeObjectSapper )

	// Function Pointers
	DEFINE_THINKFUNC( SapperThink ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CGrenadeObjectSapper, DT_GrenadeObjectSapper)
	SendPropInt(SENDINFO(m_bSapping), 1, SPROP_UNSIGNED ),
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( grenade_objectsapper, CGrenadeObjectSapper );
PRECACHE_WEAPON_REGISTER(grenade_objectsapper);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeObjectSapper::Spawn( void )
{
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetGravity( 0.0 );
	SetFriction( 1.0 );
	SetModel( "models/sapper.mdl");
	UTIL_SetSize(this, Vector( -8, -8, -8), Vector(8, 8, 8));
	SetCollisionGroup( TFCOLLISION_GROUP_WEAPON );
	m_takedamage = DAMAGE_NO;
	m_iHealth = 50.0;
	m_bSapping = false;
	
	// Set my damages to the cvar values
	SetDamage( weapon_objectsapper_damage.GetFloat() * 0.1 );
	SetDamageRadius( 0 );

	SetTouch( NULL );
	SetThink( SapperThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	m_bArmed = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGrenadeObjectSapper::Precache( void )
{
	PrecacheModel( "models/sapper.mdl" );

	PrecacheScriptSound( "GrenadeObjectSapper.Arming" );
	PrecacheScriptSound( "GrenadeObjectSapper.RemoveSapper" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeObjectSapper::PlayArmingSound( void )
{
	EmitSound( "GrenadeObjectSapper.Arming" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : armed - 
//-----------------------------------------------------------------------------
void CGrenadeObjectSapper::SetArmed( bool armed )
{
	bool ch = armed != m_bArmed;
	m_bArmed = armed;

	// Going armed
	if ( ch && m_bArmed )
	{
		PlayArmingSound();
	}

	if ( m_bArmed )
	{
		RemoveEffects( EF_NODRAW );
	}
	else
	{
		AddEffects( EF_NODRAW );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGrenadeObjectSapper::GetArmed( void ) const
{
	return m_bArmed;
}

//-----------------------------------------------------------------------------
// Purpose: Sap the health from the object I'm attached to
//-----------------------------------------------------------------------------
void CGrenadeObjectSapper::SapperThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	// Not armed yet?
	if ( !GetArmed() )
		return;

	// Remove myself if I'm armed, but don't have an object to sap
	if ( !m_hTargetObject )
	{
		UTIL_Remove( this );
		return;
	}

	m_bSapping = true;

	// Damage our target (add DMG_CRUSH to prevent physics damage)
	m_hTargetObject->TakeDamage( CTakeDamageInfo( this, GetThrower(), GetDamage(), GetDamageType() | DMG_CRUSH ) );
}

//-----------------------------------------------------------------------------
// Purpose: Set our target object
//-----------------------------------------------------------------------------
void CGrenadeObjectSapper::SetTargetObject( CBaseObject *pObject )
{
	// Remove myself from any object I'm on
	if ( m_hTargetObject != pObject )
	{
		if ( m_hTargetObject.Get() )
		{
			m_hTargetObject->RemoveSapper( this );
			SetParent( NULL );
		}

		m_hTargetObject = pObject;

		// Tell any object I've just been attached to
		if ( m_hTargetObject )
		{
			m_hTargetObject->AddSapper( this );
			SetParent( m_hTargetObject );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allow players to remove sappers from objects
//-----------------------------------------------------------------------------
void CGrenadeObjectSapper::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Only enemies remove the sapper
	if ( !InSameTeam( pActivator ) )
	{
		// Enemy is grabbing me
		EmitSound( "GrenadeObjectSapper.RemoveSapper" );
		SetTargetObject( NULL );
		UTIL_Remove( this );
	}
/*	
	ROBIN: Removed self-removal of sapper

	else
	{
		// Ignore everyone except my owner
		if ( pPlayer != m_hOwner )
			return;
		if ( pPlayer->GiveAmmo( 1, "Sappers") )
		{
			// Picked up, remove me
			SetTargetObject( NULL );
			UTIL_Remove( this );
		}
	}
*/
}

//-----------------------------------------------------------------------------
// Purpose: Create an object sapper grenade
//-----------------------------------------------------------------------------
CGrenadeObjectSapper *CGrenadeObjectSapper::Create( const Vector &vecOrigin, const Vector &vecForward, CBasePlayer *pOwner, CBaseObject *pObject )
{
	CGrenadeObjectSapper *pGrenade = (CGrenadeObjectSapper*)CreateEntityByName("grenade_objectsapper");

	UTIL_SetOrigin( pGrenade, vecOrigin );
	pGrenade->Spawn();
	pGrenade->SetThrower( pOwner );
	pGrenade->SetAbsVelocity( vec3_origin );
	QAngle angles;
	VectorAngles( vecForward, angles );
	angles.x -= 90;
	pGrenade->SetLocalAngles( angles );
	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	
	return pGrenade;
}
