//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Rising liquid that acts as a one-way portal
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "c_func_liquidportal.h"
#include "debugoverlay_shared.h"
#include "view_scene.h"
#include "view.h"
#include "ScreenSpaceEffects.h"
#include "materialsystem/imaterialvar.h"


LINK_ENTITY_TO_CLASS( func_liquidportal, C_Func_LiquidPortal );

IMPLEMENT_CLIENTCLASS_DT( C_Func_LiquidPortal, DT_Func_LiquidPortal, CFunc_LiquidPortal )
	RecvPropEHandle( RECVINFO(m_hLinkedPortal) ),
	RecvPropFloat( RECVINFO(m_fFillStartTime) ),
	RecvPropFloat( RECVINFO(m_fFillEndTime) ),
END_RECV_TABLE()


#define LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( vertnum, xtexbase, xtexscale, ytexbase, ytexscale )\
	meshBuilder.Position3fv( &vVertices[vertnum].x );\
	meshBuilder.TexCoord2f( 0, vVertices[vertnum].xtexbase * xtexscale, vVertices[vertnum].ytexbase * ytexscale );\
	meshBuilder.AdvanceVertex();


C_Func_LiquidPortal::C_Func_LiquidPortal( void )
{
	g_pPortalRender->AddPortal( this );
}

C_Func_LiquidPortal::~C_Func_LiquidPortal( void )
{
	g_pPortalRender->RemovePortal( this );
}


int C_Func_LiquidPortal::DrawModel( int flags )
{
	if( IsFillingNow() )
	{
		DrawPortal();
		return 1;
	}

	return 0;
}

void C_Func_LiquidPortal::OnDataChanged( DataUpdateType_t updateType )
{
	GetRenderBoundsWorldspace( m_vAABBMins, m_vAABBMaxs );
	m_pLinkedPortal = m_hLinkedPortal.Get();
	ComputeLinkMatrix();
	UpdateBoundingPlanes();
}

void C_Func_LiquidPortal::ComputeLinkMatrix( void )
{
	C_Func_LiquidPortal *pLinkedPortal = m_hLinkedPortal.Get();
	if( pLinkedPortal )
	{
		VMatrix matLocalToWorld, matLocalToWorldInv, matRemoteToWorld;

		//matLocalToWorld.Identity();
		//matLocalToWorld.SetTranslation( CollisionProp()->WorldSpaceCenter() );
		matLocalToWorld = EntityToWorldTransform();

		//matRemoteToWorld.Identity();
		//matRemoteToWorld.SetTranslation( pLinkedPortal->CollisionProp()->WorldSpaceCenter() );
		matRemoteToWorld = pLinkedPortal->EntityToWorldTransform();

		MatrixInverseTR( matLocalToWorld, matLocalToWorldInv );
		m_matrixThisToLinked = matRemoteToWorld * matLocalToWorldInv;

		MatrixInverseTR( m_matrixThisToLinked, pLinkedPortal->m_matrixThisToLinked );
	}
	else
	{
		m_matrixThisToLinked.Identity();
	}
}




void CPortalRenderable_Func_LiquidPortal::UpdateBoundingPlanes( void )
{
	//x min
	m_fBoundingPlanes[0][0] = 1.0f;
	m_fBoundingPlanes[0][1] = 0.0f;
	m_fBoundingPlanes[0][2] = 0.0f;
	m_fBoundingPlanes[0][3] = m_vAABBMins.x;

	//x max
	m_fBoundingPlanes[1][0] = -1.0f;
	m_fBoundingPlanes[1][1] = 0.0f;
	m_fBoundingPlanes[1][2] = 0.0f;
	m_fBoundingPlanes[1][3] = -m_vAABBMaxs.x;


	//y min
	m_fBoundingPlanes[2][0] = 0.0f;
	m_fBoundingPlanes[2][1] = 1.0f;
	m_fBoundingPlanes[2][2] = 0.0f;
	m_fBoundingPlanes[2][3] = m_vAABBMins.y;

	//y max
	m_fBoundingPlanes[3][0] = 0.0f;
	m_fBoundingPlanes[3][1] = -1.0f;
	m_fBoundingPlanes[3][2] = 0.0f;
	m_fBoundingPlanes[3][3] = -m_vAABBMaxs.y;


	//z min
	m_fBoundingPlanes[4][0] = 0.0f;
	m_fBoundingPlanes[4][1] = 0.0f;
	m_fBoundingPlanes[4][2] = 1.0f;
	m_fBoundingPlanes[4][3] = m_vAABBMins.z;

	//z max is too variable to store
}

