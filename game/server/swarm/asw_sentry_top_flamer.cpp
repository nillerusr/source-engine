#include "cbase.h"
#include "props.h"
#include "asw_sentry_base.h"
#include "asw_sentry_top_flamer.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "ammodef.h"
#include "asw_gamerules.h"
#include "beam_shared.h"
#include "asw_weapon_flamer_shared.h"
#include "asw_flamer_projectile.h"
#include "shot_manipulator.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SENTRY_TOP_MODEL "models/sentry_gun/flame_top.mdl"

LINK_ENTITY_TO_CLASS( asw_sentry_top_flamer, CASW_Sentry_Top_Flamer );
PRECACHE_REGISTER( asw_sentry_top_flamer );


IMPLEMENT_SERVERCLASS_ST(CASW_Sentry_Top_Flamer, DT_ASW_Sentry_Top_Flamer )
	SendPropBool( SENDINFO( m_bFiring ) ),	
	SendPropFloat( SENDINFO( m_flPitchHack ) ),	
END_SEND_TABLE()


BEGIN_DATADESC( CASW_Sentry_Top_Flamer )
END_DATADESC()

extern ConVar asw_weapon_max_shooting_distance;
extern ConVar asw_weapon_force_scale;
extern ConVar asw_difficulty_alien_health_step;
extern ConVar asw_sentry_debug_aim;

void CASW_Sentry_Top_Flamer::SetTopModel()
{
	SetModel(SENTRY_TOP_MODEL);
}


#define ASW_SENTRY_FIRE_RATE 0.1f		// time in seconds between each shot
#define ASW_SENTRY_FIRE_ANGLE_THRESHOLD 3

CASW_Sentry_Top_Flamer::CASW_Sentry_Top_Flamer(  int projectileVelocity  ) : m_bFiring(false), m_flPitchHack(false), m_nProjectileVelocity( projectileVelocity ? projectileVelocity : CASW_Weapon_Flamer::FLAMER_PROJECTILE_AIR_VELOCITY )
{
	m_flShootRange = 375.0f;

	// increase turn rate until I get better leading code in (so it can actually hit something)
	m_fTurnRate *= 3.0f;
}

/// @TODO attrib hooks
int CASW_Sentry_Top_Flamer::GetSentryDamage()
{
	float flDamage = 4.0f;
	if ( ASWGameRules() )
	{
		ASWGameRules()->ModifyAlienHealthBySkillLevel( flDamage );
	}

	return flDamage * ( GetSentryBase() ? GetSentryBase()->m_fDamageScale : 1.0f );
}


void CASW_Sentry_Top_Flamer::CheckFiring()
{
	bool bShouldFire = false;

	if ( HasAmmo() && m_hEnemy.IsValid() && m_hEnemy.Get())
	{
		float flDist = fabs(m_fGoalYaw - m_fCurrentYaw);
		if (flDist > 180)
			flDist = 360 - flDist;	
		// use some hysteresis
		if (flDist < (IsFiring() ? ASW_SENTRY_FIRE_ANGLE_THRESHOLD * 1.1f : ASW_SENTRY_FIRE_ANGLE_THRESHOLD) )
		{
			bShouldFire = true;
		}
	}

	if ( bShouldFire )
	{
		m_flFireHysteresisTime = gpGlobals->curtime + 0.5f;
	}
	else
	{
		bShouldFire = gpGlobals->curtime < m_flFireHysteresisTime ;
	}
	
	// turn firing on or off as appropriate
	if ( IsFiring() != bShouldFire )
	{
		if ( bShouldFire )
			StartFiring();
		else
			StopFiring();
	}
	Assert( IsFiring() == bShouldFire );

	if ( bShouldFire )
	{
		Fire();
	}
}


ITraceFilter *CASW_Sentry_Top_Flamer::GetVisibilityTraceFilter()
{
	return new CTraceFilterSkipClassname( GetSentryBase(), "asw_flamer_projectile", COLLISION_GROUP_NONE );
}

float CASW_Sentry_Top_Flamer::GetYawTo(CBaseEntity* pEnt)
{
	if (!pEnt)
		return m_fDeployYaw;

	Vector vEnemyVel = GetEnemyVelocity( pEnt );
	// pEnt->GetVelocity( &vEnemyVel );

	Vector vIdealAim = ProjectileIntercept( GetFiringPosition(), 
		GetProjectileVelocity(),
		pEnt->WorldSpaceCenter(), vEnemyVel );

	if ( vIdealAim.IsZero() )
		return m_fDeployYaw;
	else
		return UTIL_VecToYaw(vIdealAim.Normalized());
}

