//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CAMERA_H
#define CAMERA_H
#ifdef _WIN32
#pragma once
#endif


#include "mathlib/vmatrix.h"
#include "mathlib/vector4d.h"


#define OCTANT_X_POSITIVE		1
#define OCTANT_Y_POSITIVE		2
#define OCTANT_Z_POSITIVE		4


//
// Return values from BoxIsVisible.
//
enum Visibility_t
{
	VIS_NONE = 0,		// The box is completely outside the view frustum.
	VIS_PARTIAL,		// The box is partially inside the view frustum.
	VIS_TOTAL			// The box is completely inside the view frustum.
};


class CCamera  
{
	public:

		CCamera(void);
		virtual ~CCamera(void);

		void Move(Vector &vDelta);
		void Pitch(float fDegrees);
		void Roll(float fDegrees);
		void Yaw(float fDegrees);
		
		void MoveForward(float fUnits);
		void MoveRight(float fUnits);
		void MoveUp(float fUnits);
				
		void GetViewPoint(Vector& fViewPoint) const;
		void GetViewForward(Vector& ViewForward) const;
		void GetViewUp(Vector& ViewUp) const;
		void GetViewRight(Vector& ViewRight) const;
		
		void GetViewMatrix(VMatrix& Matrix);
        void GetProjMatrix(VMatrix& Matrix);
		void GetViewProjMatrix( VMatrix &Matrix );

		float GetYaw(void);
		float GetPitch(void);
		float GetRoll(void);
		QAngle GetAngles( );

		void SetYaw(float fDegrees);
		void SetPitch(float fDegrees);
		void SetRoll(float fDegrees);

		void SetViewPoint(const Vector &ViewPoint);
		void SetViewTarget(const Vector &ViewTarget);

		void	SetViewPort( int width, int height );
		void	GetViewPort( int &width, int &height );

		bool	IsOrthographic();
		void	SetFarClip(float fFarZ);
		void	SetNearClip(float fNearZ);
		float	GetNearClip(void);
		float	GetFarClip(void);

		void	SetPerspective(float fFOV, float fNearZ, float fFarZ);
		void	GetFrustumPlanes( Vector4D Planes[6] );
		float	GetFOV(void);
		
		void	SetOrthographic(float fZoom, float fNearZ, float fFarZ);
		void	SetZoom(float fScale);
        void	Zoom(float fScale);
        float	GetZoom(void);

		void	WorldToView( const Vector& vWorld, Vector2D &vView);
		void	ViewToWorld( const Vector2D &vView, Vector& vWorld);
		void	BuildRay( const Vector2D &vView, Vector& vStart, Vector& vEnd );

protected:

		void BuildViewMatrix();
		void BuildProjMatrix();
		void CameraIdentityMatrix(VMatrix& Matrix);

		VMatrix		m_ViewMatrix;	// Camera view matrix, based on current yaw, pitch, and roll.
		Vector		m_ViewPoint;
		float		m_fYaw;			// Counterclockwise rotation around the CAMERA_UP axis, in degrees [-359, 359].
		float		m_fPitch;		// Counterclockwise rotation around the CAMERA_RIGHT axis, in degrees [-90, 90].
		float		m_fRoll;		// Counterclockwise rotation around the CAMERA_FORWARD axis, in degrees [-359, 359].

		VMatrix		m_ProjMatrix;		// Camera projection matrix
		bool		m_bIsOrthographic;  // Camera projection mode
		float		m_fHorizontalFOV;	// Horizontal field of view in degrees.
		float		m_fNearClip;		// Distance to near clipping plane.
		float		m_fFarClip;			// Distance to far clipping plane.
        float		m_fZoom;			// Orthographic zoom scale

		float		m_fScaleHorz;
		float		m_fScaleVert;

		int			m_nViewWidth;
		int			m_nViewHeight;

		VMatrix		m_ViewProjMatrix;		// view and projection matrix
		VMatrix		m_InvViewProjMatrix;	// inverse view and projection matrix
};


#endif // CAMERA_H
