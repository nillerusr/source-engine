//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FILTERCONTROL_H
#define FILTERCONTROL_H
#pragma once


#include "resource.h"
#include "GroupList.h"
#include "HammerBar.h"


class CFilterControl : public CHammerBar
{
public:
	CFilterControl() : CHammerBar() { bInitialized = FALSE; }
	BOOL Create(CWnd *pParentWnd);

	void UpdateGroupList(void);
	void UpdateGroupListChecks(void);

	virtual void OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler);
	virtual CSize CalcDynamicLayout(int nLength, DWORD dwMode);

private:
	//{{AFX_DATA(CFilterControl)
	enum { IDD = IDD_MAPVIEWBAR };
	//}}AFX_DATA

	CBitmapButton m_cMoveUpButton;
	CBitmapButton m_cMoveDownButton;
	CGroupList m_cGroupBox;
	CTabCtrl m_cTabControl;

	BOOL bInitialized;
	BOOL m_bShowingAuto;

protected:

	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT *pResult);
	virtual BOOL OnInitDialog(void);
	void OnSelChangeTab(NMHDR *header, LRESULT *result); 

	//{{AFX_MSG(CFilterControl)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnEditGroups();
	afx_msg void OnMarkMembers();
	afx_msg BOOL OnMoveUpDown(UINT uCmd);
	afx_msg void UpdateControl(CCmdUI *);
	afx_msg void UpdateControlGroups(CCmdUI *pCmdUI);
	afx_msg void OnActivate(UINT nState, CWnd*, BOOL);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnWindowPosChanged(WINDOWPOS *pPos);
	afx_msg void OnEndlabeleditGrouplist(NMHDR*, LRESULT*);
	afx_msg void OnBegindragGrouplist(NMHDR*, LRESULT*);
	afx_msg void OnMousemoveGrouplist(NMHDR*, LRESULT*);
	afx_msg void OnEnddragGrouplist(NMHDR*, LRESULT*);
	afx_msg LRESULT OnListToggleState(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnListLeftDragDrop(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnListRightDragDrop(WPARAM wParam, LPARAM lParam);
	afx_msg void OnShowAllGroups(void);
	//}}AFX_MSG

	CImageList *m_pDragImageList;

	DECLARE_MESSAGE_MAP()
};


#endif // FILTERCONTROL_H