void CASW_Sentry_Top_Flamer::Fire() RESTRICT
{
	// determine the number of projectiles to be fired this frame.
	// it's best to do this early by divsion and turn it into an int, 
	// rather than use eg 
	//   for ( float shotTime = m_flLastShot; shotTime <= curTime - shotInterval ; shotTime+= shotInterval )
	// because a branch on float comparison is really bad on 360.
	// division and int conversion are slow too, but we're starting it
	// early enough that hopefully the scheduler can hide latencies.

	// hang on to the float original to avoid a LHS when doing the conversion in the other direction below
	const float flNumShotsToFire = floor( (gpGlobals->curtime - m_flLastFireTime) / ASW_SENTRY_FIRE_RATE );
	const int numShotsToFire =  flNumShotsToFire ;

	const Vector vecSrc = GetFiringPosition();
	const QAngle &curAngles = GetAbsAngles();
	Vector vecAiming;
	AngleVectors( curAngles, &vecAiming );

	// update our aim vector if we have an enemy
	// (we might not, for the half-second period of hysteresis
	//  between losing an enemy and shutting off)
	if ( !!m_hEnemy )
	{
		Vector videalToEnemy = m_hEnemy->WorldSpaceCenter() - vecSrc;
		Vector vEnemyVelocity = GetEnemyVelocity();
		//m_hEnemy->GetVelocity( &vEnemyVelocity );
		if ( asw_sentry_debug_aim.GetBool() )
		{
			NDebugOverlay::HorzArrow( vecSrc, m_hEnemy->WorldSpaceCenter(), 2, 0, 255, 0, 255, true, 0.2f );
			NDebugOverlay::HorzArrow( m_hEnemy->WorldSpaceCenter(), m_hEnemy->WorldSpaceCenter()+ vEnemyVelocity, 
				2, 0, 0, 255, 255, true, 0.2f );
		}

		Vector intercept = ProjectileIntercept( vecSrc, GetProjectileVelocity(),
			m_hEnemy->WorldSpaceCenter(), vEnemyVelocity );
		if ( intercept.IsZero( ) )
		{
			if ( asw_sentry_debug_aim.GetBool() ) 
				NDebugOverlay::Cross( m_hEnemy->WorldSpaceCenter(), 8, 255, 0, 0 , true, 0.2f );
		}
		else
		{
			videalToEnemy = intercept;
			if ( asw_sentry_debug_aim.GetBool() ) 
				NDebugOverlay::HorzArrow( vecSrc, vecSrc + videalToEnemy, 2, 255, 255, 0, 255, true, 0.2f );
		}


		Vector vecAiming2DNormalized( vecAiming.x , vecAiming.y, 0 ) ;
		vecAiming2DNormalized.NormalizeInPlace();
		Vector vIdealToEnemyNormalized = videalToEnemy.Normalized();
		float flIdeal2DLength = vIdealToEnemyNormalized.Length2D();

		vecAiming.x = vecAiming2DNormalized.x * flIdeal2DLength;
		vecAiming.y = vecAiming2DNormalized.y * flIdeal2DLength;
		vecAiming.z = vIdealToEnemyNormalized.z  ;

		m_flPitchHack = -RAD2DEG(sin(vecAiming.z));
	}

	FireProjectiles( numShotsToFire, vecSrc, vecAiming );

	// if we actually fired shots, roll the timers forward.
	// leave the "cents" in place. 
	if ( numShotsToFire > 0 )
	{
		BaseClass::Fire();

		float flMillisecondsFired = m_flLastFireTime;

		m_flLastFireTime += flNumShotsToFire * ASW_SENTRY_FIRE_RATE;
		if (ASWGameRules())
			ASWGameRules()->m_fLastFireTime = m_flLastFireTime;

		flMillisecondsFired = (m_flLastFireTime - flMillisecondsFired) * 25.0f;
		Assert( flMillisecondsFired > 0 );
		// subtract from ammo.
		CASW_Sentry_Base* RESTRICT pSentryBase = GetSentryBase();
		Assert( pSentryBase );
		pSentryBase->OnFiredShots( (int)floor(flMillisecondsFired) );

		//Msg( "%f == numShotsToFire - %d, ammo used %d\n", gpGlobals->curtime, numShotsToFire, (int)floor(flMillisecondsFired) );
	}
}

void CASW_Sentry_Top_Flamer::StartFiring()
{
	m_bFiring = true;
	m_flLastFireTime = gpGlobals->curtime;
}

void CASW_Sentry_Top_Flamer::StopFiring()
{
	m_bFiring = false;
}

/// Helper function for Fire() -- actually emit the "bullets" of flame or ice
void CASW_Sentry_Top_Flamer::FireProjectiles( int numShotsToFire, ///< number of pellets to fire this frame
											  const Vector &vecSrc, ///< origin for bullets
											  const Vector &vecAiming, ///< aim direction for bullets
											  const AngularImpulse &rotSpeed )
{
	CShotManipulator Manipulator( vecAiming );
	CASW_Marine * RESTRICT const pMarineDeployer = GetSentryBase()->m_hDeployer.Get();
	Assert( pMarineDeployer );

	for ( int i = 0 ; i < numShotsToFire ; i++ )
	{
		// create a pellet at some random spread direction		
		Vector projectileVel = Manipulator.GetShotDirection(); // Manipulator.ApplySpread(GetBulletSpread());

		projectileVel *= GetProjectileVelocity();
		projectileVel *= (1.0 + (0.1 * random->RandomFloat(-1,1)));
		CASW_Flamer_Projectile *pFlames = CASW_Flamer_Projectile::Flamer_Projectile_Create( GetSentryDamage(), 
			vecSrc + (projectileVel.Normalized() * BoundingRadius()), QAngle(0,0,0),	projectileVel, rotSpeed, 
			this, pMarineDeployer, this );

		pFlames->SetHurtIgnited( true );
	}
}






