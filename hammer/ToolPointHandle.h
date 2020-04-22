//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOOLPOINTHANDLE_H
#define TOOLPOINTHANDLE_H
#ifdef _WIN32
#pragma once
#endif

#include "ToolInterface.h"


class CMapPointHandle;


class CToolPointHandle : public CBaseTool
{

public:

	CToolPointHandle(void);
	void Attach(CMapPointHandle *pPoint);

	void CenterOnParent(CMapView *pView);

	//
	// CBaseTool implementation.
	//
	virtual ToolID_t GetToolID(void) { return TOOL_POINT_HANDLE; }

	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual void RenderTool2D(CRender2D *pRender);
	//virtual void RenderTool3D(CRender3D *pRender);

private:

	CMapPointHandle *m_pPoint;
};


#endif // TOOLPOINTHANDLE_H
