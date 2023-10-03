
#include "cbase.h"
#include "asw_flamer_projectile.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "asw_alien.h"
#include "asw_util_shared.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_equipment_list.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern ConVar sk_plr_dmg_asw_f;
extern ConVar sk_npc_dmg_asw_f;

#define PELLET_MODEL "models/swarm/Shotgun/ShotgunPellet.mdl"

ConVar asw_flamer_force("asw_flamer_force", "0.7f", FCVAR_CHEAT, "Force imparted by the flamer projectiles");
ConVar asw_flamer_size("asw_flamer_size", "40", FCVAR_CHEAT, "Radius at which flamer projectiles set aliens on fire");
ConVar asw_flamer_debug("asw_flamer_debug", "0", FCVAR_CHEAT, "Visualize flamer projectile collision");

#define ASW_FLAMER_HULL_MINS Vector(-asw_flamer_size.GetFloat(), -asw_flamer_size.GetFloat(), -asw_flamer_size.GetFloat() * 2.0f)
#define ASW_FLAMER_HULL_MAXS Vector(asw_flamer_size.GetFloat(), asw_flamer_size.GetFloat(), asw_flamer_size.GetFloat() * 2.0f)

LINK_ENTITY_TO_CLASS( asw_flamer_projectile, CASW_Flamer_Projectile );

IMPLEMENT_SERVERCLASS_ST(CASW_Flamer_Projectile, DT_ASW_Flamer_Projectile)
	
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Flamer_Projectile )
	DEFINE_FUNCTION( ProjectileTouch ),
	DEFINE_FUNCTION( CollideThink ),

	// Fields
	DEFINE_FIELD( m_pMainGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pGlowTrail, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_bHurtIgnited, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_inSolid, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fDieTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecOldPos, FIELD_VECTOR ),
END_DATADESC()

CASW_Flamer_Projectile::CASW_Flamer_Projectile()
{
	m_pLastHitEnt = NULL;
}

CASW_Flamer_Projectile::~CASW_Flamer_Projectile( void )
{
	KillEffects();
}

void CASW_Flamer_Projectile::Spawn( void )
{
	Precache( );

	//SetModel( PELLET_MODEL );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );

	m_flDamage		= sk_plr_dmg_asw_f.GetFloat();
	m_takedamage	= DAMAGE_NO;

	SetSize( -Vector(1,1,1), Vector(1,1,1) );
	SetSolid( SOLID_BBOX );
	SetGravity( 0.05f );
	SetCollisionGroup( ASW_COLLISION_GROUP_FLAMER_PELLETS );	// ASW_COLLISION_GROUP_SHOTGUN_PELLET

	SetTouch( &CASW_Flamer_Projectile::ProjectileTouch );

	CreateEffects();
	m_vecOldPos = vec3_origin;

	m_fDieTime = gpGlobals->curtime + 1.0f;		// will need to change size scale algos below if this changes
	SetThink( &CASW_Flamer_Projectile::CollideThink );
	SetNextThink( gpGlobals->curtime );

	// flamer projectile only lasts 1 second
	//SetThink( &CASW_Flamer_Projectile::SUB_Remove );
	//SetNextThink( gpGlobals->curtime + 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Flamer_Projectile::OnRestore( void )
{
	CreateEffects();

	BaseClass::OnRestore();
}


void CASW_Flamer_Projectile::KillEffects()
{
	//if (m_pMainGlow)
		//m_pMainGlow->FadeAndDie(0.05f);
	//if (m_pGlowTrail)
		//m_pGlowTrail->FadeAndDie(0.05f);
}

void CASW_Flamer_Projectile::CreateEffects( void )
{
	// Start up the eye glow
	//m_pMainGlow = CSprite::SpriteCreate( "swarm/sprites/whiteglow1.vmt", GetLocalOrigin(), false );

	//int	nAttachment = LookupAttachment( "fuse" );		// todo: make an attachment on the new model? is that even needed?

	if ( m_pMainGlow != NULL )
	{
		m_pMainGlow->FollowEntity( this );
		//m_pMainGlow->SetAttachment( this, nAttachment );
		m_pMainGlow->SetTransparency( kRenderGlow, 255, 255, 255, 200, kRenderFxNoDissipation );
		m_pMainGlow->SetScale( 0.2f );
		m_pMainGlow->SetGlowProxySize( 4.0f );
	}

	// Start up the eye trail
	//m_pGlowTrail	= CSpriteTrail::SpriteTrailCreate( "swarm/sprites/greylaser1.vmt", GetLocalOrigin(), false );

	if ( m_pGlowTrail != NULL )
	{
		m_pGlowTrail->FollowEntity( this );
		//m_pGlowTrail->SetAttachment( this, nAttachment );
		m_pGlowTrail->SetTransparency( kRenderTransAdd, 128, 128, 128, 255, kRenderFxNone );
		m_pGlowTrail->SetStartWidth( 8.0f );
		m_pGlowTrail->SetEndWidth( 1.0f );
		m_pGlowTrail->SetLifeTime( 0.5f );
	}
}

bool CASW_Flamer_Projectile::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );
	return true;
}

