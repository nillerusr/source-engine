//========= Copyright Valve Corporation, All rights reserved. ============//
// SculptOptions.cpp : implementation file
//

#include <stdafx.h>
#include "hammer.h"
#include "CollisionUtils.h"
#include "resource.h"
#include "ToolDisplace.h"
#include "MainFrm.h"
#include "FaceEditSheet.h"
#include "GlobalFunctions.h"
#include "MapAtom.h"
#include "MapSolid.h"
#include "MapView3D.h"
#include "History.h"
#include "Camera.h"
#include "MapDoc.h"
#include "ChunkFile.h"
#include "ToolManager.h"
#include "bitmap/tgaloader.h"
#include "tier1/utlbuffer.h"
#include "Material.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/itexture.h"
#include "../materialsystem/itextureinternal.h"
#include "pixelwriter.h"
#include "TextureSystem.h"
#include "SculptOptions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


extern CToolDisplace* GetDisplacementTool();
extern void FaceListSewEdges( void );


CUtlMap<EditDispHandle_t, CMapDisp *>		CSculptTool::m_OrigMapDisp( 3, 3, CSculptTool::MapDispLessFunc );


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CSculptTool::CSculptTool()
{
	m_PaintOwner = NULL;

	m_MousePoint.Init();
	m_StartingCollisionNormal.Init();

	m_OriginalCollisionPoint.Init();

	m_bAltDown = m_bCtrlDown = m_bShiftDown = false;

	m_bLMBDown = m_bRMBDown = false;
	m_ValidPaintingSpot = false;
	m_BrushSize = 50;

	m_StartingProjectedRadius = m_OriginalProjectedRadius = 10.0f;

	m_OriginalCollisionValid = m_CurrentCollisionValid = false;
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CSculptTool::~CSculptTool()
{
	FOR_EACH_MAP( m_OrigMapDisp, pos )
	{
		delete m_OrigMapDisp.Element( pos );
	}
	m_OrigMapDisp.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: setup for starting to paint on the displacement
// Input  : pView - the 3d view
//			vPoint - the initial click point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptTool::BeginPaint( CMapView3D *pView, const Vector2D &vPoint )
{
	DuplicateSelectedDisp();

	GetStartingSpot( pView, vPoint );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: main routine called when mouse move has happened to start painting
// Input  : pView - the 3d view
//			vPoint - the mouse point
//			SpatialData - the spatial data ( mostly ignored )
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptTool::Paint( CMapView3D *pView, const Vector2D &vPoint, SpatialPaintData_t &SpatialData )
{
	m_SpatialData = SpatialData;

	// Successful paint operation.
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: determines if any of the special keys ( control, shift, alt ) are pressed
//-----------------------------------------------------------------------------
void CSculptTool::DetermineKeysDown()
{
	m_bCtrlDown = ( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 );
	m_bShiftDown = ( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 );
	m_bAltDown = ( ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) != 0 );
}


//-----------------------------------------------------------------------------
// Purpose: handles the left mouse button up in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptTool::OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	DetermineKeysDown();

	// left button up
	m_bLMBDown = false;
	m_MousePoint = vPoint;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the left mouse button down in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptTool::OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	DetermineKeysDown();

	// left button down
	m_bLMBDown = true;
	m_MousePoint = vPoint;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the right mouse button up in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptTool::OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	DetermineKeysDown();

	// right button up
	m_bRMBDown = false;
	m_MousePoint = vPoint;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the right mouse button down in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptTool::OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	DetermineKeysDown();

	// right button down
	m_bRMBDown = true;
	m_MousePoint = vPoint;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the mouse move in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptTool::OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	DetermineKeysDown();

	m_MousePoint = vPoint;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: called just before painting begins to gather reference information
// Input  : pView - the 3d view
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptTool::PrePaint( CMapView3D *pView, const Vector2D &vPoint )
{
	Vector2D	RadiusPoint = vPoint;
	Vector		vecStart, vecEnd;

	RadiusPoint.x += m_BrushSize;
	pView->GetCamera()->BuildRay( RadiusPoint, vecStart, vecEnd );

	m_OriginalCollisionValid = FindCollisionIntercept( pView->GetCamera(), vPoint, true, m_OriginalCollisionPoint, m_OriginalCollisionNormal, m_OriginalCollisionIntercept );
	if ( m_OriginalCollisionValid )
	{
		m_OriginalProjectedRadius = CalcDistanceToLine( m_OriginalCollisionPoint, vecStart, vecEnd );
	}

	m_CurrentCollisionValid = FindCollisionIntercept( pView->GetCamera(), vPoint, false, m_CurrentCollisionPoint, m_CurrentCollisionNormal, m_CurrentCollisionIntercept );
	if ( m_CurrentCollisionValid )
	{
		m_CurrentProjectedRadius = CalcDistanceToLine( m_CurrentCollisionPoint, vecStart, vecEnd );
	}

	m_SpatialData.m_flRadius = 128.0f;
	m_SpatialData.m_flRadius2 = ( m_SpatialData.m_flRadius * m_SpatialData.m_flRadius );
	m_SpatialData.m_flOORadius2 = 1.0f / m_SpatialData.m_flRadius2;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: called after painting finishes to finalize things
// Input  : bAutoSew - should we sew the edges
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptTool::PostPaint( bool bAutoSew )
{
	// Get the displacement manager from the active map document.
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return false;

	// Update the modified displacements.
	int nDispCount = pDispMgr->SelectCount();
	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp )
		{
			pDisp->Paint_Update( false );
		}
	}

	// Auto "sew" if necessary.
	if ( bAutoSew )
	{
		FaceListSewEdges();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: called to dispatch the painting routine across all selected displacements
// Input  : pView - the 3d view
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptTool::DoPaint( CMapView3D *pView, const Vector2D &vPoint )
{
	// Get the displacement manager from the active map document.
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return false;

	// For each displacement surface is the selection list attempt to paint on it.
	int nDispCount = pDispMgr->SelectCount();
	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp )
		{
			CMapDisp	*OrigDisp = NULL;
			int			index = m_OrigMapDisp.Find( pDisp->GetEditHandle() );
			
			if ( index != m_OrigMapDisp.InvalidIndex() )
			{
				OrigDisp = m_OrigMapDisp[ index ];
			}
			DoPaintOperation( pView, vPoint, pDisp, OrigDisp );
		}
	}

	// Successful paint.
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: checks to see if a given displacement vert lies within the 2d screenspace of the circle
// Input  : pView - the 3d view
//			pDisp - the displacement the vert belongs to
//			pOrigDisp - the displacement prior to any moving
//			nVertIndex - the vert index
//			bUseOrigDisplacement - should we use the vert from the original displacement
//			bUseCurrentPosition - should we use the current collision test point
// Output : returns true if the point is within the circle
//-----------------------------------------------------------------------------
bool CSculptTool::IsPointInScreenCircle( CMapView3D *pView, CMapDisp *pDisp, CMapDisp *pOrigDisp, int nVertIndex, bool bUseOrigDisplacement, bool bUseCurrentPosition, float *pflLengthPercent )
{
	Vector	vVert, vTestVert;

	pDisp->GetVert( nVertIndex, vVert );

	if ( pOrigDisp && bUseOrigDisplacement )
	{
		pOrigDisp->GetVert( nVertIndex, vTestVert );
	}
	else
	{
		vTestVert = vVert;
	}

#if 0
	Vector2D ViewVert;
	pView->GetCamera()->WorldToView( vTestVert, ViewVert );

	Vector2D	Offset = ViewVert - m_MousePoint;
	float		Length = Offset.Length();

	return ( Length <= m_BrushSize );
#else
	if ( bUseCurrentPosition )
	{
		if ( !m_CurrentCollisionValid )
		{
			return false;
		}

		Vector	Offset = m_CurrentCollisionPoint - vTestVert;
		float	Length = Offset.Length();

		if ( pflLengthPercent )
		{
			*pflLengthPercent = Length / m_CurrentProjectedRadius;
		}

		return ( Length <= m_CurrentProjectedRadius );
	}
	else
	{
		if ( !m_OriginalCollisionValid )
		{
			return false;
		}

		Vector	Offset = m_OriginalCollisionPoint - vTestVert;
		float	Length = Offset.Length();

		if ( pflLengthPercent )
		{
			*pflLengthPercent = Length / m_OriginalProjectedRadius;
		}

#if 0
		if ( Length <= m_OriginalProjectedRadius || vertIndex == 66 )
		{
			Msg( "%d: ( %g %g %g ) from %g <= %g at ( %g %g %g )\n", vertIndex, vTestVert.x, vTestVert.y, vTestVert.z, Length, m_OriginalProjectedRadius, m_OriginalCollisionPoint.x, m_OriginalCollisionPoint.y, m_OriginalCollisionPoint.z );
		}
#endif
		return ( Length <= m_OriginalProjectedRadius );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Adds a displacement to the undo manager
// Input  : pDisp - the displacement
//-----------------------------------------------------------------------------
void CSculptTool::AddToUndo( CMapDisp **pDisp )
{
	CMapDisp *pUndoDisp = *pDisp;
	if ( pUndoDisp->Paint_IsDirty() )
		return;

	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( pDispMgr )
	{
		EditDispHandle_t handle = pUndoDisp->GetEditHandle();
		pDispMgr->Undo( handle, false );
		*pDisp = EditDispMgr()->GetDisp( handle );
	}
}


#if 0
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output :
//-----------------------------------------------------------------------------
void CSculptTool::DoPaintEqual( SpatialPaintData_t &spatialData, CMapDisp *pDisp )
{
	Vector vPaintPos, vVert, vFlatVert;
	float flDistance2;

	int nVertCount = pDisp->GetSize();
	for ( int iVert = 0; iVert < nVertCount; iVert++ )
	{
		// Get the current vert.
		pDisp->GetVert( iVert, vVert );

		if ( IsInSphereRadius( spatialData.m_vCenter, spatialData.m_flRadius2, vVert, flDistance2 ) )
		{
			// Get the base vert.
			pDisp->GetFlatVert( iVert, vFlatVert );

			// Build the new position (paint value) and set it.
			DoPaintOne( spatialData, vFlatVert, vPaintPos );
			AddToUndo( &pDisp );
			pDisp->Paint_SetValue( iVert, vPaintPos );
		}
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: this routine does the smoothing operation
// Input  : pView - the 3d view
//			vPoint - the mouse point
//			pDisp - the displacement to smooth
//			pOrigDisp - the displacement prior to the paint operation
// Output :
//-----------------------------------------------------------------------------
void CSculptTool::DoPaintSmooth( CMapView3D *pView, const Vector2D &vPoint, CMapDisp *pDisp, CMapDisp *pOrigDisp )
{
	Vector	vPaintPos, vVert;

	pDisp->GetSurfNormal( m_SpatialData.m_vPaintAxis );

	int nVertCount = pDisp->GetSize();
	for ( int iVert = 0; iVert < nVertCount; iVert++ )
	{
		if ( IsPointInScreenCircle( pView, pDisp, pOrigDisp, iVert, false, true ) )
		{
//			Msg( "Checking Vert %d\n", iVert );
			// Get the current vert.
			pDisp->GetVert( iVert, vVert );

			// Build the new smoothed position and set it.
			if ( DoPaintSmoothOneOverExp( vVert, vPaintPos ) )
			{
				AddToUndo( &pDisp );
				pDisp->Paint_SetValue( iVert, vPaintPos );
//				Msg( "Vert %d Updated: from %g %g %g to %g %g %g\n", iVert, vVert.x, vVert.y, vVert.z, vPaintPos.x, vPaintPos.y, vPaintPos.z );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: checks to see if the paint sphere is within the bounding box
// Input  : vCenter - center of the sphere
//			flRadius - sphere radius
//			vBBoxMin - bounding box mins
//			vBBoxMax - bounding box maxs
// Output : returns two if the two intersect
//-----------------------------------------------------------------------------
bool CSculptTool::PaintSphereDispBBoxOverlap( const Vector &vCenter, float flRadius, const Vector &vBBoxMin, const Vector &vBBoxMax )
{
	return IsBoxIntersectingSphere( vBBoxMin, vBBoxMax, vCenter, flRadius );
}


//-----------------------------------------------------------------------------
// Purpose: checkes to see if the two spheres intersect
// Input  : vCenter - center of the sphere
//			flRadius2 - sphere radius squared
//			vPos - point to test
//			flDistance2 - radius of point
// Output : returns true if the two spheres intersect
//-----------------------------------------------------------------------------
bool CSculptTool::IsInSphereRadius( const Vector &vCenter, float flRadius2, const Vector &vPos, float &flDistance2 )
{
	Vector vTmp;
	VectorSubtract( vPos, vCenter, vTmp );
	flDistance2 = ( vTmp.x * vTmp.x ) + ( vTmp.y * vTmp.y ) + ( vTmp.z * vTmp.z );
	return ( flDistance2 < flRadius2 );
}


//-----------------------------------------------------------------------------
// Purpose: calculates the smoothing radius squared
// Input  : vPoint - the point to be smoothed
// Output : returns the smoothing radius squared
//-----------------------------------------------------------------------------
float CSculptTool::CalcSmoothRadius2( const Vector &vPoint )
{
	Vector vTmp;
	VectorSubtract( m_SpatialData.m_vCenter, vPoint, vTmp );
	float flDistance2 = ( vTmp.x * vTmp.x ) + ( vTmp.y * vTmp.y ) + ( vTmp.z * vTmp.z );

	float flRatio = flDistance2 / m_SpatialData.m_flRadius2;
	flRatio = 1.0f - flRatio;

	float flRadius = flRatio * m_SpatialData.m_flRadius;
	return ( flRadius * flRadius );
}


//-----------------------------------------------------------------------------
// Purpose: smooths all displacements 
// Input  : vNewCenter - calculate the smoothing center
// Output : returns true if successful
//			vPaintPos - the new smoothing position
//-----------------------------------------------------------------------------
bool CSculptTool::DoPaintSmoothOneOverExp( const Vector &vNewCenter, Vector &vPaintPos )
{
	// Get the displacement manager from the active map document.
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return false;

	// Calculate the smoothing radius.
	float flNewRadius2 = CalcSmoothRadius2( vNewCenter );
	flNewRadius2 *= 2.0f;
	float flNewRadius = ( float )sqrt( flNewRadius2 );


	// Test all selected surfaces for smoothing.
	float flWeight = 0.0f;
	float flSmoothDist = 0.0f;

	// Calculate the plane dist.
	float flPaintDist = m_SpatialData.m_vPaintAxis.Dot( vNewCenter );

	int nDispCount = pDispMgr->SelectCount();
	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp )
		{
			// Test paint sphere displacement bbox for overlap.
			Vector vBBoxMin, vBBoxMax;
			pDisp->GetBoundingBox( vBBoxMin, vBBoxMax );
			if ( PaintSphereDispBBoxOverlap( vNewCenter, flNewRadius, vBBoxMin, vBBoxMax ) )
			{
				Vector vVert;
				int nVertCount = pDisp->GetSize();
				for ( int iVert = 0; iVert < nVertCount; iVert++ )
				{
					// Get the current vert.
					pDisp->GetVert( iVert, vVert );

					float flDistance2 = 0.0f;
					if ( IsInSphereRadius( vNewCenter, flNewRadius2, vVert, flDistance2 ) )
					{
						float flRatio = flDistance2 / flNewRadius2;
						float flFactor = 1.0f / exp( flRatio );
						if ( flFactor != 1.0f )
						{
							flFactor *= 1.0f / ( m_SpatialData.m_flScalar * 2.0f );
						}

						Vector vProjectVert;
						float flProjectDist = DotProduct( vVert, m_SpatialData.m_vPaintAxis ) - flPaintDist;
						flSmoothDist += ( flProjectDist * flFactor );
						flWeight += flFactor;
//						Msg( "Factoring %d: %g %g %g at %g\n", iVert, vVert.x, vVert.y, vVert.z, flNewRadius2 );
					}
				}
			}
		}
	}

	if ( flWeight == 0.0f )
	{
		return false;
	}

	// Re-normalize the smoothing position.
	flSmoothDist /= flWeight;
	vPaintPos = vNewCenter + ( m_SpatialData.m_vPaintAxis * flSmoothDist );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: gets the starting position when the paint operation begins
// Input  : pView - the 3d view
//			vPoint - the mouse point
// Output : returns the starting position
//-----------------------------------------------------------------------------
bool CSculptTool::GetStartingSpot( CMapView3D *pView, const Vector2D &vPoint )
{
	m_ValidPaintingSpot = FindCollisionIntercept( pView->GetCamera(), vPoint, false, m_StartingCollisionPoint, m_StartingCollisionNormal, m_StartingCollisionIntercept );

	if ( m_ValidPaintingSpot )
	{
		Vector2D	RadiusPoint = vPoint;
		Vector		vecStart, vecEnd;

		RadiusPoint.x += m_BrushSize;
		pView->GetCamera()->BuildRay( RadiusPoint, vecStart, vecEnd );
		m_StartingProjectedRadius = CalcDistanceToLine( m_StartingCollisionPoint, vecStart, vecEnd );

	}

	return m_ValidPaintingSpot;
}


//-----------------------------------------------------------------------------
// Purpose: Draws a 2d line to represent the direction
// Input  : pRender - the renderer
//			Direction - direction / normal
//			Towards - the color to be used if the direction is towards the viewer
//			Away - the color to be used if the direction is away from the view
//-----------------------------------------------------------------------------
void CSculptTool::DrawDirection( CRender3D *pRender, Vector Direction, Color Towards, Color Away )
{
	Vector		ViewPoint, ViewDir;
	Vector2D	ViewVert;

	VMatrix  Matrix;
	pRender->GetCamera()->GetViewProjMatrix( Matrix );
	Matrix.SetTranslation( Vector( 0.0f, 0.0f, 0.0f ) );
	Vector3DMultiply( Matrix, Direction, ViewDir );
	VectorNormalize( ViewDir );

	ViewVert = m_MousePoint + ( Vector2D( ViewDir.x, -ViewDir.y ) * m_BrushSize );

	if ( ViewDir.z > 0.0f )
	{
		pRender->SetDrawColor( Away.r(), Away.g(), Away.b() );
	}
	else
	{
		pRender->SetDrawColor( Towards.r(), Towards.g(), Towards.b() );
	}

	bool bPopMode = pRender->BeginClientSpace();
	pRender->DrawLine( Vector( m_MousePoint.x, m_MousePoint.y, 0.0f ), Vector( ViewVert.x, ViewVert.y, 0.0f ) );
	if ( bPopMode )
	{
		pRender->EndClientSpace();
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will copy all the selected displacements
//-----------------------------------------------------------------------------
void CSculptTool::DuplicateSelectedDisp( )
{
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
	{
		return;
	}

	FOR_EACH_MAP( m_OrigMapDisp, pos )
	{
		delete m_OrigMapDisp.Element( pos );
	}
	m_OrigMapDisp.Purge();

	int nDispCount = pDispMgr->SelectCount();

	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp )
		{
			CMapDisp *pCopy = new CMapDisp();

			pCopy->CopyFrom( pDisp, false );
			m_OrigMapDisp.Insert( pDisp->GetEditHandle(), pCopy );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will initialize all selected displacements for updating
//-----------------------------------------------------------------------------
void CSculptTool::PrepareDispForPainting( )
{
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
	{
		return;
	}

	int nDispCount = pDispMgr->SelectCount();

	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp )
		{
			pDisp->Paint_Init( DISPPAINT_CHANNEL_POSITION );
		}
	}

}


//-----------------------------------------------------------------------------
// Purpose: this function will find the collision location within the selected displacements
// Input  : pCamera - the camera
//			vPoint - the 2d point on screen
//			bUseOrigPosition - should we use the original displacements prior to updating
// Output : returns true if the point intercepted one of the selected displacements
//			vCollisionPoint the 3d interception point
//			vCollisionNormal - the normal of the tri hit
//			flCollisionIntercept - the intercept
//-----------------------------------------------------------------------------
bool CSculptTool::FindCollisionIntercept( CCamera *pCamera, const Vector2D &vPoint, bool bUseOrigPosition, Vector &vCollisionPoint, Vector &vCollisionNormal, float &flCollisionIntercept )
{
	Vector	vecStart, vecEnd;
	float	flFraction, flLeastFraction;

	flLeastFraction = -1.0f;

	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
	{
		return false;
	}

	int nDispCount = pDispMgr->SelectCount();
	pCamera->BuildRay( vPoint, vecStart, vecEnd );

	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp )
		{
			if ( bUseOrigPosition )
			{
				CMapDisp	*OrigDisp = NULL;
				int			index = m_OrigMapDisp.Find( pDisp->GetEditHandle() );

				if ( index != m_OrigMapDisp.InvalidIndex() )
				{
					OrigDisp = m_OrigMapDisp[ index ];
				}

				if ( OrigDisp )
				{
					pDisp = OrigDisp;
				}
			}

			int iTri = pDisp->CollideWithDispTri( vecStart, vecEnd, flFraction, false );
			if ( iTri != -1 && ( flLeastFraction == -1.0f || flFraction < flLeastFraction ) )
			{
				flLeastFraction = flFraction;
				vCollisionPoint = vecStart + ( ( vecEnd - vecStart ) * flFraction );

				unsigned short v1, v2, v3;
				Vector vec1, vec2, vec3;

				pDisp->GetTriIndices( iTri, v1, v2, v3 );
				pDisp->GetVert( v1, vec1 );
				pDisp->GetVert( v2, vec2 );
				pDisp->GetVert( v3, vec3 );

				ComputeTrianglePlane( vec1, vec2, vec3, vCollisionNormal, flCollisionIntercept );
			}
		}
	}

	return ( flLeastFraction != -1.0f );
}






//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CSculptPainter::CSculptPainter() :
	CSculptTool()
{
	m_InSizingMode = m_InPaintingMode = false;
	m_OrigBrushSize = m_BrushSize;
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CSculptPainter::~CSculptPainter( )
{

}


//-----------------------------------------------------------------------------
// Purpose: setup for starting to paint on the displacement
// Input  : pView - the 3d view
//			vPoint - the initial click point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptPainter::BeginPaint( CMapView3D *pView, const Vector2D &vPoint )
{
	CSculptTool::BeginPaint( pView, vPoint );

	PrepareDispForPainting();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: main routine called when mouse move has happened to start painting
// Input  : pView - the 3d view
//			vPoint - the mouse point
//			SpatialData - the spatial data ( mostly ignored )
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptPainter::Paint( CMapView3D *pView, const Vector2D &vPoint, SpatialPaintData_t &SpatialData )
{
	__super::Paint( pView, vPoint, SpatialData );

	if ( m_bRMBDown )
	{
		if ( !m_bAltDown )
		{
			DoSizing( vPoint );
		}
	}
	else if ( m_bLMBDown )
	{
		if ( !m_ValidPaintingSpot )
		{
			if ( !GetStartingSpot( pView, vPoint ) )
			{
				return false;
			}
		}

		// Setup painting.
		if ( !PrePaint( pView, vPoint ) )
		{
			return false;
		}

		// Handle painting.
		if ( !DoPaint( pView, vPoint ) )
		{
			return false;
		}

		// Finish painting.
		if ( !PostPaint( m_PaintOwner->GetAutoSew() ) )
		{
			return false;
		}
	}

	// Successful paint operation.
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the left mouse button up in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptPainter::OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	CSculptTool::OnLMouseUp3D( pView, nFlags, vPoint );

	m_InPaintingMode = false;

	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( pDispMgr )
	{
		pDispMgr->PostUndo();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the left mouse button down in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptPainter::OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	CSculptTool::OnLMouseDown3D( pView, nFlags, vPoint );

	m_InPaintingMode = true;

	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( pDispMgr )
	{
		pDispMgr->PreUndo( "Displacement Modifier" );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the right mouse button up in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptPainter::OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	CSculptTool::OnRMouseUp3D( pView, nFlags, vPoint );

	m_InSizingMode = false;

	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( pDispMgr )
	{
		pDispMgr->PostUndo();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the right mouse button down in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptPainter::OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	CSculptTool::OnRMouseDown3D( pView, nFlags, vPoint );

	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( pDispMgr )
	{
		pDispMgr->PreUndo( "Displacement Modifier" );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the mouse move in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptPainter::OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	return CSculptTool::OnMouseMove3D( pView, nFlags, vPoint );
}


//-----------------------------------------------------------------------------
// Purpose: toggles the sizing mode
// Input  : vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptPainter::DoSizing( const Vector2D &vPoint )
{
	if ( !m_InSizingMode )
	{
		m_InSizingMode = true;
		m_StartSizingPoint = vPoint;
		m_OrigBrushSize = m_BrushSize;
	}
	else
	{
		m_BrushSize = m_OrigBrushSize + ( vPoint.x - m_StartSizingPoint.x );
		if ( m_BrushSize < 1.0f )
		{
			m_BrushSize = 1.0f;
		}
	}

	return true;
}








// CSculptPushOptions dialog

IMPLEMENT_DYNAMIC(CSculptPushOptions, CDialog)


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CSculptPushOptions::CSculptPushOptions(CWnd* pParent /*=NULL*/) : 
	CDialog(CSculptPushOptions::IDD, pParent),
	CSculptPainter()
{
	m_OffsetMode = OFFSET_MODE_ABSOLUTE;
	m_NormalMode = NORMAL_MODE_Z;
	m_DensityMode = DENSITY_MODE_ADDITIVE;
	m_OffsetDistance = 10.0f;
	m_OffsetAmount = 1.0f;
	m_SmoothAmount = 0.2f;
	m_Direction = 1.0f;
	m_SelectedNormal.Init( 0.0f, 0.0f, 0.0f );

	m_flFalloffSpot = 0.5f;
	m_flFalloffEndingValue = 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CSculptPushOptions::~CSculptPushOptions()
{
}


//-----------------------------------------------------------------------------
// Purpose: initializes the dialog
// Output : returns true if successful
//-----------------------------------------------------------------------------
BOOL CSculptPushOptions::OnInitDialog( void )
{
	char	temp[ 1024 ];

	CDialog::OnInitDialog();

	m_OffsetModeControl.InsertString( -1, "Adaptive" );
	m_OffsetModeControl.InsertString( -1, "Absolute" );
	m_OffsetModeControl.SetCurSel( m_OffsetMode );

	m_OffsetDistanceControl.EnableWindow( ( m_OffsetMode == OFFSET_MODE_ABSOLUTE ) );
	m_OffsetAmountControl.EnableWindow( ( m_OffsetMode == OFFSET_MODE_ADAPTIVE ) ); 

	sprintf( temp, "%g", m_OffsetDistance );
	m_OffsetDistanceControl.SetWindowText( temp );

	sprintf( temp, "%g%%", m_OffsetAmount * 100.0f );
	m_OffsetAmountControl.SetWindowText( temp );

	sprintf( temp, "%g%%", m_SmoothAmount * 100.0f );
	m_SmoothAmountControl.SetWindowText( temp );

	sprintf( temp, "%g%%", m_flFalloffSpot * 100.0f );
	m_FalloffPositionControl.SetWindowText( temp );

	sprintf( temp, "%g%%", m_flFalloffEndingValue * 100.0f );
	m_FalloffFinalControl.SetWindowText( temp );

	m_NormalModeControl.InsertString( -1, "Brush Center" );
	m_NormalModeControl.InsertString( -1, "Screen" );
	m_NormalModeControl.InsertString( -1, "Screen XY" );
	m_NormalModeControl.InsertString( -1, "X" );
	m_NormalModeControl.InsertString( -1, "Y" );
	m_NormalModeControl.InsertString( -1, "Z" );
	m_NormalModeControl.InsertString( -1, "Selected" );
	m_NormalModeControl.SetCurSel( m_NormalMode );

	m_DensityModeControl.InsertString( -1, "Additive" );
	m_DensityModeControl.InsertString( -1, "Attenuated" );
	m_DensityModeControl.SetCurSel( m_DensityMode );

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: prevent the dialog from closing
//-----------------------------------------------------------------------------
void CSculptPushOptions::OnOK()
{
}


//-----------------------------------------------------------------------------
// Purpose: prevent the dialog from closing
//-----------------------------------------------------------------------------
void CSculptPushOptions::OnCancel()
{
}


//-----------------------------------------------------------------------------
// Purpose: set up the data exchange for the variables
// Input  : pDX - the data exchange object
//-----------------------------------------------------------------------------
void CSculptPushOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_OFFSET_MODE, m_OffsetModeControl);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_OFFSET_DISTANCE, m_OffsetDistanceControl);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_OFFSET_AMOUNT, m_OffsetAmountControl);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_SMOOTH_AMOUNT, m_SmoothAmountControl);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_DENSITY_MODE, m_DensityModeControl);
	DDX_Control(pDX, IDC_IDC_SCULPT_PUSH_OPTION_NORMAL_MODE, m_NormalModeControl);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_FALLOFF_POSITION, m_FalloffPositionControl);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_FALLOFF_FINAL, m_FalloffFinalControl);
}


BEGIN_MESSAGE_MAP(CSculptPushOptions, CDialog)
	ON_CBN_SELCHANGE(IDC_IDC_SCULPT_PUSH_OPTION_NORMAL_MODE, &CSculptPushOptions::OnCbnSelchangeIdcSculptPushOptionNormalMode)
	ON_CBN_SELCHANGE(IDC_SCULPT_PUSH_OPTION_OFFSET_MODE, &CSculptPushOptions::OnCbnSelchangeSculptPushOptionOffsetMode)
	ON_EN_CHANGE(IDC_SCULPT_PUSH_OPTION_OFFSET_DISTANCE, &CSculptPushOptions::OnEnChangeSculptPushOptionOffsetDistance)
	ON_CBN_SELCHANGE(IDC_SCULPT_PUSH_OPTION_DENSITY_MODE, &CSculptPushOptions::OnCbnSelchangeSculptPushOptionDensityMode)
	ON_EN_KILLFOCUS(IDC_SCULPT_PUSH_OPTION_SMOOTH_AMOUNT, &CSculptPushOptions::OnEnKillfocusSculptPushOptionSmoothAmount)
	ON_EN_KILLFOCUS(IDC_SCULPT_PUSH_OPTION_OFFSET_AMOUNT, &CSculptPushOptions::OnEnKillfocusSculptPushOptionOffsetAmount)
	ON_EN_KILLFOCUS(IDC_SCULPT_PUSH_OPTION_FALLOFF_POSITION, &CSculptPushOptions::OnEnKillfocusSculptPushOptionFalloffPosition)
	ON_EN_KILLFOCUS(IDC_SCULPT_PUSH_OPTION_FALLOFF_FINAL, &CSculptPushOptions::OnEnKillfocusSculptPushOptionFalloffFinal)
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: sets the normal mode of the sculpt operation
//-----------------------------------------------------------------------------
void CSculptPushOptions::OnCbnSelchangeIdcSculptPushOptionNormalMode()
{
	m_NormalMode = ( NormalMode )m_NormalModeControl.GetCurSel();
}


//-----------------------------------------------------------------------------
// Purpose: sets the offset mode of the sculpt operation
//-----------------------------------------------------------------------------
void CSculptPushOptions::OnCbnSelchangeSculptPushOptionOffsetMode()
{
	m_OffsetMode = ( OffsetMode )m_OffsetModeControl.GetCurSel();

	m_OffsetDistanceControl.EnableWindow( ( m_OffsetMode == OFFSET_MODE_ABSOLUTE ) );
	m_OffsetAmountControl.EnableWindow( ( m_OffsetMode == OFFSET_MODE_ADAPTIVE ) ); 
}


//-----------------------------------------------------------------------------
// Purpose: setup for starting to paint on the displacement
// Input  : pView - the 3d view
//			vPoint - the initial click point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptPushOptions::BeginPaint( CMapView3D *pView, const Vector2D &vPoint )
{
	__super::BeginPaint( pView, vPoint );

	if ( m_bCtrlDown )
	{
		m_Direction = -1.0f;
	}
	else
	{
		m_Direction = 1.0f;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: draws the tool in the 3d view
// Input  : pRender - the 3d renderer
//-----------------------------------------------------------------------------
void CSculptPushOptions::RenderTool3D( CRender3D *pRender )
{
//	pRender->DrawText( "mouse", m_MousePoint.x, m_MousePoint.y, 0 );
//	Msg( "%g %g\n", m_MousePoint.x, m_MousePoint.y );

	pRender->PushRenderMode( RENDER_MODE_WIREFRAME );

	if ( m_InSizingMode )
	{	// yellow for sizing mode
		pRender->BeginClientSpace();
		pRender->SetDrawColor( 255, 255, 0 );
		pRender->DrawCircle( Vector( m_StartSizingPoint.x, m_StartSizingPoint.y, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), m_BrushSize, 32 );
		if ( m_flFalloffSpot > 0.0f )
		{
			pRender->SetDrawColor( 192, 192, 0 );
			pRender->DrawCircle( Vector( m_StartSizingPoint.x, m_StartSizingPoint.y, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), m_BrushSize * m_flFalloffSpot, 32 );
		}
		pRender->EndClientSpace();
	}
	else if ( m_bShiftDown )
	{	// purple for smoothing
		pRender->SetDrawColor( 255, 0, 255 );
		pRender->BeginClientSpace();
		pRender->DrawCircle( Vector( m_MousePoint.x, m_MousePoint.y, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), m_BrushSize, 32 );
		pRender->EndClientSpace();
	}
	else if ( m_bCtrlDown )
	{	// red for negative sculpting
		pRender->BeginClientSpace();
		pRender->SetDrawColor( 255, 0, 0 );
		pRender->DrawCircle( Vector( m_MousePoint.x, m_MousePoint.y, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), m_BrushSize, 32 );
		if ( m_flFalloffSpot > 0.0f )
		{
			pRender->SetDrawColor( 192, 0, 0 );
			pRender->DrawCircle( Vector( m_MousePoint.x, m_MousePoint.y, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), m_BrushSize * m_flFalloffSpot, 32 );
		}
		pRender->EndClientSpace();

		Vector	vPaintAxis;
		GetPaintAxis( pRender->GetCamera(), m_MousePoint, vPaintAxis );
		DrawDirection( pRender, -vPaintAxis, Color( 255, 255, 255 ), Color( 255, 128, 128 ) );
	}
	else
	{	// green for positive sculpting
		pRender->BeginClientSpace();
		pRender->SetDrawColor( 0, 255, 0 );
		pRender->DrawCircle( Vector( m_MousePoint.x, m_MousePoint.y, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), m_BrushSize, 32 );
		if ( m_flFalloffSpot > 0.0f )
		{
			pRender->SetDrawColor( 0, 192, 0 );
			pRender->DrawCircle( Vector( m_MousePoint.x, m_MousePoint.y, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), m_BrushSize * m_flFalloffSpot, 32 );
		}
		pRender->EndClientSpace();

		Vector	vPaintAxis;
		GetPaintAxis( pRender->GetCamera(), m_MousePoint, vPaintAxis );
		DrawDirection( pRender, vPaintAxis, Color( 255, 255, 255 ), Color( 255, 128, 128 ) );
	}

#if 0
	FindColissionIntercept( pRender->GetCamera(), m_MousePoint, true, m_CurrentCollisionPoint, m_CurrentCollisionNormal, m_CurrentCollisionIntercept );

	Vector2D	RadiusPoint = m_MousePoint;
	Vector		vecStart, vecEnd;

	RadiusPoint.x += m_BrushSize;
	pRender->GetCamera()->BuildRay( RadiusPoint, vecStart, vecEnd );

	m_CurrentProjectedRadius = CalcDistanceToLine( m_CurrentCollisionPoint, vecStart, vecEnd );

	pRender->RenderWireframeSphere( m_CurrentCollisionPoint, m_CurrentProjectedRadius, 12, 12, 0, 255, 255 );
#endif

#if 0

	// Get the displacement manager from the active map document.
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();

	// For each displacement surface is the selection list attempt to paint on it.
	int nDispCount = pDispMgr->SelectCount();
	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp )
		{
			CMapDisp	*OrigDisp = NULL;
			int			index = m_OrigMapDisp.Find( pDisp->GetEditHandle() );

			if ( index != m_OrigMapDisp.InvalidIndex() )
			{
				OrigDisp = m_OrigMapDisp[ index ];
			}
			Vector	vPaintPos, vVert;

			int nVertCount = pDisp->GetSize();
			for ( int iVert = 0; iVert < nVertCount; iVert++ )
			{
				if ( IsPointInScreenCircle( pView, pDisp, pOrigDisp, iVert, false ) )
				{
					// Get the current vert.
					pDisp->GetVert( iVert, vVert );
				}
			}
		}
	}
#endif


	pRender->PopRenderMode();

#if 0
	if ( !FindColissionIntercept( pRender->GetCamera(), m_MousePoint, true, m_CurrentCollisionPoint, m_CurrentCollisionNormal, m_CurrentCollisionIntercept ) )
	{
		return;
	}

	Vector2D	RadiusPoint = m_MousePoint;
	Vector		vecStart, vecEnd;

	RadiusPoint.x += m_BrushSize;
	pRender->GetCamera()->BuildRay( RadiusPoint, vecStart, vecEnd );
	m_CurrentProjectedRadius = CalcDistanceToLine( m_CurrentCollisionPoint, vecStart, vecEnd );

	Msg( "Dist = %g at %g,%g,%g\n", m_CurrentProjectedRadius, m_CurrentCollisionPoint.x, m_CurrentCollisionPoint.y, m_CurrentCollisionPoint.z );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: handles the right mouse button down in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptPushOptions::OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	CSculptTool::OnRMouseDown3D( pView, nFlags, vPoint );

	if ( m_bAltDown )
	{
		m_NormalMode = NORMAL_MODE_Z;
		m_NormalModeControl.SetCurSel( m_NormalMode );

#if 0

		//
		// check for closest solid object
		//
		ULONG		ulFace;
		CMapClass	*pObject;

		if( ( ( pObject = pView->NearestObjectAt( vPoint, ulFace ) ) != NULL ) )
		{
			if( pObject->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) )
			{
				// get the solid
				CMapSolid *pSolid = ( CMapSolid* )pObject;
				if( !pSolid )
				{
					return true;
				}

				// trace a line and get the normal -- will get a displacement normal
				// if one exists
				CMapFace *pFace = pSolid->GetFace( ulFace );
				if( !pFace )
				{
					return true;
				}

				Vector vRayStart, vRayEnd;
				pView->GetCamera()->BuildRay( vPoint, vRayStart, vRayEnd );

				Vector vHitPos, vHitNormal;
				if( pFace->TraceLine( vHitPos, vHitNormal, vRayStart, vRayEnd ) )
				{
					// set the paint direction
					m_SelectedNormal = vHitNormal;

					m_NormalMode = NORMAL_MODE_SELECTED;
					m_NormalModeControl.SetCurSel( m_NormalMode );
				}
			}
		}
#else
		Vector	CollisionPoint, CollisionNormal;
		float	CollisionIntercept;

		if ( FindCollisionIntercept( pView->GetCamera(), vPoint, false, CollisionPoint, CollisionNormal, CollisionIntercept ) )
		{
			// set the paint direction
			m_SelectedNormal = -CollisionNormal;

			m_NormalMode = NORMAL_MODE_SELECTED;
			m_NormalModeControl.SetCurSel( m_NormalMode );
		}
#endif
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: returns the painting direction
// Input  : pCamera - the 3d camera
//			vPoint - the 2d mouse point
// Output : vPaintAxis - the direction the painting should go
//-----------------------------------------------------------------------------
void CSculptPushOptions::GetPaintAxis( CCamera *pCamera, const Vector2D &vPoint, Vector &vPaintAxis )
{
	switch( m_NormalMode )
	{
		case NORMAL_MODE_SCREEN:
			pCamera->GetViewForward( vPaintAxis );
			vPaintAxis = -vPaintAxis;
			break;

		case NORMAL_MODE_SCREEN_XY:
			pCamera->GetViewForward( vPaintAxis );
			vPaintAxis = -vPaintAxis;
			vPaintAxis.z = 0.f;
			break;

		case NORMAL_MODE_BRUSH_CENTER:
			if ( !m_InPaintingMode )
			{
				Vector	CollisionPoint, CollisionNormal;
				float	CollisionIntercept;

				FindCollisionIntercept( pCamera, vPoint, false, CollisionPoint, CollisionNormal, CollisionIntercept );

				vPaintAxis = -CollisionNormal;
			}
			else
			{
				vPaintAxis = -m_StartingCollisionNormal;
			}
			break;

		case NORMAL_MODE_X:
			vPaintAxis.Init( 1.0f, 0.0f, 0.0f );
			break;

		case NORMAL_MODE_Y:
			vPaintAxis.Init( 0.0f, 1.0f, 0.0f );
			break;

		case NORMAL_MODE_Z:
			vPaintAxis.Init( 0.0f, 0.0f, 1.0f );
			break;

		case NORMAL_MODE_SELECTED:
			vPaintAxis = m_SelectedNormal;
			break;

		default:
			vPaintAxis.Init( 0.0f, 0.0f, 1.0f );
	}
}


//-----------------------------------------------------------------------------
// Purpose: applies the specific push operation onto the displacement
// Input  : pView - the 3d view
//			vPoint - the mouse point
//			pDisp - the displacement to apply the push to
//			pOrigDisp - the original displacement prior to any adjustments
//-----------------------------------------------------------------------------
void CSculptPushOptions::DoPaintOperation( CMapView3D *pView, const Vector2D &vPoint, CMapDisp *pDisp, CMapDisp *pOrigDisp )
{
	Vector	vPaintPos, vVert, vDirection;
	float	flMaxDistance = 0.0f;
	float	flDistance;
	float	flLengthPercent;
	Vector	vPaintAxis;

	if ( m_bShiftDown )
	{
//		DoSmoothOperation( pView, vPoint, pDisp, pOrigDisp );
//		m_SpatialData.m_flRadius = 256.0f;
//		m_SpatialData.m_flScalar = 5.0f / m_SmoothAmount;

//		m_SpatialData.m_flRadius = m_StartingProjectedRadius * 1.5f;
		m_SpatialData.m_flRadius = m_CurrentProjectedRadius * 2.0f;
		m_SpatialData.m_flRadius2 = ( m_SpatialData.m_flRadius * m_SpatialData.m_flRadius );
		m_SpatialData.m_flOORadius2 = 1.0f / m_SpatialData.m_flRadius2;
		m_SpatialData.m_flScalar = 10.0f / m_SmoothAmount;
		m_SpatialData.m_vCenter = m_CurrentCollisionPoint;

		DoPaintSmooth( pView, vPoint, pDisp, pOrigDisp );
		return;
	}

	GetPaintAxis( pView->GetCamera(), vPoint, vPaintAxis );

	vDirection = vPaintAxis * m_Direction;

	switch( m_OffsetMode )
	{
		case OFFSET_MODE_ADAPTIVE:
			flMaxDistance = m_StartingProjectedRadius * m_OffsetAmount;
			break;
		case OFFSET_MODE_ABSOLUTE:
			flMaxDistance = m_OffsetDistance;
			break;
	}

	int nVertCount = pDisp->GetSize();
	for ( int iVert = 0; iVert < nVertCount; iVert++ )
	{
		if ( IsPointInScreenCircle( pView, pDisp, pOrigDisp, iVert, true, false, &flLengthPercent ) )
		{
			pDisp->GetVert( iVert, vVert );

			if ( flLengthPercent > m_flFalloffSpot )
			{
				flLengthPercent = ( flLengthPercent - m_flFalloffSpot ) / ( 1.0f - m_flFalloffSpot );
				flLengthPercent = 1.0 - flLengthPercent;
				flDistance = ( ( 1.0f - m_flFalloffEndingValue ) * flLengthPercent * flMaxDistance ) + ( m_flFalloffEndingValue * flMaxDistance );
			}
			else
			{
				flDistance = flMaxDistance;
			}

			if ( flDistance == 0.0f )
			{
				continue;
			}

			switch( m_DensityMode )
			{
				case DENSITY_MODE_ADDITIVE:
					VectorScale( vDirection, flDistance, vPaintPos );
					VectorAdd( vPaintPos, vVert, vPaintPos );
					break;

				case DENSITY_MODE_ATTENUATED:
					VectorScale( vDirection, flDistance, vPaintPos );
					VectorAdd( vPaintPos, vVert, vPaintPos );

					if ( pOrigDisp )
					{
						Vector	vOrigVert, vDiff;
						float	Length;

						pOrigDisp->GetVert( iVert, vOrigVert );
						vDiff = ( vPaintPos - vOrigVert );
						Length = vDiff.Length() / flMaxDistance;
						if ( Length > 1.0f )
						{
							Length = 1.0f;
						}

						vPaintPos = vOrigVert + ( Length * vDirection * flMaxDistance );
					}
					break;
			}

			AddToUndo( &pDisp );
			pDisp->Paint_SetValue( iVert, vPaintPos );
		}
	}
}


#if 0

typedef enum
{
	DISP_DIR_LEFT_TO_RIGHT = 0,		// adjoining displacement is to the left
	DISP_DIR_TOP_TO_BOTTOM = 1,		// adjoining displacement is to the top
	DISP_DIR_RIGHT_TO_LEFT = 2,		// adjoining displacement is to the right
	DISP_DIR_BOTTOM_TO_TOP = 3,		// adjoining displacement is to the bottom
} DispDirections;

typedef enum
{
	MOVE_DIR_RIGHT = 0,
	MOVE_DIR_UP,
	MOVE_DIR_LEFT,
	MOVE_DIR_DOWN,

	MOVE_DIR_MAX
} MoveDirections;


class CDispGrid
{
public:
	CDispGrid( CMapDisp	*pDisp, bool DoPopulate = false, int GridExpand = 2 );
	~CDispGrid( );

	void Populate( CMapDisp	*pDisp );
	bool GetPosition( int x, int y, int OffsetX, int OffsetY, Vector &Position );
	bool GetFlatPosition( int x, int y, int OffsetX, int OffsetY, Vector &FlatPosition );

	void SetPosition( int x, int y, Vector &NewPosition );
	void UpdatePositions( void );

	void CalcSpringForce( int x, int y, int OffsetX, int OffsetY, float Ks, Vector &SpringForce );


private:
	typedef struct SDispPoint
	{
		bool	m_IsSet;
		int		m_DispPos;
		Vector	m_Position, m_UpdatePosition;
		Vector	m_FlatPosition;
	} TDispPoint;

	int			m_Width, m_Height;
	int			m_GridWidth, m_GridHeight;
	int			m_GridExpand;
	TDispPoint	*m_Grid;

	void PopulateUp( CMapDisp *pDisp );
	void PopulateDown( CMapDisp *pDisp );
	void PopulateRight( CMapDisp *pDisp );
	void PopulateLeft( CMapDisp *pDisp );
};

CDispGrid::CDispGrid( CMapDisp *pDisp, bool DoPopulate, int GridExpand )
{
	m_GridExpand = GridExpand;
	m_Width = pDisp->GetWidth();
	m_Height = pDisp->GetHeight();
	m_GridWidth = m_Width + ( GridExpand * 2 );
	m_GridHeight = m_Height + ( GridExpand * 2 );

	m_Grid = new TDispPoint[ m_GridWidth * m_GridHeight ];
	for( int i = 0; i < m_GridWidth * m_GridHeight; i++ )
	{
		m_Grid[ i ].m_IsSet = false;
	}

	if ( DoPopulate )
	{
		Populate( pDisp );
	}
}

CDispGrid::~CDispGrid( )
{
	delete [] m_Grid;
}

void CDispGrid::PopulateUp( CMapDisp *pDisp )
{
	EditDispHandle_t handle;
	int orient;

	pDisp->GetEdgeNeighbor( DISP_DIR_TOP_TO_BOTTOM, handle, orient );
	if ( handle == EDITDISPHANDLE_INVALID )
	{
		return;
	}
	pDisp = EditDispMgr()->GetDisp( handle );

	if ( pDisp->GetWidth() != m_Width || pDisp->GetHeight() != m_Height )
	{	// don't support ones which aren't of the same subdivision
		return;
	}

	if ( orient != MOVE_DIR_DOWN )
	{	// don't support rotation for now
		return;
	}


	for( int x = 0; x < m_Width; x++ )
	{
		for( int y = 0; y < m_GridExpand; y++ )
		{
			int GridPos = ( ( m_GridHeight - y - 1 ) * m_GridWidth ) + ( x + m_GridExpand );

			m_Grid[ GridPos ].m_DispPos = ( ( m_GridExpand - y ) * m_Width ) + x;		// don't do inner row, as that is sewed
			pDisp->GetVert( m_Grid[ GridPos ].m_DispPos, m_Grid[ GridPos ].m_Position );
			m_Grid[ GridPos ].m_UpdatePosition = m_Grid[ GridPos ].m_Position;
			pDisp->GetFlatVert( m_Grid[ GridPos ].m_DispPos, m_Grid[ GridPos ].m_FlatPosition );
			m_Grid[ GridPos ].m_IsSet = true;
		}
	}
}

void CDispGrid::PopulateDown( CMapDisp *pDisp )
{
	EditDispHandle_t handle;
	int orient;

	pDisp->GetEdgeNeighbor( DISP_DIR_BOTTOM_TO_TOP, handle, orient );
	if ( handle == EDITDISPHANDLE_INVALID )
	{
		return;
	}
	pDisp = EditDispMgr()->GetDisp( handle );

	if ( pDisp->GetWidth() != m_Width || pDisp->GetHeight() != m_Height )
	{	// don't support ones which aren't of the same subdivision
		return;
	}

	if ( orient != MOVE_DIR_UP )
	{	// don't support rotation for now
		return;
	}


	for( int x = 0; x < m_Width; x++ )
	{
		for( int y = 0; y < m_GridExpand; y++ )
		{
			int GridPos = ( ( y ) * m_GridWidth ) + ( x + m_GridExpand );

			m_Grid[ GridPos ].m_DispPos = ( ( m_Height - m_GridExpand + y - 1 ) * m_Width ) + x;		// don't do inner row, as that is sewed
			pDisp->GetVert( m_Grid[ GridPos ].m_DispPos, m_Grid[ GridPos ].m_Position );
			m_Grid[ GridPos ].m_UpdatePosition = m_Grid[ GridPos ].m_Position;
			pDisp->GetFlatVert( m_Grid[ GridPos ].m_DispPos, m_Grid[ GridPos ].m_FlatPosition );
			m_Grid[ GridPos ].m_IsSet = true;
		}
	}
}

void CDispGrid::PopulateRight( CMapDisp *pDisp )
{
	EditDispHandle_t handle;
	int orient;

	pDisp->GetEdgeNeighbor( DISP_DIR_RIGHT_TO_LEFT, handle, orient );
	if ( handle == EDITDISPHANDLE_INVALID )
	{
		return;
	}
	pDisp = EditDispMgr()->GetDisp( handle );

	if ( pDisp->GetWidth() != m_Width || pDisp->GetHeight() != m_Height )
	{	// don't support ones which aren't of the same subdivision
		return;
	}

	if ( orient != MOVE_DIR_RIGHT )
	{	// don't support rotation for now
		return;
	}


	for( int x = 0; x < m_GridExpand; x++ )
	{
		for( int y = 0; y < m_Height; y++ )
		{
			int GridPos = ( ( y + m_GridExpand ) * m_GridWidth ) + ( x + m_GridExpand + m_Width );

			m_Grid[ GridPos ].m_DispPos = ( ( y ) * m_Width ) + x + 1;		// don't do inner row, as that is sewed
			pDisp->GetVert( m_Grid[ GridPos ].m_DispPos, m_Grid[ GridPos ].m_Position );
			m_Grid[ GridPos ].m_UpdatePosition = m_Grid[ GridPos ].m_Position;
			pDisp->GetFlatVert( m_Grid[ GridPos ].m_DispPos, m_Grid[ GridPos ].m_FlatPosition );
			m_Grid[ GridPos ].m_IsSet = true;
		}
	}
}

void CDispGrid::PopulateLeft( CMapDisp *pDisp )
{
	EditDispHandle_t handle;
	int orient;

	pDisp->GetEdgeNeighbor( DISP_DIR_LEFT_TO_RIGHT, handle, orient );
	if ( handle == EDITDISPHANDLE_INVALID )
	{
		return;
	}
	pDisp = EditDispMgr()->GetDisp( handle );

	if ( pDisp->GetWidth() != m_Width || pDisp->GetHeight() != m_Height )
	{	// don't support ones which aren't of the same subdivision
		return;
	}

	if ( orient != MOVE_DIR_LEFT )
	{	// don't support rotation for now
		return;
	}


	for( int x = 0; x < m_GridExpand; x++ )
	{
		for( int y = 0; y < m_Height; y++ )
		{
			int GridPos = ( ( y + m_GridExpand ) * m_GridWidth ) + ( x );

			m_Grid[ GridPos ].m_DispPos = ( ( y ) * m_Width ) + ( m_Width - m_GridExpand + x - 1 );		// don't do inner row, as that is sewed
			pDisp->GetVert( m_Grid[ GridPos ].m_DispPos, m_Grid[ GridPos ].m_Position );
			m_Grid[ GridPos ].m_UpdatePosition = m_Grid[ GridPos ].m_Position;
			pDisp->GetFlatVert( m_Grid[ GridPos ].m_DispPos, m_Grid[ GridPos ].m_FlatPosition );
			m_Grid[ GridPos ].m_IsSet = true;
		}
	}
}

void CDispGrid::Populate( CMapDisp *pDisp )
{
	for( int x = 0; x < m_Width; x++ )
	{
		for( int y = 0; y < m_Height; y++ )
		{
			int GridPos = ( ( y + m_GridExpand ) * m_GridWidth ) + ( x + m_GridExpand );

			m_Grid[ GridPos ].m_DispPos = ( y * m_Width ) + x;
			pDisp->GetVert( m_Grid[ GridPos ].m_DispPos, m_Grid[ GridPos ].m_Position );
			m_Grid[ GridPos ].m_UpdatePosition = m_Grid[ GridPos ].m_Position;
			pDisp->GetFlatVert( m_Grid[ GridPos ].m_DispPos, m_Grid[ GridPos ].m_FlatPosition );
			m_Grid[ GridPos ].m_IsSet = true;
		}
	}

	PopulateUp( pDisp );
	PopulateDown( pDisp );
	PopulateRight( pDisp );
	PopulateLeft( pDisp );
}

bool CDispGrid::GetPosition( int x, int y, int OffsetX, int OffsetY, Vector &Position )
{
	x += OffsetX;
	y += OffsetY;

	int GridPos = ( ( y + m_GridExpand ) * m_GridWidth ) + ( x + m_GridExpand );

	if ( !m_Grid[ GridPos ].m_IsSet )
	{
		return false;
	}

	Position = m_Grid[ GridPos ].m_Position;

	return true;
}

bool CDispGrid::GetFlatPosition( int x, int y, int OffsetX, int OffsetY, Vector &FlatPosition )
{
	x += OffsetX;
	y += OffsetY;

	int GridPos = ( ( y + m_GridExpand ) * m_GridWidth ) + ( x + m_GridExpand );

	if ( !m_Grid[ GridPos ].m_IsSet )
	{
		return false;
	}

	FlatPosition = m_Grid[ GridPos ].m_FlatPosition;

	return true;
}

void CDispGrid::SetPosition( int x, int y, Vector &NewPosition )
{
	int GridPos = ( ( y + m_GridExpand ) * m_GridWidth ) + ( x + m_GridExpand );

	if ( !m_Grid[ GridPos ].m_IsSet )
	{
		return;
	}

	m_Grid[ GridPos ].m_UpdatePosition = NewPosition;
}

void CDispGrid::UpdatePositions( void )
{
	for( int i = 0; i < m_GridWidth * m_GridHeight; i++ )
	{
		m_Grid[ i ].m_Position = m_Grid[ i ].m_UpdatePosition ;
	}
}

void CDispGrid::CalcSpringForce( int x, int y, int OffsetX, int OffsetY, float Ks, Vector &SpringForce )
{
	Vector	currentP1, currentP2;
	Vector	restP1, restP2;
	Vector	currentDelta, restDelta;
	float	currentDistance, restDistance;

	SpringForce.Init();

	if ( !GetPosition( x, y, 0, 0, currentP1 ) )
	{
		return;
	}
	if ( !GetPosition( x, y, OffsetX, OffsetY, currentP2 ) )
	{
		return;
	}
	if ( !GetFlatPosition( x, y, 0, 0, restP1 ) )
	{
		return;
	}
	if ( !GetFlatPosition( x, y, OffsetX, OffsetY, restP2 ) )
	{
		return;
	}

	currentDelta = currentP1 - currentP2;
	currentDistance = currentDelta.Length();

	if ( currentDistance == 0.0f )
	{
		return;
	}

	restDelta = restP1 - restP2;
	restDistance = restDelta.Length();

	float Hterm = (currentDistance - restDistance) * Ks;

	// VectorDifference(&p1->v,&p2->v,&deltaV);		// Delta Velocity Vector
	// Dterm = (DotProduct(&deltaV,&deltaP) * spring->Kd) / dist; // Damping Term
	float	Dterm = 0.0f;


	SpringForce = currentDelta * ( 1.0f / currentDistance );
	SpringForce = SpringForce * -(Hterm + Dterm);


	//VectorSum(&p1->f,&springForce,&p1->f);			// Apply to Particle 1
	//VectorDifference(&p2->f,&springForce,&p2->f);	// - Force on Particle 2
}


void CSculptPushOptions::DoSmoothOperation( CMapView3D *pView, const Vector2D &vPoint, CMapDisp *pDisp, CMapDisp *pOrigDisp )
{
	Vector	SpringForce;
	int width = pDisp->GetWidth();
	int height = pDisp->GetHeight();
	Vector	*Forces = ( Vector * )_alloca( sizeof( *Forces ) * width * height );
	bool	*DoCalc = ( bool * )_alloca( sizeof( *DoCalc ) * width * height );

	const float SPRING_CONSTANT	= 0.02f;
	const float SPRING_CONSTANT_TO_NORMAL = 0.4f;

	Vector	SurfaceNormal;

	pDisp->GetSurfNormal( SurfaceNormal );


	for( int x = 0; x < width; x++ )
	{
		for( int y = 0; y < height; y++ )
		{
			int		pVert = ( x * width ) + y;
			Vector	pos, vTestVert;

			pDisp->GetVert( pVert, pos );

			if ( pOrigDisp && 0 )
			{
				pOrigDisp->GetVert( pVert, vTestVert );
			}
			else
			{
				vTestVert = pos;
			}

			Vector2D ViewVert;
			pView->GetCamera()->WorldToView( vTestVert, ViewVert );

			Vector2D	Offset = ViewVert - m_MousePoint;
			float		Length = Offset.Length();
			if ( Length <= m_BrushSize || 0 )
			{
				DoCalc[ pVert ] = true;
			}
			else
			{
				DoCalc[ pVert ] = false;
			}
		}
	}

#if 0
	EditDispHandle_t handle;
	int orient;
	for( int i = 0; i < 4; i++ )
	{
		pDisp->GetEdgeNeighbor( i, handle, orient );
		if ( handle != EDITDISPHANDLE_INVALID )
		{
			Msg( "Handle at %d orient %d\n", i, orient );
		}
	}

	int x = 0;
	int y = 0;
	CMapDisp *pNextDisp = pDisp;
	Vector Vert;
	Vector FlatVert;

	while( 1 )
	{
		if ( !GetAdjoiningPoint( x, y, MOVE_DIR_UP, 1, pNextDisp, Vert, FlatVert ) || pDisp != pNextDisp )
		{
			break;
		}

		y++;
	}

	return;
#endif

	CDispGrid	DispGrid( pDisp, true );

	const float	StepAmount = 1.0f;

	float	CurrentSmooth = m_SmoothAmount;
	while( CurrentSmooth > 0.0f )
	{
		float SpringAmount;
		float SpringToNormalAmount;
		if ( CurrentSmooth > StepAmount )
		{
			SpringAmount = SPRING_CONSTANT * StepAmount;
			SpringToNormalAmount = SPRING_CONSTANT_TO_NORMAL * StepAmount;
		}
		else
		{
			SpringAmount = SPRING_CONSTANT * CurrentSmooth;
			SpringToNormalAmount = SPRING_CONSTANT_TO_NORMAL * CurrentSmooth;
		}
		CurrentSmooth -= StepAmount;

		for( int x = 0; x < width; x++ )
		{
			for( int y = 0; y < height; y++ )
			{
				int pVert = ( y * width ) + x;

				if ( !DoCalc[ pVert ] )
				{
					continue;
				}

				Forces[ pVert ].Init();

				// structural springs
				DispGrid.CalcSpringForce( x, y, 1, 0, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;

				DispGrid.CalcSpringForce( x, y, -1, 0, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;

				DispGrid.CalcSpringForce( x, y, 0, 1, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;

				DispGrid.CalcSpringForce( x, y, 0, -1, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;


				// shear springs
				DispGrid.CalcSpringForce( x, y, 1, 1, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;

				DispGrid.CalcSpringForce( x, y, -1, 1, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;

				DispGrid.CalcSpringForce( x, y, 1, -1, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;

				DispGrid.CalcSpringForce( x, y, -1, -1, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;

				// bend springs
				DispGrid.CalcSpringForce( x, y, 2, 0, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;

				DispGrid.CalcSpringForce( x, y, -2, 0, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;

				DispGrid.CalcSpringForce( x, y, 0, 2, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;

				DispGrid.CalcSpringForce( x, y, 0, -2, SpringAmount, SpringForce );
				Forces[ pVert ] += SpringForce;

				Vector	Vert, FlatVert, FlatVertExtended, ClosestPoint;

				DispGrid.GetPosition( x, y, 0, 0, Vert );
				DispGrid.GetFlatPosition( x, y, 0, 0, FlatVert );

				FlatVertExtended = FlatVert + ( SurfaceNormal * 10.0f );
				CalcClosestPointOnLine( Vert, FlatVert, FlatVertExtended, ClosestPoint );
				Vector	Difference = ( Vert - ClosestPoint );
				float Distance = Difference.Length();

				if ( Distance > 0.0f )
				{
					float Hterm = Distance * SpringToNormalAmount;
					float	Dterm = 0.0f;

					SpringForce = ( Difference ) * ( 1.0f / Distance );
					SpringForce = SpringForce * -(Hterm + Dterm);
					Forces[ pVert ] += SpringForce;
				}

				Vector	pos;

				DispGrid.GetPosition( x, y, 0, 0, pos );
				pos += Forces[ pVert ];

				AddToUndo( &pDisp );
				pDisp->Paint_SetValue( pVert, pos );

				DispGrid.SetPosition( x, y, pos );
			}
		}
		DispGrid.UpdatePositions();
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: sets the offset distance
//-----------------------------------------------------------------------------
void CSculptPushOptions::OnEnChangeSculptPushOptionOffsetDistance()
{
	char	temp[ 1024 ];

	m_OffsetDistanceControl.GetWindowText( temp, sizeof( temp ) );
	m_OffsetDistance = atof( temp );
}


//-----------------------------------------------------------------------------
// Purpose: sets the density mode
//-----------------------------------------------------------------------------
void CSculptPushOptions::OnCbnSelchangeSculptPushOptionDensityMode()
{
	m_DensityMode = ( DensityMode )m_DensityModeControl.GetCurSel();
}


//-----------------------------------------------------------------------------
// Purpose: sets the smooth amount
//-----------------------------------------------------------------------------
void CSculptPushOptions::OnEnKillfocusSculptPushOptionSmoothAmount()
{
	char	temp[ 1024 ], t2[ 1024 ];

	m_SmoothAmountControl.GetWindowText( temp, sizeof( temp ) );
	sscanf( temp, "%f%%", &m_SmoothAmount );
	m_SmoothAmount /= 100.0f;

	if ( m_SmoothAmount <= 0.0f )
	{
		m_SmoothAmount = 0.2f;
	}

	sprintf( t2, "%g%%", m_SmoothAmount * 100.0f );

	if ( strcmpi( temp, t2 ) != 0 )
	{
		m_SmoothAmountControl.SetWindowText( t2 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: sets the offset amount
//-----------------------------------------------------------------------------
void CSculptPushOptions::OnEnKillfocusSculptPushOptionOffsetAmount()
{
	char	temp[ 1024 ], t2[ 1024 ];

	m_OffsetAmountControl.GetWindowText( temp, sizeof( temp ) );
	sscanf( temp, "%f%%", &m_OffsetAmount );
	m_OffsetAmount /= 100.0f;

	if ( m_OffsetAmount <= 0.0f )
	{
		m_OffsetAmount = 1.0f;
	}

	sprintf( t2, "%g%%", m_OffsetAmount * 100.0f );

	if ( strcmpi( temp, t2 ) != 0 )
	{
		m_OffsetAmountControl.SetWindowText( t2 );
	}
}

void CSculptPushOptions::OnEnKillfocusSculptPushOptionFalloffPosition()
{
	char	temp[ 1024 ], t2[ 1024 ];

	m_FalloffPositionControl.GetWindowText( temp, sizeof( temp ) );
	sscanf( temp, "%f%%", &m_flFalloffSpot );
	m_flFalloffSpot /= 100.0f;

	if ( m_flFalloffSpot <= 0.0f )
	{
		m_flFalloffSpot = 0.0f;
	}

	if ( m_flFalloffSpot > 1.0f )
	{
		m_flFalloffSpot = 1.0f;
	}

	sprintf( t2, "%g%%", m_flFalloffSpot * 100.0f );

	if ( strcmpi( temp, t2 ) != 0 )
	{
		m_FalloffPositionControl.SetWindowText( t2 );
	}
}

void CSculptPushOptions::OnEnKillfocusSculptPushOptionFalloffFinal()
{
	char	temp[ 1024 ], t2[ 1024 ];

	m_FalloffFinalControl.GetWindowText( temp, sizeof( temp ) );
	sscanf( temp, "%f%%", &m_flFalloffEndingValue);
	m_flFalloffEndingValue /= 100.0f;

	if ( m_flFalloffEndingValue <= 0.0f )
	{
		m_flFalloffEndingValue = 0.0f;
	}

	if ( m_flFalloffEndingValue > 1.0f )
	{
		m_flFalloffEndingValue = 1.0f;
	}

	sprintf( t2, "%g%%", m_flFalloffEndingValue * 100.0f );

	if ( strcmpi( temp, t2 ) != 0 )
	{
		m_FalloffFinalControl.SetWindowText( t2 );
	}
}






// CSculptCarveOptions dialog

IMPLEMENT_DYNAMIC(CSculptCarveOptions, CDialog)


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CSculptCarveOptions::CSculptCarveOptions(CWnd* pParent /*=NULL*/) : 
	CDialog(CSculptCarveOptions::IDD, pParent),
	CSculptPainter()
{
	m_OffsetMode = OFFSET_MODE_ABSOLUTE;
	m_NormalMode = NORMAL_MODE_Z;
	m_DensityMode = DENSITY_MODE_ADDITIVE;
	m_OffsetDistance = 10.0f;
	m_OffsetAmount = 1.0f;
	m_SmoothAmount = 0.2f;
	m_Direction = 1.0f;
	m_SelectedNormal.Init( 0.0f, 0.0f, 0.0f );
	m_BrushLocation = -1;
	m_StartLine.Init( -1.0f, -1.0f );
	m_EndLine.Init( -1.0f, -1.0f );

	for( int i = 0; i < MAX_SCULPT_SIZE; i++ )
	{
		m_BrushPoints[ i ] = ( i / ( float )MAX_SCULPT_SIZE ); // 0.0f;
	}
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CSculptCarveOptions::~CSculptCarveOptions()
{
}


//-----------------------------------------------------------------------------
// Purpose: initializes the dialog
// Output : returns true if successful
//-----------------------------------------------------------------------------
BOOL CSculptCarveOptions::OnInitDialog( )
{
	char	temp[ 1024 ];

	CDialog::OnInitDialog();

	m_OffsetModeControl.InsertString( -1, "Adaptive" );
	m_OffsetModeControl.InsertString( -1, "Absolute" );
	m_OffsetModeControl.SetCurSel( m_OffsetMode );

	m_OffsetDistanceControl.EnableWindow( ( m_OffsetMode == OFFSET_MODE_ABSOLUTE ) );
	m_OffsetAmountControl.EnableWindow( ( m_OffsetMode == OFFSET_MODE_ADAPTIVE ) ); 

	sprintf( temp, "%g", m_OffsetDistance );
	m_OffsetDistanceControl.SetWindowText( temp );

	sprintf( temp, "%g%%", m_OffsetAmount * 100.0f );
	m_OffsetAmountControl.SetWindowText( temp );

	sprintf( temp, "%g%%", m_SmoothAmount * 100.0f );
	m_SmoothAmountControl.SetWindowText( temp );

	m_NormalModeControl.InsertString( -1, "Brush Center" );
	m_NormalModeControl.InsertString( -1, "Screen" );
	m_NormalModeControl.InsertString( -1, "Screen XY" );
	m_NormalModeControl.InsertString( -1, "X" );
	m_NormalModeControl.InsertString( -1, "Y" );
	m_NormalModeControl.InsertString( -1, "Z" );
	m_NormalModeControl.InsertString( -1, "Selected" );
	m_NormalModeControl.SetCurSel( m_NormalMode );

	m_DensityModeControl.InsertString( -1, "Additive" );
	m_DensityModeControl.InsertString( -1, "Attenuated" );
	m_DensityModeControl.SetCurSel( m_DensityMode );

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: prevent the dialog from closing
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnOK( )
{
}


//-----------------------------------------------------------------------------
// Purpose: prevent the dialog from closing
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnCancel( )
{
}


//-----------------------------------------------------------------------------
// Purpose: set up the data exchange for the variables
// Input  : pDX - the data exchange object
//-----------------------------------------------------------------------------
void CSculptCarveOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_OFFSET_MODE, m_OffsetModeControl);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_OFFSET_DISTANCE, m_OffsetDistanceControl);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_OFFSET_AMOUNT, m_OffsetAmountControl);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_SMOOTH_AMOUNT, m_SmoothAmountControl);
	DDX_Control(pDX, IDC_SCULPT_PUSH_OPTION_DENSITY_MODE, m_DensityModeControl);
	DDX_Control(pDX, IDC_IDC_SCULPT_PUSH_OPTION_NORMAL_MODE, m_NormalModeControl);
	DDX_Control(pDX, IDC_CARVE_BRUSH, m_CarveBrushControl);
}


BEGIN_MESSAGE_MAP(CSculptCarveOptions, CDialog)
	ON_CBN_SELCHANGE(IDC_IDC_SCULPT_PUSH_OPTION_NORMAL_MODE, &CSculptCarveOptions::OnCbnSelchangeIdcSculptPushOptionNormalMode)
	ON_CBN_SELCHANGE(IDC_SCULPT_PUSH_OPTION_OFFSET_MODE, &CSculptCarveOptions::OnCbnSelchangeSculptPushOptionOffsetMode)
	ON_EN_CHANGE(IDC_SCULPT_PUSH_OPTION_OFFSET_DISTANCE, &CSculptCarveOptions::OnEnChangeSculptPushOptionOffsetDistance)
	ON_CBN_SELCHANGE(IDC_SCULPT_PUSH_OPTION_DENSITY_MODE, &CSculptCarveOptions::OnCbnSelchangeSculptPushOptionDensityMode)
	ON_EN_KILLFOCUS(IDC_SCULPT_PUSH_OPTION_SMOOTH_AMOUNT, &CSculptCarveOptions::OnEnKillfocusSculptPushOptionSmoothAmount)
	ON_EN_KILLFOCUS(IDC_SCULPT_PUSH_OPTION_OFFSET_AMOUNT, &CSculptCarveOptions::OnEnKillfocusSculptPushOptionOffsetAmount)

	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: sets the normal mode
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnCbnSelchangeIdcSculptPushOptionNormalMode()
{
	m_NormalMode = ( NormalMode )m_NormalModeControl.GetCurSel();
}


//-----------------------------------------------------------------------------
// Purpose: sets the offset mode
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnCbnSelchangeSculptPushOptionOffsetMode()
{
	m_OffsetMode = ( OffsetMode )m_OffsetModeControl.GetCurSel();

	m_OffsetDistanceControl.EnableWindow( ( m_OffsetMode == OFFSET_MODE_ABSOLUTE ) );
	m_OffsetAmountControl.EnableWindow( ( m_OffsetMode == OFFSET_MODE_ADAPTIVE ) ); 
}


//-----------------------------------------------------------------------------
// Purpose: setup for starting to paint on the displacement
// Input  : pView - the 3d view
//			vPoint - the initial click point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptCarveOptions::BeginPaint( CMapView3D *pView, const Vector2D &vPoint )
{
	__super::BeginPaint( pView, vPoint );

	if ( m_bCtrlDown )
	{
		m_Direction = -1.0f;
	}
	else
	{
		m_Direction = 1.0f;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: calculates the normal / direction of the drawing line
// Input  : nPointIndex - which point to factor from
// Output : returns true if we found a valid normal
//			vNormal - the normal we found
//-----------------------------------------------------------------------------
#if 0
bool CSculptCarveOptions::CalculatePointNormal( int nPointIndex, Vector2D &vNormal )
{
	float		count = 0.0;
	Vector2D	vAverage( 0.0f, 0.0f );
	const int	max_backsize = 3;

	// keep going back from the current point until you get a total distance
	for( int j = 0; j < max_backsize; j++ )
	{
		int index = ( nPointIndex - max_backsize + j );
		if ( index < 0 )
		{
			continue;
		}
		int index2 = nPointIndex;

		Vector2D	vDiff( m_DrawPoints[ index2 ].x - m_DrawPoints[ index ].x, m_DrawPoints[ index2 ].y - m_DrawPoints[ index ].y );
		float		Length = Vector2DNormalize( vDiff );

		if ( Length == 0.0f )
		{
			continue;
		}

		float factor = ( ( j + 1 ) * 100 ); // * Length; //  * 8 * Length;
		vAverage += ( vDiff * factor );
		count += factor;
	}

	if ( count > 0.0f )
	{
		vAverage /= count;
		Vector2DNormalize( vAverage );

		vNormal = vAverage;
		return true;
	}

	return false;
}
#endif


//-----------------------------------------------------------------------------
// Purpose: calculates the normal / direction of the drawing line
// Input  : nPointIndex - which point to factor from
// Output : returns true if we found a valid normal
//			vNormal - the normal we found
//-----------------------------------------------------------------------------
bool CSculptCarveOptions::CalculateQueuePoint( Vector2D &vPoint, Vector2D &vNormal )
{
	float		count = 0.0;
	Vector2D	vAverage( 0.0f, 0.0f );
	const float fMaxLength = 40.0f;
	float		fTotalLength = 0.0f;
	Vector2D	vInitialDir;
	bool		bInitialDirSet = false;

	int PointIndex = m_PointQueue.Count() - 1;
	if ( PointIndex <= 1 )
	{
		return false;
	}

	vPoint = m_PointQueue[ PointIndex ];

	// keep going back from the current point until you get a total distance
	for( int j = PointIndex - 1; j >= 0; j-- )
	{
		int index = j;
		int index2 = PointIndex;

		Vector2D	vDiff( m_PointQueue[ index2 ].x - m_PointQueue[ index ].x, m_PointQueue[ index2 ].y - m_PointQueue[ index ].y );
		float		Length = Vector2DNormalize( vDiff );

		if ( Length == 0.0f )
		{
			continue;
		}

		if ( bInitialDirSet == false )
		{
			vInitialDir = vDiff;
			bInitialDirSet = true;
		}

		if ( DotProduct2D( vInitialDir, vDiff ) <= 0.5f )
		{
			break;
		}

		fTotalLength += Length;

		float factor;

#if 0
		factor = 1.0f - ( fTotalLength / fMaxLength );
		if ( factor <= 0.0f )
		{
			factor = 0.01;
		}
		factor *= 20.0f;
#endif
		factor = Length;

		//= Length; // ( ( j + 1 ) * 100 ); // * Length; //  * 8 * Length;
		vAverage += ( vDiff * factor );
		count += factor;

		if ( fTotalLength >= fMaxLength )
		{
			break;
		}
	}

	if ( count > 0.0f )
	{
		vAverage /= count;
		Vector2DNormalize( vAverage );

		vNormal = vAverage;
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: adds the point and normal to the queue
// Input  : vPoint - the point to be added
//			bDrawIt - if we should add this point to the draw / normal lists
//-----------------------------------------------------------------------------
void CSculptCarveOptions::AddQueuePoint( const Vector2D &vPoint, bool bDrawIt )
{
	m_PointQueue.AddToTail( vPoint );
	if ( m_PointQueue.Count() > MAX_QUEUE_SIZE )
	{
		m_PointQueue.Remove( 0 );
	}

	Vector2D	vNewPoint, vNewNormal;

	if ( bDrawIt && CalculateQueuePoint( vNewPoint, vNewNormal ) )
	{
		m_DrawPoints.AddToTail( vNewPoint );
		m_DrawNormal.AddToTail( vNewNormal );
	}
}


//-----------------------------------------------------------------------------
// Purpose: draws the tool in the 3d view
// Input  : pRender - the 3d renderer
//-----------------------------------------------------------------------------
void CSculptCarveOptions::RenderTool3D( CRender3D *pRender )
{
//	pRender->DrawText( "mouse", m_MousePoint.x, m_MousePoint.y, 0 );
//	Msg( "%g %g\n", m_MousePoint.x, m_MousePoint.y );

	pRender->PushRenderMode( RENDER_MODE_WIREFRAME );

	pRender->BeginClientSpace();

	Vector2D	vMousePoint, vMouseNormal;

	if ( CalculateQueuePoint( vMousePoint, vMouseNormal ) )
	{
		Vector2D	vRight( -vMouseNormal.y, vMouseNormal.x );

		pRender->SetDrawColor( 255, 255, 0 );
		pRender->DrawLine( Vector( vMousePoint.x, vMousePoint.y, 0.0f ), Vector( vMousePoint.x + vRight.x * m_BrushSize, vMousePoint.y + vRight.y * m_BrushSize, 0.0f ) );
		pRender->DrawLine( Vector( vMousePoint.x, vMousePoint.y, 0.0f ), Vector( vMousePoint.x - ( vRight.x * m_BrushSize ), vMousePoint.y - ( vRight.y * m_BrushSize ), 0.0f ) );
	}

#if 0
	for( int i = 2; i < m_DrawPoints.Count(); i++ )
	{
		Vector2D	vPoint = m_DrawPoints[ i ];
		Vector2D	vPreviousPoint = m_DrawPoints[ i - 1];
		Vector2D	vNormal = m_DrawNormal[ i ];
		Vector2D	vRight( -m_DrawNormal[ i ].y, m_DrawNormal[ i ].x );
		Vector2D	vDelta = vPoint - vPreviousPoint;
		float		Length = Vector2DLength( vDelta );

		pRender->SetDrawColor( 255, 255, 0 );
		pRender->DrawLine( Vector( vPreviousPoint.x, vPreviousPoint.y, 0.0f ), Vector( vPoint.x, vPoint.y, 0.0f ) );

		pRender->SetDrawColor( 255, 0, 0 );
		pRender->DrawLine( Vector( vPoint.x, vPoint.y, 0.0f ), Vector( vPoint.x + vRight.x * m_BrushSize, vPoint.y + vRight.y * m_BrushSize, 0.0f ) );
//		pRender->DrawLine( Vector( vPoint.x, vPoint.y, 0.0f ), Vector( vPoint.x - ( vRight.x * m_BrushSize ), vPoint.y - ( vRight.y * m_BrushSize ), 0.0f ) );

		vNormal *= Length;
		pRender->SetDrawColor( 0, 255, 0 );
		pRender->DrawLine( Vector( vPoint.x - vNormal.x, vPoint.y - vNormal.y, 0.0f ), Vector( vPoint.x, vPoint.y, 0.0f ) );
	}

	pRender->SetDrawColor( 255, 0, 255 );
	pRender->SetHandleStyle( 6, CRender::HANDLE_SQUARE );
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( pDispMgr )
	{
		int nDispCount = pDispMgr->SelectCount();
		for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
		{
			CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
			if ( pDisp )
			{
				int nVertCount = pDisp->GetSize();
				for ( int iVert = 0; iVert < nVertCount; iVert++ )
				{
					Vector		vVert;
					Vector2D	vViewVert;

					pDisp->GetVert( iVert, vVert );
					pRender->GetCamera()->WorldToView( vVert, vViewVert );

					for( int i = 2; i < m_DrawPoints.Count(); i++ )
					{
						float distance;
						
						float tolerance = DotProduct2D( m_DrawNormal[ i ], m_DrawNormal[ i - 1 ] );
						if ( tolerance <= 0.5f )
						{
							continue;
						}

						distance = DotProduct2D( m_DrawNormal[ i ], m_DrawPoints[ i ] );
						if ( DotProduct2D( m_DrawNormal[ i ], vViewVert ) > distance )
						{
							continue;
						}
						distance = DotProduct2D( m_DrawNormal[ i - 1 ], m_DrawPoints[ i - 1 ] );
						if ( DotProduct2D( m_DrawNormal[ i - 1 ], vViewVert ) < distance )
						{
							continue;
						}

						Vector2D	vRight( -m_DrawNormal[ i ].y, m_DrawNormal[ i ].x );
						Vector2D	vPoint;

						vPoint = m_DrawPoints[ i ] + ( vRight * m_BrushSize );
						distance = DotProduct2D( vRight, vPoint );
						if ( DotProduct2D( vRight, vViewVert ) > distance )
						{
							continue;
						}

						vPoint = m_DrawPoints[ i ] - ( vRight * m_BrushSize );
						distance = DotProduct2D( vRight, vPoint );
						if ( DotProduct2D( vRight, vViewVert ) < distance )
						{
							continue;
						}

//						pRender->DrawHandle( Vector( vViewVert.x, vViewVert.y, 0.0f ) );
						pRender->DrawHandle( vVert );
						break;
					}
				}
			}
		}
	}
#endif

	pRender->EndClientSpace();

#if 0
	if ( m_InSizingMode )
	{	// yellow for sizing mode
		pRender->SetDrawColor( 255, 255, 0 );
		pRender->BeginClientSpace();
		pRender->DrawCircle( Vector( m_StartSizingPoint.x, m_StartSizingPoint.y, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), m_BrushSize, 32 );
		pRender->EndClientSpace();
	}
	else if ( m_bShiftDown )
	{	// purple for smoothing
		pRender->SetDrawColor( 255, 0, 255 );
		pRender->BeginClientSpace();
		pRender->DrawCircle( Vector( m_MousePoint.x, m_MousePoint.y, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), m_BrushSize, 32 );
		pRender->EndClientSpace();
	}
	else if ( m_bCtrlDown )
	{	// red for negative sculpting
		pRender->SetDrawColor( 255, 0, 0 );
		pRender->BeginClientSpace();
		pRender->DrawCircle( Vector( m_MousePoint.x, m_MousePoint.y, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), m_BrushSize, 32 );
		pRender->EndClientSpace();

		Vector	vPaintAxis;
		GetPaintAxis( pRender->GetCamera(), m_MousePoint, vPaintAxis );
		DrawDirection( pRender, -vPaintAxis, Color( 255, 255, 255 ), Color( 255, 128, 128 ) );
	}
	else
	{	// green for positive sculpting
		pRender->SetDrawColor( 0, 255, 0 );
		pRender->BeginClientSpace();
		pRender->DrawCircle( Vector( m_MousePoint.x, m_MousePoint.y, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), m_BrushSize, 32 );
		pRender->EndClientSpace();

		Vector	vPaintAxis;
		GetPaintAxis( pRender->GetCamera(), m_MousePoint, vPaintAxis );
		DrawDirection( pRender, vPaintAxis, Color( 255, 255, 255 ), Color( 255, 128, 128 ) );
	}
#endif

#if 0
	FindColissionIntercept( pRender->GetCamera(), m_MousePoint, true, m_CurrentCollisionPoint, m_CurrentCollisionNormal, m_CurrentCollisionIntercept );

	Vector2D	RadiusPoint = m_MousePoint;
	Vector		vecStart, vecEnd;

	RadiusPoint.x += m_BrushSize;
	pRender->GetCamera()->BuildRay( RadiusPoint, vecStart, vecEnd );

	m_CurrentProjectedRadius = CalcDistanceToLine( m_CurrentCollisionPoint, vecStart, vecEnd );

	pRender->RenderWireframeSphere( m_CurrentCollisionPoint, m_CurrentProjectedRadius, 12, 12, 0, 255, 255 );
#endif

#if 0

	// Get the displacement manager from the active map document.
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();

	// For each displacement surface is the selection list attempt to paint on it.
	int nDispCount = pDispMgr->SelectCount();
	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp )
		{
			CMapDisp	*OrigDisp = NULL;
			int			index = m_OrigMapDisp.Find( pDisp->GetEditHandle() );

			if ( index != m_OrigMapDisp.InvalidIndex() )
			{
				OrigDisp = m_OrigMapDisp[ index ];
			}
			Vector	vPaintPos, vVert;

			int nVertCount = pDisp->GetSize();
			for ( int iVert = 0; iVert < nVertCount; iVert++ )
			{
				if ( IsPointInScreenCircle( pView, pDisp, pOrigDisp, iVert, false ) )
				{
					// Get the current vert.
					pDisp->GetVert( iVert, vVert );
				}
			}
		}
	}
#endif


	pRender->PopRenderMode();

#if 0
	if ( !FindColissionIntercept( pRender->GetCamera(), m_MousePoint, true, m_CurrentCollisionPoint, m_CurrentCollisionNormal, m_CurrentCollisionIntercept ) )
	{
		return;
	}

	Vector2D	RadiusPoint = m_MousePoint;
	Vector		vecStart, vecEnd;

	RadiusPoint.x += m_BrushSize;
	pRender->GetCamera()->BuildRay( RadiusPoint, vecStart, vecEnd );
	m_CurrentProjectedRadius = CalcDistanceToLine( m_CurrentCollisionPoint, vecStart, vecEnd );

	Msg( "Dist = %g at %g,%g,%g\n", m_CurrentProjectedRadius, m_CurrentCollisionPoint.x, m_CurrentCollisionPoint.y, m_CurrentCollisionPoint.z );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: handles the left mouse button up in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptCarveOptions::OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	__super::OnLMouseUp3D( pView, nFlags, vPoint );

	AddQueuePoint( vPoint, true );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the left mouse button down in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptCarveOptions::OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	__super::OnLMouseDown3D( pView, nFlags, vPoint );

	m_DrawPoints.Purge();
	m_DrawNormal.Purge();
	AddQueuePoint( vPoint, true );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the right mouse button down in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptCarveOptions::OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	__super::OnRMouseDown3D( pView, nFlags, vPoint );

	if ( m_bAltDown )
	{
		m_NormalMode = NORMAL_MODE_Z;
		m_NormalModeControl.SetCurSel( m_NormalMode );

#if 0

		//
		// check for closest solid object
		//
		ULONG		ulFace;
		CMapClass	*pObject;

		if( ( ( pObject = pView->NearestObjectAt( vPoint, ulFace ) ) != NULL ) )
		{
			if( pObject->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) )
			{
				// get the solid
				CMapSolid *pSolid = ( CMapSolid* )pObject;
				if( !pSolid )
				{
					return true;
				}

				// trace a line and get the normal -- will get a displacement normal
				// if one exists
				CMapFace *pFace = pSolid->GetFace( ulFace );
				if( !pFace )
				{
					return true;
				}

				Vector vRayStart, vRayEnd;
				pView->GetCamera()->BuildRay( vPoint, vRayStart, vRayEnd );

				Vector vHitPos, vHitNormal;
				if( pFace->TraceLine( vHitPos, vHitNormal, vRayStart, vRayEnd ) )
				{
					// set the paint direction
					m_SelectedNormal = vHitNormal;

					m_NormalMode = NORMAL_MODE_SELECTED;
					m_NormalModeControl.SetCurSel( m_NormalMode );
				}
			}
		}
#else
		Vector	CollisionPoint, CollisionNormal;
		float	CollisionIntercept;

		if ( FindCollisionIntercept( pView->GetCamera(), vPoint, false, CollisionPoint, CollisionNormal, CollisionIntercept ) )
		{
			// set the paint direction
			m_SelectedNormal = -CollisionNormal;

			m_NormalMode = NORMAL_MODE_SELECTED;
			m_NormalModeControl.SetCurSel( m_NormalMode );
		}
#endif
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handles the mouse move in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptCarveOptions::OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	__super::OnMouseMove3D( pView, nFlags, vPoint );

	AddQueuePoint( vPoint, m_bLMBDown );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: returns the painting direction
// Input  : pCamera - the 3d camera
//			vPoint - the 2d mouse point
// Output : vPaintAxis - the direction the painting should go
//-----------------------------------------------------------------------------
void CSculptCarveOptions::GetPaintAxis( CCamera *pCamera, const Vector2D &vPoint, Vector &vPaintAxis )
{
	switch( m_NormalMode )
	{
		case NORMAL_MODE_SCREEN:
			pCamera->GetViewForward( vPaintAxis );
			vPaintAxis = -vPaintAxis;
			break;

		case NORMAL_MODE_SCREEN_XY:
			pCamera->GetViewForward( vPaintAxis );
			vPaintAxis = -vPaintAxis;
			vPaintAxis.z = 0.f;
			break;

		case NORMAL_MODE_BRUSH_CENTER:
			if ( !m_InPaintingMode )
			{
				Vector	CollisionPoint, CollisionNormal;
				float	CollisionIntercept;

				FindCollisionIntercept( pCamera, vPoint, false, CollisionPoint, CollisionNormal, CollisionIntercept );

				vPaintAxis = -CollisionNormal;
			}
			else
			{
				vPaintAxis = -m_StartingCollisionNormal;
			}
			break;

		case NORMAL_MODE_X:
			vPaintAxis.Init( 1.0f, 0.0f, 0.0f );
			break;

		case NORMAL_MODE_Y:
			vPaintAxis.Init( 0.0f, 1.0f, 0.0f );
			break;

		case NORMAL_MODE_Z:
			vPaintAxis.Init( 0.0f, 0.0f, 1.0f );
			break;

		case NORMAL_MODE_SELECTED:
			vPaintAxis = m_SelectedNormal;
			break;

		default:
			vPaintAxis.Init( 0.0f, 0.0f, 1.0f );
	}
}


//-----------------------------------------------------------------------------
// Purpose: determines if a displacement point is affected by the carve
// Input  : pView - the 3d view
//			pDisp - the displacement
//			pOrigDisp - the displacement prior to any updates
//			nVertIndex - the vertex to look at
//			nBrushPoint - which list point to check against
//			bUseOrigDisplacement - should we use the vert from the original displacement
//			bUseCurrentPosition - should we use the current collision test point
// Output :	returns true if the point is affected
//			vViewVert - the 2d view vert location
//-----------------------------------------------------------------------------
bool CSculptCarveOptions::IsPointAffected( CMapView3D *pView, CMapDisp *pDisp, CMapDisp *pOrigDisp, int vertIndex, int nBrushPoint, Vector2D &vViewVert, bool bUseOrigDisplacement, bool bUseCurrentPosition )
{
	Vector	vVert, vTestVert;

	pDisp->GetVert( vertIndex, vVert );

	if ( pOrigDisp && bUseOrigDisplacement )
	{
		pOrigDisp->GetVert( vertIndex, vTestVert );
	}
	else
	{
		vTestVert = vVert;
	}

	pView->GetCamera()->WorldToView( vTestVert, vViewVert );

	float distance;

	float tolerance = DotProduct2D( m_DrawNormal[ nBrushPoint ], m_DrawNormal[ nBrushPoint - 1 ] );
	if ( tolerance <= 0.5f )
	{
		return false;
	}

	distance = DotProduct2D( m_DrawNormal[ nBrushPoint ], m_DrawPoints[ nBrushPoint ] );
	if ( DotProduct2D( m_DrawNormal[ nBrushPoint ], vViewVert ) > distance )
	{
		return false;
	}
	distance = DotProduct2D( m_DrawNormal[ nBrushPoint - 1 ], m_DrawPoints[ nBrushPoint - 1 ] );
	if ( DotProduct2D( m_DrawNormal[ nBrushPoint - 1 ], vViewVert ) < distance )
	{
		return false;
	}

	Vector2D	vRight( -m_DrawNormal[ nBrushPoint ].y, m_DrawNormal[ nBrushPoint ].x );
	Vector2D	vPoint;

	vPoint = m_DrawPoints[ nBrushPoint ] + ( vRight * m_BrushSize );
	distance = DotProduct2D( vRight, vPoint );
	if ( DotProduct2D( vRight, vViewVert ) > distance )
	{
		return false;
	}

	vPoint = m_DrawPoints[ nBrushPoint ] - ( vRight * m_BrushSize );
	distance = DotProduct2D( vRight, vPoint );
	if ( DotProduct2D( vRight, vViewVert ) < distance )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: applies the specific push operation onto the displacement
// Input  : pView - the 3d view
//			vPoint - the mouse point
//			pDisp - the displacement to apply the push to
//			pOrigDisp - the original displacement prior to any adjustments
//-----------------------------------------------------------------------------
void CSculptCarveOptions::DoPaintOperation( CMapView3D *pView, const Vector2D &vPoint, CMapDisp *pDisp, CMapDisp *pOrigDisp )
{
	Vector		vPaintPos, vVert, vDirection;
	Vector2D	vViewVert;
	float		flDistance = 0.0f;
	Vector		vPaintAxis;

	int nTestPoint = m_DrawPoints.Count() - 1;
	if ( nTestPoint < 2 )
	{
		return;
	}

	if ( m_bShiftDown )
	{
//		DoSmoothOperation( pView, vPoint, pDisp, pOrigDisp );
//		m_SpatialData.m_flRadius = 256.0f;
//		m_SpatialData.m_flScalar = 5.0f / m_SmoothAmount;

//		m_SpatialData.m_flRadius = m_StartingProjectedRadius * 1.5f;
		m_SpatialData.m_flRadius = m_CurrentProjectedRadius * 2.0f;
		m_SpatialData.m_flRadius2 = ( m_SpatialData.m_flRadius * m_SpatialData.m_flRadius );
		m_SpatialData.m_flOORadius2 = 1.0f / m_SpatialData.m_flRadius2;
		m_SpatialData.m_flScalar = 10.0f / m_SmoothAmount;
		m_SpatialData.m_vCenter = m_CurrentCollisionPoint;

		DoPaintSmooth( pView, vPoint, pDisp, pOrigDisp );
		return;
	}

	GetPaintAxis( pView->GetCamera(), vPoint, vPaintAxis );

	vDirection = vPaintAxis * m_Direction;

	switch( m_OffsetMode )
	{
		case OFFSET_MODE_ADAPTIVE:
			flDistance = m_StartingProjectedRadius * m_OffsetAmount;
			break;
		case OFFSET_MODE_ABSOLUTE:
			flDistance = m_OffsetDistance;
			break;
	}

	int nVertCount = pDisp->GetSize();
	for ( int iVert = 0; iVert < nVertCount; iVert++ )
	{
		if ( IsPointAffected( pView, pDisp, pOrigDisp, iVert, nTestPoint, vViewVert ) )
		{
			pDisp->GetVert( iVert, vVert );

			Vector2D	vRight( -m_DrawNormal[ nTestPoint ].y, m_DrawNormal[ nTestPoint ].x );
			float		fLineDistance = DotProduct2D( vRight, m_DrawPoints[ nTestPoint ] ) - DotProduct2D( vRight, vViewVert );
			
			fLineDistance = ( fLineDistance + m_BrushSize ) / ( m_BrushSize * 2.0f );
			int index = ( int )( fLineDistance * MAX_SCULPT_SIZE );

			index = clamp( index, 0, MAX_SCULPT_SIZE - 1 );
			index = MAX_SCULPT_SIZE - index - 1;

			float		fScaledDistance = m_BrushPoints[ index ] * flDistance;

			if ( fScaledDistance == 0.0f )
			{
				continue;
			}

			switch( m_DensityMode )
			{
				case DENSITY_MODE_ADDITIVE:
					VectorScale( vDirection, fScaledDistance, vPaintPos );
					VectorAdd( vPaintPos, vVert, vPaintPos );
					break;

				case DENSITY_MODE_ATTENUATED:
					VectorScale( vDirection, fScaledDistance, vPaintPos );
					VectorAdd( vPaintPos, vVert, vPaintPos );

					if ( pOrigDisp )
					{
						Vector	vOrigVert, vDiff;
						float	Length;

						pOrigDisp->GetVert( iVert, vOrigVert );
						vDiff = ( vPaintPos - vOrigVert );
						Length = vDiff.Length() / flDistance;
						if ( Length > 1.0f )
						{
							Length = 1.0f;
						}

						vPaintPos = vOrigVert + ( Length * vDirection * flDistance );
					}
					break;
			}

			AddToUndo( &pDisp );
			pDisp->Paint_SetValue( iVert, vPaintPos );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: sets the offset distance
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnEnChangeSculptPushOptionOffsetDistance()
{
	char	temp[ 1024 ];

	m_OffsetDistanceControl.GetWindowText( temp, sizeof( temp ) );
	m_OffsetDistance = atof( temp );
}


//-----------------------------------------------------------------------------
// Purpose: sets the density mode
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnCbnSelchangeSculptPushOptionDensityMode()
{
	m_DensityMode = ( DensityMode )m_DensityModeControl.GetCurSel();
}


//-----------------------------------------------------------------------------
// Purpose: sets the smooth amount
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnEnKillfocusSculptPushOptionSmoothAmount()
{
	char	temp[ 1024 ], t2[ 1024 ];

	m_SmoothAmountControl.GetWindowText( temp, sizeof( temp ) );
	sscanf( temp, "%f%%", &m_SmoothAmount );
	m_SmoothAmount /= 100.0f;

	if ( m_SmoothAmount <= 0.0f )
	{
		m_SmoothAmount = 0.2f;
	}

	sprintf( t2, "%g%%", m_SmoothAmount * 100.0f );

	if ( strcmpi( temp, t2 ) != 0 )
	{
		m_SmoothAmountControl.SetWindowText( t2 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: sets the offset amount
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnEnKillfocusSculptPushOptionOffsetAmount()
{
	char	temp[ 1024 ], t2[ 1024 ];

	m_OffsetAmountControl.GetWindowText( temp, sizeof( temp ) );
	sscanf( temp, "%f%%", &m_OffsetAmount );
	m_OffsetAmount /= 100.0f;

	if ( m_OffsetAmount <= 0.0f )
	{
		m_OffsetAmount = 1.0f;
	}

	sprintf( t2, "%g%%", m_OffsetAmount * 100.0f );

	if ( strcmpi( temp, t2 ) != 0 )
	{
		m_OffsetAmountControl.SetWindowText( t2 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: paints the carve brush
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CBrush black( RGB( 0, 0, 0 ) );
	CBrush red( RGB( 255, 0, 0 ) );
	CBrush green( RGB( 0, 255, 0 ) );
	CBrush blue_red( RGB( 64, 0, 128 ) );
	CBrush blue_green( RGB( 0, 64, 128 ) );
	CBrush blue( RGB( 0, 0, 255 ) );

	CRect WindowRect;
	m_CarveBrushControl.GetWindowRect( &WindowRect );
	ScreenToClient( &WindowRect );
	dc.FillRect( WindowRect, &black );

	float center = ( WindowRect.bottom + WindowRect.top ) / 2;
	float height = ( WindowRect.bottom - WindowRect.top ) - 1;

	if ( m_BrushLocation != -1 )
	{
		CRect	rect;

		rect.left = ( m_BrushLocation * 2 ) + WindowRect.left;
		rect.right = rect.left + 2;
		rect.bottom = WindowRect.bottom;
		rect.top = WindowRect.top;
		dc.FillRect( rect, &blue );
	}

	for( int i = 0; i < MAX_SCULPT_SIZE; i++ )
	{
		float	size = height / 2.0f * m_BrushPoints[ i ];
		CRect	rect;
		CBrush	*pBrush;

		rect.left = ( i * 2 ) + WindowRect.left;
		rect.right = rect.left + 2;
		rect.bottom = center - size;
		rect.top = center;

		if ( m_BrushPoints[ i ] >= 0.0f )
		{
			if ( m_BrushLocation == i )
			{
				pBrush = &blue_green;
			}
			else
			{
				pBrush = &green;
			}
		}
		else
		{
			if ( m_BrushLocation == i )
			{
				pBrush = &blue_red;
			}
			else
			{
				pBrush = &red;
			}
		}
		dc.FillRect( rect, pBrush );
	}
}


//-----------------------------------------------------------------------------
// Purpose: adjusts the carve brush
// Input  : x - location to set the height to
//			y - offset into the brush
//-----------------------------------------------------------------------------
void CSculptCarveOptions::AdjustBrush( int x, int y )
{
	CRect	WindowRect;
	CPoint	MousePoint( x, y );

	m_CarveBrushControl.GetWindowRect( &WindowRect );
	ClientToScreen( &MousePoint );

	if ( MousePoint.x >= WindowRect.left && MousePoint.x < WindowRect.right &&
		 MousePoint.y >= WindowRect.top && MousePoint.y < WindowRect.bottom )
	{
		int		pos = ( MousePoint.x - WindowRect.left ) / 2;
		float	center = ( WindowRect.bottom + WindowRect.top ) / 2;
		float	value = ( center - MousePoint.y ) / ( WindowRect.bottom - WindowRect.top ) * 2.0f;

		value = clamp( value, -1.0f, 1.0f );
		if ( pos >= 0 && pos < MAX_SCULPT_SIZE )
		{
			m_BrushPoints[ pos ] = value;
			Invalidate();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: sets the brush cursor location
// Input  : x - x location of mouse
//			y - y location of mouse
//-----------------------------------------------------------------------------
void CSculptCarveOptions::AdjustBrushCursor( int x, int y )
{
	CRect	WindowRect;
	int		OldBrushLocation = m_BrushLocation;
	CPoint	MousePoint( x, y );

	m_CarveBrushControl.GetWindowRect( &WindowRect );
	ClientToScreen( &MousePoint );

	if ( MousePoint.x >= WindowRect.left && MousePoint.x < WindowRect.right &&
		 MousePoint.y >= WindowRect.top && MousePoint.y < WindowRect.bottom )
	{
		m_BrushLocation = ( MousePoint.x - WindowRect.left ) / 2;
	}
	else
	{
		m_BrushLocation = -1;
	}

	if ( OldBrushLocation != m_BrushLocation )
	{
		Invalidate();
	}
}


//-----------------------------------------------------------------------------
// Purpose: handles adjusting the brush
// Input  : nFlags - mouse buttons
//			point - mouse point
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnLButtonDown(UINT nFlags, CPoint point)
{
	AdjustBrush( point.x, point.y );
	AdjustBrushCursor( point.x, point.y );

	__super::OnLButtonDown(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: handles adjusting the brush
// Input  : nFlags - mouse buttons
//			point - mouse point
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnLButtonUp(UINT nFlags, CPoint point)
{
	AdjustBrush( point.x, point.y );
	AdjustBrushCursor( point.x, point.y );

	__super::OnLButtonUp(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: handles adjusting the brush
// Input  : nFlags - mouse buttons
//			point - mouse point
//-----------------------------------------------------------------------------
void CSculptCarveOptions::OnMouseMove(UINT nFlags, CPoint point)
{
	if ( nFlags & MK_LBUTTON )
	{
		AdjustBrush( point.x, point.y );
	}
	AdjustBrushCursor( point.x, point.y );

	__super::OnMouseMove(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: we want to handle the messages for mouse events
//-----------------------------------------------------------------------------
BOOL CSculptCarveOptions::PreTranslateMessage( MSG* pMsg )
{
	if ( pMsg->message == WM_LBUTTONDOWN || pMsg->message == WM_MOUSEMOVE )
	{
		return FALSE;
	}

	return __super::PreTranslateMessage( pMsg );
}






#if 0

class CSculptRegenerator : public ITextureRegenerator
{
public:
	CSculptRegenerator( unsigned char *ImageData, int Width, int Height, enum ImageFormat Format ) :
	  m_ImageData( ImageData ), 
		  m_Width( Width ), 
		  m_Height( Height ),
		  m_Format( Format )
	  {
	  }

	  virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	  {
		  for (int iFrame = 0; iFrame < pVTFTexture->FrameCount(); ++iFrame )
		  {
			  for (int iFace = 0; iFace < pVTFTexture->FaceCount(); ++iFace )
			  {
				  int nWidth = pVTFTexture->Width();
				  int nHeight = pVTFTexture->Height();
				  int nDepth = pVTFTexture->Depth();
				  for (int z = 0; z < nDepth; ++z)
				  {
					  // Fill mip 0 with a checkerboard
					  CPixelWriter pixelWriter;
					  pixelWriter.SetPixelMemory( pVTFTexture->Format(), pVTFTexture->ImageData( iFrame, iFace, 0, 0, 0, z ), pVTFTexture->RowSizeInBytes( 0 ) );

					  switch( m_Format )
					  {
					  case IMAGE_FORMAT_BGR888:
						  {
							  unsigned char *data = m_ImageData;

							  for (int y = 0; y < nHeight; ++y)
							  {
								  pixelWriter.Seek( 0, y );
								  for (int x = 0; x < nWidth; ++x)
								  {
									  pixelWriter.WritePixel( *( data + 2 ), *( data + 1 ), *( data ), 255 );
									  data += 3;
								  }
							  }
						  }
						  break;
					  }
				  }
			  }
		  }
	  }

	  virtual void Release()
	  {
		  delete this;
	  }

private:
	unsigned char		*m_ImageData;
	int					m_Width;
	int					m_Height;
	enum ImageFormat	m_Format;
};




// CSculptProjectOptions dialog

IMPLEMENT_DYNAMIC(CSculptProjectOptions, CDialog)

CSculptProjectOptions::CSculptProjectOptions(CWnd* pParent /*=NULL*/) : 
	CDialog(CSculptProjectOptions::IDD, pParent),
	CSculptTool()
{
	m_FileDialog = new CFileDialog(TRUE, NULL, NULL, OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST, "Image Files (*.tga)|*.tga||");
	m_FileDialog->m_ofn.lpstrInitialDir = "";

	m_ImagePixels = NULL;
	m_pTexture = NULL;
	m_pMaterial = NULL;

	m_ProjectX = 100;
	m_ProjectY = 100;
	m_ProjectWidth = 100;
	m_ProjectHeight = 100;
	m_TileWidth = m_TileHeight = 1.0;
	m_OriginalTileWidth = m_TileWidth;
	m_OriginalTileHeight = m_TileHeight;

	m_ProjectLocation.Init( 100.0f, 100.0f, 0.0f );
	m_OriginalProjectLocation = m_ProjectLocation;
	m_ProjectSize.Init( 100.0f, 100.0f, 0.0f );
	m_OriginalProjectSize = m_ProjectSize;

	m_ToolMode = PROJECT_MODE_NONE;
}

CSculptProjectOptions::~CSculptProjectOptions()
{
	delete m_FileDialog;

	if ( m_ImagePixels )
	{
		delete [] m_ImagePixels;
	}

	if ( m_pTexture )
	{
		m_pTexture->DecrementReferenceCount();
		m_pTexture = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: set up the data exchange for the variables
// Input  : pDX - the data exchange object
//-----------------------------------------------------------------------------
void CSculptProjectOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROJECT_SIZE, m_ProjectSizeControl);
	DDX_Control(pDX, IDC_PROJECT_SIZE_NUM, m_ProjectSizeNumControl);
}


BEGIN_MESSAGE_MAP(CSculptProjectOptions, CDialog)
	ON_BN_CLICKED(IDC_LOAD_IMAGE, &CSculptProjectOptions::OnBnClickedLoadImage)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_PROJECT_SIZE, &CSculptProjectOptions::OnNMCustomdrawProjectSize)
END_MESSAGE_MAP()

bool CSculptProjectOptions::Paint( CMapView3D *pView, const Vector2D &vPoint, SpatialPaintData_t &spatialData )
{
	CSculptTool::Paint( pView, vPoint, spatialData );

	switch( m_ToolMode )
	{
		case PROJECT_MODE_SIZE:
			DoSizing( vPoint );
			break;

		case PROJECT_MODE_POSITION:
			DoPosition( vPoint );
			break;

		case PROJECT_MODE_TILE:
			DoTiling( vPoint );
			break;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: draws the tool in the 3d view
// Input  : pRender - the 3d renderer
//-----------------------------------------------------------------------------
void CSculptProjectOptions::RenderTool3D(CRender3D *pRender)
{
	if ( !m_pMaterial )
	{
		return;
	}

	pRender->PushRenderMode( RENDER_MODE_TEXTURED );
	bool bPopMode = pRender->BeginClientSpace();

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	pRender->BindMaterial( m_pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 4 );

	meshBuilder.Position3f( m_ProjectLocation.x, m_ProjectLocation.y, m_ProjectLocation.z );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color4ub( 255, 255, 255, 128 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( m_ProjectLocation.x + m_ProjectSize.x, m_ProjectLocation.y, m_ProjectLocation.z );
	meshBuilder.TexCoord2f( 0, m_TileWidth, 0.0f );
	meshBuilder.Color4ub( 255, 255, 255, 128 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( m_ProjectLocation.x + m_ProjectSize.x, m_ProjectLocation.y + m_ProjectSize.y, m_ProjectLocation.z );
	meshBuilder.TexCoord2f( 0, m_TileWidth, m_TileHeight );
	meshBuilder.Color4ub( 255, 255, 255, 128 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( m_ProjectLocation.x, m_ProjectLocation.y + m_ProjectSize.y, m_ProjectLocation.z );
	meshBuilder.TexCoord2f( 0, 0.0f, m_TileHeight );
	meshBuilder.Color4ub( 255, 255, 255, 128 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	if ( bPopMode )
	{
		pRender->EndClientSpace();
	}
	pRender->PopRenderMode();
}

//-----------------------------------------------------------------------------
// Purpose: handles the left mouse button up in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptProjectOptions::OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	CSculptTool::OnLMouseUp3D( pView, nFlags, vPoint );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: handles the left mouse button down in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptProjectOptions::OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	CSculptTool::OnLMouseDown3D( pView, nFlags, vPoint );

	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( pDispMgr )
	{
		pDispMgr->PreUndo( "Displacement Modifier" );
	}

	PrepareDispForPainting();

	// Handle painting.
	if ( !DoPaint( pView, vPoint ) )
	{
		return false;
	}

	// Finish painting.
	if ( !PostPaint( m_PaintOwner->GetAutoSew() ) )
	{
		return false;
	}

	if( pDispMgr )
	{
		pDispMgr->PostUndo();
	}

	return true;
}

bool CSculptProjectOptions::OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	CSculptTool::OnRMouseUp3D( pView, nFlags, vPoint );

	m_ToolMode = PROJECT_MODE_NONE;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: handles the right mouse button down in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptProjectOptions::OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	CSculptTool::OnRMouseDown3D( pView, nFlags, vPoint );

	m_OriginalProjectSize = m_ProjectSize;
	m_OriginalProjectLocation = m_ProjectLocation;
	m_StartSizingPoint = vPoint;

	if ( m_bCtrlDown )
	{
		m_ToolMode = PROJECT_MODE_SIZE;
	}
	else if ( m_bShiftDown )
	{
		m_ToolMode = PROJECT_MODE_TILE;
	}
	else
	{
		m_ToolMode = PROJECT_MODE_POSITION;
	}

	m_StartSizingPoint = vPoint;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: handles the mouse move in the 3d view
// Input  : pView - the 3d view
//			nFlags - the button flags
//			vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CSculptProjectOptions::OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	CSculptTool::OnMouseMove3D( pView, nFlags, vPoint );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: applies the specific push operation onto the displacement
// Input  : pView - the 3d view
//			vPoint - the mouse point
//			pDisp - the displacement to apply the push to
//			pOrigDisp - the original displacement prior to any adjustments
//-----------------------------------------------------------------------------
void CSculptProjectOptions::DoPaintOperation( CMapView3D *pView, const Vector2D &vPoint, CMapDisp *pDisp, CMapDisp *pOrigDisp )
{
	Vector	vPaintPos, vVert;
	Vector	vPaintAxis;

	pView->GetCamera()->GetViewForward( vPaintAxis );
	vPaintAxis = -vPaintAxis;

	vPaintAxis *= ( m_ProjectSizeControl.GetPos() * 16.0f );

	int nVertCount = pDisp->GetSize();
	for ( int iVert = 0; iVert < nVertCount; iVert++ )
	{
		Vector2D	ViewVert;
		Vector		vTestVert;

		pDisp->GetVert( iVert, vTestVert );
		pView->GetCamera()->WorldToView( vTestVert, ViewVert );

		if ( ViewVert.x >= m_ProjectLocation.x &&
			 ViewVert.y >= m_ProjectLocation.y &&
			 ViewVert.x <= m_ProjectLocation.x + m_ProjectSize.x &&
			 ViewVert.y <= m_ProjectLocation.y + m_ProjectSize.y )
		{
			pDisp->GetVert( iVert, vVert );

			float sCoord = ( ViewVert.x - m_ProjectLocation.x ) / m_ProjectSize.x;
			float tCoord = ( ViewVert.y - m_ProjectLocation.y ) / m_ProjectSize.y;
			
			sCoord *= m_TileWidth;
			tCoord *= m_TileHeight;

			sCoord -= ( int )sCoord;
			tCoord -= ( int )tCoord;

			int x = ( sCoord * m_Width );
			int y = ( tCoord * m_Height );

			unsigned char *pos = &m_ImagePixels[ ( y * m_Width * 3 ) + ( x * 3 ) ];
			float gray = ( 0.3f * pos[ 2 ] ) + ( 0.59f * pos[ 1 ] ) + ( 0.11f * pos[ 0 ] );
			gray /= 255.0f;

			vPaintPos = vVert + ( vPaintAxis * gray );

			AddToUndo( &pDisp );
			pDisp->Paint_SetValue( iVert, vPaintPos );
		}
	}
}

void CSculptProjectOptions::OnBnClickedLoadImage()
{
	if ( m_FileDialog->DoModal() == IDCANCEL )
	{
		return;
	}

	ReadImage( m_FileDialog->GetPathName() );
}

bool CSculptProjectOptions::ReadImage( CString &FileName )
{
	enum ImageFormat	imageFormat;
	float				sourceGamma;
	CUtlBuffer			buf;

	if ( !g_pFullFileSystem->ReadFile( FileName, NULL, buf ) )
	{
		return false;
	}

	if ( !TGALoader::GetInfo( buf, &m_Width, &m_Height, &imageFormat, &sourceGamma ) )
	{
		return false;
	}

	if ( m_ImagePixels )
	{
		delete [] m_ImagePixels;
	}

	int memRequired = ImageLoader::GetMemRequired( m_Width, m_Height, 1, imageFormat, false );
	m_ImagePixels = new unsigned char[ memRequired ];

	buf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	TGALoader::Load( m_ImagePixels, buf, m_Width, m_Height, imageFormat, sourceGamma, false );

	m_pTexture = dynamic_cast< ITextureInternal * >( g_pMaterialSystem->CreateProceduralTexture( "SculptProject", TEXTURE_GROUP_OTHER, m_Width, m_Height, imageFormat, 
		TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_PROCEDURAL ) );

	ITextureRegenerator *pRegen = new CSculptRegenerator( m_ImagePixels, m_Width, m_Height, imageFormat );
	m_pTexture->SetTextureRegenerator( pRegen );
	m_pTexture->Download();

	m_pMaterial = MaterialSystemInterface()->FindMaterial( "editor/sculpt", TEXTURE_GROUP_OTHER );

	return true;
}

bool CSculptProjectOptions::DoSizing( const Vector2D &vPoint )
{
	m_ProjectSize.x = m_OriginalProjectSize.x + ( vPoint.x - m_StartSizingPoint.x );
	if ( m_ProjectSize.x < 1.0f )
	{
		m_ProjectSize.x = 1.0f;
	}
	m_ProjectSize.y = m_OriginalProjectSize.y + ( vPoint.y - m_StartSizingPoint.y );
	if ( m_ProjectSize.y < 1.0f )
	{
		m_ProjectSize.y = 1.0f;
	}

	return true;
}

bool CSculptProjectOptions::DoPosition( const Vector2D &vPoint )
{
	m_ProjectLocation.x = m_OriginalProjectLocation.x + ( vPoint.x - m_StartSizingPoint.x );
	m_ProjectLocation.y = m_OriginalProjectLocation.y + ( vPoint.y - m_StartSizingPoint.y );

	return true;
}

bool CSculptProjectOptions::DoTiling( const Vector2D &vPoint )
{
	m_TileWidth += ( vPoint.x - m_StartSizingPoint.x ) / m_ProjectSize.x;
	m_TileHeight += ( vPoint.y - m_StartSizingPoint.y ) / m_ProjectSize.y;

	m_StartSizingPoint = vPoint;

	return true;
}

void CSculptProjectOptions::OnNMCustomdrawProjectSize(NMHDR *pNMHDR, LRESULT *pResult)
{
//	LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);

	char	temp[ 128 ];
	sprintf( temp, "%d", m_ProjectSizeControl.GetPos() * 16 );

	m_ProjectSizeNumControl.SetWindowText( temp );

	*pResult = 0;
}

//-----------------------------------------------------------------------------
// Purpose: initializes the dialog
// Output : returns true if successful
//-----------------------------------------------------------------------------
BOOL CSculptProjectOptions::OnInitDialog()
{
	__super::OnInitDialog();

	m_ProjectSizeControl.SetRange( 1, 32 );
	m_ProjectSizeControl.SetTicFreq( 1 );
	m_ProjectSizeControl.SetPageSize( 4 );
	m_ProjectSizeControl.SetLineSize( 4 );

	return TRUE;  
}

#endif

// current mouse position updates location of rectangle
// then rmb = size
// +control = st adjust


#include <tier0/memdbgoff.h>
