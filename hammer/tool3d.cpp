//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "MapDoc.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "Tool3D.h"
#include "hammer_mathlib.h"
#include "vgui/Cursor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning(disable:4244)


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Tool3D::Tool3D(void)
{
	m_vPlaneNormal.Init();
	m_vPlaneOrigin.Init();
	m_bIsTranslating = false;

	for ( int i=0;i<2; i++ )
	{
		m_bMouseDown[i] = false;
		m_bMouseDragged[i] = false;
		m_vMouseStart[i].Init();
	}

	m_vMousePos.Init();
}


void Tool3D::StartTranslation( CMapView *pView, const Vector2D &vClickPoint, bool bUseDefaultPlane )
{
	if ( bUseDefaultPlane )
	{
		Vector vecHorz, vecVert,vecThird;
		pView->GetBestTransformPlane( vecHorz, vecVert,vecThird );
		SetTransformationPlane( vec3_origin, vecHorz, vecVert, vecThird );
	}

	m_vTranslation.Init();
	ProjectTranslation( pView, vClickPoint, m_vTranslationStart, 0 );
	m_bIsTranslating = true;
	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
}

bool Tool3D::UpdateTranslation(CMapView *pView, const Vector2D &vPoint, UINT nFlags)
{
	Vector vTransform;
	ProjectTranslation( pView, vPoint, vTransform, 0 );
	vTransform -= m_vTranslationStart;
	return UpdateTranslation( vTransform, nFlags );
}

