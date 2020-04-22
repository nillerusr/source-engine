//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The document. Exposes functions for object creation, deletion, and
//			manipulation. Holds the current tool. Handles GUI messages that are
//			view-independent.
//
//=============================================================================//


#include "stdafx.h"
#include "Selection.h"
#include "mapdoc.h"
#include "MapHelper.h"
#include "MapSolid.h"
#include "Manifest.h"
#include "mapdefs.h"
#include "globalfunctions.h"
#include "mainfrm.h"
#include "objectproperties.h"

CSelection::CSelection(void)
{
	m_pDocument = NULL;
}

CSelection::~CSelection(void)
{

}

void CSelection::Init( CMapDoc *pDocument ) 
{
	m_pDocument = pDocument;
	m_eSelectMode = selectGroups;
	m_SelectionList.Purge();
	ClearHitList();

	m_LastValidBounds.bmins = Vector(0, 0, 0);
	m_LastValidBounds.bmaxs = Vector(64, 64, 64);

	UpdateSelectionBounds();
}

bool CSelection::IsSelected(CMapClass *pobj)
{
	return (m_SelectionList.Find(pobj) != m_SelectionList.InvalidIndex());
}


void CSelection::GetLastValidBounds(Vector &vecMins, Vector &vecMaxs)
{
	vecMins = m_LastValidBounds.bmins;
	vecMaxs = m_LastValidBounds.bmaxs;
}

bool CSelection::GetBounds(Vector &vecMins, Vector &vecMaxs)
{
	if ( m_bBoundsDirty )
		UpdateSelectionBounds();

	if ( m_SelectionList.Count() == 0)
		return false;

	vecMins = m_Bounds.bmins;
	vecMaxs	= m_Bounds.bmaxs;

	return true;;
}

