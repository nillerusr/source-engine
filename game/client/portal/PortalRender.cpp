//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//


#include "cbase.h"
#include "PortalRender.h"
#include "clienteffectprecachesystem.h"
#include "view.h"
#include "c_pixel_visibility.h"
#include "glow_overlay.h"
#include "portal_render_targets.h" //depth doubler
#include "materialsystem/itexture.h"
#include "toolframework/itoolframework.h"
#include "tier1/KeyValues.h"
#include "view_scene.h"
#include "viewrender.h"
#include "vprof.h"

CLIENTEFFECT_REGISTER_BEGIN( PrecachePortalDrawingMaterials )
CLIENTEFFECT_MATERIAL( "shadertest/wireframe" )
CLIENTEFFECT_MATERIAL( "engine/writez_model" )
CLIENTEFFECT_MATERIAL( "engine/TranslucentVertexColor" )
CLIENTEFFECT_REGISTER_END()

#define TEMP_DISABLE_PORTAL_VIS_QUERY

static ConVar r_forcecheapwater( "r_forcecheapwater", "0", FCVAR_CLIENTDLL | FCVAR_CHEAT, "Force all water to be cheap water, will show old renders if enabled after water has been seen" );


ConVar r_portal_use_stencils( "r_portal_use_stencils", "1", FCVAR_CLIENTDLL, "Render portal views using stencils (if available)" ); //draw portal views using stencil rendering
ConVar r_portal_stencil_depth( "r_portal_stencil_depth", "2", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "When using stencil views, this changes how many views within views we see" );

//-----------------------------------------------------------------------------
//
// Portal rendering management class
//
//-----------------------------------------------------------------------------
static CPortalRender s_PortalRender;
CPortalRender* g_pPortalRender = &s_PortalRender;


//-------------------------------------------
//Portal View ID Node helpers
//-------------------------------------------
CUtlVector<int> s_iFreedViewIDs; //when a view id node gets freed, it's primary view id gets added here

PortalViewIDNode_t *AllocPortalViewIDNode( int iChildLinkCount )
{
	PortalViewIDNode_t *pNode = new PortalViewIDNode_t; //for now we just new/delete

	int iFreedIDsCount = s_iFreedViewIDs.Count();
	if( iFreedIDsCount != 0 )
	{
		pNode->iPrimaryViewID = s_iFreedViewIDs.Tail();
		s_iFreedViewIDs.FastRemove( iFreedIDsCount - 1 );
	}
	else
	{
		static int iNewAllocationCounter = VIEW_ID_COUNT;
		pNode->iPrimaryViewID = iNewAllocationCounter; 
		iNewAllocationCounter += 2; //2 to make room for skybox view ids
	}

	CMatRenderContextPtr pRenderContext( materials );	
#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
	pNode->occlusionQueryHandle = pRenderContext->CreateOcclusionQueryObject();
#endif
	pNode->iOcclusionQueryPixelsRendered = -5;
	pNode->iWindowPixelsAtQueryTime = 0;
	pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized = -1.0f;

	if( iChildLinkCount != 0 )
	{
		pNode->ChildNodes.SetCount( iChildLinkCount );
		memset( pNode->ChildNodes.Base(), NULL, sizeof( PortalViewIDNode_t * ) * iChildLinkCount );
	}

	return pNode;
}

void FreePortalViewIDNode( PortalViewIDNode_t *pNode )
{
	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		if( pNode->ChildNodes[i] != NULL )
			FreePortalViewIDNode( pNode->ChildNodes[i] );
	}

	s_iFreedViewIDs.AddToTail( pNode->iPrimaryViewID );

	CMatRenderContextPtr pRenderContext( materials );
#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
	pRenderContext->DestroyOcclusionQueryObject( pNode->occlusionQueryHandle );
#endif

	delete pNode; //for now we just new/delete
}

void IncreasePortalViewIDChildLinkCount( PortalViewIDNode_t *pNode )
{
	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		if( pNode->ChildNodes[i] != NULL )
			IncreasePortalViewIDChildLinkCount( pNode->ChildNodes[i] );
	}
	pNode->ChildNodes.AddToTail( NULL );
}

void RemovePortalViewIDChildLinkIndex( PortalViewIDNode_t *pNode, int iRemoveIndex )
{
	Assert( pNode->ChildNodes.Count() > iRemoveIndex );

	if( pNode->ChildNodes[iRemoveIndex] != NULL )
	{
		FreePortalViewIDNode( pNode->ChildNodes[iRemoveIndex] );
		pNode->ChildNodes[iRemoveIndex] = NULL;
	}

	//I know the current behavior for CUtlVector::FastRemove() is to move the tail into the removed index. But I need that behavior to be true in the future as well so I'm doing it explicitly
	pNode->ChildNodes[iRemoveIndex] = pNode->ChildNodes.Tail();
	pNode->ChildNodes.Remove( pNode->ChildNodes.Count() - 1 );

	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		if( pNode->ChildNodes[i] )
			RemovePortalViewIDChildLinkIndex( pNode->ChildNodes[i], iRemoveIndex );
	}
}

//-----------------------------------------------------------------------------
//
// Active Portal class 
//
//-----------------------------------------------------------------------------
CPortalRenderable::CPortalRenderable( void ) : 	
	m_bIsPlaybackPortal( false )
{
	m_matrixThisToLinked.Identity();
	
	//Portal view ID indexing setup
	IncreasePortalViewIDChildLinkCount( &s_PortalRender.m_HeadPortalViewIDNode );
	m_iPortalViewIDNodeIndex = s_PortalRender.m_AllPortals.AddToTail( this );	
}

