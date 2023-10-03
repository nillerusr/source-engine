#include "cbase.h"
#include "asw_extinguisher_projectile.h"
#include "Sprite.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "EntityFlame.h"
#include "asw_fire.h"
#include "asw_marine.h"
#include "asw_weapon_flamer_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern ConVar sk_plr_dmg_asw_f;
extern ConVar sk_npc_dmg_asw_f;
extern ConVar asw_flamer_debug;

#define PELLET_MODEL "models/swarm/Shotgun/ShotgunPellet.mdl"

LINK_ENTITY_TO_CLASS( asw_extinguisher_projectile, CASW_Extinguisher_Projectile );

IMPLEMENT_SERVERCLASS_ST(CASW_Extinguisher_Projectile, DT_ASW_Extinguisher_Projectile)
	
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Extinguisher_Projectile )
	DEFINE_FUNCTION( ProjectileTouch ),
	DEFINE_FIELD( m_flDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_inSolid, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hFirer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flFreezeAmount, FIELD_FLOAT ),
END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_Extinguisher_Projectile::~CASW_Extinguisher_Projectile( void )
{
	m_flFreezeAmount = 0.001f;
}

void CASW_Extinguisher_Projectile::Spawn( void )
{
	Precache( );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );

	m_flDamage		= sk_plr_dmg_asw_f.GetFloat();
	m_takedamage	= DAMAGE_NO;
	

	SetSize( -Vector(4,4,4), Vector(4,4,4) );
	SetSolid( SOLID_BBOX );
	SetGravity( 0.05f );
	SetCollisionGroup( ASW_COLLISION_GROUP_EXTINGUISHER_PELLETS );

	SetTouch( &CASW_Extinguisher_Projectile::ProjectileTouch );

	// flamer projectile only lasts 1 second
	SetThink( &CASW_Extinguisher_Projectile::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 1.0f );
}

bool CASW_Extinguisher_Projectile::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );
	return true;
}

unsigned int CASW_Extinguisher_Projectile::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

void CASW_Extinguisher_Projectile::ProjectileTouch( CBaseEntity *pOther )
{
	// can't do this here, since CFire isn't declared in a .h   >_<
	//CFire* pFire = dynamic_cast<CFire*>(pOther);
	//if (pFire)
	//{
		//pFire->Extinguish(5);
		//TouchedEnvFire();
	//}
	if ( !g_pGameRules || !g_pGameRules->ShouldCollide( GetCollisionGroup(), pOther->GetCollisionGroup() ) )
		return;

	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;	

	if ( pOther->m_takedamage != DAMAGE_NO )
	{
		CASW_Marine *pMarine = CASW_Marine::AsMarine( pOther );
		if (pMarine && pMarine->m_bOnFire)
		{
			pMarine->Extinguish();
		}
		else
		{		
			CBaseAnimating* pAnim = dynamic_cast<CBaseAnimating*>(pOther);
			if (pAnim && pAnim->IsOnFire())
			{						
				CEntityFlame *pFireChild = dynamic_cast<CEntityFlame *>( pAnim->GetEffectEntity() );
				if ( pFireChild )
				{
					pAnim->SetEffectEntity( NULL );
					UTIL_Remove( pFireChild );
				}

				pAnim->Extinguish();
			}
		}

		//FireSystem_ExtinguishInRadius( GetAbsOrigin(), 100, 100 );

		// todo: keep going through normal entities?
		//if ( pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS )
			 //return;

		CAI_BaseNPC * RESTRICT pNPC = dynamic_cast<CAI_BaseNPC*>( pOther );
		if ( pNPC )
		{
			// Freeze faster the more frozen the object is
			//flFreeze = (0.5f * pNPC->GetFrozenAmount()) + flFreeze;

			if ( m_flFreezeAmount > 0 )
			{
				pNPC->Freeze( m_flFreezeAmount, this );
				if ( pNPC->GetFrozenAmount() >= 0.9f )
					return;
			}
		}
		SetAbsVelocity( Vector( 0, 0, 0 ) );

		SetTouch( NULL );
		SetThink( NULL );

		UTIL_Remove( this );
	}
	else
	{
		trace_t	tr;
		tr = BaseClass::GetTouchTrace();

		// See if we struck the world
		if ( pOther->GetMoveType() == MOVETYPE_NONE && !( tr.surface.flags & SURF_SKY ) )
		{
			Vector vel = GetAbsVelocity();
			if ( tr.startsolid )
			{
				if ( !m_inSolid )
				{
					// UNDONE: Do a better contact solution that uses relative velocity?
					vel *= -1.0f; // bounce backwards
					SetAbsVelocity(vel);
				}
				m_inSolid = true;
				return;
			}
			m_inSolid = false;
			if ( tr.DidHit() )
			{
				Vector dir = vel;
				VectorNormalize(dir);

				// reflect velocity around normal
				vel = -2.0f * tr.plane.normal * DotProduct(vel,tr.plane.normal) + vel;
				
				// absorb 80% in impact
				//vel *= GRENADE_COEFFICIENT_OF_RESTITUTION;
				SetAbsVelocity( vel );
			}
			return;
		}
		else
		{
			UTIL_Remove( this );
		}
	}
}

