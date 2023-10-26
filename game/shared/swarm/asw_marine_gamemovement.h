//======== (C) Copyright 1999, 2000 Valve, L.L.C. All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//==================a===========================================================
#if !defined( MARINEGAMEMOVEMENT_H )
#define MARINEGAMEMOVEMENT_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_imarinegamemovement.h"
#include "cmodel.h"
#include "tier0/vprof.h"

#define CTEXTURESMAX		512			// max number of textures loaded
#define CBTEXTURENAMEMAX	13			// only load first n chars of name

#define GAMEMOVEMENT_DUCK_TIME				1000			// ms
#define GAMEMOVEMENT_JUMP_TIME				510				// ms approx - based on the 21 unit height jump
#define GAMEMOVEMENT_ASW_JUMP_HEIGHT			70.0f		// units
#define GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS			( TIME_TO_UNDUCK_MSECS )		// ms
#define GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS_INV		( GAMEMOVEMENT_DUCK_TIME - GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS )

struct surfacedata_t;

class CBasePlayer;
class CASW_Marine;

class CASW_MarineGameMovement : public IMarineGameMovement
{
public:
	DECLARE_CLASS_NOBASE( CASW_MarineGameMovement );
	
	CASW_MarineGameMovement( void );
	virtual			~CASW_MarineGameMovement( void );

	virtual void	ProcessMovement( CBasePlayer *pPlayer, CBaseEntity *pMarine, CMoveData *pMove );

	virtual const Vector&	GetPlayerMins( bool ducked ) const;
	virtual const Vector&	GetPlayerMaxs( bool ducked ) const;
	virtual const Vector&	GetPlayerViewOffset( bool ducked ) const;

	void			DoJumpJet();

	CMoveData*		GetMoveData() { return mv; }

protected:
	// Input/Output for this movement
	CMoveData		*mv;
	CBasePlayer		*player;
	CASW_Marine		*marine;
	
	int				m_nOldWaterLevel;
	int				m_nOnLadder;

	Vector			m_vecForward;
	Vector			m_vecRight;
	Vector			m_vecUp;

protected:


	// Does most of the player movement logic.
	// Returns with origin, angles, and velocity modified in place.
	// were contacted during the move.
	virtual void	PlayerMove(	void );

	// Set ground data, etc.
	void			FinishMove( void );

	virtual float	CalcRoll( const QAngle &angles, const Vector &velocity, float rollangle, float rollspeed );

	virtual	void	DecayPunchAngle( void );

	void			CheckWaterJump(void );

	void			WaterMove( void );

	void			WaterJump( void );

	// Handles both ground friction and water friction
	void			Friction( void );

	void			AirAccelerate( Vector& wishdir, float wishspeed, float accel );

	virtual void	AirMove( void );
	
	virtual bool	CanAccelerate();
	virtual void	Accelerate( Vector& wishdir, float wishspeed, float accel);

	void StayOnGround();

	// Only used by players.  Moves along the ground when player is a MOVETYPE_WALK.
	virtual void	WalkMove( void );
	virtual void	FullMeleeMove( void );
	virtual void	MeleeMove( void );
	virtual void	FullJumpJetMove();
		
	// Handle MOVETYPE_WALK.
	virtual void	FullWalkMove();

	// Implement this if you want to know when the player collides during OnPlayerMove
	virtual void	OnTryPlayerMoveCollision( trace_t &tr ) {}

	const Vector&	GetPlayerMins( void ) const; // uses local player
	const Vector&	GetPlayerMaxs( void ) const; // uses local player
	void SetupMovementBounds( CMoveData *move );

	typedef enum
	{
		GROUND = 0,
		STUCK,
		LADDER
	} IntervalType_t;

	virtual int		GetCheckInterval( IntervalType_t type );

	// Useful for things that happen periodically. This lets things happen on the specified interval, but
	// spaces the events onto different frames for different players so they don't all hit their spikes
	// simultaneously.
	bool			CheckInterval( IntervalType_t type );


	// Decompoosed gravity
	void			StartGravity( void );
	void			FinishGravity( void );

	// Apply normal ( undecomposed ) gravity
	void			AddGravity( void );

	// Handle movement in noclip mode.
	void			FullNoClipMove( float factor, float maxacceleration );	

