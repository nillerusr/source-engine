#include "cbase.h"
#include "asw_rocket.h"
#include "smoke_trail.h"
#include "IEffects.h"
#include "te_effect_dispatch.h"
#include "explode.h"
#include "fmtstr.h"
#include "asw_gamerules.h"
#include "asw_marine.h"
#include "particle_parse.h"
#include "asw_shareddefs.h"
#include "asw_parasite.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_ROCKET_MODEL "models/swarm/MiniRocket/MiniRocket.mdl"
extern int	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

PRECACHE_REGISTER( asw_rocket );

IMPLEMENT_SERVERCLASS_ST(CASW_Rocket, DT_ASW_Rocket)
	
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Rocket )

	DEFINE_FIELD( m_hHomingTarget,			FIELD_EHANDLE ),
	//DEFINE_FIELD( m_hRocketTrail,			FIELD_EHANDLE ),
	DEFINE_FIELD( m_flGracePeriodEndsAt,	FIELD_TIME ),
	DEFINE_FIELD( m_flDamage,				FIELD_FLOAT ),
	DEFINE_FIELD( m_bCreateDangerSounds,	FIELD_BOOLEAN ),
	
	// Function Pointers
	DEFINE_FUNCTION( MissileTouch ),
	DEFINE_FUNCTION( AccelerateThink ),
	DEFINE_FUNCTION( IgniteThink ),
	DEFINE_FUNCTION( SeekThink ),

END_DATADESC()
LINK_ENTITY_TO_CLASS( asw_rocket, CASW_Rocket );

ConVar asw_rocket_min_speed("asw_rocket_min_speed", "280", FCVAR_CHEAT);
ConVar asw_rocket_max_speed("asw_rocket_max_speed", "600", FCVAR_CHEAT);
ConVar asw_rocket_acceleration("asw_rocket_acceleration", "60", FCVAR_CHEAT);
ConVar asw_rocket_hover_thrust("asw_rocket_hover_thrust", "60", FCVAR_CHEAT);
ConVar asw_rocket_hover_height("asw_rocket_hover_height", "10", FCVAR_CHEAT);
ConVar asw_rocket_drag("asw_rocket_drag", "0.90", FCVAR_CHEAT);
ConVar asw_rocket_lifetime("asw_rocket_lifetime", "3.0", FCVAR_CHEAT);
ConVar asw_rocket_homing_range("asw_rocket_homing_range", "640000", FCVAR_CHEAT);
ConVar asw_rocket_wobble_freq("asw_rocket_wobble_freq", "0.25", FCVAR_CHEAT);
ConVar asw_rocket_wobble_amp("asw_rocket_wobble_amp", "90", FCVAR_CHEAT);
ConVar asw_rocket_debug("asw_rocket_debug", "0", FCVAR_CHEAT);

#define ASW_ROCKET_MIN_SPEED asw_rocket_min_speed.GetFloat()
#define ASW_ROCKET_MAX_SPEED asw_rocket_max_speed.GetFloat()
#define ASW_ROCKET_ACCELERATION asw_rocket_acceleration.GetFloat()
#define ASW_ROCKET_DRAG asw_rocket_drag.GetFloat()
#define ASW_ROCKET_LIFETIME asw_rocket_lifetime.GetFloat();
#define ASW_ROCKET_MAX_HOMING_RANGE asw_rocket_homing_range.GetFloat()
#define ROCKET_HOVER_HEIGHT asw_rocket_hover_height.GetFloat()
#define ASW_ROCKET_HOVER_THRUST asw_rocket_hover_thrust.GetFloat()

CASW_Rocket::CASW_Rocket() : m_bFlyingWild(false)
{
	m_bCreateDangerSounds = false;
	m_szFlightSound = NULL; // this sound doesn't exist any more: "ASWRocket.Ignite";
	m_szDetonationSound = "ASWRocket.Explosion";
	m_CreatorWeaponClass = (Class_T)CLASS_ASW_UNKNOWN;
}