void CPortalRenderable_Func_LiquidPortal::DrawPreStencilMask( void )
{
	// Should we do something here like flatbasic?
}

void CPortalRenderable_Func_LiquidPortal::DrawStencilMask( void )
{
	DrawOutwardBox( g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
	DrawInnerLiquid( true, 1.0f, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
}

void CPortalRenderable_Func_LiquidPortal::DrawPostStencilFixes( void )
{
	DrawOutwardBox( g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
	DrawInnerLiquid( true, 1.0f, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
}


void CPortalRenderable_Func_LiquidPortal::RenderPortalViewToBackBuffer( CViewRender *pViewRender, const CViewSetup &cameraView )
{
	if( m_pLinkedPortal == NULL ) //not linked to any portal
		return;

	Frustum FrustumBackup;
	memcpy( FrustumBackup, pViewRender->GetFrustum(), sizeof( Frustum ) );

	Frustum seeThroughFrustum;
	bool bUseSeeThroughFrustum;

	if ( g_pPortalRender->GetViewRecursionLevel() == 0 )
	{
		bUseSeeThroughFrustum = CalcFrustumThroughPortal( cameraView.origin, seeThroughFrustum, pViewRender->GetFrustum(), FRUSTUM_NUMPLANES );
	}
	else
	{
		bUseSeeThroughFrustum = CalcFrustumThroughPortal( cameraView.origin, seeThroughFrustum );
	}

	Vector vCameraForward;
	AngleVectors( cameraView.angles, &vCameraForward, NULL, NULL );

	// Setup fog state for the camera.
	Vector ptPOVOrigin = m_matrixThisToLinked * cameraView.origin;	
	Vector vPOVForward = m_matrixThisToLinked.ApplyRotation( vCameraForward );

	CViewSetup portalView = cameraView;

	QAngle qPOVAngles = TransformAnglesToWorldSpace( cameraView.angles, m_matrixThisToLinked.As3x4() );	

	portalView.width = cameraView.width;
	portalView.height = cameraView.height;
	portalView.x = 0;
	portalView.y = 0;
	portalView.origin = ptPOVOrigin;
	portalView.angles = qPOVAngles;
	portalView.fov = cameraView.fov;
	portalView.m_bOrtho = false;
	portalView.m_flAspectRatio = cameraView.m_flAspectRatio; //use the screen aspect ratio, 0.0f doesn't work as advertised

	CopyToCurrentView( pViewRender, portalView );

	CMatRenderContextPtr pRenderContext( materials );

	{
		ViewCustomVisibility_t customVisibility;
		m_pLinkedPortal->AddToVisAsExitPortal( &customVisibility );
		render->Push3DView( portalView, 0, NULL, pViewRender->GetFrustum() );		
		{
			if( bUseSeeThroughFrustum)
				memcpy( pViewRender->GetFrustum(), seeThroughFrustum, sizeof( Frustum ) );

			render->OverrideViewFrustum( pViewRender->GetFrustum() );
			SetViewRecursionLevel( g_pPortalRender->GetViewRecursionLevel() + 1 );

			CPortalRenderable *pRenderingViewForPortalBackup = g_pPortalRender->GetCurrentViewEntryPortal();
			CPortalRenderable *pRenderingViewExitPortalBackup = g_pPortalRender->GetCurrentViewExitPortal();
			SetViewEntranceAndExitPortals( this, m_pLinkedPortal );

			//DRAW!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			ViewDrawScene_PortalStencil( pViewRender, portalView, &customVisibility );

			SetViewEntranceAndExitPortals( pRenderingViewForPortalBackup, pRenderingViewExitPortalBackup );

			SetViewRecursionLevel( g_pPortalRender->GetViewRecursionLevel() - 1 );
		}
		render->PopView( pViewRender->GetFrustum() );

		//restore old frustum
		memcpy( pViewRender->GetFrustum(), FrustumBackup, sizeof( Frustum ) );
		render->OverrideViewFrustum( FrustumBackup );
	}

	//restore old vis data
	CopyToCurrentView( pViewRender, cameraView );
}

void CPortalRenderable_Func_LiquidPortal::RenderPortalViewToTexture( CViewRender *pViewRender, const CViewSetup &cameraView )
{

}


void CPortalRenderable_Func_LiquidPortal::AddToVisAsExitPortal( ViewCustomVisibility_t *pCustomVisibility )
{
	if ( !pCustomVisibility )
		return;

	VisOverrideData_t visOverride;
	Vector vOrigin = (m_vAABBMins + m_vAABBMaxs) * 0.5f;

	visOverride.m_vecVisOrigin = vOrigin;
	visOverride.m_fDistToAreaPortalTolerance = 64.0f;				

	// Specify which leaf to use for area portal culling
	pCustomVisibility->ForceVisOverride( visOverride );
	pCustomVisibility->ForceViewLeaf( enginetrace->GetLeafContainingPoint( vOrigin ) );

	pCustomVisibility->AddVisOrigin( vOrigin );
}

bool CPortalRenderable_Func_LiquidPortal::DoesExitViewIntersectWaterPlane( float waterZ, int leafWaterDataID ) const
{
	return ((m_vAABBMins.z < waterZ) && (m_vAABBMaxs.z > waterZ));
}

SkyboxVisibility_t CPortalRenderable_Func_LiquidPortal::SkyBoxVisibleFromPortal( void )
{
	return SKYBOX_NOT_VISIBLE;
}


bool CPortalRenderable_Func_LiquidPortal::CalcFrustumThroughPortal( const Vector &ptCurrentViewOrigin, Frustum OutputFrustum, const VPlane *pInputFrustum, int iInputFrustumPlaneCount )
{
	return false;
}


const Vector& CPortalRenderable_Func_LiquidPortal::GetFogOrigin( void ) const
{
	return vec3_origin;
}

void CPortalRenderable_Func_LiquidPortal::ShiftFogForExitPortalView() const
{

}


bool CPortalRenderable_Func_LiquidPortal::ShouldUpdatePortalView_BasedOnView( const CViewSetup &currentView, Frustum currentFrustum )
{
	//return false;
	return IsFillingNow();
}

CPortalRenderable* CPortalRenderable_Func_LiquidPortal::GetLinkedPortal() const
{
	return m_pLinkedPortal;
}
	
bool CPortalRenderable_Func_LiquidPortal::ShouldUpdateDepthDoublerTexture( const CViewSetup &viewSetup )
{
	return false;
}

void CPortalRenderable_Func_LiquidPortal::DrawPortal( void )
{
	if( IsFillingNow() )
	{
		//"shadertest/gooinglass"
		//"glass/glasswindow_refract01"
		//IMaterial *pMaterial = materials->FindMaterial( "glass/glasswindow_refract01", TEXTURE_GROUP_OTHER );
		//UpdateFrontBufferTexturesForMaterial( (IMaterial *)pMaterial );

		DrawOutwardBox();
		//DrawInnerLiquid( pMaterial );
		//DrawInwardBox( pMaterial );
	}
}

void CPortalRenderable_Func_LiquidPortal::GetToolRecordingState( bool bActive, KeyValues *msg )
{

}

void CPortalRenderable_Func_LiquidPortal::HandlePortalPlaybackMessage( KeyValues *pKeyValues )
{

}

void CPortalRenderable_Func_LiquidPortal::DrawOutwardBox( const IMaterial *pMaterial )
{
	if( pMaterial == NULL )
		pMaterial = materials->FindMaterial( "glass/glasswindow_refract01", TEXTURE_GROUP_OTHER );

	const float fVerticalTextureScale = 1.0f / 100.0f;
	const float fHorizontalTextureScale = 1.0f / 100.0f;

	float fMaxZ = m_vAABBMins.z + ((m_vAABBMaxs.z - m_vAABBMins.z) * GetFillInterpolationAmount());

	Vector vVertices[8];
	for( int i = 0; i != 8; ++i )
	{
		vVertices[i].x = (i&(1<<0)) ? m_vAABBMaxs.x : m_vAABBMins.x;
		vVertices[i].y = (i&(1<<1)) ? m_vAABBMaxs.y : m_vAABBMins.y;
		vVertices[i].z = (i&(1<<2)) ? fMaxZ : m_vAABBMins.z;
	}

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( (IMaterial *)pMaterial, (CPortalRenderable_Func_LiquidPortal*)this );

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( false );
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 6 );

	//x min
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 2, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 6, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 4, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 0, y, fHorizontalTextureScale, z, -fVerticalTextureScale );

	//x max
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 1, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 5, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 7, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 3, y, fHorizontalTextureScale, z, -fVerticalTextureScale );

	//y min
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 0, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 4, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 5, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 1, x, fHorizontalTextureScale, z, -fVerticalTextureScale );

	//y max
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 3, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 7, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 6, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 2, x, fHorizontalTextureScale, z, -fVerticalTextureScale );


	//z min
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 1, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 3, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 2, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 0, x, fHorizontalTextureScale, y, fVerticalTextureScale );

	//z max
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 4, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 6, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 7, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 5, x, fHorizontalTextureScale, y, fVerticalTextureScale );

	meshBuilder.End();
	pMesh->Draw();
	pRenderContext->Flush( false );
}