CASW_Extinguisher_Projectile* CASW_Extinguisher_Projectile::Extinguisher_Projectile_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner )
{
	CASW_Extinguisher_Projectile *pPellet = (CASW_Extinguisher_Projectile *)CreateEntityByName( "asw_extinguisher_projectile" );	
	pPellet->SetAbsAngles( angles );
	pPellet->Spawn();
	pPellet->SetOwnerEntity( pOwner );
	pPellet->m_hFirer = pOwner;
	UTIL_SetOrigin( pPellet, position );
	pPellet->SetAbsVelocity( velocity );


	if (asw_flamer_debug.GetBool())
		pPellet->m_debugOverlays |= OVERLAY_BBOX_BIT;


	return pPellet;
}

#define ASW_EXTINGUISHER_PROJECTILE_ACCN 250.0f
void CASW_Extinguisher_Projectile::PhysicsSimulate()
{
	// Make sure not to simulate this guy twice per frame
	if (m_nSimulationTick == gpGlobals->tickcount)
		return;

	// slow down the projectile's velocity	
	/*
	Vector dir = GetAbsVelocity();
	VectorNormalize(dir);		
	SetAbsVelocity(GetAbsVelocity() - (dir * gpGlobals->frametime * ASW_EXTINGUISHER_PROJECTILE_ACCN));
	dir = GetAbsVelocity();
	*/
	SetAbsVelocity( GetAbsVelocity() * 
		( 1 - gpGlobals->frametime * ASW_EXTINGUISHER_PROJECTILE_ACCN / CASW_Weapon_Flamer::EXTINGUISHER_PROJECTILE_AIR_VELOCITY )  );

	if (asw_flamer_debug.GetBool())
	{
		NDebugOverlay::Box( GetAbsOrigin(), -Vector(4,4,4), Vector(4,4,4), 255, 0, 0, 255, 0.2f );

		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + GetAbsVelocity() ,
			255, 255, 0, true, 0.3f );
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + GetAbsVelocity() * gpGlobals->interval_per_tick,
			128, 255, 0, true, 0.3f );
	}
	
	BaseClass::PhysicsSimulate();
}

// need to force send as it has no model
int CASW_Extinguisher_Projectile::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

void CASW_Extinguisher_Projectile::TouchedEnvFire()
{
	SetThink( &CASW_Extinguisher_Projectile::SUB_Remove );
	SetNextThink( gpGlobals->curtime );
}

CBaseEntity* CASW_Extinguisher_Projectile::GetFirer()
{
	return m_hFirer.Get();
}	