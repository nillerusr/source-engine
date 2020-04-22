//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_GAMEMOVEMENT_H
#define TF_GAMEMOVEMENT_H
#ifdef _WIN32
#pragma once
#endif

#include "gamemovement.h"
#include "tf_movedata.h"
#include "movevars_shared.h"

class CBaseTFPlayer;
class CBasePlayer;

//-----------------------------------------------------------------------------
// This class is the GameMovement class for team fortress and overrides 
// some of the default behavior.
//-----------------------------------------------------------------------------
class CTFGameMovement : public CGameMovement
{
	// Team Fortress 2 game movement base class.
	DECLARE_CLASS( CTFGameMovement, CGameMovement );

public:

	// CGameMovement public overrides.
	virtual void PlayerMove( void );

	virtual void ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData ) {}

	// Utility
	inline CTFMoveData *TFMove( void )	{ return static_cast<CTFMoveData*>( mv ); }

protected:
	// Player movement functions.
	virtual bool	PrePlayerMove( void );
	virtual void	HandlePlayerMove( void );
	virtual void	PostPlayerMove( void );

	// Player pre-movement functions.
	virtual int		CheckStuck( void );
	virtual void	UpdateTimers( void );
	bool			CheckDeath( void );
	virtual void	SetupViewAngles( void );
	virtual void	HandleDuck( void );
	virtual void	FinishUnDuck( void );
	virtual void	SetupSpeed( void );
	void			SpeedCrop( void );
	void			Accelerate( Vector& wishdir, float wishspeed, float accel);
	void			AccelerateWithoutMomentum( Vector& wishdir, float wishspeed, float accel);
	virtual float	GetAirSpeedCap( void );
	float			CalcGravityAdjustment( const Vector &wishdir );
	void			HandleLadder( void );
	virtual void	CategorizePosition( void );
	virtual bool	CheckJumpButton( void );

	virtual void	PlayStepSound( surfacedata_t *psurface, float fvol, bool force );
	// Should the step sound play?
	virtual bool	ShouldPlayStepSound( surfacedata_t *psurface, float fvol );

	// Specific movement functions.
	virtual void	FullWalkMove();
	virtual void	WalkMove( void );
	virtual void	_WalkMove( void );
	void			WalkMove2( void );
	void			AirMove( void );
	virtual int		TryPlayerMove( Vector *pFirstDest=NULL, trace_t *pFirstTrace=NULL );
	int				TryPlayerMove2( void );
	void			ResolveStanding( void );
	void			TryStanding( void );
	bool			ChargeMove( void );
	bool			StunMove( void );

	void			EndCharge( void );

	virtual void	HandleDuckingSpeedCrop( void );

	// Figures out how the constraint should slow us down
	float			ComputeConstraintSpeedFactor( void );

	// Movement helpers.
	virtual bool	CalcWishVelocityAndPosition( Vector &vWishPos, Vector &vWishDir, float &flWishSpeed );
	inline void		TracePlayerBBoxWithStep( const Vector &vStart, const Vector &vEnd, unsigned int fMask, int collisionGroup, trace_t &trace );

	// Momentum
	void			SetMomentumList( float flValue = 1.0f );
	void			AddToMomentumList( float flValue );
	float			GetMomentum( void );

	// Collision response functions.
	bool			CollisionResponseGeneric( const trace_t &trace, int &nBlocked );
	void			CollisionResponseStuck( void );
	void			CollisionResponseNone( const trace_t &trace );
	bool			RedirectGroundVelocity( const trace_t &trace );
	bool			RedirectAirVelocity( const trace_t &trace );
	inline int		BlockerType( const Vector &vImpactNormal );

protected:

	// Per movement collision data cache(s)
	Vector					m_vecGroundNormal;
	Vector					m_vecOriginalVelocity;
	int						m_nLanding;

	enum { MAX_IMPACT_PLANES = 5 };
	int						m_nImpactPlaneCount;
	Vector					m_aImpactPlaneNormals[MAX_IMPACT_PLANES];

	enum { MOVEMENTSTACK_MAXSIZE = 10 };
	struct MovementStackData_t
	{
		Vector m_vecPosition;
		Vector m_vecVelocity;
		Vector m_vecImpactNormal;
	};
	int						m_nMovementStackSize;
	MovementStackData_t		m_aMovementStack[MOVEMENTSTACK_MAXSIZE];
};

#endif // TF_GAMEMOVEMENT_H
