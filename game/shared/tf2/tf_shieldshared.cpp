//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:			The Escort's Shield weapon effect
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "tf_shieldshared.h"
#include "edict.h"
#include "mathlib/vmatrix.h"
#include "engine/IEngineTrace.h"

#ifdef CLIENT_DLL
#include "cdll_client_int.h"
#else
#include "gameinterface.h"
#endif	


	  
BEGIN_PREDICTION_DATA_NO_BASE( CShieldEffect )

	DEFINE_FIELD( m_TestPoint, FIELD_INTEGER ),
	DEFINE_FIELD( m_Position, FIELD_VECTOR ),
	DEFINE_FIELD( m_Velocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_CurrentAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_Theta, FIELD_FLOAT ),
	DEFINE_FIELD( m_Phi, FIELD_FLOAT ),
	DEFINE_FIELD( m_ThetaVelocity, FIELD_FLOAT ),
	DEFINE_FIELD( m_PhiVelocity, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecDesiredOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_angDesiredAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_ShieldTheta, FIELD_FLOAT ),
	DEFINE_FIELD( m_ShieldPhi, FIELD_FLOAT ),

END_PREDICTION_DATA()



//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CShieldEffect::CShieldEffect( )
{
}


//-----------------------------------------------------------------------------
// compute rest positions of the springs
//-----------------------------------------------------------------------------
void CShieldEffect::ComputeRestPositions()
{
	int i;

	m_vecRenderMins.Init( FLT_MAX, FLT_MAX, FLT_MAX );
	m_vecRenderMaxs.Init( -FLT_MAX, -FLT_MAX, -FLT_MAX );

	// Set the initial directions and distances (in shield space)...
	for	( i = 0; i < SHIELD_NUM_VERTICAL_POINTS; ++i)
	{
		// Choose phi centered at pi/2
		float phi = (M_PI - m_ShieldPhi) * 0.5f + m_ShieldPhi * 
			(float)i / (float)(SHIELD_NUM_VERTICAL_POINTS - 1);

		for (int j = 0; j < SHIELD_NUM_HORIZONTAL_POINTS; ++j)
		{
			// Choose theta centered at pi/2 also (y, or forward axis)
			float theta = (M_PI - m_ShieldTheta) * 0.5f + m_ShieldTheta * 
				(float)j / (float)(SHIELD_NUM_HORIZONTAL_POINTS - 1);

			int idx = i * SHIELD_NUM_HORIZONTAL_POINTS + j;

			m_pFixedDirection[idx].x = cos(theta) * sin(phi);
			m_pFixedDirection[idx].y = sin(theta) * sin(phi);
			m_pFixedDirection[idx].z = cos(phi);

			m_pFixedDirection[idx] *= m_RestLength;

			VectorMin( m_vecRenderMins, m_pFixedDirection[idx], m_vecRenderMins );
			VectorMax( m_vecRenderMaxs, m_pFixedDirection[idx], m_vecRenderMaxs );
		}
	}

	// Compute box for fake volume testing
	Vector dist = m_pFixedDirection[0] - m_pFixedDirection[1];
	float l = dist.Length(); // * m_RestLength;
	SetShieldPanelSize( Vector( -l * 0.25f, -l * 0.25f, -l * 0.25f), 
		Vector( l * 0.25f, l * 0.25f, l * 0.25f) );
}


//-----------------------------------------------------------------------------
// Sets orientation + position
//-----------------------------------------------------------------------------
void CShieldEffect::SetDesiredOrigin( const Vector& origin )
{
	VectorCopy( origin, m_vecDesiredOrigin );
}

void CShieldEffect::SetDesiredAngles( const QAngle& angles )
{
	VectorCopy( angles, m_angDesiredAngles );
}

const QAngle& CShieldEffect::GetDesiredAngles() const
{
	return m_angDesiredAngles;
}


//-----------------------------------------------------------------------------
// Gets a point...
//-----------------------------------------------------------------------------
const Vector& CShieldEffect::GetPoint( int x, int y ) const
{
	return m_pControlPoint[ x + y * SHIELD_NUM_HORIZONTAL_POINTS ];
}

const Vector& CShieldEffect::GetPoint( int i ) const
{
	return m_pControlPoint[ i ];
}

Vector& CShieldEffect::GetPoint( int i )
{
	return m_pControlPoint[ i ];
}


//-----------------------------------------------------------------------------
// Sets the collision group
//-----------------------------------------------------------------------------
void CShieldEffect::SetCollisionGroup( int group )
{
	m_CollisionGroup = group;
}


