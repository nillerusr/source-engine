#ifndef _INCLUDE_ASW_QUEEN_GRABBER_SHARED_H
#define _INCLUDE_ASW_QUEEN_GRABBER_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
	#include "c_asw_queen.h"
	#define CASW_Queen C_ASW_Queen
#else
	#include "asw_queen.h"

#define ASW_NUM_CHILD_GRABBERS 5
#endif

//  Queen Grabbers are the tentactles that come up out of the ground and grab a marine.  They start
//   in front of the queen and move underground towards their target with a smoke trailer, before
//   bursting up and grabbing.

#ifdef CLIENT_DLL
class CASW_Queen_Grabber : public CBaseAnimating, public IASW_Client_Aim_Target

#else
class CASW_Queen_Grabber : public CBaseAnimating
#endif
{
public:
	DECLARE_CLASS( CASW_Queen_Grabber, CBaseAnimating );
	DECLARE_NETWORKCLASS();

	CASW_Queen_Grabber();
	virtual ~CASW_Queen_Grabber();

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_QUEEN_GRABBER; }

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
	void Precache();
	void Spawn();
	void SetupVPhysicsHull();
	virtual bool CreateVPhysics();
	
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	void SetBurrowing(bool bBurrowing);
	void GrabberThink();
	bool PositionToGrab(CBaseEntity *pEntity);
	bool CanGrabAtThisAngle(CBaseEntity *pEntity);
	int UpdateTransmitState();
	virtual void	ReachedEndOfSequence();
	bool m_bFinishedRetractAnim;
	void MakePrimary();
	void SetPrimary(CASW_Queen_Grabber *pPrimary);
	bool m_bPrimaryGrabber;
	CHandle<CASW_Queen_Grabber> m_hPrimary;
	CHandle<CASW_Queen_Grabber> m_hChildGrabber[ASW_NUM_CHILD_GRABBERS];

	virtual void StopLoopingSounds();
	void StartDigLoopSound();
	void StopDigLoopSound();
	void StartGrabLoopSound();
	void StopGrabLoopSound();

	CBaseEntity* GetQueenEnemy();
	void UpdateChasing();
	void UpdateRetracting();
	void CreatePeers();	// create the other grabbers that are going to grab along with us
	void StartGrabbing();	// play grab animation
	void AbortGrab();
	void DropToGround(Vector &vecPos);	// traces down from a point to the ground

	void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	void ASWTraceBleed( float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType );
	bool PassesDamageFilter( const CTakeDamageInfo &info );

	CASW_Queen* GetQueen();
	CHandle<CASW_Queen> m_hQueen;

	static CASW_Queen_Grabber *Create_Queen_Grabber( CASW_Queen *pQueen, const Vector &pos, const QAngle &ang );	

	float m_fDiverSpeed;
	float m_fMaxChasingTime;	// after this time the tentacles will stop chasing

	float m_fGrabbingEndTime;	// after this time, grabbing will stop automatically
	bool m_bWantsRetractAnim;
#else
	// client only

	// aim entity stuff
	IMPLEMENT_AUTO_LIST_GET();
	virtual float GetRadius() { return 24; }
	virtual bool IsAimTarget() { return true; }
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming) { return WorldSpaceCenter(); }
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return WorldSpaceCenter(); }
#endif
	virtual int BloodColor();
};

#define ASW_DIVER_SPEED 2000
#define ASW_DIVER_ACCELERATION 2000
#define ASW_GRAB_DISTANCE 10
#define ASW_PRE_GRAB_DISTANCE 50
#define ASW_RETRACT_DISTANCE 200

#ifndef CLIENT_DLL
class CASWTraceFilterWorldAndPropsOnly : public ITraceFilter
{
public:
	bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		return false;
	}
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_EVERYTHING;
	}
};
#endif

#endif /* _INCLUDE_ASW_QUEEN_GRABBER_SHARED_H */