	// Returns true if he started a jump (ie: should he play the jump animation)?
	virtual bool	CheckJumpButton( void );	// Overridden by each game.

	// Dead player flying through air., e.g.
	void			FullTossMove( void );
	
	// Player is a Observer chasing another player
	void			FullObserverMove( void );

	// Handle movement when in MOVETYPE_LADDER mode.
	virtual void	FullLadderMove();

	// The basic solid body movement clip that slides along multiple planes
	virtual int		TryPlayerMove( Vector *pFirstDest=NULL, trace_t *pFirstTrace=NULL );
	
	virtual bool	LadderMove( void );
	virtual bool		OnLadder( trace_t &trace );

	// See if the player has a bogus velocity value.
	void			CheckVelocity( void );

	// Does not change the entities velocity at all
	void			PushEntity( Vector& push, trace_t *pTrace );

	// Slide off of the impacting object
	// returns the blocked flags:
	// 0x01 == floor
	// 0x02 == step / wall
	int				ClipVelocity( Vector& in, Vector& normal, Vector& out, float overbounce );

	// If pmove.origin is in a solid position,
	// try nudging slightly on all axis to
	// allow for the cut precision of the net coordinates
	int				CheckStuck( void );
	
	// Check if the point is in water.
	// Sets refWaterLevel and refWaterType appropriately.
	// If in water, applies current to baseVelocity, and returns true.
	bool			CheckWater( void );
	
	// Determine if player is in water, on ground, etc.
	virtual void CategorizePosition( void );

	virtual void	CheckParameters( void );

	virtual	void	ReduceTimers( void );

	void			CheckFalling( void );

	void			PlayerWaterSounds( void );

	void ResetGetPointContentsCache();
	int GetPointContentsCached( const Vector &point );

	// Ducking
	virtual void	Duck( void );
	virtual void	HandleDuckingSpeedCrop();
	virtual void	FinishUnDuck( void );
	virtual void	FinishDuck( void );
	virtual bool	CanUnduck();
	void			UpdateDuckJumpEyeOffset( void );
	bool			CanUnDuckJump( trace_t &trace );
	void			StartUnDuckJump( void );
	void			FinishUnDuckJump( trace_t &trace );
	void			SetDuckedEyeOffset( float duckFraction );
	void			FixPlayerCrouchStuck( bool moveup );

	float			SplineFraction( float value, float scale );

	void			CategorizeGroundSurface( trace_t &pm );

	bool			InWater( void );

	// Commander view movement
	void			IsometricMove( void );

	// Traces the marine bbox as it is swept from start to end
	void			TraceMarineBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm );

	// Tests the player position
	CBaseHandle		TestPlayerPosition( const Vector& pos, int collisionGroup, trace_t& pm );

	// Checks to see if we should actually jump 
	void			PlaySwimSound();

	bool			IsDead( void ) const;

	// Figures out how the constraint should slow us down
	float			ComputeConstraintSpeedFactor( void );

	void			SetGroundEntity( trace_t *pm );

private:
	// Performs the collision resolution for fliers.
	void			PerformFlyCollisionResolution( trace_t &pm, Vector &move );

	void			StepMove( Vector &vecDestination, trace_t &trace );

protected:
	
	virtual ITraceFilter *LockTraceFilter( int collisionGroup );
	virtual void UnlockTraceFilter( ITraceFilter *&pFilter );

	// Cache used to remove redundant calls to GetPointContents().
	int m_CachedGetPointContents;
	Vector m_CachedGetPointContentsPoint;	

	Vector			m_vecProximityMins;		// Used to be globals in sv_user.cpp.
	Vector			m_vecProximityMaxs;

	float			m_fFrameTime;

//private:
	bool			m_bSpeedCropped;

	float			m_flStuckCheckTime[MAX_PLAYERS+1][2]; // Last time we did a full test

	ITraceListData	*m_pTraceListData;

	int				m_nTraceCount;

public:
	// footsteps
	//void			PlantFootprint( surfacedata_t *psurface );
	surfacedata_t*	GetSurfaceData();
};

CASW_MarineGameMovement* ASWGameMovement();

#endif // MARINEGAMEMOVEMENT_H
