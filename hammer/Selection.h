//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The document. Exposes functions for object creation, deletion, and
//			manipulation. Holds the current tool. Handles GUI messages that are
//			view-independent.
//
//=============================================================================//

#ifndef SELECTIONMANAGER_H
#define SELECTIONMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "mapclass.h"

class CMapDoc;

enum
{
	hitFirst = 0,
	hitNext = -1,
	hitPrev = -2
};

// SelectObject/SelectFace parameters:
typedef enum
{
	scToggle = 0x01,			// toogle selection state of this object
	scSelect = 0x02,			// select this object
	scUnselect = 0x04,			// unselect this object
	scClear = 0x10,				// Clear current before selecting
	scNoLift = 0x20,			// Don't lift face attributes into Face Properties dialog   dvs: lame!
	scNoApply = 0x40,			// Don't apply face attributes from Face Properties dialog to selected face   dvs: lame!
	scCascading = 0x80,				// Select all entities attached to outputs of this entity
	scCascadingRecursive = 0x100,	// Select all entities attached to outputs of this entity, recursively
	scSelectAll = 0x200,
	scSaveChanges = 0x400,		// changing the selection causes changes made in the properties dialog be saved
};


class CSelection
{
public:
	CSelection(void);
	virtual ~CSelection(void);

	void	Init(CMapDoc *pDocument);
	
	bool	SelectObject(CMapClass *pobj, int cmd = scSelect);
	void	SelectObjectList(const CMapObjectList *pList, int cmd = (scClear|scSelect|scSaveChanges) );
	
	bool	RemoveAll();		// true if any elements were removed
	bool	RemoveInvisibles(); // true if any elements were removed
	bool	RemoveDead();		// true if any elements were removed

	int		GetCount();
	bool	IsEmpty();
	bool	IsSelected(CMapClass *pObject);
	bool	IsAnEntitySelected();
	bool	IsEditable();
	bool	IsCopyable();
	

	const CMapObjectList* GetList(void);
	CMapDoc *GetMapDoc() { return m_pDocument; }

	// HitList feature
	const CMapObjectList* GetHitList(void);
	void ClearHitList();
	void AddHit(CMapClass *pObject);
	void SetCurrentHit(int iIndex, bool bCascading = false);
	
	SelectMode_t GetMode(void);
	void SetMode(SelectMode_t eSelectMode);
	void SetSelectionState(SelectionState_t eSelectionState);
	
    bool GetBounds(Vector &vecMins, Vector &vecMaxs);
	
	// Used for translations. Uses entity origins and brush bounds. That way, when moving stuff,
	// the entity origins will stay on the grid.
	void GetBoundsForTranslation( Vector &vecMins, Vector &vecMaxs );

	bool GetBoundsCenter(Vector &vecCenter);
	void GetLastValidBounds(Vector &vecMins, Vector &vecMaxs);
	bool GetLogicalBounds(Vector2D &vecMins, Vector2D &vecMaxs);
	bool GetLogicalBoundsCenter( Vector2D &vecCenter );

	void SetBoundsDirty() {m_bBoundsDirty = true;}
	
protected:

	void UpdateSelectionBounds();

	CMapDoc			*m_pDocument;		// document this selection set belongs to
	SelectMode_t	m_eSelectMode;		// Controls what gets selected based on what the user clicked on.
	CMapObjectList	m_SelectionList;	// The list of selected objects.

	bool			m_bBoundsDirty;		// recalc bounds box with next query

	BoundBox	m_Bounds;				// current bounds
	BoundBox	m_LastValidBounds;		// last valid selection bounds
	
	Vector2D m_vecLogicalMins;		// Selection bounds in "logical" space
	Vector2D m_vecLogicalMaxs;

	// Hit selection.
	CMapObjectList	m_HitList; // list of 'hit' object (potential selected object)
	int				m_iCurHit; // current hit or -1

	
	

};

#endif // SELECTIONMANAGER_H