//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// improv_locomotor.h
// Interface for moving Improvs along computed paths
// Author: Michael Booth, July 2004

#ifndef _IMPROV_LOCOMOTOR_H_
#define _IMPROV_LOCOMOTOR_H_

// TODO: Remove duplicate methods from CImprov, and update CImprov to use this class

/**
 * A locomotor owns the movement of an Improv
 */
class CImprovLocomotor
{
public:
	virtual const Vector &GetCentroid( void ) const = 0;
	virtual const Vector &GetFeet( void ) const = 0;					///< return position of "feet" - point below centroid of improv at feet level
	virtual const Vector &GetEyes( void ) const = 0;
	virtual float GetMoveAngle( void ) const = 0;						///< return direction of movement

	virtual CNavArea *GetLastKnownArea( void ) const = 0;
	virtual bool GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal = NULL ) = 0;	///< find "simple" ground height, treating current nav area as part of the floor

	virtual void Crouch( void ) = 0;
	virtual void StandUp( void ) = 0;									///< "un-crouch"
	virtual bool IsCrouching( void ) const = 0;

	virtual void Jump( void ) = 0;										///< initiate a jump
	virtual bool IsJumping( void ) const = 0;

	virtual void Run( void ) = 0;										///< set movement speed to running
	virtual void Walk( void ) = 0;										///< set movement speed to walking
	virtual bool IsRunning( void ) const = 0;

	virtual void StartLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos ) = 0;	///< invoked when a ladder is encountered while following a path
	virtual bool TraverseLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos, float deltaT ) = 0;	///< traverse given ladder
	virtual bool IsUsingLadder( void ) const = 0;

	enum MoveToFailureType
	{
		FAIL_INVALID_PATH,
		FAIL_STUCK,
		FAIL_FELL_OFF,
	};
	virtual void TrackPath( const Vector &pathGoal, float deltaT ) = 0;	///< move along path by following "pathGoal"
	virtual void OnMoveToSuccess( const Vector &goal ) { }				///< invoked when an improv reaches its MoveTo goal
	virtual void OnMoveToFailure( const Vector &goal, MoveToFailureType reason ) { }	///< invoked when an improv fails to reach a MoveTo goal
};

#endif // _IMPROV_LOCOMOTOR_H_
