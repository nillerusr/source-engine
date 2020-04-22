//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPVIEW3D_H
#define MAPVIEW3D_H
#ifdef _WIN32
#pragma once
#endif

#include "Keyboard.h"
#include "MapView.h"
#include "Render3D.h"
#include "camera.h"

namespace vgui
{
	typedef unsigned long HCursor;
}


class CMapAtom;
class CRender3D;
class CCamera;
class CTitleWnd;
class CMapDecal;

struct PLANE;


class CMapView3D : public CView, public CMapView
{
protected:

	CMapView3D();
	DECLARE_DYNCREATE(CMapView3D)

public:

	virtual LRESULT WindowProc( UINT message, WPARAM wParam, LPARAM lParam );
	enum
	{
		updNothing = 0x00,
		updMorphOnly = 0x01,
		updAll = 0x02,
		updRedrawNow = 0x04
	};

	void SetCamera(const Vector &vecPos, const Vector &vecLookAt);

	//
	// CMapView interface:
	//
	void RenderView();
	bool ShouldRender();
	void ActivateView(bool bActivate);
	void UpdateView(int nFlags);
	CView *GetViewWnd() { return (CView*)this; }
	CMapDoc *GetMapDoc() { return (CMapDoc*)m_pDocument;}
	void WorldToClient(Vector2D &ptClient, const Vector &vecWorld);
	void ClientToWorld(Vector &vecWorld, const Vector2D &ptClient);
	bool HitTest( const Vector2D &vPoint, const Vector& mins, const Vector& maxs );
	void GetBestTransformPlane( Vector &horzAxis, Vector &vertAxis, Vector &thirdAxis);
				
	void GetHitPos(const Vector2D &point, PLANE &plane, Vector &pos);
	void ProcessInput(void);

	void UpdateStatusBar();

	// Called by the camera tool to control the camera.
	void EnableMouseLook(bool bEnable);
	void EnableRotating(bool bEnable);
	void EnableStrafing(bool bEnable);
	void UpdateCameraVariables(void);

	void MoveForward(float flDistance);
	void MoveUp(float flDistance);
	void MoveRight(float flDistance);
	void Pitch(float flDegrees);
	void Yaw(float flDegrees);

	void BeginPick(void);
	void EndPick(void);

	DrawType_t GetDrawType() { return m_eDrawType; }
	void SetDrawType(DrawType_t eDrawType);

	int			ObjectsAt( const Vector2D &point, HitInfo_t *pObjects, int nMaxObjects, unsigned int nFlags = 0 );

	CMapClass	*NearestObjectAt( const Vector2D &point, ULONG &ulFace, unsigned int nFlags = 0, VMatrix *pLocalMatrix = NULL );
		
	void RenderPreloadObject(CMapAtom *pObject);

	void SetCursor( vgui::HCursor hCursor );

	// Release all video memory
	void ReleaseVideoMemory();

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMapView3D)
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL DestroyWindow();
	virtual void OnInitialUpdate();
protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	//}}AFX_VIRTUAL

public:
	virtual ~CMapView3D();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

private:

	void Render();

	void EnableCrosshair(bool bEnable);
		
	
	bool ControlCamera(const CPoint &point);

	//
	// Keyboard processing.
	//
	void InitializeKeyMap(void);
	void ProcessMouse(void);
	void ProcessKeys(float fElapsedTime);
	void ProcessMovementKeys(float fElapsedTime);
	float GetKeyScale(unsigned int uKeyState);

	// Radius culling
	void ProcessCulling( void );

	enum
	{
		MVTIMER_PICKNEXT = 0
	};

	bool m_bMouseLook;				// Set to true when we override the mouse processing to use mouselook.
	bool m_bStrafing;
	bool m_bRotating;
	CPoint m_ptLastMouseMovement;	// Last position used for tracking the mouse for camera control.

	DWORD m_dwTimeLastSample;		// Used for calculating rendering framerate.
	DWORD m_dwTimeLastInputSample;	// Used for framerate-independent input processing.

	float m_fForwardSpeed;			// Current forward speed, in world units per second.
	float m_fStrafeSpeed;			// Current side-to-side speed, in world units per second.
	float m_fVerticalSpeed;			// Current up-down speed, in world units per second.

	float m_fForwardSpeedMax;		// Maximum forward speed, in world units per second.
	float m_fStrafeSpeedMax;		// Maximum side-to-side speed, in world units per second.
	float m_fVerticalSpeedMax;		// Maximum up-down speed, in world units per second.

	float m_fForwardAcceleration;	// Forward acceleration, in world units per second squared.
	float m_fStrafeAcceleration;	// Side-to-side acceleration, in world units per second squared.
	float m_fVerticalAcceleration;	// Up-down acceleration, in world units per second squared.

	DrawType_t m_eDrawType;			// How we render - wireframe, flat, textured, lightmap grid, or lighting preview.
	bool m_bLightingPreview;

	CTitleWnd *m_pwndTitle;	// Title window.

	CRender3D *m_pRender;	// Performs the 3D rendering in our window.
	CKeyboard m_Keyboard;	// Handles binding of keys and mouse buttons to logical functions.

	bool m_bCameraPosChanged;
	bool m_bClippingChanged;

	//{{AFX_MSG(CMapView3D)
protected:
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown( UINT nFlags, CPoint point );
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnDraw(CDC *pDC);
	afx_msg BOOL OnFaceAlign(UINT uCmd);
	afx_msg BOOL OnFaceJustify(UINT uCmd);
	afx_msg void OnView3dWireframe(void);
	afx_msg void OnView3dPolygon(void);
	afx_msg void OnView3dTextured(void);
	afx_msg void OnView3dLightmapGrid(void);
	afx_msg void OnView3dLightingPreview(void);
	afx_msg void OnView3dLightingPreviewRayTraced(void);
	//afx_msg void OnView3dEngine(void);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	afx_msg void OnNcPaint( );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
		};

#endif // MAPVIEW3D_H