CPortalRenderable::~CPortalRenderable( void )
{
	int iLast = s_PortalRender.m_AllPortals.Count() - 1;

	//update the soon-to-be-transplanted portal's index
	s_PortalRender.m_AllPortals[iLast]->m_iPortalViewIDNodeIndex = m_iPortalViewIDNodeIndex;

	//I know the current behavior for CUtlVector::FastRemove() is to move the tail into the removed index. But I need that behavior to be true in the future as well so I'm doing it explicitly
	s_PortalRender.m_AllPortals[m_iPortalViewIDNodeIndex] = s_PortalRender.m_AllPortals.Tail();
	s_PortalRender.m_AllPortals.Remove( iLast );
	
	RemovePortalViewIDChildLinkIndex( &s_PortalRender.m_HeadPortalViewIDNode, m_iPortalViewIDNodeIndex ); //does the same transplant operation as above to all portal view id nodes
}


void CPortalRenderable::BeginPortalPixelVisibilityQuery( void )
{
#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
	return;
#endif

	if( g_pPortalRender->ShouldUseStencilsToRenderPortals() ) //this function exists because we require help in texture mode, we need no assistance in stencil mode. Moreover, doing the query twice will probably fubar the results
		return;

	PortalViewIDNode_t *pCurrentPortalViewNode = g_pPortalRender->m_PortalViewIDNodeChain[g_pPortalRender->m_iViewRecursionLevel]->ChildNodes[m_iPortalViewIDNodeIndex];
	
	if( pCurrentPortalViewNode )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->BeginOcclusionQueryDrawing( pCurrentPortalViewNode->occlusionQueryHandle );

		int iX, iY, iWidth, iHeight;
		pRenderContext->GetViewport( iX, iY, iWidth, iHeight );

		pCurrentPortalViewNode->iWindowPixelsAtQueryTime = iWidth * iHeight;
	}
}

void CPortalRenderable::EndPortalPixelVisibilityQuery( void )
{
#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
	return;
#endif

	if( g_pPortalRender->ShouldUseStencilsToRenderPortals() ) //this function exists because we require help in texture mode, we need no assistance in stencil mode. Moreover, doing the query twice will probably fubar the results
		return;

	PortalViewIDNode_t *pCurrentPortalViewNode = g_pPortalRender->m_PortalViewIDNodeChain[g_pPortalRender->m_iViewRecursionLevel]->ChildNodes[m_iPortalViewIDNodeIndex];
	
	if( pCurrentPortalViewNode )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->EndOcclusionQueryDrawing( pCurrentPortalViewNode->occlusionQueryHandle );
	}
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CPortalRender::CPortalRender()
: m_MaterialsAccess( m_Materials )
{
	m_iRemainingPortalViewDepth = 1; //let's portals know that they should do "end of the line" kludges to cover up that portals don't go infinitely recursive
	m_iViewRecursionLevel = 0;
	m_pRenderingViewForPortal = NULL;
	m_pRenderingViewExitPortal = NULL;

	m_PortalViewIDNodeChain[0] = &m_HeadPortalViewIDNode;
}


void CPortalRender::LevelInitPreEntity()
{
	// refresh materials - not sure if this needs to be done every level
	m_Materials.m_Wireframe.Init( "shadertest/wireframe", TEXTURE_GROUP_CLIENT_EFFECTS );
	m_Materials.m_WriteZ_Model.Init( "engine/writez_model", TEXTURE_GROUP_CLIENT_EFFECTS );
	m_Materials.m_TranslucentVertexColor.Init( "engine/TranslucentVertexColor", TEXTURE_GROUP_CLIENT_EFFECTS );
}

void CPortalRender::LevelShutdownPreEntity()
{
	int nCount = m_RecordedPortals.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		delete m_RecordedPortals[i].m_pActivePortal;
	}
	m_RecordedPortals.RemoveAll();
}

//-----------------------------------------------------------------------------
// only use stencils when it's requested, and expensive water won't cause it to freak out
//-----------------------------------------------------------------------------
bool CPortalRender::ShouldUseStencilsToRenderPortals( ) const
{
	// only use stencils when it's requested, and available
	return r_portal_use_stencils.GetBool() && ( materials->StencilBufferBits() != 0 ); 
}


int CPortalRender::ShouldForceCheaperWaterLevel() const
{
	if( r_forcecheapwater.GetBool() )
		return 0;

	if( m_iViewRecursionLevel > 0 )
	{
		if( portalrendertargets->GetWaterReflectionTextureForStencilDepth( m_iViewRecursionLevel ) == NULL )
			return 0;

		PortalViewIDNode_t *pPixelVisNode = m_PortalViewIDNodeChain[m_iViewRecursionLevel - 1]->ChildNodes[m_pRenderingViewForPortal->m_iPortalViewIDNodeIndex];
		
		if( pPixelVisNode->fScreenFilledByPortalSurfaceLastFrame_Normalized >= 0.0f )
		{
			if( pPixelVisNode->fScreenFilledByPortalSurfaceLastFrame_Normalized < 0.005f )
				return 0;

			if( pPixelVisNode->fScreenFilledByPortalSurfaceLastFrame_Normalized < 0.02f )
				return 1;

			if( pPixelVisNode->fScreenFilledByPortalSurfaceLastFrame_Normalized < 0.05f )
				return 2;
		}
	}

	return 3;
}

bool CPortalRender::ShouldObeyStencilForClears() const
{
	return (m_iViewRecursionLevel > 0) && ShouldUseStencilsToRenderPortals();
}

void CPortalRender::WaterRenderingHandler_PreReflection() const
{
	if( (m_iViewRecursionLevel > 0) && ShouldUseStencilsToRenderPortals() )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->SetStencilEnable( false );
	}
}

void CPortalRender::WaterRenderingHandler_PostReflection() const
{
	if( (m_iViewRecursionLevel > 0) && ShouldUseStencilsToRenderPortals() )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->SetStencilEnable( true );
	}
}

void CPortalRender::WaterRenderingHandler_PreRefraction() const
{
	if( (m_iViewRecursionLevel > 0) && ShouldUseStencilsToRenderPortals() )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->SetStencilEnable( false );
	}
}

