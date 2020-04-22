//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sapper's draining beam
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "in_buttons.h"
#include "weapon_combatshield.h"
#include "engine/IEngineSound.h"
#include "grenade_base_empable.h"

#if defined( CLIENT_DLL )

#include "particles_simple.h"

#else

#include "tf_gamerules.h"
#include "tf_class_sapper.h"

#endif

#include "weapon_drainbeam.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define PARTICLE_PATH_VEL				140.0
#define NUM_PATH_PARTICLES_PER_SEC		600.0f
#define NUM_DRIBBLE_PARTICLES_PER_SEC	20.0f

#define NUM_DRAINBEAM_PATH_POINTS		8

// Buff ranges
static ConVar weapon_drainbeam_target_range( "weapon_drainbeam_target_range", "900", FCVAR_REPLICATED, "The farthest away you can be for the drain gun to initially lock onto a target." );
static ConVar weapon_drainbeam_stick_range( "weapon_drainbeam_stick_range", "612", FCVAR_REPLICATED, "How far away the drain gun can stay locked onto a target." );
static ConVar weapon_drainbeam_max_rate( "weapon_drainbeam_max_rate", "10", FCVAR_REPLICATED );
static ConVar weapon_drainbeam_max_rate_time( "weapon_drainbeam_max_rate_time", "5", FCVAR_REPLICATED );
static ConVar weapon_drainbeam_emp_time_factor( "weapon_drainbeam_emp_time_factor", "0.75", FCVAR_REPLICATED );


LINK_ENTITY_TO_CLASS( weapon_drainbeam, CWeaponDrainBeam );

PRECACHE_WEAPON_REGISTER( weapon_drainbeam );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponDrainBeam, DT_WeaponDrainBeam )

BEGIN_NETWORK_TABLE( CWeaponDrainBeam, DT_WeaponDrainBeam )
#if !defined( CLIENT_DLL )
	SendPropInt( SENDINFO( m_bDraining ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bAttacking ), 1, SPROP_UNSIGNED ),
	SendPropVector( SENDINFO( m_vFireTarget ), 0, SPROP_COORD ),
	SendPropEHandle( SENDINFO( m_hDrainTarget ) ),
#else
	RecvPropInt( RECVINFO( m_bAttacking ) ),
	RecvPropInt( RECVINFO( m_bDraining ) ),
	RecvPropVector( RECVINFO(m_vFireTarget) ),
	RecvPropEHandle( RECVINFO(m_hDrainTarget) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponDrainBeam  )

	DEFINE_PRED_FIELD( m_bDraining, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bAttacking, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_vFireTarget, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.125f ),

	DEFINE_PRED_FIELD( m_hDrainTarget, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),

//	DEFINE_PRED_FIELD( m_pEmitter, FIELD_POINTER ),
//	DEFINE_PRED_FIELD( m_hParticleMaterial, FIELD_???,  ),
//	DEFINE_PRED_FIELD( m_PathParticleEvent, FIELD_???,  ),
//	DEFINE_PRED_FIELD( m_DribbleParticleEvent, FIELD_??? ),
//	DEFINE_PRED_FIELD( m_bPlayingSound, FIELD_BOOLEAN ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponDrainBeam::CWeaponDrainBeam( void )
{
	m_bDraining = false;
	m_bAttacking = false;
	m_flNextBuzzTime = 0;

	SetPredictionEligible( true );
#if defined( CLIENT_DLL )
	m_pEmitter = NULL;
	m_hParticleMaterial = INVALID_MATERIAL_HANDLE;
	m_PathParticleEvent.Init( NUM_PATH_PARTICLES_PER_SEC );
	m_DribbleParticleEvent.Init( NUM_DRIBBLE_PARTICLES_PER_SEC ); 
	m_bPlayingSound = false;
#endif
}

void CWeaponDrainBeam::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "WeaponRepairGun.Healing" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponDrainBeam::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	RemoveDrainTarget();
	
	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to a drainable target
//-----------------------------------------------------------------------------
CBaseEntity *CWeaponDrainBeam::GetTargetToDrain( CBaseEntity *pCurDrainTarget )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return NULL;

#if !defined( CLIENT_DLL )
	Vector vecSrc = pOwner->Weapon_ShootPosition( );

	// ROBIN: Disabled drainbeam locking to target
