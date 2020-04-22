//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_SHIELD_SHARED_H
#define TF_SHIELD_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "mathlib/vmatrix.h"
#include "utlvector.h"
#include "SheetSimulator.h"
#include "predictable_entity.h"


//-----------------------------------------------------------------------------
// Purpose: Shield (mobile version)
//-----------------------------------------------------------------------------
enum
{
	SHIELD_NUM_HORIZONTAL_POINTS = 8,
	SHIELD_NUM_VERTICAL_POINTS = 8,
	SHIELD_NUM_CONTROL_POINTS = SHIELD_NUM_HORIZONTAL_POINTS * SHIELD_NUM_VERTICAL_POINTS,
	SHIELD_INITIAL_THETA = 135,
	SHIELD_INITIAL_PHI = 90,
	SHIELD_HORIZONTAL_PANEL_COUNT = (SHIELD_NUM_HORIZONTAL_POINTS - 1),
	SHIELD_VERTICAL_PANEL_COUNT = (SHIELD_NUM_VERTICAL_POINTS - 1),
	SHIELD_PANELS_COUNT = (SHIELD_HORIZONTAL_PANEL_COUNT * SHIELD_VERTICAL_PANEL_COUNT),
	SHIELD_VERTEX_BYTES = (SHIELD_NUM_CONTROL_POINTS + 7) >> 3,
	SHIELD_TIME_SUBVISIBIONS = 2
};

//--------------------------------------------------------------------------
// Mobile shield state flags
//--------------------------------------------------------------------------
enum
{
	SHIELD_MOBILE_EMP = 0x1
};


//--------------------------------------------------------------------------
// Shield grenade state
//--------------------------------------------------------------------------
enum
{
	SHIELD_FLAT_EMP = 0x1,
	SHIELD_FLAT_INACTIVE = 0x2
};

enum
{
	SHIELD_FLAT_SHUTDOWN_TIME = 1
};

enum
{
	SHIELD_GRENADE_WIDTH = 150,
	SHIELD_GRENADE_HEIGHT = 150,
};


#define SHIELD_DAMAGE_CHANGE_TIME	1.5f


//-----------------------------------------------------------------------------
// Amount of time it takes to fade the shield in or out due to EMP 
//-----------------------------------------------------------------------------
#define SHIELD_EMP_FADE_TIME 0.7f


//-----------------------------------------------------------------------------
// Amount of time it takes a point to wobble when EMPed
//-----------------------------------------------------------------------------
#define SHIELD_EMP_WOBBLE_TIME 0.1f


//-----------------------------------------------------------------------------
// Methods we must install into the effect
//-----------------------------------------------------------------------------
class IActiveVertList
{
public:
	virtual int		GetActiveVertState( int iVert ) = 0;
	virtual void	SetActiveVertState( int iVert, int bOn ) = 0;
};


class CShieldEffect
{
	DECLARE_CLASS_NOBASE( CShieldEffect );
	DECLARE_PREDICTABLE();

public:
	CShieldEffect();

	void Precache();
	void Spawn(const Vector& currentPosition, const QAngle& currentAngles);

	// Sets the collision group
	void SetCollisionGroup( int group );

	// Computes the opacity....
	float ComputeOpacity( const Vector& pt, const Vector& center ) const;

	// Computes the bounds
	void ComputeBounds( Vector& mins, Vector& maxs );

	// Simulation
	void Simulate( float dt );

	// Sets desired orientation + position
	void SetDesiredOrigin( const Vector& origin );
	void SetDesiredAngles( const QAngle& angles );
	const QAngle& GetDesiredAngles() const;

	// Hooks in active bits...
	void SetActiveVertexList( IActiveVertList *pActiveVerts );

	// Gets a point...
	const Vector& GetPoint( int x, int y ) const;
	const Vector& GetPoint( int i ) const;
	Vector& GetPoint( int i );

	// Computes control points
	void ComputeControlPoints();

	// The current angles (computed by Simulate on the server)
	const QAngle& GetCurrentAngles() const;
	void SetCurrentAngles( const QAngle& angles);

	// The current position (computed by Simulate on the server)
	const Vector& GetCurrentPosition();
	void SetCurrentPosition( const Vector& pos );

	// Compute vertex activity
	void ComputeVertexActivity();

	// Recompute whether the panels are active or not
	void ComputePanelActivity();

	// Is a particular vertex active?
	bool IsVertexActive( int x, int y ) const;

	// Is a particular panel active?
	bool IsPanelActive( int x, int y ) const;

	// Gets a control point (for collision)
	const Vector& GetControlPoint( int i ) const { return m_pControlPoint[i]; }

	// Returns the panel size (for collision testing)
	void GetPanelSize( Vector& mins, Vector& maxs ) const;

	// Change the angular spring constant. This affects how fast the shield rotates to face the angles
	// given in SetAngles. Higher numbers are more responsive, but if you go too high (around 40), it will
	// jump past the specified angles and wiggle a little bit.
	void SetAngularSpringConstant( float flConstant );

	// Set the shield theta & phi
	void SetThetaPhi( float flTheta, float flPhi );

	// Returns the render bounds
	const Vector& GetRenderMins() const;
	const Vector& GetRenderMaxs() const;

private:
	// Simulation set up
	void ComputeRestPositions();
	void SetShieldPanelSize( Vector& mins, Vector& maxs );
	void SimulateTranslation( float dt );
	void SimulateRotation( float dt, const Vector& forward );
	void ComputeOrientationMatrix();

	float m_RestLength;
	float m_PlaneDist;
	float m_ShieldTheta;
	float m_ShieldPhi;

	// Spring constants
	float m_SpringConstant;
	float m_DampConstant;
	float m_ViscousDrag;
	float m_Mass;

	float m_AngularSpringConstant;
	float m_AngularViscousDrag;

	// collision group
	int		m_CollisionGroup;

	// Directions of the control points in shield space
	Vector	m_pFixedDirection[SHIELD_NUM_CONTROL_POINTS];

	// Position of the control points in world space
	Vector	m_pControlPoint[SHIELD_NUM_CONTROL_POINTS];

	// Bitfield indicating which vertices are active
	IActiveVertList *m_pActiveVerts;

	// Bitfield indicating which panels are active
	bool	m_pActivePanels[SHIELD_PANELS_COUNT];

	// Which point on the shield to test next
	int		m_TestPoint;
	int		m_PointList[SHIELD_NUM_CONTROL_POINTS];

	// desired position + orientation
	Vector	m_vecDesiredOrigin;
	QAngle	m_angDesiredAngles;

	// collision box
	Vector  m_PanelBoxMin;
	Vector	m_PanelBoxMax;

	// Render bounds (shield space)
	Vector  m_vecRenderMins;
	Vector	m_vecRenderMaxs;

	// Actual center position (relative to m_Origin)
	// + velocity (world space)
	Vector	m_Position;
	Vector	m_Velocity;

	// our current orientation....
	QAngle  m_CurrentAngles;
	VMatrix	m_Orientation;
	float	m_Theta;
	float	m_Phi;
	float	m_ThetaVelocity;
	float	m_PhiVelocity;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline const QAngle& CShieldEffect::GetCurrentAngles() const 
{ 
	return m_CurrentAngles; 
}

//-----------------------------------------------------------------------------
// Returns the render bounds
//-----------------------------------------------------------------------------
inline const Vector& CShieldEffect::GetRenderMins() const
{
	return m_vecRenderMins;
}

inline const Vector& CShieldEffect::GetRenderMaxs() const
{
	return m_vecRenderMaxs;
}


#endif // TF_SHIELD_SHARED_H
