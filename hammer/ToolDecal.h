//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOOLDECAL_H
#define TOOLDECAL_H
#ifdef _WIN32
#pragma once
#endif

#include "ToolInterface.h"


class CToolDecal : public CBaseTool
{

public:

	//
	// CBaseTool implementation.
	//
	virtual ToolID_t GetToolID(void) { return TOOL_DECAL; }

	virtual bool OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	
private:
	
	void SetDecalCursor(void);
};


#endif // TOOLDECAL_H
