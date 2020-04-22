//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the Undo/Redo system.
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "History.h"
#include "hammer.h"
#include "Options.h"
#include "MainFrm.h"
#include "MapDoc.h"
#include "GlobalFunctions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static CHistory *pCurHistory;	// The Undo/Redo history associated with the active doc.
static CHistory FakeHistory;	// Used when there is no active doc. Always paused.


//-----------------------------------------------------------------------------
// Purpose: Returns the current active Undo/Redo history.
//-----------------------------------------------------------------------------
CHistory *GetHistory(void)
{
	if (!pCurHistory)
	{
		return(&FakeHistory);
	}

	return(pCurHistory);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CHistory::CHistory(void)
{
	static BOOL bFirst = TRUE;	// fake history is always first
	Opposite = NULL;
	CurTrack = NULL;
	bPaused = bFirst ? 2 : FALSE;	// if 2, never unpaused
	bFirst = FALSE;
	m_bActive = TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CHistory::~CHistory()
{
	Tracks.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bUndo - 
//			pOpposite - 
//-----------------------------------------------------------------------------
void CHistory::SetOpposite(BOOL bUndo_, CHistory *pOpposite)
{
	this->bUndo = bUndo_;
	Opposite = pOpposite;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CHistory::IsUndoable()
{
	// return status flag depending on the current track
	return (CurTrack && m_bActive) ? TRUE : FALSE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bActive - 
//-----------------------------------------------------------------------------
void CHistory::SetActive(BOOL bActive)
{
	m_bActive = bActive;
	if (!m_bActive)
	{
		// kill all tracks right now
		FOR_EACH_OBJ( Tracks, pos )
		{
			CHistoryTrack *pTrack = Tracks.Element(pos);
			delete pTrack;
		}

		Tracks.RemoveAll();
		MarkUndoPosition();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Actually, this implements both Undo and Redo, because a Redo is just
//			an Undo in the opposite history track. 
// Input  : pNewSelection - List to populate with the new selection set after the Undo.
//-----------------------------------------------------------------------------
void CHistory::Undo(CMapObjectList *pNewSelection)
{
	Opposite->MarkUndoPosition(&CurTrack->Selected, GetCurTrackName(), TRUE);

	//
	// Track entries are consumed LIFO.
	//
	int pos = Tracks.Count()-1;
	Tracks.Remove(pos);

	//
	// Perform the undo.
	//
	Pause();
	CurTrack->Undo();
	Resume();

	//
	// Get the objects that should be selected from the track entry.
	// 
	pNewSelection->RemoveAll();
	pNewSelection->AddVectorToTail(CurTrack->Selected);

	//
	// Done with this track entry. This track entry will be recreated by the
	// opposite history track if necessary.
	//
	uDataSize -= CurTrack->uDataSize;
	delete CurTrack;

	//
	// Move to the previous track entry.
	//
	if ( Tracks.Count() > 0 )
	{
		CurTrack = Tracks.Element(Tracks.Count()-1);
	}
	else
	{
		CurTrack = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSelection - 
//			pszName - 
//			bFromOpposite - 
//-----------------------------------------------------------------------------
void CHistory::MarkUndoPosition( const CMapObjectList *pSelection, LPCTSTR pszName, BOOL bFromOpposite)
{
	if(Opposite && bUndo && !bFromOpposite)
	{
		// this is the undo tracker and the call is NOT from the redo
		// tracker. kill the redo tracker's history.
		FOR_EACH_OBJ( Opposite->Tracks, pos )
		{
			CHistoryTrack *pTrack = Opposite->Tracks.Element(pos);
			pTrack->m_bAutoDestruct = true;
			delete pTrack;

		}

		Opposite->Tracks.RemoveAll();
		Opposite->CurTrack = NULL;
	}

	// create a new track
	CurTrack = new CHistoryTrack(this, pSelection);
	Tracks.AddToTail(CurTrack);
	CurTrack->SetName(pszName);

	// check # of undo levels ..
	if(Tracks.Count() > Options.general.iUndoLevels)
	{
		// remove some.
		int i, i2;
		i = i2 = Tracks.Count() - Options.general.iUndoLevels;
		int pos = 0;
		while(i--)
		{
			CHistoryTrack *pTrack = Tracks.Element(pos); pos++;
			if(pTrack == CurTrack)
			{
				i2 -= (i2 - i);
				break;	// safeguard
			}
			delete pTrack;

		}
		// delete them from the list now
		while(i2--)
		{
			Tracks.Remove(0);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Keeps an object, so changes to it can be undone.
// Input  : pObject - Object to keep.
//-----------------------------------------------------------------------------
void CHistory::Keep(CMapClass *pObject)
{
	if (CurTrack == NULL)
	{
		MarkUndoPosition();
	}

	CurTrack->Keep(pObject, true);
	
	//
	// Keep this object's children.
	//
	EnumChildrenPos_t pos;
	CMapClass *pChild = pObject->GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		CurTrack->Keep(pChild, true);
		pChild = pObject->GetNextDescendent(pos);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Keeps an object, so changes to it can be undone.
// Input  : pObject - Object to keep.
//-----------------------------------------------------------------------------
void CHistory::KeepNoChildren(CMapClass *pObject)
{
	if (CurTrack == NULL)
	{
		MarkUndoPosition();
	}

	CurTrack->Keep(pObject, false);
}


//-----------------------------------------------------------------------------
// Purpose: Keeps a list of objects, so changes to them can be undone.
// Input  : pList - List of objects to keep.
//-----------------------------------------------------------------------------
void CHistory::Keep(const CMapObjectList *pList)
{
	FOR_EACH_OBJ( *pList, pos )
	{
		CMapClass *pObject = pList->Element(pos);
		Keep(pObject);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void CHistory::KeepForDestruction(CMapClass *pObject)
{
	if (CurTrack == NULL)
	{
		MarkUndoPosition();
	}

	CurTrack->KeepForDestruction(pObject);
}


//-----------------------------------------------------------------------------
// Purpose: Keeps a new object, so it can be deleted on an undo.
// Input  : pObject - Object to keep.
//-----------------------------------------------------------------------------
void CHistory::KeepNew(CMapClass *pObject, bool bKeepChildren)
{
	if (CurTrack == NULL)
	{
		MarkUndoPosition();
	}

	//
	// Keep this object's children.
	//
	if (bKeepChildren)
	{
		EnumChildrenPos_t pos;
		CMapClass *pChild = pObject->GetFirstDescendent(pos);
		while (pChild != NULL)
		{
			CurTrack->KeepNew(pChild);
			pChild = pObject->GetNextDescendent(pos);
		}
	}

	CurTrack->KeepNew(pObject);
}


//-----------------------------------------------------------------------------
// Purpose: Keeps a list of new objects, so changes to them can be undone.
// Input  : pList - List of objects to keep.
//-----------------------------------------------------------------------------
void CHistory::KeepNew( const CMapObjectList *pList, bool bKeepChildren)
{
	FOR_EACH_OBJ( *pList, pos )
	{
		CMapClass *pObject = pList->Element(pos);
		KeepNew(pObject, bKeepChildren);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the given history object as the one to use for all Undo operations.
//-----------------------------------------------------------------------------
void CHistory::SetHistory(class CHistory *pHistory)
{
	pCurHistory = pHistory;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHistory::OnRemoveVisGroup(CVisGroup *pVisGroup)
{
	if (CurTrack)
	{
		CurTrack->OnRemoveVisGroup(pVisGroup);
	}

	if (Opposite && Opposite->CurTrack)
	{
		Opposite->CurTrack->OnRemoveVisGroup(pVisGroup);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CTrackEntry::CTrackEntry()
{
	m_bAutoDestruct = true;
	m_nDataSize = 0;
	m_eType = ttNone;
	m_bUndone = false;
	m_bKeptChildren = false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructs a track entry from a list of parameters.
// Input  : t - 
//-----------------------------------------------------------------------------
CTrackEntry::CTrackEntry(TrackType_t eType, ...)
{
	m_bAutoDestruct = false;
	m_eType = eType;
	m_bUndone = false;
	m_bKeptChildren = false;

	va_list vl;
	va_start(vl, eType);

	switch (m_eType)
	{
		//
		// Keep track of an object that was modified by the user. An Undo will cause this
		// object to revert to its original state.
		//
		case ttCopy:
		{
			m_Copy.pCurrent = va_arg(vl, CMapClass *);
			m_Copy.pKeptObject = m_Copy.pCurrent->Copy(false);
			m_nDataSize = sizeof(*this) + m_Copy.pKeptObject->GetSize();
			break;
		}
		
		//
		// Keep track of an object that was created by the user. An Undo will cause this
		// object to be removed from the world.
		//
		case ttCreate:
		{
			m_Create.pCreated = va_arg(vl, CMapClass *);
			Assert(m_Create.pCreated != NULL);
			Assert(m_Create.pCreated->m_pParent != NULL);
			m_nDataSize = sizeof(*this);
			break;
		}

		//
		// Keep track of an object that was deleted by the user. An Undo will cause this
		// object to be added back into the world.
		//
		case ttDelete:
		{
			m_Delete.pDeleted = va_arg(vl, CMapClass *);
			m_Delete.pKeptParent = m_Delete.pDeleted->GetParent();
			m_nDataSize = sizeof(*this);
			break;
		}
	}

	va_end(vl);
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Called when history events are removed from the Undo
//			history. The goal here is to clean up any copies of objects that
//			were kept in the history.
//
//			Once a track entry object is destroyed, the user event that it
//			tracks can no longer be undone or redone.
//-----------------------------------------------------------------------------
CTrackEntry::~CTrackEntry()
{
	if (!m_bAutoDestruct || m_eType == ttNone)
	{
		return;
	}

	switch (m_eType)
	{
		//
		// We kept a copy of an object. Delete our copy of the object.
		//
		case ttCopy:
		{
			if (!m_bUndone)
			{
				delete m_Copy.pKeptObject;
			}

			break;
		}

		//
		// We kept track of an object's creation. Nothing to delete here. The object is in the world.
		//
		case ttCreate:
		{
			break;
		}

		//
		// We kept a pointer to an object that was deleted from the world. We need to delete the object,
		// because the object's deletion can no longer be undone.
		//
		case ttDelete:
		{
			//
			// If this entry was undone, the object has been added back into the world, so we
			// should not delete the object.
			//
			if (!m_bUndone)
			{
				delete m_Delete.pDeleted;
			}
			break;
		}

		default:
		{
			Assert( false );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTrackEntry::SetKeptChildren(bool bSet)
{
	m_bKeptChildren = bSet;
}


//-----------------------------------------------------------------------------
// Purpose: Performs the undo by restoring the kept object to its original state.
// Input  : Opposite - Pointer to the opposite history track. If we are in the
//				undo history, it points to the redo history, and vice-versa.
//-----------------------------------------------------------------------------
void CTrackEntry::Undo(CHistory *Opposite)
{
	switch (m_eType)
	{
		//
		// We are undoing a change to an object. Restore it to its original state.
		//
		case ttCopy:
		{
			if (m_bKeptChildren)
			{
				Opposite->Keep(m_Copy.pCurrent);
			}
			else
			{
				Opposite->KeepNoChildren(m_Copy.pCurrent);
			}

			//
			// Copying back into the world, so update object dependencies.
			//
			m_Copy.pCurrent->CopyFrom(m_Copy.pKeptObject, true);

			//
			// Delete the copy of the kept object.
			//
			delete m_Copy.pKeptObject;
			m_Copy.pKeptObject = NULL;
			break;
		}

		//
		// We are undoing the deletion of an object. Add it to the world.
		//
		case ttDelete:
		{
			//
			// First restore the deleted object's parent so that it is properly kept in the
			// opposite history track. The opposite history track sees this as a new object
			// being created.
			//
			m_Delete.pDeleted->m_pParent = m_Delete.pKeptParent;
			Opposite->KeepNew(m_Delete.pDeleted, false);

			//
			// Put the object back in the world.
			//
			Opposite->GetDocument()->AddObjectToWorld(m_Delete.pDeleted, m_Delete.pKeptParent);
			break;
		}

		//
		// We are undoing the creation of an object. Remove it from the world.
		//
		case ttCreate:
		{
			//
			// Create a symmetrical track event in the other history track.
			//
			Opposite->KeepForDestruction(m_Create.pCreated);

			//
			// Remove the object from the world, but not its children. If its children
			// were new to the world they were kept seperately.
			//
			Opposite->GetDocument()->RemoveObjectFromWorld(m_Create.pCreated, false);
			m_Create.pCreated = NULL; // dvs: why do we do this?
			break;
		}
	}

	m_bUndone = true;
}


//-----------------------------------------------------------------------------
// Purpose: Notifies the object that it has been undone/redone. Called after all
//			undo entries have been handled so that objects are dealing with the
//			correct data set when they calculate bounds, etc.
//-----------------------------------------------------------------------------
void CTrackEntry::DispatchUndoNotify(void)
{
	switch (m_eType)
	{
		//
		// We are undoing a change to an object. Restore it to its original state.
		//
		case ttCopy:
		{
			m_Copy.pCurrent->OnUndoRedo();
			m_Copy.pCurrent->NotifyDependents(Notify_Changed);
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: The given visgroup is being deleted. Remove pointers to it from
//			the object in this track entry.
// Input  : pVisGroup - 
//-----------------------------------------------------------------------------
void CTrackEntry::OnRemoveVisGroup(CVisGroup *pVisGroup)
{
	switch (m_eType)
	{
		case ttCopy:
		{
			m_Copy.pKeptObject->RemoveVisGroup(pVisGroup);
			break;
		}
		
		case ttCreate:
		{
			break;
		}

		case ttDelete:
		{
			m_Delete.pDeleted->RemoveVisGroup(pVisGroup);
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParent - 
//			*pSelected - 
// Output : 
//-----------------------------------------------------------------------------
CHistoryTrack::CHistoryTrack(CHistory *pParent, const CMapObjectList *pSelected)
{
	Parent = pParent;

	Data.EnsureCapacity(16);

	uDataSize = 0;

	static int dwTrackerID = 1;	// objects start at 0, so we don't want to
	dwID = dwTrackerID ++;
	
	// add to local list of selected objects at time of creation
	if (pSelected)
	{
		Selected.AddVectorToTail(*pSelected);
	}

	m_bAutoDestruct = true;
	szName[0] = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Called when this track's document is being deleted.
//			Marks all entries in this track for autodestruction, so that when
//			their destructor gets called, they free any object pointers that they
//			hold.
//-----------------------------------------------------------------------------
CHistoryTrack::~CHistoryTrack()
{
	for (int i = 0; i < Data.Count(); i++)
	{
		Data[i].m_bAutoDestruct = m_bAutoDestruct;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//			iFlag - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CHistoryTrack::CheckObjectFlag(CMapClass *pObject, int iFlag)
{
	// check for saved copy already..
	if(pObject->Kept.ID != dwID)
	{
		// no id.. make sure types is flag only
		pObject->Kept.ID = dwID;
		pObject->Kept.Types = iFlag;
	}
	else if(!(pObject->Kept.Types & iFlag))
	{
		// if we've already stored that this is a new object in this
		//  track, there is no point in storing a copy since UNDOing
		//  this track will delete the object.
		if(iFlag == CTrackEntry::ttCopy && 
			(pObject->Kept.Types & CTrackEntry::ttCreate))
		{
			return TRUE;
		}

		// id, but no copy flag.. make sure types has flag set
		pObject->Kept.Types |= iFlag;
	}
	else
	{
		// both here.. we have a copy
		return TRUE;
	}

	return FALSE;
}




//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHistoryTrack::OnRemoveVisGroup(CVisGroup *pVisGroup)
{
	for (int i = 0; i < Data.Count(); i++)
	{
		Data[i].OnRemoveVisGroup(pVisGroup);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void CHistoryTrack::Keep(CMapClass *pObject, bool bKeepChildren)
{
	if(Parent->IsPaused() || pObject->IsTemporary())
		return;

	// make a copy of this object so we can undo changes to it

	if(CheckObjectFlag(pObject, CTrackEntry::ttCopy))
		return;

	Parent->Pause();
	CTrackEntry te(CTrackEntry::ttCopy, pObject);
	te.SetKeptChildren(bKeepChildren);
	Data.AddToTail(te);
	te.m_bAutoDestruct = false;
	
	uDataSize += te.GetSize();
	Parent->Resume();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void CHistoryTrack::KeepForDestruction(CMapClass *pObject)
{
	if(Parent->IsPaused() || pObject->IsTemporary())
		return;

	// check for saved destruction already..
	if(CheckObjectFlag(pObject, CTrackEntry::ttDelete))
		return;

	Parent->Pause();
	CTrackEntry te(CTrackEntry::ttDelete, pObject);
	Data.AddToTail(te);
	
	te.m_bAutoDestruct = false;
	uDataSize += te.GetSize();
	Parent->Resume();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void CHistoryTrack::KeepNew(CMapClass *pObject)
{
	if(Parent->IsPaused() || pObject->IsTemporary())
		return;

	// check for saved creation already..
	VERIFY(!CheckObjectFlag(pObject, CTrackEntry::ttCreate));

	Parent->Pause();
	CTrackEntry te(CTrackEntry::ttCreate, pObject);
	Data.AddToTail(te);
	
	te.m_bAutoDestruct = false;
	uDataSize += te.GetSize();
	Parent->Resume();
}


//-----------------------------------------------------------------------------
// Purpose: Undoes all the track entries in this track.
//-----------------------------------------------------------------------------
void CHistoryTrack::Undo()
{
	for (int i = Data.Count() - 1; i >= 0; i--)
	{
		Data[i].Undo(Parent->Opposite);
	}

	//
	// Do notification separately so that objects are dealing with the
	// correct data set when they calculate bounds, etc.
	//
	for (int i = Data.Count() - 1; i >= 0; i--)
	{
		Data[i].DispatchUndoNotify();
	}
}
