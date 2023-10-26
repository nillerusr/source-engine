#include "cbase.h"
#include "asw_queen_spit.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "asw_gamerules.h"
#include "iasw_spawnable_npc.h"
#include "world.h"
#include "asw_util_shared.h"
#include "asw_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_queen_spit_damage("asw_queen_spit_damage", "10", FCVAR_CHEAT, "Damage caused by the queen's goo spit");
ConVar asw_queen_spit_radius("asw_queen_spit_radius", "350", FCVAR_CHEAT, "Radius of the queen's goo spit attack");

#define SPIT_MODEL "models/swarm/Shotgun/ShotgunPellet.mdl"

LINK_ENTITY_TO_CLASS( asw_queen_spit, CASW_Queen_Spit );

IMPLEMENT_SERVERCLASS_ST( CASW_Queen_Spit, DT_ASW_Queen_Spit )
	
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Queen_Spit )
	DEFINE_FUNCTION( SpitTouch ),
		
	DEFINE_FIELD( m_inSolid, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_DmgRadius, FIELD_FLOAT ),
END_DATADESC()

extern int	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_Queen_Spit::~CASW_Queen_Spit( void )
{
	KillEffects();
}

void CASW_Queen_Spit::Spawn( void )
{
	Precache( );

	SetModel( SPIT_MODEL );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );

	m_flDamage		= ASWGameRules()->ModifyAlienDamageBySkillLevel(asw_queen_spit_damage.GetFloat());
	m_DmgRadius		= asw_queen_spit_radius.GetFloat();
	m_takedamage	= DAMAGE_NO;

	SetSize( -Vector(1,1,1), Vector(1,1,1) );
	SetSolid( SOLID_BBOX );
	SetGravity( 0.01f );
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	//CreateVPhysics();

	SetTouch( &CASW_Queen_Spit::SpitTouch );

	CreateEffects();

	SetThink( &CASW_Queen_Spit::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 5.0f );	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Queen_Spit::OnRestore( void )
{
	CreateEffects();

	BaseClass::OnRestore();
}


void CASW_Queen_Spit::KillEffects()
{
	
}

void CASW_Queen_Spit::CreateEffects( void )
{
	
}

bool CASW_Queen_Spit::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );
	return true;
}

unsigned int CASW_Queen_Spit::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

void CASW_Queen_Spit::Precache( void )
{
	PrecacheModel( SPIT_MODEL );
	//m_nSpitBurstSprite = PrecacheModel("sprites/greenspit1.vmt");// client side spittle.

	BaseClass::Precache();
}

void CASW_Queen_Spit::SpitTouch( CBaseEntity *pOther )
{
	if (!pOther)
		return;

	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;

	// make sure we don't die on things we shouldn't
	if (!ASWGameRules() || !ASWGameRules()->ShouldCollide(GetCollisionGroup(), pOther->GetCollisionGroup()))
		return;

	Detonate();
	return;
}

CASW_Queen_Spit* CASW_Queen_Spit::Queen_Spit_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner )
{
	CASW_Queen_Spit *pSpit = (CASW_Queen_Spit *)CreateEntityByName( "asw_queen_spit" );	
	pSpit->SetAbsAngles( angles );
	pSpit->Spawn();
	pSpit->SetOwnerEntity( pOwner );
	//Msg("making spit with velocity %f,%f,%f\n", velocity.x, velocity.y, velocity.z);
	UTIL_SetOrigin( pSpit, position );
	pSpit->SetAbsVelocity( velocity );

	return pSpit;
}

void CASW_Queen_Spit::Detonate()
{
	m_takedamage	= DAMAGE_NO;	

	// todo: make a goo explosion instead of this kind
	CPASFilter filter( GetAbsOrigin() );

	//te->Explosion( filter, 0.0,
		//&GetAbsOrigin(), 
		//g_sModelIndexFireball,
		//2.0, 
		//15,
		//TE_EXPLFLAG_NONE,
		//m_DmgRadius,
		//m_flDamage );
	//CPVSFilter filter( GetAbsOrigin() );
	Vector vecDir(0,0,1);
	//te->SpriteSpray( filter, 0.0,
		//&GetAbsOrigin(), &vecDir, m_nSpitBurstSprite, 10, 10, 15 );
	CEffectData	data;
	
	data.m_vOrigin = GetAbsOrigin();
	data.m_vNormal = vecDir;		
	data.m_flScale = 1.0f;

	DispatchEffect( "QueenSpitBurst", data );

	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60*vecForward, MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);

	// make a goo decal
	if ((tr.m_pEnt != GetWorldEntity()) || (tr.hitbox != 0))
	{
		// non-world needs smaller decals
		if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
		{
			UTIL_DecalTrace( &tr, "BeerSplash" );
		}
	}
	else
	{
		UTIL_DecalTrace( &tr, "BeerSplash" );
	}

	RadiusDamage ( CTakeDamageInfo( this, GetOwnerEntity(), m_flDamage, DMG_ACID | DMG_BLURPOISON ), GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );

	UTIL_Remove( this );
}