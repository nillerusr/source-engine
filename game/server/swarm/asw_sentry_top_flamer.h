#ifndef _DEFINED_ASW_SENTRY_TOP_FLAMER_H
#define _DEFINED_ASW_SENTRY_TOP_FLAMER_H
#pragma once

#include "asw_sentry_top.h"

class CASW_Sentry_Top_Flamer : public CASW_Sentry_Top
{
public:
	DECLARE_CLASS( CASW_Sentry_Top_Flamer, CASW_Sentry_Top );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();


	CASW_Sentry_Top_Flamer( int projectileVelocity = 0 ); // zero actually means "default"
	// virtual ~CASW_Sentry_Top_Machinegun();

	/// Override this because flamer-style turrets don't have discrete fire,
	/// but rather start and stop firing continuously.
	virtual void CheckFiring();

	virtual float GetYawTo(CBaseEntity* pEnt);

	/// This emits one frame's worth of fire; it works differently
	/// from the single-shot fire on other turret types.
	virtual void Fire();

	virtual void SetTopModel();

	/// flamer turrets are on/off with their firing.
	virtual void StartFiring();
	virtual void StopFiring();
	/// True iff currently firing this frame.
	inline bool IsFiring() { return m_bFiring; };

	int GetSentryDamage();

	virtual ITraceFilter *GetVisibilityTraceFilter();

	inline float GetProjectileVelocity() const { return m_nProjectileVelocity; }

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SENTRY_FLAMER; }

protected:
	/// Helper function for Fire() -- actually emit the "bullets" of flame or ice
	virtual void FireProjectiles( int numShotsToFire,
		const Vector &vecSrc, const Vector &vecAiming, const AngularImpulse &rotSpeed = AngularImpulse(0,0,720)	);

	CNetworkVar(bool, m_bFiring);
	CNetworkVar(float, m_flPitchHack); // just for a second to deal with stairs until I get proper pitch gimballing on base class

	float m_flLastFireTime;
	float m_flFireHysteresisTime;
	const int m_nProjectileVelocity;
};

/// Compute the necessary trajectory for a projectile to hit a moving object.
Vector ProjectileIntercept( const Vector &vProjectileOrigin, const float fProjectileVelocity,
						   const Vector &vTargetOrigin, const Vector &vTargetDirection,
						   float * RESTRICT pflTime  = NULL );

#endif /* _DEFINED_ASW_SENTRY_TOP_FLAMER_H */