unsigned int CASW_Flamer_Projectile::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

void CASW_Flamer_Projectile::Precache( void )
{
	PrecacheModel( PELLET_MODEL );

	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );

	BaseClass::Precache();
}

void CASW_Flamer_Projectile::FlameHit( CBaseEntity *pOther, const Vector &vecHitPos, bool bOnlyHurtUnignited )
{
	if (!pOther)
		return;

	bool bHurt = true;

	if ( pOther->m_takedamage != DAMAGE_NO)
	{
		if ( pOther == m_pLastHitEnt )
			return;

		if ( bOnlyHurtUnignited)
		{
			CBaseAnimating* pAnim = dynamic_cast<CBaseAnimating*>(pOther);
			if ( pAnim && pAnim->IsOnFire() )
			{
				bHurt = false;
			}
		}

		if ( bHurt )
		{
			Vector	vecNormalizedVel = GetAbsVelocity();

			ClearMultiDamage();
			VectorNormalize( vecNormalizedVel );

			if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() && pOther->IsNPC() )
			{
				CTakeDamageInfo	dmgInfo( this, m_pGetsCreditedForDamage, m_flDamage, DMG_BURN );
				dmgInfo.AdjustPlayerDamageInflictedForSkillLevel();
				CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, vecHitPos, asw_flamer_force.GetFloat() );
				dmgInfo.SetDamagePosition( vecHitPos );
				dmgInfo.SetWeapon( m_hCreatorWeapon.Get() );

				pOther->TakeDamage(dmgInfo);
			}
			else
			{
				CTakeDamageInfo	dmgInfo( this, m_pGetsCreditedForDamage, m_flDamage, DMG_BURN );
				CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, vecHitPos, asw_flamer_force.GetFloat() );
				dmgInfo.SetDamagePosition( vecHitPos );
				dmgInfo.SetWeapon( m_hCreatorWeapon.Get() );

				pOther->TakeDamage(dmgInfo);
			}

			ApplyMultiDamage();

			// keep going through normal entities?
			m_pLastHitEnt = pOther;
		}

		if ( pOther->Classify() == CLASS_ASW_SHIELDBUG )	// We also want to bounce off shield bugs
		{
			Vector vel = GetAbsVelocity();
			Vector dir = vel;
			VectorNormalize( dir );

			// reflect velocity around normal
			vel = -2.0f * dir + vel;
			vel *= 0.4f;

			// absorb 80% in impact
			SetAbsVelocity( vel );
		}

		return;
	}

	if ( pOther->GetCollisionGroup() == ASW_COLLISION_GROUP_PASSABLE )
		return;

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
			vel *= 0.4f;
			
			// absorb 80% in impact
			//vel *= GRENADE_COEFFICIENT_OF_RESTITUTION;
			SetAbsVelocity( vel );
		}
		return;
	}
	else
	{
		// Put a mark unless we've hit the sky
		if ( ( tr.surface.flags & SURF_SKY ) == false )
		{
			UTIL_ImpactTrace( &tr, DMG_BURN );
		}
		KillEffects();
		UTIL_Remove( this );
	}
}

void CASW_Flamer_Projectile::ProjectileTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;

	//if (pOther)
		//Msg("Flamer projectile touched %s\n", pOther->GetClassname());
	FlameHit(pOther, GetAbsOrigin(), false);
	//NDebugOverlay::Cross3D(GetAbsOrigin(), 10, 255, 255, 0, true, 10.0f);
}

CASW_Flamer_Projectile * CASW_Flamer_Projectile::Flamer_Projectile_Create( float flDamage, const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pEntityToCreditForTheDamage /*= NULL*/, CBaseEntity *pCreatorWeapon /*= NULL */ )
{
	CASW_Flamer_Projectile *pPellet = (CASW_Flamer_Projectile *)CreateEntityByName( "asw_flamer_projectile" );	
	pPellet->SetAbsAngles( angles );
	pPellet->Spawn();
	pPellet->SetOwnerEntity( pOwner );
	pPellet->m_flDamage = flDamage;
	pPellet->m_pGetsCreditedForDamage = pEntityToCreditForTheDamage ? pEntityToCreditForTheDamage : pOwner;
	UTIL_SetOrigin( pPellet, position );
	pPellet->SetAbsVelocity( velocity );
	pPellet->m_hCreatorWeapon.Set( pCreatorWeapon );

	if (asw_flamer_debug.GetBool())
		pPellet->m_debugOverlays |= OVERLAY_BBOX_BIT;

	return pPellet;
}

