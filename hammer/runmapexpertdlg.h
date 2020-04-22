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

#ifndef RUNMAPEXPERTDLG_H
#define RUNMAPEXPERTDLG_H
#pragma once

#include "RunCommands.h"
#include "MyCheckListBox.h"


class CCommandSequence;


class CRunMapExpertDlg : public CDialog
{
// Construction
public:
	CRunMapExpertDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CRunMapExpertDlg)
	enum { IDD = IDD_RUNMAPEXPERT };
	CButton	m_cUseProcessWnd;
	CComboBox	m_cCmdSequences;
	CButton	m_cMoveUp;
	CButton	m_cMoveDown;
	CEdit	m_cEnsureFn;
	CButton	m_cEnsureCheck;
	CButton	m_cLongFilenames;
	CEdit	m_cParameters;
	CEdit	m_cCommand;
	//}}AFX_DATA

	BOOL m_bSwitchMode;

	CMyCheckListBox	m_cCommandList;
	BOOL m_bNoUpdateCmd;

	// currently active sequence - might be NULL <at startup>
	CCommandSequence *m_pActiveSequence;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRunMapExpertDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	void DeleteCommand(int iIndex);
	void AddCommand(int iIndex, PCCOMMAND pCommand);
	void MoveCommand(int iIndex, BOOL bUp);
	PCCOMMAND GetCommandAtIndex(int *piIndex);
	void UpdateCommandWithEditFields(int iIndex);
	void InitSequenceList();
	void SaveCommandsToSequence();
	LPCTSTR GetCmdString(PCCOMMAND pCommand);

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CRunMapExpertDlg)
	afx_msg void OnBrowsecommand();
	afx_msg void OnSelchangeCommandlist();
	afx_msg void OnInsertparm();
	afx_msg void OnMovedown();
	afx_msg void OnMoveup();
	afx_msg void OnNew();
	afx_msg void OnNormal();
	afx_msg void OnRemove();
	afx_msg void OnUpdateCommand();
	afx_msg void OnUpdateParameters();
	afx_msg void OnEnsurecheck();
	afx_msg void OnUpdateEnsurefn();
	afx_msg void OnLongfilenames();
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnSelchangeConfigurations();
	afx_msg void OnEditconfigs();
	afx_msg void OnUseprocesswnd();
	virtual void OnCancel();
	//}}AFX_MSG

	afx_msg BOOL HandleInsertParm(UINT nID);
	afx_msg BOOL HandleInsertCommand(UINT nID);

	DECLARE_MESSAGE_MAP()
};

#endif // RUNMAPEXPERTDLG_H