void CPortalRenderable_Func_LiquidPortal::DrawInwardBox( const IMaterial *pMaterial )
{
	if( pMaterial == NULL )
		pMaterial = materials->FindMaterial( "glass/glasswindow_refract01", TEXTURE_GROUP_OTHER );

	const float fVerticalTextureScale = 1.0f / 100.0f;
	const float fHorizontalTextureScale = 1.0f / 100.0f;

	float fMaxZ = m_vAABBMins.z + ((m_vAABBMaxs.z - m_vAABBMins.z) * GetFillInterpolationAmount());

	Vector vVertices[8];
	for( int i = 0; i != 8; ++i )
	{
		vVertices[i].x = (i&(1<<0)) ? m_vAABBMaxs.x : m_vAABBMins.x;
		vVertices[i].y = (i&(1<<1)) ? m_vAABBMaxs.y : m_vAABBMins.y;
		vVertices[i].z = (i&(1<<2)) ? fMaxZ : m_vAABBMins.z;
	}

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( (IMaterial *)pMaterial, (CPortalRenderable_Func_LiquidPortal*)this );

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( false );
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 6 );

	//x min
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 0, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 4, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 6, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 2, y, fHorizontalTextureScale, z, -fVerticalTextureScale );

	//x max
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 3, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 7, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 5, y, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 1, y, fHorizontalTextureScale, z, -fVerticalTextureScale );

	//y min
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 1, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 5, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 4, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 0, x, fHorizontalTextureScale, z, -fVerticalTextureScale );

	//y max
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 2, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 6, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 7, x, fHorizontalTextureScale, z, -fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 3, x, fHorizontalTextureScale, z, -fVerticalTextureScale );


	//z min
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 0, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 2, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 3, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 1, x, fHorizontalTextureScale, y, fVerticalTextureScale );

	//z max
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 5, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 7, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 6, x, fHorizontalTextureScale, y, fVerticalTextureScale );
	LIQUIDPORTAL_DYNAMICMESH_BOX_ADDVERTEX( 4, x, fHorizontalTextureScale, y, fVerticalTextureScale );

	meshBuilder.End();
	pMesh->Draw();
	pRenderContext->Flush( false );
}

