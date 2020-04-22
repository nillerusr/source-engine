//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "GlobalFunctions.h"
#include "MainFrm.h"	// dvs: hack for tools migration code
#include "MapDoc.h"
#include "MapView2D.h"
#include "Options.h"
#include "Render2D.h"
#include "Render3D.h"
#include "RenderUtils.h"
#include "resource.h"
#include "StatusBarIDs.h"
#include "hammer_mathlib.h"
#include "vgui/Cursor.h"
#include "HammerVGui.h"
#include <VGuiMatSurface/IMatSystemSurface.h>
#include "camera.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning(disable:4244)

extern float g_MAX_MAP_COORD; // dvs: move these into Globals.h!!
extern float g_MIN_MAP_COORD; // dvs: move these into Globals.h!!


WorldUnits_t Box3D::m_eWorldUnits = Units_None;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Box3D::Box3D(void)
{
	SetEmpty();
	SetDrawFlags(0);
	m_TranslateMode = modeScale;
	m_vTranslationFixPoint.Init();
	m_TranslateHandle.Init();
	m_bEnableHandles = true;
	SetDrawColors(Options.colors.clrToolHandle, Options.colors.clrToolBlock);
}

void Box3D::SetEmpty()
{
	Tool3D::SetEmpty();
	ResetBounds();

	if ( m_pDocument )
	{
		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			pt - Point in client coordinates.
//			bValidOnly - 
// Output : 
//-----------------------------------------------------------------------------
int Box3D::HitTest(CMapView *pView, const Vector2D &ptClient, bool bTestHandles)
{
	bool bHit = false;

	if ( pView->HitTest( ptClient, bmins, bmaxs ) )
	{
		// The point is inside the main rect.
		m_LastHitTestHandle.Init();
		bHit = true;
	}

	if ( !m_bEnableHandles || !bTestHandles )
	{
		// Handles are turned off, so we don't need to do any more testing.
		// Return whether we hit the main rect or not.
		return bHit;
	}

	// check if we hit a handle

	Vector handles[3*3*3];
	int numHandles = GetVisibleHandles( handles, pView, m_TranslateMode );


	Vector vOffset(HANDLE_OFFSET,HANDLE_OFFSET,HANDLE_OFFSET);

	if ( pView->IsOrthographic() )
	{
		vOffset /= pView->GetCamera()->GetZoom();
	}
	else
	{
		vOffset.Init();
	}
		
	
	Vector vCenter = (bmins+bmaxs)/2;
	Vector vDelta = (bmaxs + vOffset) - vCenter;

	for ( int i = 0; i<numHandles; i++ )
	{

		Vector pos = vCenter + vDelta * handles[i];

		if ( HitRect( pView, ptClient, pos, HANDLE_RADIUS ) )
		{
			// remember handle found
			m_LastHitTestHandle = handles[i];
			bHit = true;
			break;
		}
	}

	return bHit;
}


//-----------------------------------------------------------------------------
// Purpose: Set the cursor based on the hit test results and current translate mode.
// Input  : eHandleHit - The handle that the cursor is over.
//			eTransformMode - The current transform mode of the tool - scale, rotate, or shear.
//-----------------------------------------------------------------------------
unsigned long Box3D::UpdateCursor(CMapView *pView, const Vector &vHandleHit, TransformMode_t eTransformMode)
{
	if ( eTransformMode == modeMove || vHandleHit.IsZero() )
		return vgui::dc_sizeall;
	
	if ( eTransformMode == modeNone )
		return vgui::dc_arrow;
		
	if (eTransformMode == modeRotate)
		return  g_pMatSystemSurface->CreateCursorFromFile("Resource/rotate.cur");
	
	// cursor icon depends on handle and map view :

	Vector2D ptOrigin; pView->WorldToClient( ptOrigin, Vector(0,0,0) );
	Vector2D ptHit; pView->WorldToClient( ptHit, vHandleHit );
	Vector2D pt; pt.x = ptHit.x - ptOrigin.x; pt.y = ptHit.y - ptOrigin.y;

	if (eTransformMode == modeScale)
	{
		if ( pt.x > 0 )
		{
			if ( pt.y > 0 )
				return vgui::dc_sizenwse;
			else if ( pt.y < 0 )
				return vgui::dc_sizenesw;
			else
				return vgui::dc_sizewe;
		}
		else  if ( pt.x < 0 )
		{
			if ( pt.y > 0 )
				return vgui::dc_sizenesw;
			else if ( pt.y < 0 )
				return vgui::dc_sizenwse;
			else
				return vgui::dc_sizewe;
				
		}
		else // pt.x == 0 
		{
			if ( pt.y != 0 )
				return vgui::dc_sizens;
			else
				return vgui::dc_sizeall;
		}
	}
	else if (eTransformMode == modeShear)
	{
		if ( pt.x == 0 )
			return vgui::dc_sizewe;
		else
			return vgui::dc_sizens;
	}
	
	return vgui::dc_none;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bEnable - 
//-----------------------------------------------------------------------------
void Box3D::EnableHandles(bool bEnable)
{
	m_bEnableHandles = bEnable;
}


//-----------------------------------------------------------------------------
// Purpose: Finds the corner nearest to a given point in world coordinates.
// Output : Returns the corner in world coordinates (axThird is always 0).
//-----------------------------------------------------------------------------
const Vector Box3D::NearestCorner( const Vector2D &vPoint, CMapView *pView, const Vector *pCustomHandleBox )
{
	Vector	vHandles[3*3*3];
	float	fBestDist = 999999.9f;
	Vector	vBestCorner(0,0,0);
	int		nFace = -1;
	Vector	start,end,pos;
	
	pView->BuildRay( vPoint, start,end );
	float dist = IntersectionLineAABBox( bmins, bmaxs, start, end, nFace );

	if ( dist < 0 )
		return vBestCorner;

	// get point where we hit the bbox
	pos = end-start; VectorNormalize( pos );
	pos = start + pos*dist;
	
	// mode rotate has only corner handles
	int nNumHandles = GetVisibleHandles( vHandles, pView, modeRotate );
	
	for ( int i=0; i<nNumHandles; i++ )
	{
		Vector vecCorner;

		HandleToWorld( vecCorner, vHandles[i], pCustomHandleBox );

		float distance = VectorLength( vecCorner - pos );

		if ( distance < fBestDist )
		{
			fBestDist = distance;
			vBestCorner = vecCorner;
		}
	}
	 	
	return vBestCorner;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt - 
//			ptForceMoveRef - 
// Output : Returns TRUE if pt hits a handle or is in box area, FALSE otherwise.
//-----------------------------------------------------------------------------
void Box3D::StartTranslation( 
	CMapView *pView, 
	const Vector2D &vPoint, 
	const Vector &vHandleOrigin, 
	const Vector *pRefPoint,
	const Vector *pCustomHandleBox )
{
	if ( vHandleOrigin.IsZero() )
	{
		// we hit the main body, switch to move translation then
		m_LastTranslateMode = m_TranslateMode;
		m_TranslateMode = modeMove;
	}

	m_TranslateHandle = vHandleOrigin;
	m_bPreventOverlap = true;
	
	if ( pRefPoint )
	{
		// transformation reference point was given
		m_vTranslationFixPoint = *pRefPoint;
	}
	else
	{
		// build reference point based on mode & handle
		if (m_TranslateMode == modeRotate)
		{
			// user center of object for rotation
			m_vTranslationFixPoint = (bmins + bmaxs) / 2;
		}
		else if (m_TranslateMode == modeMove)
		{
			// chose nearest corner to  
			m_vTranslationFixPoint = NearestCorner( vPoint, pView, pCustomHandleBox );
		}
		else
		{
			// find opposite point to handle
			m_vTranslationFixPoint.Init();
			for ( int i=0; i<3; i++ )
			{
				float handle = m_TranslateHandle[i];

				if ( handle > 0 )
				{
					m_vTranslationFixPoint[i] = bmins[i]; 
				}
				else if ( handle < 0 )
				{
					m_vTranslationFixPoint[i] = bmaxs[i]; 
				}
			}
		}
	}

	// get axis normals from picked face
	Vector v1,v2,v3,vOrigin;

	// if no valid translation handle, cull against BBox
	if ( m_TranslateMode == modeMove )
	{
		int nFace;
		pView->BuildRay( vPoint, v1, v2 );
					
		IntersectionLineAABBox( bmins, bmaxs, v1, v2, nFace );

		if ( nFace >= 0 )
		{
			// get axis & normals of face we hit
			GetAxisFromFace( nFace, v1, v2, v3 );
		}
		else
		{
			pView->GetBestTransformPlane( v1,v2,v3 );
		}

		vOrigin = m_vTranslationFixPoint;
	}
	else
	{
		pView->GetBestTransformPlane( v1,v2,v3 );
		HandleToWorld( vOrigin, m_TranslateHandle );
	}

	// set temp transformation plane 
	SetTransformationPlane(vOrigin, v1, v2, v3 );

	// align translation plane to world origin
	ProjectOnTranslationPlane( vec3_origin, vOrigin, 0 );

	// set transformation plane
	SetTransformationPlane(vOrigin, v1, v2, v3 );

	Tool3D::StartTranslation( pView, vPoint, false );

	m_TransformMatrix.Identity();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszBuf - 
//-----------------------------------------------------------------------------
void Box3D::GetStatusString(char *pszBuf)
{
	*pszBuf = '\0';



	Vector mins(0,0,0);
	Vector maxs(0,0,0);

	if ( IsValidBox() )
	{
		mins = bmins;
		maxs = bmaxs;
	}

	if ( IsTranslating() )
	{
		TranslateBox( mins, maxs );
	}

	Vector size = maxs - mins;
	Vector center = ( maxs + mins ) * 0.5f;

	if ( !IsTranslating() || m_TranslateMode == modeScale || m_TranslateMode == modeMove )
	{
		if (!IsEmpty())
		{
			if ( IsTranslating() && m_TranslateMode == modeMove )
			{
				center = m_vTranslationFixPoint;
				TranslatePoint( center );
			}

			switch (m_eWorldUnits)
			{
				case Units_None:
				{
					sprintf(pszBuf, " %dw %dl %dh @(%.0f %.0f %.0f)", 
						(int)fabs(size.x), (int)fabs(size.y), (int)fabs(size.z),
						center.x,center.y,center.z );
					break;
				}

				case Units_Inches:
				{
					sprintf(pszBuf, " %d\"w %d\"l %d\"h", (int)fabs(size.x), (int)fabs(size.y), (int)fabs(size.z));
					break;
				}

				case Units_Feet_Inches:
				{
					int nFeetWide = (int)fabs(size.x) / 12;
					int nInchesWide = (int)fabs(size.x) % 12;

					int nFeetLong = (int)fabs(size.y) / 12;
					int nInchesLong = (int)fabs(size.y) % 12;

					int nFeetHigh = (int)fabs(size.z) / 12;
					int nInchesHigh = (int)fabs(size.z) % 12;

					sprintf(pszBuf, " %d' %d\"w %d' %d\"l %d' %d\"h", nFeetWide, nInchesWide, nFeetLong, nInchesLong, nFeetHigh, nInchesHigh);
					break;
				}
			}
		}
	}
	else if ( m_TranslateMode == modeShear )
	{
		sprintf(pszBuf, " shear: %d %d %d ", (int)m_vTranslation.x, (int)m_vTranslation.y, (int)m_vTranslation.z );
	}
	else if ( m_TranslateMode == modeRotate )
	{
		int rotAxis = GetTransformationAxis();

		if ( rotAxis != -1  )
		{
			sprintf(pszBuf, " %.2f%c", m_vTranslation[abs(rotAxis+2)%3], 0xF8);
		}
		else
		{
			sprintf(pszBuf, " %.2f %.2f %.2f%c", m_vTranslation.x, m_vTranslation.y, m_vTranslation.z, 0xF8);
		}
	}
	else
	{
		Assert( 0 );
	}
	
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Box3D::UpdateStatusBar()
{
	char szBuf[MAX_PATH];
	GetStatusString(szBuf);
	SetStatusText(SBI_SIZE, szBuf);
}

int Box3D::GetVisibleHandles( Vector *handles, CMapView *pView, int nMode )
{
	bool bCorners, bEdges, bFaces;
	bool bIs2D = pView->IsOrthographic();

	Vector vViewAxis = pView->GetViewAxis();
	Vector vViewPoint; pView->GetCamera()->GetViewPoint( vViewPoint );
	
	if ( bIs2D )
	{
		bCorners = false;
		bEdges	= nMode == modeRotate || nMode == modeScale;
		bFaces = nMode == modeShear || nMode == modeScale;  
	}
	else
	{
		bCorners = nMode == modeRotate || nMode == modeScale;
		bEdges	= nMode == modeScale;
		bFaces = nMode == modeShear; 
	}

	if ( !bCorners && !bEdges && !bFaces )
		return 0;

	int count = 0;

	for ( int x = -1; x < 2; x++ )
	{
		if ( bIs2D && (x != 0) && (fabs(vViewAxis.x) == 1) )
			continue;

		for ( int y = -1; y < 2; y++ )
		{
			if ( bIs2D && (y != 0) && (fabs(vViewAxis.y) == 1) )
				continue;

			for ( int z = -1; z<2; z++)
 			{
				if ( bIs2D && (z != 0) && (fabs(vViewAxis.z) == 1) )
					continue;

				int n = abs(x) + abs(y) + abs(z);

				if ( n == 0 )
				{
					// don't add center as handle
					continue;
				}
				else if ( n == 1 )
				{
					if ( !bFaces )
						continue;
				}
				else if ( n == 2 )
				{
					if ( !bEdges )
						continue;
				}
				else
				{
					if ( !bCorners )
						continue;
				}

				if ( !bIs2D  )
				{
					Vector vHandle; HandleToWorld( vHandle, Vector(x,y,z) );
					Vector vDelta = vHandle - vViewPoint; 
					float fDistance = VectorLength( vDelta );
					int nFace;

					vDelta /= fDistance; // normalize

					if ( DotProduct(vDelta,vViewAxis) < 0 )
						continue; 

					float fIntersection = IntersectionLineAABBox( bmins, bmaxs, vViewPoint, vViewPoint+vDelta*99999, nFace );

 					if ( fIntersection >= 0 && fIntersection*1.01 < fDistance )
						continue;
				}
				
				// add handle as visible
				handles[count] = Vector(x,y,z);
				count++;
			}
		}
	}

	return count;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ptWorld - point to update with in world coordinates
//			uConstraints - 
//			dragSize - 
// Output : 
//-----------------------------------------------------------------------------
bool Box3D::UpdateTranslation(const Vector &vUpdate, UINT uConstraints)
{
	if (m_TranslateMode == modeNone)
	{
		return false;
	}

	else if ( m_TranslateMode == modeRotate )
	{
 		Vector vCenter; ProjectOnTranslationPlane( m_vTranslationFixPoint, vCenter );
		Vector vStart; HandleToWorld( vStart, m_TranslateHandle );
		Vector v1 = vStart-vCenter; VectorNormalize( v1 );
		Vector v2 = (vStart+vUpdate)-vCenter; VectorNormalize( v2 );
		float volume = DotProduct( m_vPlaneNormal, CrossProduct( v1, v2) );
		float angle = RAD2DEG( acos( DotProduct( v1,v2) ) );

		if (uConstraints & constrainSnap)
		{
			angle += 7.5;
			angle -= fmod(double(angle), double(15.0));
		}
		else
		{
			angle += 0.25;
			angle -= fmod(double(angle), double(.5));
		}

        if ( volume < 0 )
			angle = -angle;

		if ( fabs(m_vPlaneNormal.x) == 1 )
			m_vTranslation.z = (m_vPlaneNormal.x>0)?angle:-angle;
		else if ( fabs(m_vPlaneNormal.y) == 1 )
			m_vTranslation.x = (m_vPlaneNormal.y>0)?angle:-angle;
		else if ( fabs(m_vPlaneNormal.z) == 1 )
			m_vTranslation.y = (m_vPlaneNormal.z>0)?angle:-angle;
	}
	else 
	{
		if ( vUpdate == m_vTranslation )
			return false; // no change

		m_vTranslation = vUpdate;

		// restrict translation, snap to grid, prevent overlap etc
		// make sure reference point snaps if enabled
		if ( uConstraints )
		{
			// project back on projection plane
			Vector pos; 
			
			if ( m_TranslateMode == modeMove )
			{
				// when moving opbject make sure reference point is on grid
				pos = m_vTranslationFixPoint;
			}
			else
			{
				// otherwise translated handle should be on grid
				HandleToWorld( pos, m_TranslateHandle);
			}

			ProjectOnTranslationPlane( pos + m_vTranslation, m_vTranslation, uConstraints );
			m_vTranslation -= pos;
		}

		if ( m_TranslateMode == modeScale )
		{
			for ( int i=0; i<3; i++ )
			{
				float handle = m_TranslateHandle[i];

 				if ( handle > 0 )
				{
					float newMaxs = bmaxs[i] + m_vTranslation[i];

					if( m_bPreventOverlap && newMaxs <= bmins[i] )
					{
						m_vTranslation[i] = bmins[i] - bmaxs[i] + 1;
					}
				}
				else if ( handle < 0 )
				{
					float newMins = bmins[i] + m_vTranslation[i];

					if( m_bPreventOverlap && newMins >= bmaxs[i] )
					{
						m_vTranslation[i] = bmaxs[i] - bmins[i] - 1;
					}
				}
			}
		}
	}

	UpdateTransformMatrix();

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwHandleColor - 
//			dwBoxColor - 
//-----------------------------------------------------------------------------
void Box3D::SetDrawColors(COLORREF dwHandleColor, COLORREF dwBoxColor)
{
	if (dwHandleColor != 0xffffffff)
	{
		m_clrHandle = dwHandleColor;
	}

	if (dwBoxColor != 0xffffffff)
	{
		m_clrBox = dwBoxColor;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pt - 
//-----------------------------------------------------------------------------
void Box3D::TranslatePoint(Vector& pt)
{
	TransformPoint( m_TransformMatrix, pt );
}

const VMatrix& Box3D::GetTransformMatrix()
{
	return m_TransformMatrix;
}

void Box3D::UpdateTransformMatrix()
{
	m_TransformMatrix.Identity();

	if ( m_TranslateMode == modeNone )
	{
		return;
	}
	else if ( m_TranslateMode == modeMove )
	{
		m_TransformMatrix.SetTranslation( m_vTranslation );
		return;
	}
	else if ( m_TranslateMode == modeScale )
	{
		Vector vScale( 1,1,1);
		Vector vMove(0,0,0);
		Vector vSize = bmaxs-bmins;

		for ( int i=0; i<3; i++ )
		{
			float handle = m_TranslateHandle[i];

			if ( vSize[i] == 0 )
				continue;

			if ( handle > 0 )
			{
				vScale[i] = (m_vTranslation[i]+vSize[i]) / vSize[i];
				vMove[i] = m_vTranslation[i] / 2;
			}
			else if ( handle < 0 )
			{
				vScale[i] = (-m_vTranslation[i]+vSize[i]) / vSize[i];
 				vMove[i] = m_vTranslation[i] / 2;
			}
		}

		m_TransformMatrix = m_TransformMatrix.Scale( vScale );
		m_TransformMatrix.SetTranslation( vMove );
	}
	else if ( m_TranslateMode == modeShear )
	{
		Vector vSize = bmaxs-bmins;

		int axisS = -1; // shear axis that wont change
		int axisA = -1; // first shear axis
		int axisB = -1; // second shear axis
		
		for ( int i=0; i<3; i++ )
		{
 			float handle = m_TranslateHandle[i];

			if ( handle > 0 )
			{
				Assert( axisS == -1);
				axisS = i;
			}
			else if ( handle < 0 )
			{
				Assert( axisS == -1);
				axisS = i;
				vSize *= -1;
			}
			else
			{
				if ( axisA == -1 )
					axisA = i;
				else
					axisB = i;
			}
		}

		Assert( (axisA!=-1) && (axisB!=-1) && (axisS!=-1) );
		
 		m_TransformMatrix.m[axisA][axisS] = (m_vTranslation[axisA])/(vSize[axisS]);
 		m_TransformMatrix.m[axisB][axisS] = (m_vTranslation[axisB])/(vSize[axisS]);
	}
	else if ( m_TranslateMode == modeRotate )
	{
		QAngle angle = *(QAngle*)&m_vTranslation; // buuuhhh
 		m_TransformMatrix.SetupMatrixOrgAngles( vec3_origin, angle );
	}

	// apply m_vTranslationFixPoint offset

	Vector offset;
	m_TransformMatrix.V3Mul( m_vTranslationFixPoint, offset );
	offset = m_vTranslationFixPoint - offset;

	m_TransformMatrix.m[0][3] += offset[0];
	m_TransformMatrix.m[1][3] += offset[1];
	m_TransformMatrix.m[2][3] += offset[2];
}

void Box3D::TranslateBox(Vector& mins, Vector& maxs)
{
	if ( m_TranslateMode == modeNone )
	{
		return;
	}

	if ( m_TranslateMode == modeMove )
	{
		mins += m_vTranslation;
		maxs += m_vTranslation;
	}

	else if ( m_TranslateMode == modeScale )
	{
		for ( int i=0; i<3; i++ )
		{
			float handle = m_TranslateHandle[i];

			if ( handle > 0 )
			{
				maxs[i] += m_vTranslation[i];
			}
			else if ( handle < 0 )
			{
				mins[i] += m_vTranslation[i];
			}
		}
	}

	else if ( m_TranslateMode == modeShear )
	{
		TranslatePoint( mins );
		TranslatePoint( maxs );
	}
	else if ( m_TranslateMode == modeRotate )
	{
		TranslatePoint( mins );
		TranslatePoint( maxs );
	}

	NormalizeBox( mins, maxs );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bSave - 
//-----------------------------------------------------------------------------
void Box3D::FinishTranslation(bool bSave)
{
	if( bSave )
	{
		Vector newMins = bmins;
		Vector newMaxs = bmaxs;

		TranslateBox( newMins, newMaxs );
		LimitBox( newMins, newMaxs, g_MAX_MAP_COORD ); 

		SetBounds( newMins, newMaxs );

		m_bEmpty = false;
	}

	// if we are finished with moving the selection, switch back to the
	// original translation mode
	if ( m_TranslateMode == modeMove )
	{
		m_TranslateMode = m_LastTranslateMode;
	}

	Tool3D::FinishTranslation(bSave);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Box3D::ToggleTranslateMode(void)
{
	if( m_TranslateMode == modeMove )
	{
		m_TranslateMode = modeScale;
	}
	else if( m_TranslateMode == modeScale )
	{
		m_TranslateMode = modeRotate;
	}
	else if( m_TranslateMode == modeRotate )
	{
		m_TranslateMode = modeShear;
	}
	else if( m_TranslateMode == modeShear )
	{
		m_TranslateMode = modeScale; // don't go back to move mode
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwFlags - 
//-----------------------------------------------------------------------------
void Box3D::SetDrawFlags(DWORD dwFlags)
{
	m_dwDrawFlags = dwFlags;
}

void Box3D::RenderHandles2D(CRender2D *pRender, const Vector &mins, const Vector &maxs)
{
	Vector handles[3*3*3];

	int numHandles = GetVisibleHandles( handles, pRender->GetView(), m_TranslateMode );

	if ( numHandles == 0 )
		return;

	pRender->SetHandleColor( GetRValue(m_clrHandle), GetGValue(m_clrHandle), GetBValue(m_clrHandle) );

	if ( m_TranslateMode == modeRotate )
	{
		pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_CIRCLE );
		
	}
	else
	{
		pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_SQUARE );
	}
	
	
	Vector vCenter = (mins+maxs)/2;
	Vector vDelta = maxs - vCenter;
	Vector2D vOffset; 

	bool bPopMode = pRender->BeginClientSpace();
	
	for ( int i=0; i<numHandles; i++)
	{
		pRender->TransformNormal( vOffset, handles[i] );
		vOffset.x = fsign(vOffset.x);
		vOffset.y = fsign(vOffset.y);
		vOffset*=HANDLE_OFFSET;

		Vector pos = vCenter + vDelta * handles[i];
		pRender->DrawHandle( pos, &vOffset );
	}

	if ( bPopMode )
		pRender->EndClientSpace();
}

void Box3D::RenderHandles3D(CRender3D *pRender, const Vector &mins, const Vector &maxs)
{
	Vector handles[3*3*3];

	int numHandles = GetVisibleHandles( handles, pRender->GetView(), m_TranslateMode );

	if ( numHandles == 0 )
		return;

	Vector vCenter = (mins+maxs)/2;
	Vector vDelta = maxs - vCenter;

	if ( m_TranslateMode == modeRotate )
	{
		pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_CIRCLE );

	}
	else
	{
		pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_SQUARE );
	}

	pRender->SetHandleColor( GetRValue(m_clrHandle), GetGValue(m_clrHandle), GetBValue(m_clrHandle) );
	pRender->PushRenderMode( RENDER_MODE_FLAT_NOZ );
	
	bool bPopMode = pRender->BeginClientSpace();

	for ( int i=0; i<numHandles; i++)
	{
		Vector pos = vCenter + vDelta * handles[i];

		pRender->DrawHandle( pos );
	}

	if ( bPopMode )
		pRender->EndClientSpace();

	pRender->PopRenderMode();
}

void Box3D::HandleToWorld( Vector &vWorld, const Vector &vHandle, const Vector *pCustomHandleBox)
{
	Vector vCenter, vDelta;
	if ( pCustomHandleBox )
	{
		vCenter = (pCustomHandleBox[0] + pCustomHandleBox[1]) / 2;
		vDelta = pCustomHandleBox[1] - vCenter;
	}
	else
	{
		vCenter = (bmins+bmaxs)/2;
		vDelta = bmaxs - vCenter;
	}
	
	vWorld = vCenter + (vDelta * vHandle);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDC - 
//			bounds - 
//-----------------------------------------------------------------------------
void Box3D::RenderTool2D(CRender2D *pRender)
{
	Vector mins = bmins;
	Vector maxs = bmaxs;
	CMapView2D *pView = (CMapView2D*)pRender->GetView();

	Assert( pRender );

	if ( IsTranslating() )
	{
		TranslateBox( mins, maxs );
	}
	else if ( IsEmpty() )
	{
		return;
	}


	if ( m_dwDrawFlags & boundstext)
	{
		DrawBoundsText(pRender, mins, maxs, DBT_TOP | DBT_LEFT);
	}

	if ( IsTranslating() )
	{
	    pRender->PushRenderMode( RENDER_MODE_DOTTED );
		pRender->SetDrawColor( GetRValue(Options.colors.clrToolDrag), GetGValue(Options.colors.clrToolDrag), GetBValue(Options.colors.clrToolDrag) );
	}
	else if (!(m_dwDrawFlags & thicklines))
	{
		pRender->PushRenderMode( RENDER_MODE_DOTTED );
		pRender->SetDrawColor( GetRValue(m_clrBox), GetGValue(m_clrBox), GetBValue(m_clrBox) );
	}
	else
	{
		pRender->PushRenderMode( RENDER_MODE_FLAT_NOZ );
		pRender->SetDrawColor( GetRValue(m_clrBox), GetGValue(m_clrBox), GetBValue(m_clrBox) );
	}

	// render bounds
	if ( !IsTranslating() || m_TranslateMode == modeScale || m_TranslateMode == modeMove )
	{
		// draw simple rectangle
		pRender->DrawRectangle( mins, maxs, false, 0 );
	}
	else
	{
		// during rotation or shearing, draw transformed bounding box

		Vector v[4];
		
		// init all points to center
		v[0] = v[1] = v[2] = v[3] = (bmins+bmaxs) / 2;

		int axis = pView->axHorz;

		v[0][axis] = v[1][axis] = bmins[axis];
		v[2][axis] = v[3][axis] = bmaxs[axis];

		axis = pView->axVert;

		v[1][axis] = v[2][axis] = bmins[axis];
		v[0][axis] = v[3][axis] = bmaxs[axis];

		for ( int i=0; i<4; i++)
		{
			TranslatePoint( v[i] );
		}
		
		pRender->DrawLine( v[0], v[1] );
		pRender->DrawLine( v[1], v[2] );
		pRender->DrawLine( v[2], v[3] );
		pRender->DrawLine( v[3], v[0] );
	}

	pRender->PopRenderMode();

	// draw a cross for translation origin in move or rotation mode
	if ( IsTranslating() )
	{
		if ( m_TranslateMode == modeMove || m_TranslateMode == modeRotate )
		{
			Vector vec = m_vTranslationFixPoint;

			if ( m_TranslateMode == modeMove  )
 			{
				TranslatePoint( vec );
			}

			// draw 'X'
			pRender->SetHandleStyle( 7, CRender::HANDLE_CROSS );
			pRender->SetHandleColor( GetRValue(Options.colors.clrToolDrag), GetGValue(Options.colors.clrToolDrag), GetBValue(Options.colors.clrToolDrag) );
			pRender->DrawHandle( vec );
			
		}
	}
	else if ( m_bEnableHandles )
	{
		RenderHandles2D( pRender, mins, maxs );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Renders this region as a wireframe box.
// Input  : pRender - 3D Renderer.
//-----------------------------------------------------------------------------
void Box3D::RenderTool3D(CRender3D *pRender)
{
    if ( IsTranslating() )
	{
		VMatrix matrix = GetTransformMatrix();
		pRender->BeginLocalTransfrom( matrix );
	}
	else if (IsEmpty())
	{
		return;
	}

	pRender->PushRenderMode( RENDER_MODE_FLAT );
	pRender->SetDrawColor( GetRValue(m_clrBox), GetGValue(m_clrBox), GetBValue(m_clrBox) );
	pRender->DrawBox( bmins, bmaxs );
	pRender->PopRenderMode();

	if ( IsTranslating() )
	{
		pRender->EndLocalTransfrom();

		if ( m_TranslateMode == modeMove || m_TranslateMode == modeRotate )
		{
			Vector vec = m_vTranslationFixPoint;

			if ( m_TranslateMode == modeMove  )
			{
				TranslatePoint( vec );
			}

			// draw 'X'
			pRender->PushRenderMode( RENDER_MODE_FLAT_NOZ );
			pRender->SetHandleStyle( 7, CRender::HANDLE_CROSS );
			pRender->SetHandleColor( GetRValue(Options.colors.clrToolDrag), GetGValue(Options.colors.clrToolDrag), GetBValue(Options.colors.clrToolDrag) );
			pRender->DrawHandle( vec );
			pRender->PopRenderMode();

		}
	}
	else if ( m_bEnableHandles )
	{
		RenderHandles3D( pRender, bmins, bmaxs );
	};

	

	
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *vecStart - 
//			*mins - 
//			*maxs - 
//-----------------------------------------------------------------------------
void Box3D::StartNew( CMapView *pView, const Vector2D &vPoint, const Vector &vecStart, const Vector &vecSize )
{
	//Setup our info
	m_TranslateMode	= modeScale;
	m_TranslateHandle = Vector( 1, 1, 1 );
	bmins = vecStart;
	bmaxs = vecStart+vecSize;
	NormalizeBox( bmins, bmaxs );

	StartTranslation( pView, vPoint, Vector( 1, 1, 1 )  );

	m_bPreventOverlap = false;
	m_bEmpty = false;
}




