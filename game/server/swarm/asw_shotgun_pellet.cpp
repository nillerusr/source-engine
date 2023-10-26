#include "cbase.h"
#include "asw_shotgun_pellet.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "asw_gamerules.h"
#include "iasw_spawnable_npc.h"
#include "asw_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


const float GRENADE_COEFFICIENT_OF_RESTITUTION = 0.2f;

ConVar asw_shotgun_pellet_fade_length("asw_shotgun_pellet_fade_length", "100.0f", 0, "Length of the trail on the shotgun pellets");
ConVar asw_shotgun_pellets_pass("asw_shotgun_pellets_pass", "0", FCVAR_CHEAT, "Set to make shotgun pellets pass through aliens (as opposed to stopping on contact)");

#define PELLET_MODEL "models/swarm/Shotgun/ShotgunPellet.mdl"



LINK_ENTITY_TO_CLASS( asw_shotgun_pellet, CASW_Shotgun_Pellet );

BEGIN_DATADESC( CASW_Shotgun_Pellet )
	DEFINE_FUNCTION( PelletTouch ),

	// Fields
	DEFINE_FIELD( m_pMainGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pGlowTrail, FIELD_EHANDLE ),
	DEFINE_FIELD( m_inSolid, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hLastHit, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flDamage, FIELD_FLOAT ),
END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_Shotgun_Pellet::~CASW_Shotgun_Pellet( void )
{
	KillEffects();
}

void CASW_Shotgun_Pellet::Spawn( void )
{
	Precache( );

	SetModel( PELLET_MODEL );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );

	m_flDamage		= 7;	// just a default, would get overriden by the skill base damage passed in in the create function
	m_takedamage	= DAMAGE_NO;

	SetSize( -Vector(1,1,1), Vector(1,1,1) );
	SetSolid( SOLID_BBOX );
	SetGravity( 0.05f );
	//SetCollisionGroup( ASW_COLLISION_GROUP_SHOTGUN_PELLET );
	SetCollisionGroup( ASW_COLLISION_GROUP_IGNORE_NPCS );	
	//CreateVPhysics();

	SetTouch( &CASW_Shotgun_Pellet::PelletTouch );

	CreateEffects();

	Msg("Created sg pellet\n");

	//AddSolidFlags( FSOLID_NOT_STANDABLE );

	//BaseClass::Spawn();
	// projectile only lasts 1 second
	SetThink( &CASW_Shotgun_Pellet::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 1.0f );

	//AddSolidFlags(FSOLID_TRIGGER);
	m_hLastHit = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Shotgun_Pellet::OnRestore( void )
{
	CreateEffects();

	BaseClass::OnRestore();
}


void CASW_Shotgun_Pellet::KillEffects()
{
	if (m_pMainGlow)
		m_pMainGlow->FadeAndDie(0.05f);
	if (m_pGlowTrail)
		m_pGlowTrail->FadeAndDie(0.05f);
}

void CASW_Shotgun_Pellet::CreateEffects( void )
{
	// Start up the eye glow
	m_pMainGlow = CSprite::SpriteCreate( "swarm/sprites/whiteglow1.vmt", GetLocalOrigin(), false );

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
	m_pGlowTrail	= CSpriteTrail::SpriteTrailCreate( "swarm/sprites/greylaser1.vmt", GetLocalOrigin(), false );

	if ( m_pGlowTrail != NULL )
	{
		m_pGlowTrail->FollowEntity( this );
		//m_pGlowTrail->SetAttachment( this, nAttachment );
		m_pGlowTrail->SetTransparency( kRenderTransAdd, 128, 128, 128, 255, kRenderFxNone );
		m_pGlowTrail->SetStartWidth( 8.0f );
		m_pGlowTrail->SetEndWidth( 1.0f );
		m_pGlowTrail->SetLifeTime( 0.5f );
		m_pGlowTrail->SetMinFadeLength( asw_shotgun_pellet_fade_length.GetFloat() );
	}
}

bool CASW_Shotgun_Pellet::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );
	return true;
}

unsigned int CASW_Shotgun_Pellet::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

void CASW_Shotgun_Pellet::Precache( void )
{
	PrecacheModel( PELLET_MODEL );

	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );
	PrecacheScriptSound("Weapon_Crossbow.BoltHitBody");

	BaseClass::Precache();
}

