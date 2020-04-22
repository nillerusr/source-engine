//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defender's sentrygun
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_SENTRYGUN_H
#define TF_OBJ_SENTRYGUN_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_obj.h"

enum TFTURRET_ANIM
{
	TFTURRET_ANIM_NONE = 0,
	TFTURRET_ANIM_FIRE,
	TFTURRET_ANIM_SPIN,
};

enum target_ranges
{
	RANGE_NEAR,
	RANGE_MID,
	RANGE_FAR,
};

// Sentrygun damages
#define SG_MACHINEGUN_DAMAGE				5

// ------------------------------------------------------------------------ //
// The Base Sentrygun 
// ------------------------------------------------------------------------ //
class CObjectSentrygun : public CBaseObject
{
	DECLARE_CLASS( CObjectSentrygun, CBaseObject );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CObjectSentrygun();

	static CObjectSentrygun* Create(const Vector &vOrigin, const QAngle &vAngles, int iType);

	virtual void	Spawn();
	virtual void	Precache();
	virtual void	SetupAttachedVersion( void );
	virtual void	SetupUnattachedVersion( void );
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual void	FinishedBuilding( void );
	virtual bool	IsSentrygun( void ) { return true; };
	virtual bool	WantsCoverFromSentryGun() { return false; }
	virtual void	SetTechnology( bool bSmarter, bool bSensors );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual void	Killed( void );

	// Ammo filling
	virtual bool	ClientCommand( CBaseTFPlayer *pPlayer, const CCommand &args );
	virtual bool	TakeAmmoFrom( CBaseTFPlayer *pPlayer );

	// Think functions
	void	SentryRotate(void);
	void	Attack(void);
	void	Sentry_Explode( void );
	bool	FInViewCone( CBaseEntity *pEntity );
	int		BloodColor( void ) { return DONT_BLEED; }

	virtual void	CheckShield( void );
	void		RestartAnimation();
	void		ResetOrientation();

	virtual void	SetSentryAnim( TFTURRET_ANIM anim );
	virtual CBaseEntity *FindTarget( void );
	virtual float		GetPriority( CBaseEntity *pTarget );
	virtual void	FoundTarget();
	virtual bool	ValidTarget( CBaseEntity *pTarget );
	virtual int		Range( CBaseEntity *pTarget );

	// Combat functions
	virtual bool	HasAmmo( void );
	virtual bool	Fire( void );
	virtual bool	WillSuppress( void ) { return true; };

	virtual bool	CanTakeEMPDamage( void ) { return true; }

	// Turret Functions
	bool	MoveTurret( void );

	// Object functions
	void	ObjectMoved( void );

	// Designator interactions
	void	DesignateTarget( CBaseEntity *pTarget );
	// Turtle mode
	bool	IsTurtled( void );
	bool	IsTurtling( void );		// Return true if we're in the process of turtling / unturtling
	void	ToggleTurtle( void );
	void	Turtle( void );
	void	UnTurtle( void );

private:
	// Recompute sentrygun orientation...
	void RecomputeOrientation();

public:
	// Variables
	int		m_iRightBound;
	int		m_iLeftBound;
	bool	m_bTurningRight;
	int		m_iShardIndex;
	int		m_iAmmoType;
	bool	m_bSmarter;
	bool	m_bSensors;
	float	m_flNextLook;

	// Attacking
	float	m_flNextAttack;
	CNetworkVar( int, m_iAmmo );
	int		m_iMaxAmmo;
	Vector	m_vecFireTarget;
	Vector	m_vecLastKnownPosition;
	bool	m_bSuppressing;
	float	m_flStartedSuppressing;

	// Movement
	CNetworkVar( int, m_iBaseTurnRate );
	float	m_fTurnRate;
	QAngle	m_vecCurAngles;
	QAngle	m_vecGoalAngles;
	Vector	m_vecCurDishAngles;

	// Turtling
	CNetworkVar( bool, m_bTurtled );
	bool	m_bTurtling;
	float	m_flTurtlingFinishedAt;

	// Data sent to clients
	// Bone controllers
	float	m_fBoneXRotator;
	float	m_fBoneYRotator;

	// Target data
	CNetworkHandle( CBaseEntity, m_hEnemy );
	EHANDLE	m_hDesignatedEnemy;

	CNetworkVar( int, m_nAnimationParity );
	CNetworkVar( int, m_nOrientationParity );
};

//-----------------------------------------------------------------------------
// Purpose: Plasma Sentrygun
//-----------------------------------------------------------------------------
class CObjectSentrygunPlasma : public CObjectSentrygun
{
	DECLARE_CLASS( CObjectSentrygunPlasma, CObjectSentrygun );
public:
	DECLARE_SERVERCLASS();
	virtual void	Spawn();
	virtual bool	Fire( void );
	virtual void	CheckShield( void );
	virtual bool	HasAmmo( void );

private:
	float	m_flNextAmmoRecharge;
	int		m_nBurstCount;
};

#define SG_PLASMA_MODEL						"models/sentry2.mdl"
#define PLASMA_SENTRYGUN_RECHARGE_TIME		1.25			// Time it takes to recharge 1 round of ammo
#define PLASMA_SENTRY_BURST_COUNT			4

//-----------------------------------------------------------------------------
// Purpose: Rocket launcher Sentrygun
//-----------------------------------------------------------------------------
class CObjectSentrygunRocketlauncher : public CObjectSentrygun
{
	DECLARE_CLASS( CObjectSentrygunRocketlauncher, CObjectSentrygun );
public:
	DECLARE_SERVERCLASS();
	virtual void	Spawn();
	virtual bool	Fire( void );

	virtual void	SetTechnology( bool bSmarter, bool bSensors );
};

#define SG_ROCKETLAUNCHER_MODEL		"models/sentry3.mdl"

#endif // TF_OBJ_SENTRYGUN_H
