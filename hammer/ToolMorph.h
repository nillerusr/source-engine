//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MORPH3D_H
#define MORPH3D_H
#ifdef _WIN32
#pragma once
#endif

#include "MapClass.h"			// dvs: For CMapObjectList
#include "Box3D.h"
#include "SSolid.h"
#include "Resource.h"
#include "ScaleVerticesDlg.h"
#include "ToolInterface.h"
#include "mathlib/vector.h"


class IMesh;
class Morph3D;
class CRender2D;
class CRender3D;


const SSHANDLE SSH_SCALEORIGIN = 0xffff0L;


typedef struct
{
	CMapSolid *pMapSolid;
	CSSolid *pStrucSolid;
	SSHANDLE ssh;
} MORPHHANDLE;


class Morph3D : public Box3D
{
public:

	Morph3D();
	virtual ~Morph3D();

	BOOL IsMorphing(CMapSolid *pSolid, CSSolid **pStrucSolidRvl = NULL);

	bool SplitFace();
	bool CanSplitFace();

	void SelectHandle(MORPHHANDLE *pInfo, UINT cmd = scSelect);
	void SelectHandle2D( CMapView2D *pView, MORPHHANDLE *pInfo, UINT cmd = scSelect);
	void DeselectHandle(MORPHHANDLE *pInfo);

	void MoveSelectedHandles(const Vector &Delta);
	int GetSelectedHandleCount(void) { return m_SelectedHandles.Count(); }
	void GetSelectedCenter(Vector& pt);
	SSHANDLETYPE GetSelectedType() { return m_SelectedType; }
	bool IsSelected(MORPHHANDLE &mh);

	void SelectObject(CMapSolid *pSolid, UINT cmd = scSelect);
	bool SelectAt( CMapView *pView, UINT nFlags, const Vector2D &vPoint );

	void GetMorphBounds(Vector &mins, Vector &maxs, bool bReset);

	void SnapSelectedToGrid( int nGridSpacing );

	// Toggle mode - vertex & edge, vertex, edge.
	void ToggleMode();

	void OnScaleCmd(BOOL bReInit = FALSE);
	void UpdateScale();
	BOOL IsScaling() { return m_bScaling; }

	void GetMorphingObjects(CUtlVector<CMapClass *> &List);

	inline int GetObjectCount(void);
	inline CSSolid *GetObject(int pos);
	
	//
	// Tool3D implementation.
	//
	virtual bool IsEmpty() { return !m_StrucSolids.Count() && !m_bBoxSelecting; }
	virtual void SetEmpty();
	virtual void FinishTranslation(bool bSave);
	virtual unsigned int GetConstraints(unsigned int nKeyFlags);

	//
	// CBaseTool implementation.
	//
	virtual void OnActivate();
	virtual void OnDeactivate();
	virtual ToolID_t GetToolID(void) { return TOOL_MORPH; }

	virtual bool CanDeactivate( void );

	virtual bool OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnChar2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual void RenderTool2D(CRender2D *pRender);
	virtual void RenderTool3D(CRender3D *pRender);

private:

	void OnEscape(void);
	bool NudgeHandles(CMapView *pView, UINT nChar, bool bSnap);
	
	bool MorphHitTest(CMapView *pView, const Vector2D &vPoint, MORPHHANDLE *pInfo);

	void GetHandlePos(MORPHHANDLE *pInfo, Vector& pt);

	SSHANDLE Get2DMatches(CMapView2D *pView, CSSolid *pStrucSolid, SSHANDLEINFO &hi, CUtlVector<SSHANDLE>*pSimilarList = NULL);

	void StartTranslation(CMapView *pView, const Vector2D &vPoint, MORPHHANDLE *pInfo );

	void RenderSolid3D(CRender3D *pRender, CSSolid *pSolid);
	
	//
	// Tool3D implementations.
	//
	int HitTest(CMapView *pView, const Vector2D &ptClient, bool bTestHandles = false);
	
	virtual bool UpdateTranslation( const Vector &pos, UINT uFlags );

	bool StartBoxSelection( CMapView *pView, const Vector2D &vPoint, const Vector &vStart);
	void SelectInBox();
	void EndBoxSelection();
	bool IsBoxSelecting() { return m_bBoxSelecting; }

	bool CanDeselectList( void );

	// list of active Structured Solids:
	CUtlVector<CSSolid*> m_StrucSolids;
	
	// list of selected nodes:
	CUtlVector<MORPHHANDLE> m_SelectedHandles;
	
	// type of selected handles:
	SSHANDLETYPE m_SelectedType;

	// main morph handle:
	MORPHHANDLE m_MorphHandle;
	Vector m_OrigHandlePos;

	// morph bounds:
	BoundBox m_MorphBounds;
	
	// handle mode:
	enum
	{
		hmBoth = 0x01 | 0x02,
		hmVertex = 0x01,
		hmEdge = 0x02
	};

	bool		m_bLButtonDownControlState;
	Vector2D	m_vLastMouseMovement;

	bool m_bHit;

	MORPHHANDLE m_DragHandle;	// The morph handle that we are dragging.

	bool m_bMorphing;
	bool m_bMovingSelected;	// not moving them yet - might just select this

	int m_HandleMode;
	bool m_bBoxSelecting;
	bool m_bScaling;
	bool m_bUpdateOrg;
	CScaleVerticesDlg m_ScaleDlg;
	Vector *m_pOrigPosList;
	Vector m_ScaleOrg;
};


//-----------------------------------------------------------------------------
// Purpose: Returns the number of solids selected for morphing.
//-----------------------------------------------------------------------------
inline int Morph3D::GetObjectCount(void)
{
	return(m_StrucSolids.Count());
}


//-----------------------------------------------------------------------------
// Purpose: Iterates the selected solids.
//-----------------------------------------------------------------------------
inline CSSolid *Morph3D::GetObject(int pos)
{
	return(m_StrucSolids.Element(pos));
}

#endif // MORPH3D_H
