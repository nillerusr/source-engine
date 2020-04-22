//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "mortar_round.h"
#include "engine/IEngineSound.h"
#include "tf_gamerules.h"
#include "tf_team.h"



// Damage CVars
ConVar	weapon_mortar_shell_damage( "weapon_mortar_shell_damage","0", FCVAR_NONE, "Mortar's standard shell maximum damage" );
ConVar	weapon_mortar_shell_radius( "weapon_mortar_shell_radius","0", FCVAR_NONE, "Mortar's standard shell splash radius" );
ConVar	weapon_mortar_starburst_damage( "weapon_mortar_starburst_damage","0", FCVAR_NONE, "Mortar's starburst maximum damage" );
ConVar	weapon_mortar_starburst_radius( "weapon_mortar_starburst_radius","0", FCVAR_NONE, "Mortar's starburst splash radius" );
ConVar	weapon_mortar_cluster_shells( "weapon_mortar_cluster_shells","0", FCVAR_NONE, "Number of shells a mortar cluster round bursts into" );

//=====================================================================================================
// MORTAR ROUND
//=====================================================================================================
BEGIN_DATADESC( CMortarRound )

	// Function Pointers
	DEFINE_FUNCTION( MissileTouch ),
	DEFINE_FUNCTION( FallThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( mortar_round, CMortarRound );
PRECACHE_WEAPON_REGISTER(mortar_round);

CMortarRound::CMortarRound()
{
	m_pSmokeTrail = NULL;
	m_pLauncher = NULL;
	m_iRoundType = MA_SHELL;
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMortarRound::Precache( void )
{
	PrecacheModel( "models/weapons/w_grenade.mdl" );

	PrecacheScriptSound( "MortarRound.StopSound" );
	PrecacheScriptSound( "MortarRound.IncomingSound" );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMortarRound::Spawn( void )
{
	Precache();

	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetSolid( SOLID_BBOX );
	SetModel( "models/weapons/w_grenade.mdl" );
	UTIL_SetSize( this, vec3_origin, vec3_origin );

	SetTouch( MissileTouch );
	SetCollisionGroup( TFCOLLISION_GROUP_WEAPON );

	// Trail smoke
	m_pSmokeTrail = SmokeTrail::CreateSmokeTrail();
	if ( m_pSmokeTrail )
	{
		m_pSmokeTrail->m_SpawnRate = 90;
		m_pSmokeTrail->m_ParticleLifetime = 1.5;
		m_pSmokeTrail->m_StartColor.Init(0.0, 0.0, 0.0);
		m_pSmokeTrail->m_EndColor.Init( 0.5,0.5,0.5 );
		m_pSmokeTrail->m_StartSize = 10;
		m_pSmokeTrail->m_EndSize = 50;
		m_pSmokeTrail->m_SpawnRadius = 1;
		m_pSmokeTrail->m_MinSpeed = 15;
		m_pSmokeTrail->m_MaxSpeed = 25;
		m_pSmokeTrail->SetLifetime(15);
		m_pSmokeTrail->FollowEntity( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMortarRound::MissileTouch( CBaseEntity *pOther )
{
	CMortarRound *pRound = dynamic_cast< CMortarRound* >(pOther);
	if ( pRound )
		return;

	// Stop emitting smoke/sound
	m_pSmokeTrail->SetEmit(false);
	EmitSound( "MortarRound.StopSound" );

	// Create an explosion.
	if ( m_iRoundType == MA_STARBURST )
	{
		// Small explosion, blind people in the area
		float flBlind = 0;

		// Shift it up a bit for the explosion
		SetLocalOrigin( GetAbsOrigin() + Vector(0,0,32) );
		CPASFilter filter( GetAbsOrigin() );
		te->Explosion( filter, 0.0, &GetAbsOrigin(), g_sModelIndexFireball, 3.0, 15, TE_EXPLFLAG_NONE, 512, 100 );
		RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), weapon_mortar_starburst_damage.GetFloat(), DMG_BLAST ), GetAbsOrigin(), weapon_mortar_starburst_radius.GetFloat(), CLASS_NONE, NULL );

		// Blind all players nearby
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseTFPlayer *pPlayer = ToBaseTFPlayer( UTIL_PlayerByIndex(i) );
			if ( pPlayer  )
			{
				Vector vecSrc = pPlayer->EyePosition();
				Vector vecToTarget = (vecSrc - GetAbsOrigin());
				float flLength = VectorNormalize( vecToTarget );
				// If the player's looking at the grenade, blind him a lot
				if ( flLength < 2048 )
				{
					Vector forward, right, up;
					AngleVectors( pPlayer->pl.v_angle, &forward, &right, &up );
					float flDot = DotProduct( vecToTarget, forward );

//					Msg( "Dot: %f\n", flDot );

					// Make sure it's in front of the player
					if ( flDot < 0.0f )
					{
						flDot = fabs( DotProduct(vecToTarget, right ) ) + fabs( DotProduct(vecToTarget, up ) );

						// Open LOS?
						trace_t tr;
						UTIL_TraceLine( vecSrc, GetAbsOrigin(), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
						if ( ( tr.fraction == 1.0f ) || ( tr.m_pEnt == pPlayer ) )
						{
							flBlind = flDot;
						}
						else
						{
							// Blocked blind is only half as effective
							flBlind = flDot * 0.5;
						}
					}
					else if ( flLength < 512 )
					{
						// Otherwise, if the player's near the grenade blind him a little
						flBlind = 0.2;
					}


					// Flash the screen red
					color32 white = {255,255,255, 255};
					white.a = MIN( (flBlind * 255), 255 );
					UTIL_ScreenFade( pPlayer, white, 0.3, 5.0, 0  );
				}
			}
		}

		UTIL_Remove( this );
	}
	else 
	{
		// Large explosion

		// Shift it up a bit for the explosion
		SetLocalOrigin( GetAbsOrigin() + Vector(0,0,64) );
		CPASFilter filter( GetAbsOrigin() );
		te->Explosion( filter, 0.0,	&GetAbsOrigin(), g_sModelIndexFireball, 10.0, 15, TE_EXPLFLAG_NONE, 512, 300 );
		RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), weapon_mortar_shell_damage.GetFloat(), DMG_BLAST ), GetAbsOrigin(), weapon_mortar_shell_radius.GetFloat(), CLASS_NONE, NULL );
		UTIL_Remove( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play a fall sound when the mortar round begins to fall
//-----------------------------------------------------------------------------
void CMortarRound::FallThink( void )
{
	if ( m_pLauncher )
	{
		CRecipientFilter filter;
		filter.AddAllPlayers();
		EmitSound( filter, entindex(), "MortarRound.IncomingSound" );

		// Cluster bombs split up in the air
		if ( m_iRoundType == MA_CLUSTER )
		{
			Vector forward, right;
			QAngle angles;
			VectorAngles( GetAbsVelocity(), angles );
			SetLocalAngles( angles );
			AngleVectors( GetLocalAngles(), &forward, &right, NULL );
			for ( int i = 0; i < weapon_mortar_cluster_shells.GetInt(); i++ )
			{
				Vector vecVelocity = GetAbsVelocity();
				vecVelocity += forward * random->RandomFloat( -200, 200 );
				vecVelocity += right * random->RandomFloat( -200, 200 );

				CMortarRound *pRound = CMortarRound::Create( GetAbsOrigin(), vecVelocity, GetOwnerEntity() ? GetOwnerEntity()->edict() : NULL );
				pRound->SetLauncher( m_pLauncher );
				pRound->ChangeTeam( GetTeamNumber() );
				pRound->m_iRoundType = MA_SHELL;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Keep a pointer to the Launcher
//-----------------------------------------------------------------------------
void CMortarRound::SetLauncher( CVehicleMortar *pLauncher )
{
	m_pLauncher = pLauncher;
}

//-----------------------------------------------------------------------------
// Purpose: Set the point at which we should start playing the fall sound
//-----------------------------------------------------------------------------
void CMortarRound::SetFallTime( float flFallTime )
{
	SetThink( FallThink );
	SetNextThink( gpGlobals->curtime + flFallTime );
}

//-----------------------------------------------------------------------------
// Purpose: Set the round's type
//-----------------------------------------------------------------------------
void CMortarRound::SetRoundType( int iRoundType )
{
	m_iRoundType = iRoundType;
}

//-----------------------------------------------------------------------------
// Purpose: Create a missile
//-----------------------------------------------------------------------------
CMortarRound* CMortarRound::Create( const Vector &vecOrigin, const Vector &vecVelocity, edict_t *pentOwner = NULL )
{
	CMortarRound *pGrenade = (CMortarRound*)CreateEntityByName("mortar_round");

	UTIL_SetOrigin( pGrenade, vecOrigin );
	pGrenade->SetOwnerEntity( Instance( pentOwner ) );
	pGrenade->Spawn();
	pGrenade->SetAbsVelocity( vecVelocity );
	QAngle angles;
	VectorAngles( vecVelocity, angles );
	pGrenade->SetLocalAngles( angles );

	return pGrenade;
}

