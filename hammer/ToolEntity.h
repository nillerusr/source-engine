//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the interface to the entity placement tool.
//
//=============================================================================

#ifndef TOOLENTITY_H
#define TOOLENTITY_H
#pragma once


#include "ToolInterface.h"
#include "Tool3D.h"


class CRender2D;
class CRender3D;


class CToolEntity : public Tool3D
{

friend class CToolEntityMessageWnd;

public:

	CToolEntity(void);
	~CToolEntity(void);

	inline void GetPos(Vector &vecPos);

	//
	// CBaseTool implementation.
	//
	virtual ToolID_t GetToolID(void) { return TOOL_ENTITY; }

	virtual bool OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);

	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual bool OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	
	virtual void RenderTool2D(CRender2D *pRender);
	virtual void RenderTool3D(CRender3D *pRender);

protected:

	//
	// Tool3D implementation.
	//
			void StartTranslation( CMapView *pView, const Vector2D &vPoint);
	virtual bool UpdateTranslation(const Vector &vUpdate, UINT flags);
	virtual void FinishTranslation(bool bSave);
	virtual int  HitTest(CMapView *pView, const Vector2D &vPoint, bool bTestHandles = false);

private:

	void OnEscape(void);
	void CreateMapObject(CMapView2D *pView);

	Vector m_vecPos;			// Current position of the marker.
};


//-----------------------------------------------------------------------------
// Purpose: Returns the current position of the marker.
//-----------------------------------------------------------------------------
inline void CToolEntity::GetPos(Vector &vecPos)
{
	vecPos = m_vecPos;
}


#endif // TOOLENTITY_H
