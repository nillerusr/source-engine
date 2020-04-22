//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SEARCHREPLACEDLG_H
#define SEARCHREPLACEDLG_H
#ifdef _WIN32
#pragma once
#endif

#include "resource.h"
#include "utlvector.h"
#include "MapClass.h"


class CMapEntity;

struct FindObject_t;


enum FindReplaceIn_t
{
	FindInSelection = 0,
	FindInWorld,
};


class CSearchReplaceDlg : public CDialog
{
// Construction
public:
	CSearchReplaceDlg(CWnd* pParent = NULL);   // standard constructor
	int Create(CWnd *pwndParent = NULL);

// Dialog Data
	//{{AFX_DATA(CSearchReplaceDlg)
	enum { IDD = IDD_SEARCH_REPLACE };
	CString m_strFindText;
	CString m_strReplaceText;
	BOOL m_bVisiblesOnly;
	BOOL m_bWholeWord;
	BOOL m_bCaseSensitive;
	int m_nFindIn;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSearchReplaceDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSearchReplaceDlg)
	afx_msg BOOL OnFindReplace(UINT uCmd);
	virtual void OnOK();
	virtual void OnCancel();
	virtual void OnShowWindow(BOOL bShow, UINT nStatus);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:

	void GetFindCriteria(FindObject_t &FindObject, CMapDoc *pDoc);

	void FindFirst();
	bool FindNext(bool bReplace);

	bool m_bNewSearch;								// Set to true every time the dialog is brought up.
};


#endif // SEARCHREPLACEDLG_H
