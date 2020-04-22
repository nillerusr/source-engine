//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MATERIALDLG_H
#define MATERIALDLG_H
#pragma once

#include "resource.h"
#include "smoothinggroupmgr.h"

//=============================================================================
//
// Face Smoothing Group Dialog
//
class CFaceSmoothingDlg : public CDialog
{
public:

	CFaceSmoothingDlg( CWnd *pParent = NULL );
	~CFaceSmoothingDlg();

	void UpdateControls( void );

	//{{AFX_DATA( CFaceSmoothingDlg )
	enum { IDD = IDD_SMOOTHING_GROUPS };
	//}}AFX_DATA

	//{{AFX_VIRTUAL( CFaceSmoothingDlg )
	virtual BOOL OnInitDialog( void );
	//}}AFX_VIRTUAL

protected:

	void InitButtonIDs( void );

	UINT GetSmoothingGroup( UINT uCmd );
	int GetActiveSmoothingGroup( void );

	void CheckGroupButtons( int *pGroupCounts, int nFaceCount );

	float GetEditBoxSmoothingAngle( void );
	void SetEditBoxSmoothingAngle( float flAngle );
	void UpdateSmoothingAngle( int *pGroupCounts, int nFaceCount );

	//{{AFX_MSG( CFaceSmoothingDlg )
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnButtonApply( void );
	afx_msg BOOL OnButtonGroup( UINT uCmd );
	afx_msg void OnClose( void );
	afx_msg void OnDestroy( void );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:

	UINT			m_ButtonIDs[MAX_SMOOTHING_GROUP_COUNT+1];			// Ids
	CRect			m_DialogPosRect;									// Save/Restore the window position.

	CBrush			m_Brush;
	bool			m_bColorOverride;
	bool			m_bColorSelectAll;
};


//=============================================================================
//
// Face Smoothing Group Visual Dialog
//
class CFaceSmoothingVisualDlg : public CDialog
{
public:

	CFaceSmoothingVisualDlg( CWnd *pParent = NULL );
	~CFaceSmoothingVisualDlg();

	//{{AFX_DATA( CFaceSmoothingVisualDlg )
	enum { IDD = IDD_SMOOTHING_GROUP_VISUAL };
	//}}AFX_DATA

	//{{AFX_VIRTUAL( CFaceSmoothingVisualDlg )
	virtual BOOL OnInitDialog( void );
	//}}AFX_VIRTUAL

protected:

	void InitButtonIDs( void );
	UINT GetSmoothingGroup( UINT uCmd );

	//{{AFX_MSG( CFaceSmoothingDlg )
	afx_msg BOOL OnButtonGroup( UINT uCmd );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:

	UINT			m_ButtonIDs[MAX_SMOOTHING_GROUP_COUNT+1];			// Ids
};

#endif // MATERIALDLG_H