CASW_Rocket::~CASW_Rocket()
{
	if ( m_hHomingTarget.Get() )
	{
		m_RocketAssigner.Deallocate( m_hHomingTarget.Get(), this );
	}
}

void CASW_Rocket::Precache( void )
{
	if ( m_szFlightSound )
		PrecacheScriptSound( m_szFlightSound );
	// not used: PrecacheScriptSound("ASWRocket.Accelerate");
	PrecacheScriptSound("ASWRocket.Explosion");
	PrecacheModel( ASW_ROCKET_MODEL );
	PrecacheParticleSystem( "rocket_trail_small" );
	PrecacheParticleSystem( "explosion_air_small" );	
}

void CASW_Rocket::Spawn( void )
{
	Precache();

	SetSolid( SOLID_BBOX );
	SetModel(ASW_ROCKET_MODEL);
	UTIL_SetSize( this, -Vector(4,4,4), Vector(4,4,4) );

	SetTouch( &CASW_Rocket::MissileTouch );

	SetCollisionGroup( ASW_COLLISION_GROUP_PLAYER_MISSILE );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetThink( &CASW_Rocket::IgniteThink );
	
	SetNextThink( gpGlobals->curtime + 0.01f );
	SetDamage( 200.0f );

	AddEffects( EF_NOSHADOW );

	m_takedamage = DAMAGE_NO;
	m_iHealth = m_iMaxHealth = 100;
	m_bloodColor = DONT_BLEED;
	m_flGracePeriodEndsAt = 0;
	m_flNextWobbleTime = gpGlobals->curtime + asw_rocket_wobble_freq.GetFloat();

	m_fSpawnTime = gpGlobals->curtime;
	m_fDieTime = gpGlobals->curtime + ASW_ROCKET_LIFETIME;

	AddFlag( FL_OBJECT );
}

void CASW_Rocket::Activate( void )
{
	BaseClass::Activate();

	if ( m_szFlightSound )
		EmitSound( m_szFlightSound );
}

void CASW_Rocket::StopLoopingSounds( void )
{
	if ( m_szFlightSound )
		StopSound( m_szFlightSound );
}

void CASW_Rocket::Event_Killed( const CTakeDamageInfo &info )
{
	m_takedamage = DAMAGE_NO;
}

unsigned int CASW_Rocket::PhysicsSolidMaskForEntity( void ) const
{ 
	return BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX;
}

int CASW_Rocket::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	return 0;	// rocket can't be damaged for now.  This could be expanded into shooting rockets down, etc. later if we want
}

void CASW_Rocket::DumbFire( void )
{
	SetThink( NULL );
	SetMoveType( MOVETYPE_FLY );

	SetModel(ASW_ROCKET_MODEL);
	UTIL_SetSize( this, vec3_origin, vec3_origin );
}

void CASW_Rocket::SetGracePeriod( float flGracePeriod )
{
	m_flGracePeriodEndsAt = gpGlobals->curtime + flGracePeriod;

	// Go non-solid until the grace period ends
	AddSolidFlags( FSOLID_NOT_SOLID );
}

