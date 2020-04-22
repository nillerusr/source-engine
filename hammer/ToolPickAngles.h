//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Tool used for point-and-click picking of angles for filling out
//			entity properties.
//
//=============================================================================//

#ifndef TOOLPICKANGLES_H
#define TOOLPICKANGLES_H
#ifdef _WIN32
#pragma once
#endif

#include "MapEntity.h"
#include "ToolInterface.h"


class CMapView3D;
class CToolPickAngles;


//
// Interface for notification by the angles picking tool. Inherit from this if you
// are a client of the angles picker.
//
class IPickAnglesTarget
{
public:
	virtual void OnNotifyPickAngles(const Vector &vecPos) = 0;
};


class CToolPickAngles : public CBaseTool
{
public:

	//
	// Constructor/Destructor
	//
    CToolPickAngles();
    ~CToolPickAngles();

	//
	// CBaseTool virtual implementations
	//
	virtual ToolID_t GetToolID(void) { return TOOL_PICK_ANGLES; }

	virtual bool OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
    virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
    virtual bool OnLMouseDblClk3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnRMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
    virtual bool OnRMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);

	//
	// Functions specific to this tool.
	//
	inline void Attach(IPickAnglesTarget *pTarget);

protected:

	void SetToolCursor(void);

	IPickAnglesTarget *m_pNotifyTarget;			// Object to notify when selection events occur.
};


//-----------------------------------------------------------------------------
// Purpose: Attaches the given notification target to this tool. That object
//			will be used for all future notifications and updates by the tool.
//-----------------------------------------------------------------------------
void CToolPickAngles::Attach(IPickAnglesTarget *pNotifyTarget)
{
	m_pNotifyTarget = pNotifyTarget;
}

#endif // TOOLPICKANGLES_H
