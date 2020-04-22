//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OVERLAY3D_H
#define OVERLAY3D_H
#pragma once

#include <afxwin.h>
#include "Box3D.h"
#include "ToolInterface.h"
#include "MapOverlay.h"

class CMapDoc;
struct Shoreline_t;

class CToolOverlay : public Box3D
{
public:

	//=========================================================================
	//
	// Constructur/Destructor
	//
	CToolOverlay();
	~CToolOverlay();

	//=========================================================================
	//
	// CBaseTool virtual implementations
	//
	ToolID_t	GetToolID( void ) { return TOOL_OVERLAY; }
	
    void		OnActivate();
    void		OnDeactivate();

	bool		OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
    bool		OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	bool		OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	bool		OnContextMenu2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint );

	void		RenderTool3D(CRender3D *pRender);

protected:

	bool		UpdateTranslation( const Vector &vUpdate, UINT = 0 );

private:

	bool		HandleSelection( CMapView *pView, const Vector2D &vPoint );
	void		OverlaySelection( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );

	bool		CreateOverlay( CMapSolid *pSolid, ULONG iFace, CMapView3D *pView, Vector2D point );
	void		InitOverlay( CMapEntity *pEntity, CMapFace *pFace );

	void		OnDrag( Vector const &vecRayStart, Vector const &vecRayEnd, bool bShift );
	void		PreDrag( void );
	void		PostDrag( void );
	void		SetupHandleDragUndo( void );

	void		HandlesReset( void );
	void		SnapHandle( Vector &vecHandlePt );
	bool		HandleInBBox( CMapOverlay *pOverlay, Vector const &vecHandlePt );
	bool		HandleSnap( CMapOverlay *pOverlay, Vector &vecHandlePt );

private:

	bool			m_bDragging;		// Are we dragging overlay handles?
	Shoreline_t		*m_pShoreline;		// 
	CMapOverlay		*m_pActiveOverlay;	// The overlay currently being acted upon
};

#endif // OVERLAY3D_H