void CASW_Rocket::AccelerateThink( void )
{
	Vector vecForward;

	// !!!UNDONE - make this work exactly the same as HL1 RPG, lest we have looping sound bugs again!
	//EmitSound( "ASWRocket.Accelerate" );

	// SetEffects( EF_LIGHT );

	AngleVectors( GetLocalAngles(), &vecForward );
	SetAbsVelocity( vecForward * ASW_ROCKET_MIN_SPEED );

	SetThink( &CASW_Rocket::SeekThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CASW_Rocket::DoExplosion( bool bHitWall )
{
	Vector vecExplosionPos = GetAbsOrigin();
	CPASFilter filter( vecExplosionPos );

	EmitSound( m_szDetonationSound );

	DispatchParticleEffect( "explosion_air_small", GetAbsOrigin(), vec3_angle );

	CTakeDamageInfo info( this, GetOwnerEntity(), GetDamage(), DMG_BLAST );
	info.SetWeapon( m_hCreatorWeapon );
	ASWGameRules()->RadiusDamage( info, GetAbsOrigin(), 50, CLASS_NONE, NULL );
}

void CASW_Rocket::Explode( void )
{
	// Don't explode against the skybox. Just pretend that 
	// the missile flies off into the distance.
	Vector forward;

	GetVectors( &forward, NULL, NULL );

	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + forward * 16, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	m_takedamage = DAMAGE_NO;
	SetSolid( SOLID_NONE );
	if( tr.fraction == 1.0 || !(tr.surface.flags & SURF_SKY) )
	{
		bool bHitCreature = tr.m_pEnt && tr.m_pEnt->IsNPC();
		DoExplosion(!bHitCreature);
	}

	//UTIL_Remove( this );
	
	SetTouch( NULL );
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );
	SetNextThink( gpGlobals->curtime + 0.2f );
	SetThink( &CASW_Rocket::SUB_Remove );
}

void CASW_Rocket::MissileTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	
	// Don't touch triggers (but DO hit weapons)
	if ( pOther->IsSolidFlagSet(FSOLID_TRIGGER|FSOLID_VOLUME_CONTENTS) && pOther->GetCollisionGroup() != COLLISION_GROUP_WEAPON )
		return;

	if (pOther == GetOwnerEntity())
		return;

	// make sure we don't die on things we shouldn't
	if ( !ASWGameRules() || !ASWGameRules()->ShouldCollide( GetCollisionGroup(), pOther->GetCollisionGroup() ) )
		return;

	if (asw_rocket_debug.GetBool())
		Msg("Rocket exploding on %d:%s\n", pOther->entindex(), pOther->GetClassname());
	//Msg("owner is %d:%s\n", GetOwnerEntity() ? GetOwnerEntity()->entindex() : -1,
		//GetOwnerEntity() ? GetOwnerEntity()->GetClassname() : "unknown");

	Explode();
}

void CASW_Rocket::IgniteThink( void )
{
	SetMoveType( MOVETYPE_FLY );
	SetModel(ASW_ROCKET_MODEL);
	UTIL_SetSize( this, vec3_origin, vec3_origin );
 	RemoveSolidFlags( FSOLID_NOT_SOLID );

	//TODO: Play opening sound

	Vector vecForward;

	AngleVectors( GetLocalAngles(), &vecForward );
	SetAbsVelocity( vecForward * ASW_ROCKET_MIN_SPEED );

	SetThink( &CASW_Rocket::SeekThink );
	SetNextThink( gpGlobals->curtime );
}

