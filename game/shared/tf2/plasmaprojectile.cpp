//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "plasmaprojectile.h"
//#include "smoke_trail.h"
#include "basecombatweapon_shared.h"
#include "tf_shareddefs.h"
#if !defined( CLIENT_DLL )
#include "tf_shield.h"
#else
#include "c_tracer.h"
#include "hud.h"
#include "view.h"
#include "c_te_effect_dispatch.h"
#endif
#include "IEffects.h"
//#include "tf_player.h"
#include "basetfplayer_shared.h"
#include "engine/IEngineSound.h"
#include "worldsize.h"
#include "tf_gamerules.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern ConVar tf_knockdowntime;

#define PLASMA_LIFETIME				2.0

// Time intervals at which we should simulate plasma projectiles
#define PLASMA_SIM_DELTA			0.01
#define PLASMA_VELOCITY_SQR			(PLASMA_VELOCITY*PLASMA_VELOCITY)

#if defined( CLIENT_DLL )
	ConVar	shot_width( "shot_width","8", 0, "Shot" );
	ConVar	shot_length( "shot_length","140", 0, "Shot" );
	ConVar	shot_head_size( "shot_head_size","6", 0, "Shot" );
#endif

#if !defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: PLASMA PROJECTILE
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CBasePlasmaProjectile )

	DEFINE_FIELD( m_flDamage, FIELD_FLOAT ),

	// Function Pointers
	DEFINE_ENTITYFUNC( MissileTouch ),

END_DATADESC()
#endif

BEGIN_NETWORK_TABLE_NOBASE( CPlasmaProjectileShared, DT_PlasmaProjectileShared )
#if !defined( CLIENT_DLL )
	// These are parameters that are used to generate the entire motion
	SendPropVector(SENDINFO(m_vecSpawnPosition), 0, SPROP_COORD),
	SendPropVector(SENDINFO(m_vTracerDir), 0, SPROP_NOSCALE), //SPROP_NORMAL),
	SendPropTime(SENDINFO(m_flSpawnTime)),
	SendPropTime(SENDINFO(m_flDeathTime)),
	SendPropFloat(SENDINFO(m_flSpawnSpeed), 0, SPROP_NOSCALE),