bool Tool3D::UpdateTranslation(const Vector &vUpdate, UINT flags /* = 0 */)
{
    if ( m_vTranslation == vUpdate )
		return false;

	m_vTranslation = vUpdate;

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bSave - 
//-----------------------------------------------------------------------------
void Tool3D::FinishTranslation(bool bSave)
{
	m_bIsTranslating = false;

	if ( bSave )
	{
		// assume the tool changed an object
		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
	}
	else
	{
		// just update the tool
		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
	}
}

void Tool3D::TranslatePoint(Vector& vPos)
{
	vPos += m_vTranslation; // most simple translation
}

int Tool3D::HitTest(CMapView *pView, const Vector &vecWorld, bool bTestHandles)
{
	Vector2D vClient;
	pView->WorldToClient( vClient, vecWorld );
	return HitTest( pView, vClient, bTestHandles );
}

bool Tool3D::HitRect(CMapView *pView, const Vector2D &vPoint, const Vector &vCenter, int extent )
{
	Vector2D vClientCenter;
	pView->WorldToClient( vClientCenter, vCenter );

	if ( vPoint.x < (vClientCenter.x-extent) || vPoint.x > ( vClientCenter.x+extent) )
		return false;

	if ( vPoint.y < (vClientCenter.y-extent) || vPoint.y > ( vClientCenter.y+extent) )
		return false;

	return true;
}

int Tool3D::GetTransformationAxis()
{
	if ( fabs( m_vPlaneNormal.x ) == 1 )
	{
		return 0;
	}
	else if ( fabs( m_vPlaneNormal.y ) == 1 )
	{
		return 1;
	}
	else if ( fabs( m_vPlaneNormal.z ) == 1 )
	{
		return 2;
	}
	else
		return -1;
}

void Tool3D::SetTransformationPlane( const Vector &vOrigin, const Vector &vHorz, const Vector &vVert, const Vector &vNormal )
{
	Assert( DotProduct(vNormal,vVert) == 0 );
	Assert( DotProduct(vNormal,vHorz) == 0 );
	Assert( vNormal.Length() > 0.9999 && vNormal.Length() < 1.0001 );

	m_vPlaneOrigin = vOrigin;
	m_vPlaneNormal = vNormal;
	m_vPlaneHorz = vHorz;
	m_vPlaneVert = vVert;
}

unsigned int Tool3D::GetConstraints(unsigned int nKeyFlags)
{
	unsigned int uConstraints = 0;

	bool bDisableSnap = (GetKeyState(VK_MENU) & 0x8000)!=0;
	
	if ( !bDisableSnap )
	{
		uConstraints |= constrainSnap;

		if ( !m_pDocument->IsSnapEnabled() )
		{
			uConstraints |= constrainIntSnap;
		}
	}

	return uConstraints;
}

void Tool3D::ProjectOnTranslationPlane( const Vector &vWorld, Vector &vTransform, int nFlags )
{
	if ( !nFlags )
	{
		float frac = DotProduct( m_vPlaneNormal, m_vPlaneOrigin-vWorld );
		vTransform = vWorld + frac*m_vPlaneNormal;
	}
	else
	{
		Vector v0 = vWorld - m_vPlaneOrigin;
		Vector vOut;

		if ( !SolveLinearEquation( v0, m_vPlaneHorz, m_vPlaneVert, m_vPlaneNormal, vOut) )
		{
			vTransform.Init();
			return;
		}

		if ( nFlags & constrainOnlyHorz )
		{
			vOut.y = 0;
		}

		if ( nFlags & constrainOnlyVert )
		{
			vOut.x = 0;
		}

		if ( nFlags & constrainSnap )
		{
			if ( nFlags & constrainIntSnap )
			{
				// just snap to next integer
				vOut.x = V_rint(vOut.x);
				vOut.y = V_rint(vOut.y);
			}
			else 
			{
				// snap to user grid
				float flGridSpacing = m_pDocument->GetGridSpacing();

				if ( nFlags & constrainHalfSnap )
				{
					flGridSpacing *= 0.5f;
				}
				
				vOut.y = V_rint(vOut.y / flGridSpacing) * flGridSpacing;
				vOut.x = V_rint(vOut.x / flGridSpacing) * flGridSpacing;
			}
		}

		vTransform = m_vPlaneOrigin + vOut.x * m_vPlaneHorz + vOut.y * m_vPlaneVert;
	}
}

void Tool3D::ProjectTranslation( CMapView *pView, const Vector2D &vPoint, Vector &vTransform, int nFlags )
{
	Vector vStart, vEnd;

	pView->BuildRay( vPoint, vStart, vEnd );

	Vector vLine = vEnd-vStart;

	if ( !nFlags )
	{
		// simple plane & line intersection
		float d1 = DotProduct( m_vPlaneNormal, m_vPlaneOrigin-vStart );
		float d2 = DotProduct( m_vPlaneNormal, vLine );

		if ( d2 == 0 )
		{
			// line & plane are parallel !
			vTransform.Init();
			return;
		}

		vTransform = vStart + (d1/d2) *vLine;
		return;
	}

	Vector v0 = vStart - m_vPlaneOrigin;
	Vector vOut;
	
	if ( !SolveLinearEquation( v0, m_vPlaneHorz, m_vPlaneVert, -vLine, vOut) )
	{
		vTransform.Init();
		return;
	}

	if ( nFlags & constrainOnlyHorz )
	{
		vOut.y = 0;
	}

	if ( nFlags & constrainOnlyVert )
	{
		vOut.x = 0;
	}

	if ( nFlags & constrainSnap )
	{
		if ( nFlags & constrainIntSnap )
		{
			// just snap to next integer
			vOut.x = V_rint(vOut.x);
			vOut.y = V_rint(vOut.y);
		}
		else 
		{
			// snap to user grid
			float flGridSpacing = m_pDocument->GetGridSpacing();

			if ( nFlags & constrainHalfSnap )
			{
				flGridSpacing *= 0.5f;
			}

			vOut.y = V_rint(vOut.y / flGridSpacing) * flGridSpacing;
			vOut.x = V_rint(vOut.x / flGridSpacing) * flGridSpacing;
		}
	}
	
	vTransform = m_vPlaneOrigin + vOut.x * m_vPlaneHorz + vOut.y * m_vPlaneVert;
}

bool Tool3D::OnLMouseDown2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint )
{
	m_bMouseDown[MOUSE_LEFT] = true;
	m_bMouseDragged[MOUSE_LEFT] = false;
	m_vMousePos = m_vMouseStart[MOUSE_LEFT] = vPoint;
	pView->SetCapture();
	return true;
}

bool Tool3D::OnLMouseUp2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint )
{
	m_vMousePos = vPoint;
	m_bMouseDown[MOUSE_LEFT] = false;
	m_bMouseDragged[MOUSE_LEFT] = false;
	ReleaseCapture();
	return true;
}

