//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot from the MP5 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "soundent.h"
#include "engine/IEngineSound.h"
#include "ai_senses.h"
#include "hl1_npc_snark.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"


ConVar sk_snark_health		( "sk_snark_health",				"0" );
ConVar sk_snark_dmg_bite	( "sk_snark_dmg_bite",				"0" );
ConVar sk_snark_dmg_pop		( "sk_snark_dmg_pop",				"0" );


LINK_ENTITY_TO_CLASS( monster_snark, CSnark);


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CSnark )
	DEFINE_FIELD( m_flDie, FIELD_TIME ),
	DEFINE_FIELD( m_vecTarget, FIELD_VECTOR ),
	DEFINE_FIELD( m_flNextHunt, FIELD_TIME ),
	DEFINE_FIELD( m_flNextHit, FIELD_TIME ),
	DEFINE_FIELD( m_posPrev, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iMyClass, FIELD_INTEGER ),

	DEFINE_ENTITYFUNC( SuperBounceTouch ),
	DEFINE_THINKFUNC( HuntThink ),
END_DATADESC()


#define SQUEEK_DETONATE_DELAY	15.0
#define SNARK_EXPLOSION_VOLUME	512


enum w_squeak_e {
	WSQUEAK_IDLE1 = 0,
	WSQUEAK_FIDGET,
	WSQUEAK_JUMP,
	WSQUEAK_RUN,
};

float CSnark::m_flNextBounceSoundTime = 0;

void CSnark::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( "models/w_squeak2.mdl" );

	PrecacheScriptSound( "Snark.Die" );
	PrecacheScriptSound( "Snark.Gibbed" );
	PrecacheScriptSound( "Snark.Squeak" );
	PrecacheScriptSound( "Snark.Deploy" );
	PrecacheScriptSound( "Snark.Bounce" );

}


void CSnark::Spawn( void )
{
	Precache();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	SetFriction(1.0);	

	SetModel( "models/w_squeak2.mdl" );
	UTIL_SetSize( this, Vector( -4, -4, 0 ), Vector( 4, 4, 8 ) );

	SetBloodColor( BLOOD_COLOR_YELLOW );

	SetTouch( &CSnark::SuperBounceTouch );
	SetThink( &CSnark::HuntThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	m_flNextHit				= gpGlobals->curtime;
	m_flNextHunt			= gpGlobals->curtime + 1E6;
	m_flNextBounceSoundTime	= gpGlobals->curtime;

	AddFlag( FL_AIMTARGET | FL_NPC );
	m_takedamage = DAMAGE_YES;

	m_iHealth		= sk_snark_health.GetFloat();
	m_iMaxHealth	= m_iHealth;

	SetGravity( UTIL_ScaleForGravity( 400 ) );	// use a lower gravity for snarks
	SetFriction( 0.5 );

	SetDamage( sk_snark_dmg_pop.GetFloat() );

	m_flDie = gpGlobals->curtime + SQUEEK_DETONATE_DELAY;

	m_flFieldOfView = 0; // 180 degrees

	if ( GetOwnerEntity() )
		m_hOwner = GetOwnerEntity();

	m_flNextBounceSoundTime = gpGlobals->curtime;// reset each time a snark is spawned.

	SetSequence( WSQUEAK_RUN );
	ResetSequenceInfo( );

	m_iMyClass = CLASS_NONE;

	m_posPrev = Vector( 0, 0, 0 );
}


Class_T	CSnark::Classify( void )
{
	if ( m_iMyClass != CLASS_NONE )
		return m_iMyClass; // protect against recursion

	if ( GetEnemy() != NULL )
	{
		m_iMyClass = CLASS_INSECT; // no one cares about it
		switch( GetEnemy()->Classify( ) )
		{
			case CLASS_PLAYER:
			case CLASS_HUMAN_PASSIVE:
			case CLASS_HUMAN_MILITARY:
				m_iMyClass = CLASS_NONE;
				return CLASS_ALIEN_MILITARY; // barney's get mad, grunts get mad at it
		}
		m_iMyClass = CLASS_NONE;
	}

	return CLASS_ALIEN_BIOWEAPON;
}


void CSnark::Event_Killed( const CTakeDamageInfo &inputInfo )
{
//	pev->model = iStringNull;// make invisible
	SetThink( &CSnark::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 0.1f );
	SetTouch( NULL );

	// since squeak grenades never leave a body behind, clear out their takedamage now.
	// Squeaks do a bit of radius damage when they pop, and that radius damage will
	// continue to call this function unless we acknowledge the Squeak's death now. (sjb)
	m_takedamage = DAMAGE_NO;

	// play squeek blast
	CPASAttenuationFilter filter( this, 0.5 );
	EmitSound( filter, entindex(), "Snark.Die" );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SNARK_EXPLOSION_VOLUME, 3.0 );

	UTIL_BloodDrips( WorldSpaceCenter(), Vector( 0, 0, 0 ), BLOOD_COLOR_YELLOW, 80 );

	if ( m_hOwner != NULL )
	{
		RadiusDamage( CTakeDamageInfo( this, m_hOwner, GetDamage(), DMG_BLAST ), GetAbsOrigin(), GetDamage() * 2.5, CLASS_NONE, NULL );
	}
	else
	{
		RadiusDamage( CTakeDamageInfo( this, this, GetDamage(), DMG_BLAST ), GetAbsOrigin(), GetDamage() * 2.5, CLASS_NONE, NULL );
	}

	// reset owner so death message happens
	if ( m_hOwner != NULL )
		SetOwnerEntity( m_hOwner );

	CTakeDamageInfo info = inputInfo;
	int iGibDamage = g_pGameRules->Damage_GetShouldGibCorpse();
	info.SetDamageType( iGibDamage );

	BaseClass::Event_Killed( info );
}