#else
	RecvPropVector(RECVINFO(m_vecSpawnPosition)),
	RecvPropVector(RECVINFO(m_vTracerDir)),
	RecvPropTime(RECVINFO(m_flSpawnTime)),
	RecvPropTime(RECVINFO(m_flDeathTime)),
	RecvPropFloat(RECVINFO(m_flSpawnSpeed)),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( CPlasmaProjectileShared )

	DEFINE_PRED_FIELD_TOL( m_vecSpawnPosition, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.125f ),
	DEFINE_PRED_FIELD_TOL( m_vTracerDir, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.01f ),
	DEFINE_PRED_FIELD( m_flSpawnTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flDeathTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flSpawnSpeed, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA_NO_BASE( PositionHistory_t )

	DEFINE_FIELD( m_Position, FIELD_VECTOR ),
	DEFINE_FIELD( m_Time, FIELD_FLOAT ),

END_PREDICTION_DATA()

IMPLEMENT_NETWORKCLASS_ALIASED( BasePlasmaProjectile, DT_BasePlasmaProjectile)

BEGIN_NETWORK_TABLE( CBasePlasmaProjectile, DT_BasePlasmaProjectile )
#if !defined( CLIENT_DLL )
	SendPropDataTable(SENDINFO_DT(m_Shared), &REFERENCE_SEND_TABLE(DT_PlasmaProjectileShared)),

	SendPropExclude( "DT_BaseEntity", "m_vecVelocity" ),
	SendPropExclude( "DT_BaseEntity", "m_vecAbsOrigin" ),

	//SendPropVector(SENDINFO(m_vecGunOriginOffset), 0, SPROP_COORD),

#else
	RecvPropDataTable(RECVINFO_DT(m_Shared), 0, &REFERENCE_RECV_TABLE(DT_PlasmaProjectileShared)),

	//RecvPropVector(RECVINFO(m_vecGunOriginOffset)),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( base_plasmaprojectile, CBasePlasmaProjectile );
PRECACHE_REGISTER(base_plasmaprojectile);

BEGIN_PREDICTION_DATA( CBasePlasmaProjectile )

	DEFINE_PRED_TYPEDESCRIPTION( m_Shared, CPlasmaProjectileShared ),

	DEFINE_PRED_FIELD( m_vecAbsOrigin, FIELD_VECTOR, FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE ),
	DEFINE_PRED_FIELD( m_vecVelocity, FIELD_VECTOR, FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE ),

	DEFINE_FIELD( m_flMaxRange, FIELD_FLOAT ),

	// Predicted, but not in networking stream
	DEFINE_PRED_TYPEDESCRIPTION( m_pPreviousPositions[0], PositionHistory_t ), 
	DEFINE_PRED_TYPEDESCRIPTION( m_pPreviousPositions[1], PositionHistory_t ), 
	DEFINE_PRED_TYPEDESCRIPTION( m_pPreviousPositions[2], PositionHistory_t ), 
	DEFINE_PRED_TYPEDESCRIPTION( m_pPreviousPositions[3], PositionHistory_t ), 
	DEFINE_PRED_TYPEDESCRIPTION( m_pPreviousPositions[4], PositionHistory_t ), 

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBasePlasmaProjectile::CBasePlasmaProjectile()
{
#if defined( CLIENT_DLL )
	m_pHeadParticle = NULL;
	m_pTrailParticle = NULL;
	m_pParticleMgr = NULL;
#endif

	SetPredictionEligible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBasePlasmaProjectile::~CBasePlasmaProjectile()
{
#if defined( CLIENT_DLL )
	if( m_pParticleMgr )
	{
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::Precache( void )
{
	SetCollisionGroup( TFCOLLISION_GROUP_WEAPON );

	PrecacheScriptSound( "BasePlasmaProjectile.ShieldBlock" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::Spawn( void )
{
	Precache();

	SetSolid( SOLID_BBOX );
	SetSize( vec3_origin, vec3_origin );
	SetCollisionGroup( TFCOLLISION_GROUP_WEAPON );
	SetTouch( MissileTouch );
	m_DamageType = DMG_ENERGYBEAM;
	SetMoveType( MOVETYPE_CUSTOM );
	m_flDamage = 0;
	// SetMaxRange( 0 );
	SetExplosive( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::Activate( void )
{
	BaseClass::Activate();

#if defined( CLIENT_DLL )
	if ( IsClientCreated() && !m_pParticleMgr )
	{
		Start(ParticleMgr(), NULL);
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::SetDamage( float flDamage )
{
	m_flDamage = flDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CBasePlasmaProjectile::GetDamage( void )
{
	return m_flDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::SetMaxRange( float flRange )
{
	m_flMaxRange = flRange;

	// If we have a max range, calculate death time based upon velocity
	if ( m_flMaxRange )
	{
		float flSpeed = GetAbsVelocity().Length();
		Assert( flSpeed );
		m_Shared.SetDeathTime( m_Shared.GetSpawnTime() + (flRange / flSpeed) );
	}
	else
	{
		m_Shared.SetDeathTime( m_Shared.GetSpawnTime() + PLASMA_LIFETIME );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the radius of the explosion created when this shot impacts
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::SetExplosive( float flRadius )
{
	m_flExplosiveRadius = flRadius;
}

//-----------------------------------------------------------------------------
// Perform custom physics on this dude (when we're in ballistic mode)
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::PerformCustomPhysics( Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity )
{
#ifdef CLIENT_DLL
	RecalculatePositions( pNewPosition, pNewVelocity, pNewAngles, pNewAngVelocity );
#else
	// Simulate next position
	m_Shared.ComputePosition( gpGlobals->curtime, pNewPosition, pNewVelocity, pNewAngles, pNewAngVelocity );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//			tr - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBasePlasmaProjectile::ProjectileHitShield( CBaseEntity *pOther, trace_t& tr )
{
	if ( !pOther )
		return false;

	if ( !pOther->IsPlayer() )
		return false;
#if !defined( CLIENT_DLL )
	CBaseTFPlayer* pPlayer = static_cast<CBaseTFPlayer*>(pOther);
	float flDamage = GetDamage();
	if ( !pPlayer->IsHittingShield( GetAbsVelocity(), &flDamage ) )
		return false;
#else
	return false;
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//			tr - 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::HandleShieldImpact( CBaseEntity *pOther, trace_t& tr )
{
	// Block
	EmitSound( "BasePlasmaProjectile.ShieldBlock" );

	// Remove the particle, and make a particle shower
	g_pEffects->EnergySplash( tr.endpos, tr.plane.normal, ( m_flExplosiveRadius != 0 ) );

	Remove( );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::MissileTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	// Create a plasma effect
	trace_t	tr;
	Vector velDir = GetAbsVelocity();
	VectorNormalize( velDir );
	Vector vecSpot = GetLocalOrigin() - velDir * 32;

	// First, just clip to the box
	Ray_t ray;
	ray.Init( vecSpot, vecSpot + velDir * 64 );
	enginetrace->ClipRayToEntity( ray, MASK_SHOT, pOther, &tr );

	// Create the appropriate impact
	bool bHurtTarget = ( !InSameTeam( pOther ) && pOther->m_takedamage != DAMAGE_NO );
	WeaponImpact( &tr, velDir, bHurtTarget, pOther, GetDamageType() );

#if !defined( CLIENT_DLL )
	CBaseEntity *pOwner = m_hOwner;

	// Do damage (unless I'm explosive, in which case I'll do damage later)
	if ( m_flDamage && !m_flExplosiveRadius )
	{
		ClearMultiDamage();
		// Assume it's a projectile, so use its velocity instead
		Vector vecDamageOrigin = GetAbsVelocity();
		VectorNormalize( vecDamageOrigin );
		vecDamageOrigin = GetAbsOrigin() - (vecDamageOrigin * 32);
		CTakeDamageInfo info( this, pOwner, m_flDamage, m_DamageType );
		CalculateBulletDamageForce( &info, GetAmmoDef()->Index("MediumRound"), GetAbsVelocity(), vecDamageOrigin );
		pOther->DispatchTraceAttack( info, velDir, &tr );
		ApplyMultiDamage();
	}
#endif

	Detonate();
}

//-----------------------------------------------------------------------------
// Purpose: Plasma projectiles return their owner as their scorer
//-----------------------------------------------------------------------------
CBasePlayer *CBasePlasmaProjectile::GetScorer( void )
{
	return ToBasePlayer( m_hOwner );
}

//-----------------------------------------------------------------------------
// Purpose: Explode and die
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::Detonate( void )
{
#if !defined( CLIENT_DLL )
	// Should I explode?
	if ( m_flExplosiveRadius )
	{
		RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), m_flDamage, m_DamageType | DMG_BLAST ), GetAbsOrigin(), m_flExplosiveRadius, CLASS_NONE, NULL );
	}
#endif
	Remove( );
}

#if defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
// Add the position to the history 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::AddPositionToHistory( const Vector& org, float flSimTime )
{
	// Store the particle position history
	// Push the others down the stack
	for ( int i = MAX_HISTORY-1; i >= 1; i-- )
	{
		m_pPreviousPositions[i].m_Position = m_pPreviousPositions[i-1].m_Position;
		m_pPreviousPositions[i].m_Time = m_pPreviousPositions[i-1].m_Time;
	}

	m_pPreviousPositions[0].m_Position = org;
	m_pPreviousPositions[0].m_Time = flSimTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::ResetPositionHistories( const Vector& org )
{
	for ( int i = 0; i < MAX_HISTORY; i++ )
	{
		m_pPreviousPositions[ i ].m_Position = org; //; - (m_Shared.TracerDir() * 48 * i);;
		m_pPreviousPositions[ i ].m_Time = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged(updateType);

	if ( updateType != DATA_UPDATE_CREATED )
		return;

	if ( !m_pParticleMgr )
	{
		Start(ParticleMgr(), NULL);
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::RecalculatePositions( Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity )
{
	// Recalculate all points?
	float flSimTime;
	if ( !m_pPreviousPositions[0].m_Time )
	{
		flSimTime = m_Shared.GetSpawnTime();
	}
	else
	{
		flSimTime = gpGlobals->curtime;
	}

	// Simulate the points
	for ( int i = 0; i < MAX_HISTORY; i++ )
	{
		if ( flSimTime < m_Shared.GetSpawnTime() )
		{
			flSimTime = m_Shared.GetSpawnTime();
		}

		Vector vecVelocity, vNewOrigin;
  		QAngle vecAngles, vecAngularVelocity;
		// Only fill out the data with the most recent sim
		if ( i == 0 )
		{
			m_Shared.ComputePosition( flSimTime, &vNewOrigin, &vecVelocity, pNewAngles, pNewAngVelocity );
			*pNewPosition = vNewOrigin;
			*pNewVelocity =vecVelocity;
		}
		else
		{
  			m_Shared.ComputePosition( flSimTime, &vNewOrigin, &vecVelocity, &vecAngles, &vecAngularVelocity );
		}
		AddPositionToHistory( vNewOrigin, flSimTime );

		// As we slow down, simulate slower
		float flSpeed = vecVelocity.LengthSqr();
		if ( flSpeed )
		{
			flSimTime -= PLASMA_SIM_DELTA * (PLASMA_VELOCITY_SQR / flSpeed);
		}
		else
		{
			flSimTime -= PLASMA_SIM_DELTA;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::ClientThink( void )
{
	BaseClass::ClientThink();

  	// Don't mess with origin if it's being forward simulated on the client
  	if ( GetPredictable() || IsClientCreated() )
  		return;

  	Assert( !GetMoveParent() );

	Vector pNewPosition, pNewVelocity;
	QAngle pNewAngles, pNewAngVelocity;
	RecalculatePositions( &pNewPosition, &pNewVelocity, &pNewAngles, &pNewAngVelocity );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : isbeingremoved - 
//			*predicted - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBasePlasmaProjectile::OnPredictedEntityRemove( bool isbeingremoved, C_BaseEntity *predicted )
{
	BaseClass::OnPredictedEntityRemove( isbeingremoved, predicted );

	CBasePlasmaProjectile *bpp = dynamic_cast< CBasePlasmaProjectile * >( predicted );
	if ( !bpp )
	{
		// Hrm, we didn't link up to correct type!!!
		Assert( 0 );
		// Delete right away since it's fucked up
		return true;
	}

	memcpy( m_pPreviousPositions, bpp->m_pPreviousPositions, sizeof( m_pPreviousPositions ) );

	m_vecGunOriginOffset = bpp->m_vecGunOriginOffset;

	// Don't delete right away
	return true; // isbeingremoved;
}

#define REMAP_BLEND_TIME		0.5f

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slot - 
//			curtime - 
//			outpos - 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::RemapPosition( Vector &vecStart, float curtime, Vector& outpos )
{
	outpos = vecStart;
	if ( curtime > m_Shared.GetSpawnTime() + REMAP_BLEND_TIME )
		return;

	float frac = ( curtime - m_Shared.GetSpawnTime() ) / REMAP_BLEND_TIME;
	frac = 1.0f - clamp( frac, 0.0f, 1.0f );

	Vector scaledOffset;
	VectorScale( m_vecGunOriginOffset, frac, scaledOffset );

	outpos += scaledOffset;
}

#define TIME_TILL_MAX_LENGTH	1.0

//-----------------------------------------------------------------------------
// Purpose: Update state + render
//-----------------------------------------------------------------------------
bool CBasePlasmaProjectile::SimulateAndRender(Particle *pInParticle, ParticleDraw *pDraw, float &sortKey)
{
	if ( IsDormantPredictable() )
		return true;

	if ( GetMoveType() == MOVETYPE_NONE )
		return true;

	// Update the particle position
	pInParticle->m_Pos = GetAbsOrigin();

	// Add our blended offset
	if ( gpGlobals->curtime < m_Shared.GetSpawnTime() + REMAP_BLEND_TIME )
	{
		float frac = ( gpGlobals->curtime - m_Shared.GetSpawnTime() ) / REMAP_BLEND_TIME;
		frac = 1.0f - clamp( frac, 0.0f, 1.0f );
		Vector scaledOffset;
		VectorScale( m_vecGunOriginOffset, frac, scaledOffset );
		pInParticle->m_Pos += scaledOffset;
	}

	float timeDelta = pDraw->GetTimeDelta();

	// Render the head particle
	if ( pInParticle == m_pHeadParticle )
	{
		SimpleParticle *pParticle = (SimpleParticle *) pInParticle;
		pParticle->m_flLifetime += timeDelta;

		// Render
		Vector tPos, vecOrigin;
		RemapPosition( m_pPreviousPositions[MAX_HISTORY-1].m_Position, m_pPreviousPositions[MAX_HISTORY-1].m_Time, vecOrigin );

		TransformParticle( ParticleMgr()->GetModelView(), vecOrigin, tPos );
		sortKey = (int) tPos.z;

		//Render it
		RenderParticle_ColorSizeAngle(
			pDraw,
			tPos,
			UpdateColor( pParticle, timeDelta ),
			UpdateAlpha( pParticle, timeDelta ) * GetAlphaDistanceFade( tPos, 16, 64 ),
			UpdateScale( pParticle, timeDelta ),
			UpdateRoll( pParticle, timeDelta ) );

		/*
		if ( m_flNextSparkEffect < gpGlobals->curtime )
		{
			// Drop sparks?
			if ( GetTeamNumber() == TEAM_HUMANS )
			{
				g_pEffects->Sparks( pInParticle->m_Pos, 1, 3 );
			}
			else
			{
				g_pEffects->EnergySplash( pInParticle->m_Pos, vec3_origin );
			}
			m_flNextSparkEffect = gpGlobals->curtime + RandomFloat( 0.5, 2 );
		}
		*/

		return true;
	}

	// Render the trail
	TrailParticle *pParticle = (TrailParticle *) pInParticle;
	pParticle->m_flLifetime += timeDelta;
	Vector vecScreenStart, vecScreenDelta;
	sortKey = pParticle->m_Pos.z;

	// NOTE: We need to do everything in screen space
	float flFragmentLength = (MAX_HISTORY > 1) ? 1.0 / (float)(MAX_HISTORY-1) : 1.0;

	for ( int i = 0; i < (MAX_HISTORY-1); i++ )
	{
		Vector vecWorldStart, vecWorldEnd, vecScreenEnd;
		float flStartV, flEndV;

		// Did we just appear?
		if ( m_pPreviousPositions[i].m_Time == 0 )
			continue;

		RemapPosition( m_pPreviousPositions[i+1].m_Position, m_pPreviousPositions[i+1].m_Time, vecWorldStart );
		RemapPosition( m_pPreviousPositions[i].m_Position, m_pPreviousPositions[i].m_Time, vecWorldEnd );

		// Texture wrapping
		flStartV = (flFragmentLength * (i+1));
		flEndV = (flFragmentLength * i);
		
		TransformParticle( ParticleMgr()->GetModelView(), vecWorldStart, vecScreenStart );
		TransformParticle( ParticleMgr()->GetModelView(), vecWorldEnd, vecScreenEnd );
		Vector vecScreenDelta = (vecScreenEnd - vecScreenStart);
		if ( vecScreenDelta == vec3_origin )
			continue;

		/*
		Vector vecForward, vecRight;
		AngleVectors( MainViewAngles(), &vecForward, &vecRight, NULL );
		Vector vecWorldDelta = ( vecWorldEnd - vecWorldStart );
		VectorNormalize( vecWorldDelta );
		float flDot = fabs(DotProduct( vecWorldDelta, vecForward ));
		if ( flDot > 0.99 )
		{
			// Remap alpha
			pParticle->m_flColor[3] = 1.0 - MIN( 1.0, RemapVal( flDot, 0.99, 1.0, 0, 1 ) );
		}
		*/

		// See if we should fade
		float color[4];
		Color32ToFloat4( color, pParticle->m_color );
		Tracer_Draw( pDraw, vecScreenStart, vecScreenDelta, pParticle->m_flWidth, color, flStartV, flEndV );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs)
{
	m_pParticleMgr = pParticleMgr;
	m_pParticleMgr->AddEffect( &m_ParticleEffect, this );

	PMaterialHandle	HeadMaterial, TrailMaterial;

	// Load the projectile material
	if ( GetTeamNumber() == TEAM_HUMANS )
	{
		HeadMaterial = m_ParticleEffect.FindOrAddMaterial( "effects/human_tracers/human_sparksprite_A1" );
		TrailMaterial = m_ParticleEffect.FindOrAddMaterial( "effects/human_tracers/human_sparktracer_A_" );
	}
	else
	{
		HeadMaterial = m_ParticleEffect.FindOrAddMaterial( "effects/alien_tracers/alien_pbsprite_A1" );
		TrailMaterial = m_ParticleEffect.FindOrAddMaterial( "effects/alien_tracers/alien_pbtracer_A_" );
	}

	// Create the head & trail
	m_pHeadParticle = (SimpleParticle *)m_ParticleEffect.AddParticle(sizeof(SimpleParticle), HeadMaterial );
	m_pTrailParticle = (TrailParticle *)m_ParticleEffect.AddParticle(sizeof(TrailParticle), TrailMaterial );
	if ( !m_pHeadParticle || !m_pTrailParticle )
		return;

	// 3rd person particles are larger
	bool bFirst = (GetOwnerEntity() == C_BasePlayer::GetLocalPlayer());

	m_pHeadParticle->m_Pos = GetRenderOrigin();
	m_pHeadParticle->m_uchColor[0] = 255;
	m_pHeadParticle->m_uchColor[1] = 255;
	m_pHeadParticle->m_uchColor[2] = 255;
	if ( bFirst )
	{
		m_pHeadParticle->m_uchStartSize = 6;
	}
	else
	{
		m_pHeadParticle->m_uchStartSize = shot_head_size.GetInt();
	}
	m_pHeadParticle->m_uchEndSize	= m_pHeadParticle->m_uchStartSize;
	m_pHeadParticle->m_uchStartAlpha = 255;
	m_pHeadParticle->m_uchEndAlpha = 255;
	m_pHeadParticle->m_flRoll = 0;
	m_pHeadParticle->m_flRollDelta = 10;
	m_pHeadParticle->m_iFlags = 0;

	m_pTrailParticle->m_flLifetime = 0;
	m_pTrailParticle->m_Pos = GetRenderOrigin();
	if ( bFirst )
	{
		m_pTrailParticle->m_flWidth = 25;
		m_pTrailParticle->m_flLength = 140;
	}
	else
	{
		m_pTrailParticle->m_flWidth = shot_width.GetFloat();
		m_pTrailParticle->m_flLength = shot_length.GetFloat();
	}
	Color32Init( m_pTrailParticle->m_color, 255, 255, 255, 255 );

	m_flNextSparkEffect = gpGlobals->curtime + RandomFloat( 0.05, 0.4 );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Setup this projectile's starting values
//-----------------------------------------------------------------------------
void CBasePlasmaProjectile::SetupProjectile( const Vector &vecOrigin, const Vector &vecForward, int damageType, CBaseEntity *pOwner )
{
	UTIL_SetOrigin( this, vecOrigin );

	QAngle angles;
	VectorAngles( vecForward, angles );
	SetLocalAngles( angles );

	SetOwnerEntity( pOwner );
	Spawn();

	float flMySpeed = PLASMA_VELOCITY;// + RandomFloat( -500, 500 );
	SetAbsVelocity( vecForward * flMySpeed );
	m_DamageType = damageType;
	m_Shared.Init( vecOrigin, vecForward, flMySpeed );
#ifdef CLIENT_DLL
	ResetPositionHistories( GetAbsOrigin() );
#endif
	m_Shared.SetSpawnTime( gpGlobals->curtime );

	// Set my team
	ChangeTeam( pOwner->GetTeamNumber() );
}

//-----------------------------------------------------------------------------
// Purpose: Create a missile
//-----------------------------------------------------------------------------
CBasePlasmaProjectile *CBasePlasmaProjectile::Create( const Vector &vecOrigin, const Vector &vecForward, int damageType, CBaseEntity *pOwner = NULL )
{
	CBasePlasmaProjectile *pMissile = (CBasePlasmaProjectile*)CreateEntityByName("base_plasmaprojectile");
	pMissile->SetupProjectile( vecOrigin, vecForward, damageType, pOwner );

	return pMissile;
}

CBasePlasmaProjectile *CBasePlasmaProjectile::CreatePredicted( const Vector &vecOrigin, const Vector &vecForward, const Vector& gunOffset, int damageType, CBasePlayer *pOwner )
{
	CBasePlasmaProjectile *pMissile = (CBasePlasmaProjectile*)CREATE_PREDICTED_ENTITY("base_plasmaprojectile");
	if ( pMissile )
	{
		pMissile->SetOwnerEntity( pOwner );
		pMissile->SetPlayerSimulated( pOwner );
		pMissile->SetupProjectile( vecOrigin, vecForward, damageType, pOwner );
		pMissile->m_vecGunOriginOffset = gunOffset;
	}

	return pMissile;
}

//===============================================================================================================
// Power Projectile
//===============================================================================================================
//-----------------------------------------------------------------------------
// Purpose: Create a power projectile
//-----------------------------------------------------------------------------
CPowerPlasmaProjectile *CPowerPlasmaProjectile::Create( const Vector &vecOrigin, const Vector &vecForward, int damageType, CBaseEntity *pOwner = NULL )
{
	CPowerPlasmaProjectile *pMissile = (CPowerPlasmaProjectile*)CreateEntityByName("powerplasmaprojectile");
	pMissile->SetupProjectile( vecOrigin, vecForward, damageType, pOwner );
	pMissile->SetPower( 1.0 );

	return pMissile;
}

CPowerPlasmaProjectile *CPowerPlasmaProjectile::CreatePredicted( const Vector &vecOrigin, const Vector &vecForward, const Vector& gunOffset, int damageType, CBasePlayer *pOwner )
{
	CPowerPlasmaProjectile *pMissile = (CPowerPlasmaProjectile*)CREATE_PREDICTED_ENTITY("powerplasmaprojectile");
	if ( pMissile )
	{
		pMissile->SetOwnerEntity( pOwner );
		pMissile->SetPlayerSimulated( pOwner );
		pMissile->SetupProjectile( vecOrigin, vecForward, damageType, pOwner );
		pMissile->SetPower( 1.0 );
		pMissile->m_vecGunOriginOffset = gunOffset;
	}

	return pMissile;
}

CPowerPlasmaProjectile::CPowerPlasmaProjectile( void )
{
	m_flPower = 0;
	SetPredictionEligible( true );
}

//-----------------------------------------------------------------------------
// Purpose: Factor power into size
//-----------------------------------------------------------------------------
float CPowerPlasmaProjectile::GetSize( void )
{
	return ( 2 * (m_flPower * 2));
}

IMPLEMENT_NETWORKCLASS_ALIASED( PowerPlasmaProjectile, DT_PowerPlasmaProjectile);

BEGIN_NETWORK_TABLE( CPowerPlasmaProjectile, DT_PowerPlasmaProjectile)
#if !defined( CLIENT_DLL )
	SendPropFloat( SENDINFO( m_flPower ), 7, SPROP_ROUNDDOWN, 1.0f, 10.0 ),
#else
	RecvPropFloat(RECVINFO(m_flPower)),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( powerplasmaprojectile, CPowerPlasmaProjectile );
PRECACHE_REGISTER(powerplasmaprojectile);

BEGIN_PREDICTION_DATA( CPowerPlasmaProjectile )

	DEFINE_PRED_FIELD_TOL( m_flPower, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.05f ),

END_PREDICTION_DATA()