void CPortalRender::WaterRenderingHandler_PostRefraction() const
{
	if( (m_iViewRecursionLevel > 0) && ShouldUseStencilsToRenderPortals() )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->SetStencilEnable( true );
	}
}


void Recursive_UpdatePortalPixelVisibility( PortalViewIDNode_t *pNode, IMatRenderContext *pRenderContext )
{
	if( pNode->iWindowPixelsAtQueryTime > 0 )
	{
		if( pNode->iOcclusionQueryPixelsRendered < -1 )
		{
			//First couple queries. We seem to be getting bogus 0's on the first queries sometimes. ignore the results.
			++pNode->iOcclusionQueryPixelsRendered;
			pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized = -1.0f;
		}
		else
		{
			pNode->iOcclusionQueryPixelsRendered = pRenderContext->OcclusionQuery_GetNumPixelsRendered( pNode->occlusionQueryHandle );
			pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized = ((float)pNode->iOcclusionQueryPixelsRendered) / ((float)pNode->iWindowPixelsAtQueryTime);
		}
	}
	else
	{
		pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized = -1.0f;
	}

	pNode->iWindowPixelsAtQueryTime = 0;

	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		PortalViewIDNode_t *pChildNode = pNode->ChildNodes[i];
		if( pChildNode )
			Recursive_UpdatePortalPixelVisibility( pChildNode, pRenderContext );
	}
}

void CPortalRender::UpdatePortalPixelVisibility( void )
{
#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
	return;
#endif

	if( m_iViewRecursionLevel != 0 )
		return;

	IMatRenderContext *pRenderContext = materials->GetRenderContext();
	//CMatRenderContextPtr pRenderContext( materials );

	for( int i = m_HeadPortalViewIDNode.ChildNodes.Count(); --i >= 0; )
	{
		PortalViewIDNode_t *pChildNode = m_HeadPortalViewIDNode.ChildNodes[i];
		if( pChildNode )
			Recursive_UpdatePortalPixelVisibility( pChildNode, pRenderContext );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Invalidates pixel visibility data for all portals for this next frame.
//-----------------------------------------------------------------------------
void Recursive_InvalidatePortalPixelVis( PortalViewIDNode_t *pNode )
{
	pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized = -1.0f;
	pNode->iOcclusionQueryPixelsRendered = -5;
	pNode->iWindowPixelsAtQueryTime = 0;
	
	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		PortalViewIDNode_t *pChildNode = pNode->ChildNodes[i];
		if( pChildNode )
			Recursive_InvalidatePortalPixelVis( pChildNode );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Preserves pixel visibility data when view id's are getting swapped around
//-----------------------------------------------------------------------------
void CPortalRender::EnteredPortal( CPortalRenderable *pEnteredPortal )
{
	CPortalRenderable *pExitPortal = pEnteredPortal->GetLinkedPortal();
	Assert( pExitPortal != NULL );

	if ( pExitPortal == NULL )
		return;

	int iNodeLinkCount = m_HeadPortalViewIDNode.ChildNodes.Count();

	PortalViewIDNode_t *pNewHead = m_HeadPortalViewIDNode.ChildNodes[pEnteredPortal->m_iPortalViewIDNodeIndex];
	m_HeadPortalViewIDNode.ChildNodes[pEnteredPortal->m_iPortalViewIDNodeIndex] = NULL;

	//Create a new node that will preserve main's visibility. This new node will be linked to the new head node at the exit portal's index (imagine entering a portal walking backwards)
	PortalViewIDNode_t *pExitPortalsNewNode = AllocPortalViewIDNode( iNodeLinkCount );
	{
		for( int i = 0; i != iNodeLinkCount; ++i )
		{
			pExitPortalsNewNode->ChildNodes[i] = m_HeadPortalViewIDNode.ChildNodes[i];
			m_HeadPortalViewIDNode.ChildNodes[i] = NULL;
		}

		PixelVisibility_ShiftVisibilityViews( VIEW_MAIN, pExitPortalsNewNode->iPrimaryViewID );
		PixelVisibility_ShiftVisibilityViews( VIEW_3DSKY, pExitPortalsNewNode->iPrimaryViewID + 1 );
	}

	

	if( pNewHead ) //it's possible we entered a portal we couldn't see through
	{
		Assert( pNewHead->ChildNodes.Count() == m_HeadPortalViewIDNode.ChildNodes.Count() );
		Assert( pNewHead->ChildNodes[pExitPortal->m_iPortalViewIDNodeIndex] == NULL ); //seeing out of an exit portal back into itself should be impossible

		for( int i = 0; i != iNodeLinkCount; ++i )
		{
			m_HeadPortalViewIDNode.ChildNodes[i] = pNewHead->ChildNodes[i];
			pNewHead->ChildNodes[i] = NULL; //going to be freeing the node in a minute, don't want to kill transplanted children
		}

		//Since the primary views will always be 0 and 1, we have to shift results instead of replacing the id's
		PixelVisibility_ShiftVisibilityViews( pNewHead->iPrimaryViewID, VIEW_MAIN );
		PixelVisibility_ShiftVisibilityViews( pNewHead->iPrimaryViewID + 1, VIEW_3DSKY );

		FreePortalViewIDNode( pNewHead );
	}

	Assert( m_HeadPortalViewIDNode.ChildNodes[pExitPortal->m_iPortalViewIDNodeIndex] == NULL ); //asserted above in pNewHead code, but call me paranoid
	m_HeadPortalViewIDNode.ChildNodes[pExitPortal->m_iPortalViewIDNodeIndex] = pExitPortalsNewNode;

	//Because pixel visibility is based off of *last* frame's visibility. We can get cases where a certain portal
	//wasn't visible last frame, but is takes up most of the screen this frame.
	//Set all portal pixel visibility to unknown visibility.
	for( int i = m_HeadPortalViewIDNode.ChildNodes.Count(); --i >= 0; )
	{
		PortalViewIDNode_t *pChildNode = m_HeadPortalViewIDNode.ChildNodes[i];
		if( pChildNode )
			Recursive_InvalidatePortalPixelVis( pChildNode );
	}
}




   
bool CPortalRender::DrawPortalsUsingStencils( CViewRender *pViewRender )
{	  
	VPROF( "CPortalRender::DrawPortalsUsingStencils" );

	if( !ShouldUseStencilsToRenderPortals() )
		return false;

	int iDrawFlags = pViewRender->GetDrawFlags();

	if ( (iDrawFlags & DF_RENDER_REFLECTION) != 0 )
		return false;

	if ( ((iDrawFlags & DF_CLIP_Z) != 0) && ((iDrawFlags & DF_CLIP_BELOW) == 0) ) //clipping above the water height
		return false;

	int iNumRenderablePortals = m_ActivePortals.Count();

	// This loop is necessary because tools can suppress rendering without telling the portal system
	CUtlVector< CPortalRenderable* > actualActivePortals( 0, iNumRenderablePortals );
	for ( int i = 0; i < iNumRenderablePortals; ++i )
	{
		CPortalRenderable *pPortalRenderable = m_ActivePortals[i];
		C_BaseEntity *pPairedEntity = pPortalRenderable->PortalRenderable_GetPairedEntity();
		bool bIsVisible = (pPairedEntity == NULL) || (pPairedEntity->IsVisible() && pPairedEntity->ShouldDraw()); //either unknown visibility or definitely visible.

		if ( !pPortalRenderable->m_bIsPlaybackPortal )
		{
			if ( !bIsVisible )
			{
				//can't see through the portal, free up it's view id node for use elsewhere
				if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] != NULL )
				{
					FreePortalViewIDNode( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] );
					m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortalRenderable->m_iPortalViewIDNodeIndex] = NULL;
				}

				continue;
			}
		}

		actualActivePortals.AddToTail( m_ActivePortals[i] );
	}
	iNumRenderablePortals = actualActivePortals.Count();
	if( iNumRenderablePortals == 0 )
		return false;

	const int iMaxDepth = MIN( r_portal_stencil_depth.GetInt(), MIN( MAX_PORTAL_RECURSIVE_VIEWS, (1 << materials->StencilBufferBits()) ) - 1 );

	if( m_iViewRecursionLevel >= iMaxDepth ) //can't support any more views	
	{
		m_iRemainingPortalViewDepth = 0; //special case handler for max depth 0 cases
		for( int i = 0; i != iNumRenderablePortals; ++i )
		{
			CPortalRenderable *pCurrentPortal = actualActivePortals[i];
			pCurrentPortal->DrawPortal();
		}
		return false;
	}

	m_iRemainingPortalViewDepth = (iMaxDepth - m_iViewRecursionLevel) - 1;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Flush( true ); //to prevent screwing up the last opaque object

	//queued mode makes us pass the barrier of just noticeable difference when using a previous frame's occlusion as a draw skip check
	bool bIsQueuedMode = (materials->GetThreadMode() == MATERIAL_QUEUED_THREADED);

	const CViewSetup *pViewSetup = pViewRender->GetViewSetup();
	m_RecursiveViewSetups[m_iViewRecursionLevel] = *pViewSetup;

	CViewSetup ViewBackup;// = *pViewSetup; //backup the view, we'll need to restore it
	memcpy( &ViewBackup, pViewSetup, sizeof( CViewSetup ) );

	Vector ptCameraOrigin = pViewSetup->origin;
	Vector vCameraForward;
	AngleVectors( pViewSetup->angles, &vCameraForward, NULL, NULL );

	int iX, iY, iWidth, iHeight;
	pRenderContext->GetViewport( iX, iY, iWidth, iHeight );
