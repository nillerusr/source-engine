//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmecamera.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "mathlib/vector.h"
#include "movieobjects/dmetransform.h"
#include "materialsystem/imaterialsystem.h"
#include "movieobjects_interfaces.h"
#include "tier2/tier2.h"

// FIXME: REMOVE
#include "istudiorender.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeCamera, CDmeCamera );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeCamera::OnConstruction()
{
	m_fieldOfView.InitAndSet( this, "fieldOfView", 30.0f );

	// FIXME: This currently matches the client DLL for HL2
	// but we probably need a way of getting this state from the client DLL
	m_zNear.InitAndSet( this, "znear", 3.0f );
	m_zFar.InitAndSet( this, "zfar", 16384.0f * 1.73205080757f );

	m_fFocalDistance.InitAndSet( this, "focalDistance", 72.0f);
	m_fAperture.InitAndSet( this, "aperture", 0.2f);
	m_fShutterSpeed.InitAndSet( this, "shutterSpeed", 1.0f / 48.0f );
	m_fToneMapScale.InitAndSet( this, "toneMapScale", 1.0f );
	m_fBloomScale.InitAndSet( this, "bloomScale", 0.28f );
	m_nDoFQuality.InitAndSet( this, "depthOfFieldQuality", 0 );
	m_nMotionBlurQuality.InitAndSet( this, "motionBlurQuality", 0 );
}

void CDmeCamera::OnDestruction()
{
}
	
//-----------------------------------------------------------------------------
// Loads the material system view matrix based on the transform
//-----------------------------------------------------------------------------
void CDmeCamera::LoadViewMatrix( bool bUseEngineCoordinateSystem )
{
	if ( !g_pMaterialSystem )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	VMatrix view;
	GetViewMatrix( view, bUseEngineCoordinateSystem );
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadMatrix( view );
}

//-----------------------------------------------------------------------------
// Loads the material system projection matrix based on the fov, etc.
//-----------------------------------------------------------------------------
void CDmeCamera::LoadProjectionMatrix( int nDisplayWidth, int nDisplayHeight )
{
	if ( !g_pMaterialSystem )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );

	VMatrix proj;
	GetProjectionMatrix( proj, nDisplayWidth, nDisplayHeight );
	pRenderContext->LoadMatrix( proj );
}

//-----------------------------------------------------------------------------
// Sets up studiorender camera state
//-----------------------------------------------------------------------------
void CDmeCamera::LoadStudioRenderCameraState()
{
	// FIXME: Remove this! This should automatically happen in DrawModel
	// in studiorender.
	if ( !g_pStudioRender )
		return;

	matrix3x4_t transform;
	GetTransform()->GetTransform( transform );

	Vector vecOrigin, vecRight, vecUp, vecForward;
	MatrixGetColumn( transform, 0, vecRight );
	MatrixGetColumn( transform, 1, vecUp );
	MatrixGetColumn( transform, 2, vecForward );
	MatrixGetColumn( transform, 3, vecOrigin );
	g_pStudioRender->SetViewState( vecOrigin, vecRight, vecUp, vecForward );
}

//-----------------------------------------------------------------------------
// Returns the x FOV (the full angle)
//-----------------------------------------------------------------------------
float CDmeCamera::GetFOVx() const
{
	return m_fieldOfView;
}

void CDmeCamera::SetFOVx( float fov )
{
	m_fieldOfView = fov;
}

//-----------------------------------------------------------------------------
// Returns the focal distance in inches
//-----------------------------------------------------------------------------
float CDmeCamera::GetFocalDistance() const
{
	return m_fFocalDistance;
}

//-----------------------------------------------------------------------------
// Sets the focal distance in inches
//-----------------------------------------------------------------------------
void CDmeCamera::SetFocalDistance( const float &fFocalDistance )
{
	m_fFocalDistance = fFocalDistance;
}

//-----------------------------------------------------------------------------
// Returns the camera aperture in inches
//-----------------------------------------------------------------------------
float CDmeCamera::GetAperture() const
{
	return m_fAperture;
}

//-----------------------------------------------------------------------------
// Returns the camera aperture in inches
//-----------------------------------------------------------------------------
float CDmeCamera::GetShutterSpeed() const
{
	return m_fShutterSpeed;
}

//-----------------------------------------------------------------------------
// Returns the tone map scale
//-----------------------------------------------------------------------------
float CDmeCamera::GetToneMapScale() const
{
	return m_fToneMapScale;
}

//-----------------------------------------------------------------------------
// Returns the bloom scale
//-----------------------------------------------------------------------------
float CDmeCamera::GetBloomScale() const
{
	return m_fBloomScale;
}

