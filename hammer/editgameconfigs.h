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

#ifndef EDITGAMECONFIGS_H
#define EDITGAMECONFIGS_H
#pragma once

#include "Options.h"
#include "Resource.h"


class CEditGameConfigs : public CDialog
{
// Construction
public:
	CEditGameConfigs(BOOL bSelectOnly = FALSE,
		CWnd* pParent = NULL);   // standard constructor

	CGameConfig *GetSelectedGame()
	{ return m_pSelectedGame; }	// get selected game config after dialog is run

// Dialog Data
	//{{AFX_DATA(CEditGameConfigs)
	enum { IDD = IDD_EDITGAMECONFIGS };
	CListBox	m_cConfigs;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEditGameConfigs)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CEditGameConfigs)
	afx_msg void OnAdd();
	afx_msg void OnCopy();
	afx_msg void OnRemove();
	afx_msg void OnSelchangeConfigs();
	virtual BOOL OnInitDialog();
	afx_msg void OnDblclkConfigs();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	BOOL m_bSelectOnly;	// just select a game config
	CGameConfig *m_pSelectedGame;	// last selected game

	void FillConfigList(DWORD dwSelectID = 0xffffffff);
};


#endif // EDITGAMECONFIGS_H