bool Tool3D::OnRMouseDown2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint )
{
	m_bMouseDown[MOUSE_RIGHT] = true;
	m_bMouseDragged[MOUSE_RIGHT] = false;
	m_vMousePos = m_vMouseStart[MOUSE_RIGHT] = vPoint;
	pView->SetCapture();
	return true;
}

bool Tool3D::OnRMouseUp2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint )
{
	m_vMousePos = vPoint;
	m_bMouseDown[MOUSE_RIGHT] = false;
	m_bMouseDragged[MOUSE_RIGHT] = false;
	ReleaseCapture();
	return true;
}

bool Tool3D::OnMouseMove2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint )
{
	m_vMousePos = vPoint;

	for ( int i=0;i<2;i++)
	{
		if ( m_bMouseDown[i] )
		{
			if ( !m_bMouseDragged[i] )
			{
				// check if mouse was dragged if button is pressed down
				Vector2D sizeDragged = vPoint - m_vMouseStart[i];

				if ((abs(sizeDragged.x) > DRAG_THRESHHOLD) || (abs(sizeDragged.y) > DRAG_THRESHHOLD))
				{
					// If here, means we've dragged the mouse
					m_bMouseDragged[i] = true;
				}
			}

			// Make sure the point is visible.
			pView->ToolScrollToPoint( vPoint );
		}
	}

	return true;
}

bool Tool3D::OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	m_bMouseDown[MOUSE_LEFT] = true;
	m_bMouseDragged[MOUSE_LEFT] = false;
	m_vMousePos = m_vMouseStart[MOUSE_LEFT] = vPoint;
	return true;
}

bool Tool3D::OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	m_vMousePos = vPoint;
	m_bMouseDown[MOUSE_LEFT] = false;
	m_bMouseDragged[MOUSE_LEFT] = false;
	::ReleaseCapture();
	return true;
}

bool Tool3D::OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	m_bMouseDown[MOUSE_RIGHT] = true;
	m_bMouseDragged[MOUSE_RIGHT] = false;
	m_vMousePos = m_vMouseStart[MOUSE_RIGHT] = vPoint;
	return true;
}

bool Tool3D::OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	m_vMousePos = vPoint;
	m_bMouseDown[MOUSE_RIGHT] = false;
	m_bMouseDragged[MOUSE_RIGHT] = false;
	::ReleaseCapture();
	return true;
}

bool Tool3D::OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	m_vMousePos = vPoint;
	for ( int i=0;i<2;i++)
	{
		if ( m_bMouseDown[i] )
		{
			if ( !m_bMouseDragged[i] )
			{
				// check if mouse was dragged if button is pressed down
				Vector2D sizeDragged = vPoint - m_vMouseStart[i];

				if ((abs(sizeDragged.x) > DRAG_THRESHHOLD) || (abs(sizeDragged.y) > DRAG_THRESHHOLD))
				{
					// If here, means we've dragged the mouse
					m_bMouseDragged[i] = true;
				}
			}

			// Make sure the point is visible.
			// pView->ToolScrollToPoint( vPoint );
		}
	}

	pView->SetCursor( vgui::dc_arrow );

	return true;
}

void Tool3D::RenderTranslationPlane(CRender *pRender)
{
	pRender->PushRenderMode( RENDER_MODE_WIREFRAME );
	pRender->SetDrawColor( Color(128,128,128) );

	Vector viewPoint,vOffset;
	
	ProjectTranslation( pRender->GetView(), m_vMousePos, viewPoint, constrainSnap );

	float fGrid = m_pDocument->GetGridSpacing();
	int nSteps = 16;

	vOffset = m_vPlaneVert * (fGrid * nSteps);
	for (int h=-nSteps;h<=nSteps;h++)
	{
		Vector pos = viewPoint + ( m_vPlaneHorz * ( fGrid*h ) );
		pRender->DrawLine( pos+vOffset, pos-vOffset );
	}

	vOffset = m_vPlaneHorz * (fGrid * nSteps);
	for (int v=-nSteps;v<=nSteps;v++)
	{
		Vector pos = viewPoint + ( m_vPlaneVert * ( fGrid*v ) );
		pRender->DrawLine( pos+vOffset, pos-vOffset );
	}
	
	pRender->PopRenderMode();
}