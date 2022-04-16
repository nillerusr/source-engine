//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portalrenderable_flatbasic.h"
#include "clienteffectprecachesystem.h"
#include "Portal_DynamicMeshRenderingUtils.h"
#include "portal_shareddefs.h"
#include "view.h"
#include "c_pixel_visibility.h"
#include "glow_overlay.h"
#include "portal_render_targets.h"
#include "materialsystem/itexture.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "tier1/KeyValues.h"
#include "prop_portal_shared.h"
#include "view_scene.h"
#include "materialsystem/imaterialvar.h"
#include "tier0/vprof.h"


#define PORTALRENDERABLE_FLATBASIC_MINPIXELVIS 0.0f


CLIENTEFFECT_REGISTER_BEGIN( PrecacheFlatBasicPortalDrawingMaterials )
#if !defined( _X360 ) //XBox 360 is guaranteed to use stencil mode, and therefore doesn't need texture mode materials
CLIENTEFFECT_MATERIAL( "models/portals/portal_1_dynamicmesh" )
CLIENTEFFECT_MATERIAL( "models/portals/portal_2_dynamicmesh" )
CLIENTEFFECT_MATERIAL( "models/portals/portal_1_renderfix_dynamicmesh" )
CLIENTEFFECT_MATERIAL( "models/portals/portal_2_renderfix_dynamicmesh" )
#endif
CLIENTEFFECT_MATERIAL( "models/portals/portal_depthdoubler" )
CLIENTEFFECT_MATERIAL( "models/portals/portalstaticoverlay_1" )
CLIENTEFFECT_MATERIAL( "models/portals/portalstaticoverlay_2" )
CLIENTEFFECT_MATERIAL( "models/portals/portal_stencil_hole" )
CLIENTEFFECT_MATERIAL( "models/portals/portal_refract_1" )
CLIENTEFFECT_MATERIAL( "models/portals/portal_refract_2" )
//CLIENTEFFECT_MATERIAL( "effects/flashlight001" ) //light transfers disabled indefinitely
CLIENTEFFECT_REGISTER_END()