#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
	int iScreenPixelCount = iWidth * iHeight;
#endif

	bool bRebuildDrawListsWhenDone = false;


	int iParentLevelStencilReferenceValue = m_iViewRecursionLevel;
	int iStencilReferenceValue = iParentLevelStencilReferenceValue + 1;
	
	if( m_iViewRecursionLevel == 0 ) //first entry into the stencil drawing
	{
		pRenderContext->SetStencilEnable( true );
		pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_ALWAYS );
		pRenderContext->SetStencilPassOperation( STENCILOPERATION_REPLACE );
		pRenderContext->SetStencilFailOperation( STENCILOPERATION_KEEP );
		pRenderContext->SetStencilZFailOperation( STENCILOPERATION_KEEP );
		pRenderContext->SetStencilTestMask( 0xFF );
		pRenderContext->SetStencilWriteMask( 0xFF );
		pRenderContext->SetStencilReferenceValue( 0 );

        m_RecursiveViewComplexFrustums[0].RemoveAll(); //clear any garbage leftover in the complex frustums from last frame
	}

	if( m_RecursiveViewComplexFrustums[m_iViewRecursionLevel].Count() == 0 )
	{
		//nothing in the complex frustum from the current view, copy the standard frustum in
		m_RecursiveViewComplexFrustums[m_iViewRecursionLevel].AddMultipleToTail( FRUSTUM_NUMPLANES, pViewRender->GetFrustum() );
	}

	for( int i = 0; i != iNumRenderablePortals; ++i )
	{
		CPortalRenderable *pCurrentPortal = actualActivePortals[i];

		m_RecursiveViewComplexFrustums[m_iViewRecursionLevel + 1].RemoveAll(); //clear any previously stored complex frustum
		
		if( (pCurrentPortal->GetLinkedPortal() == NULL) ||
			(pCurrentPortal == m_pRenderingViewExitPortal) ||
			(pCurrentPortal->ShouldUpdatePortalView_BasedOnView( *pViewSetup, m_RecursiveViewComplexFrustums[m_iViewRecursionLevel] ) == false) )
		{
			//can't see through the portal, free up it's view id node for use elsewhere
			if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] != NULL )
			{
				FreePortalViewIDNode( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] );
				m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] = NULL;
			}
			continue;
		}

		Assert( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes.Count() > pCurrentPortal->m_iPortalViewIDNodeIndex );

		if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] == NULL )
			m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] = AllocPortalViewIDNode( m_HeadPortalViewIDNode.ChildNodes.Count() );

		PortalViewIDNode_t *pCurrentPortalViewNode = m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex];
		
		// Step 0, Allow for special effects to happen before cutting a hole
		{
			pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
			pRenderContext->SetStencilPassOperation( STENCILOPERATION_KEEP );
			pRenderContext->SetStencilFailOperation( STENCILOPERATION_KEEP );
			pRenderContext->SetStencilZFailOperation( STENCILOPERATION_KEEP );
			pRenderContext->SetStencilReferenceValue( iParentLevelStencilReferenceValue );

			pCurrentPortal->DrawPreStencilMask();
		}

		//step 1, write out the stencil values (and colors if you want, but really not necessary)
		{
			//pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
			pRenderContext->SetStencilPassOperation( STENCILOPERATION_INCR );
			//pRenderContext->SetStencilFailOperation( STENCILOPERATION_KEEP );
			//pRenderContext->SetStencilZFailOperation( STENCILOPERATION_KEEP );
			//pRenderContext->SetStencilReferenceValue( iParentLevelStencilReferenceValue );

#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
			pRenderContext->BeginOcclusionQueryDrawing( pCurrentPortalViewNode->occlusionQueryHandle );
			pCurrentPortalViewNode->iWindowPixelsAtQueryTime = iScreenPixelCount;
#endif

			pCurrentPortal->DrawStencilMask();

#ifndef TEMP_DISABLE_PORTAL_VIS_QUERY
			pRenderContext->EndOcclusionQueryDrawing( pCurrentPortalViewNode->occlusionQueryHandle );
#endif
		}

		//see if we can skip the heavy lifting due to low visibility
		if ( bIsQueuedMode || //don't use pixel visibly as a skip check in queued mode, the data is simply too old.
			pCurrentPortal->ShouldUpdatePortalView_BasedOnPixelVisibility( pCurrentPortalViewNode->fScreenFilledByPortalSurfaceLastFrame_Normalized ) )
		{
			//step 2, clear the depth buffer in stencil areas so we can render a new scene to them
			{
				pRenderContext->SetStencilPassOperation( STENCILOPERATION_KEEP );
				pRenderContext->SetStencilReferenceValue( iStencilReferenceValue );
				pRenderContext->ClearBuffersObeyStencil( false, true );
			}


			//step 3, fill in stencil views (remember that in multiple depth situations that any subportals will run through this function again before this section completes, thereby screwing with stencil settings)
			{
				bRebuildDrawListsWhenDone = true;

				MaterialFogMode_t fogModeBackup = pRenderContext->GetFogMode();
				unsigned char fogColorBackup[4];
				pRenderContext->GetFogColor( fogColorBackup );
				float fFogStartBackup, fFogEndBackup, fFogZBackup;
				pRenderContext->GetFogDistances( &fFogStartBackup, &fFogEndBackup, &fFogZBackup );
				CGlowOverlay::BackupSkyOverlayData( m_iViewRecursionLevel );

				Assert( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes.Count() > pCurrentPortal->m_iPortalViewIDNodeIndex );

				m_PortalViewIDNodeChain[m_iViewRecursionLevel + 1] = m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex];
				
				pCurrentPortal->RenderPortalViewToBackBuffer( pViewRender, *pViewSetup );
				
				m_PortalViewIDNodeChain[m_iViewRecursionLevel + 1] = NULL;

				CGlowOverlay::RestoreSkyOverlayData( m_iViewRecursionLevel );
				memcpy( (void *)pViewSetup, &ViewBackup, sizeof( CViewSetup ) );
				pViewRender->m_pActiveRenderer->EnableWorldFog();

				pRenderContext->FogMode( fogModeBackup );
				pRenderContext->FogColor3ubv( fogColorBackup );
				pRenderContext->FogStart( fFogStartBackup );
				pRenderContext->FogEnd( fFogEndBackup );
				pRenderContext->SetFogZ( fFogZBackup );

				
				//do a full reset of what we think the stencil operations are in case the recursive calls got weird
				pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
				pRenderContext->SetStencilPassOperation( STENCILOPERATION_KEEP );
				pRenderContext->SetStencilFailOperation( STENCILOPERATION_KEEP );
				pRenderContext->SetStencilZFailOperation( STENCILOPERATION_KEEP );
				pRenderContext->SetStencilTestMask( 0xFF );
				pRenderContext->SetStencilWriteMask( 0xFF );
				pRenderContext->SetStencilReferenceValue( iStencilReferenceValue );
			}

			//step 4, patch up the fact that we just made a hole in the wall because it's not *really* a hole at all
			{
				pCurrentPortal->DrawPostStencilFixes();
			}
		}

		//step 5, restore the stencil mask to the parent level
		{
			pRenderContext->SetStencilReferenceValue( iStencilReferenceValue );
			pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
			pRenderContext->SetStencilPassOperation( STENCILOPERATION_DECR );
			pRenderContext->SetStencilFailOperation( STENCILOPERATION_KEEP );
			pRenderContext->SetStencilZFailOperation( STENCILOPERATION_KEEP );

			pRenderContext->PerformFullScreenStencilOperation();
		}
	}

	//step 6, go back to non-stencil rendering mode in preparation to resume normal scene rendering
	if( m_iViewRecursionLevel == 0 )
	{
		Assert( m_pRenderingViewForPortal == NULL );
		Assert( m_pRenderingViewExitPortal == NULL );
		m_pRenderingViewExitPortal = NULL;
		m_pRenderingViewForPortal = NULL;

		pRenderContext->SetStencilEnable( false );
		pRenderContext->SetStencilCompareFunction(STENCILCOMPARISONFUNCTION_NEVER);
		pRenderContext->SetStencilPassOperation(STENCILOPERATION_KEEP);
		pRenderContext->SetStencilFailOperation(STENCILOPERATION_KEEP);
		pRenderContext->SetStencilZFailOperation(STENCILOPERATION_KEEP);
		pRenderContext->SetStencilTestMask( 0xFF );
		pRenderContext->SetStencilWriteMask( 0xFF );
		pRenderContext->SetStencilReferenceValue( 0 );

		m_RecursiveViewComplexFrustums[0].RemoveAll();
	}
	else
	{
		pRenderContext->SetStencilReferenceValue( iParentLevelStencilReferenceValue );
		pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
		pRenderContext->SetStencilPassOperation( STENCILOPERATION_KEEP );
	}

	if( bRebuildDrawListsWhenDone )
	{
		memcpy( (void *)pViewSetup, &ViewBackup, sizeof( CViewSetup ) ); //if we don't restore this, the view is permanently altered (in mid render of an existing scene)
	}

	pRenderContext->Flush( true ); //just in case

	++m_iRemainingPortalViewDepth;
		  
	for( int i = 0; i != iNumRenderablePortals; ++i )
	{
		CPortalRenderable *pCurrentPortal = actualActivePortals[i];
		pCurrentPortal->DrawPortal();
	}

	return bRebuildDrawListsWhenDone;
}