CBaseEntity	* CASW_Rocket::FindPotentialTarget( void ) const
{
	float		bestdist = 0;		
	CBaseEntity	*bestent = NULL;

	Vector v_forward, v_right, v_up;
	AngleVectors( GetAbsAngles(), &v_forward, &v_right, &v_up );

	// find the aimtarget nearest us
	int count = AimTarget_ListCount();		
	if ( count )
	{
		CBaseEntity **pList = (CBaseEntity **)stackalloc( sizeof(CBaseEntity *) * count );
		AimTarget_ListCopy( pList, count );

		CTraceFilterSkipTwoEntities filter(this, GetOwnerEntity(), COLLISION_GROUP_NONE);

		for ( int i = 0; i < count; i++ )
		{
			CBaseEntity *pEntity = pList[i];

			if (!pEntity || !pEntity->IsAlive() || !pEntity->edict() || !pEntity->IsNPC() )
			{
				//Msg("not alive or not an edict, skipping\n");
				continue;
			}

			if (!pEntity || !pEntity->IsAlive() || !pEntity->edict() || !pEntity->IsNPC() )
			{
				//Msg("not alive or not an edict, skipping\n");
				continue;
			}
	
			// don't autoaim onto marines
			if (pEntity->Classify() == CLASS_ASW_MARINE)
				continue;

			if ( pEntity->Classify() == CLASS_ASW_PARASITE )
			{
				CASW_Parasite *pParasite = static_cast< CASW_Parasite* >( pEntity );
				if ( pParasite->m_bInfesting )
				{
					continue;
				}
			}

			Vector center = pEntity->BodyTarget( GetAbsOrigin() );
			Vector center_flat = center;
			center_flat.z = GetAbsOrigin().z;

			Vector dir = (center - GetAbsOrigin());
			VectorNormalize( dir );

			Vector dir_flat = (center_flat - GetAbsOrigin());
			VectorNormalize( dir_flat );

			// make sure it's in front of the rocket
			float dot = DotProduct (dir, v_forward );
			//if (dot < 0)
			//{					
			//continue;
			//}

			float dist = (pEntity->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
			if (dist > ASW_ROCKET_MAX_HOMING_RANGE)
				continue;

			// check another marine isn't between us and the target to reduce FF
			trace_t tr;
			UTIL_TraceLine(GetAbsOrigin(), pEntity->WorldSpaceCenter(), MASK_SHOT, &filter, &tr);
			if (tr.fraction < 1.0f && tr.m_pEnt != pEntity && tr.m_pEnt && tr.m_pEnt->Classify() == CLASS_ASW_MARINE)
				continue;

			// does this critter already have enough rockets to kill it?
			{ 
				CASW_DamageAllocationMgr::IndexType_t assignmentIndex = m_RocketAssigner.Find( pEntity );
				if ( m_RocketAssigner.IsValid(assignmentIndex) )
				{
					if ( m_RocketAssigner[assignmentIndex].m_flAccumulatedDamage > pEntity->GetHealth() )
					{
						continue;
					}
				}
			}


			// check another marine isn't between us and the target to reduce FF
			UTIL_TraceLine(GetAbsOrigin(), pEntity->WorldSpaceCenter(), MASK_SHOT, &filter, &tr);
			if (tr.fraction < 1.0f && tr.m_pEnt != pEntity && tr.m_pEnt && tr.m_pEnt->Classify() == CLASS_ASW_MARINE)
				continue;

			// increase distance if dot isn't towards us
			dist += (1.0f - dot) * 150;	// bias of x units when object is 90 degrees to the side
			if (bestdist == 0 || dist < bestdist)
			{
				bestdist = dist;
				bestent = pEntity;
			}
		}

		if ( bestent && asw_rocket_debug.GetBool() )
		{
			Vector center = bestent->BodyTarget( GetAbsOrigin() );
			Vector center_flat = center;
			center_flat.z = GetAbsOrigin().z;

			Vector dir = (center - GetAbsOrigin());
			VectorNormalize( dir );
			Msg( "Rocket[%d] starting homing in on %s(%d) dir = %f %f %f\n", entindex(), bestent->GetClassname(), bestent->entindex(), VectorExpand( dir ) );
		}
	}

	return bestent;
}

void CASW_Rocket::SetTarget( CBaseEntity *pTarget )
{
	if ( m_hHomingTarget.Get() )
	{
		// if I had an old target, strike me from its list
		m_RocketAssigner.Deallocate( m_RocketAssigner.Find(pTarget), this );
	}
	m_hHomingTarget = pTarget;
	if ( pTarget )
	{
		m_RocketAssigner.Allocate( m_RocketAssigner.Insert(pTarget), this, GetDamage() );
	}
}

void CASW_Rocket::FindHomingPosition( Vector *pTarget )
{	
	CBaseEntity *pHomingTarget = m_hHomingTarget.Get();

	if ( !pHomingTarget )
	{
		SetTarget( pHomingTarget = FindPotentialTarget() );
		m_bFlyingWild = false;
	}

	if ( pHomingTarget )
	{
		*pTarget = pHomingTarget->WorldSpaceCenter();
		return;
	}
	else
	{
		// just fly straight if there's nothing to home in on
		Vector vecDir = GetAbsVelocity();
		vecDir.z = 0;
		*pTarget = GetAbsOrigin() + vecDir * 200.0f;
		VectorAngles( vecDir, m_vWobbleAngles );
		m_bFlyingWild = true;
	}
}

Vector CASW_Rocket::IntegrateRocketThrust( const Vector &vTargetDir,  ///< direction of target
										   float flDist )			 ///< distance to target
										   const
{
	Vector vNewVelocity;
	const float flSimulateScale = IsSimulatingOnAlternateTicks() ? 2 : 1 ;

	// apply thrust in our desired direction
	Vector vecThrust = vTargetDir * ASW_ROCKET_ACCELERATION * flSimulateScale;
	if (asw_rocket_debug.GetInt() == 2)
	{
		Msg("vecThrust = %s\n", VecToString(vecThrust));
	}
	vNewVelocity = GetAbsVelocity() + vecThrust;

	// apply drag
	float fDrag = ASW_ROCKET_DRAG - (0.1f * GetLifeFraction());	// increase drag as lifetime goes on (to help stop rockets spinning around their target)
	if (flDist < 300.0f && flDist > 0)
		fDrag -= (1.0f - (flDist / 300.0f)) * 0.1f;	// reduce drag further as we get close
	vNewVelocity *= ASW_ROCKET_DRAG;

	// cap velocity to our min/max
	float speed = vNewVelocity.Length();
	if ((speed > ASW_ROCKET_MAX_SPEED || speed < ASW_ROCKET_MIN_SPEED)
		&& speed > 0)
	{
		vNewVelocity *= (ASW_ROCKET_MAX_SPEED / speed);
	}

	// thrust away from the ground if we get too low
	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, ROCKET_HOVER_HEIGHT ), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction != 1.0f )
	{
		if ( vNewVelocity.z < 0 )
		{
			vNewVelocity.z += tr.fraction * ASW_ROCKET_HOVER_THRUST;
			if ( asw_rocket_debug.GetBool() )
				Msg( "Rocket[%d] hover thrusting %f vel.z is now %f\n", entindex(), tr.fraction * ASW_ROCKET_HOVER_THRUST, vNewVelocity.z );

			if ( vNewVelocity.z > 0 )
			{
				vNewVelocity.z = 0;
			}
		}
	}

	return vNewVelocity;
}

