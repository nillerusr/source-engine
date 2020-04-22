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

#ifndef OPTGENERAL_H
#define OPTGENERAL_H
#pragma once

#include "Resource.h"


class COPTGeneral : public CPropertyPage
{
	DECLARE_DYNCREATE(COPTGeneral)

// Construction
public:
	COPTGeneral();
	~COPTGeneral();

// Dialog Data
	//{{AFX_DATA(COPTGeneral)
	enum { IDD = IDD_OPTIONS_MAIN };
	CButton	m_cLoadWinPos;
	CButton	m_cIndependentWin;
	CButton m_cEnableAutosave;
	CStatic	m_cAutosaveTimeLabel;
	CEdit	m_cAutosaveTime;
	CStatic m_cAutosaveSpaceLabel;
	CEdit	m_cAutosaveSpace;
	CStatic m_cAutosaveIterationLabel;
	CEdit	m_cAutosaveIterations;
	CStatic	m_cAutosaveDirectoryLabel;
	CButton m_cAutosaveBrowseButton;
	CSpinButtonCtrl	m_UndoSpin;
	int		m_iUndoLevels;
	int    	m_nMaxCameras;
	int		m_iMaxAutosavesPerMap;
	int 	m_iMaxAutosaveSpace;
	int		m_iTimeBetweenSaves;
	CEdit	m_cAutosaveDir;
	//}}AFX_DATA

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COPTGeneral)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COPTGeneral)
	virtual BOOL OnInitDialog(void);
	afx_msg void OnIndependentwindows(void);
	afx_msg void OnEnableAutosave(void);
	afx_msg void OnBrowseAutosaveDir(void);
	//}}AFX_MSG

	BOOL BrowseForFolder(char *pszTitle, char *pszDirectory);
	BOOL OnApply();
	
	DECLARE_MESSAGE_MAP()

	
	
};


#endif // OPTGENERAL_H
