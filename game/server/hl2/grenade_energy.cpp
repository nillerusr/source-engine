//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Projectile shot by mortar synth.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "grenade_energy.h"
#if 1
#include "explode.h"
#include "particle_parse.h"
#include "te_effect_dispatch.h"
#include "world.h"
#endif
#include "soundent.h"
#include "player.h"
#include "hl2_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	 ENERGY_GRENADE_LIFETIME 1

#if 1
#define ENERGY_GRENADE_MAX_DANGER_RADIUS 300
#endif

ConVar    sk_dmg_energy_grenade		( "sk_dmg_energy_grenade","0");
ConVar	  sk_energy_grenade_radius	( "sk_energy_grenade_radius","0");

BEGIN_DATADESC( CGrenadeEnergy )

	DEFINE_FIELD( m_flMaxFrame,	  FIELD_INTEGER ),
	DEFINE_FIELD( m_nEnergySprite, FIELD_INTEGER ),
	DEFINE_FIELD( m_flLaunchTime, FIELD_TIME ),		

	// Function pointers
#if 1
	DEFINE_FIELD( m_pFragSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pFragTrail,	FIELD_CLASSPTR ),

	DEFINE_THINKFUNC( Animate ),
	DEFINE_ENTITYFUNC( GrenadeEnergyTouch ),
#else
	DEFINE_FUNCTION( Animate ),
	DEFINE_FUNCTION( GrenadeEnergyTouch ),
#endif

END_DATADESC()

LINK_ENTITY_TO_CLASS( grenade_energy, CGrenadeEnergy );

