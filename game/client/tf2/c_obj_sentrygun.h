//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_OBJ_SENTRYGUN_H
#define C_OBJ_SENTRYGUN_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Base Sentrygun
//-----------------------------------------------------------------------------
class C_ObjectSentrygun : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectSentrygun, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_ENTITY_PANEL();

	C_ObjectSentrygun();
	virtual void	GetBoneControllers(float controllers[MAXSTUDIOBONES]);
	virtual int		DrawModel( int flags );
	virtual void	SetDormant( bool bDormant );
	virtual void	PreDataUpdate( DataUpdateType_t updateType );
	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual float	GetInitialBuilderYaw();
	int				GetAmmoLeft( void ) { return m_iAmmo; }
	virtual void	FinishedBuilding( void );

	virtual void	ClientThink( void );

private:
	bool	IsTurtled( void ) { return m_bTurtled; }

	// Turret Functions
	bool	MoveTurret( void );

	// Recompute sentrygun orientation...
	void RecomputeOrientation();

public:

	int				m_iRightBound;
	int				m_iLeftBound;
	bool			m_bTurningRight;

	// Movement
	int				m_iBaseTurnRate;
	float			m_fTurnRate;
	QAngle			m_vecCurAngles;
	QAngle			m_vecGoalAngles;
	Vector			m_vecCurDishAngles;

	float			m_fBoneXRotator;
	float			m_fBoneYRotator;
	int				m_iAmmo;

	// Turtling
	bool			m_bTurtled;
	bool			m_bLastTurtled;
	float			m_flStartedTurtlingAt;
	float			m_flStartedUnTurtlingAt;

	int				m_nAnimationParity;
 	int				m_nLastAnimationParity;

	// Networked from server
	EHANDLE			m_hEnemy;


	QAngle			m_angPrevLocalAngles;
	int				m_nOrientationParity;
	int				m_nPrevOrientationParity;

private:
	C_ObjectSentrygun( const C_ObjectSentrygun & ); // not defined, not accessible
};

class C_ObjectSentrygunPlasma : public C_ObjectSentrygun
{
	DECLARE_CLASS( C_ObjectSentrygunPlasma, C_ObjectSentrygun );
public:
	C_ObjectSentrygunPlasma();
	DECLARE_CLIENTCLASS();

private:
	C_ObjectSentrygunPlasma( const C_ObjectSentrygunPlasma & ); // not defined, not accessible
};

class C_ObjectSentrygunRocketlauncher : public C_ObjectSentrygun
{
	DECLARE_CLASS( C_ObjectSentrygunRocketlauncher, C_ObjectSentrygun );
public:
	C_ObjectSentrygunRocketlauncher();
	DECLARE_CLIENTCLASS();

private:
	C_ObjectSentrygunRocketlauncher( const C_ObjectSentrygunRocketlauncher & ); // not defined, not accessible
};

#endif // C_OBJ_SENTRYGUN_H