//-----------------------------------------------------------------------------
// Returns the number of Depth of Field samples
//-----------------------------------------------------------------------------
int CDmeCamera::GetDepthOfFieldQuality() const
{
	return m_nDoFQuality;
}

//-----------------------------------------------------------------------------
// Returns the number of Motion Blur samples
//-----------------------------------------------------------------------------
int CDmeCamera::GetMotionBlurQuality() const
{
	return m_nMotionBlurQuality;
}

//-----------------------------------------------------------------------------
// Returns the view direction
//-----------------------------------------------------------------------------
void CDmeCamera::GetViewDirection( Vector *pDirection )
{
	matrix3x4_t transform;
	GetTransform()->GetTransform( transform );
	MatrixGetColumn( transform, 2, *pDirection );

	// We look down the -z axis
	*pDirection *= -1.0f;
}

//-----------------------------------------------------------------------------
// Sets up render state in the material system for rendering
//-----------------------------------------------------------------------------
void CDmeCamera::SetupRenderState( int nDisplayWidth, int nDisplayHeight, bool bUseEngineCoordinateSystem /* = false */ )
{
	LoadViewMatrix( bUseEngineCoordinateSystem );
	LoadProjectionMatrix( nDisplayWidth, nDisplayHeight );
	LoadStudioRenderCameraState( );
}

//-----------------------------------------------------------------------------
// accessors for generated matrices
//-----------------------------------------------------------------------------
void CDmeCamera::GetViewMatrix( VMatrix &view, bool bUseEngineCoordinateSystem /* = false */ )
{
	matrix3x4_t transform, invTransform;
	CDmeTransform *pTransform = GetTransform();
	pTransform->GetTransform( transform );

	if ( bUseEngineCoordinateSystem )
	{
		VMatrix matRotate( transform );
		VMatrix matRotateZ;
		MatrixBuildRotationAboutAxis( matRotateZ, Vector(0,0,1), -90 );
		MatrixMultiply( matRotate, matRotateZ, matRotate );

		VMatrix matRotateX;
		MatrixBuildRotationAboutAxis( matRotateX, Vector(1,0,0), 90 );
		MatrixMultiply( matRotate, matRotateX, matRotate );
		transform = matRotate.As3x4();
	}

	MatrixInvert( transform, invTransform );
	view = invTransform;
}

void CDmeCamera::GetProjectionMatrix( VMatrix &proj, int width, int height )
{
	float flFOV = m_fieldOfView.Get();
	float flZNear = m_zNear.Get();
	float flZFar = m_zFar.Get();
	float flApsectRatio = (float)width / (float)height;

//	MatrixBuildPerspective( proj, flFOV, flFOV * flApsectRatio, flZNear, flZFar );

#if 1
	float halfWidth = tan( flFOV * M_PI / 360.0 );
	float halfHeight = halfWidth / flApsectRatio;
#else
	float halfHeight = tan( flFOV * M_PI / 360.0 );
	float halfWidth = flApsectRatio * halfHeight;
#endif
	memset( proj.Base(), 0, sizeof( proj ) );
	proj[0][0]  = 1.0f / halfWidth;
	proj[1][1]  = 1.0f / halfHeight;
	proj[2][2] = flZFar / ( flZNear - flZFar );
	proj[3][2] = -1.0f;
	proj[2][3] = flZNear * flZFar / ( flZNear - flZFar );
}

void CDmeCamera::GetViewProjectionInverse( VMatrix &viewprojinv, int width, int height )
{
	VMatrix view, proj;
	GetViewMatrix( view );
	GetProjectionMatrix( proj, width, height );

	VMatrix viewproj;
	MatrixMultiply( proj, view, viewproj );
	bool success = MatrixInverseGeneral( viewproj, viewprojinv );
	if ( !success )
	{
		Assert( 0 );
		MatrixInverseTR( viewproj, viewprojinv );
	}
}

//-----------------------------------------------------------------------------
// Computes the screen space position given a screen size
//-----------------------------------------------------------------------------
void CDmeCamera::ComputeScreenSpacePosition( const Vector &vecWorldPosition, int width, int height, Vector2D *pScreenPosition )
{
	VMatrix view, proj, viewproj;
	GetViewMatrix( view );
	GetProjectionMatrix( proj, width, height );
	MatrixMultiply( proj, view, viewproj );

	Vector vecScreenPos;
	Vector3DMultiplyPositionProjective( viewproj, vecWorldPosition, vecScreenPos );

	pScreenPosition->x = ( vecScreenPos.x + 1.0f ) * width / 2.0f;
	pScreenPosition->y = ( -vecScreenPos.y + 1.0f ) * height / 2.0f;
}