void CASW_Rocket::SeekThink( void )
{
	// If we have a grace period, go solid when it ends
	if ( m_flGracePeriodEndsAt )
	{
		if ( m_flGracePeriodEndsAt < gpGlobals->curtime )
		{
			RemoveSolidFlags( FSOLID_NOT_SOLID );
			m_flGracePeriodEndsAt = 0;
		}
	}

	Vector vNewVelocity = GetAbsVelocity();
	
	if ( m_bFlyingWild )
	{
		// wobble crazily. Poll for a new target every quarter second, and if none is found, go
		// careering off.
		if ( gpGlobals->curtime >= m_flNextWobbleTime )
		{
			Assert( !m_hHomingTarget.Get() );
			CBaseEntity *pHomingTarget = FindPotentialTarget(); 
			if ( pHomingTarget )
			{
				SetTarget( pHomingTarget );
				m_bFlyingWild = false;
			}
			else
			{
				// pick a new wobble direction
				/*
				m_vWobbleAngles = GetAbsAngles();
				m_vWobbleAngles.y =  m_vWobbleAngles.y + RandomFloat( -asw_rocket_wobble_amp.GetFloat(), asw_rocket_wobble_amp.GetFloat() ) ;
				if ( m_vWobbleAngles.y < 0 )
				{
					m_vWobbleAngles.y = 360 + m_vWobbleAngles.y;
				}
				else if ( m_vWobbleAngles.y > 360 )
				{
					m_vWobbleAngles.y = fmod( m_vWobbleAngles.y, 360 );
				}

				*/

				m_vWobbleAngles = GetAbsAngles();
				m_vWobbleAngles.y = fmodf( m_vWobbleAngles.y + RandomFloat( -asw_rocket_wobble_amp.GetFloat(), asw_rocket_wobble_amp.GetFloat() ), 360 );

				m_flNextWobbleTime = gpGlobals->curtime + asw_rocket_wobble_freq.GetFloat();
			}
		}
	}


	if ( !m_bFlyingWild )
	{
		Vector	targetPos;
		FindHomingPosition( &targetPos );

		// find target direction
		Vector	vTargetDir;
		VectorSubtract( targetPos, GetAbsOrigin(), vTargetDir );
		float flDist = VectorNormalize( vTargetDir );

		// find current direction
		Vector	vDir	= GetAbsVelocity();
		//float	flSpeed	= VectorNormalize( vDir );

		vNewVelocity = IntegrateRocketThrust( vTargetDir, flDist );

		// face direction of movement
		QAngle	finalAngles;
		VectorAngles( vNewVelocity, finalAngles );
		SetAbsAngles( finalAngles );

		// set to the new calculated velocity
		SetAbsVelocity( vNewVelocity );
	}
	else // wobble crazily
	{
#pragma message("TODO: straighten out this math")
		if ( gpGlobals->curtime >= m_flNextWobbleTime )
		{
			// pick a new wobble direction
			m_vWobbleAngles = GetAbsAngles();
			m_vWobbleAngles.y = fmodf( m_vWobbleAngles.y + RandomFloat( -asw_rocket_wobble_amp.GetFloat(), asw_rocket_wobble_amp.GetFloat() ), 360 );

			m_flNextWobbleTime = gpGlobals->curtime + asw_rocket_wobble_freq.GetFloat();
		}
		QAngle finalAngles = GetAbsAngles();
		finalAngles.y = ApproachAngle( m_vWobbleAngles.y, finalAngles.y, 360.f * (gpGlobals->curtime - GetLastThink()) );

		Vector forward;
		AngleVectors( finalAngles, &forward );
		vNewVelocity = forward * FastSqrtEst( vNewVelocity.LengthSqr() );
		if ( IsWallDodging() )
		{
			ComputeWallDodge( vNewVelocity );
			finalAngles.y = ApproachAngle( m_vWobbleAngles.y, finalAngles.y, 360.f * (gpGlobals->curtime - GetLastThink()) );
		}

		vNewVelocity = IntegrateRocketThrust( forward, 0 );

		// face direction of movement
		SetAbsAngles( finalAngles );

		// set to the new calculated velocity
		SetAbsVelocity( vNewVelocity );
	}

	// blow us up after our lifetime
	if (GetLifeFraction() >= 1.0f)
	{
		Explode();
	}
	else
	{
		// Think as soon as possible
		SetNextThink( gpGlobals->curtime );
	}
}


