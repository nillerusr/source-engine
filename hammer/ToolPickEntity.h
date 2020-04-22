//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Tool used for picking entities for filling out entity properties and
//			I/O connections. Anywhere you see an entity name field there should
//			be an eyedropper button next to it for picking the entity.
//
//			TODO: make the entity field / picker button a single control?
//			TODO: combine the face picker with the entity picker?
//
//=============================================================================//

#ifndef TOOLPICKENTITY_H
#define TOOLPICKENTITY_H
#ifdef _WIN32
#pragma once
#endif

#include "MapEntity.h"
#include "ToolInterface.h"


class CMapView3D;
class CMapViewLogical;
class CToolPickEntity;


//
// Selection states for entries in our list of selected faces.
//
enum EntityState_t
{
	EntityState_Select = 0,			// 
	EntityState_Partial,			// Used for multiselect; the face is in at least one of the face lists being edited.
	EntityState_None,				// Used for multiselect; to deselect partially selected faces. Otherwise they are removed from the list.
};


//
// An entry in our list of selected entities.
//
struct SelectedEntity_t
{
	CMapEntity *pEntity;				// Pointer to the entity.
	EntityState_t eState;				// The current selection state of this entity.
	EntityState_t eOriginalState;		// The original selection state of this entity.
};


//
// Interface for notification by the entity picking tool. Inherit from this if you
// are a client of the entity picker.
//
class IPickEntityTarget
{
public:
	virtual void OnNotifyPickEntity(CToolPickEntity *pTool) = 0;
};


class CToolPickEntity : public CBaseTool
{
public:

	//
	// Constructor/Destructor
	//
    CToolPickEntity();
    ~CToolPickEntity();

	//
	// CBaseTool virtual implementations
	//
	virtual void OnDeactivate();
	virtual ToolID_t GetToolID(void) { return TOOL_PICK_ENTITY; }

	virtual bool OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
    virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
    virtual bool OnLMouseDblClk3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnRMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
    virtual bool OnRMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual bool OnLMouseUpLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint) { return true; }
	virtual bool OnLMouseDownLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint);
    virtual bool OnLMouseDblClkLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint) { return true; }
	virtual bool OnRMouseUpLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint) { return true; }
    virtual bool OnRMouseDownLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint) { return true; }
	virtual bool OnMouseMoveLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint);

	//
	// Functions specific to this tool.
	//
	inline void Attach(IPickEntityTarget *pTarget);
	void AllowMultiSelect(bool bAllow);
	void GetSelectedEntities(CMapEntityList &EntityListFull, CMapEntityList &EntityListPartial);
	void SetSelectedEntities(CMapEntityList &EntityListFull, CMapEntityList &EntityListPartial);

protected:

	void CycleSelectEntity(CMapEntity *pEntity);
	void DeselectAll(void);
	void DeselectEntity(int nIndex);
	void DeselectEntity(CMapEntity *pEntity);
	int FindEntity(CMapEntity *pEntity);
	void SelectEntity(CMapEntity *pEntity);

	void AddToList(CMapEntity *pEntity, EntityState_t eState);
	void RemoveFromList(int nIndex);

	void SetEyedropperCursor(void);

	IPickEntityTarget *m_pNotifyTarget;			// Object to notify when selection events occur.
	bool m_bAllowMultiSelect;					// If false, only one entity can be selected at a time.
	CUtlVector <SelectedEntity_t> m_Entities;	// Picked entities and their selection state (partial or full).
};


//-----------------------------------------------------------------------------
// Purpose: Attaches the given notification target to this tool. That object
//			will be used for all future notifications and updates by the tool.
//-----------------------------------------------------------------------------
void CToolPickEntity::Attach(IPickEntityTarget *pNotifyTarget)
{
	m_pNotifyTarget = pNotifyTarget;
}

#endif // TOOLPICKENTITY_H
