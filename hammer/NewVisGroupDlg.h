//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A dialog that is invoked when a new visgroup is created.
//
//=============================================================================//

#ifndef NEWVISGROUPDLG_H
#define NEWVISGROUPDLG_H
#ifdef _WIN32
#pragma once
#endif

#include "resource.h"
#include "GroupList.h"


class CNewVisGroupDlg : public CDialog
{
public:
	CNewVisGroupDlg(CString &str, CWnd *pParent = NULL);

	void GetName(CString &str);
	CVisGroup *GetPickedVisGroup(void);
	bool GetRemoveFromOtherGroups(void);
	bool GetHideObjectsOption(void);

	//{{AFX_DATA(CNewVisGroupDlg)
	enum { IDD = IDD_NEW_VISGROUP };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNewVisGroupDlg)
	protected:
	virtual void DoDataExchange(CDataExchange *pDX);
	virtual BOOL OnInitDialog(void);
	//}}AFX_VIRTUAL

protected:

	void UpdateGroupList(void);

	// Generated message map functions
	//{{AFX_MSG(CNewVisGroupDlg)
	virtual void OnOK();
	LRESULT OnSelChangeGroupList(WPARAM wParam, LPARAM lParam);
	void OnCreateNewVisGroup();
	void OnPlaceInExistingVisGroup();
	//}}AFX_MSG

	CGroupList m_cGroupList;
	CVisGroup *m_pPickedVisGroup;
	BOOL m_bRemoveFromOtherGroups;
	BOOL m_bHideObjects;
	CString m_strName;

	DECLARE_MESSAGE_MAP()
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline bool CNewVisGroupDlg::GetRemoveFromOtherGroups(void)
{
	return m_bRemoveFromOtherGroups == TRUE;
}

inline bool CNewVisGroupDlg::GetHideObjectsOption()
{
	return (m_bHideObjects != FALSE);
}

#endif // NEWVISGROUPDLG_H
