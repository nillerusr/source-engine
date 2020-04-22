//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TF_WALKER_BASE_H
#define TF_WALKER_BASE_H
#ifdef _WIN32
#pragma once
#endif


#include "basetfvehicle.h"


class CWalkerBase : public CBaseTFVehicle
{
public:
	DECLARE_CLASS( CWalkerBase, CBaseTFVehicle );
	DECLARE_SERVERCLASS();


public:
	CWalkerBase();

	// Derived classes must call this from inside their Spawn() function. Their Spawn() function
	// should NOT chain down to CWalkerBase.
	void SpawnWalker(
		const char *pModelName,
		int objectType,
		const Vector &vPlacementMins,
		const Vector &vPlacementMaxs,
		int iHealth,
		int nMaxPassengers,
		float flPlaybackSpeedBoost	// Strider likes the animations played at 2x speed.
		);

	virtual void AdjustInitialBuildAngles();

	// When it's in walk mode, it'll set a walk animation, process user input, set pose parameters,
	// and move the entity around as it walks.
	void EnableWalkMode( bool bEnable );

	// This is called each frame to do thinking. Derived classes can hook this to do their own stuff each frame.
	virtual void WalkerThink();

	// Returns the new local origin to set based on the walker's movement.
	// This usually just asks for the movement from the animation, but some walkers may 
	// want to generate the motion in code.
	virtual Vector GetWalkerLocalMovement();

	// See notes on m_vSteerVelocity below.
	const Vector2D& GetSteerVelocity() const;

	void	WalkerActivate( void );

	// See m_flVelocityDecayRate.
	void SetVelocityDecayRate( float flDecayRate );

	// Walkers think 10x/second.
	float GetTimeDelta() const;

// CBaseEntity.
public:
	virtual void Spawn();
	virtual void Activate();

// IVehicle overrides.
public:
	virtual void SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );


// IServerVehicle overrides.
public:
	virtual bool IsPassengerVisible( int nRole );


// CBaseObject overrides.
public:
	virtual bool StartBuilding( CBaseEntity *pBuilder );


public:

	// This is a hack to prevent the strider from playing a lot of footstep sounds, until we figure out 9-way anim events
	float m_flDontMakeSoundsUntil; 

	// Last usercmd buttons.
	int m_LastButtons;
	QAngle m_vLastCmdViewAngles;


private:

	// This is the (local) velocity that the player's controls are saying the player wants to move in.
	// Note that it is completely virtual - the speed that the walker moves at is gotten from the animations ground speed.
	// It maps this velocity a 9-way blend for movement. +X = forward, +Y = left.
	Vector2D m_vSteerVelocity;

	// Which pose parameters we use to make this guy walk around on X and Y.
	int m_iMovePoseParamX;
	int m_iMovePoseParamY;

	float m_flPlaybackSpeedBoost;

	bool m_bWalkMode;

	// This is a magic number that can be tweaked to make the velocity decay faster.
	// Default: 80
	float m_flVelocityDecayRate;
};


#endif // TF_WALKER_BASE_H
