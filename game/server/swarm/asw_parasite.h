#ifndef _INCLUDED_ASW_PARASITE_H
#define _INCLUDED_ASW_PARASITE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_alien.h"
#include "asw_egg.h"

class CSoundPatch;
class CASW_Marine;
class CASW_Colonist;

class CASW_Parasite : public CASW_Alien
{
	DECLARE_CLASS( CASW_Parasite, CASW_Alien );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();	
	CASW_Parasite();
	virtual ~CASW_Parasite();

	void Spawn();
	void Precache();
	float GetIdealSpeed() const;
	float GetIdealAccel( ) const;
	float MaxYawSpeed( void );
	void HandleAnimEvent( animevent_t *pEvent );	
	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_PARASITE; }
	virtual bool		ShouldGib( const CTakeDamageInfo &info );
	bool CorpseGib( const CTakeDamageInfo &info );
	void BuildScheduleTestBits( void );
	void GatherEnemyConditions( CBaseEntity *pEnemy );
	virtual bool CanBeSeenBy( CAI_BaseNPC *pNPC );
	void StartTask(const Task_t *pTask);
	void RunTask( const Task_t *pTask );
	int SelectSchedule();
	int TranslateSchedule( int scheduleType );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void JumpAttack( bool bRandomJump, const Vector &vecPos = vec3_origin, bool bThrown = false );
	void Leap( const Vector &vecVel );
	int RangeAttack1Conditions( float flDot, float flDist );
	virtual float InnateRange1MinRange( void );
	virtual float InnateRange1MaxRange( void );
	void LeapThink();
	void InfestThink();
	virtual void NPCThink();
	void NormalTouch(CBaseEntity* pOther);
	void LeapTouch(CBaseEntity* pOther);
	int CalcDamageInfo( CTakeDamageInfo *pInfo );
	void TouchDamage( CBaseEntity *pOther );
	bool HasHeadroom();
	void DoJumpFromEgg();
	void SetJumpFromEgg(bool b, float flJumpDistance = 100.0f);
	bool m_bJumpFromEgg;
	float m_flEggJumpDistance;
	void IdleInEgg(bool b);
	CNetworkVar( bool, m_bDoEggIdle );
	virtual void UpdateSleepState(bool bInPVS);
	Activity TranslateActivity( Activity baseAct, Activity *pIdealWeaponActivity );
	void UpdatePlaybackRate();
	virtual float GetSpringColRadius() { return 10.0f; }
	
	bool m_bMidJump, m_bCommittedToJump;
	Vector m_vecCommittedJumpPos;
	float	m_flNextNPCThink;
	void SetIgnoreCollisionTime(float f) { m_flIgnoreWorldCollisionTime = f; }
	float	m_flIgnoreWorldCollisionTime;

	virtual void SetHealthByDifficultyLevel();
	void RunAnimation ( void );
	// sounds
	virtual void AlertSound();
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void AttackSound();
	virtual void IdleSound();
	virtual void PrescheduleThink();
	virtual void BiteSound();	

	// the egg we hatched from, if any (we notify it when we die)
	void SetEgg(CASW_Egg* pEgg);
	CASW_Egg* GetEgg();
	EHANDLE m_hEgg;
	CNetworkVar(bool, m_bStartIdleSound);

	// infesting
	void FinishedInfesting();
	void InfestMarine(CASW_Marine* pMarine);
	void InfestColonist(CASW_Colonist* pColonist);
	CNetworkVar(bool, m_bInfesting);
	bool m_bDefanged;	// if the parasite is defanged, he won't infest, just do melee damage on contact

	// parasites can be spawned by a parent Harvester, who wishes to be notified when we die
	EHANDLE m_hMother;
	virtual void SetMother(CASW_Alien* spawner);
	CASW_Alien* GetMother();
	float m_fSuicideTime;	// harvesites don't hang around for ever, but pop after a while if they're not in combat

	static float s_fNextSpottedChatterTime;
	static float s_fLastHarvesiteAttackSound;

	void StartInfestation();
	bool CheckInfestTarget( CBaseEntity *pOther );
	EHANDLE m_hPrepareToInfest;

protected:
	DEFINE_CUSTOM_AI;
};

#endif // _INCLUDED_ASW_PARASITE_H
