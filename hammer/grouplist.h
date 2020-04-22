//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef GROUPLIST_H
#define GROUPLIST_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"


#define GROUPLIST_MSG_TOGGLE_STATE			"GroupList_ToggleState"
#define GROUPLIST_MSG_LEFT_DRAG_DROP		"GroupList_LeftDragDrop"
#define GROUPLIST_MSG_RIGHT_DRAG_DROP		"GroupList_RightDragDrop"
#define GROUPLIST_MSG_SEL_CHANGE			"GroupList_SelChange"


class CVisGroup;


//
// A structure that maps visgroups to HTREEITEMs so that callers don't have
// to deal with the hierarchical data.
//
//struct VisGroupTreeItem_t
//{
//	CVisGroup *pVisGroup;
//	HTREEITEM hItem;
//};

struct GroupListPair
{
	CVisGroup *pVisGroup;
	bool	  bExpanded;
};

class CGroupList : public CTreeCtrl
{
public:

	CGroupList();
	virtual ~CGroupList();

	void DeleteAllItems();
	void AddVisGroup(CVisGroup *pVisGroup);

	inline bool SubclassDlgItem(int nCtrlID, CWnd *pwndParent);
	inline void SetRedraw(bool bRedraw);
	inline void Invalidate(bool bErase = true);

	void SelectVisGroup(CVisGroup *pVisGroup);
	void EnsureVisible(CVisGroup *pVisGroup);

	void ExpandAll(void);
	void EnableChecks(void);

	void UpdateVisGroup(CVisGroup *pVisGroup);

	CVisGroup *GetSelectedVisGroup();
	
	int GetVisGroupCount(void);
	CVisGroup *GetVisGroup(int nIndex);
	void SetCheck(CVisGroup *pVisGroup, int nCheckState);
	int GetCheck(CVisGroup *pVisGroup);

	int GetGroupPairCount(void);
	void SaveVisGroupExpandStates();
	void RestoreVisGroupExpandStates();

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGroupList)
	//}}AFX_VIRTUAL

protected:

	// Generated message map functions
	//{{AFX_MSG(CGroupList)
	afx_msg void OnBegindrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndlabeledit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnContextMenu(CWnd *, CPoint);
	afx_msg void OnSelChange(NMHDR *pNMHDR, LRESULT *pResult);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:

	enum DropType_t
	{
		DROP_LEFT = 0,
		DROP_RIGHT,
	};

	void ExpandRecursive(HTREEITEM hItem);

	void AddVisGroupRecursive(CVisGroup *pGroup, HTREEITEM hItemParent);

	HTREEITEM FindVisGroupItem(CVisGroup *pVisGroup);
	HTREEITEM FindVisGroupItemRecursive(HTREEITEM hItem, CVisGroup *pVisGroup);

	int GetCheck(HTREEITEM hItem);

	HTREEITEM TreeItemForVisGroup(CVisGroup *pVisGroup);

	void BeginDrag(CPoint pt, HTREEITEM hItem);
	void Drop(DropType_t eDropType, UINT nFlags, CPoint point);

	bool m_bRButtonDown;
	CPoint m_ptRButtonDown;

	CPoint m_ptLDown;

	CImageList m_cNormalImageList;
	CImageList *m_pDragImageList;

	HTREEITEM m_hDragItem;

	CUtlVector<CVisGroup *> m_VisGroups;
	CUtlVector<GroupListPair> m_GroupPairs;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGroupList::Invalidate(bool bErase)
{
	CTreeCtrl::Invalidate(bErase ? TRUE : FALSE);
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables updates. Useful for populating the groups list
//			and only updating at the end.
//-----------------------------------------------------------------------------
void CGroupList::SetRedraw(bool bRedraw)
{
	CTreeCtrl::SetRedraw(bRedraw ? TRUE : FALSE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nCtrlID - 
//			*pwndParent - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGroupList::SubclassDlgItem(int nCtrlID, CWnd *pwndParent)
{
	return (CTreeCtrl::SubclassDlgItem(nCtrlID, pwndParent) == TRUE);
}


#endif // GROUPLIST_H