#ifdef _DEBUG
extern bool g_bRenderingCameraView;
#endif

void CPortalRender::DrawPortalsToTextures( CViewRender *pViewRender, const CViewSetup &cameraView )
{
	if( ShouldUseStencilsToRenderPortals() )
		return;

	/*if ( (pViewRender->GetDrawFlags() & DF_RENDER_REFLECTION) != 0 )
		return;*/

	m_iRemainingPortalViewDepth = 1;
	m_iViewRecursionLevel = 0;
	m_pRenderingViewForPortal = NULL;
	m_pRenderingViewExitPortal = NULL;

	m_RecursiveViewSetups[m_iViewRecursionLevel] = cameraView;

	m_RecursiveViewComplexFrustums[0].RemoveAll(); //clear any garbage leftover in the complex frustums from last frame
	m_RecursiveViewComplexFrustums[0].AddMultipleToTail( FRUSTUM_NUMPLANES, pViewRender->GetFrustum() );


#ifdef _DEBUG
	g_bRenderingCameraView = true;
#endif

	int iNumRenderablePortals = g_pPortalRender->m_ActivePortals.Count();

	Vector ptCameraOrigin = cameraView.origin;

	//an extraneous push to update the frustum
	render->Push3DView( cameraView, 0, NULL, pViewRender->GetFrustum() );

	for( int i = 0; i != iNumRenderablePortals; ++i )
	{
		CPortalRenderable *pCurrentPortal = g_pPortalRender->m_ActivePortals[i];

		if( (pCurrentPortal->ShouldUpdatePortalView_BasedOnView( cameraView, m_RecursiveViewComplexFrustums[m_iViewRecursionLevel] ) == false) ||
			(pCurrentPortal->GetLinkedPortal() == NULL) )
		{
			//can't see through the portal, free up it's view id node for use elsewhere
			if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] != NULL )
			{
				FreePortalViewIDNode( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] );
				m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] = NULL;
			}
			continue;
		}

		Assert( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes.Count() > pCurrentPortal->m_iPortalViewIDNodeIndex );

		if( m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] == NULL )
			m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex] = AllocPortalViewIDNode( m_HeadPortalViewIDNode.ChildNodes.Count() );

		m_PortalViewIDNodeChain[m_iViewRecursionLevel + 1] = m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pCurrentPortal->m_iPortalViewIDNodeIndex];

		pCurrentPortal->RenderPortalViewToTexture( pViewRender, cameraView );

		m_PortalViewIDNodeChain[m_iViewRecursionLevel + 1] = NULL;		
	}	

	render->PopView( pViewRender->GetFrustum() );

	m_iRemainingPortalViewDepth = 1;
	m_iViewRecursionLevel = 0;

	Assert( m_pRenderingViewForPortal == NULL );
	Assert( m_pRenderingViewExitPortal == NULL );
	m_pRenderingViewForPortal = NULL;
	m_pRenderingViewExitPortal = NULL;

