//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOOLSPHERE_H
#define TOOLSPHERE_H
#ifdef _WIN32
#pragma once
#endif


#include "ToolInterface.h"


class CMapSphere;


class CToolSphere : public CBaseTool
{

public:

	CToolSphere();

	void Attach(CMapSphere *pShere);

	//
	// CBaseTool implementation.
	//
	virtual ToolID_t GetToolID(void) { return TOOL_SPHERE; }

	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);

	//virtual void RenderTool2D(CRender2D *pRender);

private:

	CMapSphere *m_pSphere;

};



#endif // TOOLSPHERE_H