void CGrenadeEnergy::Spawn( void )
{
	Precache( );
	SetSolid( SOLID_BBOX );
#if 1
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	SetModel( "models/weapons/glueblob.mdl" );

	SetUse( &CBaseGrenade::DetonateUse );
	SetTouch( &CGrenadeEnergy::GrenadeEnergyTouch );
#else
	SetMoveType( MOVETYPE_FLY );

	SetModel( "Models/weapons/w_energy_grenade.mdl" );

	SetUse( DetonateUse );
	SetTouch( GrenadeEnergyTouch );
#endif
	SetNextThink( gpGlobals->curtime + 0.1f );

	m_flDamage			= sk_dmg_energy_grenade.GetFloat();
	m_DmgRadius		= sk_energy_grenade_radius.GetFloat();
#if 1
	m_takedamage	= DAMAGE_NO;
#else
	m_takedamage	= DAMAGE_YES;
#endif
	m_iHealth		= 1;

#if 1
	SetCycle( 0 );
#else
	m_flCycle		= 0;
#endif
	m_flLaunchTime	= gpGlobals->curtime;

#if 1
	m_fDangerRadius = 100;

	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
#else
	SetCollisionGroup( HL2COLLISION_GROUP_HOUNDEYE ); 
#endif

	UTIL_SetSize( this, vec3_origin, vec3_origin );

	m_flMaxFrame = (float) modelinfo->GetModelFrameCount( GetModel() ) - 1;

#if 1
#ifdef EZ2
	DispatchParticleEffect( "energy_grenade_electrical_arc", PATTACH_ABSORIGIN_FOLLOW, this );
#else
	DispatchParticleEffect( "electrical_arc_01_system", PATTACH_ABSORIGIN_FOLLOW, this );
#endif

	// Start up the eye glow
	m_pFragSprite = CSprite::SpriteCreate( "sprites/light_glow03.vmt", GetLocalOrigin(), false );

	if ( m_pFragSprite != NULL )
	{
		m_pFragSprite->SetParent( this );
		m_pFragSprite->SetTransparency( kRenderTransAdd, 70, 130, 245, 200, kRenderFxNone );
		m_pFragSprite->SetScale( 0.25f );
	}

	// Start up the eye trail
	m_pFragTrail = CSpriteTrail::SpriteTrailCreate( "sprites/laser.vmt", GetLocalOrigin(), false );

	if ( m_pFragTrail != NULL )
	{
		m_pFragTrail->SetParent( this );
		m_pFragTrail->SetTransparency( kRenderTransAdd, 70, 130, 245, 200, kRenderFxNone );
		m_pFragTrail->SetStartWidth( 8.0f );
		m_pFragTrail->SetLifeTime( 0.75f );
	}
#endif
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
#if 1
void CGrenadeEnergy::Shoot( CBaseEntity* pOwner, const Vector &vStart, const Vector &vVelocity, QAngle &vShootAng )
#else	// !OBSIDIAN
void CGrenadeEnergy::Shoot( CBaseEntity* pOwner, const Vector &vStart, Vector vVelocity )
#endif
{
	CGrenadeEnergy *pEnergy = (CGrenadeEnergy *)CreateEntityByName( "grenade_energy" );
	pEnergy->Spawn();
	
	UTIL_SetOrigin( pEnergy, vStart );
	pEnergy->SetAbsVelocity( vVelocity );
#if 1
	pEnergy->SetLocalAngles( vShootAng );
#endif
	pEnergy->SetOwnerEntity( pOwner );

#if 1
	pEnergy->SetThink ( &CGrenadeEnergy::Animate );
#else	// !OBSIDIAN
	pEnergy->SetThink ( Animate );
#endif
	pEnergy->SetNextThink( gpGlobals->curtime + 0.1f );

	pEnergy->m_nRenderMode = kRenderTransAdd;
#if 1
	pEnergy->SetRenderColor( 70, 130, 245, 200 );
#else
	pEnergy->SetRenderColor( 160, 160, 160, 255 );
#endif
	pEnergy->m_nRenderFX = kRenderFxNone;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeEnergy::Animate( void )
{
#if 0
	float flLifeLeft = 1-(gpGlobals->curtime  - m_flLaunchTime)/ENERGY_GRENADE_LIFETIME;

	if (flLifeLeft < 0)
	{
		SetRenderColorA( 0 );
		SetThink(NULL);
		UTIL_Remove(this);
	}

	SetNextThink( gpGlobals->curtime + 0.01f );

	QAngle angles;
	VectorAngles( GetAbsVelocity(), angles );
	SetLocalAngles( angles );

	SetNextThink( gpGlobals->curtime + 0.1f );

	StudioFrameAdvance( );

	SetRenderColorA( flLifeLeft );
#else
	SetNextThink( gpGlobals->curtime + 0.05f );

	// If I just went solid and my velocity is zero, it means I'm resting on
	// the floor already when I went solid so blow up
	if (GetAbsVelocity().Length() == 0.0 ||
			GetGroundEntity() != NULL )
		{
			Detonate();
		}

	// The old way of making danger sounds would scare the crap out of EVERYONE between you and where the grenade
	// was going to hit. The radius of the danger sound now 'blossoms' over the grenade's lifetime, making it seem
	// dangerous to a larger area downrange than it does from where it was fired.
	if( m_fDangerRadius <= ENERGY_GRENADE_MAX_DANGER_RADIUS )
	{
		m_fDangerRadius += ( ENERGY_GRENADE_MAX_DANGER_RADIUS * 0.05 );
	}

	CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * 0.5, m_fDangerRadius, 0.2, this, SOUNDENT_CHANNEL_REPEATED_DANGER );
#endif

}

void CGrenadeEnergy::Event_Killed( const CTakeDamageInfo &info )
{
	Detonate( );
}

void CGrenadeEnergy::GrenadeEnergyTouch( CBaseEntity *pOther )
{
#if 0 
	if ( pOther->m_takedamage )
	{
		float flLifeLeft = 1-(gpGlobals->curtime  - m_flLaunchTime)/ENERGY_GRENADE_LIFETIME;

		if ( pOther->GetFlags() & (FL_CLIENT) )
		{
			CBasePlayer *pPlayer = ( CBasePlayer * )pOther;
			float		flKick	 = 120 * flLifeLeft;
			pPlayer->m_Local.m_vecPunchAngle.SetX( flKick * (random->RandomInt(0,1) == 1) ? -1 : 1 );
			pPlayer->m_Local.m_vecPunchAngle.SetY( flKick * (random->RandomInt(0,1) == 1) ? -1 : 1 );
		}
		float flDamage = m_flDamage * flLifeLeft;
		if (flDamage < 1) 
		{
			flDamage = 1;
		}

		trace_t tr;
		tr = GetTouchTrace();
		CTakeDamageInfo info( this, GetThrower(), m_flDamage * flLifeLeft, DMG_SONIC );
		CalculateMeleeDamageForce( &info, (tr.endpos - tr.startpos), tr.endpos );
		pOther->TakeDamage( info );
	}
#else
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;
#endif
	Detonate();
}

void CGrenadeEnergy::Detonate(void)
{
	m_takedamage	= DAMAGE_NO;
#if 1
	UTIL_Remove( m_pFragSprite );
	m_pFragSprite = NULL;

	UTIL_Remove( m_pFragTrail );
	m_pFragTrail = NULL;

	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);

	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60 * vecForward, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	if ( ( tr.m_pEnt != GetWorldEntity() ) || ( tr.hitbox != 0 ) )
	{
		// non-world needs smaller decals
		if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
			UTIL_DecalTrace( &tr, "SmallScorch" );
	}
	else
		UTIL_DecalTrace( &tr, "Scorch" );

	CEffectData data;

	data.m_vOrigin = GetAbsOrigin();
	data.m_flRadius = m_DmgRadius * 0.5f;
	data.m_vNormal	= tr.plane.normal;
		
	DispatchEffect( "AR2Explosion", data );

	EmitSound( "Weapon_Mortar.Impact" );

	UTIL_ScreenShake( GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START );

	RadiusDamage ( CTakeDamageInfo( this, GetThrower(), m_flDamage, DMG_BLAST | DMG_DISSOLVE ), GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );

	CSoundEnt::InsertSound( SOUND_COMBAT | SOUND_CONTEXT_EXPLOSION, WorldSpaceCenter(), 180.0f, 0.25, this );
#endif
	UTIL_Remove( this );
}

void CGrenadeEnergy::Precache( void )
{
#if 1
	PrecacheMaterial( "effects/ar2ground2" );

	engine->PrecacheModel( "models/weapons/glueblob.mdl" );

#ifdef EZ2
	PrecacheParticleSystem( "energy_grenade_electrical_arc" );
#else
	PrecacheParticleSystem( "electrical_arc_01_system" );
#endif

	PrecacheModel( "sprites/light_glow03.vmt" );
	PrecacheModel( "sprites/laser.vmt" );

	PrecacheScriptSound( "Weapon_Mortar.Impact" );
#else
	PrecacheModel("Models/weapons/w_energy_grenade.mdl");
#endif
}