//-----------------------------------------------------------------------------
// Hooks in active bits...
//-----------------------------------------------------------------------------
void CShieldEffect::SetActiveVertexList( IActiveVertList *pActiveVerts )
{
	m_pActiveVerts = pActiveVerts;

	// No points are visible initially
	for ( int i=0; i < SHIELD_VERTEX_BYTES*8; i++ )
		m_pActiveVerts->SetActiveVertState( i, 0 );
}


//-----------------------------------------------------------------------------
// Is a particular vertex active?
//-----------------------------------------------------------------------------
bool CShieldEffect::IsVertexActive( int x, int y ) const
{
	if ((x < 0) || (y < 0) || (x >= SHIELD_NUM_HORIZONTAL_POINTS) ||
		(y >= SHIELD_NUM_VERTICAL_POINTS))
	{
		return false;
	}

	int idx = x + (SHIELD_NUM_HORIZONTAL_POINTS) * y;
	return m_pActiveVerts->GetActiveVertState( idx ) != 0;
}


//-----------------------------------------------------------------------------
// Is a particular panel active?
//-----------------------------------------------------------------------------
bool CShieldEffect::IsPanelActive( int x, int y ) const
{
	if ((x < 0) || (y < 0) || (x >= SHIELD_HORIZONTAL_PANEL_COUNT) ||
		(y >= SHIELD_VERTICAL_PANEL_COUNT))
	{
		return false;
	}

	int idx = x + (SHIELD_HORIZONTAL_PANEL_COUNT) * y;
	return m_pActivePanels[idx];
}


//-----------------------------------------------------------------------------
// Recompute whether the panels are active or not
//-----------------------------------------------------------------------------
void CShieldEffect::ComputePanelActivity()
{
	// Check neighbors to see how many squares we've got
	for	( int i = 0; i < SHIELD_NUM_HORIZONTAL_POINTS - 1; ++i)
	{
		for ( int j = 0; j < SHIELD_NUM_VERTICAL_POINTS - 1; ++j)
		{
			int idx = i + j * (SHIELD_NUM_HORIZONTAL_POINTS - 1);

			// Test the neighbors
			m_pActivePanels[idx] = 
				IsVertexActive( i, j ) ||
				IsVertexActive( i+1, j ) ||
				IsVertexActive( i, j+1 ) ||
				IsVertexActive( i+1, j+1 );
		}
	}
}


//-----------------------------------------------------------------------------
// Compute vertex activity
//-----------------------------------------------------------------------------

enum
{
	SHIELD_TESTS_PER_FRAME = 4
};


void CShieldEffect::ComputeVertexActivity()
{
	int i;
	for ( i = 0; i < SHIELD_TESTS_PER_FRAME; ++i )
	{
		// Visit points in random order...
		int pt = m_PointList[m_TestPoint];

		// Collision test...
		// Check a line that goes farther out than our current point...
		// This will let us check for resting contact
		trace_t tr;
		CTraceFilterWorldOnly traceFilter;
		UTIL_TraceHull( m_Position, m_pControlPoint[pt], 
			m_PanelBoxMin, m_PanelBoxMax, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );
		bool isActive = (!tr.allsolid) && ( (tr.fraction - 1.0f) >= 0.0f );

		m_pActiveVerts->SetActiveVertState( pt, isActive );

		if (++m_TestPoint >= SHIELD_NUM_CONTROL_POINTS)
			m_TestPoint = 0;

	}

	ComputePanelActivity();
}


//-----------------------------------------------------------------------------
// bounding box for collision
//-----------------------------------------------------------------------------
void CShieldEffect::SetShieldPanelSize( Vector& mins, Vector& maxs )
{
	m_PanelBoxMin = mins;
	m_PanelBoxMax = maxs;
}


