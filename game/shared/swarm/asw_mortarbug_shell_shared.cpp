#include "cbase.h"
#include "engine/IEngineSound.h"
#include "asw_shareddefs.h"
#include "basegrenade_shared.h"
#include "Sprite.h"
#include "asw_mortarbug_shell_shared.h"
#ifdef CLIENT_DLL
#include "c_asw_marine.h"
#include "particles_simple.h"
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#include "baseparticleentity.h"
#define CASW_Marine C_ASW_Marine
#else
#include "asw_marine.h"
#include "iasw_spawnable_npc.h"
#include "IEffects.h"
#include "te_effect_dispatch.h"
#include "world.h"
#include "asw_util_shared.h"
#include "particle_parse.h"
#include "asw_boomer_blob.h"
#endif
#include "asw_gamerules.h"
#include "util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SHELL_MODEL "models/swarm/MortarBugProjectile/MortarBugProjectile.mdl"
#define SHELL_AURA "mortar_shell_aura"
#define SHELL_WARN_DELAY 1.5f

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Mortarbug_Shell, DT_ASW_Mortarbug_Shell );

BEGIN_NETWORK_TABLE( CASW_Mortarbug_Shell, DT_ASW_Mortarbug_Shell )
END_NETWORK_TABLE()

#ifdef GAME_DLL

LINK_ENTITY_TO_CLASS( asw_mortarbug_shell, CASW_Mortarbug_Shell );
PRECACHE_REGISTER( asw_mortarbug_shell );

BEGIN_DATADESC( CASW_Mortarbug_Shell )
	DEFINE_FUNCTION( VShellTouch ),
	DEFINE_THINKFUNC( ShellFlyThink ),
	DEFINE_THINKFUNC( ShellThink ),
	DEFINE_THINKFUNC( Detonate ),
END_DATADESC()

extern int g_sModelIndexFireball;

ConVar asw_mortarbug_shell_gravity("asw_mortarbug_shell_gravity", "0.8f", FCVAR_CHEAT, "Gravity of mortarbug shell");
ConVar asw_mortarbug_shell_fuse("asw_mortarbug_shell_fuse", "3.0f", FCVAR_CHEAT, "Time before mortarbug shell explodes");

CASW_Mortarbug_Shell::CASW_Mortarbug_Shell()
{
	m_bDoScreenShake = false;
	
	g_aExplosiveProjectiles.AddToTail( this );
}

CASW_Mortarbug_Shell::~CASW_Mortarbug_Shell()
{
	g_aExplosiveProjectiles.FindAndRemove( this );
}

void CASW_Mortarbug_Shell::Precache( void )
{
	BaseClass::Precache( );

	PrecacheModel( SHELL_MODEL );
	PrecacheParticleSystem( SHELL_AURA );

	PrecacheScriptSound( "ASW_Boomer_Grenade.Explode" );
	PrecacheScriptSound( "ASW_Boomer_Projectile.Spawned" );
	PrecacheScriptSound( "ASW_Boomer_Projectile.ImpactHard" );
}

void CASW_Mortarbug_Shell::Spawn( void )
{
	Precache( );

	SetModel( SHELL_MODEL );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSequence( LookupSequence( "MortarBugProjectile_Closed" ) );

	// TODO: 
	m_flDamage		= 50;
	m_DmgRadius		= 220.0f;

	//Ignite(3.0, false, 0, false);

	m_takedamage	= DAMAGE_NO;

	m_bModelOpening = false;

	EmitSound( "ASW_Boomer_Projectile.Spawned" );

	SetSize( -Vector(4,4,4), Vector(4,4,4) );
	SetSolid( SOLID_BBOX );
	SetGravity( asw_mortarbug_shell_gravity.GetFloat() );

	SetCollisionGroup( COLLISION_GROUP_DEBRIS );	// TODO: change this to a custom collision group?
	SetTouch( &CASW_Mortarbug_Shell::VShellTouch );

	// TODO: Have a sound for the shell?
	//EmitSound( "ASWGrenade.Alarm" );  // 3 second warning sound

	m_vecLastPosition = vec3_origin;
	SetThink( &CASW_Mortarbug_Shell::ShellFlyThink );
	SetNextThink( gpGlobals->curtime + 0.05f );
}

void CASW_Mortarbug_Shell::ShellFlyThink()
{	
	// face the direction we're moving
	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);
	QAngle angles;
	VectorAngles( vecForward, angles );
	angles[PITCH] -= 90;		// TODO: rotate pitch properly?
	SetAbsAngles( angles );

	SetNextThink( gpGlobals->curtime + 0.05f );


	// check for not moving
	float flDist = ( GetAbsOrigin() - m_vecLastPosition ).Length();
	if ( flDist < 5.0f )
	{
		SetFuseLength( asw_mortarbug_shell_fuse.GetFloat() + RandomFloat( -0.3f, 0.3f ) );
	}
	m_vecLastPosition = GetAbsOrigin();
}

