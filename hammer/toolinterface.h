//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the interface that tools implement to allow views to call
//			through them.
//
//===========================================================================//

#ifndef TOOLINTERFACE_H
#define TOOLINTERFACE_H
#ifdef _WIN32
#pragma once
#endif


class CMapView2D;
class CMapView3D;
class CMapViewLogical;
class CRender2D;
class CRender3D;
class CMapDoc;
class CMapClass;
class Vector2D;
class CChunkFile;
class CSaveInfo;

#define	HANDLE_RADIUS		4

enum ChunkFileResult_t;


enum ToolID_t
{
	TOOL_NONE = -1,
	TOOL_POINTER,
	TOOL_BLOCK,
	TOOL_ENTITY,
	TOOL_CAMERA,
	TOOL_DECAL,
	TOOL_MAGNIFY,
	TOOL_MORPH,
	TOOL_CLIPPER,
	TOOL_EDITCORDON,
	TOOL_FACEEDIT_MATERIAL,
	TOOL_FACEEDIT_DISP,
	TOOL_OVERLAY,
	TOOL_AXIS_HANDLE,
	TOOL_POINT_HANDLE,
	TOOL_SPHERE,
	TOOL_PICK_FACE,
	TOOL_PICK_ENTITY,
	TOOL_PICK_ANGLES,
	TOOL_SWEPT_HULL,
	TOOL_PLAYERHULL_HANDLE,
};

enum
{
	constrainNone		= 0x00,	// transformation with no constrains
	constrainOnlyHorz	= 0x01,	// only horizontal translations
	constrainOnlyVert	= 0x02, // only vertical translations
	constrainSnap		= 0x04,	// rounds to document snap grid
	constrainIntSnap	= 0x08,	// rounds value to one unit (integer)
	constrainHalfSnap	= 0x10, // rounds to half of snap grid
	constrainCenter		= 0x20,
	constrainMoveAll	= 0x40, // translate all handles
};


class CBaseTool
{
public:

	inline CBaseTool();
    virtual ~CBaseTool() {}

	//
	// Called by the tool manager to activate/deactivate tools.
	//

	virtual void Init( CMapDoc *pDocument );

    void Activate();
    void Deactivate();
	virtual bool CanDeactivate( void ) { return true; }

	virtual bool IsTranslating(void) { return false; }	// return true if tool is currently changing objects
	inline bool IsActiveTool( void ) { return m_bActiveTool; }

	// true if tool has objects to work on
	virtual bool IsEmpty() { return m_bEmpty; }

	// detach tool from any object working on
	virtual void SetEmpty() { m_bEmpty = true; }

	// attach a certain object to that tool
	virtual void Attach(CMapClass *pObject) {};

	//
	// Notifications for tool activation/deactivation.
	//
    virtual void OnActivate() {}
    virtual void OnDeactivate() {}

	virtual ToolID_t GetToolID(void) { return TOOL_NONE; }

	virtual const char* GetVMFChunkName() { return NULL; }
	virtual ChunkFileResult_t LoadVMF(CChunkFile *pFile) { return (ChunkFileResult_t)0; /*ChunkFile_Ok*/ }
	virtual ChunkFileResult_t SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo) { return (ChunkFileResult_t)0 ; /*ChunkFile_Ok*/ }
	//
	// Messages sent by the 3D view:
	//
	virtual bool OnContextMenu3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint) { return false; }
	virtual bool OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnLMouseDblClk3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnMouseWheel3D( CMapView3D *pView, UINT nFlags, short zDelta, const Vector2D &vPoint) { return false; }

	virtual bool OnKeyDown3D( CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags ) { return false; }
	virtual bool OnKeyUp3D( CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags ) { return false; }
	virtual bool OnChar3D( CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags ) { return false; }

	//
	// Messages sent by the 2D view:
	//
	virtual bool OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) { return false; }
	virtual bool OnLMouseDown2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnLMouseUp2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnLMouseDblClk2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnRMouseDown2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnRMouseUp2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnMouseMove2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint ) { return true; }
	virtual bool OnMouseWheel2D( CMapView2D *pView, UINT nFlags, short zDelta, const Vector2D &vPoint) { return false; }

	virtual bool OnKeyDown2D( CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags ) { return false; }
	virtual bool OnKeyUp2D( CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags ) { return false; }
	virtual bool OnChar2D( CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags ) { return false; }

	//
	// Messages sent by the logical view:
	//
	virtual bool OnContextMenuLogical( CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint) { return false; }
	virtual bool OnLMouseDownLogical( CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnLMouseUpLogical( CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnLMouseDblClkLogical( CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnRMouseDownLogical( CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnRMouseUpLogical( CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint ) { return false; }
	virtual bool OnMouseMoveLogical( CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint ) { return true; }
	virtual bool OnMouseWheelLogical( CMapViewLogical *pView, UINT nFlags, short zDelta, const Vector2D &vPoint) { return false; }

	virtual bool OnKeyDownLogical( CMapViewLogical *pView, UINT nChar, UINT nRepCnt, UINT nFlags ) { return false; }
	virtual bool OnKeyUpLogical( CMapViewLogical *pView, UINT nChar, UINT nRepCnt, UINT nFlags ) { return false; }
	virtual bool OnCharLogical( CMapViewLogical *pView, UINT nChar, UINT nRepCnt, UINT nFlags ) { return false; }

	//
	// Rendering.
	//
	virtual void RenderTool2D( CRender2D *pRender ) {}
	virtual void RenderToolLogical( CRender2D *pRender ) {}
	virtual void RenderTool3D( CRender3D *pRender ) {}
	virtual void UpdateStatusBar( void ) {}

protected:

	bool m_bActiveTool;		// Set to true when this is the active tool.
	bool m_bEmpty;		// true if the tool has objects to work on
	CMapDoc *m_pDocument;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CBaseTool::CBaseTool()
{
	m_bEmpty = true;
	m_bActiveTool = false;
	m_pDocument = NULL;
}

#endif // TOOLINTERFACE_H
