#ifndef _INCLUDED_ASW_DRONE_MOVEMENT_H
#define _INCLUDED_ASW_DRONE_MOVEMENT_H

#include "tier0/vprof.h"
#include "ai_basenpc.h"

class CMoveData;

class CASW_Drone_Movement
{
public:
	DECLARE_CLASS_NOBASE( CASW_Drone_Movement );

	CASW_Drone_Movement();

	void ProcessMovement( CAI_BaseNPC *pNPC, CMoveData *pMove, float flInterval);
	void StepMove( Vector &vecDestination, trace_t &trace );
	void WalkMove();
	void CategorizePosition();
	int TryMove( Vector *pFirstDest, trace_t *pFirstTrace );
	int ClipVelocity( Vector& in, Vector& normal, Vector& out, float overbounce );
	void SetGroundEntity( CBaseEntity *newGround );
	void CheckVelocity();
	void StartGravity();
	void FinishGravity();

	virtual const Vector&	GetOuterMins() const;
	virtual const Vector&	GetOuterMaxs() const;
	void TraceBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm );

	// Input/Output for this movement
	CMoveData		*mv;
	CAI_BaseNPC		*m_pNPC;
	float		m_flInterval;	// time to simulate over
	Vector m_LastHitWallNormal;

	float			m_surfaceFriction;
};

inline void CASW_Drone_Movement::TraceBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm )
{
	VPROF( "CASW_Drone_Movement::TraceBBox" );

	Ray_t ray;
	ray.Init( start, end, GetOuterMins(), GetOuterMaxs() );
	UTIL_TraceRay( ray, fMask, m_pNPC, collisionGroup, &pm );
}

#endif // _INCLUDED_ASW_DRONE_MOVEMENT_H