void CPortalRenderable_Func_LiquidPortal::DrawInnerLiquid( bool bClipToBounds, float fOpacity, const IMaterial *pMaterial ) //quads in front of camera clipped to box dimensions
{
	if( !IsFillingNow() && bClipToBounds )
		return;

	PortalMeshPoint_t WorkVertices[4];
	
	//view->GetViewSetup()->zNear;
	Vector vForward, vUp, vRight, vOrigin;
	vForward = CurrentViewForward();
	vUp = CurrentViewUp();
	vRight = CurrentViewRight();

	//vOrigin = CurrentViewOrigin() + vForward * (view->GetViewSetup()->zNear + 0.011f); //experimentation has shown this to be the optimal distance on the Nvidia 6800 cards we develop on
	vOrigin = CurrentViewOrigin() + vForward * (view->GetViewSetup()->zNear + 1.0f );

	const float fScalingAmount = 5.0f;

	WorkVertices[0].texCoord.x = fScalingAmount;
	WorkVertices[0].texCoord.y = fScalingAmount;

	WorkVertices[1].texCoord.x = fScalingAmount;
	WorkVertices[1].texCoord.y = 0.0f;

	WorkVertices[2].texCoord.x = 0.0f;
	WorkVertices[2].texCoord.y = 0.0f;

	WorkVertices[3].texCoord.x = 0.0f;
	WorkVertices[3].texCoord.y = fScalingAmount;


	WorkVertices[0].vWorldSpacePosition = vOrigin + (vRight * 40.0f) + (vUp * -40.0f);
	WorkVertices[1].vWorldSpacePosition = vOrigin + (vRight * 40.0f) + (vUp * 40.0f);
	WorkVertices[2].vWorldSpacePosition = vOrigin + (vRight * -40.0f) + (vUp * 40.0f);
	WorkVertices[3].vWorldSpacePosition = vOrigin + (vRight * -40.0f) + (vUp * -40.0f);

	PortalMeshPoint_t *pInVerts = (PortalMeshPoint_t *)stackalloc( 4 * (6) * 2 * sizeof( PortalMeshPoint_t ) ); //really only should need 2x points, but I'm paranoid
	PortalMeshPoint_t *pOutVerts = (PortalMeshPoint_t *)stackalloc( 4 * (6) * 2 * sizeof( PortalMeshPoint_t ) );
	
	PortalMeshPoint_t *pFinalVerts;
	int iVertCount;
	if( bClipToBounds )
	{
		PortalMeshPoint_t *pTempVerts;

		//clip by first plane and put output into pInVerts
		iVertCount = ClipPolyToPlane_LerpTexCoords( WorkVertices, 4, pInVerts, Vector( 0.0f, 0.0f, -1.0f ), -(m_vAABBMins.z + ((m_vAABBMaxs.z - m_vAABBMins.z) * GetFillInterpolationAmount())), 0.01f );

		//clip by other planes and flipflop in and out pointers
		for( int i = 0; i != 5; ++i )
		{
			if( iVertCount < 3 )
				return; //nothing to draw

			iVertCount = ClipPolyToPlane_LerpTexCoords( pInVerts, iVertCount, pOutVerts, *(Vector *)m_fBoundingPlanes[i], m_fBoundingPlanes[i][3], 0.01f );
			pTempVerts = pInVerts; pInVerts = pOutVerts; pOutVerts = pTempVerts; //swap vertex pointers
		}

		if( iVertCount < 3 )
			return; //nothing to draw

		pFinalVerts = pInVerts;
	}
	else
	{
		pFinalVerts = WorkVertices;
		iVertCount = 4;
	}

	bool bInterpOpacity = false;
	if( pMaterial == NULL )
	{
		pMaterial = materials->FindMaterial( "glass/glasswindow_refract01", TEXTURE_GROUP_OTHER );
		bInterpOpacity = ( fOpacity != 1.0f );
	}

	if( bInterpOpacity )
	{
		IMaterial *pEditMaterial = (IMaterial *)pMaterial; //we'll be making changes, then changing it back

		IMaterialVar *pRefractAmount = pEditMaterial->FindVar( "$refractamount", NULL );
		IMaterialVar *pBlurAmount = pEditMaterial->FindVar( "$bluramount", NULL );
		IMaterialVar *pTint = pEditMaterial->FindVar( "$refracttint", NULL );

		float fOriginalRefractAmount = pRefractAmount->GetFloatValue();
		float fOriginalBlurAmount = pBlurAmount->GetFloatValue();
		Vector4D vOriginalTint;
		pTint->GetVecValue( &vOriginalTint.x, 4 );

		Vector4D vModdedTint = vOriginalTint;

		pRefractAmount->SetFloatValue( fOriginalRefractAmount * fOpacity );
		pBlurAmount->SetFloatValue( fOriginalBlurAmount * fOpacity );
		vModdedTint.x = 1.0f - ((1.0f - vOriginalTint.x) * fOpacity);
		vModdedTint.y = 1.0f - ((1.0f - vOriginalTint.y) * fOpacity);
		vModdedTint.z = 1.0f - ((1.0f - vOriginalTint.z) * fOpacity);
		pTint->SetVecValue( &vModdedTint.x, 4 );

		Clip_And_Render_Convex_Polygon( pFinalVerts, iVertCount, pEditMaterial, this );
		materials->Flush();

		pRefractAmount->SetFloatValue( fOriginalRefractAmount );
		pBlurAmount->SetFloatValue( fOriginalBlurAmount );
		pTint->SetVecValue( &vOriginalTint.x, 4 );
	}
	else
	{
		Clip_And_Render_Convex_Polygon( pFinalVerts, iVertCount, pMaterial, this );
	}

	
}



