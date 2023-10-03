#ifndef _INCLUDED_ASW_GRUB_H
#define _INCLUDED_ASW_GRUB_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_alien.h"

class CSoundPatch;

class CASW_Grub : public CASW_Alien
{
	DECLARE_CLASS( CASW_Grub, CASW_Alien );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();	
	CASW_Grub();
	virtual ~CASW_Grub();

	void Spawn();
	void Precache();
	float GetIdealSpeed() const;
	float GetIdealAccel( ) const;
	float MaxYawSpeed( void );
	virtual bool BlocksLOS();
	//void HandleAnimEvent( animevent_t *pEvent );	
	Class_T		Classify( void ) { return CLASS_EARTH_FAUNA; }	
	bool CorpseGib( const CTakeDamageInfo &info );
	//void BuildScheduleTestBits( void );
	int SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	//void GatherEnemyConditions( CBaseEntity *pEnemy );
	void StartTask(const Task_t *pTask);
	void RunTask( const Task_t *pTask );
	int SelectSchedule();
	int TranslateSchedule( int scheduleType );
	virtual void JumpAttack( bool bRandomJump, const Vector &vecPos = vec3_origin, bool bThrown = false );
	void Leap( const Vector &vecVel );
	//int RangeAttack1Conditions( float flDot, float flDist );
	void LeapThink();
	void NormalTouch(CBaseEntity* pOther);
	void LeapTouch(CBaseEntity* pOther);
	bool HasHeadroom();
	void DoJumpFromGoo();
	void SetJumpFromGoo(bool bDoJump, float flJumpAngle, float flJumpDistance = 100.0f);
	bool m_bJumpFromGoo;
	float m_flGooJumpDistance;
	void IdleInGoo(bool b);
	bool bDoGooIdle;
	Activity TranslateActivity( Activity baseAct, Activity *pIdealWeaponActivity );
	virtual bool ShouldCollide(int collisionGroup, int contentsMask);
	void Squash(CBaseEntity* pSquasher);
	virtual void UpdateEfficiency( bool bInPVS );
	
	bool m_bMidJump, m_bCommittedToJump;
	float m_flGooJumpAngle;
	Vector m_vecCommittedJumpPos;
	float	m_flNextNPCThink;
	void SetIgnoreCollisionTime(float f) { m_flIgnoreWorldCollisionTime = f; }
	float	m_flIgnoreWorldCollisionTime;

	// sounds
	virtual void AlertSound();
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void AttackSound();
	virtual void IdleSound();
	//virtual void PrescheduleThink();
	
protected:
	DEFINE_CUSTOM_AI;
};

#endif // _INCLUDED_ASW_GRUB_H
