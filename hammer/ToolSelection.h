//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef SELECTION3D_H
#define SELECTION3D_H
#ifdef _WIN32
#pragma once
#endif


#include "Box3D.h"
#include "MapClass.h"			// For CMapObjectList
#include "ToolInterface.h"
#include "utlvector.h"


class CMapWorld;
class CMapView;
class CMapView2D;
class CMapView3D;
class GDinputvariable;
class CRender2D;

class Selection3D : public Box3D
{

public:

	Selection3D();
	~Selection3D();

	void Init( CMapDoc *pDocument );

	inline bool IsBoxSelecting();
	inline bool IsLogicalBoxSelecting();
	void EndBoxSelection();
	
	// Start, end logical selection
	void StartLogicalBoxSelection( CMapViewLogical *pView, const Vector &vStart );
	void EndLogicalBoxSelection( );

	// Tool3D implementation. 
	virtual void SetEmpty();
	virtual bool IsEmpty();

	//
	// CBaseTool implementation.
	//
	virtual void OnActivate();
	virtual void OnDeactivate();
	virtual ToolID_t GetToolID() { return TOOL_POINTER; }

	virtual bool OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual bool OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDblClk3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual void RenderTool2D(CRender2D *pRender);
	virtual void RenderToolLogical(CRender2D *pRender);
	virtual void RenderTool3D(CRender3D *pRender);

	virtual bool OnContextMenuLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDownLogical(CMapViewLogical *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDownLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUpLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMoveLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseDblClkLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint);

	void UpdateSelectionBounds();

	bool		m_bBoxSelection;
	
protected:

	void TransformSelection();
	void TransformLogicalSelection( const Vector2D &vecTranslation );

	void FinishTranslation(bool bSave, bool bClone );
	void StartTranslation(CMapView *pView, const Vector2D &vPoint, const Vector &vHandleOrigin );
	bool StartBoxSelection( CMapView *pView, const Vector2D &vPoint, const Vector &vStart);
	
	void UpdateHandleState();

	virtual unsigned int GetConstraints(unsigned int nKeyFlags);

	void NudgeObjects(CMapView *pView, int nChar, bool bSnap, bool bClone);
	
	GDinputvariable *ChooseEyedropperVar(CMapView *pView, CUtlVector<GDinputvariable *> &VarList);
	
	CMapEntity *FindEntityInTree(CMapClass *pObject);

	void SelectInBox(CMapDoc *pDoc, bool bInsideOnly);
	CBaseTool *GetToolObject( CMapView2D *pView, const Vector2D &ptScreen, bool bAttach );
	CBaseTool *GetToolObjectLogical( CMapViewLogical *pView, const Vector2D &vPoint, bool bAttach );

	void SetEyedropperCursor();

	void EyedropperPick2D(CMapView2D *pView, const Vector2D &vPoint);
	void EyedropperPick3D(CMapView3D *pView, const Vector2D &vPoint);
	void EyedropperPick(CMapView *pView, CMapClass *pObject);

	void OnEscape(CMapDoc *pDoc);

	//
	// Tool3D implementation.
	//
	virtual int HitTest(CMapView *pView, const Vector2D &pt, bool bTestHandles = false);

	// Methods related to logical operations
	void EyedropperPickLogical( CMapViewLogical *pView, const Vector2D &vPoint );
	bool HitTestLogical( CMapView *pView, const Vector2D &ptClient );
	void SelectInLogicalBox(CMapDoc *pDoc, bool bInsideOnly);

	CSelection	*m_pSelection;	// the documents selection opject
	
	bool m_bEyedropper;			// True if we are holding down the eyedropper hotkey.

	bool m_bSelected;			// Did we select an object on left button down?
	bool m_b3DEditMode;			// editing mode in 3D on/off

	bool m_bDrawAsSolidBox;		// sometimes we want to render the tool bbox solid

	// These are fields related to manipulation in logical views
	Vector2D m_vLDownLogicalClient;	// Logical client pos at which lbutton was pressed.
	Vector2D m_vecLogicalSelBoxMins;
	Vector2D m_vecLogicalSelBoxMaxs;
	bool m_bInLogicalBoxSelection;	// Are we doing box selection in the logical mode?
	COLORREF m_clrLogicalBox;		// The color of the logical box
	Vector2D m_vLastLogicalDragPoint;	// Last point at which we dragged (world coords)
	Vector2D m_vLogicalTranslation;
	bool m_bIsLogicalTranslating;	// true while translation in logical view
	bool m_bLButtonDown;
	bool m_bLeftDragged;
};


//-----------------------------------------------------------------------------
// Are we in box selection?
//-----------------------------------------------------------------------------
inline bool Selection3D::IsBoxSelecting() 
{ 
	return m_bBoxSelection; 
}

inline bool Selection3D::IsLogicalBoxSelecting() 
{ 
	return m_bInLogicalBoxSelection; 
}

	
#endif // SELECTION3D_H
