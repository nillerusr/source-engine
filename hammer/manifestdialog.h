//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MANIFESTDIALOG_H
#define MANIFESTDIALOG_H
#pragma once

#include "afxcmn.h"
#include "afxwin.h"
#include "HammerBar.h"

class CManifestMap;


// CManifestMove dialog
class CManifestMove : public CDialog
{
	DECLARE_DYNAMIC(CManifestMove)

public:
	CManifestMove( bool bIsMove, CWnd* pParent = NULL );   // standard constructor
	virtual ~CManifestMove();

	void	GetFriendlyName( CString &Result ) { Result = m_FriendlyName; }
	void	GetFileName( CString &Result ) { Result = m_FileName; }
	bool	GetCenterContents( void ) { return m_CenterContents; }

// Dialog Data
	enum { IDD = IDD_MANIFEST_MOVE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CEdit m_FileNameControl;
	CButton m_CenterContentsControl;
	CEdit m_FriendlyNameControl;
	CStatic m_FullPathNameControl;
	bool	m_bIsMove;

protected:
	virtual void OnOK();

	CString		m_FriendlyName;
	CString		m_FileName;
	bool		m_CenterContents;
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnEnChangeManifestFilename();
};

class CManifestListBox : public CListBox
{
public:
	CManifestListBox( void ); 

	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );
	virtual void MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct );
	virtual int CompareItem( LPCOMPAREITEMSTRUCT lpCompareItemStruct );

private:
	CImageList		m_Icons;
	CMenu			m_ManifestFilterMenu, m_ManifestFilterSecondaryMenu, m_ManifestFilterPrimaryMenu, m_ManifestFilterBlankMenu;
	CManifestMap	*m_pTrackerManifestMap;

protected:
	//{{AFX_MSG(CManifestListBox)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnMoveSelectionToSubMap();
	afx_msg void OnMoveSelectionToNewSubMap();
	afx_msg void OnVersionControlCheckOut();
	afx_msg void OnVersionControlCheckIn();
	afx_msg void OnVersionControlAdd();
	afx_msg void OnInsertEmptySubMap();
	afx_msg void OnInsertExistingSubMap();
	afx_msg void OnManifestProperties();
	afx_msg void OnManifestRemove();
};


// CManifestFilter dialog

class CManifestFilter : public CHammerBar
{
public:
	CManifestFilter() : CHammerBar() { bInitialized = FALSE; }
	BOOL Create(CWnd *pParentWnd);

	virtual ~CManifestFilter();

	void UpdateManifestList( void );

// Dialog Data
	enum { IDD = IDD_MANIFEST_CONTROL };

private:
	BOOL				bInitialized;
	CManifestListBox	m_ManifestList;
	CBrush				*m_pBkBrush;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	//{{AFX_MSG(CManifestFilter)
	afx_msg void OnLbnSelchangeManifestList();
	afx_msg void OnLbnDblClkManifestList();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
};


// CManifestMapDlg dialog

class CManifestMapDlg : public CDialog
{
	DECLARE_DYNAMIC(CManifestMapDlg)

public:
	CManifestMapDlg( CManifestMap *pManifestMap, CWnd* pParent = NULL );   // standard constructor
	virtual ~CManifestMapDlg();

	// Dialog Data
	enum { IDD = IDD_MANIFEST_MAP };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();

private:
	CManifestMap	*m_pManifestMap;

public:
	CEdit m_FriendlyNameControl;
protected:
	virtual void OnOK();
public:
	CStatic m_FullFileNameCtrl;
};

// CManifestCheckin dialog

class CManifestCheckin : public CDialog
{
	DECLARE_DYNAMIC(CManifestCheckin)

public:
	CManifestCheckin(CWnd* pParent = NULL);   // standard constructor
	virtual ~CManifestCheckin();

	// Dialog Data
	enum { IDD = IDD_MANIFEST_CHECKIN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedOk();

	CListCtrl m_CheckinListCtrl;
	CEdit m_DescriptionCtrl;
};

#endif // MANIFESTDIALOG_H