void CASW_Mortarbug_Shell::ShellThink( )
{	
	if ( !m_bModelOpening && gpGlobals->curtime >= (m_fDetonateTime - SHELL_WARN_DELAY) )
	{
		// we are one second from detonating, commit to detonating and start the opening sequence
		// regardless of anyone nearby
		m_bModelOpening = true;
		ResetSequence( LookupSequence( "MortarBugProjectile_Opening" ) );

		CEffectData	data;
		data.m_vOrigin = GetAbsOrigin();
		CPASFilter filter( data.m_vOrigin );
		filter.SetIgnorePredictionCull(true);
		DispatchParticleEffect( "mortar_grenade_open", PATTACH_ABSORIGIN_FOLLOW, this, -1, false, -1, &filter );
	}

	// if we exceeded the detonation time, just detonate
	if ( gpGlobals->curtime >= m_fDetonateTime )
	{
		Detonate();
		return;
	}

	// if the model is opening, do the animation advance
	if ( m_bModelOpening )
	{
		StudioFrameAdvance();
		SetNextThink( gpGlobals->curtime );
		return;
	}

	if ( m_fDetonateTime <= gpGlobals->curtime + 0.1f )
	{
		SetThink( &CASW_Mortarbug_Shell::Detonate );
		SetNextThink( m_fDetonateTime );
	}
	else
	{
		SetThink( &CASW_Mortarbug_Shell::ShellThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

void CASW_Mortarbug_Shell::VShellTouch( CBaseEntity *pOther )
{
	if (!pOther || pOther == GetOwnerEntity())
		return;

	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;

	// make sure we don't die on things we shouldn't
	if (!ASWGameRules() || !ASWGameRules()->ShouldCollide(GetCollisionGroup(), pOther->GetCollisionGroup()))
		return;

	// trace to latch onto the thing we collided with
	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60 * vecForward, MASK_SOLID, 
		this, GetCollisionGroup(), &tr);

	if ( !tr.DidHit() )
		return;

	QAngle angles;
	VectorAngles( tr.plane.normal, angles );
	angles[PITCH] += 90;		// TODO: rotate pitch properly?
	SetAbsAngles( angles );
	SetAbsVelocity( vec3_origin );
	UTIL_SetOrigin( this, tr.endpos );
	//SetParent( tr.m_pEnt );

	EmitSound( "ASW_Boomer_Projectile.ImpactHard" );

	SetTouch( NULL );
	
	SetFuseLength( asw_mortarbug_shell_fuse.GetFloat() + RandomFloat( -0.3f, 0.3f ) );
}

void CASW_Mortarbug_Shell::SetFuseLength(float fSeconds)
{
	m_fDetonateTime = gpGlobals->curtime + fSeconds;

	SetThink( &CASW_Mortarbug_Shell::ShellThink );
	SetNextThink( gpGlobals->curtime );

	//SetSequence( LookupSequence( "MortarBugProjectile_Opening" ) );
	//SetThink( &CASW_Mortarbug_Shell::Detonate );
	//SetNextThink( gpGlobals->curtime + fSeconds );
}

CASW_Mortarbug_Shell* CASW_Mortarbug_Shell::CreateShell( const Vector &vecOrigin, const Vector &vecForward, CBaseEntity *pOwner )
{
	CASW_Mortarbug_Shell *pShell = (CASW_Mortarbug_Shell *)CreateEntityByName( "asw_mortarbug_shell" );
	QAngle angles;
	VectorAngles( vecForward, angles );
	pShell->SetAbsAngles( angles );
	UTIL_SetOrigin( pShell, vecOrigin );
	pShell->Spawn();
	pShell->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pShell, vecOrigin );

	return pShell;
}

void CASW_Mortarbug_Shell::Detonate()
{
	m_takedamage	= DAMAGE_NO;	

	// explosion effects
	DispatchParticleEffect( "boomer_drop_explosion", GetAbsOrigin(), Vector( m_DmgRadius, 0.0f, 0.0f ), QAngle( 0.0f, 0.0f, 0.0f ) );
	EmitSound( "ASW_Boomer_Grenade.Explode" );

	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60*vecForward, MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);

	if ((tr.m_pEnt != GetWorldEntity()) || (tr.hitbox != 0))
	{
		// non-world needs smaller decals
		if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
		{
			UTIL_DecalTrace( &tr, "SmallScorch" );
		}
	}
	else
	{
		UTIL_DecalTrace( &tr, "Scorch" );
	}

	ScreenShake_t shake;
	shake.direction = Vector( 0, 0, 1 );
	shake.amplitude = 40.0f;
	shake.duration = 0.3f;
	shake.frequency = 1.0f;
	shake.command = SHAKE_START;
	UTIL_ASW_ScreenPunch( GetAbsOrigin(), 350.0f, shake );

	RadiusDamage ( CTakeDamageInfo( this, GetOwnerEntity(), m_flDamage, DMG_BLAST ), GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );

	UTIL_Remove( this );
}

#endif		// GAME_DLL



#ifdef CLIENT_DLL

CASW_Mortarbug_Shell::CASW_Mortarbug_Shell()
{
	
}

void CASW_Mortarbug_Shell::PostDataUpdate( DataUpdateType_t updateType )
{	
	BaseClass::PostDataUpdate(updateType);
	// If this entity was new, then latch in various values no matter what.
	if ( updateType == DATA_UPDATE_CREATED )
	{
		CNewParticleEffect	*pBurningEffect = ParticleProp()->Create( SHELL_AURA, PATTACH_ABSORIGIN_FOLLOW );
		if (pBurningEffect)
		{
		ParticleProp()->AddControlPoint( pBurningEffect, 1, this, PATTACH_ABSORIGIN_FOLLOW );
		pBurningEffect->SetControlPoint( 0, GetAbsOrigin() );
		pBurningEffect->SetControlPoint( 1, GetAbsOrigin() );
		pBurningEffect->SetControlPointEntity( 0, this );
		pBurningEffect->SetControlPointEntity( 1, this );
		}
	}
}

#endif      // CLIENT_DLL