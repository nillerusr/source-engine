//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef DISPVIEW_H
#define DISPVIEW_H
#pragma once

//=============================================================================

#include "Render3D.h"
#include "Camera.h"
#include "Keyboard.h"

//=============================================================================

enum
{
    FORWARD = 0,
    STRAFE,
    VERTICAL
};

//=============================================================================

class CDispView : public CMapView
{
	DECLARE_DYNCREATE( CDispView )

public:

    enum DispViewType_t
    {
        DISPVIEW_IMAGE = 0,
        DISPVIEW_OUTLINED_IMAGE,
    };

	virtual ~CDispView();

    void Render( void );

    CMapDoc* GetDocument( void );
	void Activate( BOOL bActivate );

    void ToggleCameraMode( void );
    void ProcessInput( void );

	//{{AFX_VIRTUAL( CDispView )
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnDraw( CDC *pDC );
	virtual BOOL PreCreateWindow( CREATESTRUCT &cs );
	//}}AFX_VIRTUAL

protected:

	CDispView();

    CRender3D       *m_pRender;	            // view renderer
    CCamera         *m_pCamera;		        // view camera
    DispViewType_t  m_eDispViewType;        // type of displacement view

    bool            m_bCameraEnable;        // view in camera mode

	CKeyboard		m_Keyboard;				// handles binding of keys and mouse buttons to logical functions
    DWORD           m_TimeLastInputSample;	// used for framerate-independent input processing.
    float           m_Speed[3];             // speed in world units/sec = forward, side-to-side, and up-down
    float           m_SpeedMax[3];          // max speed in world units/sec
    float           m_Accel[3];             // accel in world units/sec
    bool            m_bLMBDown;             // is the left mouse button down?
	bool			m_bRMBDown;				// is the right mouse button down?

	void InitializeKeyMap( void );
	void ProcessKeys( float elapsedTime );
	void ProcessMovementKeys( float elapsedTime );
    float Accelerate( float vel, float accel, float accelScale, float timeScale, float velMax);
    void LockCursorInCenter( bool bLock );

    void ApplyDispTool( UINT nFlags, CPoint point, bool bLMBDown );
    bool OnSelection( UINT nFlags, CPoint point );

    bool InitRenderer( void );
    bool InitCamera( void );
    bool AllocRenderer( void );
    void FreeRenderer( void );
    bool AllocCamera( void );
    void FreeCamera( void );

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump( CDumpContext &dc ) const;
#endif

	//{{AFX_MSG( CDispView )
	afx_msg void OnChar( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg BOOL OnEraseBkgnd( CDC *pDC );
	afx_msg void OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnKeyUp( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnRButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnRButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnTimer(UINT nIDEvent);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


inline CMapDoc *CDispView::GetDocument( void )
{
	return( ( CMapDoc* )m_pDocument );
}


#endif // DISPVIEW_H