bool CSnark::Event_Gibbed( const CTakeDamageInfo &info )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Snark.Gibbed" );

	return BaseClass::Event_Gibbed( info );
}


void CSnark::HuntThink( void )
{
	if (!IsInWorld())
	{
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}
	
	StudioFrameAdvance( );
	SetNextThink( gpGlobals->curtime + 0.1f );
	
	//FIXME: There's a problem in this movetype that causes it to set a ground entity but never recheck to clear it
	//		 For now, we stomp it clear and force it to revalidate -- jdw

	SetGroundEntity( NULL );
	PhysicsStepRecheckGround();

	// explode when ready
	if ( gpGlobals->curtime >= m_flDie )
	{
		g_vecAttackDir = GetAbsVelocity();
		VectorNormalize( g_vecAttackDir );
		m_iHealth = -1;
		CTakeDamageInfo	info( this, this, 1, DMG_GENERIC );
		Event_Killed( info );
		return;
	}

	// float
	if ( GetWaterLevel() != 0)
	{
		if ( GetMoveType() == MOVETYPE_FLYGRAVITY )
		{
			SetMoveType( MOVETYPE_FLY, MOVECOLLIDE_FLY_CUSTOM );
		}

		Vector vecVel = GetAbsVelocity();
		vecVel *= 0.9;
		vecVel.z += 8.0;
		SetAbsVelocity( vecVel );
	}
	else if ( GetMoveType() == MOVETYPE_FLY )
	{
		SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	}

	// return if not time to hunt
	if ( m_flNextHunt > gpGlobals->curtime )
		return;

	m_flNextHunt = gpGlobals->curtime + 2.0;
	
	Vector vecFlat = GetAbsVelocity();
	vecFlat.z = 0;
	VectorNormalize( vecFlat );

	if ( GetEnemy() == NULL || !GetEnemy()->IsAlive() )
	{
		// find target, bounce a bit towards it.
		GetSenses()->Look( 1024 );
		SetEnemy( BestEnemy() );
	}

	// squeek if it's about time blow up
	if ( (m_flDie - gpGlobals->curtime <= 0.5) && (m_flDie - gpGlobals->curtime >= 0.3) )
	{
		CPASAttenuationFilter filter( this );
		EmitSound( filter, entindex(), "Snark.Squeak" );
		CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 256, 0.25 );
	}

	// higher pitch as squeeker gets closer to detonation time
	float flpitch = 155.0 - 60.0 * ( (m_flDie - gpGlobals->curtime) / SQUEEK_DETONATE_DELAY );
	if ( flpitch < 80 )
		flpitch = 80;

	if ( GetEnemy() != NULL )
	{
		if ( FVisible( GetEnemy() ) )
		{
			m_vecTarget = GetEnemy()->EyePosition() - GetAbsOrigin();
			VectorNormalize( m_vecTarget );
		}

		float flVel = GetAbsVelocity().Length();
		float flAdj = 50.0 / ( flVel + 10.0 );

		if ( flAdj > 1.2 )
			flAdj = 1.2;
		
		// ALERT( at_console, "think : enemy\n");

		// ALERT( at_console, "%.0f %.2f %.2f %.2f\n", flVel, m_vecTarget.x, m_vecTarget.y, m_vecTarget.z );

		SetAbsVelocity( GetAbsVelocity() * flAdj + (m_vecTarget * 300) );
	}

	if ( GetFlags() & FL_ONGROUND )
	{
		SetLocalAngularVelocity( QAngle( 0, 0, 0 ) );
	}
	else
	{
		QAngle angVel = GetLocalAngularVelocity();
		if ( angVel == QAngle( 0, 0, 0 ) )
		{
			angVel.x = random->RandomFloat( -100, 100 );
			angVel.z = random->RandomFloat( -100, 100 );
			SetLocalAngularVelocity( angVel );
		}
	}

	if ( ( GetAbsOrigin() - m_posPrev ).Length() < 1.0 )
	{
		Vector vecVel = GetAbsVelocity();
		vecVel.x = random->RandomFloat( -100, 100 );
		vecVel.y = random->RandomFloat( -100, 100 );
		SetAbsVelocity( vecVel );
	}

	m_posPrev = GetAbsOrigin();

	QAngle angles;
	VectorAngles( GetAbsVelocity(), angles );
	angles.z = 0;
	angles.x = 0;
	SetAbsAngles( angles );
}

