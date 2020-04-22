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
#include "engine/IEngineSound.h"
#include "tf_shareddefs.h"
#include "IEffects.h"
#include "Sprite.h"
#include "grenade_antipersonnel.h"

// Damage CVars
ConVar	weapon_antipersonnel_grenade_damage( "weapon_antipersonnel_grenade_damage","0", FCVAR_NONE, "Anti-personnel grenade maximum damage" );
ConVar	weapon_antipersonnel_grenade_radius( "weapon_antipersonnel_grenade_radius","0", FCVAR_NONE, "Anti-personnel grenade splash radius" );

#if !defined( CLIENT_DLL )
// Server Only
ConVar weapon_antipersonnel_grenade_force( "weapon_antipersonnel_grenade_force","225.0", FCVAR_NONE, "Grenade explosive force modifier." );
#endif


IMPLEMENT_SERVERCLASS_ST(CGrenadeAntiPersonnel, DT_GrenadeAntiPersonnel)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( grenade_antipersonnel, CGrenadeAntiPersonnel );
PRECACHE_WEAPON_REGISTER(grenade_antipersonnel);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGrenadeAntiPersonnel::CGrenadeAntiPersonnel()
{
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGrenadeAntiPersonnel::Precache( void )
{
	BaseClass::Precache( );

	PrecacheModel( "models/weapons/w_grenade.mdl" );
	PrecacheModel( "sprites/redglow1.vmt" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeAntiPersonnel::Spawn( void )
{
	Precache();

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSolid( SOLID_BBOX );
	SetGravity( 1.0 );
	SetFriction( 0.9 );
	SetElasticity( 2.0f );
	SetModel( "models/weapons/w_grenade.mdl" );
	UTIL_SetSize(this, vec3_origin, vec3_origin);
	SetTouch( BounceTouch );
	SetCollisionGroup( TFCOLLISION_GROUP_GRENADE );

	m_flDetonateTime = gpGlobals->curtime + 3.0;
	SetThink( TumbleThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	// Set my damages to the cvar values
	SetDamage( weapon_antipersonnel_grenade_damage.GetFloat() );
	SetDamageRadius( weapon_antipersonnel_grenade_radius.GetFloat() );

	// Create a green light
	m_pLiveSprite = CSprite::SpriteCreate( "sprites/redglow1.vmt", GetLocalOrigin() + Vector(0,0,1), false );
	m_pLiveSprite->SetTransparency( kRenderGlow, 0, 255, 0, 128, kRenderFxNoDissipation );
	m_pLiveSprite->SetBrightness( 255 );
	m_pLiveSprite->SetScale( 1 );
	m_pLiveSprite->SetAttachment( this, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeAntiPersonnel::UpdateOnRemove( void )
{
	// Remove our live sprite
	if ( m_pLiveSprite )
	{
		UTIL_Remove( m_pLiveSprite );
		m_pLiveSprite = NULL;
	}

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Allow shield parry's
//-----------------------------------------------------------------------------
void CGrenadeAntiPersonnel::BounceTouch( CBaseEntity *pOther )
{
	// Don't blow up on trigger brushes
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	if ( pOther->GetCollisionGroup() == TFCOLLISION_GROUP_SHIELD )
	{
		// Move away from the shield...
		// Fling it out a little extra along the plane normal
		Vector vecCenter;
		AngleVectors( pOther->GetAbsAngles(), &vecCenter );

		// Bounce off the ground if it's on the ground...
		Vector vecNewVelocity = GetAbsVelocity();
		VectorMultiply( vecCenter, 400.0f, vecNewVelocity );
		if ((GetFlags() & FL_ONGROUND) && vecNewVelocity.z <= 100.0f)
		{
			vecNewVelocity.z = 100.0f;
		}
		SetAbsVelocity( vecNewVelocity );
	}

	// If we're set to explode on contact, and we just hit an enemy, go kaboom
	if ( m_bExplodeOnContact && !InSameTeam(pOther) && pOther->m_takedamage != DAMAGE_NO )
	{
		Detonate();
		return;
	}

	BaseClass::BounceTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Return the radius for the screenshake
//-----------------------------------------------------------------------------
float CGrenadeAntiPersonnel::GetShakeRadius( void )
{
	return (m_DmgRadius * 2);
}

//-----------------------------------------------------------------------------
// Purpose: Create a missile
//-----------------------------------------------------------------------------
void CGrenadeAntiPersonnel::Detonate( void )
{
	BaseClass::Detonate();

	// iterate on all entities in the vicinity and find vehicles
	CBaseEntity *pEntity = NULL;
	for ( CEntitySphereQuery sphere( GetAbsOrigin(), m_DmgRadius ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		// Check team.
		if ( pEntity->GetTeam() == GetTeam() )
			continue;

		if ( pEntity->GetServerVehicle() ) 
		{
			IPhysicsObject *pPhysObject = pEntity->VPhysicsGetObject();
			if ( pPhysObject )
			{
				// Rocket the vehicle in the direction of the incoming rocket.
				Vector vecForceDir = pEntity->GetAbsOrigin() - GetAbsOrigin();
				float flDistance = VectorNormalize( vecForceDir );

				if ( flDistance >= 0.0f && flDistance < m_DmgRadius )
				{
					vecForceDir.z = 1.0f;
					VectorNormalize( vecForceDir );
					
					float flForce = pPhysObject->GetMass();
					flForce += ( 4 * 500.0f );				// Wheels
					flForce *= weapon_antipersonnel_grenade_force.GetFloat();
					flForce *= ( 1.0f - ( flDistance / m_DmgRadius ) );
					
					vecForceDir *= flForce;
					
					pPhysObject->ApplyForceOffset( vecForceDir, GetAbsOrigin() );
				}
			}		
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create a missile
//-----------------------------------------------------------------------------
CGrenadeAntiPersonnel *CGrenadeAntiPersonnel::Create( const Vector &vecOrigin, const Vector &vecForward, CBasePlayer *pOwner )
{
	CGrenadeAntiPersonnel *pGrenade = (CGrenadeAntiPersonnel*)CreateEntityByName("grenade_antipersonnel");

	UTIL_SetOrigin( pGrenade, vecOrigin );
	pGrenade->Spawn();
	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	pGrenade->SetOwnerEntity( pOwner );
	pGrenade->SetThrower( pOwner );
	pGrenade->SetAbsVelocity( vecForward );
	QAngle angles;
	VectorAngles( vecForward, angles );
	pGrenade->SetLocalAngles( angles );
	pGrenade->SetLocalAngularVelocity( RandomAngle(-500,500) );

	return pGrenade;
}



