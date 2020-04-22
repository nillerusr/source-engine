//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Tool used for picking brush faces.
//
//			Left click - single select face
//				+Ctrl - multiselect a single face
//				+Shift - all faces on the brush
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "MapFace.h"
#include "resource.h"
#include "ToolPickFace.h"
#include "MapSolid.h"
#include "MapView3D.h"
#include "mapdoc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Constructor. Inits data members.
//-----------------------------------------------------------------------------
CToolPickFace::CToolPickFace(void)
{
	m_pNotifyTarget = NULL;
	m_bAllowMultiSelect = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CToolPickFace::~CToolPickFace(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables multiselect for this tool.
//-----------------------------------------------------------------------------
void CToolPickFace::AllowMultiSelect(bool bAllow)
{
	m_bAllowMultiSelect = bAllow;

	//
	// Shouldn't ever happen, but you never know.
	//
	if ((!bAllow) && (m_Faces.Count() > 1))
	{
		CMapFace *pFace = m_Faces[0].pFace;
		DeselectAll();
		SelectFace(pFace);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when the tool is deactivated.
// Input  : eNewTool - The ID of the tool that is being activated.
//-----------------------------------------------------------------------------
void CToolPickFace::OnDeactivate()
{
	DeselectAll();
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse button up message in the 3D view.
// Input  : pView - The view that the event occurred in.
//			nFlags - Flags per the Windows mouse message.
//			point - Point in client coordinates where the event occurred.
// Output : Returns true if the message was handled by the tool, false if not.
//-----------------------------------------------------------------------------
bool CToolPickFace::OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
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
bool CToolPickFace::OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	bool bControl = ((nFlags & MK_CONTROL) != 0);
	bool bShift = ((nFlags & MK_SHIFT) != 0);

	if (!m_bAllowMultiSelect)
	{
		// Ignore shift click for single selection mode.
		bShift = false;
	}

	unsigned long uFace;
	CMapClass *pObject = pView->NearestObjectAt( vPoint, uFace);
	if (pObject != NULL)
	{
		CMapSolid *pSolid = dynamic_cast <CMapSolid *>(pObject);
		if (pSolid != NULL)
		{
			//
			// We clicked on a solid.
			//
			if (!bShift)
			{
				//
				// Get the face that we clicked on.
				//
				CMapFace *pFace = pSolid->GetFace(uFace);
				Assert(pFace != NULL);

				if (pFace != NULL)
				{
					if ((!m_bAllowMultiSelect) || (!bControl))
					{
						// Single select.
						DeselectAll();
						SelectFace(pFace);
					}
					else
					{
						// Multiselect.
						CycleSelectFace(pFace);
					}
				}
			}
			else
			{
				//
				// Holding down shift. Select or deselect all the faces on the solid.
				// Only deselect if all the faces in the solid were selected.
				//
				bool bAllSelected = true;
				int nFaceCount = pSolid->GetFaceCount();
				for (int nFace = 0; nFace < nFaceCount; nFace++)
				{
					CMapFace *pFace = pSolid->GetFace(nFace);
					int nIndex = FindFace(pFace);
					if ((nIndex == -1) || (m_Faces[nIndex].eState != FaceState_Select))
					{
						bAllSelected = false;
						break;
					}
				}

				if (!bControl)
				{
					DeselectAll();
				}

				nFaceCount = pSolid->GetFaceCount();
				for (int nFace = 0; nFace < nFaceCount; nFace++)
				{
					CMapFace *pFace = pSolid->GetFace(nFace);
					if (bAllSelected)
					{
						DeselectFace(pFace);
					}
					else
					{
						SelectFace(pFace);
					}
				}
			}

			if (m_pNotifyTarget)
			{
				m_pNotifyTarget->OnNotifyPickFace(this);
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
bool CToolPickFace::OnLMouseDblClk3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
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
bool CToolPickFace::OnRMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
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
bool CToolPickFace::OnRMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
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
bool CToolPickFace::OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	SetEyedropperCursor();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the cursor to the eyedropper cursor.
//-----------------------------------------------------------------------------
void CToolPickFace::SetEyedropperCursor(void)
{
	static HCURSOR hcurEyedropper = NULL;
	
	if (!hcurEyedropper)
	{
		hcurEyedropper = LoadCursor(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_EYEDROPPER));
	}
	
	SetCursor(hcurEyedropper);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFace - 
//-----------------------------------------------------------------------------
void CToolPickFace::CycleSelectFace(CMapFace *pFace)
{
	int nIndex = FindFace(pFace);
	if (nIndex != -1)
	{
		//
		// The face is in our list, update its selection state.
		//
		if (m_Faces[nIndex].eState == FaceState_Partial)
		{
			DeselectFace(nIndex);
		}
		else if (m_Faces[nIndex].eState == FaceState_Select)
		{
			if (m_Faces[nIndex].eOriginalState == FaceState_Partial)
			{
				m_Faces[nIndex].eState = FaceState_Partial;
				pFace->SetSelectionState(SELECT_MULTI_PARTIAL);
			}
			else
			{
				DeselectFace(nIndex);
			}
		}
		else if (m_Faces[nIndex].eState == FaceState_None)
		{
			m_Faces[nIndex].eState = FaceState_Select;
			pFace->SetSelectionState(SELECT_NORMAL);
		}
	}
	else
	{
		//
		// The face is not in our list, add it.
		//
		AddToList(pFace, FaceState_Select);
		pFace->SetSelectionState(SELECT_NORMAL);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the fully selected and partially selected faces for the picker.
// Input  : FaceListFull - 
//			FaceListPartial - 
//-----------------------------------------------------------------------------
void CToolPickFace::SetSelectedFaces(CMapFaceList &FaceListFull, CMapFaceList &FaceListPartial)
{
	m_Faces.RemoveAll();

	for (int i = 0; i < FaceListFull.Count(); i++)
	{
		CMapFace *pFace = FaceListFull.Element(i);

		AddToList(pFace, FaceState_Select);
		pFace->SetSelectionState(SELECT_NORMAL);
	}

	for (int i = 0; i < FaceListPartial.Count(); i++)
	{
		CMapFace *pFace = FaceListPartial.Element(i);

		AddToList(pFace, FaceState_Partial);
		pFace->SetSelectionState(SELECT_MULTI_PARTIAL);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFace - 
//-----------------------------------------------------------------------------
int CToolPickFace::FindFace(CMapFace *pFace)
{
	for (int i = 0; i < m_Faces.Count(); i++)
	{
		if (m_Faces[i].pFace == pFace)
		{
			return i;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: Clears the selection set.
//-----------------------------------------------------------------------------
void CToolPickFace::DeselectAll(void)
{
	for (int i = 0; i < m_Faces.Count(); i++)
	{
		m_Faces[i].pFace->SetSelectionState(SELECT_NONE);
	}

	m_Faces.RemoveAll();

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
}


//-----------------------------------------------------------------------------
// Purpose: Selects the face unconditionally.
// Input  : pFace - face to select.
//-----------------------------------------------------------------------------
void CToolPickFace::SelectFace(CMapFace *pFace)
{
	int nIndex = FindFace(pFace);
	if (nIndex != -1)
	{
		//
		// The face is in our list, update its selection state.
		//
		m_Faces[nIndex].eState = FaceState_Select;
		pFace->SetSelectionState(SELECT_NORMAL);
	}
	else
	{
		//
		// The face is not in our list, add it.
		//
		AddToList(pFace, FaceState_Select);
		pFace->SetSelectionState(SELECT_NORMAL);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Deselects the given face.
//-----------------------------------------------------------------------------
void CToolPickFace::DeselectFace(CMapFace *pFace)
{
	int nIndex = FindFace(pFace);
	if (nIndex != -1)
	{
		DeselectFace(nIndex);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes the face at the given index from the selection set.
// Input  : nIndex - Index of the face in the selection list.
//-----------------------------------------------------------------------------
void CToolPickFace::DeselectFace(int nIndex)
{
	Assert(m_Faces.IsValidIndex(nIndex));

	if (m_Faces.IsValidIndex(nIndex))
	{
		if (m_Faces[nIndex].eOriginalState != FaceState_Partial)
		{
			m_Faces[nIndex].pFace->SetSelectionState(SELECT_NONE);

			//
			// Remove the face from our list.
			//
			RemoveFromList(nIndex);
		}
		else
		{
			//
			// Just set the state to deselected so we can cycle back to partial selection.
			//
			m_Faces[nIndex].pFace->SetSelectionState(SELECT_NONE);
			m_Faces[nIndex].eState = FaceState_None;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFace - 
//			eState - 
//-----------------------------------------------------------------------------
void CToolPickFace::AddToList(CMapFace *pFace, FaceState_t eState)
{
	int nIndex = m_Faces.AddToTail();
	m_Faces[nIndex].pFace = pFace;
	m_Faces[nIndex].eState = eState;
	m_Faces[nIndex].eOriginalState = eState;

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
//-----------------------------------------------------------------------------
void CToolPickFace::RemoveFromList(int nIndex)
{
	Assert(m_Faces.IsValidIndex(nIndex));

	if (m_Faces.IsValidIndex(nIndex))
	{
		m_Faces.FastRemove(nIndex);

		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns lists of fully selected and partially selected faces.
//-----------------------------------------------------------------------------
void CToolPickFace::GetSelectedFaces(CMapFaceList &FaceListFull, CMapFaceList &FaceListPartial)
{
	FaceListFull.RemoveAll();
	FaceListPartial.RemoveAll();

	for (int i = 0; i < m_Faces.Count(); i++)
	{
		if (m_Faces[i].eState == FaceState_Select)
		{
			FaceListFull.AddToTail(m_Faces[i].pFace);
		}
		else if (m_Faces[i].eState == FaceState_Partial)
		{
			FaceListPartial.AddToTail(m_Faces[i].pFace);
		}
	}
}