/// Compute the necessary trajectory for a projectile to hit a moving object.
/// Given a static gun position, a moving target, the target's velocity vector,
/// and the SCALAR speed of the projectile, return the vector direction for
/// the projectile necessary to hit the target. 
/// Ie, solves (A - S) + Vt = Pt  where A is the enemy position,
/// S the gun position, V the enemy velocity, and P the projectile velocity,
/// where magnitude is known.
/// It's a quadratic, so if there are two solutions, returns the sooner one.
/// If there are no solutions, returns 0,0,0
/// If a Time output parameter is given, the (t) parameter from the quadratic
/// is written there. If there is no solution, time will be negative.
Vector ProjectileIntercept( const Vector &vProjectileOrigin, const float fProjectileVelocity,
						   const Vector &vTargetOrigin, const Vector &vTargetDirection,
						   float * RESTRICT pflTime  )
{
#pragma message("TODO: make SIMD")
	/* math:
	let	B	= (A - S)
	|P|	= Q

	B + Vt = Pt
	(B + Vt)^2 = (P.P)t^2 = (Q^2)(t^2)
	B^2 + 2t(B.V) + (V^2)(t^2) = (Q^2)(t^2)
	so
	(t^2)( V^2 - Q^2 ) + 2t(B.V) + B^2 = 0
	thus by quadratic
	t = ( -2(B.V) +- sqrt( 4(B.V)^2 - 4(V^2-Q^2)(B^2) ) ) /
	2(V^2-Q^2) 
	= ( -(B.V) +- sqrt( (B.V)^2 - (V^2-Q^2)(B^2) ) ) /
	(V^2-Q^2)

	and therefore P = (B+V)/t
	*/
	const Vector vB = vTargetOrigin - vProjectileOrigin;


	// quadratic term A
	const float fQA = vTargetDirection.LengthSqr() - ( fProjectileVelocity*fProjectileVelocity );
	// quadratic term B
	const float fDotVB = vTargetDirection.Dot( vB );

	// quadratic term C 
	const float fDotBB = vB.LengthSqr(); 
	if ( fQA == 0 )
	{
		// linear system only
		// so 0 = 2t(B.V) + B.B 
		// -(B.B)/2(B.V) = t
		// which is the same as saying that if the projectile and target have the same
		// velocity exactly, a solution is only possible if the gun is "ahead" of the 
		// target's path 
		float t = -fDotBB / ( 2 * fDotVB );
		if ( pflTime )
			*pflTime = t;
		return ( t > 0								 ?
			(vB + vTargetDirection)*FastRecip(t) :
		Vector(0,0,0) );
	}

	// else not linear
	const float discrim = fDotVB*fDotVB - fQA*fDotBB;
	if ( discrim < 0 )
	{
		// there is no solution
		if ( pflTime )
			*pflTime = -1;

		return Vector(0,0,0);
	}	
	else
	{
		const float discrimSqrt = FastSqrtEst(discrim);

#if 1
		// select the + or - radicand to match B's sign
		// to improve precision, then extract the other root
		float tmp = -fsel( fDotVB, fDotVB + discrimSqrt, fDotVB - discrimSqrt );
		const float t1 = tmp/fQA;
		const float t2 = fDotBB/tmp;
#else   
		// this is the simpler quadratic implementation, but it can suffer
		// from bad imprecision if the signs of the params mismatch and 
		// they differ only slightly
		const float t1 = ( -fDotVB + discrimSqrt )/fQA;
		const float t2 = ( -fDotVB - discrimSqrt )/fQA;
#endif

		float t; 
		if ( t1 < 0 )
		{
			t = t2;
		}
		else if ( t2 < 0 )
		{
			t = t1;
		}
		else 
		{
			t = fpmin( t1, t2 );
		}
		if ( pflTime )
			*pflTime = t;

		if ( t > 0 )
		{
			Vector retval = vB/t + vTargetDirection ;
#ifdef DBGFLAG_ASSERT
			Vector interceptA = (vProjectileOrigin + ( t * retval ));
			Vector interceptB = ( vTargetOrigin + vTargetDirection * t );
			float err = (interceptA - interceptB).Length();
			Assert( err < 0.001f );
#endif
			return retval;
		}
		else
			return Vector(0,0,0);
	}
}
