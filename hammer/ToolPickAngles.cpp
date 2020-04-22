//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Tool used for point-and-click setting of angles.
//
//=============================================================================//

#include "stdafx.h"
#include "resource.h"
#include "ToolPickAngles.h"
#include "MapView3D.h"
#include "MapSolid.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Constructor. Inits data members.
//-----------------------------------------------------------------------------
CToolPickAngles::CToolPickAngles(void)
{
	m_pNotifyTarget = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CToolPickAngles::~CToolPickAngles(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse button up message in the 3D view.
// Input  : pView - The view that the event occurred in.
//			nFlags - Flags per the Windows mouse message.
//			point - Point in client coordinates where the event occurred.
// Output : Returns true if the message was handled by the tool, false if not.
//-----------------------------------------------------------------------------
bool CToolPickAngles::OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse button up message in the 3D view.
// Input  : pView - The view that the event occurred in.
//			nFlags - Flags per the Windows mouse message.
//			point - Point in client coordinates where the event occurred.
// Output : Returns true if the message was handled by the tool, false if not.
//-----------------------------------------------------------------------------
bool CToolPickAngles::OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	unsigned long ulFace;
	CMapClass *pObject = pView->NearestObjectAt( vPoint, ulFace);
	if (pObject != NULL)
	{
		CMapClass *pSelObject = pObject->PrepareSelection(selectObjects);
		CMapEntity *pEntity = dynamic_cast <CMapEntity *>(pSelObject);
		if (pEntity != NULL)
		{
			//
			// We clicked on an entity.
			//
			if (m_pNotifyTarget)
			{
				Vector vecCenter;
				pEntity->GetBoundsCenter(vecCenter);
				m_pNotifyTarget->OnNotifyPickAngles(vecCenter);
			}
		}
		else
		{
			CMapSolid *pSolid = dynamic_cast <CMapSolid *> (pObject);
			if (pSolid == NULL)
			{
				return true;
			}

			//
			// Build a ray to trace against the face that they clicked on to
			// find the point of intersection.
			//			
			Vector Start,End;
			pView->GetCamera()->BuildRay( vPoint, Start, End);

			Vector HitPos;
			Vector HitNormal;
			CMapFace *pFace = pSolid->GetFace(ulFace);
			if (pFace->TraceLine(HitPos, HitNormal, Start, End))
			{
				if (m_pNotifyTarget)
				{
					m_pNotifyTarget->OnNotifyPickAngles(HitPos);
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse button double click message in the 3D view.
// Input  : pView - The view that the event occurred in.
//			nFlags - Flags per the Windows mouse message.
//			point - Point in client coordinates where the event occurred.
// Output : Returns true if the message was handled by the tool, false if not.
//-----------------------------------------------------------------------------
bool CToolPickAngles::OnLMouseDblClk3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the right mouse button up message in the 3D view.
// Input  : pView - The view that the event occurred in.
//			nFlags - Flags per the Windows mouse message.
//			point - Point in client coordinates where the event occurred.
// Output : Returns true if the message was handled by the tool, false if not.
//-----------------------------------------------------------------------------
bool CToolPickAngles::OnRMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the mouse button up message in the 3D view.
// Input  : pView - The view that the event occurred in.
//			nFlags - Flags per the Windows mouse message.
//			point - Point in client coordinates where the event occurred.
// Output : Returns true if the message was handled by the tool, false if not.
//-----------------------------------------------------------------------------
bool CToolPickAngles::OnRMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the mouse move message in the 3D view.
// Input  : pView - The view that the event occurred in.
//			nFlags - Flags per the Windows mouse message.
//			point - Point in client coordinates where the event occurred.
// Output : Returns true if the message was handled by the tool, false if not.
//-----------------------------------------------------------------------------
bool CToolPickAngles::OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	SetToolCursor();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the cursor to the correct cursor for this tool.
//-----------------------------------------------------------------------------
void CToolPickAngles::SetToolCursor(void)
{
	static HCURSOR hcur = NULL;
	
	if (!hcur)
	{
		hcur = LoadCursor(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_CROSSHAIR));
	}
	
	SetCursor(hcur);
}