void	CASW_Rocket::ComputeWallDodge( const Vector &vCurVel )
{
	// trace to see if I'll hit a wall in the next second
	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + ( vCurVel * 0.8f ), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction < 1.0f )
	{
		// find a vector perpendicular to the wall
		Vector perp = tr.plane.normal.Cross( Vector(0,0,1) );
		Vector antiperp = -perp;

		// figure out whether to turn left or right!
		if ( antiperp.Dot(vCurVel) > perp.Dot(vCurVel) )
		{
			perp = antiperp;
		}

		// push the direction a little further away from the wall
		perp += fsel( vCurVel.Dot(tr.plane.normal) , -0.1f , 0.1f ) * tr.plane.normal;

		// work out new angles based on this direction
		VectorAngles( perp, m_vWobbleAngles );
	}
}

CASW_Rocket * CASW_Rocket::Create( float fDamage, const Vector &vecOrigin, const QAngle &vecAngles, CBaseCombatCharacter *pentOwner /*= NULL */, CBaseEntity * pCreatorWeapon /*= NULL*/, const char *className /*= "asw_rocket" */ )
{
	CASW_Rocket *pMissile = (CASW_Rocket *) CBaseEntity::Create( className, vecOrigin, vecAngles, pentOwner );
	pMissile->SetDamage( fDamage );
	pMissile->ChangeFaction( pentOwner->GetFaction() );
	pMissile->Activate();
	
	if (asw_rocket_debug.GetBool())
	{
		Msg("Rocket ang=%s\n", VecToString(vecAngles));
		pMissile->m_debugOverlays |= OVERLAY_TEXT_BIT;
	}
	
	Vector vecForward;
	AngleVectors( vecAngles, &vecForward );

	pMissile->SetAbsVelocity( vecForward * ASW_ROCKET_MIN_SPEED );	//  + Vector( 0,0, 128 )

	pMissile->m_hCreatorWeapon.Set( pCreatorWeapon );
	if( pCreatorWeapon )
		pMissile->m_CreatorWeaponClass = pCreatorWeapon->Classify();

	return pMissile;
}

