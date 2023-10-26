#include "cbase.h"
#include "props.h"
#include "asw_sentry_base.h"
#include "asw_sentry_top_icer.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "ammodef.h"
#include "asw_gamerules.h"
#include "beam_shared.h"
#include "asw_weapon_flamer_shared.h"
#include "asw_extinguisher_projectile.h"
#include "shot_manipulator.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SENTRY_TOP_MODEL "models/sentry_gun/freeze_top.mdl"

LINK_ENTITY_TO_CLASS( asw_sentry_top_icer, CASW_Sentry_Top_Icer );
PRECACHE_REGISTER( asw_sentry_top_icer );


IMPLEMENT_SERVERCLASS_ST( CASW_Sentry_Top_Icer, DT_ASW_Sentry_Top_Icer )
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Sentry_Top_Icer )
END_DATADESC()

extern ConVar asw_weapon_max_shooting_distance;
extern ConVar asw_weapon_force_scale;
extern ConVar asw_difficulty_alien_health_step;

ConVar asw_sentry_debug_aim("asw_sentry_debug_aim", "0", FCVAR_CHEAT, "Draw debug lines for sentry gun aim");


#define ASW_SENTRY_FIRE_RATE 0.1f		// time in seconds between each shot
#define ASW_SENTRY_FIRE_ANGLE_THRESHOLD 3

void CASW_Sentry_Top_Icer::SetTopModel()
{
	SetModel(SENTRY_TOP_MODEL);
}



CASW_Sentry_Top_Icer::CASW_Sentry_Top_Icer() : CASW_Sentry_Top_Flamer(CASW_Weapon_Flamer::EXTINGUISHER_PROJECTILE_AIR_VELOCITY)
{
	m_flShootRange = 300;
	// increase turn rate until I get better leading code in (so it can actually hit something)
	m_fTurnRate *= 3.0f;
}

/// @TODO attrib hooks
int CASW_Sentry_Top_Icer::GetSentryDamage()
{
	return 1; // copied from flamer? i guess it doesn't come from script??

	/*
	float flDamage = 25.0f;
	float flMultiplier = 1.0f;
	if ( ASWGameRules() )
	{
		float fDiff = ASWGameRules()->GetMissionDifficulty() - 5;
		flMultiplier = 1.0 + fDiff * asw_difficulty_alien_health_step.GetFloat();
	}

	//Msg ( "Damage = %f\n", flDamage );
	return (flDamage * flMultiplier) * GetSentryBase()->m_fDamageScale;
	*/
}


ITraceFilter *CASW_Sentry_Top_Icer::GetVisibilityTraceFilter()
{
	return new CTraceFilterSkipClassname( GetSentryBase(), "asw_extinguisher_projectile", COLLISION_GROUP_NONE );
}

bool CASW_Sentry_Top_Icer::IsValidEnemy( CAI_BaseNPC *pNPC )
{
	if ( !pNPC )
		return false;

	if ( !BaseClass::IsValidEnemy( pNPC ) )
	{
		return false;
	}

	// don't freeze current enemy past 60% unless there's no one better
	// around (prevents one alien from being frozen solid while rest rush past)
	// don't pick new enemies that are more than 60%
	if ( pNPC == m_hEnemy.Get() )
		return pNPC->GetFrozenAmount( ) < fsel( m_flEnemyOverfreezePermittedUntil - gpGlobals->curtime , 0.9f , 0.6f );
	else
		return pNPC->GetFrozenAmount( ) < 0.6f;
}


/// Helper function for Fire() -- actually emit the "bullets" of flame or ice
void CASW_Sentry_Top_Icer::FireProjectiles( int numShotsToFire, ///< number of pellets to fire this frame
											 const Vector &vecSrc, ///< origin for bullets
											 const Vector &vecAiming, ///< aim direction for bullets
											 const AngularImpulse &rotSpeed )
{
	CShotManipulator Manipulator( vecAiming );

	/*
	CASW_Marine * RESTRICT const pMarineDeployer = GetSentryBase()->m_hDeployer.Get();
	Assert( pMarineDeployer );
	*/

	for ( int i = 0 ; i < numShotsToFire ; i++ )
	{
		// create a pellet at some random spread direction		
		Vector projectileVel = Manipulator.GetShotDirection(); // Manipulator.ApplySpread(GetBulletSpread());

		projectileVel *= GetProjectileVelocity();
		projectileVel *= (1.0 + (0.1 * random->RandomFloat(-1,1)));
		CASW_Extinguisher_Projectile *pProjectile = CASW_Extinguisher_Projectile::Extinguisher_Projectile_Create( 
			vecSrc + (projectileVel.Normalized() * BoundingRadius()), QAngle(0,0,0),	projectileVel, rotSpeed, 
			this /*, pMarineDeployer*/ );
		if ( pProjectile )
		{
			pProjectile->SetFreezeAmount( 0.4f );
		}
	}
}