class CAutoInitFlatBasicPortalDrawingMaterials : public CAutoGameSystem
{
public:
	FlatBasicPortalRenderingMaterials_t m_Materials;
	void LevelInitPreEntity()
	{
		m_Materials.m_PortalMaterials[0].Init( "models/portals/portal_1_dynamicmesh", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalMaterials[1].Init( "models/portals/portal_2_dynamicmesh", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalRenderFixMaterials[0].Init( "models/portals/portal_1_renderfix_dynamicmesh", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalRenderFixMaterials[1].Init( "models/portals/portal_2_renderfix_dynamicmesh", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalDepthDoubler.Init( "models/portals/portal_depthdoubler", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalStaticOverlay[0].Init( "models/portals/portalstaticoverlay_1", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalStaticOverlay[1].Init( "models/portals/portalstaticoverlay_2", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_Portal_Stencil_Hole.Init( "models/portals/portal_stencil_hole", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_Portal_Refract[0].Init( "models/portals/portal_refract_1", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_Portal_Refract[1].Init( "models/portals/portal_refract_2", TEXTURE_GROUP_CLIENT_EFFECTS );
		//m_Materials.m_PortalLightTransfer_ShadowTexture.Init( "effects/flashlight001", TEXTURE_GROUP_OTHER ); //light transfers disabled indefinitely
	
		m_Materials.m_pDepthDoubleViewMatrixVar = m_Materials.m_PortalDepthDoubler->FindVar( "$alternateviewmatrix", NULL, false );
		Assert( m_Materials.m_pDepthDoubleViewMatrixVar != NULL );
	}
};
static CAutoInitFlatBasicPortalDrawingMaterials s_FlatBasicPortalDrawingMaterials;

const FlatBasicPortalRenderingMaterials_t& CPortalRenderable_FlatBasic::m_Materials = s_FlatBasicPortalDrawingMaterials.m_Materials;


LINK_ENTITY_TO_CLASS( prop_portal_flatbasic, CPortalRenderable_FlatBasic );


CPortalRenderable_FlatBasic::CPortalRenderable_FlatBasic( void ) 
: m_pLinkedPortal( NULL ),
	m_ptOrigin( 0.0f, 0.0f, 0.0f ),
	m_vForward( 1.0f, 0.0f, 0.0f ),
	m_vUp( 0.0f, 0.0f, 1.0f ),
	m_vRight( 0.0f, 1.0f, 0.0f ),
	m_fStaticAmount( 0.0f ),
	m_fSecondaryStaticAmount( 0.0f ),
	m_fOpenAmount( 0.0f ),
	m_bIsPortal2( false )
{
	m_InternallyMaintainedData.m_VisData.m_fDistToAreaPortalTolerance = 64.0f;
	m_InternallyMaintainedData.m_VisData.m_vecVisOrigin = Vector(0,0,0);
	m_InternallyMaintainedData.m_iViewLeaf = -1;

	m_InternallyMaintainedData.m_DepthDoublerTextureView.Identity();
	m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration = false;
	m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners = SKYBOX_NOT_VISIBLE;

	m_InternallyMaintainedData.m_ptForwardOrigin.Init( 1.0f, 0.0f, 0.0f );
	m_InternallyMaintainedData.m_ptCorners[0] = 
		m_InternallyMaintainedData.m_ptCorners[1] = 
		m_InternallyMaintainedData.m_ptCorners[2] =
		m_InternallyMaintainedData.m_ptCorners[3] =
		Vector( 0.0f, 0.0f, 0.0f );
}

void CPortalRenderable_FlatBasic::GetToolRecordingState( bool bActive, KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	VPROF_BUDGET( "CPortalRenderable_FlatBasic::GetToolRecordingState", VPROF_BUDGETGROUP_TOOLS );

	BaseClass::GetToolRecordingState( msg );
	CPortalRenderable::GetToolRecordingState( bActive, msg );

	C_Prop_Portal *pLinkedPortal = static_cast<C_Prop_Portal*>( m_pLinkedPortal );

	static PortalRecordingState_t state;
	state.m_nPortalId = static_cast<C_Prop_Portal*>( this )->index;
	state.m_nLinkedPortalId = pLinkedPortal ? pLinkedPortal->index : -1;
	state.m_fStaticAmount = m_fStaticAmount;
	state.m_fSecondaryStaticAmount = m_fSecondaryStaticAmount;
	state.m_fOpenAmount = m_fOpenAmount;
	state.m_bIsPortal2 = m_bIsPortal2;
	msg->SetPtr( "portal", &state );
}

void CPortalRenderable_FlatBasic::PortalMoved( void )
{
	m_InternallyMaintainedData.m_ptForwardOrigin = m_ptOrigin + m_vForward;
	m_InternallyMaintainedData.m_fPlaneDist = m_vForward.Dot( m_ptOrigin );

	// Update the points on the portal which we add to PVS
	{
		Vector vScaledRight = m_vRight * PORTAL_HALF_WIDTH;
		Vector vScaledUp = m_vUp * PORTAL_HALF_HEIGHT;

		m_InternallyMaintainedData.m_ptCorners[0] = (m_InternallyMaintainedData.m_ptForwardOrigin + vScaledRight) + vScaledUp;
		m_InternallyMaintainedData.m_ptCorners[1] = (m_InternallyMaintainedData.m_ptForwardOrigin - vScaledRight) + vScaledUp;
		m_InternallyMaintainedData.m_ptCorners[2] = (m_InternallyMaintainedData.m_ptForwardOrigin - vScaledRight) - vScaledUp;
		m_InternallyMaintainedData.m_ptCorners[3] = (m_InternallyMaintainedData.m_ptForwardOrigin + vScaledRight) - vScaledUp;


		m_InternallyMaintainedData.m_VisData.m_vecVisOrigin = m_InternallyMaintainedData.m_ptForwardOrigin;
		m_InternallyMaintainedData.m_VisData.m_fDistToAreaPortalTolerance = 64.0f;				
		m_InternallyMaintainedData.m_iViewLeaf = enginetrace->GetLeafContainingPoint( m_InternallyMaintainedData.m_ptForwardOrigin );
	}

	m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners = engine->IsSkyboxVisibleFromPoint( m_InternallyMaintainedData.m_ptForwardOrigin );
	for( int i = 0; i < 4 && ( m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners != SKYBOX_3DSKYBOX_VISIBLE ); ++i )
	{
		SkyboxVisibility_t nCornerVis = engine->IsSkyboxVisibleFromPoint( m_InternallyMaintainedData.m_ptCorners[i] );
		if ( ( m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners == SKYBOX_NOT_VISIBLE ) || ( nCornerVis != SKYBOX_NOT_VISIBLE ) )
		{
			m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners = nCornerVis;
		}
	}

	//render fix bounding planes
	{
		for( int i = 0; i != PORTALRENDERFIXMESH_OUTERBOUNDPLANES; ++i )
		{
			float fCirclePos = ((float)(i)) * ((M_PI * 2.0f) / (float)PORTALRENDERFIXMESH_OUTERBOUNDPLANES);
			float fUpBlend = cosf( fCirclePos );
			float fRightBlend = sinf( fCirclePos );

			Vector vNormal = -fUpBlend * m_vUp - fRightBlend * m_vRight;
			Vector ptOnPlane = m_ptOrigin + (m_vUp * (fUpBlend * PORTAL_HALF_HEIGHT * 1.1f)) + (m_vRight * (fRightBlend * PORTAL_HALF_WIDTH * 1.1f));

			m_InternallyMaintainedData.m_BoundingPlanes[i].Init( vNormal, vNormal.Dot( ptOnPlane ) );
		}

		m_InternallyMaintainedData.m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES].Init( -m_vForward, (-m_vForward).Dot( m_ptOrigin ) );
		m_InternallyMaintainedData.m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES + 1].Init( m_vForward, m_vForward.Dot( m_ptOrigin - (m_vForward * 5.0f) ) );
	}

	//update depth doubler usability flag
	m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration = 
		( m_pLinkedPortal && //linked to another portal
		( m_vForward.Dot( m_pLinkedPortal->m_ptOrigin - m_ptOrigin ) > 0.0f ) && //this portal looking in the general direction of the other portal
		( m_vForward.Dot( m_pLinkedPortal->m_vForward ) < -0.7071f ) ); //within 45 degrees of facing directly at each other

	if( m_pLinkedPortal )
		m_pLinkedPortal->m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration = true;



	//lastly, update link matrix
	if ( m_pLinkedPortal != NULL )
	{
		matrix3x4_t localToWorld( m_vForward, -m_vRight, m_vUp, m_ptOrigin );
		matrix3x4_t remoteToWorld( m_pLinkedPortal->m_vForward, -m_pLinkedPortal->m_vRight, m_pLinkedPortal->m_vUp, m_pLinkedPortal->m_ptOrigin );
		CProp_Portal_Shared::UpdatePortalTransformationMatrix( localToWorld, remoteToWorld, &m_matrixThisToLinked );

		// update the remote portal
		MatrixInverseTR( m_matrixThisToLinked, m_pLinkedPortal->m_matrixThisToLinked );
	}
	else
	{
		m_matrixThisToLinked.Identity(); // don't accidentally teleport objects to zero space
	}
}




bool CPortalRenderable_FlatBasic::WillUseDepthDoublerThisDraw( void ) const
{
	return g_pPortalRender->ShouldUseStencilsToRenderPortals() &&
		m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration && 
		(g_pPortalRender->GetRemainingPortalViewDepth() == 0) && 
		(g_pPortalRender->GetViewRecursionLevel() > 1) &&
		(g_pPortalRender->GetCurrentViewExitPortal() != this);
}



ConVar r_portal_use_complex_frustums( "r_portal_use_complex_frustums", "1", FCVAR_CLIENTDLL, "View optimization, turn this off if you get odd visual bugs." );

bool CPortalRenderable_FlatBasic::CalcFrustumThroughPortal( const Vector &ptCurrentViewOrigin, Frustum OutputFrustum )
{
	if( r_portal_use_complex_frustums.GetBool() == false )
		return false;

	int i;

	int iViewRecursionLevel = g_pPortalRender->GetViewRecursionLevel();
	int iNextViewRecursionLevel = iViewRecursionLevel + 1;

	if( (iViewRecursionLevel == 0) && 
		( (ptCurrentViewOrigin - m_ptOrigin).LengthSqr() < (PORTAL_HALF_HEIGHT * PORTAL_HALF_HEIGHT) ) )//FIXME: Player closeness check might need reimplementation
	{
		//calculations are most likely going to be completely useless, return nothing
		return false;
	}

	if( m_pLinkedPortal == NULL )
		return false;

	if( m_vForward.Dot( ptCurrentViewOrigin ) <= m_InternallyMaintainedData.m_fPlaneDist )
		return false; //looking at portal backface

	//VPlane *pInputFrustum = view->GetFrustum(); //g_pPortalRender->m_RecursiveViewComplexFrustums[iViewRecursionLevel].Base();
	//int iInputFrustumPlaneCount = 6; //g_pPortalRender->m_RecursiveViewComplexFrustums[iViewRecursionLevel].Count();
	VPlane *pInputFrustum = g_pPortalRender->m_RecursiveViewComplexFrustums[iViewRecursionLevel].Base();
	int iInputFrustumPlaneCount = g_pPortalRender->m_RecursiveViewComplexFrustums[iViewRecursionLevel].Count();
	Assert( iInputFrustumPlaneCount > 0 );

	Vector ptTempWork[2];
	int iAllocSize = 4 + iInputFrustumPlaneCount;

	Vector *pInVerts = (Vector *)stackalloc( sizeof( Vector ) * iAllocSize * 2 ); //possible to add 1 point per cut, 4 starting points, iInputFrustumPlaneCount cuts
	Vector *pOutVerts = pInVerts + iAllocSize;
	Vector *pTempVerts;

	//clip by first plane and put output into pInVerts
	int iVertCount = ClipPolyToPlane( m_InternallyMaintainedData.m_ptCorners, 4, pInVerts, pInputFrustum[0].m_Normal, pInputFrustum[0].m_Dist, 0.01f );

	//clip by other planes and flipflop in and out pointers
	for( i = 1; i != iInputFrustumPlaneCount; ++i )
	{
		if( iVertCount < 3 )
			return false; //nothing left in the frustum

		iVertCount = ClipPolyToPlane( pInVerts, iVertCount, pOutVerts, pInputFrustum[i].m_Normal, pInputFrustum[i].m_Dist, 0.01f );
		pTempVerts = pInVerts; pInVerts = pOutVerts; pOutVerts = pTempVerts; //swap vertex pointers
	}

	if( iVertCount < 3 )
		return false; //nothing left in the frustum

	g_pPortalRender->m_RecursiveViewComplexFrustums[iNextViewRecursionLevel].SetCount( iVertCount + 2 ); //+2 for near and far z planes

	Vector ptTransformedCamera = m_matrixThisToLinked * ptCurrentViewOrigin;

	//generate planes defined by each line around the convex and the camera origin
	for( i = 0; i != iVertCount; ++i )
	{
		Vector *p1, *p2;
		p1 = &pInVerts[i];
		p2 = &pInVerts[(i+1)%iVertCount];

		Vector vLine1 = *p1 - ptCurrentViewOrigin;
		Vector vLine2 = *p2 - ptCurrentViewOrigin;
		Vector vNormal = vLine1.Cross( vLine2 );
		vNormal.NormalizeInPlace();

		vNormal = m_matrixThisToLinked.ApplyRotation( vNormal );
		g_pPortalRender->m_RecursiveViewComplexFrustums[iNextViewRecursionLevel].Element(i).Init( vNormal, vNormal.Dot( ptTransformedCamera ) );
	}

	//Near Z
	g_pPortalRender->m_RecursiveViewComplexFrustums[iNextViewRecursionLevel].Element(i).Init( m_pLinkedPortal->m_vForward, m_pLinkedPortal->m_InternallyMaintainedData.m_fPlaneDist );

	//Far Z
	++i;
	{
		Vector vNormal = m_matrixThisToLinked.ApplyRotation( pInputFrustum[iInputFrustumPlaneCount - 1].m_Normal );
		Vector ptOnPlane = pInputFrustum[iInputFrustumPlaneCount - 1].m_Dist * pInputFrustum[iInputFrustumPlaneCount - 1].m_Normal;
		g_pPortalRender->m_RecursiveViewComplexFrustums[iNextViewRecursionLevel].Element(i).Init( vNormal, vNormal.Dot( m_matrixThisToLinked * ptOnPlane ) );
	}




	if( iVertCount > 4 )
	{
		float *fLineLengthSqr = (float *)stackalloc( sizeof( float ) * iVertCount );
		VPlane *Planes = (VPlane *)stackalloc( sizeof( VPlane ) * iVertCount );

		memcpy( Planes, g_pPortalRender->m_RecursiveViewComplexFrustums[iNextViewRecursionLevel].Base(), sizeof( VPlane ) * iVertCount );

		for( i = 0; i != (iVertCount - 1); ++i )
		{
			fLineLengthSqr[i] = (pInVerts[i + 1] - pInVerts[i]).LengthSqr();
		}
		fLineLengthSqr[i] = (pInVerts[0] - pInVerts[i]).LengthSqr(); //wrap around


		while( iVertCount > 4 )
		{
			//we have too many verts to represent this accurately as a frustum,
			//so, we're going to eliminate the smallest sides and bridge the surrounding sides until we're down to 4

			float fMinSide = fLineLengthSqr[0];
			int iMinSideFirstPoint = 0;
			int iOldVertCount = iVertCount;
			--iVertCount; //we're going to decrement this sometime in this block, it makes math easier to do it now

			for( i = 1; i != iOldVertCount; ++i )
			{
				if( fLineLengthSqr[i] < fMinSide )
				{
					fMinSide = fLineLengthSqr[i];
					iMinSideFirstPoint = i;
				}
			}

			int i1, i2, i3, i4;
			i1 = (iMinSideFirstPoint + iVertCount)%(iOldVertCount);
			i2 = iMinSideFirstPoint;
			i3 = (iMinSideFirstPoint + 1)%(iOldVertCount);
			i4 = (iMinSideFirstPoint + 2)%(iOldVertCount);

			Vector *p1, *p2, *p3, *p4;
			p1 = &pInVerts[i1];
			p2 = &pInVerts[i2];
			p3 = &pInVerts[i3];
			p4 = &pInVerts[i4];


			//now we know the two points that we have to merge to one, project and make a merged point from the surrounding lines
			if( fMinSide >= 0.1f ) //only worth doing the math if it's actually going to be accurate and make a difference
			{
				Vector vLine1 = *p2 - *p1;
				Vector vLine2 = *p3 - *p4;

				Vector vLine1Normal = vLine1.Cross( m_vForward );
				vLine1Normal.NormalizeInPlace();

				float fNormalDot = vLine1Normal.Dot( vLine2 );

				AssertMsgOnce( fNormalDot != 0.0f, "Tell Dave Kircher if this pops up. It won't interfere with gameplay though" );
				if( fNormalDot == 0.0f )
				{
					return false; //something went horribly wrong, bail and just suffer a slight framerate penalty for now
				}

				float fDist = vLine1Normal.Dot(*p1 - *p4); 
				*p2 = *p4 + (vLine2 * (fDist/fNormalDot));
			}

			fLineLengthSqr[i1] = (*p2 - *p1).LengthSqr();
			fLineLengthSqr[i2] = (*p4 - *p2).LengthSqr(); //must do this BEFORE possibly shifting points p4+ left

			if( i2 < i3 )
			{
				VPlane *v2 = &Planes[iMinSideFirstPoint];
				memcpy( v2, v2 + 1, sizeof( VPlane ) * (iVertCount - iMinSideFirstPoint) );
			}

			if( i3 < i4 ) //not the last point in the array
			{
				int iElementShift = (iOldVertCount - i4);

				//eliminate p3, we merged p2+p3 and already stored the result in p2
				memcpy( p3, p3 + 1, sizeof( Vector ) * iElementShift );

				float *l3 = &fLineLengthSqr[i3];
				memcpy( l3, l3 + 1, sizeof( float ) * iElementShift );
			}
		}

		for( i = 0; i != 4; ++i )
		{
			OutputFrustum[i] = Planes[i];
		}
	}
	else
	{
		for( i = 0; i != iVertCount; ++i )
		{
			OutputFrustum[i] = g_pPortalRender->m_RecursiveViewComplexFrustums[iNextViewRecursionLevel].Element(i);
		}

		for( ; i != 4; ++i )
		{
			//we had less than 4 planes for the sides, just copy from the last valid plane
			OutputFrustum[i] = OutputFrustum[iVertCount-1];
		}
	}

	//copy near/far planes
	int iComplexCount = g_pPortalRender->m_RecursiveViewComplexFrustums[iNextViewRecursionLevel].Count();
	OutputFrustum[FRUSTUM_NEARZ] = g_pPortalRender->m_RecursiveViewComplexFrustums[iNextViewRecursionLevel].Element(iComplexCount-2);
	OutputFrustum[FRUSTUM_FARZ] = g_pPortalRender->m_RecursiveViewComplexFrustums[iNextViewRecursionLevel].Element(iComplexCount-1);

	return true;
}



void CPortalRenderable_FlatBasic::RenderPortalViewToBackBuffer( CViewRender *pViewRender, const CViewSetup &cameraView )
{
	VPROF( "CPortalRenderable_FlatBasic::RenderPortalViewToBackBuffer" );

	if( m_fStaticAmount == 1.0f )
		return; //not going to see anything anyways

	if( m_pLinkedPortal == NULL ) //not linked to any portal, so we'll be all static anyways
		return;

	Frustum FrustumBackup;
	memcpy( FrustumBackup, pViewRender->GetFrustum(), sizeof( Frustum ) );

	Frustum seeThroughFrustum;
	bool bUseSeeThroughFrustum;

	bUseSeeThroughFrustum = CalcFrustumThroughPortal( cameraView.origin, seeThroughFrustum );

	Vector vCameraForward;
	AngleVectors( cameraView.angles, &vCameraForward, NULL, NULL );

	// Setup fog state for the camera.
	Vector ptPOVOrigin = m_matrixThisToLinked * cameraView.origin;	
	Vector vPOVForward = m_matrixThisToLinked.ApplyRotation( vCameraForward );

	Vector ptRemotePortalPosition = m_pLinkedPortal->m_ptOrigin;
	Vector vRemotePortalForward = m_pLinkedPortal->m_vForward;

	CViewSetup portalView = cameraView;

	if( portalView.zNear < 1.0f )
		portalView.zNear = 1.0f;

	QAngle qPOVAngles = TransformAnglesToWorldSpace( cameraView.angles, m_matrixThisToLinked.As3x4() );	

	portalView.width = cameraView.width;
	portalView.height = cameraView.height;
	portalView.x = cameraView.x;
	portalView.y = cameraView.y;
	portalView.origin = ptPOVOrigin;
	portalView.angles = qPOVAngles;
	portalView.fov = cameraView.fov;
	portalView.m_bOrtho = false;
	portalView.m_flAspectRatio = cameraView.m_flAspectRatio;

	CopyToCurrentView( pViewRender, portalView );

	CMatRenderContextPtr pRenderContext( materials );

	{
		float fCustomClipPlane[4];
		fCustomClipPlane[0] = vRemotePortalForward.x;
		fCustomClipPlane[1] = vRemotePortalForward.y;
		fCustomClipPlane[2] = vRemotePortalForward.z;
		fCustomClipPlane[3] = vRemotePortalForward.Dot( ptRemotePortalPosition - (vRemotePortalForward * 0.5f) ); //moving it back a smidge to eliminate visual artifacts for half-in objects

		pRenderContext->PushCustomClipPlane( fCustomClipPlane ); //this is technically the same plane within recursive views, but pushing it anyway in-case something else has been added to the stack
	}


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

			if( m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration && (g_pPortalRender->GetRemainingPortalViewDepth() == 1) )
			{
				//save the view matrix for usage with the depth doubler. 
				//It's important that we do this AFTER using the depth doubler this frame to compensate for the fact that the front buffer is 1 frame behind the current view matrix
				//otherwise we get a lag effect when the player changes their viewing angles
				pRenderContext->GetMatrix( MATERIAL_VIEW, &m_InternallyMaintainedData.m_DepthDoublerTextureView );
			}

			SetViewRecursionLevel( g_pPortalRender->GetViewRecursionLevel() - 1 );
		}
		render->PopView( pViewRender->GetFrustum() );

		//restore old frustum
		memcpy( pViewRender->GetFrustum(), FrustumBackup, sizeof( Frustum ) );
		render->OverrideViewFrustum( FrustumBackup );
	}

	pRenderContext->PopCustomClipPlane();

	//restore old vis data
	CopyToCurrentView( pViewRender, cameraView );
}


void CPortalRenderable_FlatBasic::RenderPortalViewToTexture( CViewRender *pViewRender, const CViewSetup &cameraView )
{
	if( m_fStaticAmount == 1.0f )
		return; //not going to see anything anyways

	if( m_pLinkedPortal == NULL ) //not linked to any portal, so we'll be all static anyways
		return;

	float fPixelVisibilty = g_pPortalRender->GetPixelVisilityForPortalSurface( this );
	if( (fPixelVisibilty >= 0.0f) && (fPixelVisibilty <= PORTALRENDERABLE_FLATBASIC_MINPIXELVIS) )
		return;

	ITexture *pRenderTarget;
	if( m_bIsPortal2 )
		pRenderTarget = portalrendertargets->GetPortal2Texture();
	else
		pRenderTarget = portalrendertargets->GetPortal1Texture();

	// Require that we have render textures for drawing
	AssertMsg( pRenderTarget, "Portal render targets not initialized properly" );

	// We're about to dereference this, so just bail if we can't
	if ( !pRenderTarget )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	Vector vCameraForward;
	AngleVectors( cameraView.angles, &vCameraForward, NULL, NULL );

	Frustum seeThroughFrustum;
	bool bUseSeeThroughFrustum = CalcFrustumThroughPortal( cameraView.origin, seeThroughFrustum );

	// Setup fog state for the camera.
	Vector ptPOVOrigin = m_matrixThisToLinked * cameraView.origin;	
	Vector vPOVForward = m_matrixThisToLinked.ApplyRotation( vCameraForward );

	Vector vCameraToPortal = m_ptOrigin - cameraView.origin;

	CViewSetup portalView = cameraView;
	Frustum frustumBackup;
	memcpy( frustumBackup, pViewRender->GetFrustum(), sizeof( Frustum ) );

	QAngle qPOVAngles = TransformAnglesToWorldSpace( cameraView.angles, m_matrixThisToLinked.As3x4() );	

	portalView.width = pRenderTarget->GetActualWidth();
	portalView.height = pRenderTarget->GetActualHeight();
	portalView.x = 0;
	portalView.y = 0;
	portalView.origin = ptPOVOrigin;
	portalView.angles = qPOVAngles;
	portalView.fov = cameraView.fov;
	portalView.m_bOrtho = false;
	portalView.m_flAspectRatio = (float)cameraView.width / (float)cameraView.height; //use the screen aspect ratio, 0.0f doesn't work as advertised

	//pRenderContext->Flush( false );

	float fCustomClipPlane[4];
	fCustomClipPlane[0] = m_pLinkedPortal->m_vForward.x;
	fCustomClipPlane[1] = m_pLinkedPortal->m_vForward.y;
	fCustomClipPlane[2] = m_pLinkedPortal->m_vForward.z;
	fCustomClipPlane[3] = m_pLinkedPortal->m_vForward.Dot( m_pLinkedPortal->m_ptOrigin - (m_pLinkedPortal->m_vForward * 0.5f) ); //moving it back a smidge to eliminate visual artifacts for half-in objects

	pRenderContext->PushCustomClipPlane( fCustomClipPlane );

	{
		render->Push3DView( portalView, VIEW_CLEAR_DEPTH, pRenderTarget, pViewRender->GetFrustum() );

		{
			ViewCustomVisibility_t customVisibility;
			m_pLinkedPortal->AddToVisAsExitPortal( &customVisibility );

			SetRemainingViewDepth( 0 );
			SetViewRecursionLevel( 1 );

			CPortalRenderable *pRenderingViewForPortalBackup = g_pPortalRender->GetCurrentViewEntryPortal();
			CPortalRenderable *pRenderingViewExitPortalBackup = g_pPortalRender->GetCurrentViewExitPortal();
			SetViewEntranceAndExitPortals( this, m_pLinkedPortal );

			bool bDrew3dSkybox = false;
			SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;

			// if the 3d skybox world is drawn, then don't draw the normal skybox
			int nClearFlags = 0;
			Draw3dSkyboxworld_Portal( pViewRender, portalView, nClearFlags, bDrew3dSkybox, nSkyboxVisible, pRenderTarget );

			if( bUseSeeThroughFrustum )
			{
				memcpy( pViewRender->GetFrustum(), seeThroughFrustum, sizeof( Frustum ) );
			}

			render->OverrideViewFrustum( pViewRender->GetFrustum() );

			pRenderContext->EnableUserClipTransformOverride( false );

			ViewDrawScene( pViewRender, bDrew3dSkybox, nSkyboxVisible, portalView, nClearFlags, (view_id_t)g_pPortalRender->GetCurrentViewId(), false, 0, &customVisibility );

			SetViewEntranceAndExitPortals( pRenderingViewForPortalBackup, pRenderingViewExitPortalBackup );

			SetRemainingViewDepth( 1 );
			SetViewRecursionLevel( 0 );

			memcpy( pViewRender->GetFrustum(), frustumBackup, sizeof( Frustum ) );
			render->OverrideViewFrustum( pViewRender->GetFrustum() );
		}

		render->PopView( pViewRender->GetFrustum() );
	}

	pRenderContext->PopCustomClipPlane();

	//pRenderContext->Flush( false );

	CopyToCurrentView( pViewRender, cameraView );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AddToVisAsExitPortal
// input -  pViewRender: pointer to the CViewRender class used to render this scene.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPortalRenderable_FlatBasic::AddToVisAsExitPortal( ViewCustomVisibility_t *pCustomVisibility )
{
	if ( !pCustomVisibility )
		return;

	// Add four corners of the portal to the renderer as visibility origins
	for ( int i = 0; i < 4; ++i )
	{
		if( enginetrace->GetLeafContainingPoint( m_InternallyMaintainedData.m_ptCorners[i] ) != -1 )
			pCustomVisibility->AddVisOrigin( m_InternallyMaintainedData.m_ptCorners[i] );
	}

	// Specify which leaf to use for area portal culling
	pCustomVisibility->ForceVisOverride( m_InternallyMaintainedData.m_VisData );
	pCustomVisibility->ForceViewLeaf( m_InternallyMaintainedData.m_iViewLeaf );
}

void CPortalRenderable_FlatBasic::DrawPreStencilMask( void )
{
	if ( ( m_fOpenAmount > 0.0f ) && ( m_fOpenAmount < 1.0f ) )
	{
		DrawSimplePortalMesh( m_Materials.m_Portal_Refract[ ( ( m_bIsPortal2 ) ? ( 1 ) : ( 0 ) ) ] );
	}
}

void CPortalRenderable_FlatBasic::DrawStencilMask( void )
{
	DrawSimplePortalMesh( m_Materials.m_Portal_Stencil_Hole );
	DrawRenderFixMesh( g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
}

void CPortalRenderable_FlatBasic::DrawPostStencilFixes( void )
{
	CMatRenderContextPtr pRenderContext( materials );

	//fast clipping may have hosed depth, reset it
	pRenderContext->ClearBuffersObeyStencil( false, true );

	//replace the fog we overwrote
	RenderFogQuad();

	//replace depth
	DrawSimplePortalMesh( g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model, 0.0f );
	DrawRenderFixMesh( g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model, 0.0f );
}

bool CPortalRenderable_FlatBasic::ShouldUpdateDepthDoublerTexture( const CViewSetup &viewSetup )
{
	return	( (m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration) && 
		(m_pLinkedPortal != NULL) &&
		(m_fStaticAmount < 1.0f) );
}

void CPortalRenderable_FlatBasic::HandlePortalPlaybackMessage( KeyValues *pKeyValues )
{
	int nLinkedPortalId = pKeyValues->GetInt( "linkedPortalId" );
	m_fOpenAmount = pKeyValues->GetFloat( "openAmount" );
	m_fStaticAmount = pKeyValues->GetFloat( "staticAmount" );
	m_fSecondaryStaticAmount = pKeyValues->GetFloat( "secondaryStaticAmount" );
	m_bIsPortal2 = pKeyValues->GetInt( "isPortal2" ) != 0;
	m_pLinkedPortal = nLinkedPortalId >= 0 ? (CPortalRenderable_FlatBasic *)FindRecordedPortal( nLinkedPortalId ) : NULL;
	matrix3x4_t *pMat = (matrix3x4_t*)pKeyValues->GetPtr( "portalToWorld" );

	MatrixGetColumn( *pMat, 3, m_ptOrigin );
	MatrixGetColumn( *pMat, 0, m_vForward );
	MatrixGetColumn( *pMat, 1, m_vRight );
	MatrixGetColumn( *pMat, 2, m_vUp );
	m_vRight *= -1.0f;

	PortalMoved();
}

extern ConVar mat_wireframe;
static void DrawComplexPortalMesh_SubQuad( Vector &ptBottomLeft, Vector &vUp, Vector &vRight, float *fSubQuadRect, void *pBindEnt, const IMaterial *pMaterial, const VMatrix *pReplacementViewMatrixForTexCoords = NULL )
{
	PortalMeshPoint_t Vertices[4];

	Vertices[0].vWorldSpacePosition = ptBottomLeft + (vRight * fSubQuadRect[2]) + (vUp * fSubQuadRect[3]);
	Vertices[0].texCoord.x = fSubQuadRect[2];
	Vertices[0].texCoord.y = fSubQuadRect[3];

	Vertices[1].vWorldSpacePosition = ptBottomLeft + (vRight * fSubQuadRect[2]) + (vUp * fSubQuadRect[1]);
	Vertices[1].texCoord.x = fSubQuadRect[2];
	Vertices[1].texCoord.y = fSubQuadRect[1];

	Vertices[2].vWorldSpacePosition = ptBottomLeft + (vRight * fSubQuadRect[0]) + (vUp * fSubQuadRect[1]);
	Vertices[2].texCoord.x = fSubQuadRect[0];
	Vertices[2].texCoord.y = fSubQuadRect[1];

	Vertices[3].vWorldSpacePosition = ptBottomLeft + (vRight * fSubQuadRect[0]) + (vUp * fSubQuadRect[3]);
	Vertices[3].texCoord.x = fSubQuadRect[0];
	Vertices[3].texCoord.y = fSubQuadRect[3];	

	Clip_And_Render_Convex_Polygon( Vertices, 4, pMaterial, pBindEnt );
}

#define PORTAL_PROJECTION_MESH_SUBDIVIDE_HEIGHTCHUNKS 8
#define PORTAL_PROJECTION_MESH_SUBDIVIDE_WIDTHCHUNKS 6

void CPortalRenderable_FlatBasic::DrawComplexPortalMesh( const IMaterial *pMaterialOverride, float fForwardOffsetModifier ) //generates and draws the portal mesh (Needed for compatibility with fixed function rendering)
{
	PortalMeshPoint_t BaseVertices[4];

	Vector ptBottomLeft = m_ptOrigin + (m_vForward * (fForwardOffsetModifier)) - (m_vRight * PORTAL_HALF_WIDTH) - (m_vUp * PORTAL_HALF_HEIGHT);
	Vector vScaledUp = m_vUp * (2.0f * PORTAL_HALF_HEIGHT);
	Vector vScaledRight = m_vRight * (2.0f * PORTAL_HALF_WIDTH);

	CMatRenderContextPtr pRenderContext( materials );
	VMatrix matView;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &matView );

	const IMaterial *pMaterial;
	if( pMaterialOverride )
	{
		pMaterial = pMaterialOverride;	
	}
	else
	{
		pMaterial = m_Materials.m_PortalMaterials[(m_bIsPortal2)?1:0];
	}


	float fSubQuadRect[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

	float fHeightBegin = 0.0f;
	for( int i = 0; i != PORTAL_PROJECTION_MESH_SUBDIVIDE_HEIGHTCHUNKS; ++i )
	{
		float fHeightEnd = fHeightBegin + (1.0f / ((float)PORTAL_PROJECTION_MESH_SUBDIVIDE_HEIGHTCHUNKS));

		fSubQuadRect[1] = fHeightBegin;
		fSubQuadRect[3] = fHeightEnd;

		float fWidthBegin = 0.0f;
		for( int j = 0; j != PORTAL_PROJECTION_MESH_SUBDIVIDE_WIDTHCHUNKS; ++j )
		{
			float fWidthEnd = fWidthBegin + (1.0f / ((float)PORTAL_PROJECTION_MESH_SUBDIVIDE_WIDTHCHUNKS));

			fSubQuadRect[0] = fWidthBegin;
			fSubQuadRect[2] = fWidthEnd;

			DrawComplexPortalMesh_SubQuad( ptBottomLeft, vScaledUp, vScaledRight, fSubQuadRect, GetClientRenderable(), pMaterial );	

			fWidthBegin = fWidthEnd;
		}
		fHeightBegin = fHeightEnd; 
	}

	//pRenderContext->Flush( false );	
}

void CPortalRenderable_FlatBasic::DrawDepthDoublerMesh( float fForwardOffsetModifier )
{
	IMaterialVar *pDepthDoubleViewMatrixVar = s_FlatBasicPortalDrawingMaterials.m_Materials.m_PortalDepthDoubler->FindVar( "$alternateviewmatrix", NULL, false );
	if ( pDepthDoubleViewMatrixVar )
		pDepthDoubleViewMatrixVar->SetMatrixValue( m_InternallyMaintainedData.m_DepthDoublerTextureView );
	DrawSimplePortalMesh( m_Materials.m_PortalDepthDoubler, fForwardOffsetModifier );
}



void CPortalRenderable_FlatBasic::DrawSimplePortalMesh( const IMaterial *pMaterialOverride, float fForwardOffsetModifier )
{
	const IMaterial *pMaterial;
	if( pMaterialOverride )
		pMaterial = pMaterialOverride;
	else
		pMaterial = m_Materials.m_PortalMaterials[m_bIsPortal2?1:0];

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( (IMaterial *)pMaterial, GetClientRenderable() );
	
	// This can depend on the Bind command above, so keep this after!
	UpdateFrontBufferTexturesForMaterial( (IMaterial *)pMaterial );

	pRenderContext->MatrixMode( MATERIAL_MODEL ); //just in case
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	Vector ptCenter = m_ptOrigin + (m_vForward * fForwardOffsetModifier);

	Vector verts[4];
	verts[0] = ptCenter + (m_vRight * PORTAL_HALF_WIDTH) - (m_vUp * PORTAL_HALF_HEIGHT);
	verts[1] = ptCenter + (m_vRight * PORTAL_HALF_WIDTH) + (m_vUp * PORTAL_HALF_HEIGHT);	
	verts[2] = ptCenter - (m_vRight * PORTAL_HALF_WIDTH) - (m_vUp * PORTAL_HALF_HEIGHT);
	verts[3] = ptCenter - (m_vRight * PORTAL_HALF_WIDTH) + (m_vUp * PORTAL_HALF_HEIGHT);

	float vTangent[4] = { -m_vRight.x, -m_vRight.y, -m_vRight.z, 1.0f };

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( false );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 2 );

	meshBuilder.Position3fv( &verts[0].x );
	meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 1, 0.0f, 1.0f );
	meshBuilder.Normal3f( m_vForward.x, m_vForward.y, m_vForward.z );
	meshBuilder.UserData( vTangent );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( &verts[1].x );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.TexCoord2f( 1, 0.0f, 0.0f );
	meshBuilder.Normal3f( m_vForward.x, m_vForward.y, m_vForward.z );
	meshBuilder.UserData( vTangent );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( &verts[2].x );
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.TexCoord2f( 1, 1.0f, 1.0f );
	meshBuilder.Normal3f( m_vForward.x, m_vForward.y, m_vForward.z );
	meshBuilder.UserData( vTangent );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( &verts[3].x );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.TexCoord2f( 1, 1.0f, 0.0f );
	meshBuilder.Normal3f( m_vForward.x, m_vForward.y, m_vForward.z );
	meshBuilder.UserData( vTangent );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
	
	if( mat_wireframe.GetBool() )
	{
		pRenderContext->Bind( (IMaterial *)(const IMaterial *)g_pPortalRender->m_MaterialsAccess.m_Wireframe, (CPortalRenderable*)this );

		IMesh* pMesh = pRenderContext->GetDynamicMesh( false );
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 2 );

		meshBuilder.Position3fv( &verts[0].x );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.TexCoord2f( 1, 0.0f, 1.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &verts[1].x );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.TexCoord2f( 1, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &verts[2].x );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.TexCoord2f( 1, 1.0f, 1.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( &verts[3].x );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.TexCoord2f( 1, 1.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}



void CPortalRenderable_FlatBasic::DrawRenderFixMesh( const IMaterial *pMaterialOverride, float fFrontClipDistance )
{
	const IMaterial *pMaterial;
	if( pMaterialOverride )
		pMaterial = pMaterialOverride;
	else
		pMaterial = m_Materials.m_PortalRenderFixMaterials[(m_bIsPortal2)?1:0];

	if( g_pPortalRender->GetViewRecursionLevel() != 0 )
		return; //a render fix should only ever be necessary in the primary view

	Vector ptCameraOrigin = CurrentViewOrigin();

	Vector vPortalCenterToCamera = ptCameraOrigin - m_ptOrigin;
	if( (vPortalCenterToCamera.Dot( m_vForward ) < -1.0f) ) //camera coplanar (to 1.0 units) or in front of portal plane
		return;

	if( vPortalCenterToCamera.LengthSqr() < (PORTAL_HALF_HEIGHT * PORTAL_HALF_HEIGHT) ) //FIXME: Player closeness check might need reimplementation
	{
		//if the player is this close to the portal, immediately get rid of any static it has as well as draw the fix
		m_fStaticAmount = 0.0f;
		//m_fSecondaryStaticAmount = 0.0f;

		float fOldDist = m_InternallyMaintainedData.m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES].m_Dist;


		float fCameraDist = m_vForward.Dot(ptCameraOrigin - m_ptOrigin);

		if( fFrontClipDistance > fCameraDist ) //never clip further out than the camera, we can see into the garbage space of the portal view's texture
			fFrontClipDistance = fCameraDist;

		m_InternallyMaintainedData.m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES].m_Dist -= fFrontClipDistance;
		Internal_DrawRenderFixMesh( pMaterial );
		m_InternallyMaintainedData.m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES].m_Dist = fOldDist;
	}


}

void CPortalRenderable_FlatBasic::Internal_DrawRenderFixMesh( const IMaterial *pMaterial )
{
	PortalMeshPoint_t WorkVertices[4];

	CMatRenderContextPtr pRenderContext( materials );
	
	pRenderContext->MatrixMode( MATERIAL_MODEL ); //just in case
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();


	//view->GetViewSetup()->zNear;
	Vector vForward, vUp, vRight, vOrigin;
	vForward = CurrentViewForward();
	vUp = CurrentViewUp();
	vRight = CurrentViewRight();

	vOrigin = CurrentViewOrigin() + vForward * (view->GetViewSetup()->zNear + 0.05f);

	for( int i = 0; i != 4; ++i )
	{
		WorkVertices[i].texCoord.x = WorkVertices[i].texCoord.y = 0.0f;
	}

	WorkVertices[0].vWorldSpacePosition = vOrigin + (vRight * 40.0f) + (vUp * -40.0f);
	WorkVertices[1].vWorldSpacePosition = vOrigin + (vRight * 40.0f) + (vUp * 40.0f);
	WorkVertices[2].vWorldSpacePosition = vOrigin + (vRight * -40.0f) + (vUp * 40.0f);
	WorkVertices[3].vWorldSpacePosition = vOrigin + (vRight * -40.0f) + (vUp * -40.0f);

	ClipFixToBoundingAreaAndDraw( WorkVertices, pMaterial);

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}

void CPortalRenderable_FlatBasic::ClipFixToBoundingAreaAndDraw( PortalMeshPoint_t *pVerts, const IMaterial *pMaterial )
{
	PortalMeshPoint_t *pInVerts = (PortalMeshPoint_t *)stackalloc( 4 * (PORTALRENDERFIXMESH_OUTERBOUNDPLANES + 2) * 16 * sizeof( PortalMeshPoint_t ) ); //really only should need 4x points, but I'm paranoid
	PortalMeshPoint_t *pOutVerts = (PortalMeshPoint_t *)stackalloc( 4 * (PORTALRENDERFIXMESH_OUTERBOUNDPLANES + 2) * 16 * sizeof( PortalMeshPoint_t ) );
	PortalMeshPoint_t *pTempVerts;

	memcpy( pInVerts, pVerts, sizeof( PortalMeshPoint_t ) * 4 );
	int iVertCount = 4;
	
	//clip by bounding area planes
	{
		for( int i = 0; i != (PORTALRENDERFIXMESH_OUTERBOUNDPLANES + 1); ++i )
		{
			if( iVertCount < 3 )
				return; //nothing to draw

			iVertCount = ClipPolyToPlane_LerpTexCoords( pInVerts, iVertCount, pOutVerts, m_InternallyMaintainedData.m_BoundingPlanes[i].m_Normal, m_InternallyMaintainedData.m_BoundingPlanes[i].m_Dist, 0.01f );
			pTempVerts = pInVerts; pInVerts = pOutVerts; pOutVerts = pTempVerts; //swap vertex pointers
		}

		if( iVertCount < 3 )
			return; //nothing to draw
	}


	//clip by the viewing frustum
	{
		VPlane *pFrustum = view->GetFrustum();

		for( int i = 0; i != FRUSTUM_NUMPLANES; ++i )
		{
			if( iVertCount < 3 )
				return; //nothing to draw

			iVertCount = ClipPolyToPlane_LerpTexCoords( pInVerts, iVertCount, pOutVerts, pFrustum[i].m_Normal, pFrustum[i].m_Dist, 0.01f );
			pTempVerts = pInVerts; pInVerts = pOutVerts; pOutVerts = pTempVerts; //swap vertex pointers
		}

		if( iVertCount < 3 )
			return; //nothing to draw
	}

	CMatRenderContextPtr pRenderContext( materials );

	//project the points so we can fudge the numbers a bit and move them to exactly 0.0f depth
	{
		VMatrix matProj, matView, matViewProj;
		pRenderContext->GetMatrix( MATERIAL_PROJECTION, &matProj );
		pRenderContext->GetMatrix( MATERIAL_VIEW, &matView );
		MatrixMultiply( matProj, matView, matViewProj );

		for( int i = 0; i != iVertCount; ++i )
		{
			float W, inverseW;

			W = matViewProj.m[3][0] * pInVerts[i].vWorldSpacePosition.x;
			W += matViewProj.m[3][1] * pInVerts[i].vWorldSpacePosition.y;
			W += matViewProj.m[3][2] * pInVerts[i].vWorldSpacePosition.z;
			W += matViewProj.m[3][3];
			
			inverseW = 1.0f / W;


			pInVerts[i].vWorldSpacePosition = matViewProj * pInVerts[i].vWorldSpacePosition;
			pInVerts[i].vWorldSpacePosition *= inverseW;
			pInVerts[i].vWorldSpacePosition.z = 0.00001f; //the primary reason we're projecting on the CPU to begin with
		}
	}

	//render with identity transforms and clipping disabled
	{
		bool bClippingEnabled = pRenderContext->EnableClipping( false );

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();

		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();

		RenderPortalMeshConvexPolygon( pInVerts, iVertCount, pMaterial, this );
		if( mat_wireframe.GetBool() )
			RenderPortalMeshConvexPolygon( pInVerts, iVertCount, materials->FindMaterial( "shadertest/wireframe", TEXTURE_GROUP_CLIENT_EFFECTS, false ), this );

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PopMatrix();

		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PopMatrix();

		pRenderContext->EnableClipping( bClippingEnabled );
	}
}


//-----------------------------------------------------------------------------
// renders a quad that simulates fog as an overlay for something else (most notably the hole we create for stencil mode portals)
//-----------------------------------------------------------------------------
void CPortalRenderable_FlatBasic::RenderFogQuad( void )
{
	CMatRenderContextPtr pRenderContext( materials );
	if( pRenderContext->GetFogMode() == MATERIAL_FOG_NONE )
		return;

	float fFogStart, fFogEnd;
	pRenderContext->GetFogDistances( &fFogStart, &fFogEnd, NULL );
	float vertexColor[4];
	unsigned char fogColor[3];
	pRenderContext->GetFogColor( fogColor );

	/*float fColorScale = LinearToGammaFullRange( pRenderContext->GetToneMappingScaleLinear().x );

	fogColor[0] *= fColorScale;
	fogColor[1] *= fColorScale;
	fogColor[2] *= fColorScale;*/

	vertexColor[0] = fogColor[0] * (1.0f / 255.0f);
	vertexColor[1] = fogColor[1] * (1.0f / 255.0f);
	vertexColor[2] = fogColor[2] * (1.0f / 255.0f);

	float ooFogRange = 1.0f;
	if ( fFogEnd != fFogStart )
	{
		ooFogRange = 1.0f / (fFogEnd - fFogStart);
	}

	float FogEndOverFogRange = ooFogRange * fFogEnd;

	VMatrix matView, matViewProj, matProj;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &matView );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &matProj );
	MatrixMultiply( matProj, matView, matViewProj );

	Vector vUp = m_vUp * (PORTAL_HALF_HEIGHT * 2.0f);
	Vector vRight = m_vRight * (PORTAL_HALF_WIDTH * 2.0f);

	Vector ptCorners[4];
	ptCorners[0] = (m_ptOrigin + vUp) + vRight;
	ptCorners[1] = (m_ptOrigin + vUp) - vRight;
	ptCorners[2] = (m_ptOrigin - vUp) + vRight;
	ptCorners[3] = (m_ptOrigin - vUp) - vRight;


	pRenderContext->Bind( (IMaterial *)(const IMaterial *)g_pPortalRender->m_MaterialsAccess.m_TranslucentVertexColor );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 2 );

	for( int i = 0; i != 4; ++i )
	{
		float projZ;
		projZ = matViewProj.m[2][0] * ptCorners[i].x;
		projZ += matViewProj.m[2][1] * ptCorners[i].y;
		projZ += matViewProj.m[2][2] * ptCorners[i].z;
		projZ += matViewProj.m[2][3];

		float fFogAmount = ((-projZ * ooFogRange) + FogEndOverFogRange); //projZ should be negative

		//range fix
		if( fFogAmount >= 1.0f )
			fFogAmount = 1.0f;

		if( fFogAmount <= 0.0f )
			fFogAmount = 0.0f;

		vertexColor[3] = 1.0f - fFogAmount; //alpha is inverse fog

		meshBuilder.Position3fv( &ptCorners[i].x );
		meshBuilder.Color4fv( vertexColor );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();
}

void CPortalRenderable_FlatBasic::DrawPortal( void )
{
	if( (view->GetDrawFlags() & DF_RENDER_REFLECTION) != 0 )
		return;

	if ( g_pPortalRender->ShouldUseStencilsToRenderPortals() )
	{
		//stencil-based rendering
		if( g_pPortalRender->IsRenderingPortal() == false ) //main view
		{
			if( m_pLinkedPortal == NULL ) //didn't pass through pre-stencil mask
			{
				if ( ( m_fOpenAmount > 0.0f ) && ( m_fOpenAmount < 1.0f ) )
				{
					DrawSimplePortalMesh( m_Materials.m_Portal_Refract[ ( ( m_bIsPortal2 ) ? ( 1 ) : ( 0 ) ) ] );
				}
			}

			DrawSimplePortalMesh( m_Materials.m_PortalStaticOverlay[((m_bIsPortal2)?(1):(0))] );
			DrawRenderFixMesh( g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
		}
		else if( g_pPortalRender->GetCurrentViewExitPortal() != this )
		{
			if( m_pLinkedPortal == NULL ) //didn't pass through pre-stencil mask
			{
				if ( ( m_fOpenAmount > 0.0f ) && ( m_fOpenAmount < 1.0f ) )
				{
					DrawSimplePortalMesh( m_Materials.m_Portal_Refract[ ( ( m_bIsPortal2 ) ? ( 1 ) : ( 0 ) ) ] );
				}
			}

			if( (m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration) 
				&& (g_pPortalRender->GetRemainingPortalViewDepth() == 0) 
				&& (g_pPortalRender->GetViewRecursionLevel() > 1) )
			{
				DrawDepthDoublerMesh();
			}
			else
			{
				DrawSimplePortalMesh( m_Materials.m_PortalStaticOverlay[((m_bIsPortal2)?(1):(0))] );
			}
		}
	}
	else
	{
		BeginPortalPixelVisibilityQuery();

		//texture-based rendering
		if( g_pPortalRender->IsRenderingPortal() == false )
		{			
			if ( ( m_fOpenAmount > 0.0f ) && ( m_fOpenAmount < 1.0f ) )
			{
				DrawSimplePortalMesh( m_Materials.m_Portal_Refract[ ( ( m_bIsPortal2 ) ? ( 1 ) : ( 0 ) ) ] );
			}

			DrawComplexPortalMesh();
			DrawRenderFixMesh();
		}
		else if( g_pPortalRender->GetCurrentViewExitPortal() != this ) //don't render portals that our view is exiting from
		{
			if ( ( m_fOpenAmount > 0.0f ) && ( m_fOpenAmount < 1.0f ) )
			{
				DrawSimplePortalMesh( m_Materials.m_Portal_Refract[ ( ( m_bIsPortal2 ) ? ( 1 ) : ( 0 ) ) ] );
			}
			DrawSimplePortalMesh( m_Materials.m_PortalStaticOverlay[((m_bIsPortal2)?(1):(0))] ); //FIXME: find out why the projection mesh screws up at the second level of rendering in -nouserclip situations
		}

		EndPortalPixelVisibilityQuery();
	}
}

bool CPortalRenderable_FlatBasic::DoesExitViewIntersectWaterPlane( float waterZ, int leafWaterDataID ) const
{
	bool bAboveWater = false, bBelowWater = false;
	for( int i = 0; i != 4; ++i )
	{
		bAboveWater |= (m_InternallyMaintainedData.m_ptCorners[i].z >= waterZ);
		bBelowWater |= (m_InternallyMaintainedData.m_ptCorners[i].z <= waterZ);
	}

	return (bAboveWater && bBelowWater);
}

bool CPortalRenderable_FlatBasic::ShouldUpdatePortalView_BasedOnView( const CViewSetup &currentView, CUtlVector<VPlane> &currentComplexFrustum  )
{
	if( m_pLinkedPortal == NULL )
		return false;

	if( m_fStaticAmount == 1.0f )
		return false;

	Vector vCameraPos = currentView.origin;

	if( (g_pPortalRender->GetViewRecursionLevel() == 0) &&
		((m_ptOrigin - vCameraPos).LengthSqr() < (PORTAL_HALF_HEIGHT * PORTAL_HALF_HEIGHT)) ) //FIXME: Player closeness check might need reimplementation
	{
		return true; //fudgery time. The player might not be able to see the surface, but they can probably see the render fix
	}

	if( m_vForward.Dot( vCameraPos ) <= m_InternallyMaintainedData.m_fPlaneDist )
		return false; //looking at portal backface

	VPlane *currentFrustum = currentComplexFrustum.Base();
	int iCurrentFrustmPlanes = currentComplexFrustum.Count();

	//now slice up the portal quad and see if any is visible within the frustum
	int allocSize = (6 + currentComplexFrustum.Count()); //possible to add 1 point per cut, 4 starting points, N plane cuts, 2 extra because I'm paranoid
	Vector *pInVerts = (Vector *)stackalloc( sizeof( Vector ) * allocSize * 2 );
	Vector *pOutVerts = pInVerts + allocSize;
	Vector *pTempVerts;

	//clip by first plane and put output into pInVerts
	int iVertCount = ClipPolyToPlane( m_InternallyMaintainedData.m_ptCorners, 4, pInVerts, currentFrustum[0].m_Normal, currentFrustum[0].m_Dist, 0.01f );

	//clip by other planes and flipflop in and out pointers
	for( int i = 1; i != iCurrentFrustmPlanes; ++i )
	{
		if( iVertCount < 3 )
			return false; //nothing left in the frustum

		iVertCount = ClipPolyToPlane( pInVerts, iVertCount, pOutVerts, currentFrustum[i].m_Normal, currentFrustum[i].m_Dist, 0.01f );
		pTempVerts = pInVerts; pInVerts = pOutVerts; pOutVerts = pTempVerts; //swap vertex pointers
	}

	if( iVertCount < 3 )
		return false; //nothing left in the frustum

	return true;
}

bool CPortalRenderable_FlatBasic::ShouldUpdatePortalView_BasedOnPixelVisibility( float fScreenFilledByStencilMaskLastFrame_Normalized )
{
	return (fScreenFilledByStencilMaskLastFrame_Normalized < 0.0f) || // < 0 is an error value
			(fScreenFilledByStencilMaskLastFrame_Normalized > PORTALRENDERABLE_FLATBASIC_MINPIXELVIS );
}

CPortalRenderable *CreatePortal_FlatBasic_Fn( void )
{
	return new CPortalRenderable_FlatBasic;
}

static CPortalRenderableCreator CreatePortal_FlatBasic( "flatBasic", CreatePortal_FlatBasic_Fn );
