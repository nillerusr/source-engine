//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include "DispPaint.h"
#include "ToolDisplace.h"
#include "CollisionUtils.h"
#include "DispManager.h"
#include "MapDoc.h"
#include "MapDisp.h"
#include "GlobalFunctions.h"
#include "History.h"
#include "DispSew.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define DISPPAINT_RADIUS_OUTER_CLAMP	0.01f

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CDispPaintMgr::CDispPaintMgr()
{
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CDispPaintMgr::~CDispPaintMgr()
{
	m_aNudgeData.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDispPaintMgr::Paint( SpatialPaintData_t &spatialData, bool bAutoSew )
{
	// Setup painting.
	if ( !PrePaint( spatialData ) )
		return false;

	// Handle painting.
	if ( !DoPaint( spatialData ) )
		return false;

	// Finish painting.
	if ( !PostPaint( bAutoSew ) )
		return false;

	// Successful paint operation.
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDispPaintMgr::PrePaint( SpatialPaintData_t &spatialData )
{
	// Generate cached spatial data.
	spatialData.m_flRadius2 = ( spatialData.m_flRadius * spatialData.m_flRadius );
	spatialData.m_flOORadius2 = 1.0f / spatialData.m_flRadius2;

	// Setup nudge data.
	if ( spatialData.m_bNudgeInit )
	{
		m_aNudgeData.RemoveAll();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDispPaintMgr::PostPaint( bool bAutoSew )
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
// Purpose: 
//-----------------------------------------------------------------------------
bool CDispPaintMgr::DoPaint( SpatialPaintData_t &spatialData )
{
	// Get the displacement manager from the active map document.
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return false;

	// Special case - nudging!
	if ( spatialData.m_bNudge && !spatialData.m_bNudgeInit )
	{
		DoNudgeAdd( spatialData );
		return true;
	}

	// For each displacement surface is the selection list attempt to paint on it.
	int nDispCount = pDispMgr->SelectCount();
	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp )
		{
			// Test paint sphere displacement bbox for overlap.
			Vector vBBoxMin, vBBoxMax;
			pDisp->GetBoundingBox( vBBoxMin, vBBoxMax );
			if ( PaintSphereDispBBoxOverlap( spatialData.m_vCenter, spatialData.m_flRadius, vBBoxMin, vBBoxMax ) )
			{
				// Paint with the correct effect
				switch ( spatialData.m_nEffect )
				{
				case DISPPAINT_EFFECT_RAISELOWER: 
					{ 
						DoPaintAdd( spatialData, pDisp );
						break; 
					}
				case DISPPAINT_EFFECT_RAISETO: 
					{ 
						DoPaintEqual( spatialData, pDisp );
						break;
					}
				case DISPPAINT_EFFECT_SMOOTH: 
					{ 
						DoPaintSmooth( spatialData, pDisp );
						break;
					}
				}
			}	                             
		}
	}

	// Successful paint.
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintMgr::NudgeAdd( CMapDisp *pDisp, int iVert )
{
	int iNudge = m_aNudgeData.AddToTail();
	m_aNudgeData[iNudge].m_hDisp = pDisp->GetEditHandle();
	m_aNudgeData[iNudge].m_iVert = iVert;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintMgr::DoNudgeAdd( SpatialPaintData_t &spatialData )
{
	Vector vPaintPos, vVert;
	float flDistance2;

	int nNudgeCount = m_aNudgeData.Count();
	for ( int iNudge = 0; iNudge < nNudgeCount; iNudge++ )
	{
		DispVertPair_t *pPairData = &m_aNudgeData[iNudge];

		// Get the current vert.
		CMapDisp *pDisp = EditDispMgr()->GetDisp( pPairData->m_hDisp );
		pDisp->GetVert( pPairData->m_iVert, vVert );

		if ( IsInSphereRadius( spatialData.m_vCenter, spatialData.m_flRadius2, vVert, flDistance2 ) )
		{
			// Build the new position (paint value) and set it.
			if ( spatialData.m_uiBrushType == DISPPAINT_BRUSHTYPE_SOFT )
			{
				DoPaintOneOverR( spatialData, vVert, flDistance2, vPaintPos );
			}
			else if ( spatialData.m_uiBrushType == DISPPAINT_BRUSHTYPE_HARD )
			{
				DoPaintOne( spatialData, vVert, vPaintPos );
			}
			AddToUndo( &pDisp );
			pDisp->Paint_SetValue( pPairData->m_iVert, vPaintPos );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDispPaintMgr::PaintSphereDispBBoxOverlap( const Vector &vCenter, float flRadius, 
											    const Vector &vBBoxMin, const Vector &vBBoxMax )
{
	return IsBoxIntersectingSphere( vBBoxMin, vBBoxMax, vCenter, flRadius );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDispPaintMgr::IsInSphereRadius( const Vector &vCenter, float flRadius2,
									  const Vector &vPos, float &flDistance2 )
{
	Vector vTmp;
	VectorSubtract( vPos, vCenter, vTmp );
	flDistance2 = ( vTmp.x * vTmp.x ) + ( vTmp.y * vTmp.y ) + ( vTmp.z * vTmp.z );
	return ( flDistance2 < flRadius2 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintMgr::AddToUndo( CMapDisp **pDisp )
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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintMgr::DoPaintAdd( SpatialPaintData_t &spatialData, CMapDisp *pDisp )
{
	Vector vPaintPos, vVert;
	float flDistance2;

	int nVertCount = pDisp->GetSize();
	for ( int iVert = 0; iVert < nVertCount; iVert++ )
	{
		// Get the current vert.
		pDisp->GetVert( iVert, vVert );

		if ( IsInSphereRadius( spatialData.m_vCenter, spatialData.m_flRadius2, vVert, flDistance2 ) )
		{
			// Build the new position (paint value) and set it.
			if ( spatialData.m_uiBrushType == DISPPAINT_BRUSHTYPE_SOFT )
			{
				DoPaintOneOverR( spatialData, vVert, flDistance2, vPaintPos );
			}
			else if ( spatialData.m_uiBrushType == DISPPAINT_BRUSHTYPE_HARD )
			{
				DoPaintOne( spatialData, vVert, vPaintPos );
			}
			AddToUndo( &pDisp );
			pDisp->Paint_SetValue( iVert, vPaintPos );

			// Add data to nudge list.
			if ( spatialData.m_bNudgeInit )
			{
				NudgeAdd( pDisp, iVert );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintMgr::DoPaintEqual( SpatialPaintData_t &spatialData, CMapDisp *pDisp )
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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintMgr::DoPaintSmooth( SpatialPaintData_t &spatialData, CMapDisp *pDisp )
{
	Vector vPaintPos, vVert;
	float flDistance2;

	int nVertCount = pDisp->GetSize();
	for ( int iVert = 0; iVert < nVertCount; iVert++ )
	{
		// Get the current vert.
		pDisp->GetVert( iVert, vVert );

		if ( IsInSphereRadius( spatialData.m_vCenter, spatialData.m_flRadius2, vVert, flDistance2 ) )
		{
			// Build the new smoothed position and set it.
			if ( DoPaintSmoothOneOverExp( spatialData, vVert, vPaintPos ) )
			{
				AddToUndo( &pDisp );
				pDisp->Paint_SetValue( iVert, vPaintPos );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CDispPaintMgr::CalcSmoothRadius2( const SpatialPaintData_t &spatialData, const Vector &vPoint )
{
	Vector vTmp;
	VectorSubtract( spatialData.m_vCenter, vPoint, vTmp );
	float flDistance2 = ( vTmp.x * vTmp.x ) + ( vTmp.y * vTmp.y ) + ( vTmp.z * vTmp.z );

	float flRatio = flDistance2 / spatialData.m_flRadius2;
	flRatio = 1.0f - flRatio;

	float flRadius = flRatio * spatialData.m_flRadius;
	return ( flRadius * flRadius );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDispPaintMgr::DoPaintSmoothOneOverExp( const SpatialPaintData_t &spatialData, 
										     const Vector &vNewCenter,
										     Vector &vPaintPos )
{
	// Get the displacement manager from the active map document.
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return false;

	// Calculate the smoothing radius.
	float flNewRadius2 = CalcSmoothRadius2( spatialData, vNewCenter );
	float flNewRadius = ( float )sqrt( flNewRadius2 );

	// Test all selected surfaces for smoothing.
	float flWeight = 0.0f;
	float flSmoothDist = 0.0f;

	// Calculate the plane dist.
	float flPaintDist = spatialData.m_vPaintAxis.Dot( vNewCenter );

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
							flFactor *= 1.0f / ( spatialData.m_flScalar * 2.0f );
						}
						
						Vector vProjectVert;
						float flProjectDist = DotProduct( vVert, spatialData.m_vPaintAxis ) - flPaintDist;
						flSmoothDist += ( flProjectDist * flFactor );
						flWeight += flFactor;
					}
				}
			}
		}
	}

	// Re-normalize the smoothing position.
	flSmoothDist /= flWeight;
	vPaintPos = vNewCenter + ( spatialData.m_vPaintAxis * flSmoothDist );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintMgr::DoPaintOneOverR( const SpatialPaintData_t &spatialData,
									 const Vector &vPos, float flDistance2, 
									 Vector &vNewPos )
{
	float flValue = 1.0f - ( flDistance2 * spatialData.m_flOORadius2 );
	flValue *= spatialData.m_flScalar;
	VectorScale( spatialData.m_vPaintAxis, flValue, vNewPos );
	VectorAdd( vNewPos, vPos, vNewPos );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintMgr::DoPaintOne( const SpatialPaintData_t &spatialData,
							    const Vector &vPos, Vector &vNewPos )
{
	float flValue = spatialData.m_flScalar;
	VectorScale( spatialData.m_vPaintAxis, flValue, vNewPos );
	VectorAdd( vNewPos, vPos, vNewPos );
}