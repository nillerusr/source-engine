//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef OP_GROUPS_H
#define OP_GROUPS_H
#ifdef _WIN32
#pragma once
#endif


#include "resource.h"
#include "GroupList.h"
#include "ObjectPage.h"
#include "AnchorMgr.h"


class COP_Groups : public CObjectPage
{
	DECLARE_DYNCREATE(COP_Groups)

public:
	COP_Groups();
	~COP_Groups();

	virtual bool SaveData(void);
	virtual void UpdateData( int Mode, PVOID pData, bool bCanEdit );

	void SetMultiEdit(bool b);
	void UpdateGroupList(void);

	CAnchorMgr m_AnchorMgr;
	CMapClass *pUpdateObject;

	//{{AFX_DATA(COP_Groups)
	enum { IDD = IDD_OBJPAGE_GROUPS };
	CGroupList m_cGroups;
	CButton	m_EditGroupsControl;
	//}}AFX_DATA

	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COP_Groups)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

protected:

	// Generated message map functions
	//{{AFX_MSG(COP_Groups)
	afx_msg void OnEditgroups();
	afx_msg void OnSetFocus(CWnd *pOld);
	afx_msg LRESULT OnListToggleState(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize( UINT nType, int cx, int cy );
	//}}AFX_MSG

	
	DECLARE_MESSAGE_MAP()

};


#endif // OP_GROUPS_H
