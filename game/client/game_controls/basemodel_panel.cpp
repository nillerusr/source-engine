//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "basemodel_panel.h"
#include "activitylist.h"
#include "animation.h"
#include "vgui/iinput.h"
#include "matsys_controls/manipulator.h"

using namespace vgui;
DECLARE_BUILD_FACTORY( CBaseModelPanel );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseModelPanel::CBaseModelPanel( vgui::Panel *pParent, const char *pName ): BaseClass( pParent, pName )
{
	m_bForcePos = false;
	m_bMousePressed = false;
	m_bAllowRotation = false;

	vgui::SETUP_PANEL( this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseModelPanel::~CBaseModelPanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: Load in the model portion of the panel's resource file.
//-----------------------------------------------------------------------------
void CBaseModelPanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	// Grab and set the camera FOV.
	float flFOV = GetCameraFOV();
	m_BMPResData.m_flFOV = inResourceData->GetInt( "fov", flFOV );
	SetCameraFOV( m_BMPResData.m_flFOV );

	// Do we allow rotation on these panels.
	m_bAllowRotation = ( inResourceData->GetInt( "allow_rot", 0 ) == 1 );

	// Parse our resource file and apply all necessary updates to the MDL.
 	for ( KeyValues *pData = inResourceData->GetFirstSubKey() ; pData != NULL ; pData = pData->GetNextKey() )
 	{
 		if ( !Q_stricmp( pData->GetName(), "model" ) )
 		{
 			ParseModelResInfo( pData );
 		}
 	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModelPanel::ParseModelResInfo( KeyValues *inResourceData )
{
	m_bForcePos = ( inResourceData->GetInt( "force_pos", 0 ) == 1 );
	m_BMPResData.m_pszModelName = ReadAndAllocStringValue( inResourceData, "modelname" );
	m_BMPResData.m_pszModelName_HWM = ReadAndAllocStringValue( inResourceData, "modelname_hwm" );
	m_BMPResData.m_pszVCD = ReadAndAllocStringValue( inResourceData, "vcd" );
	m_BMPResData.m_angModelPoseRot.Init( inResourceData->GetFloat( "angles_x", 0.0f ), inResourceData->GetFloat( "angles_y", 0.0f ), inResourceData->GetFloat( "angles_z", 0.0f ) );
	m_BMPResData.m_vecOriginOffset.Init( inResourceData->GetFloat( "origin_x", 110.0 ), inResourceData->GetFloat( "origin_y", 5.0 ), inResourceData->GetFloat( "origin_z", 5.0 ) );
	m_BMPResData.m_vecFramedOriginOffset.Init( inResourceData->GetFloat( "frame_origin_x", 110.0 ), inResourceData->GetFloat( "frame_origin_y", 5.0 ), inResourceData->GetFloat( "frame_origin_z", 5.0 ) );
	m_BMPResData.m_vecViewportOffset.Init();
	m_BMPResData.m_nSkin = inResourceData->GetInt( "skin", -1 );
	m_BMPResData.m_bUseSpotlight = ( inResourceData->GetInt( "spotlight", 0 ) == 1 );

	m_angPlayer = m_BMPResData.m_angModelPoseRot;
	m_vecPlayerPos = m_BMPResData.m_vecOriginOffset;

	for ( KeyValues *pData = inResourceData->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
	{
		if ( !Q_stricmp( pData->GetName(), "animation" ) )
		{
			ParseModelAnimInfo( pData );
		}
		else if ( !Q_stricmp( pData->GetName(), "attached_model" ) )
		{
			ParseModelAttachInfo( pData );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModelPanel::ParseModelAnimInfo( KeyValues *inResourceData )
{
	if ( !inResourceData )
		return;

	int iAnim = m_BMPResData.m_aAnimations.AddToTail();
	if ( iAnim == m_BMPResData.m_aAnimations.InvalidIndex() )
		return;

	m_BMPResData.m_aAnimations[iAnim].m_pszName = ReadAndAllocStringValue( inResourceData, "name" );
	m_BMPResData.m_aAnimations[iAnim].m_pszSequence = ReadAndAllocStringValue( inResourceData, "sequence" );
	m_BMPResData.m_aAnimations[iAnim].m_pszActivity = ReadAndAllocStringValue( inResourceData, "activity" );
	m_BMPResData.m_aAnimations[iAnim].m_bDefault = ( inResourceData->GetInt( "default", 0 ) == 1 );

	for ( KeyValues *pAnimData = inResourceData->GetFirstSubKey(); pAnimData != NULL; pAnimData = pAnimData->GetNextKey() )
	{
		if ( !Q_stricmp( pAnimData->GetName(), "pose_parameters" ) )
		{
			m_BMPResData.m_aAnimations[iAnim].m_pPoseParameters = pAnimData->MakeCopy();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModelPanel::ParseModelAttachInfo( KeyValues *inResourceData )
{
	if ( !inResourceData )
		return;

	int iAttach = m_BMPResData.m_aAttachModels.AddToTail();
	if ( iAttach == m_BMPResData.m_aAttachModels.InvalidIndex() )
		return;

	m_BMPResData.m_aAttachModels[iAttach].m_pszModelName = ReadAndAllocStringValue( inResourceData, "modelname" );
	m_BMPResData.m_aAttachModels[iAttach].m_nSkin = inResourceData->GetInt( "skin", -1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModelPanel::SetupModelDefaults( void )
{
	SetupModelAnimDefaults();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModelPanel::SetupModelAnimDefaults( void )
{
	// Verify that we have animations for this model.
	int nAnimCount = m_BMPResData.m_aAnimations.Count();
	if ( nAnimCount == 0 )
		return;

	// Find the default animation if one exists.
	int iIndex = FindDefaultAnim();
	if ( iIndex == -1 )
		return;

	SetModelAnim( iIndex );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseModelPanel::FindDefaultAnim( void )
{
	int iIndex = -1;

	int nAnimCount = m_BMPResData.m_aAnimations.Count();
	for ( int iAnim = 0; iAnim < nAnimCount; ++iAnim )
	{
		if ( m_BMPResData.m_aAnimations[iAnim].m_bDefault )
			return iAnim;
	}

	return iIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseModelPanel::FindAnimByName( const char *pszName )
{
	int iIndex = -1;

	int nAnimCount = m_BMPResData.m_aAnimations.Count();
	for ( int iAnim = 0; iAnim < nAnimCount; ++iAnim )
	{
		if ( !Q_stricmp( m_BMPResData.m_aAnimations[iAnim].m_pszName, pszName ) )
			return iAnim;
	}

	return iIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseModelPanel::FindSequenceFromActivity( CStudioHdr *pStudioHdr, const char *pszActivity )
{
	if ( !pStudioHdr )
		return -1;

	for ( int iSeq = 0; iSeq < pStudioHdr->GetNumSeq(); ++iSeq )
	{
		mstudioseqdesc_t &seqDesc = pStudioHdr->pSeqdesc( iSeq );
		if ( !stricmp( seqDesc.pszActivityName(), pszActivity ) )
		{
			return iSeq;
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModelPanel::SetModelAnim( int iAnim )
{
	int nAnimCount = m_BMPResData.m_aAnimations.Count();
	if ( nAnimCount == 0 || !m_BMPResData.m_aAnimations.IsValidIndex( iAnim ) )
		return;

	// Do we have an activity or a sequence?
	if ( m_BMPResData.m_aAnimations[iAnim].m_pszActivity && m_BMPResData.m_aAnimations[iAnim].m_pszActivity[0] )
	{
		SetModelAnim( m_BMPResData.m_aAnimations[iAnim].m_pszActivity );
	}
	else if ( m_BMPResData.m_aAnimations[iAnim].m_pszSequence && m_BMPResData.m_aAnimations[iAnim].m_pszSequence[0] )
	{
		SetModelAnim( m_BMPResData.m_aAnimations[iAnim].m_pszSequence );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModelPanel::SetModelAnim( const char *pszName )
{
	MDLCACHE_CRITICAL_SECTION();

	// Get the studio header of the root model.
	studiohdr_t *pStudioHdr = m_RootMDL.m_MDL.GetStudioHdr();
	if ( !pStudioHdr )
		return;

	CStudioHdr studioHdr( pStudioHdr, g_pMDLCache );

	int iSequence = ACT_INVALID;

	iSequence = FindSequenceFromActivity( &studioHdr, pszName );

	if ( iSequence == ACT_INVALID )
	{
		iSequence = LookupSequence( &studioHdr, pszName );
	}

	if ( iSequence != ACT_INVALID )
	{
		SetSequence( iSequence );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModelPanel::SetMDL( MDLHandle_t handle )
{
	BaseClass::SetMDL( handle );

	SetupModelDefaults();

	// Need to invalidate the layout so the panel will adjust is LookAt for the new model.
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModelPanel::SetMDL( const char *pMDLName )
{
	SetSequence( 0 );

	BaseClass::SetMDL( pMDLName );

	// Need to invalidate the layout so the panel will adjust is LookAt for the new model.
//	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModelPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( m_bForcePos )
	{
		ResetCameraPivot();
		SetCameraOffset( Vector( 0.0f, 0.0f, 0.0f ) );
		SetCameraPositionAndAngles( vec3_origin, vec3_angle );
		SetModelAnglesAndPosition( m_angPlayer, m_vecPlayerPos );
	}

	// Center and fill the frame with the model?
	if ( m_bStartFramed )
	{
		Vector vecBoundsMin, vecBoundsMax;
		if ( GetBoundingBox( vecBoundsMin, vecBoundsMax ) )
		{
			LookAtBounds( vecBoundsMin, vecBoundsMax );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseModelPanel::OnKeyCodePressed ( vgui::KeyCode code )
{
	return;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseModelPanel::OnKeyCodeReleased( vgui::KeyCode code )
{
	return;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseModelPanel::OnMousePressed ( vgui::MouseCode code )
{
	if ( !m_bAllowRotation )
		return;

	RequestFocus();

	EnableMouseCapture( true, code );

	// Warp the mouse to the center of the screen
	int width, height;
	GetSize( width, height );
	int x = width / 2;
	int y = height / 2;

	int xpos = x;
	int ypos = y;
	LocalToScreen( xpos, ypos );
	input()->SetCursorPos( xpos, ypos );

	m_nManipStartX = xpos;
	m_nManipStartY = ypos;

	m_bMousePressed = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseModelPanel::OnMouseReleased( vgui::MouseCode code )
{
	if ( !m_bAllowRotation )
		return;

	EnableMouseCapture( false );
	m_bMousePressed = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseModelPanel::OnCursorMoved( int x, int y )
{
	if ( !m_bAllowRotation )
		return;

	if ( m_bMousePressed )
	{
		WarpMouse( x, y );

		int xpos, ypos;
		input()->GetCursorPos( xpos, ypos );

		// Only want the x delta.
		float flDelta = xpos - m_nManipStartX;

		// Apply the delta and rotate the player.
		m_angPlayer.y += flDelta;
		if ( m_angPlayer.y > 360.0f )
		{
			m_angPlayer.y = m_angPlayer.y - 360.0f;
		}
		else if ( m_angPlayer.y < -360.0f )
		{
			m_angPlayer.y = m_angPlayer.y + 360.0f;
		}

		SetModelAnglesAndPosition( m_angPlayer, m_vecPlayerPos );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseModelPanel::OnMouseWheeled( int delta )
{
	return;
}

//-----------------------------------------------------------------------------
// Purpose: Set the camera to a distance that allows the object to fill the model panel.
//-----------------------------------------------------------------------------
void CBaseModelPanel::LookAtBounds( const Vector &vecBoundsMin, const Vector &vecBoundsMax )
{
	// Get the model space render bounds.
	Vector vecMin = vecBoundsMin;
	Vector vecMax = vecBoundsMax;
	Vector vecCenter = ( vecMax + vecMin ) * 0.5f;
	vecMin -= vecCenter;
	vecMax -= vecCenter;

	// Get the bounds points and transform them by the desired model panel rotation.
	Vector aBoundsPoints[8];
	aBoundsPoints[0].Init( vecMax.x, vecMax.y, vecMax.z ); 
	aBoundsPoints[1].Init( vecMin.x, vecMax.y, vecMax.z ); 
	aBoundsPoints[2].Init( vecMax.x, vecMin.y, vecMax.z ); 
	aBoundsPoints[3].Init( vecMin.x, vecMin.y, vecMax.z ); 
	aBoundsPoints[4].Init( vecMax.x, vecMax.y, vecMin.z ); 
	aBoundsPoints[5].Init( vecMin.x, vecMax.y, vecMin.z ); 
	aBoundsPoints[6].Init( vecMax.x, vecMin.y, vecMin.z ); 
	aBoundsPoints[7].Init( vecMin.x, vecMin.y, vecMin.z ); 

	// Translated center point (offset from camera center).
	Vector vecTranslateCenter = -vecCenter;

	// Build the rotation matrix.
	matrix3x4_t matRotation;
	AngleMatrix( m_BMPResData.m_angModelPoseRot, matRotation );

	Vector aXFormPoints[8];
	for ( int iPoint = 0; iPoint < 8; ++iPoint )
	{
		VectorTransform( aBoundsPoints[iPoint], matRotation, aXFormPoints[iPoint] );
	}

	Vector vecXFormCenter;
	VectorTransform( -vecTranslateCenter, matRotation, vecXFormCenter );

	int w, h;
	GetSize( w, h );
	float flW = (float)w;
	float flH = (float)h;

	float flFOVx = DEG2RAD( m_BMPResData.m_flFOV * 0.5f );
	float flFOVy = CalcFovY( ( m_BMPResData.m_flFOV * 0.5f ), flW/flH );
	flFOVy = DEG2RAD( flFOVy );

	float flTanFOVx = tan( flFOVx );
	float flTanFOVy = tan( flFOVy );

	// Find the max value of x, y, or z
	Vector2D dist[8];
	float flDist = 0.0f;
	for ( int iPoint = 0; iPoint < 8; ++iPoint )
	{
		float flDistY = fabs( aXFormPoints[iPoint].y / flTanFOVx ) - aXFormPoints[iPoint].x;
		float flDistZ = fabs( aXFormPoints[iPoint].z / flTanFOVy ) - aXFormPoints[iPoint].x;
		dist[iPoint].x = flDistY;
		dist[iPoint].y = flDistZ;
		float flTestDist = MAX( flDistZ, flDistY );
		flDist = MAX( flDist, flTestDist );
	}

	// Screen space points.
	Vector2D aScreenPoints[8];
	Vector aCameraPoints[8];
	for ( int iPoint = 0; iPoint < 8; ++iPoint )
	{
		aCameraPoints[iPoint] = aXFormPoints[iPoint];
		aCameraPoints[iPoint].x += flDist;

		aScreenPoints[iPoint].x = aCameraPoints[iPoint].y / ( flTanFOVx * aCameraPoints[iPoint].x );
		aScreenPoints[iPoint].y = aCameraPoints[iPoint].z / ( flTanFOVy * aCameraPoints[iPoint].x );

		aScreenPoints[iPoint].x = ( aScreenPoints[iPoint].x * 0.5f + 0.5f ) * flW;
		aScreenPoints[iPoint].y = ( aScreenPoints[iPoint].y * 0.5f + 0.5f ) * flH;
	}

	// Find the min/max and center of the 2D bounding box of the object.
	Vector2D vecScreenMin( 99999.0f, 99999.0f ), vecScreenMax( -99999.0f, -99999.0f );
	for ( int iPoint = 0; iPoint < 8; ++iPoint )
	{
		vecScreenMin.x = MIN( vecScreenMin.x, aScreenPoints[iPoint].x );
		vecScreenMin.y = MIN( vecScreenMin.y, aScreenPoints[iPoint].y );
		vecScreenMax.x = MAX( vecScreenMax.x, aScreenPoints[iPoint].x );
		vecScreenMax.y = MAX( vecScreenMax.y, aScreenPoints[iPoint].y );
	}

	// Offset the model to the be the correct distance away from the camera.
	Vector vecModelPos;
	vecModelPos.x = flDist - vecXFormCenter.x;
	vecModelPos.y = -vecXFormCenter.y;
	vecModelPos.z = -vecXFormCenter.z;
	SetModelAnglesAndPosition( m_BMPResData.m_angModelPoseRot, vecModelPos );

	// Back project to figure out the camera offset to center the model.
	Vector2D vecPanelCenter( ( flW * 0.5f ), ( flH * 0.5f ) );
	Vector2D vecScreenCenter = ( vecScreenMax + vecScreenMin ) * 0.5f;

	Vector2D vecPanelCenterCamera, vecScreenCenterCamera;
	vecPanelCenterCamera.x = ( ( vecPanelCenter.x / flW ) * 2.0f ) - 0.5f;
	vecPanelCenterCamera.y = ( ( vecPanelCenter.y / flH ) * 2.0f ) - 0.5f;
	vecPanelCenterCamera.x *= ( flTanFOVx * flDist );
	vecPanelCenterCamera.y *= ( flTanFOVy * flDist );
	vecScreenCenterCamera.x = ( ( vecScreenCenter.x / flW ) * 2.0f ) - 0.5f;
	vecScreenCenterCamera.y = ( ( vecScreenCenter.y / flH ) * 2.0f ) - 0.5f;
	vecScreenCenterCamera.x *= ( flTanFOVx * flDist );
	vecScreenCenterCamera.y *= ( flTanFOVy * flDist );

	Vector2D vecCameraOffset( 0.0f, 0.0f );
	vecCameraOffset.x = vecPanelCenterCamera.x - vecScreenCenterCamera.x;
	vecCameraOffset.y = vecPanelCenterCamera.y - vecScreenCenterCamera.y;

	// Clear the camera pivot and set position matrix.
	ResetCameraPivot();
	SetCameraOffset( Vector( 0.0f, -vecCameraOffset.x, -vecCameraOffset.y ) );
	UpdateCameraTransform();
}

