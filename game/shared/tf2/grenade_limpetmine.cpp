//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_basecombatweapon.h"
#include "basegrenade_shared.h"
#include "weapon_limpetmine.h"
#include "engine/IEngineSound.h"
#include "grenade_limpetmine.h"
#include "tf_shareddefs.h"
#include "IEffects.h"
#include "player.h"
#include "basetfvehicle.h"

#define LIMPET_LIVE_TIME			0.5		// Time it takes before a limpet can be detonated after placement
#define LIMPET_LIFETIME				120		// After this time, limpets fizzle naturally
#define LIMPET_FIZZLE_DURATION		2.0f
#define LIMPET_MINS					Vector(-5, -5, 0)
#define LIMPET_MAXS					Vector( 5,  5, 10)

// Damage CVars
ConVar	weapon_limpetmine_grenade_damage( "weapon_limpetmine_grenade_damage","0", FCVAR_NONE, "Limpet Mine's grenade maximum damage" );
ConVar	weapon_limpetmine_grenade_radius( "weapon_limpetmine_grenade_radius","0", FCVAR_NONE, "Limpet Mine's grenade splash radius" );

// Global Savedata for friction modifier
BEGIN_DATADESC( CLimpetMine )
	// Function Pointers
	DEFINE_THINKFUNC( LiveThink ),
	DEFINE_ENTITYFUNC( StickyTouch ),
	DEFINE_THINKFUNC( LimpetThink ),
END_DATADESC()


