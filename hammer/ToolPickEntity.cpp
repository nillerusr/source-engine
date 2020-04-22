//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Tool used for picking entities. This is used anywhere a field that
//			contains an entity name is found.
//
//			Left click - single select entity
//				+Ctrl - multiselect a single entity
//
//=============================================================================//

#include "stdafx.h"
#include "resource.h"
#include "ToolPickEntity.h"
#include "MapViewLogical.h"
#include "MapView3D.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Constructor. Inits data members.
//-----------------------------------------------------------------------------
CToolPickEntity::CToolPickEntity(void)
{
	m_pNotifyTarget = NULL;
	m_bAllowMultiSelect = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CToolPickEntity::~CToolPickEntity(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables multiselect for this tool.
//-----------------------------------------------------------------------------
void CToolPickEntity::AllowMultiSelect(bool bAllow)
{
	m_bAllowMultiSelect = bAllow;

	//
	// Shouldn't ever happen, but you never know.
	//
	if ((!bAllow) && (m_Entities.Count() > 1))
	{
		CMapEntity *pEntity = m_Entities[0].pEntity;
		DeselectAll();
		SelectEntity(pEntity);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when the tool is deactivated.
// Input  : eNewTool - The ID of the tool that is being activated.
//-----------------------------------------------------------------------------
void CToolPickEntity::OnDeactivate()
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
bool CToolPickEntity::OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
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
bool CToolPickEntity::OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	bool bControl = ((nFlags & MK_CONTROL) != 0);

	unsigned long uEntity;
	CMapClass *pObject = pView->NearestObjectAt( vPoint, uEntity);
	if (pObject != NULL)
	{
		CMapClass *pSelObject = pObject->PrepareSelection(selectObjects);
		CMapEntity *pEntity = dynamic_cast <CMapEntity *>(pSelObject);
		if (pEntity != NULL)
		{
			//
			// We clicked on an entity.
			//
			if ((!m_bAllowMultiSelect) || (!bControl))
			{
				// Single select.
				DeselectAll();
				SelectEntity(pEntity);
			}
			else
			{
				// Multiselect.
				CycleSelectEntity(pEntity);
			}

			if (m_pNotifyTarget)
			{
				m_pNotifyTarget->OnNotifyPickEntity(this);
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse button up message in the 3D view.
// Input  : pView - The view that the event occurred in.
//			nFlags - Flags per the Windows mouse message.
//			point - Point in client coordinates where the event occurred.
// Output : Returns true if the message was handled by the tool, false if not.
//-----------------------------------------------------------------------------
bool CToolPickEntity::OnLMouseDownLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint)
{
	bool bControl = ((nFlags & MK_CONTROL) != 0);

	HitInfo_t hitData;
	int nHits = pView->ObjectsAt( vPoint, &hitData, 1 );
	if ( ( nHits > 0 ) && hitData.pObject )
	{
		CMapClass *pSelObject = hitData.pObject->PrepareSelection( selectObjects );
		CMapEntity *pEntity = dynamic_cast <CMapEntity *>( pSelObject );
		if (pEntity != NULL)
		{
			//
			// We clicked on an entity.
			//
			if ((!m_bAllowMultiSelect) || (!bControl))
			{
				// Single select.
				DeselectAll();
				SelectEntity(pEntity);
			}
			else
			{
				// Multiselect.
				CycleSelectEntity(pEntity);
			}

			if (m_pNotifyTarget)
			{
				m_pNotifyTarget->OnNotifyPickEntity(this);
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
bool CToolPickEntity::OnLMouseDblClk3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
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
bool CToolPickEntity::OnRMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
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
bool CToolPickEntity::OnRMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
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
bool CToolPickEntity::OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	SetEyedropperCursor();
	return true;
}


bool CToolPickEntity::OnMouseMoveLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint)
{
	SetEyedropperCursor();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the cursor to the eyedropper cursor.
//-----------------------------------------------------------------------------
void CToolPickEntity::SetEyedropperCursor(void)
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
// Input  : pEntity - 
//-----------------------------------------------------------------------------
void CToolPickEntity::CycleSelectEntity(CMapEntity *pEntity)
{
	int nIndex = FindEntity(pEntity);
	if (nIndex != -1)
	{
		//
		// The entity is in our list, update its selection state.
		//
		if (m_Entities[nIndex].eState == EntityState_Partial)
		{
			DeselectEntity(nIndex);
		}
		else if (m_Entities[nIndex].eState == EntityState_Select)
		{
			if (m_Entities[nIndex].eOriginalState == EntityState_Partial)
			{
				m_Entities[nIndex].eState = EntityState_Partial;
				//pEntity->SetSelectionState(SELECT_MULTI_PARTIAL);
			}
			else
			{
				DeselectEntity(nIndex);
			}
		}
		else if (m_Entities[nIndex].eState == EntityState_None)
		{
			m_Entities[nIndex].eState = EntityState_Select;
			//pEntity->SetSelectionState(SELECT_NORMAL);
		}
	}
	else
	{
		//
		// The entity is not in our list, add it.
		//
		AddToList(pEntity, EntityState_Select);
		//pEntity->SetSelectionState(SELECT_NORMAL);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the fully selected and partially selected entities for the picker.
// Input  : EntityListFull - 
//			EntityListPartial - 
//-----------------------------------------------------------------------------
void CToolPickEntity::SetSelectedEntities(CMapEntityList &EntityListFull, CMapEntityList &EntityListPartial)
{
	m_Entities.RemoveAll();

	for (int i = 0; i < EntityListFull.Count(); i++)
	{
		CMapEntity *pEntity = EntityListFull.Element(i);

		AddToList(pEntity, EntityState_Select);
		pEntity->SetSelectionState(SELECT_NORMAL);
	}

	for (int i = 0; i < EntityListPartial.Count(); i++)
	{
		CMapEntity *pEntity = EntityListPartial.Element(i);

		AddToList(pEntity, EntityState_Partial);
		pEntity->SetSelectionState(SELECT_MULTI_PARTIAL);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEntity - 
//-----------------------------------------------------------------------------
int CToolPickEntity::FindEntity(CMapEntity *pEntity)
{
	for (int i = 0; i < m_Entities.Count(); i++)
	{
		if (m_Entities[i].pEntity == pEntity)
		{
			return i;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: Clears the selection set.
//-----------------------------------------------------------------------------
void CToolPickEntity::DeselectAll(void)
{
	for (int i = 0; i < m_Entities.Count(); i++)
	{
		m_Entities[i].pEntity->SetSelectionState(SELECT_NONE);
	}

	m_Entities.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Selects the entity unconditionally.
// Input  : pEntity - entity to select.
//-----------------------------------------------------------------------------
void CToolPickEntity::SelectEntity(CMapEntity *pEntity)
{
	int nIndex = FindEntity(pEntity);
	if (nIndex != -1)
	{
		//
		// The entity is in our list, update its selection state.
		//
		m_Entities[nIndex].eState = EntityState_Select;
		pEntity->SetSelectionState(SELECT_NORMAL);
	}
	else
	{
		//
		// The entity is not in our list, add it.
		//
		AddToList(pEntity, EntityState_Select);
		pEntity->SetSelectionState(SELECT_NORMAL);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Deselects the given entity.
//-----------------------------------------------------------------------------
void CToolPickEntity::DeselectEntity(CMapEntity *pEntity)
{
	int nIndex = FindEntity(pEntity);
	if (nIndex != -1)
	{
		DeselectEntity(nIndex);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes the entity at the given index from the selection set.
// Input  : nIndex - Index of the entity in the selection list.
//-----------------------------------------------------------------------------
void CToolPickEntity::DeselectEntity(int nIndex)
{
	Assert(m_Entities.IsValidIndex(nIndex));

	if (m_Entities.IsValidIndex(nIndex))
	{
		if (m_Entities[nIndex].eOriginalState != EntityState_Partial)
		{
			m_Entities[nIndex].pEntity->SetSelectionState(SELECT_NONE);

			//
			// Remove the entity from our list.
			//
			RemoveFromList(nIndex);
		}
		else
		{
			//
			// Just set the state to deselected so we can cycle back to partial selection.
			//
			m_Entities[nIndex].pEntity->SetSelectionState(SELECT_NONE);
			m_Entities[nIndex].eState = EntityState_None;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEntity - 
//			eState - 
//-----------------------------------------------------------------------------
void CToolPickEntity::AddToList(CMapEntity *pEntity, EntityState_t eState)
{
	int nIndex = m_Entities.AddToTail();
	m_Entities[nIndex].pEntity = pEntity;
	m_Entities[nIndex].eState = eState;
	m_Entities[nIndex].eOriginalState = eState;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
//-----------------------------------------------------------------------------
void CToolPickEntity::RemoveFromList(int nIndex)
{
	Assert(m_Entities.IsValidIndex(nIndex));

	if (m_Entities.IsValidIndex(nIndex))
	{
		m_Entities.FastRemove(nIndex);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns lists of fully selected and partially selected entities.
//-----------------------------------------------------------------------------
void CToolPickEntity::GetSelectedEntities(CMapEntityList &EntityListFull, CMapEntityList &EntityListPartial)
{
	EntityListFull.RemoveAll();
	EntityListPartial.RemoveAll();

	for (int i = 0; i < m_Entities.Count(); i++)
	{
		if (m_Entities[i].eState == EntityState_Select)
		{
			EntityListFull.AddToTail(m_Entities[i].pEntity);
		}
		else if (m_Entities[i].eState == EntityState_Partial)
		{
			EntityListPartial.AddToTail(m_Entities[i].pEntity);
		}
	}
}

