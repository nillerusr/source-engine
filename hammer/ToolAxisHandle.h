//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOOLAXISHANDLE_H
#define TOOLAXISHANDLE_H
#ifdef _WIN32
#pragma once
#endif

#include "ToolInterface.h"


class CMapAxisHandle;
class CMapPointHandle;


class CToolAxisHandle : public CBaseTool
{

public:

	CToolAxisHandle(void);
	void Attach(CMapAxisHandle *pPoint, int nPointIndex);

	//
	// CBaseTool implementation.
	//
	virtual ToolID_t GetToolID(void) { return TOOL_AXIS_HANDLE; }

	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual void RenderTool2D(CRender2D *pRender);
	//virtual void RenderTool3D(CRender3D *pRender);

private:

	CMapAxisHandle *m_pAxis;		// The axis we are manipulating.
	int m_nPointIndex;				// The index of the endpoint we are manipulating [0,1].
};

#endif // TOOLAXISHANDLE_H