unsigned int CSnark::PhysicsSolidMaskForEntity( void ) const
{
	unsigned int iMask = BaseClass::PhysicsSolidMaskForEntity();

	iMask &= ~CONTENTS_MONSTERCLIP;

	return iMask;
}


// Custom collision that provides a good bounce when we hit walls
// and also gives gravity velocity so the snarks fall off of edges.
void CSnark::ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity )
{
	// Get the impact surface's friction.
	float flSurfaceFriction;
	physprops->GetPhysicsProperties( trace.surface.surfaceProps, NULL, NULL, &flSurfaceFriction, NULL );

	Vector vecAbsVelocity = GetAbsVelocity();

	// If we hit a wall
	if ( trace.plane.normal.z <= 0.7 )			// Floor
	{
		Vector vecDir = vecAbsVelocity;

		float speed = vecDir.Length();

		VectorNormalize( vecDir );

		float hitDot = DotProduct( trace.plane.normal, -vecDir );
			
		Vector vReflection = 2.0f * trace.plane.normal * hitDot + vecDir;
		
		SetAbsVelocity( vReflection * speed * 0.6f );

		return;
	}

	// Stop if on ground.
	// Get the total velocity (player + conveyors, etc.)
	VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecVelocity );
	float flSpeedSqr = DotProduct( vecVelocity, vecVelocity );

	// Verify that we have an entity.
	CBaseEntity *pEntity = trace.m_pEnt;
	Assert( pEntity );

	if ( vecVelocity.z < ( 800 * gpGlobals->frametime ) )
	{
		vecAbsVelocity.z = 0.0f;

		// Recompute speedsqr based on the new absvel
		VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecVelocity );
		flSpeedSqr = DotProduct( vecVelocity, vecVelocity );
	}
	SetAbsVelocity( vecAbsVelocity );

	if ( flSpeedSqr < ( 30 * 30 ) )
	{
		if ( pEntity->IsStandable() )
		{
			SetGroundEntity( pEntity );
		}

		// Reset velocities.
		SetAbsVelocity( vec3_origin );
		SetLocalAngularVelocity( vec3_angle );
	}
	else
	{
		vecAbsVelocity += GetBaseVelocity();
		vecAbsVelocity *= ( 1.0f - trace.fraction ) * gpGlobals->frametime * flSurfaceFriction;
		PhysicsPushEntity( vecAbsVelocity, &trace );
	}
}