bool CSelection::GetLogicalBounds(Vector2D &vecMins, Vector2D &vecMaxs)
{
	if ( m_bBoundsDirty )
		UpdateSelectionBounds();

	if ( m_SelectionList.Count() == 0)
		return false;

	vecMins = m_vecLogicalMins;
	vecMaxs = m_vecLogicalMaxs;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Used for translations. Uses entity origins and brush bounds. 
// That way, when moving stuff, the entity origins will stay on the grid.
//-----------------------------------------------------------------------------
void CSelection::GetBoundsForTranslation( Vector &vecMins, Vector &vecMaxs )
{
	vecMins.Init( COORD_NOTINIT, COORD_NOTINIT, 0 );
	vecMaxs.Init( -COORD_NOTINIT, -COORD_NOTINIT, 0 );

	for (int i = 0; i < m_SelectionList.Count(); i++)
	{
		CMapClass *pobj = m_SelectionList[i];

		// update physical bounds
		Vector mins, maxs;
		
		CEditGameClass *pEdit = dynamic_cast< CEditGameClass* >( pobj );
		if ( (pEdit && pEdit->IsSolidClass()) || dynamic_cast<CMapSolid *>(pobj) )
		{
			pobj->GetRender2DBox(mins, maxs);
		}
		else
		{
			pobj->GetOrigin( mins );
			maxs = mins;
		}		
		
		VectorMin( mins, vecMins, vecMins );
		VectorMax( maxs, vecMaxs, vecMaxs );
	}
}

void CSelection::UpdateSelectionBounds( void )
{
	m_Bounds.ResetBounds();
	
	m_vecLogicalMins[0] = m_vecLogicalMins[1] = COORD_NOTINIT;
	m_vecLogicalMaxs[0] = m_vecLogicalMaxs[1] = -COORD_NOTINIT;
	
	for (int i = 0; i < m_SelectionList.Count(); i++)
	{
		CMapClass *pobj = m_SelectionList[i];

		// update physical bounds
		Vector mins,maxs;
		pobj->GetRender2DBox(mins, maxs);
		m_Bounds.UpdateBounds(mins, maxs);

		// update logical bounds
		Vector2D logicalMins,logicalMaxs;
		pobj->GetRenderLogicalBox( logicalMins, logicalMaxs );
		Vector2DMin( logicalMins, m_vecLogicalMins, m_vecLogicalMins );
		Vector2DMax( logicalMaxs, m_vecLogicalMaxs, m_vecLogicalMaxs );
	}

	// remeber bounds if valid
	if ( m_Bounds.IsValidBox() )
	{
		m_LastValidBounds = m_Bounds;
	}

	m_bBoundsDirty = false;
}

bool CSelection::GetBoundsCenter(Vector &vecCenter)
{
	if ( m_bBoundsDirty )
		UpdateSelectionBounds();

	if ( m_SelectionList.Count() == 0 )
		return false;

	m_Bounds.GetBoundsCenter( vecCenter );

	return true;
}

bool CSelection::GetLogicalBoundsCenter( Vector2D &vecCenter )
{
	if ( m_bBoundsDirty )
		UpdateSelectionBounds();

	if ( m_SelectionList.Count() == 0 )
		return false;

	vecCenter = (m_vecLogicalMins+m_vecLogicalMaxs)/2;

	return true;
}

bool CSelection::IsEmpty()
{
	return m_SelectionList.Count() == 0;
}

const CMapObjectList *CSelection::GetList()
{
	return &m_SelectionList;
}

const CMapObjectList* CSelection::GetHitList()
{
	return &m_HitList;
}

int CSelection::GetCount()
{
	return m_SelectionList.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current selection mode. The selection mode determines
//			what gets selected when the user clicks on something - the group,
//			the entity, or the solid.
//-----------------------------------------------------------------------------
SelectMode_t CSelection::GetMode()
{
	return m_eSelectMode;
}

void CSelection::SetSelectionState(SelectionState_t eSelectionState)
{
	for ( int i=0; i<m_SelectionList.Count(); i++ )
	{
		CMapEntity *pObject = (CMapEntity *)m_SelectionList.Element(i);
		pObject->SetSelectionState( eSelectionState );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSelection::IsAnEntitySelected(void)
{
	if (m_SelectionList.Count() > 0)
	{
		int nSelCount = m_SelectionList.Count();
		for (int i = 0; i < nSelCount; i++)
		{
			CMapClass *pObject = m_SelectionList.Element(i);
			CMapEntity *pEntity = dynamic_cast <CMapEntity *> (pObject);
			if (pEntity != NULL)
			{
				return true;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the selection is editable.  Every object must be
//			individually editable for this routine to return true.
//-----------------------------------------------------------------------------
bool CSelection::IsEditable()
{
	if ( m_SelectionList.Count() > 0 )
	{
		int nSelCount = m_SelectionList.Count();
		for (int i = 0; i < nSelCount; i++)
		{
			CMapClass *pObject = m_SelectionList.Element(i);

			if ( pObject->IsEditable() == false )
			{
				return false;
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the selection is copyable.  CManifestInstance classes
//			are not copyable.
//-----------------------------------------------------------------------------
bool CSelection::IsCopyable()
{
	if ( m_SelectionList.Count() > 0 )
	{
		int nSelCount = m_SelectionList.Count();
		for (int i = 0; i < nSelCount; i++)
		{
			CMapClass *pObject = m_SelectionList.Element(i);

			if ( pObject->IsMapClass( MAPCLASS_TYPE( CManifestInstance ) ) )
			{
				return false;
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the current selection mode, which determines which objects
//			are selected when the user clicks on things.
//-----------------------------------------------------------------------------
void CSelection::SetMode(SelectMode_t eNewSelectMode)
{
	SelectMode_t eOldSelectMode = m_eSelectMode;
	m_eSelectMode = eNewSelectMode;
	
	if ((eOldSelectMode == selectSolids) ||
		((eOldSelectMode == selectObjects) && (eNewSelectMode == selectGroups)))
	{
		//
		// If we are going from a more specific selection mode to a less specific one,
		// clear the selection. This avoids unexpectedly selecting new things.
		//
		SelectObject(NULL, scClear|scSaveChanges);
	}
	else
	{
		//
		// Put all the children of the selected objects in a list, along with their children.
		//
		CMapObjectList NewList;
		int nSelCount = m_SelectionList.Count();
		for (int i = 0; i < nSelCount; i++)
		{
			CMapClass *pObject = m_SelectionList[i];
			AddLeavesToListCallback(pObject, &NewList);
			pObject->EnumChildren((ENUMMAPCHILDRENPROC)AddLeavesToListCallback, (DWORD)&NewList);
		}

		SelectObject(NULL, scClear|scSaveChanges);

		//
		// Add child objects to selection.
		//
		for (int pos=0;pos<NewList.Count();pos++)
		{
			CMapClass *pObject = NewList[pos];
			CMapClass *pSelObject = pObject->PrepareSelection(eNewSelectMode);
			if (pSelObject)
			{
				SelectObject(pSelObject, scSelect);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void CSelection::AddHit(CMapClass *pObject)
{
	if ( m_HitList.Find(pObject) == -1 )
	{
		m_HitList.AddToTail(pObject);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSelection::ClearHitList(void)
{
	m_HitList.RemoveAll();
	m_iCurHit = -1;
}

bool CSelection::RemoveAll(void)
{
	for ( int i=0;i<m_SelectionList.Count(); i++ )
	{
		CMapClass *pObject = m_SelectionList.Element(i);
		pObject->SetSelectionState(SELECT_NONE);
	} 

	m_SelectionList.RemoveAll();
	SetBoundsDirty();

	return true;
}

bool CSelection::RemoveDead(void)
{
	bool bFoundOne = false;
	
	for ( int i=m_SelectionList.Count()-1; i>=0; i-- )
	{
		CMapClass *pObject = m_SelectionList.Element(i);
		if (!pObject->GetParent())
		{
			m_SelectionList.FastRemove(i);
			pObject->SetSelectionState(SELECT_NONE);
			bFoundOne = true;
		}
	} 

	// TODO check if we do the same as in SelectObject
	SetBoundsDirty();

	return bFoundOne;
}

//-----------------------------------------------------------------------------
// Purpose: Removes objects that are not visible from the selection set.
//-----------------------------------------------------------------------------
bool CSelection::RemoveInvisibles(void)
{
	bool bFoundOne = false;

	for ( int i=m_SelectionList.Count()-1; i>=0; i-- )
	{
		CMapClass *pObject = m_SelectionList.Element(i);
		if ( !pObject->IsVisible() )
		{
			m_SelectionList.FastRemove(i);
			pObject->SetSelectionState(SELECT_NONE);
			bFoundOne = true;
		}
	} 

	SetBoundsDirty();

	return bFoundOne;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iIndex - 
//			bUpdateViews - 
//-----------------------------------------------------------------------------
void CSelection::SetCurrentHit(int iIndex, bool bCascading)
{
	if ( m_HitList.Count() == 0)
	{
		Assert( m_iCurHit == -1);
		return;
	}

	// save & toggle old selection off
	if (m_iCurHit != -1)
	{
		CMapClass *pObject =  m_HitList[m_iCurHit];
		SelectObject(pObject, scToggle|scSaveChanges);
	}

	if (iIndex == hitNext)
	{
		// hit next object
		m_iCurHit++;
	}
	else if (iIndex == hitPrev)
	{
		// hit prev object
		m_iCurHit--;
	}
	else
	{
		m_iCurHit = iIndex;
	}

	// make sure curhit is valid
	if (m_iCurHit >= m_HitList.Count())
	{
		m_iCurHit = 0;
	}
	else if (m_iCurHit < 0)
	{
		m_iCurHit = m_HitList.Count() - 1;
	}

	CMapClass *pObject = m_HitList[m_iCurHit];

	if ( bCascading )
	{
		// Build actual selection list based on cascading...
		CUtlRBTree< CMapClass*, unsigned short > tree( 0, 0, DefLessFunc( CMapClass* ) );
				
		bool bRecursive = false; // not used yet
		m_pDocument->BuildCascadingSelectionList( pObject, tree, bRecursive );
				
		CMapObjectList list;
		list.AddToTail( pObject );
		bool bRootIsSelected = IsSelected(pObject);
		bool bUniformSelectionState = true;
		
		for ( unsigned short h = tree.FirstInorder(); h != tree.InvalidIndex(); h = tree.NextInorder(h) )
		{
			list.AddToTail( list[h] );
			
			if ( IsSelected( list[h] ) != bRootIsSelected )
			{
				bUniformSelectionState = false;
			}
		}

		/* Change toggle to select or unselect if we're toggling and cascading
		// but the root + children have different selection state 
		if ( ( !bUniformSelectionState ) && ( cmd == scToggle ) )
		{
			cmd = bRootIsSelected ? scSelect : scUnselect;
		}*/

		SelectObjectList( &list, scSelect );
	}
	else
	{
		SelectObject(pObject, scToggle );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pobj - 
//			cmd - 
//-----------------------------------------------------------------------------
bool CSelection::SelectObject(CMapClass *pObj, int cmd)
{
	// if no object is given we only can execute the clear command
	if ( pObj == NULL )
	{
		// check if selection is already empty
		if (m_SelectionList.Count() == 0) 
			return false; // nothing to do

		if ( cmd & scClear )
		{
			RemoveAll();
		}
	}
	else // object oriented operation
	{
		int iIndex = m_SelectionList.Find(pObj);
		bool bAlreadySelected = iIndex != -1;
	
		if ( cmd & scToggle )
		{
			if ( bAlreadySelected )
				cmd |= scUnselect;
			else
				cmd |= scSelect;
		}

		if ( cmd & scSelect )
		{
			if ( cmd & scClear )
			{
				// if we re-selected the only selected element, nothing changes
				if ( bAlreadySelected && m_SelectionList.Count() == 1 )
					return false;

				RemoveAll();
				bAlreadySelected = false; // reset that flag
			}

			if ( bAlreadySelected )
				return false;
			
			m_SelectionList.AddToTail(pObj);
			pObj->SetSelectionState(SELECT_NORMAL);
		}
		else if ( (cmd & scUnselect) && bAlreadySelected )
		{
			// ok unselect an yet selected object
			m_SelectionList.Remove(iIndex);
			pObj->SetSelectionState(SELECT_NONE);
		}
		else
		{
			return false; // nothing was changed
		}
	}

	// ok something in the selection was changed, set dirty flags
	SetBoundsDirty();

	if ( cmd & scSaveChanges )
	{
		// changing the selection automatically saves changes made to the properties dialog
		GetMainWnd()->pObjectProperties->SaveData();
	}

	// always mark data dirty
	GetMainWnd()->pObjectProperties->MarkDataDirty();

	// uddate all views
	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_SELECTION );
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Clears the current selection and selects everything in the given list.
// Input  : pList - Objects to select.
//-----------------------------------------------------------------------------
void CSelection::SelectObjectList( const CMapObjectList *pList, int cmd )
{
	// Clear the current selection.

	// Clear the current selection.
	if ( cmd & scSaveChanges )
	{
		GetMainWnd()->pObjectProperties->SaveData();
		cmd &= ~scSaveChanges;
	}

	if ( cmd & scClear )
	{
		RemoveAll();
		cmd &= ~scClear;
	}

	if ( pList != NULL )
	{
		for (int pos=0;pos<pList->Count();pos++)
		{
			CMapClass *pObject = pList->Element(pos);
			CMapClass *pSelObject = pObject->PrepareSelection( m_eSelectMode );
			if (pSelObject)
			{
				SelectObject( pSelObject, cmd );
			}
		}
	}
}
