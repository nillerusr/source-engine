#ifndef _DEFINED_ASW_MISSILE_ROUND_H
#define _DEFINED_ASW_MISSILE_ROUND_H
#pragma once

#ifdef CLIENT_DLL
#include "c_basecombatcharacter.h"
#define CASW_Missile_Round C_ASW_Missile_Round
#define CBaseCombatCharacter C_BaseCombatCharacter
#else
#include "basecombatcharacter.h"
#include "asw_lag_compensation.h"
#endif
#include "asw_shareddefs.h"

class CASW_Missile_Round : public CBaseCombatCharacter
{
public:
	DECLARE_CLASS( CASW_Missile_Round, CBaseCombatCharacter );

public:
	CASW_Missile_Round();	
	DECLARE_NETWORKCLASS();

#ifdef GAME_DLL
	virtual ~CASW_Missile_Round();
	DECLARE_DATADESC();

	static CASW_Missile_Round *Missile_Round_Create( const CASW_AlienShot &shot, const Vector &position, const QAngle &angles, const Vector &velocity, CBaseEntity *pOwner );	

	void			Setup( const CASW_AlienShot &shot, const Vector &position, const QAngle &angles, const Vector &velocity, CBaseEntity *pOwner );	
	virtual void	Spawn( );
	virtual void	Precache( );

	virtual void	PerformCustomPhysics( Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity );

	virtual void	DoLagCompensatedMarineCollision();

	CASW_Lag_Compensation m_LagCompensation;

private:
	void			Touch( CBaseEntity *pOther );
	void			MissileHit( CBaseEntity *pEnt, trace_t &tr );

	CASW_AlienShot	m_ShotDef;
	EHANDLE			m_hOwner;
	CNetworkVar( int, m_nParticleTrail );
	CNetworkVector( m_vEndPosition );
	CNetworkVar( bool, m_bDetonated );

	Vector			m_vecOldPosition;
	bool			m_bMarineFriendly;			// if set, this round will pass through marines and hit aliens

#else
	virtual void	PostDataUpdate( DataUpdateType_t updateType );

private:
	CNetworkVar( int, m_nParticleTrail );
	CNetworkVector( m_vEndPosition );
	CNetworkVar( bool, m_bDetonated );
	CUtlReference<CNewParticleEffect> m_pTrail;
#endif

private:	
	CASW_Missile_Round( const CASW_Missile_Round & );
	void ClientThink();
};

#ifdef GAME_DLL
extern CUtlVector<CASW_Missile_Round*> g_vecMissileRounds;
#endif

#endif // _DEFINED_ASW_MISSILE_ROUND_H