extern ConVar asw_sentry_friendly_target;
CAI_BaseNPC * CASW_Sentry_Top_Icer::SelectOptimalEnemy()
{
	// prioritize unfrozen aliens who are going to leave the cone soon.
	// prioritize aliens less the more frozen they get.
	CUtlVectorFixedGrowable< CAI_BaseNPC *,16 > candidates;
	CUtlVectorFixedGrowable< float, 16 > candidatescores;

	// search through all npcs, any that are in LOS and have health
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		if (ppAIs[i]->GetHealth() > 0 && CanSee(ppAIs[i]))
		{
			// don't shoot marines
			if ( !asw_sentry_friendly_target.GetBool() && ppAIs[i]->Classify() == CLASS_ASW_MARINE )
				continue;

			if ( ppAIs[i]->Classify() == CLASS_SCANNER )
				continue;

			if ( !IsValidEnemy( ppAIs[i] ) )
				continue;

			candidates.AddToTail( ppAIs[i] );
		}
	}

	// bail out if we don't have anyone
	if ( candidates.Count() < 1 )
		return NULL;
	else if ( candidates.Count() == 1 ) // just one candidate is an obvious result
		return candidates[0];
	
	// collect some statistics on the available enemies as a whole
	float flFreezeAmtMin, flFreezeAmtMax;
	flFreezeAmtMin = flFreezeAmtMax = candidates[0]->GetFrozenAmount();
	for ( int i = candidates.Count() - 1; i > 0 ; --i )
	{
		const float frz = candidates[i]->GetFrozenAmount();
		flFreezeAmtMin = fpmin( flFreezeAmtMin, frz );
		flFreezeAmtMax = fpmax( flFreezeAmtMax, frz );
	}
	if ( flFreezeAmtMin > 0.35f )
	{
		m_flEnemyOverfreezePermittedUntil = gpGlobals->curtime + 0.5f;
	}

	// score each of the candidates
	candidatescores.EnsureCount( candidates.Count() );
	for ( int i = candidates.Count() - 1; i >= 0 ; --i )
	{
		CAI_BaseNPC * RESTRICT pCandidate = candidates[i];
		// is the candidate moving into or out of the cone?
		Vector vCandVel = GetEnemyVelocity(pCandidate);
		Vector vMeToTarget = pCandidate->GetAbsOrigin() - GetFiringPosition();
		Vector vBaseForward = UTIL_YawToVector( m_fDeployYaw );

		// crush everything to 2d for simplicity
		vMeToTarget.z = 0;
		vCandVel.z = 0;
		vBaseForward.z = 0;

		Vector velCross = vBaseForward.Cross(vCandVel); // this encodes also some info on perpendicularity
		Vector vAimCross = vBaseForward.Cross(vMeToTarget);
		bool bTargetHeadedOutOfCone = !vCandVel.IsZero() && velCross.z * vAimCross.z >= 0; // true if same sign
		float flConeLeavingUrgency;

		if ( bTargetHeadedOutOfCone )
		{
			flConeLeavingUrgency = fabs( velCross.z / vCandVel.Length2D() ); 
			// just the sin; varies 0..1 where 1 means moving perpendicular to my aim
		}
		else
		{
			flConeLeavingUrgency = 0; // not at threat of leaving just yet
		}

		// the angle between my current yaw and what's needed to hit the target
		float flSwivelNeeded = fabs( UTIL_AngleDiff(  // i wish we weren't storing euler angles
			UTIL_VecToYaw( vMeToTarget ) , m_fDeployYaw ) );
		flSwivelNeeded /= ASW_SENTRY_ANGLE; // normalize to 0..2

		float flFreezeNeeded = 1 - pCandidate->GetFrozenAmount();

		candidatescores[i] = Vector( 3.0f, -1.5f, 2.0f ).Dot( 
						     Vector(flConeLeavingUrgency, flSwivelNeeded, flFreezeNeeded) );
	}
	// find the highest scoring candidate
	int best = 0;
	for ( int i = 1 ; i < candidatescores.Count() ; ++i )
	{
		if ( candidatescores[i] > candidatescores[best] )
			best = i;
	}

	// NDebugOverlay::EntityBounds(candidates[best], 255, 255, 0, 255, 0.2f );

	return candidates[best];
}




