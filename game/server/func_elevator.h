// func_elevator.h
// Copyright 2007 Turtle Rock Studios, Inc.

#ifndef FUNC_ELEVATOR_H
#define FUNC_ELEVATOR_H

#include "basetoggle.h"
#include "utlmap.h"

//--------------------------------------------------------------------------------------------------------
class CInfoElevatorFloor : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoElevatorFloor, CPointEntity );
	DECLARE_DATADESC();

	void OnReachedFloor( CBaseEntity *elevator );

	COutputEvent m_OnReachedFloor;
};


//--------------------------------------------------------------------------------------------------------
struct FloorInfo
{
	float height;		// Height of this floor
	string_t name;		// Name of this floor
	EHANDLE button;		// Button that moves the elevator to this floor
};


//--------------------------------------------------------------------------------------------------------
class CFuncElevator : public CBaseToggle
{
public:
	DECLARE_CLASS( CFuncElevator, CBaseToggle );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_fFlags );

	CFuncElevator();

	virtual void		Spawn( void );
	virtual void		Precache( void );
	virtual bool		CreateVPhysics( void );
	virtual int			DrawDebugTextOverlays( void );

	// FIXMEL4DTOMAINMERGE
	//	virtual void		PhysicsSimulate( void );

	virtual void		MoveDone( void );
	virtual void		Blocked( CBaseEntity *pOther );

	int					GetNumFloors( void ) const;				// Returns the number of floors at which this elevator can stop
	const FloorInfo		*GetFloor( int floorNumber ) const;		// Returns a FloorInfo * for the given floor
	int					GetCurrentFloor( void ) const;			// Returns the current floor, or -1 if the elevator is in-between floors
	int					GetDestinationFloor( void ) const;		// Returns the floor to which the elevator is moving, or the current floor number if the elevator is stopped
	float				GetCurrentSpeed( void ) const;			// Returns the z velocity of the elevator, or 0 if the elevator is stopped

	int					GetFloorForHeight( float height ) const;	// Returns the floor nearest the given height, or -1 if no floor is nearby
	EHANDLE				GetButtonForHeight( float height ) const;	// Returns a button near the current floor height that drives the elevator to the given height
	EHANDLE				GetButtonAtCurrentHeight( void ) const;		// Returns a button near the current floor that drives the elevator to *ANY* other height

	bool				IsEnabled( void ) const;				// Returns true if the elevator can be moved by buttons etc
	bool				IsMoving( void ) const;					// Returns true if the elevator is in motion

	void				FindPlayersOnElevator( CUtlVector< CBasePlayer * > *players, int teamNumber = TEAM_UNASSIGNED );	// Fills in a vector of players on the elevator
	bool				IsPlayerOnElevator( CBasePlayer *player );	// Returns true if a player is on the elevator

	float				GetCurrentHeight( void );

private:
	void				MoveTo( float destinationZ );
	void				StopMoveSoundThink( void );
	void				AccelerationThink( void );

	// Input handlers
	void				InputMoveToFloor( inputdata_t &inputdata );	// Start the brush moving to the specified floor
	void				InputDisable( inputdata_t &inputdata );		// Prevent the elevator from moving again

	string_t	m_soundStart;			// start and looping sound
	string_t	m_soundStop;			// stop sound
	string_t	m_soundDisable;			// disable sound
	string_t	m_currentSound;			// sound I'm playing

	float		m_flBlockDamage;		// Damage inflicted when blocked.

	Vector		m_topFloorPosition;
	Vector		m_bottomFloorPosition;

	CNetworkVar( float, m_maxSpeed );
	CNetworkVar( float, m_currentSpeed );
	CNetworkVar( float, m_acceleration );
	IntervalTimer m_accelerationTimer;
	CNetworkVar( float, m_movementStartTime );
	CNetworkVar( float, m_movementStartSpeed );
	CNetworkVar( float, m_movementStartZ );
	CNetworkVar( float, m_destinationFloorPosition );
	CNetworkVar( bool, m_isMoving );
	EHANDLE		m_targetFloor;
	bool		m_enabled;

	// Outputs
	COutputEvent m_OnReachedTop;
	COutputEvent m_OnReachedBottom;

	CUtlVector< FloorInfo > m_floors;
};


//--------------------------------------------------------------------------------------------------------
inline bool CFuncElevator::IsMoving( void ) const
{
	return m_isMoving;
}


//--------------------------------------------------------------------------------------------------------
inline int CFuncElevator::GetNumFloors( void ) const
{
	return m_floors.Count();
}


//--------------------------------------------------------------------------------------------------------
inline const FloorInfo *CFuncElevator::GetFloor( int floorNumber ) const
{
	if ( floorNumber < 0 || floorNumber >= m_floors.Count() )
		return NULL;

	return &m_floors[ floorNumber ];
}


//--------------------------------------------------------------------------------------------------------
inline float CFuncElevator::GetCurrentSpeed( void ) const
{
	return m_currentSpeed;
}


//--------------------------------------------------------------------------------------------------------
inline bool CFuncElevator::IsEnabled( void ) const
{
	return m_enabled;
}




#endif // FUNC_ELEVATOR_H