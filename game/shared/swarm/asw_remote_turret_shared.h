#ifndef _INCLUDED_ASW_REMOTE_TURRET_H
#define _INCLUDED_ASW_REMOTE_TURRET_H
#pragma once

#ifdef CLIENT_DLL
class C_BasePlayer;
#else
class CBasePlayer;
#endif
class CUserCmd;
class IMoveHelper;
class CMoveData;

// a turret in the world that can be remote controlled by a player (from a computer).

class CASW_Remote_Turret : public CBaseAnimating
{
public:
	DECLARE_CLASS( CASW_Remote_Turret, CBaseAnimating );
	DECLARE_NETWORKCLASS();

	CASW_Remote_Turret();
	virtual ~CASW_Remote_Turret();

	void SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData );
	const QAngle& EyeAngles();
	const QAngle& RealEyeAngles();
	void SmoothTurretAngle(QAngle &ang);
	Vector GetTurretCamPosition();
	Vector GetTurretMuzzlePosition();
	void GetButtons(bool& bAttack1, bool& bAttack2, bool& bReload );
	void FireTurret(CBasePlayer *pPlayer);
	int GetSentryDamage();

	virtual void MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
	void Precache();
	void Spawn();
	int UpdateTransmitState();
	int ShouldTransmit( const CCheckTransmitInfo *pInfo );
	void StopUsingTurret();
	void StartedUsingTurret(CBaseEntity* pUser);
	QAngle AutoaimDeflection( Vector &vecSrc, const QAngle &eyeAngles, autoaim_params_t &params );
	float GetAutoaimScore( const Vector &eyePosition, const Vector &viewDir, const Vector &vecTarget, CBaseEntity *pTarget, float fScale );
		
	CNetworkQAngle( m_angEyeAngles );	
#else	
	virtual void ClientThink();
	QAngle m_angEyeAngles;
	const QAngle& GetRenderAngles();
	float GetMuzzleFlashScale();
	int GetMuzzleAttachment();
	void ProcessMuzzleFlashEvent();

	QAngle m_LastAngle;
	CSoundPatch		*m_pLoopingSound;
	bool m_bLastPlaySound;
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void UpdateOnRemove();
	virtual void OnRestore();
	void SoundInit();
	void SoundShutdown();
	virtual void CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	void ASWRemoteTurretTracer( const Vector &vecEnd );
#endif
	CNetworkVar(bool, m_bUpsideDown);
	CBaseEntity* GetUser();
	CNetworkHandle(CBaseEntity, m_hUser);
	float m_fNextFireTime;
	int m_iAmmoType;
	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		
		cone = VECTOR_CONE_PRECALCULATED;//VECTOR_CONE_5DEGREES;

		return cone;
	}
	virtual Class_T Classify( void ) { return (Class_T) CLASS_ASW_REMOTE_TURRET; }

	CNetworkVar(QAngle, m_angDefault);	// reference angle for view limits
	CNetworkVar(QAngle, m_angViewLimit);	// how far we can look either side
};


#endif /* _INCLUDED_ASW_REMOTE_TURRET_H */
