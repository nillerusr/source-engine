//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Data and functionality for the logical 2D view
//
//===========================================================================//

#ifndef MAPVIEWLOGICAL_H
#define MAPVIEWLOGICAL_H

#ifdef _WIN32
#pragma once
#endif

#include "MapView2DBase.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlstack.h"

class CWnd;
class CView;
class CMapAtom;
class CMapClass;
class CMapDoc;
class CCamera;
class CTitleWnd;
class CEntityConnection;


class CMapViewLogical : public CMapView2DBase
{
public:
	// Other public methods
	virtual void Render();

protected:
	CMapViewLogical();           // protected constructor used by dynamic creation
	virtual ~CMapViewLogical();
	DECLARE_DYNCREATE(CMapViewLogical)

	virtual bool IsLogical() { return true; }
	virtual void OnRenderListDirty();

	// convert client view space to map world coordinates (2D versions for convenience) 
	void WorldToClient( Vector2D &ptClient, const Vector2D &vWorld );
	void ClientToWorld( Vector2D &vWorld, const Vector2D &vClient );
	virtual void WorldToClient( Vector2D &ptClient, const Vector &vWorld );
	virtual void ClientToWorld( Vector &vWorld, const Vector2D &vClient );

	// Performs a selection which selects an object at the point as well as entities connected to outputs.
	bool SelectAtCascading( const Vector2D &ptClient, bool bMakeFirst );

private:
	// timer IDs:
	enum 
	{ 
		TIMER_CONNECTIONUPDATE = 2, 
	};

	typedef CUtlRBTree<CMapClass*, unsigned short> MapClassDict_t;

	// Purpose: Builds up list of mapclasses to render
	void AddToRenderLists( CMapClass *pObject );
	void PopulateConnectionList( );

	// Purpose: 
	void RenderConnections(const bool bDrawSelected, const bool bAnySelected);

	// Draws a wire from a particular point to a target
	const color32 & GetWireColor(const char *pszName, const bool bSelected, const bool bBroken, const bool bAnySelected );
	void DrawConnectingWire( float x, float y, CMapEntity *pSource, CEntityConnection *pConnection, CMapEntity *pTarget );

	bool	m_bUpdateRenderObjects;	// Should I build a list of things to render?
	CUtlVector<CMapClass *> m_RenderList;	// list of current rendered objects
	CUtlVector<CMapClass *> m_ConnectionList;	// list of all objects which are in the render list of have connections to something in the renderlist
	CUtlStack<CMapClass *> m_ConnectionUpdate;	// for iteratively determining connectivity
	MapClassDict_t m_RenderDict;

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMapView2D)
	protected:
	virtual void OnInitialUpdate();     // first time after construct
	//}}AFX_VIRTUAL

	// Generated message map functions
	//{{AFX_MSG(CMapView2D)
	afx_msg void OnTimer(UINT nIDEvent);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


#endif // MAPVIEWLOGICAL_H