#define ASW_FLAMER_PROJECTILE_ACCN 650.0f
void CASW_Flamer_Projectile::PhysicsSimulate()
{
	// Make sure not to simulate this guy twice per frame
	if (m_nSimulationTick == gpGlobals->tickcount)
		return;

	// slow down the projectile's velocity	
	Vector dir = GetAbsVelocity();
	VectorNormalize(dir);		
	SetAbsVelocity(GetAbsVelocity() - (dir * gpGlobals->frametime * ASW_FLAMER_PROJECTILE_ACCN));
	dir = GetAbsVelocity();
	
	BaseClass::PhysicsSimulate();
}

// need to force send as it has no model
int CASW_Flamer_Projectile::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

void CASW_Flamer_Projectile::CollideThink()
{
	if (gpGlobals->curtime >= m_fDieTime)
	{
		SUB_Remove();
		return;
	}

	SetNextThink( gpGlobals->curtime + 0.1f );

	if (m_vecOldPos == vec3_origin)
		m_vecOldPos = GetAbsOrigin();

	trace_t tr;
	UTIL_TraceLine(GetAbsOrigin(), m_vecOldPos, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	bool bHit = false;
	if (tr.fraction != 1.0)
	{
		if (tr.m_pEnt && !dynamic_cast<CASW_Flamer_Projectile*>(tr.m_pEnt))
		{
			//Msg("Flamer projectile CollideThinked %s\n", tr.m_pEnt->GetClassname());		
			FlameHit(tr.m_pEnt, tr.endpos, false);
			bHit = true;

			if (asw_flamer_debug.GetBool())
				NDebugOverlay::Cross3D(tr.endpos, 10, 0, 0, 255, true, 10.0f);
		}
	}
	// scan for setting on fire nearby NPCs
	if (!bHit)
	{
		trace_t tr;
		Ray_t ray;
		CTraceFilterAliensEggsGoo filter( this, COLLISION_GROUP_NONE );				
		//UTIL_TraceHull(GetAbsOrigin(), GetAbsOrigin() + Vector(0,0,1), ASW_FLAMER_HULL_MINS, ASW_FLAMER_HULL_MAXS, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr); 
		float size_scale = 0.5f + 0.5f * (1.0f - clamp<float>(m_fDieTime - gpGlobals->curtime, 0.0f, 1.0f));	// NOTE: assumes 1.0 lifetime
		ray.Init( GetAbsOrigin(), m_vecOldPos, ASW_FLAMER_HULL_MINS * size_scale, ASW_FLAMER_HULL_MAXS * size_scale );
		enginetrace->TraceRay( ray, MASK_SOLID, &filter, &tr );
		if ( tr.m_pEnt )
		{						
			FlameHit(tr.m_pEnt, tr.endpos, !m_bHurtIgnited);
			//NDebugOverlay::Cross3D(tr.endpos, 10, 255, 0, 0, true, 10.0f);
			if (asw_flamer_debug.GetBool())
			{
				Msg("Flame hit %d %s\n", tr.m_pEnt->entindex(), tr.m_pEnt->GetClassname());
				NDebugOverlay::SweptBox(GetAbsOrigin(), m_vecOldPos, ASW_FLAMER_HULL_MINS * size_scale, ASW_FLAMER_HULL_MAXS * size_scale, vec3_angle, 255, 255, 0, 0 ,0.1f);
				NDebugOverlay::Line(GetAbsOrigin(), tr.m_pEnt->GetAbsOrigin(), 255, 255, 0, false, 0.1f );
			}
		}
		else if (tr.allsolid || tr.startsolid)
		{
			if (asw_flamer_debug.GetBool())
				NDebugOverlay::Box(GetAbsOrigin(), ASW_FLAMER_HULL_MINS * size_scale, ASW_FLAMER_HULL_MAXS * size_scale, 0, 0, 255, 0 ,0.1f);
		}
		else
		{
			if (asw_flamer_debug.GetBool())
				NDebugOverlay::Box(GetAbsOrigin(), ASW_FLAMER_HULL_MINS * size_scale, ASW_FLAMER_HULL_MAXS * size_scale, 255, 0, 0, 0 ,0.1f);
		}
	}
	m_vecOldPos = GetAbsOrigin();
}

void CASW_Flamer_Projectile::UpdateOnRemove()
{
	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(GetOwnerEntity());
	if (pMarine && pMarine->GetMarineResource())
	{
		// count as a shot fired
		if (pMarine->GetMarineResource()->m_iOnlyWeaponEquipIndex == -1 && ASWEquipmentList())		// check if marine hasn't used any weapon yet
		{
			// marine hasn't used any weapon, we need to pass the index of the flamethrower in, so it can be set
			//  (we could do this everytime, but we only do it if no weapon is set to save doing this search needlessly every time you fire)
			pMarine->GetMarineResource()->UsedWeapon(ASWEquipmentList()->GetRegularIndex("asw_weapon_flamer"), false, 1);
		}
		else
		{
			pMarine->GetMarineResource()->UsedWeapon(NULL, 1);
		}
	}
	BaseClass::UpdateOnRemove();
}