void CSnark::SuperBounceTouch( CBaseEntity *pOther )
{
	float	flpitch;
	trace_t tr;
	tr = CBaseEntity::GetTouchTrace( );

	// don't hit the guy that launched this grenade
	if ( GetOwnerEntity() && ( pOther == GetOwnerEntity() ) )
		return;

	// at least until we've bounced once
	SetOwnerEntity( NULL );

	QAngle angles = GetAbsAngles();
	angles.x = 0;
	angles.z = 0;
	SetAbsAngles( angles );

	// avoid bouncing too much
	if ( m_flNextHit > gpGlobals->curtime) 
		return;

	// higher pitch as squeeker gets closer to detonation time
	flpitch = 155.0 - 60.0 * ( ( m_flDie - gpGlobals->curtime ) / SQUEEK_DETONATE_DELAY );

	if ( pOther->m_takedamage && m_flNextAttack < gpGlobals->curtime )
	{
		// attack!

		// make sure it's me who has touched them
		if ( tr.m_pEnt == pOther )
		{
			// and it's not another squeakgrenade
			if ( tr.m_pEnt->GetModelIndex() != GetModelIndex() )
			{
				// ALERT( at_console, "hit enemy\n");
				ClearMultiDamage();

				Vector vecForward;
				AngleVectors( GetAbsAngles(), &vecForward );

				if ( m_hOwner != NULL )
				{
					CTakeDamageInfo info( this, m_hOwner, sk_snark_dmg_bite.GetFloat(), DMG_SLASH );
					CalculateMeleeDamageForce( &info, vecForward, tr.endpos );
					pOther->DispatchTraceAttack( info, vecForward, &tr );
				}
				else
				{
					CTakeDamageInfo info( this, this, sk_snark_dmg_bite.GetFloat(), DMG_SLASH );
					CalculateMeleeDamageForce( &info, vecForward, tr.endpos );
					pOther->DispatchTraceAttack( info, vecForward, &tr );
				}

				ApplyMultiDamage();

				SetDamage( GetDamage() + sk_snark_dmg_pop.GetFloat() ); // add more explosion damage
				// m_flDie += 2.0; // add more life

				// make bite sound
				CPASAttenuationFilter filter( this );
				CSoundParameters params;
				if ( GetParametersForSound( "Snark.Deploy", params, NULL ) )
				{
					EmitSound_t ep( params );
					ep.m_nPitch = (int)flpitch;

					EmitSound( filter, entindex(), ep );
				}
				m_flNextAttack = gpGlobals->curtime + 0.5;
			}
		}
		else
		{
			// ALERT( at_console, "been hit\n");
		}
	}

	m_flNextHit = gpGlobals->curtime + 0.1;
	m_flNextHunt = gpGlobals->curtime;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// in multiplayer, we limit how often snarks can make their bounce sounds to prevent overflows.
		if ( gpGlobals->curtime < m_flNextBounceSoundTime )
		{
			// too soon!
			return;
		}
	}

	if ( !( GetFlags() & FL_ONGROUND ) )
	{
		// play bounce sound
		CPASAttenuationFilter filter2( this );

		CSoundParameters params;
		if ( GetParametersForSound( "Snark.Bounce", params, NULL ) )
		{
			EmitSound_t ep( params );
			ep.m_nPitch = (int)flpitch;

			EmitSound( filter2, entindex(), ep );
		}

		CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 256, 0.25 );
	}
	else
	{
		// skittering sound
		CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 100, 0.1 );
	}

	m_flNextBounceSoundTime = gpGlobals->curtime + 0.5;// half second.
}


bool CSnark::IsValidEnemy( CBaseEntity *pEnemy )
{
	return CHL1BaseNPC::IsValidEnemy( pEnemy );
}

