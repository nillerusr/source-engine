//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a camera for the 3D view.
//
// $NoKeywords: $
//=============================================================================//

#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "Camera.h"
#include "hammer_mathlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
// Indices of camera axes.
//
#define CAMERA_RIGHT		0
#define CAMERA_UP			1
#define CAMERA_FORWARD		2
#define CAMERA_ORIGIN		3

#define MIN_PITCH		-90.0f
#define MAX_PITCH		90.0f


static void DBG(PRINTF_FORMAT_STRING const char *fmt, ...)
{
    char ach[128];
    va_list va;

    va_start(va, fmt);
    vsprintf(ach, fmt, va);
    va_end(va);
    OutputDebugString(ach);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CCamera::CCamera(void)
{
	m_ViewPoint.Init();

	m_fPitch = 0.0;
	m_fRoll = 0.0;
	m_fYaw = 0.0;

	m_fHorizontalFOV = 90;
	m_fNearClip = 1.0;
	m_fFarClip = 5000;

    m_fZoom = 1.0f;
	m_bIsOrthographic = false;

	m_fScaleHorz = m_fScaleVert = 1;
	m_nViewWidth = m_nViewHeight = 100;

	BuildViewMatrix();
	BuildProjMatrix();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CCamera::~CCamera(void)
{
}

void CCamera::SetViewPort( int width, int height )
{
	if ( m_nViewWidth != width || m_nViewHeight != height )
	{
		m_nViewWidth = width;
		m_nViewHeight = height;
		BuildProjMatrix();
	}
}

void CCamera::GetViewPort( int &width, int &height )
{
	width = m_nViewWidth;
	height = m_nViewHeight;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the current value of the camera's pitch.
//-----------------------------------------------------------------------------
float CCamera::GetPitch(void)
{
	return(m_fPitch);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current value of the camera's roll.
//-----------------------------------------------------------------------------
float CCamera::GetRoll(void)
{
	return(m_fRoll);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current value of the camera's yaw.
//-----------------------------------------------------------------------------
float CCamera::GetYaw(void)
{
	return(m_fYaw);
}


//-----------------------------------------------------------------------------
// Purpose: returns the camera angles
// Output : returns the camera angles
//-----------------------------------------------------------------------------
QAngle CCamera::GetAngles()
{
	return QAngle( m_fPitch, m_fYaw, m_fRoll );
}


//-----------------------------------------------------------------------------
// Purpose: Moves the camera along the camera's right axis.
// Input  : fUnits - World units to move the camera.
//-----------------------------------------------------------------------------
void CCamera::MoveRight(float fUnits)
{
	if (fUnits != 0)
	{
		m_ViewPoint[0] += m_ViewMatrix[CAMERA_RIGHT][0] * fUnits;
		m_ViewPoint[1] += m_ViewMatrix[CAMERA_RIGHT][1] * fUnits;
		m_ViewPoint[2] += m_ViewMatrix[CAMERA_RIGHT][2] * fUnits;
		BuildViewMatrix();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Moves the camera along the camera's up axis.
// Input  : fUnits - World units to move the camera.
//-----------------------------------------------------------------------------
void CCamera::MoveUp(float fUnits)
{
	if (fUnits != 0)
	{
		m_ViewPoint[0] += m_ViewMatrix[CAMERA_UP][0] * fUnits;
		m_ViewPoint[1] += m_ViewMatrix[CAMERA_UP][1] * fUnits;
		m_ViewPoint[2] += m_ViewMatrix[CAMERA_UP][2] * fUnits;
		BuildViewMatrix();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Moves the camera along the camera's forward axis.
// Input  : fUnits - World units to move the camera.
//-----------------------------------------------------------------------------
void CCamera::MoveForward(float fUnits)
{
	if (fUnits != 0)
	{
		m_ViewPoint[0] -= m_ViewMatrix[CAMERA_FORWARD][0] * fUnits;
		m_ViewPoint[1] -= m_ViewMatrix[CAMERA_FORWARD][1] * fUnits;
		m_ViewPoint[2] -= m_ViewMatrix[CAMERA_FORWARD][2] * fUnits;
		BuildViewMatrix();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the camera's viewpoint.
//-----------------------------------------------------------------------------
void CCamera::GetViewPoint(Vector& ViewPoint) const
{
	ViewPoint = m_ViewPoint;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a vector indicating the camera's forward axis.
//-----------------------------------------------------------------------------
void CCamera::GetViewForward(Vector& ViewForward) const
{
	ViewForward[0] = -m_ViewMatrix[CAMERA_FORWARD][0];
	ViewForward[1] = -m_ViewMatrix[CAMERA_FORWARD][1];
	ViewForward[2] = -m_ViewMatrix[CAMERA_FORWARD][2];
}


//-----------------------------------------------------------------------------
// Purpose: Returns a vector indicating the camera's up axis.
//-----------------------------------------------------------------------------
void CCamera::GetViewUp(Vector& ViewUp) const
{
	ViewUp[0] = m_ViewMatrix[CAMERA_UP][0];
	ViewUp[1] = m_ViewMatrix[CAMERA_UP][1];
	ViewUp[2] = m_ViewMatrix[CAMERA_UP][2];
}


//-----------------------------------------------------------------------------
// Purpose: Returns a vector indicating the camera's right axis.
//-----------------------------------------------------------------------------
void CCamera::GetViewRight(Vector& ViewRight) const
{
	ViewRight[0] = m_ViewMatrix[CAMERA_RIGHT][0];
	ViewRight[1] = m_ViewMatrix[CAMERA_RIGHT][1];
	ViewRight[2] = m_ViewMatrix[CAMERA_RIGHT][2];
}

//-----------------------------------------------------------------------------
// Purpose: Returns the horizontal field of view in degrees.
//-----------------------------------------------------------------------------
float CCamera::GetFOV(void)
{
	return(m_fHorizontalFOV);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the distance from the camera to the near clipping plane in world units.
//-----------------------------------------------------------------------------
float CCamera::GetNearClip(void)
{
	return(m_fNearClip);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the distance from the camera to the far clipping plane in world units.
//-----------------------------------------------------------------------------
float CCamera::GetFarClip(void)
{
	return(m_fFarClip);
}

//-----------------------------------------------------------------------------
// Purpose: Sets up fields of view & clip plane distances for the view frustum.
// Input  : fHorizontalFOV - 
//			fVerticalFOV - 
//			fNearClip - 
//			fFarClip - 
//-----------------------------------------------------------------------------
void CCamera::SetPerspective(float fHorizontalFOV, float fNearClip, float fFarClip)
{
	m_bIsOrthographic = false;
	m_fHorizontalFOV = fHorizontalFOV;
	m_fNearClip = fNearClip;
	m_fFarClip = fFarClip;
	BuildProjMatrix();
}

void CCamera::SetOrthographic(float fZoom, float fNearClip, float fFarClip)
{
	m_fZoom = fZoom;
	m_fNearClip = fNearClip;
	m_fFarClip = fFarClip;
	m_bIsOrthographic = true;
	BuildProjMatrix();
}

void CCamera::GetFrustumPlanes( Vector4D Planes[6] )
{
	// TODO check for FrustumPlanesFromMatrix, maybe we can use that

	Vector ViewForward;
	GetViewForward(ViewForward);

	VMatrix CameraMatrix = m_ProjMatrix * m_ViewMatrix;

	//
	// Now the plane coefficients can be pulled directly out of the the camera
	// matrix as follows:
	//
	// Right :  first_column  - fourth_column
	// Left  : -first_column  - fourth_column
	// Top   :  second_column - fourth_column
	// Bottom: -second_column - fourth_column
	// Front : -third_column  - fourth_column
	// Back  :  third_column  + fourth_column
	//
	// dvs: My plane constants should be coming directly from the matrices,
	//		but they aren't (for some reason). Instead I calculate the plane
	//		constants myself. Sigh.
	//
	Planes[0][0] = CameraMatrix[0][0] - CameraMatrix[3][0];
	Planes[0][1] = CameraMatrix[0][1] - CameraMatrix[3][1];
	Planes[0][2] = CameraMatrix[0][2] - CameraMatrix[3][2];
	VectorNormalize(Planes[0].AsVector3D());
	Planes[0][3] = DotProduct(m_ViewPoint, Planes[0].AsVector3D());

	Planes[1][0] = -CameraMatrix[0][0] - CameraMatrix[3][0];
	Planes[1][1] = -CameraMatrix[0][1] - CameraMatrix[3][1];
	Planes[1][2] = -CameraMatrix[0][2] - CameraMatrix[3][2];
	VectorNormalize(Planes[1].AsVector3D());
	Planes[1][3] = DotProduct(m_ViewPoint, Planes[1].AsVector3D());

	Planes[2][0] = CameraMatrix[1][0] - CameraMatrix[3][0];
	Planes[2][1] = CameraMatrix[1][1] - CameraMatrix[3][1];
	Planes[2][2] = CameraMatrix[1][2] - CameraMatrix[3][2];
	VectorNormalize(Planes[2].AsVector3D());
	Planes[2][3] = DotProduct(m_ViewPoint, Planes[2].AsVector3D());

	Planes[3][0] = -CameraMatrix[1][0] - CameraMatrix[3][0];
	Planes[3][1] = -CameraMatrix[1][1] - CameraMatrix[3][1];
	Planes[3][2] = -CameraMatrix[1][2] - CameraMatrix[3][2];
	VectorNormalize(Planes[3].AsVector3D());
	Planes[3][3] = DotProduct(m_ViewPoint, Planes[3].AsVector3D());

	Planes[4][0] = -CameraMatrix[2][0] - CameraMatrix[3][0];
	Planes[4][1] = -CameraMatrix[2][1] - CameraMatrix[3][1];
	Planes[4][2] = -CameraMatrix[2][2] - CameraMatrix[3][2];
	VectorNormalize(Planes[4].AsVector3D());
	Planes[4][3] = DotProduct(m_ViewPoint + ViewForward * m_fNearClip, Planes[4].AsVector3D());

	Planes[5][0] = CameraMatrix[2][0] + CameraMatrix[3][0];
	Planes[5][1] = CameraMatrix[2][1] + CameraMatrix[3][1];
	Planes[5][2] = CameraMatrix[2][2] + CameraMatrix[3][2];
	VectorNormalize(Planes[5].AsVector3D());
	Planes[5][3] = DotProduct(m_ViewPoint + ViewForward * m_fFarClip, Planes[5].AsVector3D());
}

bool CCamera::IsOrthographic()
{
	return m_bIsOrthographic;
}

void CCamera::BuildProjMatrix()
{
	memset( &m_ProjMatrix,0,sizeof(m_ProjMatrix) );
	VMatrix &m = m_ProjMatrix;

	if ( m_bIsOrthographic )
	{
		// same as D3DXMatrixOrthoRH
		float w = (float)m_nViewWidth / m_fZoom;
		float h = (float)m_nViewHeight / m_fZoom;
	
		m[0][0] = 2/w;
		m[1][1] = 2/h;

		m[2][2] = 1/(m_fNearClip-m_fFarClip);
		m[2][3] = m_fNearClip/(m_fNearClip-m_fFarClip);

		m[3][3] = 1;
	}
	else
	{
		// same as D3DXMatrixPerspectiveRH
		float w = 2 * m_fNearClip * tan( m_fHorizontalFOV * M_PI / 360.0 );
		float h = ( w * float(m_nViewHeight) ) / float(m_nViewWidth);

		m[0][0] = 2*m_fNearClip/w;
		m[1][1] = 2*m_fNearClip/h;

		m[2][2] = m_fFarClip/(m_fNearClip-m_fFarClip);
		m[2][3] = (m_fNearClip*m_fFarClip)/(m_fNearClip-m_fFarClip);

		m[3][2] = -1;
	}

	m_ViewProjMatrix = m_ProjMatrix * m_ViewMatrix;
	m_ViewProjMatrix.InverseGeneral( m_InvViewProjMatrix );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the distance from the camera to the near clipping plane in world units.
//-----------------------------------------------------------------------------
void CCamera::SetNearClip(float fNearClip)
{
	if ( m_fNearClip != fNearClip )
	{
		m_fNearClip = fNearClip;
		BuildProjMatrix();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the distance from the camera to the far clipping plane in world units.
//-----------------------------------------------------------------------------
void CCamera::SetFarClip(float fFarClip)
{
	if ( m_fFarClip != fFarClip )
	{
		m_fFarClip = fFarClip;
		BuildProjMatrix();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the pitch in degrees, from [MIN_PITCH..MAX_PITCH]
//-----------------------------------------------------------------------------
void CCamera::SetPitch(float fDegrees)
{
	if (m_fPitch != fDegrees)
	{
		m_fPitch = fDegrees;

		if (m_fPitch > MAX_PITCH)
		{
			m_fPitch = MAX_PITCH;
		}
		else if (m_fPitch < MIN_PITCH)
		{
			m_fPitch = MIN_PITCH;
		}
		BuildViewMatrix();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the roll in degrees, from [0..360)
//-----------------------------------------------------------------------------
void CCamera::SetRoll(float fDegrees)
{
	while (fDegrees >= 360)
	{
		fDegrees -= 360;
	}

	while (fDegrees < 0)
	{
		fDegrees += 360;
	}

	if (m_fRoll != fDegrees)
	{
		m_fRoll = fDegrees;
		BuildViewMatrix();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the yaw in degrees, from [0..360)
//-----------------------------------------------------------------------------
void CCamera::SetYaw(float fDegrees)
{
	while (fDegrees >= 360)
	{
		fDegrees -= 360;
	}

	while (fDegrees < 0)
	{
		fDegrees += 360;
	}

	if (m_fYaw != fDegrees)
	{
		m_fYaw = fDegrees;
		BuildViewMatrix();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the view point.
//-----------------------------------------------------------------------------
void CCamera::SetViewPoint(const Vector &ViewPoint)
{
	if ( m_ViewPoint != ViewPoint )
	{
		m_ViewPoint = ViewPoint;
		BuildViewMatrix();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the camera target, rebuilding the view matrix.
// Input  : ViewTarget - the point in world space that the camera should look at.
//-----------------------------------------------------------------------------
void CCamera::SetViewTarget(const Vector &ViewTarget)
{
	Vector ViewOrigin;
	Vector ViewForward;

	GetViewPoint( ViewOrigin );

	VectorSubtract(ViewTarget, ViewOrigin, ViewForward);
	VectorNormalize(ViewForward);

	//
	// Ideally we could replace the math below with standard VectorAngles stuff, but Hammer
	// camera matrices use a different convention from QAngle (sadly).
	//
	float fYaw = RAD2DEG(atan2(ViewForward[0], ViewForward[1]));
	SetYaw(fYaw);

	float fPitch = -RAD2DEG(asin(ViewForward[2]));
	SetPitch(fPitch);
}


//-----------------------------------------------------------------------------
// Purpose: Pitches the camera forward axis toward the camera's down axis a
//			given number of degrees.
//-----------------------------------------------------------------------------
void CCamera::Pitch(float fDegrees)
{
	if (fDegrees != 0)
	{
		float fPitch = GetPitch();
		fPitch += fDegrees;
		SetPitch(fPitch);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Rolls the camera's right axis toward the camera's up axis a given
//			number of degrees.
//-----------------------------------------------------------------------------
void CCamera::Roll(float fDegrees)
{
	if (fDegrees != 0)
	{
		float fRoll = GetRoll();
		fRoll += fDegrees;
		SetRoll(fRoll);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Yaws the camera's forward axis toward the camera's right axis a
//			given number of degrees.
//-----------------------------------------------------------------------------
void CCamera::Yaw(float fDegrees)
{
	if (fDegrees != 0)
	{
		float fYaw = GetYaw();
		fYaw += fDegrees;
		SetYaw(fYaw);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Loads the given matrix with an identity matrix.
// Input  : Matrix - 4 x 4 matrix to be loaded with the identity matrix.
//-----------------------------------------------------------------------------
void CCamera::CameraIdentityMatrix(VMatrix& Matrix)
{
	// This function produces a transform which transforms from
	// material system camera space to quake camera space

	// Camera right axis lies along the world X axis.
	Matrix[CAMERA_RIGHT][0] = 1;
	Matrix[CAMERA_RIGHT][1] = 0;
	Matrix[CAMERA_RIGHT][2] = 0;
	Matrix[CAMERA_RIGHT][3] = 0;

	// Camera up axis lies along the world Z axis.
	Matrix[CAMERA_UP][0] = 0;
	Matrix[CAMERA_UP][1] = 0;
	Matrix[CAMERA_UP][2] = 1;
	Matrix[CAMERA_UP][3] = 0;

	// Camera forward axis lies along the negative world Y axis.
	Matrix[CAMERA_FORWARD][0] = 0;
	Matrix[CAMERA_FORWARD][1] = -1;
	Matrix[CAMERA_FORWARD][2] = 0;
	Matrix[CAMERA_FORWARD][3] = 0;

	Matrix[CAMERA_ORIGIN][0] = 0;
	Matrix[CAMERA_ORIGIN][1] = 0;
	Matrix[CAMERA_ORIGIN][2] = 0;
	Matrix[CAMERA_ORIGIN][3] = 1;
}

//-----------------------------------------------------------------------------
// Purpose: Generates a view matrix based on our current yaw, pitch, and roll.
//			The view matrix does not consider FOV or clip plane distances.
//-----------------------------------------------------------------------------
void CCamera::BuildViewMatrix()
{
	// The camera transformation is produced by multiplying roll * yaw * pitch.
	// This will transform a point from world space into quake camera space, 
	// which is exactly what we want for our view matrix. However, quake
	// camera space isn't the same as material system camera space, so
	// we're going to have to apply a transformation that goes from quake
	// camera space to material system camera space.
	
	CameraIdentityMatrix( m_ViewMatrix );

	RotateAroundAxis(m_ViewMatrix, m_fPitch, 0 );
	RotateAroundAxis(m_ViewMatrix, m_fRoll, 1);
	RotateAroundAxis(m_ViewMatrix, m_fYaw, 2);

	// Translate the viewpoint to the world origin.
	VMatrix TempMatrix;
	TempMatrix.Identity();
	TempMatrix.SetTranslation( -m_ViewPoint );

	m_ViewMatrix = m_ViewMatrix * TempMatrix;

	m_ViewProjMatrix = m_ProjMatrix * m_ViewMatrix;
	m_ViewProjMatrix.InverseGeneral( m_InvViewProjMatrix );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Matrix - 
//-----------------------------------------------------------------------------
void CCamera::GetViewMatrix(VMatrix& Matrix)
{
	Matrix = m_ViewMatrix;

}

void CCamera::GetProjMatrix(VMatrix& Matrix)
{
	Matrix = m_ProjMatrix;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the view matrix of the current projection
// Output : Matrix - the matrix to store the current projection matrix
//-----------------------------------------------------------------------------
void CCamera::GetViewProjMatrix( VMatrix &Matrix )
{
	Matrix = m_ViewProjMatrix;
}


//-----------------------------------------------------------------------------
// Purpose: to set the zoom in the orthographic gl view
//   Input: fScale - the zoom scale
//-----------------------------------------------------------------------------
void CCamera::SetZoom( float fScale )
{
	if ( m_fZoom != fScale )
	{
	    m_fZoom = fScale;
		BuildProjMatrix();
	}
}


//-----------------------------------------------------------------------------
// Purpose: to accumulate the zoom in the orthographic gl view
//   Input: fScale - the zoom scale
//-----------------------------------------------------------------------------
void CCamera::Zoom( float fScale )
{
    m_fZoom += fScale;

    // don't zoom negative 
    if( m_fZoom < 0.00001f )
        m_fZoom = 0.00001f;
}


//-----------------------------------------------------------------------------
// Purpose: to get the zoom in the orthographic gl view
//  Output: return the zoom scale
//-----------------------------------------------------------------------------
float CCamera::GetZoom( void )
{
    return m_fZoom;
}

void CCamera::WorldToView( const Vector& vWorld, Vector2D &vView )
{
	Vector vView3D;

	Vector3DMultiplyPositionProjective( m_ViewProjMatrix, vWorld, vView3D );

	// NOTE: The negative sign on y is because wc wants to think about screen
	// coordinates in a different way than the material system
	vView.x = 0.5 * (vView3D.x + 1.0) * m_nViewWidth;
	vView.y = 0.5 * (-vView3D.y + 1.0) * m_nViewHeight;
}

void CCamera::ViewToWorld( const Vector2D &vView, Vector& vWorld)
{	
	Vector vView3D;

	vView3D.x = 2.0 * vView.x / m_nViewWidth - 1;
	vView3D.y = -2.0 * vView.y / m_nViewHeight + 1;
	vView3D.z = 0;

	Vector3DMultiplyPositionProjective( m_InvViewProjMatrix, vView3D, vWorld );
}

void CCamera::BuildRay( const Vector2D &vView, Vector& vStart, Vector& vEnd )
{
	// Find the point they clicked on in world coordinates. It lies on the near
	// clipping plane.
	Vector vClickPoint;
	ViewToWorld(vView, vClickPoint );

	// Build a ray from the viewpoint through the point on the near clipping plane.
	Vector vRay = vClickPoint - m_ViewPoint;
	VectorNormalize( vRay );
	
	vStart = m_ViewPoint;
	vEnd = vStart + vRay * 99999;
}