float CASW_Rocket::GetLifeFraction() const 
{
	if (m_fDieTime <= m_fSpawnTime)
		return 0;

	return clamp<float>( (gpGlobals->curtime - m_fSpawnTime) / (m_fDieTime - m_fSpawnTime), 0.0f, 1.0f);
}

void CASW_Rocket::DrawDebugGeometryOverlays()
{
	if (m_hHomingTarget.Get())
	{
		NDebugOverlay::Line(GetAbsOrigin(), m_hHomingTarget->WorldSpaceCenter(), 255, 0, 0, true, 0.1f);
	}
	Vector vecTarget;
	FindHomingPosition(&vecTarget);
	NDebugOverlay::Line(GetAbsOrigin(), vecTarget, 0, 255, 0, true, 0.1f);
	BaseClass::DrawDebugGeometryOverlays();
}

bool CASW_Rocket::IsWallDodging() const
{
	return m_bFlyingWild;
}

CASW_DamageAllocationMgr CASW_Rocket::m_RocketAssigner;

CASW_DamageAllocationMgr::IndexType_t CASW_DamageAllocationMgr::Find( const EHANDLE &handle ) 
{
	for ( int i = m_Targets.Count() - 1; i >= 0 ; --i )
	{
		if ( m_Targets[i].m_hTargeted == handle )
			return (&m_Targets[i]);
	}

	// didn't find
	return InvalidIndex();
}

CASW_DamageAllocationMgr::IndexType_t CASW_DamageAllocationMgr::Insert( CBaseEntity* pTarget )
{
	IndexType_t i = Find( pTarget );
	if ( IsValid(i) )
	{
		// AssertMsg1( false, "Tried to double-insert %s into a damage allocation manager\n", pTarget->GetDebugName() );
		return i;
	}
	else
	{
		i = &m_Targets[m_Targets.AddToTail(tuple_t(pTarget))];
		return i;
	}
}

void CASW_DamageAllocationMgr::Remove( const EHANDLE &handle )
{
	IndexType_t i = Find( handle );
	if ( IsValid(i) )
	{
		// strike all projectiles for this target
		ProjectilePool_t::IndexLocalType_t j = i->m_nProjectiles;
		while ( m_ProjectileLists.IsValidIndex(j) )
		{
			ProjectilePool_t::IndexLocalType_t old = j;
			j = m_ProjectileLists.Next(j);
			m_ProjectileLists.Remove( old );
		}

		int vectoridx = i - m_Targets.Base();
		Assert( &m_Targets[vectoridx] == i );

		m_Targets.FastRemove( vectoridx );
	}
	else
	{
		AssertMsg1( false, "Tried to remove %s from a damage allocation manager whence it was absent.\n",
			handle->GetDebugName()	);
	}
}

float CASW_DamageAllocationMgr::Allocate( IndexType_t iTarget, CBaseEntity *pProjectile, float flDamage )
{
	if ( !IsValid( iTarget ) )
		return 0;

	Rebuild( iTarget );

	AssertMsg2( !IsProjectileForTarget( iTarget, pProjectile ), "Double-allocated %s to %s\n", pProjectile->GetDebugName(), 
		iTarget->m_hTargeted->GetDebugName() );

	tuple_t * RESTRICT ptarget = &Elem(iTarget);

	ProjectilePool_t::IndexLocalType_t projIdx = m_ProjectileLists.Alloc( true );
	m_ProjectileLists[projIdx].m_hHandle = pProjectile; 
	m_ProjectileLists[projIdx].m_flDamage = flDamage; 

	if ( m_ProjectileLists.IsValidIndex(ptarget->m_nProjectiles) )
	{
		m_ProjectileLists.LinkBefore( ptarget->m_nProjectiles, projIdx );
	}

	ptarget->m_nProjectiles = projIdx;
	return ptarget->m_flAccumulatedDamage += flDamage;
}

