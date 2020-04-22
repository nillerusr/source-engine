//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "tier2/camerautils.h"
#include "tier0/dbg.h"
#include "mathlib/vector.h"
#include "mathlib/vmatrix.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// accessors for generated matrices
//-----------------------------------------------------------------------------
void ComputeViewMatrix( matrix3x4_t *pWorldToCamera, const Camera_t &camera )
{
	matrix3x4_t transform;
	AngleMatrix( camera.m_angles, camera.m_origin, transform );

	VMatrix matRotate( transform );
	VMatrix matRotateZ;
	MatrixBuildRotationAboutAxis( matRotateZ, Vector(0,0,1), -90 );
	MatrixMultiply( matRotate, matRotateZ, matRotate );

	VMatrix matRotateX;
	MatrixBuildRotationAboutAxis( matRotateX, Vector(1,0,0), 90 );
	MatrixMultiply( matRotate, matRotateX, matRotate );
	transform = matRotate.As3x4();

	MatrixInvert( transform, *pWorldToCamera );
}

void ComputeViewMatrix( VMatrix *pWorldToCamera, const Camera_t &camera )
{
	matrix3x4_t transform, invTransform;
	AngleMatrix( camera.m_angles, camera.m_origin, transform );

	VMatrix matRotate( transform );
	VMatrix matRotateZ;
	MatrixBuildRotationAboutAxis( matRotateZ, Vector(0,0,1), -90 );
	MatrixMultiply( matRotate, matRotateZ, matRotate );

	VMatrix matRotateX;
	MatrixBuildRotationAboutAxis( matRotateX, Vector(1,0,0), 90 );
	MatrixMultiply( matRotate, matRotateX, matRotate );
	transform = matRotate.As3x4();

	MatrixInvert( transform, invTransform );
	*pWorldToCamera = invTransform;
}

void ComputeProjectionMatrix( VMatrix *pCameraToProjection, const Camera_t &camera, int width, int height )
{
	float flFOV = camera.m_flFOV;
	float flZNear = camera.m_flZNear;
	float flZFar = camera.m_flZFar;
	float flApsectRatio = (float)width / (float)height;

//	MatrixBuildPerspective( proj, flFOV, flFOV * flApsectRatio, flZNear, flZFar );

#if 1
	float halfWidth = tan( flFOV * M_PI / 360.0 );
	float halfHeight = halfWidth / flApsectRatio;
#else
	float halfHeight = tan( flFOV * M_PI / 360.0 );
	float halfWidth = flApsectRatio * halfHeight;
#endif
	memset( pCameraToProjection, 0, sizeof( VMatrix ) );
	pCameraToProjection->m[0][0]  = 1.0f / halfWidth;
	pCameraToProjection->m[1][1]  = 1.0f / halfHeight;
	pCameraToProjection->m[2][2] = flZFar / ( flZNear - flZFar );
	pCameraToProjection->m[3][2] = -1.0f;
	pCameraToProjection->m[2][3] = flZNear * flZFar / ( flZNear - flZFar );
}


//-----------------------------------------------------------------------------
// Computes the screen space position given a screen size
//-----------------------------------------------------------------------------
void ComputeScreenSpacePosition( Vector2D *pScreenPosition, const Vector &vecWorldPosition, 
	const Camera_t &camera, int width, int height )
{
	VMatrix view, proj, viewproj;
	ComputeViewMatrix( &view, camera );
	ComputeProjectionMatrix( &proj, camera, width, height );
	MatrixMultiply( proj, view, viewproj );

	Vector vecScreenPos;
	Vector3DMultiplyPositionProjective( viewproj, vecWorldPosition, vecScreenPos );

	pScreenPosition->x = ( vecScreenPos.x + 1.0f ) * width / 2.0f;
	pScreenPosition->y = ( -vecScreenPos.y + 1.0f ) * height / 2.0f;
}