ADD_SCREENSPACE_EFFECT( CLiquidPortal_InnerLiquidEffect, LiquidPortal_InnerLiquid );
const float	CLiquidPortal_InnerLiquidEffect::s_fFadeBackEffectTime = 5.0f;


CLiquidPortal_InnerLiquidEffect::CLiquidPortal_InnerLiquidEffect( void )
: m_bEnable(true),
	m_pImmersionPortal(NULL),
	m_bFadeBackToReality(false),
	m_fFadeBackTimeLeft(0.0f)
{	
}


void CLiquidPortal_InnerLiquidEffect::SetParameters( KeyValues *params )
{
	/*KeyValues *pImmersionPortal = params->FindKey( "immersion_portal" );
	if( pImmersionPortal )
		m_pImmersionPortal = (C_Func_LiquidPortal *)pImmersionPortal->GetPtr();*/
}


void CLiquidPortal_InnerLiquidEffect::Render( int x, int y, int w, int h )
{
	if( !m_pImmersionPortal || !m_bEnable )
		return;

	if( m_bFadeBackToReality )
	{
		//effect should cover whole screen and have a alpha-like fade back to normal view
		m_fFadeBackTimeLeft -= gpGlobals->absoluteframetime;
		if( m_fFadeBackTimeLeft > 0.0f )
		{
			float fInterp = m_fFadeBackTimeLeft/s_fFadeBackEffectTime;

			//clear depth buffer so we can be all warpy on the view model too
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->ClearBuffers( false, true, false );
			pRenderContext->OverrideDepthEnable( true, false );
			
			m_pImmersionPortal->DrawInnerLiquid( false, fInterp );

			pRenderContext->OverrideDepthEnable( false, true );
		}
		else
		{
			m_bFadeBackToReality = false;
			m_pImmersionPortal = NULL;
		}
	}
	else
	{
		//effect should only cover a portion of the screen and be in full warpiness

		//clear depth buffer so we can be all warpy on the view model too
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->ClearBuffers( false, true, false );
		pRenderContext->OverrideDepthEnable( true, false );

		m_pImmersionPortal->DrawInnerLiquid();

		pRenderContext->OverrideDepthEnable( false, true );
	}
}


