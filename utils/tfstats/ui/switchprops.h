//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_SWITCHPROPS_H__D6AFABE1_5BD5_11D3_A5CF_005004039597__INCLUDED_)
#define AFX_SWITCHPROPS_H__D6AFABE1_5BD5_11D3_A5CF_005004039597__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SwitchProps.h : header file
//

#include "PersistentString.h"
/////////////////////////////////////////////////////////////////////////////
// CSwitchProps dialog
class CSwitchProps : public CPropertyPage
{
// Construction
public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	CSwitchProps(CWnd* pParent = NULL);   // standard constructor

	CPersistentString m_persistDefall;
	CPersistentString m_persistDisplayMM2;
	CPersistentString m_persistDisplayStartupInfo;
	CPersistentString m_persistPersistPlayerStats;
	CPersistentString m_persistUseSupportDir;
	CPersistentString m_persistElimPlayers;
	CPersistentString m_persistElimDays;
	CPersistentString m_persistPause;
	CPersistentString m_persistPauseSecs;
	bool alreadyAcknowledged;

// Dialog Data
	//{{AFX_DATA(CSwitchProps)
	enum { IDD = IDD_SWITCHES };
	CStatic	m_OnlyHereToBeDisabledToo;
	CStatic	m_OnlyHereToBeDisabled;
	CButton	m_Pause;
	CEdit	m_PauseSecs;
	CButton	m_Defall;
	CButton	m_DisplayStartupInfo;
	CEdit	m_elimDays;
	CButton	m_ElimOldPlrs;
	CButton	m_UseSupportDir;
	CButton	m_PersistPlayerStats;
	CButton	m_DisplayMM2;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSwitchProps)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSwitchProps)
	afx_msg void OnDefall();
	afx_msg void OnPlrpersist();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SWITCHPROPS_H__D6AFABE1_5BD5_11D3_A5CF_005004039597__INCLUDED_)
