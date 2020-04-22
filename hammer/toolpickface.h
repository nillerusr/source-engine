//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Tool used for picking brush faces.
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOOLPICKFACE_H
#define TOOLPICKFACE_H
#ifdef _WIN32
#pragma once
#endif


#include "MapFace.h"
#include "ToolInterface.h"


class CMapView3D;
class CToolPickFace;


//
// Selection states for entries in our list of selected faces.
//
enum FaceState_t
{
	FaceState_Select = 0,		// 
	FaceState_Partial,			// Used for multiselect; the face is in at least one of the face lists being edited.
	FaceState_None,				// Used for multiselect; to deselect partially selected faces. Otherwise they are removed from the list.
};


//
// An entry in our list of selected faces.
//
struct SelectedFace_t
{
	CMapFace *pFace;				// Pointer to the face.
	FaceState_t eState;				// The current selection state of this face.
	FaceState_t eOriginalState;		// The original selection state of this face.
};


//
// Interface for notification by the face picking tool. Inherit from this if you
// are a client of the face picker.
//
class IPickFaceTarget
{
public:
	virtual void OnNotifyPickFace(CToolPickFace *pTool) = 0;
};


class CToolPickFace : public CBaseTool
{
public:

	//
	// Constructor/Destructor
	//
    CToolPickFace();
    ~CToolPickFace();

	//
	// CBaseTool virtual implementations
	//
	virtual void OnDeactivate();
	virtual ToolID_t GetToolID(void) { return TOOL_PICK_FACE; }

	virtual bool OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
    virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
    virtual bool OnLMouseDblClk3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnRMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
    virtual bool OnRMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);

	//
	// Functions specific to this tool.
	//
	inline void Attach(IPickFaceTarget *pTarget);
	void AllowMultiSelect(bool bAllow);
	void GetSelectedFaces(CMapFaceList &FaceListFull, CMapFaceList &FaceListPartial);
	void SetSelectedFaces(CMapFaceList &FaceListFull, CMapFaceList &FaceListPartial);

protected:

	void CycleSelectFace(CMapFace *pFace);
	void DeselectAll(void);
	void DeselectFace(int nIndex);
	void DeselectFace(CMapFace *pFace);
	int FindFace(CMapFace *pFace);
	void SelectFace(CMapFace *pFace);

	void AddToList(CMapFace *pFace, FaceState_t eState);
	void RemoveFromList(int nIndex);

	void SetEyedropperCursor(void);

	IPickFaceTarget *m_pNotifyTarget;		// Object to notify when selection events occur.
	bool m_bAllowMultiSelect;				// If false, only one face can be selected at a time.
	CUtlVector <SelectedFace_t> m_Faces;	// Picked faces and their selection state (partial or full).
};


//-----------------------------------------------------------------------------
// Purpose: Attaches the given notification target to this tool. That object
//			will be used for all future notifications and updates by the tool.
//-----------------------------------------------------------------------------
void CToolPickFace::Attach(IPickFaceTarget *pNotifyTarget)
{
	m_pNotifyTarget = pNotifyTarget;
}


#endif // TOOLPICKFACE_H