/*
	// If we're already draining something, stick onto it as long as possible.
	CBaseEntity *pEntity = pCurDrainTarget;
	if ( pEntity && pEntity->IsAlive() )
	{
		// Make sure the target didn't go out of range.
		Vector vecTargetCenter = pEntity->Center();
		if ( (vecTargetCenter - vecSrc).Length() < weapon_drainbeam_stick_range.GetFloat() )
		{
			// Now make sure there isn't something other than team players in the way.
			class CDrainFilter : public CTraceFilterSimple
			{
			public:
				CDrainFilter( CBaseEntity *pShooter ) : CTraceFilterSimple( pShooter, TFCOLLISION_GROUP_WEAPON )
				{
					m_pShooter = pShooter;
				}

				virtual bool ShouldHitEntity( IServerEntity *pServerEntity, int contentsMask )
				{
					// If it hit an edict the isn't the target and is on our team, then the ray is blocked.
					CBaseEntity *pEnt = (CBaseEntity *)pServerEntity;

					// Ignore collisions with the shooter
					if ( pEnt == m_pShooter )
						return false;

					// Ignore neutral objects
					if ( pEnt->GetTeamNumber() == 0 )
						return false;

					// Ignore collisions with teammates
					if ( pEnt->GetTeam() == m_pShooter->GetTeam() )
						return false;

					// Ignore players
					if ( pEnt->IsPlayer() )
						return false;

					return CTraceFilterSimple::ShouldHitEntity( pServerEntity, contentsMask );
				}

				CBaseEntity	*m_pShooter;
			};

			trace_t tr;
			CDrainFilter drainFilter( pOwner );
			engine->TraceLine( vecSrc, vecTargetCenter, MASK_SHOT, &drainFilter, &tr );
			
			if (( tr.fraction == 1.0f) || (tr.m_pEnt == pEntity))
				return pEntity;
		}

		// Return null so we can't target this player but m_hDrainTarget stays set to them.
		return NULL;
	}
	else
*/
	{	
		// Ok, try to find a new target to drain.
		// Get the target point and location
		Vector vecAiming;
		pOwner->EyeVectors( &vecAiming );

		// Find a player in range of this player, and make sure they're drainable.
		Vector vecEnd = vecSrc + vecAiming * weapon_drainbeam_target_range.GetFloat();
		trace_t tr;

		// Use WeaponTraceLine so shields are tested...
		TFGameRules()->WeaponTraceLine( vecSrc, vecEnd, MASK_SHOT & (~CONTENTS_HITBOX), pOwner, DMG_PROBE, &tr );
		
		if ( tr.fraction != 1.0 )
		{
			CBaseEntity *pEntity = tr.m_pEnt;
			if ( pEntity && (pEntity != pOwner) && pEntity->IsAlive() )
			{
				// Target needs to not be a player, take EMP damage, and needs to be an enemy
				if ( !pEntity->IsPlayer() && pEntity->CanBePoweredUp() && pEntity->GetTeamNumber() != 0 && !pEntity->InSameTeam( pOwner ) )
				{
					// If we've just locked onto a new target, restart our drain rate
					if ( pCurDrainTarget != pEntity )
					{
						m_flDrainStartedAt = gpGlobals->curtime;
					}
					return pEntity;
				}
			}
		}

		return NULL;
	}
