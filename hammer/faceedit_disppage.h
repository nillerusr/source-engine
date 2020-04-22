//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FACEEDIT_DISPPAGE_H
#define FACEEDIT_DISPPAGE_H
#pragma once

#include "resource.h"
#include "DispDlg.h"

class CMapSolid;
class CMapFace;
class CMapDisp;

//=============================================================================
//
// Face Edit Displacement Page Class
//
class CFaceEditDispPage : public CPropertyPage
{

	DECLARE_DYNAMIC( CFaceEditDispPage );

public:

	enum {	FACEEDITTOOL_SELECT = 0,
		    FACEEDITTOOL_CREATE,
			FACEEDITTOOL_DESTROY,
			FACEEDITTOOL_PAINTGEO,
			FACEEDITTOOL_PAINTDATA,
			FACEEDITTOOL_PAINTSCULPT,
			FACEEDITTOOL_SEW,
			FACEEDITTOOL_SUBDIV,
			FACEEDITTOOL_NOISE,
			FACEEDITTOOL_TAG_WALK,
			FACEEDITTOOL_TAG_BUILD,
			FACEEDITTOOL_TAG_REMOVE,
	};

public:

	CFaceEditDispPage();
	~CFaceEditDispPage();

	void ClickFace( CMapSolid *pSolid, int faceIndex, int cmd, int clickMode = -1 );	// primary interface update call
	void Apply( void );

	void UpdateDialogData( void );
	void CloseAllDialogs( void );
	void ResetForceShows( void );

	void SetTool( unsigned int tool );
	unsigned int GetTool( void )				{ return m_uiTool; }

	void UpdatePaintDialogs( void );

	//{{AFX_DATA( CFaceEditDispPage )
	enum { IDD = IDD_FACEEDIT_DISP };
	//}}AFX_DATA

	//{{AFX_VIRTUAL( CFaceEditDispPage )
	BOOL OnSetActive( void );
	BOOL OnKillActive( void );
	virtual BOOL PreTranslateMessage( MSG *pMsg );
	//}}AFX_VIRTUAL

protected:

	unsigned int		m_uiTool;

	CDispCreateDlg		m_CreateDlg;
	CDispNoiseDlg		m_NoiseDlg;
	CDispPaintDistDlg	m_PaintDistDlg;
	CDispPaintDataDlg	m_PaintDataDlg;
	CPaintSculptDlg		m_PaintSculptDlg;

	bool				m_bForceShowWalkable;
	bool				m_bForceShowBuildable;
	bool				m_bIsEditable;

protected:

	inline void PostToolUpdate( void );

	void FillEditControls( bool bAllDisps );
	void UpdateEditControls( bool bAllDisps, bool bHasFace );

	void UpdatePower( CMapDisp *pDisp );
	void UpdateElevation( CMapDisp *pDisp );
	void UpdateScale( CMapDisp *pDisp );

	//=========================================================================
	//
	// Message Map
	//
	//{{AFX_MSG( CFaceEditDispPage )
	afx_msg void OnCheckMaskSelect( void );
	afx_msg void OnCheckMaskGrid( void );

	afx_msg void OnCheckNoPhysicsCollide( void );
	afx_msg void OnCheckNoHullCollide( void );
	afx_msg void OnCheckNoRayCollide( void );

	afx_msg void OnButtonSelect( void );
	afx_msg void OnButtonCreate( void );
	afx_msg void OnButtonDestroy( void );
	afx_msg void OnButtonNoise( void );
	afx_msg void OnButtonSubdivide( void );
	afx_msg void OnButtonSew( void );
	afx_msg void OnButtonPaintGeo( void );
	afx_msg void OnButtonPaintData( void );
	afx_msg void OnButtonTagWalkable( void );
	afx_msg void OnButtonTagBuildable( void );
	afx_msg void OnButtonTagRemove( void );
	afx_msg void OnSelectAdjacent();
	afx_msg void OnButtonInvertAlpha( void );

	afx_msg void OnSpinUpDown( NMHDR *pNMHDR, LRESULT *pResult );
	afx_msg void OnButtonApply( void );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedDispSculptPaint( );
};


#endif // FACEDIT_DISPPAGE_H