#ifndef _DEFINED_ASW_SENTRY_TOP_H
#define _DEFINED_ASW_SENTRY_TOP_H
#pragma once

class CASW_Player;
class CASW_Marine;
class CASW_Sentry_Base;
class ITraceFilter;

class CASW_Sentry_Top : public CBaseAnimating
{
public:
	DECLARE_CLASS( CASW_Sentry_Top, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache();

	CASW_Sentry_Top();
	virtual ~CASW_Sentry_Top();
	virtual void Spawn( void );
	void	AnimThink( void );
	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo );
	int UpdateTransmitState();
	void SetSentryBase(CASW_Sentry_Base* pSentryBase);
	void SetDeployYaw(float yaw) { m_fDeployYaw = anglemod(yaw); }
	void SetCurrentYaw(float yaw) { m_fCurrentYaw = anglemod(yaw); }
	virtual int GetSentryDamage();
	virtual void SetTopModel();
	void PlayTurnSound();

	void UpdateGoal();
	void TurnToGoal(float deltatime);
	void FindEnemy();
	virtual bool IsValidEnemy( CAI_BaseNPC *pNPC );
	virtual void CheckFiring();
	virtual void Fire( void );
	virtual void MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );
	virtual void OnUsedQuarterAmmo( void );
	virtual void OnLowAmmo( void );
	virtual void OnOutOfAmmo( void );
	bool CanSee(CBaseEntity* pEnt);
	virtual ITraceFilter *GetVisibilityTraceFilter(); // new's up and returns a pointer to a trace filter. you must delete this trace filter after use.
	virtual float GetYawTo(CBaseEntity* pEnt);
	float GetPitchTo(CBaseEntity* pEnt);
	Vector GetFiringPosition();
	float GetRange();
	
	float m_fLastThinkTime;
	float m_fNextFireTime;
	float m_fGoalYaw, m_fCurrentYaw;

	CNetworkVar(float, m_fDeployYaw); 	
	CNetworkVar(bool, m_bLowAmmo); 	

	int m_iEnemySkip;	
	EHANDLE m_hEnemy;	
	int m_iCanSeeError;
	int m_iAmmoType;
	int m_iBaseTurnRate;	// angles per second
	CNetworkVar(int, m_iSentryAngle);
	//float m_flTimeNextScanPing;
	float m_fTurnRate;		// actual turn rate	

	float m_flTimeFirstFired;
	
	// sound stuff
	float m_flNextTurnSound;

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		
		cone = VECTOR_CONE_5DEGREES;

		return cone;
	}

	CASW_Sentry_Base* GetSentryBase();
	bool HasAmmo();
	int GetAmmo();
	//EHANDLE m_hSentryBase;
	CNetworkHandle(CBaseEntity, m_hSentryBase);

	/// Constants:
	enum
	{
		ASW_SENTRY_TURNRATE=	150,		// angles per second
		ASW_SENTRY_ANGLE= 60,			// spread on each side
		ASW_SENTRY_FIRING_HEIGHT= 50,
		ASW_SENTRY_FIRE_ANGLE_THRESHOLD= 3,
		ASW_SENTRY_RANGE= 525, // just the default
	};

protected:
	// helper function used by FindEnemy
	Vector GetEnemyVelocity( CBaseEntity *pEnemy = NULL );
	virtual CAI_BaseNPC *SelectOptimalEnemy() ;

	float m_flShootRange;
	bool  m_bHasHysteresis; // if true, CheckFiring() will be called even if there is no m_hEnemy
};


inline float CASW_Sentry_Top::GetRange()
{
	return m_flShootRange;
}


#endif /* _DEFINED_ASW_SENTRY_TOP_H */
