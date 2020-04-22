//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a list view for visgroups. Supports drag and drop, and
//			posts a registered windows message to the list view's parent window
//			when visgroups are hidden or shown.
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "GroupList.h"
#include "MapDoc.h"
#include "MapSolid.h"
#include "MapWorld.h"
#include "GlobalFunctions.h"
#include "VisGroup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//
// Timer IDs.
//
enum
{
	TIMER_GROUP_DRAG_SCROLL = 1,
};


static const unsigned int g_uToggleStateMsg = ::RegisterWindowMessage(GROUPLIST_MSG_TOGGLE_STATE);
static const unsigned int g_uLeftDragDropMsg = ::RegisterWindowMessage(GROUPLIST_MSG_LEFT_DRAG_DROP);
static const unsigned int g_uRightDragDropMsg = ::RegisterWindowMessage(GROUPLIST_MSG_RIGHT_DRAG_DROP);
static const unsigned int g_uSelChangeMsg = ::RegisterWindowMessage(GROUPLIST_MSG_SEL_CHANGE);


BEGIN_MESSAGE_MAP(CGroupList, CTreeCtrl)
	//{{AFX_MSG_MAP(CGroupList)
	ON_NOTIFY_REFLECT(TVN_BEGINDRAG, OnBegindrag)
	ON_NOTIFY_REFLECT(TVN_ENDLABELEDIT, OnEndlabeledit)
	ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnSelChange)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_TIMER()
	ON_WM_CONTEXTMENU()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGroupList::CGroupList(void)
{
	m_pDragImageList = NULL;
	m_hDragItem = NULL;
	m_bRButtonDown = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGroupList::~CGroupList(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGroupList::EnableChecks(void)
{
	if (!m_cNormalImageList.GetSafeHandle())
	{
		m_cNormalImageList.Create(IDB_VISGROUPSTATUS, 16, 1, RGB(255, 255, 255));
		m_cNormalImageList.SetOverlayImage(1, 1);
		m_cNormalImageList.SetOverlayImage(2, 2);
	}

	CTreeCtrl::SetImageList(&m_cNormalImageList, TVSIL_NORMAL);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pVisGroup - 
//			hItemParent - 
//-----------------------------------------------------------------------------
void CGroupList::AddVisGroupRecursive(CVisGroup *pVisGroup, HTREEITEM hItemParent)
{
	HTREEITEM hItem = InsertItem(pVisGroup->GetName(), hItemParent, TVI_LAST);
	if (hItem != NULL)
	{
		SetItemData(hItem, (DWORD)pVisGroup);

		// Add the item to our flattened list.
//		VisGroupTreeItem_t item;
//		item.pVisGroup = pVisGroup;
//		item.hItem = hItem;
//		m_TreeItems.AddToTail(item);
		m_VisGroups.AddToTail(pVisGroup);
		
		int nCount = pVisGroup->GetChildCount();
		for (int i = 0; i < nCount; i++)
		{
			CVisGroup *pChild = pVisGroup->GetChild(i);
			AddVisGroupRecursive(pChild, hItem);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pGroup - 
//-----------------------------------------------------------------------------
void CGroupList::AddVisGroup(CVisGroup *pGroup)
{
	AddVisGroupRecursive(pGroup, TVI_ROOT);
}


static void UnsetItemData_R( CTreeCtrl *pCtrl, HTREEITEM hItem )
{
	pCtrl->SetItemData( hItem, 0 );
	
	HTREEITEM hChildItem = pCtrl->GetChildItem( hItem );

	while( hChildItem != NULL )
	{
		UnsetItemData_R( pCtrl, hChildItem );
		hChildItem = pCtrl->GetNextItem(hChildItem, TVGN_NEXT);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGroupList::DeleteAllItems(void)
{
	// Un-set all item data because sometimes during a delete it'll trigger selection change notifications
	// which might crash things later.
	if ( GetSafeHwnd() && m_VisGroups.Count() > 0 )
	{
		UnsetItemData_R( this, TVI_ROOT );
	}
	
	DeleteItem(TVI_ROOT);
	m_VisGroups.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGroupList::EnsureVisible(CVisGroup *pVisGroup)
{
	//DBG("EnsureVisible: %s\n", pVisGroup->GetName());
	HTREEITEM hItem = FindVisGroupItem(pVisGroup);
	if (hItem)
	{
		CTreeCtrl::EnsureVisible(hItem);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGroupList::ExpandRecursive(HTREEITEM hItem)
{
	if (hItem)
	{
		Expand(hItem, TVE_EXPAND);

		if (ItemHasChildren(hItem))
		{
			HTREEITEM hChildItem = GetChildItem(hItem);
			while (hChildItem != NULL)
			{
				ExpandRecursive(hChildItem);
				hChildItem = GetNextItem(hChildItem, TVGN_NEXT);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGroupList::ExpandAll(void)
{
	HTREEITEM hItem = GetRootItem();
	while (hItem)
	{
		ExpandRecursive(hItem);
		hItem = GetNextItem(hItem, TVGN_NEXT);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the tree item in the given subtree associated with the given
//			visgroup, NULL if none.
//-----------------------------------------------------------------------------
HTREEITEM CGroupList::FindVisGroupItemRecursive(HTREEITEM hItem, CVisGroup *pVisGroup)
{
	if (hItem)
	{
		CVisGroup *pVisGroupCheck = (CVisGroup *)GetItemData(hItem);
		if (pVisGroupCheck == pVisGroup)
		{
			return hItem;
		}

		if (ItemHasChildren(hItem))
		{
			HTREEITEM hChildItem = GetChildItem(hItem);
			while (hChildItem != NULL)
			{
				HTREEITEM hFoundItem = FindVisGroupItemRecursive(hChildItem, pVisGroup);
				if (hFoundItem)
				{
					return hFoundItem;
				}

				hChildItem = GetNextItem(hChildItem, TVGN_NEXT);
			}
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the tree item associated with the given visgroup, NULL if none.
//-----------------------------------------------------------------------------
HTREEITEM CGroupList::FindVisGroupItem(CVisGroup *pVisGroup)
{
	HTREEITEM hItem = GetRootItem();
	while (hItem)
	{
		HTREEITEM hFound = FindVisGroupItemRecursive(hItem, pVisGroup);
		if (hFound)
		{
			return hFound;
		}

		hItem = GetNextItem(hItem, TVGN_NEXT);
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the currently selected visgroup in the tree control.
//-----------------------------------------------------------------------------
CVisGroup *CGroupList::GetSelectedVisGroup(void)
{
	HTREEITEM hItem = GetSelectedItem();
	if (hItem)
	{
		return (CVisGroup *)GetItemData(hItem);
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CGroupList::OnLButtonDown(UINT nFlags, CPoint point) 
{
	unsigned int uFlags;
	HTREEITEM hItemHit = HitTest(point, &uFlags);
	if (hItemHit != NULL)
	{
		if (uFlags & TVHT_ONITEMICON)
		{
			// Don't forward to the base if they clicked on the check box.
			// This prevents undesired expansion/collapse of tree.
			return;
		}
	}

	CTreeCtrl::OnLButtonDown(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CGroupList::OnLButtonUp(UINT nFlags, CPoint point) 
{
	KillTimer(TIMER_GROUP_DRAG_SCROLL);
	ReleaseCapture();

	if (!m_hDragItem)
	{
		unsigned int uFlags;
		HTREEITEM hItemHit = HitTest(point, &uFlags);
		if (hItemHit != NULL)
		{
			if (uFlags & TVHT_ONITEMICON)
			{
				//
				// Notify our parent window that this item's state has changed.
				//
				CWnd *pwndParent = GetParent();
				if (pwndParent != NULL)
				{
					int nCheckState = GetCheck(hItemHit);
					if (!nCheckState)
					{
						nCheckState = 1;
					}
					else
					{
						nCheckState = 0;
					}

					CVisGroup *pGroup = (CVisGroup *)GetItemData(hItemHit);
					pwndParent->PostMessage(g_uToggleStateMsg, (WPARAM)pGroup, nCheckState);
				}

				// Don't forward to the base if they clicked on the check box.
				// This prevents undesired expansion/collapse of tree.
				return;
			}
		}

		CTreeCtrl::OnLButtonUp(nFlags, point);
		return;
	}

	Drop(DROP_LEFT, nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CGroupList::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	unsigned int uFlags;
	HTREEITEM hItemHit = HitTest(point, &uFlags);
	if (hItemHit != NULL)
	{
		if (uFlags & TVHT_ONITEMICON)
		{
			// Don't forward to the base if they clicked on the check box.
			// This prevents undesired expansion/collapse of tree.
			return;
		}
	}

	CTreeCtrl::OnLButtonDblClk(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: Forwards selection change notifications to our parent window.
// Input  : pNMHDR - 
//			pResult - 
//-----------------------------------------------------------------------------
void CGroupList::OnSelChange(NMHDR *pNMHDR, LRESULT *pResult)
{
	CWnd *pwndParent = GetParent();
	if (pwndParent != NULL)
	{
		pwndParent->PostMessage(g_uSelChangeMsg, 0, 0);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pNMHDR - 
//			pResult - 
//-----------------------------------------------------------------------------
void CGroupList::OnEndlabeledit(NMHDR *pNMHDR, LRESULT *pResult) 
{
	NMTVDISPINFO *pInfo = (NMTVDISPINFO *)pNMHDR;
	if (!pInfo->item.pszText)
		return;

	CVisGroup *pVisGroup = (CVisGroup *)GetItemData(pInfo->item.hItem);
	Assert(pVisGroup);
	if (!pVisGroup)
		return;

	pVisGroup->SetName(pInfo->item.pszText);
	pResult[0] = TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Begins dragging an item in the visgroup list. The drag image is
//			created and anchored relative to the mouse cursor.
// Input  : pNMHDR - 
//			pResult - 
//-----------------------------------------------------------------------------
void CGroupList::OnBegindrag(NMHDR *pNMHDR, LRESULT *pResult) 
{
	NMTREEVIEW *ptv = (NMTREEVIEW *)pNMHDR;
	BeginDrag(ptv->ptDrag, ptv->itemNew.hItem);
	*pResult = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt - 
//			hItem - 
//-----------------------------------------------------------------------------
void CGroupList::BeginDrag(CPoint point, HTREEITEM hItem) 
{
	m_hDragItem = hItem;
	if (m_hDragItem)
	{
		m_pDragImageList = CreateDragImage(m_hDragItem);
		if (m_pDragImageList)
		{
			CPoint ptHotSpot(0, 0);
			m_pDragImageList->BeginDrag(0, ptHotSpot);
			m_pDragImageList->DragEnter(this, point);
			SelectDropTarget(NULL);
		}

		// Timer handles scrolling the list control when dragging outside the window bounds.
		SetTimer(TIMER_GROUP_DRAG_SCROLL, 300, NULL);

		SetCapture();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CGroupList::OnRButtonDown(UINT nFlags, CPoint point)
{
	m_bRButtonDown = true;
	m_ptRButtonDown = point;
	m_hDragItem = NULL;
	SetCapture();

	// Chaining to the base class causes us never to receive the button up message
	// for a right click without drag, so we don't do that.
	//CTreeCtrl::OnRButtonDown(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pWnd - 
//			point - 
//-----------------------------------------------------------------------------
void CGroupList::OnContextMenu(CWnd *pWnd, CPoint point)
{
	KillTimer(TIMER_GROUP_DRAG_SCROLL);
	ReleaseCapture();

	m_bRButtonDown = false;

	if (!m_hDragItem)
	{
		CTreeCtrl::OnContextMenu(pWnd, point);
		return;
	}

	Drop(DROP_RIGHT, 0, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CGroupList::OnRButtonUp(UINT nFlags, CPoint point)
{
	KillTimer(TIMER_GROUP_DRAG_SCROLL);
	ReleaseCapture();

	m_bRButtonDown = false;

	if (!m_hDragItem)
	{
		CTreeCtrl::OnRButtonUp(nFlags, point);
		return;
	}

	Drop(DROP_RIGHT, nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eDropType - 
//			nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CGroupList::Drop(DropType_t eDropType, UINT nFlags, CPoint point)
{
	SelectDropTarget(NULL);

	HTREEITEM hDragItem = m_hDragItem;
	m_hDragItem = NULL;

	//
	// We are dragging. Drop!
	//
	if (m_pDragImageList)
	{
		m_pDragImageList->DragLeave(this);
		m_pDragImageList->EndDrag();
		delete m_pDragImageList;
		m_pDragImageList = NULL;
	}

	//
	// Get the group that we were dragging.
	//
	CVisGroup *pDragGroup = (CVisGroup *)GetItemData(hDragItem);

	//
	// Determine what group was dropped onto.
	//
	HTREEITEM hDropItem = HitTest(point);
	if (hDropItem == hDragItem)
	{
		return;
	}

	CVisGroup *pDropGroup = NULL;
	if (hDropItem)
	{
		pDropGroup = (CVisGroup *)GetItemData(hDropItem);
	}

	if (pDragGroup == pDropGroup)
	{
		// Shouldn't happen, but just in case.
		return;
	}

	CWnd *pwndParent = GetParent();
	if (pwndParent != NULL)
	{
		if (eDropType == DROP_LEFT)
		{
			pwndParent->PostMessage(g_uLeftDragDropMsg, (WPARAM)pDragGroup, (LPARAM)pDropGroup);
		}
		else
		{
			pwndParent->PostMessage(g_uRightDragDropMsg, (WPARAM)pDragGroup, (LPARAM)pDropGroup);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIDEvent - 
//-----------------------------------------------------------------------------
void CGroupList::OnTimer(UINT nIDEvent) 
{
	//DBG("OnTimer\n");
	switch (nIDEvent)
	{
		case TIMER_GROUP_DRAG_SCROLL:
		{
			CPoint point;
			GetCursorPos(&point);

			CRect rect;
			GetWindowRect(&rect);

			if (!rect.PtInRect(point))
			{
				if (point.y > rect.bottom)
				{
					// scroll down
					int nCount = GetVisibleCount();
					HTREEITEM hItem = GetFirstVisibleItem();
					for (int i = 1; i < nCount; i++)
					{
						hItem = GetNextVisibleItem(hItem);
					}

					hItem = GetNextVisibleItem(hItem);

					if (hItem)
					{
						CTreeCtrl::EnsureVisible(hItem);
					}
				}
				else if (point.y < rect.top)
				{
					HTREEITEM hItem = GetFirstVisibleItem();
					HTREEITEM hPrevVisible = this->GetPrevVisibleItem(hItem);
					if (hPrevVisible)
					{
						// scroll up
						CTreeCtrl::EnsureVisible(hPrevVisible);
					}
				}
			}

			break;
		}
	
		default:
		{
			CTreeCtrl::OnTimer(nIDEvent);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CGroupList::OnMouseMove(UINT nFlags, CPoint point) 
{
	CTreeCtrl::OnMouseMove(nFlags, point);

	if (m_bRButtonDown && !m_hDragItem && (point.x != m_ptRButtonDown.x) && (point.y != m_ptRButtonDown.y))
	{
		// First mouse move since a right button down. Start dragging.
		HTREEITEM hItem = HitTest(m_ptRButtonDown);
		BeginDrag(point, hItem);
	}

	if (!m_hDragItem)
	{
		return;
	}

	if (m_pDragImageList)
	{
		m_pDragImageList->DragMove(point);
	}

	//
	// Highlight the item we hit.
	//
	HTREEITEM hItem = HitTest(point);
	if (hItem == GetDropHilightItem())
	{
		return;
	}

	// hide image first
	if (m_pDragImageList)
	{
		m_pDragImageList->DragLeave(this);
		m_pDragImageList->DragShowNolock(FALSE);
	}

	SelectDropTarget(hItem);

	if (m_pDragImageList)
	{
		m_pDragImageList->DragEnter(this, point);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGroupList::SelectVisGroup(CVisGroup *pVisGroup)
{
	//DBG("SelectVisGroup: %s\n", pVisGroup->GetName());
	HTREEITEM hItem = FindVisGroupItem(pVisGroup);
	if (hItem)
	{
		Select(hItem, TVGN_CARET);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the check status for the given group.
// Input  : pVisGroup - 
//			nCheckState - 0=not checked, 1=checked, -1=gray check (undefined)
//-----------------------------------------------------------------------------
void CGroupList::SetCheck(CVisGroup *pVisGroup, int nCheckState)
{
	HTREEITEM hItem = FindVisGroupItem(pVisGroup);
	if (hItem)
	{
		UINT uState = INDEXTOOVERLAYMASK(0);
		if (nCheckState == 1)
		{
			uState = INDEXTOOVERLAYMASK(1);
		}
		else if (nCheckState != 0)
		{
			uState = INDEXTOOVERLAYMASK(2);
		}

		SetItemState(hItem, uState, TVIS_OVERLAYMASK);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the check state for the given visgroup.
// Input  : pVisGroup - 
//-----------------------------------------------------------------------------
int CGroupList::GetCheck(CVisGroup *pVisGroup)
{
	HTREEITEM hItem = FindVisGroupItem(pVisGroup);
	if (hItem)
	{
		return GetCheck(hItem);
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CGroupList::GetCheck(HTREEITEM hItem)
{
	UINT uState = (GetItemState(hItem, TVIS_OVERLAYMASK) & TVIS_OVERLAYMASK);
	if (uState == INDEXTOOVERLAYMASK(1))
	{
		return 1;
	}
	else if (uState == INDEXTOOVERLAYMASK(0))
	{
		return 0;
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of visgroups in the whole tree.
//-----------------------------------------------------------------------------
int CGroupList::GetVisGroupCount()
{
	return m_VisGroups.Count();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVisGroup *CGroupList::GetVisGroup(int nIndex)
{
	return m_VisGroups.Element(nIndex);
}


//-----------------------------------------------------------------------------
// Purpose: Updates the tree control item text with the new group name.
//-----------------------------------------------------------------------------
void CGroupList::UpdateVisGroup(CVisGroup *pVisGroup)
{
	HTREEITEM hItem = FindVisGroupItem(pVisGroup);
	if (hItem)
	{
		SetItemText(hItem, pVisGroup->GetName());
	}
}

int CGroupList::GetGroupPairCount(void)
{
	return m_GroupPairs.Count();
}

void CGroupList::SaveVisGroupExpandStates()
{
	for ( int i = 0; i < GetVisGroupCount(); i++ )
	{
		CVisGroup *thisGroup = GetVisGroup(i);
		GroupListPair newPair;
		for ( int j = 0; j < GetGroupPairCount(); j++ )
		{
			GroupListPair thisPair = m_GroupPairs.Element( j );
			if ( thisGroup == thisPair.pVisGroup )
			{
				m_GroupPairs.Remove( j );
				break;
			}
		}

		HTREEITEM thisItem = FindVisGroupItem( thisGroup );
		newPair.pVisGroup = thisGroup;
		newPair.bExpanded = false;
		if ( thisItem && (GetItemState( thisItem, TVIS_EXPANDED) & TVIS_EXPANDED) )
		{
			newPair.bExpanded = true;
		}
		m_GroupPairs.AddToTail( newPair );
	}
}

void CGroupList::RestoreVisGroupExpandStates()
{
	ExpandAll();
	for ( int i = 0; i <  GetGroupPairCount(); i++ )
	{
		GroupListPair thisPair = m_GroupPairs.Element( i );
		HTREEITEM thisItem = FindVisGroupItem( thisPair.pVisGroup );
		if ( thisItem )
		{
			if ( thisPair.bExpanded )
			{
				Expand(thisItem, TVE_EXPAND);
			}
			else
			{
				Expand( thisItem, TVE_COLLAPSE );				
			}
		}
	}
}