//-----------------------------------------------------------------------------
// bounding box for collision
//-----------------------------------------------------------------------------
void CShieldEffect::ComputeBounds( Vector& mins, Vector& maxs )
{
	VectorCopy( m_pControlPoint[0], mins );
	VectorCopy( m_pControlPoint[0], maxs );

	for (int i = 1; i < SHIELD_NUM_CONTROL_POINTS; ++i)
	{
		VectorMin( mins, m_pControlPoint[i], mins );
		VectorMax( maxs, m_pControlPoint[i], maxs );
	}

	// Bounds are in local coords
	mins -= m_Position;
	maxs -= m_Position;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CShieldEffect::Precache( void )
{
	m_RestLength = 200.0;

	m_SpringConstant = 30.0f;
	m_DampConstant = 4.0f; 
	m_ViscousDrag = 4.0f;
	m_Mass = 1.0f;

	m_AngularSpringConstant = 2.0f;
	m_AngularViscousDrag = 4.0f;

	SetThetaPhi( SHIELD_INITIAL_THETA, SHIELD_INITIAL_PHI );
}


//-----------------------------------------------------------------------------
// Compute orientation matrix: 
//-----------------------------------------------------------------------------
void CShieldEffect::ComputeOrientationMatrix()
{
	// Generate the orientation matrix from theta and phi...
	// X = forward direction, Y - left direction
	Vector forward, left, up;
	forward.x = cos(m_Theta) * sin(m_Phi);
	forward.y = sin(m_Theta) * sin(m_Phi);
	forward.z = cos(m_Phi);

	left.x = -forward.y;
	left.y = forward.x;
	left.z = 0;

	if ( VectorNormalize(left) == 0.0f )
		left.Init( 0.0f, 1.0f, 0.0f );

	CrossProduct( forward, left, up );

	m_Orientation.SetBasisVectors( forward, left, up );

	// Turn the current matrix into angles...
	MatrixToAngles( m_Orientation, m_CurrentAngles ); 
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CShieldEffect::Spawn( const Vector& currentPosition, const QAngle& currentAngles )
{
	Precache();

	VectorCopy( currentPosition, m_Position );
	m_Velocity.Init();
	
	Vector forward;
	AngleVectors( currentAngles, &forward, 0, 0 );
	m_Phi = acos( forward.z );
	m_Theta = atan2( forward.y, forward.x );
	m_PhiVelocity = 0.0f;
	m_ThetaVelocity = 0.0f;
	ComputeOrientationMatrix();
	VectorCopy( currentAngles, m_CurrentAngles );
	VectorCopy( currentAngles, m_angDesiredAngles );

	// No points are visible initially
	memset( m_pActivePanels, 0, SHIELD_PANELS_COUNT );

	m_TestPoint = 0;

	// Choose random order to visit shield verts
	int i;
	for ( i = 0; i < SHIELD_NUM_CONTROL_POINTS; ++i )
	{
		m_PointList[i] = i;
	}

	for ( i = 0; i < SHIELD_NUM_CONTROL_POINTS; ++i )
	{
		int j = rand() % SHIELD_NUM_CONTROL_POINTS;
		swap( m_PointList[i], m_PointList[j] ); 
	}
}


//-----------------------------------------------------------------------------
// Computes the opacity....
//-----------------------------------------------------------------------------
float CShieldEffect::ComputeOpacity( const Vector& pt, const Vector& center ) const
{
	float dist = pt.DistTo( center ) / m_RestLength;
	if (dist > 1.0)
		dist = 1.0f;
	return 32 + (1.0 - dist) * 192;
}


//-----------------------------------------------------------------------------
// Computes control points
//-----------------------------------------------------------------------------
void CShieldEffect::ComputeControlPoints()
{
	Vector forward, right, up;
	AngleVectors(m_CurrentAngles, &forward, &right, &up);

	for ( int i = 0; i < SHIELD_NUM_CONTROL_POINTS; ++i )
	{
		// Compute the world space position...
		VectorCopy( m_Position, m_pControlPoint[i] );
		m_pControlPoint[i] += right * m_pFixedDirection[i].x;
		m_pControlPoint[i] += up * m_pFixedDirection[i].z;
		m_pControlPoint[i] += forward * m_pFixedDirection[i].y;
	}
}


//-----------------------------------------------------------------------------
// Gets the frustum size
//-----------------------------------------------------------------------------
void CShieldEffect::GetPanelSize( Vector& mins, Vector& maxs ) const
{
	VectorCopy( m_PanelBoxMin, mins );
	VectorCopy( m_PanelBoxMax, maxs );
}


void CShieldEffect::SetAngularSpringConstant( float flConstant )
{
	m_AngularSpringConstant = flConstant;
}


//-----------------------------------------------------------------------------
// Set the shield theta & phi
//-----------------------------------------------------------------------------
void CShieldEffect::SetThetaPhi( float flTheta, float flPhi )
{
	m_ShieldTheta = M_PI * flTheta / 180.0f;
	m_ShieldPhi = M_PI * flPhi / 180.0f;

	// Computes the rest positions
	ComputeRestPositions();
}


//-----------------------------------------------------------------------------
// The current position (computed by Simulate on the server)
//-----------------------------------------------------------------------------
const Vector& CShieldEffect::GetCurrentPosition()
{
	return m_Position;
}

void CShieldEffect::SetCurrentPosition( const Vector& pos )
{
	m_Position = pos;
}

void CShieldEffect::SetCurrentAngles( const QAngle& angles )
{
	VectorCopy( angles, m_CurrentAngles );
}


//-----------------------------------------------------------------------------
// Simulate the center of mass 
//-----------------------------------------------------------------------------
void CShieldEffect::SimulateTranslation( float dt )
{
	// Hook's law for a damped spring:
	// got two particles, a and b with positions xa and xb and velocities va and vb
	// and l = xa - xb
	// fa = -( ks * (|l| - r) + kd * (va - vb) dot (l) / |l|) * l/|l|
	Vector dx, force;

	// Case where we're connected to a control point
	dx = m_Position - m_vecDesiredOrigin;

	// rest condition
	float length = dx.Length();
	float speedSq = m_Velocity.LengthSqr(); 
	if ((length < 1e-3) && (speedSq < 1e-6))
		return;

	// Compute force
	if (length > 1e-3)
		dx /= length;
	else
		dx.Init( 0, 0, 0 );

	float springfactor = m_SpringConstant * length;
	float dampfactor = m_DampConstant * DotProduct( m_Velocity, dx );
	force = dx * -( springfactor + dampfactor );

	assert( force.IsValid( ) );
	Vector drag = m_Velocity * m_ViscousDrag;
	force -= drag;

	// Update position and velocity
	m_Position += m_Velocity * dt; 
	m_Velocity += force * dt / m_Mass;

	assert( m_Velocity.IsValid( ) );

	// clamp for stability
	if (speedSq > 1e6)
	{
		m_Velocity *= 1e3 / sqrt(speedSq);
	}
}


void CShieldEffect::SimulateRotation( float dt, const Vector& forward )
{
	// Here's a torsional spring for the angular component...
	// A little tricky: We need to actually think about 2 torsional springs,
	// one in thetha (x-y plane), and one in phi (z-plane)

	float phi2 = acos( forward.z );
	float dPhi = m_Phi - phi2;

	float theta2 = atan2( forward.y, forward.x );
	float dTheta = (m_Theta - theta2);
	if (dTheta > M_PI)
		dTheta -= 2 * M_PI;
	else if (dTheta < -M_PI)
		dTheta += 2 * M_PI;

	// rest condition...
	if ((fabs(dTheta) < 1e-3) && (fabs(m_ThetaVelocity) < 1e-6) && 
		(fabs(dPhi) < 1e-3) && (fabs(m_PhiVelocity) < 1e-6))
	{
		return;
	}

	float springfactor = m_AngularSpringConstant * dTheta;
	float torqueTheta = -springfactor; // + dampfactor);
	torqueTheta -= m_ThetaVelocity * m_AngularViscousDrag;

	springfactor = m_AngularSpringConstant * dPhi;
	float torqueTPhi = -springfactor; // + dampfactor);
	torqueTPhi -= m_PhiVelocity * m_AngularViscousDrag;

	// Update position and velocity
	m_Theta += m_ThetaVelocity * dt; 
	m_ThetaVelocity += torqueTheta * dt;
	m_Phi += m_PhiVelocity * dt; 
	m_PhiVelocity += torqueTPhi * dt;

	// clamp for stability
	if (fabs(m_ThetaVelocity) > 1e2)
	{
		m_ThetaVelocity *= 1e2 / m_ThetaVelocity;
	}
	if (fabs(m_PhiVelocity) > 1e2)
	{
		m_PhiVelocity *= 1e2 / m_PhiVelocity;
	}

	ComputeOrientationMatrix();
}


//-----------------------------------------------------------------------------
// Update the shield position: 
//-----------------------------------------------------------------------------
void CShieldEffect::Simulate( float dt )
{
	// We're gonna basically assume a spring connected to the center control point
	Vector forward;
	AngleVectors(m_angDesiredAngles, &forward, 0, 0);

	// We've got two springs: a spring connected to the origin
	// and a torsional spring connected to the view direction.

	// Stiff spring, subdivide time....
	dt /= SHIELD_TIME_SUBVISIBIONS;
	for (int i = 0; i < SHIELD_TIME_SUBVISIBIONS; ++i)
	{
		SimulateTranslation( dt );
		SimulateRotation( dt, forward );
	}

	ComputeControlPoints();
}


//-----------------------------------------------------------------------------
// Determines shield obstructions 
//-----------------------------------------------------------------------------
static inline bool IsPointValid( bool* pActivePoints, int i, int j )
{
	// Here's the control point we're checking
	int idx = j * SHIELD_NUM_HORIZONTAL_POINTS + i;

	return pActivePoints[idx];
}



