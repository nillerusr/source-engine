//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOOLMATERIAL_H
#define TOOLMATERIAL_H
#ifdef _WIN32
#pragma once
#endif


#include "ToolInterface.h"


class CToolMaterial : public CBaseTool
{
	//
	// CBaseTool implementation.
	//
	virtual ToolID_t GetToolID(void) { return TOOL_FACEEDIT_MATERIAL; }

	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnRMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual void UpdateStatusBar();

	virtual void OnDeactivate();
};


#endif // TOOLMATERIAL_H