#endif

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Overloaded to handle the hold-down draining
//-----------------------------------------------------------------------------
void CWeaponDrainBeam::ItemPostFrame( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	// Try to start draining
	m_bAttacking = false;
	if ( (pOwner->m_nButtons & IN_ATTACK) && GetShieldState() == SS_DOWN )
	{
		PrimaryAttack();
		m_bAttacking = true;
	}
	else if ( GetCurDrainTarget() )
	{
		// Detach from the player if they release the attack button.
		RemoveDrainTarget();
	}

	// Prevent shield post frame if we're not ready to attack, or we're draining
	AllowShieldPostFrame( !m_bAttacking );

	WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponDrainBeam::RemoveDrainTarget( void )
{
	m_hDrainTarget = NULL;
	m_bDraining = false;
}

//-----------------------------------------------------------------------------
// Purpose: Attempt to drain any player within range of the medikit
//-----------------------------------------------------------------------------
void CWeaponDrainBeam::PrimaryAttack( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

#if !defined( CLIENT_DLL )
	// Find a target to drain
	CBaseEntity *pTarget = GetTargetToDrain( GetCurDrainTarget() );
	if ( pTarget )
	{
		// EMP the target
		if ( pTarget->CanBePoweredUp() )
		{
			// Increase the EMP time based upon the time we spent draining
			float flDrainTime = (gpGlobals->curtime - m_flDrainStartedAt);
			float flPostDrainTime = weapon_drainbeam_emp_time_factor.GetFloat() * flDrainTime;
			if ( pTarget->AttemptToPowerup( POWERUP_EMP, flPostDrainTime ) )
			{
				// Crank up the drain rate based on the time since we started draining this target
				flDrainTime = MIN( weapon_drainbeam_max_rate_time.GetFloat(), flDrainTime );
				float flDrainAmount = weapon_drainbeam_max_rate.GetFloat() * ( flDrainTime / weapon_drainbeam_max_rate_time.GetFloat() );

				// It uses floating point in here so it doesn't lose the fractional part on small frame times.
				static float flCurDamageAmt = 0.99;
				float flOldDamageAmt = flCurDamageAmt;
				flCurDamageAmt += gpGlobals->frametime * flDrainAmount;
				int iDamage = (int)flCurDamageAmt - (int)flOldDamageAmt;

				// Accumulated enough to do something?
				if ( iDamage )
				{
					// EMPed my target, make it take damage
					if ( pTarget->GetHealth() )
					{
						pTarget->TakeDamage( CTakeDamageInfo( this, pOwner, iDamage, DMG_GENERIC ) );
					}

					// Increase my health
					if ( pOwner->GetHealth() < pOwner->GetMaxHealth() )
					{
						int maxHealthToAdd = pOwner->GetMaxHealth() - pOwner->GetHealth();
						int nHealthAdded = MIN( iDamage, maxHealthToAdd );
						pOwner->TakeHealth( nHealthAdded, DMG_GENERIC );
					}

					// If I'm a sapper, I store off the energy
					if ( pOwner->IsClass( TFCLASS_SAPPER ) )
					{
						((CPlayerClassSapper*)pOwner->GetPlayerClass())->AddDrainedEnergy( iDamage );
					}
				}
			}
		}

		// Tell the client who we're trying to drain.
		m_bDraining = true;
		m_vFireTarget = pTarget->WorldSpaceCenter();
		m_hDrainTarget = pTarget;
	}
	else
	{
		RemoveDrainTarget();
	}
#endif

	// Fire continuously.
	m_flNextPrimaryAttack = gpGlobals->curtime;

	CheckRemoveDisguise();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponDrainBeam::PlayAttackAnimation( int activity )
{
	SendWeaponAnim( activity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponDrainBeam::WeaponIdle( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponDrainBeam::GainedNewTechnology( CBaseTechnology *pTechnology )
{
}

#if defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponDrainBeam::StopDrainSound( bool bStopHealingSound, bool bStopNoTargetSound )
{
	if ( bStopHealingSound )
		StopSound( entindex(), "WeaponRepairGun.Healing" );

	if ( bStopNoTargetSound )
		StopSound( entindex(), "WeaponRepairGun.NoTarget" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponDrainBeam::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_pEmitter = CSimpleEmitter::Create( "C_WeaponDrainBeam" );
		m_hParticleMaterial = m_pEmitter->GetPMaterial( "sprites/chargeball" );

		ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_ALWAYS );
	}

	// Think?
	if ( !m_bDraining )
	{
		m_bPlayingSound = false;
		StopDrainSound( true, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponDrainBeam::ClientThink()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// Don't show it while the player is dead. Ideally, we'd respond to m_bDraining in OnDataChanged,
	// but it stops sending the weapon when it's holstered, and it gets holstered when the player dies.
	C_BasePlayer *pFiringPlayer = dynamic_cast< C_BasePlayer* >( GetOwner() );
	if ( !pFiringPlayer || pFiringPlayer->IsPlayerDead() )
	{
		m_bPlayingSound = false;
		StopDrainSound();
		return;
	}

	// Abort if we're not draining, or the player doesn't have the fire button down
	if ( !m_bDraining && !m_bAttacking )
		return;

	// Ready for particle emission, sir
	m_pEmitter->SetSortOrigin( (m_vFireTarget + pPlayer->GetAbsOrigin()) * 0.5f );
	float flCur = gpGlobals->frametime;
	Vector vForward, vOrigin;
	QAngle vAngles;
	GetShootPosition( vOrigin, vAngles );
	AngleVectors( vAngles, &vForward );

	// Are they holding the attack button but not draining anyone? Give feedback.
	if ( !m_bDraining )
	{
		// Add random short-lived particles from the gun tip to the target.
		while ( m_DribbleParticleEvent.NextEvent( flCur ) )
		{
			Vector vPos = vOrigin;
			vPos += RandomVector( -2, 2 );
			SimpleParticle *pParticle = m_pEmitter->AddSimpleParticle( m_hParticleMaterial, vPos );
			if ( pParticle )
			{
				// Move the points along the path.
				pParticle->m_vecVelocity = vForward;
				pParticle->m_vecVelocity *= 50;

				pParticle->m_flRoll = 0;
				pParticle->m_flRollDelta = 0;
				pParticle->m_flDieTime = 0.4f;
				pParticle->m_flLifetime = 0;
				pParticle->m_uchColor[0] = 255; 
				pParticle->m_uchColor[1] = 255;
				pParticle->m_uchColor[2] = 255;
				pParticle->m_uchStartAlpha = 32;
				pParticle->m_uchEndAlpha = 0;
				pParticle->m_uchStartSize = 4;
				pParticle->m_uchEndSize = 0;
				pParticle->m_iFlags = 0;
			}
		}
		return;
	}

	if ( !m_bPlayingSound )
	{
		m_bPlayingSound = true;
		CLocalPlayerFilter filter;

		EmitSound( filter, entindex(), "WeaponRepairGun.Healing" );
	}

	Vector points[NUM_DRAINBEAM_PATH_POINTS];

	// First generate a sequence of points so we can parameterize the (curvy) path from the
	// tip of the gun to the target.
	Vector vDirTo = m_vFireTarget - vOrigin;
	float flDistanceTo = vDirTo.Length();
	vDirTo /= flDistanceTo;

	float flBendDist = flDistanceTo * 3;
	const Vector  A = vOrigin - vForward * flBendDist;
	const Vector &B = vOrigin;
	const Vector &C = m_vFireTarget;
	const Vector  D = m_vFireTarget - vForward * flBendDist;
	
	for ( int i=0; i < NUM_DRAINBEAM_PATH_POINTS; i++ )
	{
		Catmull_Rom_Spline( A, B, C, D, (float)i / (NUM_DRAINBEAM_PATH_POINTS-1), points[i] );
	}

	// Add random short-lived particles from the gun tip to the target.
	while ( m_PathParticleEvent.NextEvent( flCur ) )
	{
		float t = RandomFloat( 0, 1 );
		int iPrev = (int)( t * (NUM_DRAINBEAM_PATH_POINTS - 1.001) );
		float tPrev = (float)iPrev / (NUM_DRAINBEAM_PATH_POINTS - 1);
		float tNext = (float)(iPrev+1) / (NUM_DRAINBEAM_PATH_POINTS - 1);
		Assert( tNext <= NUM_DRAINBEAM_PATH_POINTS-1 );

		Vector vPos;
		VectorLerp( points[iPrev], points[iPrev+1], (t-tPrev) / (tNext - tPrev), vPos );
		vPos += RandomVector( -2, 2 );
		
		SimpleParticle *pParticle = m_pEmitter->AddSimpleParticle( m_hParticleMaterial, vPos );
		if ( pParticle )
		{
			// Move the points along the path.
			pParticle->m_vecVelocity = points[iPrev+1] - points[iPrev];
			VectorNormalize( pParticle->m_vecVelocity );
			pParticle->m_vecVelocity *= -PARTICLE_PATH_VEL;

			pParticle->m_flRoll = 0;
			pParticle->m_flRollDelta = 0;
			pParticle->m_flDieTime = 0.2f;
			pParticle->m_flLifetime = 0;
			pParticle->m_uchColor[0] = 255; 
			pParticle->m_uchColor[1] = 255;
			pParticle->m_uchColor[2] = 255;
			pParticle->m_uchStartAlpha = 32;
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = 4;
			pParticle->m_uchEndSize = 2;
			pParticle->m_iFlags = 0;
		}
	}
}
#endif