#ifdef _DEBUG
	g_bRenderingCameraView = false;
#endif
}

void CPortalRender::AddPortal( CPortalRenderable *pPortal )
{
	for( int i = m_ActivePortals.Count(); --i >= 0; )
	{
		if( m_ActivePortals[i] == pPortal )
			return;
	}

	m_ActivePortals.AddToTail( pPortal );
}

void CPortalRender::RemovePortal( CPortalRenderable *pPortal )
{
	for( int i = m_ActivePortals.Count(); --i >= 0; )
	{
		if( m_ActivePortals[i] == pPortal )
		{
			m_ActivePortals.FastRemove( i );
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Are we currently rendering a portal?
//-----------------------------------------------------------------------------
bool CPortalRender::IsRenderingPortal() const
{
	return m_pRenderingViewForPortal != NULL;
}


//-----------------------------------------------------------------------------
// Returns view recursion level
//-----------------------------------------------------------------------------
int CPortalRender::GetViewRecursionLevel() const
{
	return m_iViewRecursionLevel;
}

//-----------------------------------------------------------------------------
//normalized for how many of the screen's possible pixels it takes up, less than zero indicates a lack of data from last frame
//-----------------------------------------------------------------------------
float CPortalRender::GetPixelVisilityForPortalSurface( const CPortalRenderable *pPortal ) const
{
	PortalViewIDNode_t *pNode = m_PortalViewIDNodeChain[m_iViewRecursionLevel]->ChildNodes[pPortal->m_iPortalViewIDNodeIndex];
	if( pNode )
		return pNode->fScreenFilledByPortalSurfaceLastFrame_Normalized;

	return -1.0f;
}


//-----------------------------------------------------------------------------
// Methods to query about the exit portal associated with the currently rendering portal
//-----------------------------------------------------------------------------
const Vector &CPortalRender::GetExitPortalFogOrigin() const
{
	return m_pRenderingViewExitPortal->GetFogOrigin();
}

void CPortalRender::ShiftFogForExitPortalView() const
{
	if ( m_pRenderingViewExitPortal )
	{
		m_pRenderingViewExitPortal->ShiftFogForExitPortalView();
	}
}

void CPortalRenderable::ShiftFogForExitPortalView() const
{
	CMatRenderContextPtr pRenderContext( materials );
	float fFogStart, fFogEnd, fFogZ;
	pRenderContext->GetFogDistances( &fFogStart, &fFogEnd, &fFogZ );

	Vector vFogOrigin = GetFogOrigin();
	Vector vCameraToExitPortal = vFogOrigin - CurrentViewOrigin();
	float fDistModifier = vCameraToExitPortal.Dot( CurrentViewForward() );

	fFogStart += fDistModifier;
	fFogEnd += fDistModifier;
	//fFogZ += something; //FIXME: find out what the hell to do with this

	pRenderContext->FogStart( fFogStart );
	pRenderContext->FogEnd( fFogEnd );
	pRenderContext->SetFogZ( fFogZ );
}

SkyboxVisibility_t CPortalRender::IsSkyboxVisibleFromExitPortal() const
{
	return m_pRenderingViewExitPortal->SkyBoxVisibleFromPortal();
}

bool CPortalRender::DoesExitPortalViewIntersectWaterPlane( float waterZ, int leafWaterDataID ) const
{
	return m_pRenderingViewExitPortal->DoesExitViewIntersectWaterPlane( waterZ, leafWaterDataID );
}


//-----------------------------------------------------------------------------
// Returns the remaining number of portals to render within other portals
// lets portals know that they should do "end of the line" kludges to cover up that portals don't go infinitely recursive
//-----------------------------------------------------------------------------
int	CPortalRender::GetRemainingPortalViewDepth() const
{
	return m_iRemainingPortalViewDepth;
}


//-----------------------------------------------------------------------------
// Returns the current View IDs 
//-----------------------------------------------------------------------------
int CPortalRender::GetCurrentViewId() const
{
	Assert( m_PortalViewIDNodeChain[m_iViewRecursionLevel] != NULL );
#ifdef _DEBUG
	for( int i = 0; i != m_iViewRecursionLevel; ++i )
	{
		Assert( m_PortalViewIDNodeChain[i]->iPrimaryViewID != m_PortalViewIDNodeChain[m_iViewRecursionLevel]->iPrimaryViewID );
	}
#endif

	return m_PortalViewIDNodeChain[m_iViewRecursionLevel]->iPrimaryViewID;
}

int CPortalRender::GetCurrentSkyboxViewId() const
{
	Assert( m_PortalViewIDNodeChain[m_iViewRecursionLevel] != NULL );
	return m_PortalViewIDNodeChain[m_iViewRecursionLevel]->iPrimaryViewID + 1;
}


void OverlayCameraRenderTarget( const char *pszMaterialName, float flX, float flY, float w, float h ); //implemented in view_scene.cpp

void CPortalRender::OverlayPortalRenderTargets( float w, float h )
{
	OverlayCameraRenderTarget( "engine/debug_portal_1", 0,0, w,h );
	OverlayCameraRenderTarget( "engine/debug_portal_2", w+10,0, w,h );

	OverlayCameraRenderTarget( "engine/debug_water_reflect_0", 0, h+10, w,h );
	OverlayCameraRenderTarget( "engine/debug_water_reflect_1", w+10, h+10, w,h );
	OverlayCameraRenderTarget( "engine/debug_water_reflect_2", (w+10) * 2, h+10, w,h );

	OverlayCameraRenderTarget( "engine/debug_water_refract_0", 0, (h+10) * 2, w,h );
	OverlayCameraRenderTarget( "engine/debug_water_refract_1", w+10, (h+10) * 2, w,h );
	OverlayCameraRenderTarget( "engine/debug_water_refract_2", (w+10) * 2, (h+10) * 2, w,h );
}

void CPortalRender::UpdateDepthDoublerTexture( const CViewSetup &viewSetup )
{
	bool bShouldUpdate = false;

	for( int i = m_ActivePortals.Count(); --i >= 0; )
	{
		CPortalRenderable *pPortal = m_ActivePortals[i];

		if( pPortal->ShouldUpdateDepthDoublerTexture( viewSetup ) )
		{
			bShouldUpdate = true;
			break;
		}
	}
	
	if( bShouldUpdate )
	{
		Rect_t srcRect;
		srcRect.x = viewSetup.x;
		srcRect.y = viewSetup.y;
		srcRect.width = viewSetup.width;
		srcRect.height = viewSetup.height;

		ITexture *pTexture = portalrendertargets->GetDepthDoublerTexture();

		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->CopyRenderTargetToTextureEx( pTexture, 0, &srcRect, NULL );
	}
}


//-----------------------------------------------------------------------------
// Finds a recorded portal
//-----------------------------------------------------------------------------
int CPortalRender::FindRecordedPortalIndex( int nPortalId )
{
	int nCount = m_RecordedPortals.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_RecordedPortals[i].m_nPortalId == nPortalId )
			return i;
	}
	return -1;
}

CPortalRenderable* CPortalRender::FindRecordedPortal( int nPortalId )
{
	int nIndex = FindRecordedPortalIndex( nPortalId );
	return ( nIndex >= 0 ) ? m_RecordedPortals[nIndex].m_pActivePortal : NULL;
}

CPortalRenderable* CPortalRender::FindRecordedPortal( IClientRenderable *pRenderable )
{
	int nCount = m_RecordedPortals.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_RecordedPortals[i].m_pPlaybackRenderable == pRenderable )
			return m_RecordedPortals[i].m_pActivePortal;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Handles a portal update message
//-----------------------------------------------------------------------------
void CPortalRender::HandlePortalPlaybackMessage( KeyValues *pKeyValues )
{
	// Iterate through all the portal ids of all the portals in the keyvalues message
	CUtlVector<int> foundIds;
	for ( KeyValues *pCurr = pKeyValues->GetFirstTrueSubKey(); pCurr; pCurr = pCurr->GetNextTrueSubKey() )
	{
		// Create new area portals for those ids that don't exist
        int nPortalId = pCurr->GetInt( "portalId" );
		IClientRenderable *pRenderable = (IClientRenderable*)pCurr->GetPtr( "clientRenderable" );
		int nIndex = FindRecordedPortalIndex( nPortalId );
		if ( nIndex < 0 )
		{
			CPortalRenderable *pPortal = NULL;
			const char *szType = pCurr->GetString( "portalType", "flatBasic" ); //"flatBasic" being the type commonly found in "Portal" mod
			//search through registered creation functions for one that makes this type of portal
			for( int i = m_PortalRenderableCreators.Count(); --i >= 0; )
			{
				if( FStrEq( szType, m_PortalRenderableCreators[i].portalType.String() ) )
				{
					pPortal = m_PortalRenderableCreators[i].creationFunc();
					break;
				}
			}

			if( pPortal == NULL )
			{
				AssertMsg( false, "Unable to find creation function for portal type." );
				Warning( "CPortalRender::HandlePortalPlaybackMessage() unable to find creation function for portal type: %s\n", szType );
			}
			else
			{
				pPortal->m_bIsPlaybackPortal = true;
				int k = m_RecordedPortals.AddToTail( );
				m_RecordedPortals[k].m_pActivePortal = pPortal;
				m_RecordedPortals[k].m_nPortalId = nPortalId;
				m_RecordedPortals[k].m_pPlaybackRenderable = pRenderable;
				AddPortal( pPortal );
			}
		}
		else
		{
			m_RecordedPortals[nIndex].m_pPlaybackRenderable = pRenderable;
		}
		foundIds.AddToTail( nPortalId );
	}

	// Delete portals that didn't appear in the list
	int nFoundCount = foundIds.Count();
	int nCount = m_RecordedPortals.Count();
	for ( int i = nCount; --i >= 0; )
	{
		int j;
		for ( j = 0; j < nFoundCount; ++j )
		{
			if ( foundIds[j] == m_RecordedPortals[i].m_nPortalId )
				break;
		}

		if ( j == nFoundCount )
		{
			RemovePortal( m_RecordedPortals[i].m_pActivePortal );
			delete m_RecordedPortals[i].m_pActivePortal;
			m_RecordedPortals.FastRemove(i);
		}
	}

	// Iterate through all the portal ids of all the portals in the keyvalues message
	for ( KeyValues *pCurr = pKeyValues->GetFirstTrueSubKey(); pCurr; pCurr = pCurr->GetNextTrueSubKey() )
	{
		// Update the state of the portals based on the recorded info
		int nPortalId = pCurr->GetInt( "portalId" );
		CPortalRenderable *pPortal = FindRecordedPortal( nPortalId );
		Assert( pPortal );

		pPortal->HandlePortalPlaybackMessage( pCurr );
	}

	// Make the portals update their internal state
	/*nCount = m_RecordedPortals.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_RecordedPortals[i].m_pActivePortal->PortalMoved();
		m_RecordedPortals[i].m_pActivePortal->ComputeLinkMatrix();
	}*/
}


void CPortalRender::AddPortalCreationFunc( const char *szPortalType, PortalRenderableCreationFunc creationFunc )
{
#ifdef _DEBUG
	for( int i = m_PortalRenderableCreators.Count(); --i >= 0; )
	{
		AssertMsg( FStrEq( m_PortalRenderableCreators[i].portalType.String(), szPortalType ) == false, "Multiple portal renderable creation functions for same type of portal renderable."  );
	}
#endif

	PortalRenderableCreationFunction_t temp;
	temp.creationFunc = creationFunc;
	temp.portalType.Set( szPortalType );

	m_PortalRenderableCreators.AddToTail( temp );
}

bool Recursive_IsPortalViewID( PortalViewIDNode_t *pNode, view_id_t id )
{	
	if ( pNode->iPrimaryViewID == id )
		return true;

	for( int i = pNode->ChildNodes.Count(); --i >= 0; )
	{
		PortalViewIDNode_t *pChildNode = pNode->ChildNodes[i];
		if( pChildNode )
		{
			return Recursive_IsPortalViewID( pChildNode, id );
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Tests the parameter view ID against ID's used by portal pixel vis queries
// Input  : id - id tested against used portal view ids
// Output : Returns true if id matches an ID used by a portal, or it's recursive sub portals
//-----------------------------------------------------------------------------
bool CPortalRender::IsPortalViewID( view_id_t id )
{
	if ( id == m_HeadPortalViewIDNode.iPrimaryViewID )
		return true;

	for ( int i = 0; i < MAX_PORTAL_RECURSIVE_VIEWS; ++i )
	{
		PortalViewIDNode_t* pNode = m_PortalViewIDNodeChain[i];
		if ( pNode )
		{
			// recursively search child nodes, they get their own ids.
			if ( Recursive_IsPortalViewID( pNode, id ) )
				return true;
		}
	}

	return false;
}