void CASW_Shotgun_Pellet::PelletHit( CBaseEntity *pOther)
{
	if (!pOther)
		return;

	if (pOther == GetLastHit())		// don't damage the same alien twice
		return;

	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;	

	if ( pOther->m_takedamage != DAMAGE_NO )
	{
		trace_t	tr, tr2;
		tr = BaseClass::GetTouchTrace();
		Vector	vecNormalizedVel = GetAbsVelocity();

		ClearMultiDamage();
		VectorNormalize( vecNormalizedVel );

		if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() && pOther->IsNPC() )
		{
			CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), m_flDamage, DMG_NEVERGIB );
			dmgInfo.AdjustPlayerDamageInflictedForSkillLevel();
			CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
			dmgInfo.SetDamagePosition( tr.endpos );
			pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );
		}
		else
		{
			CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), m_flDamage, DMG_BULLET | DMG_NEVERGIB );
			CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
			dmgInfo.SetDamagePosition( tr.endpos );
			pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );
		}

		ApplyMultiDamage();

		//Adrian: keep going through the glass.
		if ( pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS )
			 return;

		// pellets should carry on through spawnable enemies?
		IASW_Spawnable_NPC* pSpawnable = dynamic_cast<IASW_Spawnable_NPC*>(pOther);
		if (pSpawnable && asw_shotgun_pellets_pass.GetBool())
		{
			m_hLastHit = pOther;
			return;
		}

		SetAbsVelocity( Vector( 0, 0, 0 ) );

		// play body "thwack" sound
		EmitSound( "Weapon_Crossbow.BoltHitBody" );

		Vector vForward;

		AngleVectors( GetAbsAngles(), &vForward );
		VectorNormalize ( vForward );

		UTIL_TraceLine( GetAbsOrigin(),	GetAbsOrigin() + vForward * 128, MASK_OPAQUE, pOther, COLLISION_GROUP_NONE, &tr2 );

		if ( tr2.fraction != 1.0f )
		{
//			NDebugOverlay::Box( tr2.endpos, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 255, 0, 0, 10 );
//			NDebugOverlay::Box( GetAbsOrigin(), Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 0, 255, 0, 10 );

			if ( tr2.m_pEnt == NULL || ( tr2.m_pEnt && tr2.m_pEnt->GetMoveType() == MOVETYPE_NONE ) )
			{
				CEffectData	data;

				data.m_vOrigin = tr2.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = tr2.fraction != 1.0f;
			
				//DispatchEffect( "BoltImpact", data );
			}
		}		
		
		SetTouch( NULL );
		SetThink( NULL );

		KillEffects();
		UTIL_Remove( this );
	}
	else
	{
		trace_t	tr;
		tr = BaseClass::GetTouchTrace();

		// See if we struck the world
		if ( pOther->GetMoveType() == MOVETYPE_NONE && !( tr.surface.flags & SURF_SKY ) )
		{
			//EmitSound( "Weapon_Crossbow.BoltHitWorld" );

			// if what we hit is static architecture, can stay around for a while.
			Vector vecDir = GetAbsVelocity();
			VectorNormalize( vecDir );
								
			SetMoveType( MOVETYPE_NONE );
		
			Vector vForward;

			AngleVectors( GetAbsAngles(), &vForward );
			VectorNormalize ( vForward );

			CEffectData	data;

			data.m_vOrigin = tr.endpos;
			data.m_vNormal = vForward;
			data.m_nEntIndex = 0;
		
			//DispatchEffect( "BoltImpact", data );
			
			UTIL_ImpactTrace( &tr, DMG_BULLET );

			AddEffects( EF_NODRAW );
			SetTouch( NULL );
			KillEffects();
			SetThink( &CASW_Shotgun_Pellet::SUB_Remove );
			SetNextThink( gpGlobals->curtime + 2.0f );
			
			// Shoot some sparks
			if ( UTIL_PointContents( GetAbsOrigin(), CONTENTS_WATER ) != CONTENTS_WATER )
			{
				g_pEffects->Sparks( GetAbsOrigin() );
			}
		}
		else
		{
			// Put a mark unless we've hit the sky
			if ( ( tr.surface.flags & SURF_SKY ) == false )
			{
				UTIL_ImpactTrace( &tr, DMG_BULLET );
			}
			KillEffects();
			UTIL_Remove( this );
		}
	}
}

void CASW_Shotgun_Pellet::PelletTouch( CBaseEntity *pOther )
{
	if (!pOther)
		return;

	Msg("sg pellet testing colgroup %d vs %d\n", GetCollisionGroup(), pOther->GetCollisionGroup());
	// make sure we don't die on things we shouldn't
	if (!ASWGameRules() || !ASWGameRules()->ShouldCollide(GetCollisionGroup(), pOther->GetCollisionGroup()))
		return;
	Msg(" sg pellet allowed to hit %s\n", pOther->GetClassname());
	PelletHit(pOther);
}

CASW_Shotgun_Pellet* CASW_Shotgun_Pellet::Shotgun_Pellet_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float damage )
{
	Msg("CASW_Shotgun_Pellet::Shotgun_Pellet_Create\n");
	CASW_Shotgun_Pellet *pPellet = (CASW_Shotgun_Pellet *)CreateEntityByName( "asw_shotgun_pellet" );	
	pPellet->SetAbsAngles( angles );	
	pPellet->Spawn();
	pPellet->SetPelletDamage( damage );
	pPellet->SetOwnerEntity( pOwner );
	//Msg("making pellet with velocity %f,%f,%f\n", velocity.x, velocity.y, velocity.z);
	UTIL_SetOrigin( pPellet, position );
	pPellet->SetAbsVelocity( velocity );

	return pPellet;
}

CBaseEntity* CASW_Shotgun_Pellet::GetLastHit()
{
	return m_hLastHit.Get();
}