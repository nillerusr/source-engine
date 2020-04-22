//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOOLSWEPTHULL_H
#define TOOLSWEPTHULL_H
#ifdef _WIN32
#pragma once
#endif

#include "ToolInterface.h"


class CMapSweptPlayerHull;
class CMapPointHandle;


class CToolSweptPlayerHull : public CBaseTool
{

public:

	CToolSweptPlayerHull(void);
	void Attach(CMapSweptPlayerHull *pPoint, int nPointIndex);

	//
	// CBaseTool implementation.
	//
	virtual ToolID_t GetToolID(void) { return TOOL_SWEPT_HULL; }

	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual void RenderTool2D(CRender2D *pRender);
	//virtual void RenderTool3D(CRender3D *pRender);

private:

	CMapSweptPlayerHull *m_pSweptHull;		// The swept hull we are manipulating.
	int m_nPointIndex;				// The index of the endpoint we are manipulating [0,1].
};

#endif // TOOLSWEPTHULL_H