IMPLEMENT_SERVERCLASS_ST(CLimpetMine, DT_LimpetMine)
	SendPropInt(SENDINFO(m_bLive), 1, SPROP_UNSIGNED ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( grenade_limpetmine, CLimpetMine );

//-----------------------------------------------------------------------------
// Static initializers: 
//-----------------------------------------------------------------------------
CLimpetMine*	CLimpetMine::allLimpets		= NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLimpetMine::CLimpetMine( void )
{
	UseClientSideAnimation();

	// ---------------------------------
	//  Add to linked list of limpets
	// ---------------------------------
	nextLimpet = CLimpetMine::allLimpets;
	CLimpetMine::allLimpets = this;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLimpetMine::~CLimpetMine( void )
{
	// --------------------------------------
	//  Remove from linked list of limpets
	// --------------------------------------
	CLimpetMine *pLimpet = CLimpetMine::allLimpets;
	if (pLimpet == this)
	{
		CLimpetMine::allLimpets = pLimpet->nextLimpet;
	}
	else
	{
		while (pLimpet)
		{
			if (pLimpet->nextLimpet == this)
			{
				pLimpet->nextLimpet = pLimpet->nextLimpet->nextLimpet;
				break;
			}
			pLimpet = pLimpet->nextLimpet;
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLimpetMine::Precache( void )
{
	PrecacheModel( "models/projectiles/grenade_limpet.mdl" );
	PrecacheScriptSound( "LimpetMine.Beep" );
	PrecacheScriptSound( "LimpetMine.Fizzle" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLimpetMine::Spawn( void )
{
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSolid( SOLID_BBOX );
	SetGravity( 1.0 );
	SetFriction( 1.25 );
	SetModel( "models/projectiles/grenade_limpet.mdl");
	UTIL_SetSize( this, LIMPET_MINS, LIMPET_MAXS );
	m_bLive = false;
	m_bFizzleInit = false;
	m_bEMPed = false;
	SetThink( LiveThink );
	SetNextThink( gpGlobals->curtime + LIMPET_LIVE_TIME );
	SetTouch( StickyTouch );

	// Causes these to collide with everything but NPCs and players
	SetCollisionGroup( TFCOLLISION_GROUP_GRENADE );

	AddFlag( FL_OBJECT ); 
	// Prevent sentry guns detecting these.
	AddFlag( FL_NOTARGET );

	// Set my damages to the cvar values
	SetDamage( weapon_limpetmine_grenade_damage.GetFloat() );
	SetDamageRadius( weapon_limpetmine_grenade_radius.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this limpet mine can be detonated yet
//-----------------------------------------------------------------------------
bool CLimpetMine::IsLive( void )
{
	return m_bLive;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLimpetMine::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Assert( pCaller );

	// USE_SET Means we're being asked to fizzle out and dielf
	if ( useType == USE_SET )
	{
		if ( !m_bFizzleInit )
		{
			// Set the defuse - fizzle think
			m_flFizzleDuration = gpGlobals->curtime + 0.3;
			SetThink( LimpetThink );
			SetNextThink( gpGlobals->curtime + 0.1f );
			m_bFizzleInit = true;
		}
	}
	else if ( IsLive() )
	{
		// Get the TF2 player that owns this limpet:
		CBaseTFPlayer *pPlayer = NULL;
		if ( m_hLauncher )
		{
			pPlayer = ToBaseTFPlayer( m_hLauncher->GetOwner() );
		}

		// Get the TF2 player that is calling this object, if any:
		CBaseTFPlayer *pCallerPlayer = NULL;
		if ( pCaller )
		{
			pCallerPlayer = ToBaseTFPlayer( pCaller );
		}

		// If the owning player is directly using the limpet, then pick it up:
		if( pPlayer && pCallerPlayer && pPlayer->IsSameClass( pCallerPlayer ) )
		{	
			if ( m_hLauncher )
			{
				pPlayer->GiveAmmo( 1, m_hLauncher->m_iPrimaryAmmoType );
				m_hLauncher->DecrementLimpets();
			}
			UTIL_Remove( this );
		}
		else if ( pActivator && !pActivator->InSameTeam( this ) )
		{
			// only the owning player can detonate his own limpets, so return if this isn't the owner.
			return;
		}

		// We're being detonated

		// If are EMPed then we cannot be detonated.
		else if ( !IsEMPed() )
		{
			// Beep and detonate soon afterwards
			EmitSound( "LimpetMine.Beep" );

			SetThink( Detonate );
			SetNextThink( gpGlobals->curtime + 0.5f );

			// Pretend I'm not live anymore so I don't get exploded again
			m_bLive = false;

			if ( m_hLauncher )
			{
				m_hLauncher->DecrementLimpets();
			}

		}


	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CLimpetMine::TakeEMPDamage( float duration )
{
	if ( !m_bFizzleInit )
	{
		// Set the defuse - fizzle think
		float flDuration = MIN( duration, LIMPET_FIZZLE_DURATION );
		m_flFizzleDuration = gpGlobals->curtime + ( flDuration - 1.0f );
		SetThink( LimpetThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
		m_bFizzleInit = true;
		m_bEMPed = true;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Create a limpet mine
//-----------------------------------------------------------------------------
CLimpetMine* CLimpetMine::Create( const Vector &vecOrigin, const Vector &vecForward, CBasePlayer *pOwner )
{
	CLimpetMine *pGrenade = (CLimpetMine*)CreateEntityByName("grenade_limpetmine");

	pGrenade->Teleport( &vecOrigin, NULL, NULL );
	pGrenade->Spawn();
	pGrenade->SetOwnerEntity( pOwner );
	pGrenade->SetThrower( pOwner );
	pGrenade->SetAbsVelocity( vecForward );
	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );

	return pGrenade;
}

//-----------------------------------------------------------------------------
// Purpose: Keep a pointer to the launcher (parent)
//-----------------------------------------------------------------------------
void CLimpetMine::SetLauncher( CWeaponLimpetmine *pLauncher )
{
	m_hLauncher = pLauncher;
}

//-----------------------------------------------------------------------------
// Purpose: Go Live
//-----------------------------------------------------------------------------
void CLimpetMine::LiveThink( void )
{
	m_bLive = true;

	// Remove myself after a while
	m_flFizzleDuration = gpGlobals->curtime + LIMPET_LIFETIME + 0.3;
	SetNextThink( gpGlobals->curtime + 0.1f );
	SetThink( LimpetThink );
}

//-----------------------------------------------------------------------------
// Purpose: Make the grenade stick to whatever it touches
//-----------------------------------------------------------------------------
void CLimpetMine::StickyTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	if ( !pOther->IsBSPModel() && !pOther->GetBaseAnimating() )
		return;

	BounceSound();
	m_bStuckToTarget = false;

	// Bounce off of shields
	if ( pOther->GetCollisionGroup() == TFCOLLISION_GROUP_SHIELD )
	{
		// Move away from the shield...
		// Fling it out a little extra along the plane normal
		Vector vecNewVelocity;
		Vector vecCenter;
		AngleVectors( pOther->GetAbsAngles(), &vecCenter );
		VectorMultiply( vecCenter, 400.0f, vecNewVelocity );
		SetAbsVelocity( vecNewVelocity );
		return;
	}

	// Only stick to non-moving entities
	if ( !pOther->GetBaseAnimating() )
		return;

	// Don't stick to team members
	if ( InSameTeam( pOther ) )
		return;

	// ROBIN: Removed stick to enemies for now
 	{
 		SetAbsVelocity( vec3_origin );
 		SetMoveType( MOVETYPE_NONE );
 		return;
	}

	m_bStuckToTarget = true;

	// Orient to stick to the wall I just hit
	trace_t tr;
	Vector vecPrev = GetLocalOrigin() - (GetAbsVelocity() * 0.1);
	UTIL_TraceLine( vecPrev, vecPrev + (GetAbsVelocity() * 2), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction != 1 )
	{
		// Orient the *up* axis to be along the plane normal
		Vector perp( 1, 0, 0 );
		Vector forward, right;
		CrossProduct( perp, tr.plane.normal, forward );
		if (forward.LengthSqr() < 0.1f)
		{
			perp.Init( 0, 1, 0 );
			CrossProduct( perp, tr.plane.normal, forward );
		}
		VectorNormalize( forward );
		CrossProduct( tr.plane.normal, forward, right );

		VMatrix orientation( forward, right, tr.plane.normal );

		QAngle angles;
		MatrixToAngles( orientation, angles );
		SetAbsAngles( angles );
	}

	SetAbsVelocity( vec3_origin );
	SetMoveType( MOVETYPE_NONE );

	// At this point, it shouldn't affect player movement
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	BounceSound();

	SetParent( pOther );
}


//-----------------------------------------------------------------------------
// Purpose: Once the limpet's active, it starts running this
//-----------------------------------------------------------------------------
void CLimpetMine::LimpetThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	// If I'm not ready to fizzle yet, make sure my parent's still there.
	if ( m_bStuckToTarget )
	{
		// Lost our parent?
		if ( !GetMoveParent() )
		{
			m_bStuckToTarget = false;
			// Fall to the ground
			SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
			SetTouch( StickyTouch );
		}
	}

	float flDeltaTime = m_flFizzleDuration - gpGlobals->curtime;

	// Not ready to fizzle yet?
	if ( flDeltaTime > 0.3 )
		return;

	// Start fizzling
	if ( flDeltaTime > 0.0f )
	{
		// Emit a fizzle sound
		EmitSound( "LimpetMine.Fizzle" );

		g_pEffects->Sparks( GetAbsOrigin() );

		// Smoke.
		UTIL_Smoke( GetAbsOrigin(), random->RandomInt( 1, 3), 10 );
	}
	else
	{
		// Done fizzling - no more sound.
		StopSound( "LimpetMine.Fizzle" );
		UTIL_Remove( this );

		// Remove this limpet mine from the launcher deployment count.
		if ( m_hLauncher )
		{
			m_hLauncher->DecrementLimpets();
		}

		return;
	}
}