float CASW_DamageAllocationMgr::Deallocate( IndexType_t iTarget, CBaseEntity *pProjectile )
{
	if ( !IsValid( iTarget ) )
		return 0;

	Rebuild( iTarget );

	tuple_t * RESTRICT ptarget = &Elem(iTarget);
	ProjectilePool_t::IndexLocalType_t projIdx = iTarget->m_nProjectiles;
	const EHANDLE hProj = pProjectile; // for faster comparison
	while ( m_ProjectileLists.IsValidIndex(projIdx) )
	{
		if ( m_ProjectileLists[projIdx].m_hHandle == hProj )
		{
			ptarget->m_flAccumulatedDamage -= m_ProjectileLists[projIdx].m_flDamage;
			ptarget->m_nProjectiles = m_ProjectileLists.Next(projIdx);
			m_ProjectileLists.Remove( projIdx );
			return ptarget->m_flAccumulatedDamage; // better than break bc of load-hit-store
		}

		// clean up any dead projectiles (note the work to advance
		// the iterator while also deleting an element)
		if ( !m_ProjectileLists[projIdx].m_hHandle.Get() )
		{
			ProjectilePool_t::IndexLocalType_t toRemove = projIdx;
			ptarget->m_flAccumulatedDamage -= m_ProjectileLists[projIdx].m_flDamage;
			ptarget->m_nProjectiles = projIdx = m_ProjectileLists.Next( projIdx );
			m_ProjectileLists.Remove( toRemove );
		}
		else
		{
			Assert( static_cast<CASW_Rocket *>(m_ProjectileLists[projIdx].m_hHandle.Get())->GetTarget() == ptarget->m_hTargeted );
			projIdx = m_ProjectileLists.Next( projIdx );
		}
	}

	return ptarget->m_flAccumulatedDamage;
}

CASW_DamageAllocationMgr::ProjectilePool_t::IndexType_t CASW_DamageAllocationMgr::GetProjectileIndexInTarget( IndexType_t iTarget, CBaseEntity *pProjectile )
{
	for ( CASW_DamageAllocationMgr::ProjectilePool_t::IndexLocalType_t i = iTarget->m_nProjectiles;
		  m_ProjectileLists.IsValidIndex( i );
		  i = m_ProjectileLists.Next(i) )
	{
		if ( m_ProjectileLists[i].m_hHandle.Get() == pProjectile )
		{
			return i;
		}
	}

	return m_ProjectileLists.InvalidIndex();
}

void CASW_DamageAllocationMgr::Rebuild( IndexType_t iTarget )
{
	float flRecompedDamage = 0;

	for ( CASW_DamageAllocationMgr::ProjectilePool_t::IndexLocalType_t i = iTarget->m_nProjectiles;
		m_ProjectileLists.IsValidIndex( i );
		i = m_ProjectileLists.Next(i) )
	{

		// clean up any dead projectiles (note the work to advance
		// the iterator while also deleting an element)
		if ( !m_ProjectileLists[i].m_hHandle.Get() )
		{
			ProjectilePool_t::IndexLocalType_t toRemove = i;
			iTarget->m_flAccumulatedDamage -= m_ProjectileLists[i].m_flDamage;
			iTarget->m_nProjectiles = i = m_ProjectileLists.Next( i );
			m_ProjectileLists.Remove( toRemove );
		}
		else
		{
			flRecompedDamage += m_ProjectileLists[i].m_flDamage;
		}
	}

	// Assert( flRecompedDamage == iTarget->m_flAccumulatedDamage );
	iTarget->m_flAccumulatedDamage = flRecompedDamage;
}

