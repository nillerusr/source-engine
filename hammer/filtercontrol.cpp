//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "FilterControl.h"
#include "ControlBarIDs.h"
#include "MapWorld.h"
#include "GlobalFunctions.h"
#include "EditGroups.h"
#include "CustomMessages.h"
#include "VisGroup.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


typedef struct
{
	CVisGroup *pGroup;
	CMapDoc *pDoc;
	SelectMode_t eSelectMode;
} MARKMEMBERSINFO;


static const unsigned int g_uToggleStateMsg = ::RegisterWindowMessage(GROUPLIST_MSG_TOGGLE_STATE);
static const unsigned int g_uLeftDragDropMsg = ::RegisterWindowMessage(GROUPLIST_MSG_LEFT_DRAG_DROP);
static const unsigned int g_uRightDragDropMsg = ::RegisterWindowMessage(GROUPLIST_MSG_RIGHT_DRAG_DROP);


BEGIN_MESSAGE_MAP(CFilterControl, CHammerBar)
	//{{AFX_MSG_MAP(CFilterControl)
	ON_BN_CLICKED(IDC_EDITGROUPS, OnEditGroups)
	ON_BN_CLICKED(IDC_MARKMEMBERS, OnMarkMembers)
	ON_BN_CLICKED(IDC_SHOW_ALL, OnShowAllGroups)
	ON_COMMAND_EX(IDC_VISGROUP_MOVEUP, OnMoveUpDown)
	ON_COMMAND_EX(IDC_VISGROUP_MOVEDOWN, OnMoveUpDown)
	ON_UPDATE_COMMAND_UI(IDC_GROUPS, UpdateControlGroups)
	ON_UPDATE_COMMAND_UI(IDC_EDITGROUPS, UpdateControl)
	ON_UPDATE_COMMAND_UI(IDC_MARKMEMBERS, UpdateControl)
	ON_UPDATE_COMMAND_UI(IDC_SHOW_ALL, UpdateControl)
	ON_UPDATE_COMMAND_UI(IDC_VISGROUP_MOVEUP, UpdateControl)
	ON_UPDATE_COMMAND_UI(IDC_VISGROUP_MOVEDOWN, UpdateControl)		
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, OnSelChangeTab)
	ON_WM_ACTIVATE()
	ON_WM_SHOWWINDOW()
	ON_WM_SIZE()
	ON_WM_WINDOWPOSCHANGED()
	ON_REGISTERED_MESSAGE(g_uToggleStateMsg, OnListToggleState)
	ON_REGISTERED_MESSAGE(g_uLeftDragDropMsg, OnListLeftDragDrop)
	ON_REGISTERED_MESSAGE(g_uRightDragDropMsg, OnListRightDragDrop)	
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pParentWnd - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CFilterControl::Create(CWnd *pParentWnd)
{
	if (!CHammerBar::Create(pParentWnd, IDD_FILTERCONTROL, CBRS_RIGHT | CBRS_SIZE_DYNAMIC, IDCB_FILTERCONTROL, "Filter Control"))
		
	{
		return FALSE;
	}

	m_cGroupBox.SubclassDlgItem(IDC_GROUPS, this);	
	m_cGroupBox.EnableChecks();

	m_cTabControl.SubclassDlgItem(IDC_TAB1, this);
	m_cTabControl.InsertItem(0, "User");
	m_cTabControl.InsertItem(1, "Auto");

	//
	// Set up button icons.
	//
	CWinApp *pApp = AfxGetApp();
	HICON hIcon = pApp->LoadIcon(IDI_MOVE_UP);
	((CButton *)GetDlgItem(IDC_VISGROUP_MOVEUP))->SetIcon(hIcon);

	hIcon = pApp->LoadIcon(IDI_MOVE_DOWN);
	((CButton *)GetDlgItem(IDC_VISGROUP_MOVEDOWN))->SetIcon(hIcon);

    AddControl( IDC_GROUPS, GROUP_BOX );
	AddControl( IDC_VISGROUP_MOVEUP, BOTTOM_JUSTIFY );
	AddControl( IDC_VISGROUP_MOVEDOWN, BOTTOM_JUSTIFY );
	AddControl( IDC_SHOW_ALL, BOTTOM_JUSTIFY );
	AddControl( IDC_EDITGROUPS, BOTTOM_JUSTIFY );
	AddControl( IDC_MARKMEMBERS, BOTTOM_JUSTIFY );
	AddControl( IDC_TAB1, GROUP_BOX );

	// Add all the VisGroups to the list.
	UpdateGroupList();

	bInitialized = TRUE;
	m_bShowingAuto = FALSE;

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nLength - 
//			dwMode - 
// Output : CSize
//-----------------------------------------------------------------------------
CSize CFilterControl::CalcDynamicLayout(int nLength, DWORD dwMode)
{
	// TODO: make larger / resizable when floating
	//if (IsFloating())
	//{
	//	return CSize(200, 300);
	//}

	return CHammerBar::CalcDynamicLayout(nLength, dwMode);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nType - 
//			cx - 
//			cy - 
//-----------------------------------------------------------------------------
void CFilterControl::OnSize(UINT nType, int cx, int cy)
{
	// TODO: make larger / resizable when floating
	//if (IsFloating())
	//{
	//	CWnd *pwnd = GetDlgItem(IDC_GROUPS);
	//	if (pwnd && IsWindow(pwnd->GetSafeHwnd()))
	//	{
	//		pwnd->MoveWindow(2, 10, cx - 2, cy - 2, TRUE);
	//	}
	//}

	CHammerBar::OnSize(nType, cx, cy);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFilterControl::UpdateGroupList(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc == NULL)
	{
		m_cGroupBox.DeleteAllItems();
		return;
	}

	m_cGroupBox.SaveVisGroupExpandStates();

	CVisGroup *pVisGroup = m_cGroupBox.GetSelectedVisGroup();

	m_cGroupBox.SetRedraw(false);
	m_cGroupBox.DeleteAllItems();

	int nCount = pDoc->VisGroups_GetRootCount();

	for (int i = 0; i < nCount; i++)
	{
		CVisGroup *pGroup = pDoc->VisGroups_GetRootVisGroup(i);
		int compareValue = strcmp( pGroup->GetName(), "Auto" );
		if ( (compareValue == 0 && m_bShowingAuto) ||(compareValue != 0 && !m_bShowingAuto) )
		{
			m_cGroupBox.AddVisGroup(pGroup);
		}
	}

	UpdateGroupListChecks();		

	if (pVisGroup)
	{
		m_cGroupBox.EnsureVisible(pVisGroup);
		m_cGroupBox.SelectVisGroup(pVisGroup);
	}

	m_cGroupBox.RestoreVisGroupExpandStates();
	m_cGroupBox.SetRedraw(true);
	m_cGroupBox.Invalidate();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCmdUI - 
//-----------------------------------------------------------------------------
void CFilterControl::UpdateControl(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetActiveWorld() ? TRUE : FALSE);
}


//-----------------------------------------------------------------------------
// Purpose: Disables the group list when there's no active world or when the
//			visgroups are currently overridden by the "Show All" button.
//-----------------------------------------------------------------------------
void CFilterControl::UpdateControlGroups(CCmdUI *pCmdUI)
{
	pCmdUI->Enable((GetActiveWorld() != NULL) && !CVisGroup::IsShowAllActive());
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTarget - 
//			bDisableIfNoHndler - 
//-----------------------------------------------------------------------------
void CFilterControl::OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler)
{
	UpdateDialogControls(pTarget, FALSE);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFilterControl::OnShowAllGroups(void)
{
	CButton *pButton = (CButton *)GetDlgItem(IDC_SHOW_ALL);
	if (pButton != NULL)
	{
		UINT uCheck = pButton->GetCheck();
		CVisGroup::ShowAllVisGroups(uCheck == 1);

		CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
		pDoc->UpdateVisibilityAll();

		UpdateGroupListChecks();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CFilterControl::OnMoveUpDown(UINT uCmd)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (!pDoc)
	{
		return TRUE;
	}

	CVisGroup *pVisGroup = m_cGroupBox.GetSelectedVisGroup();
	if (pVisGroup == NULL)
	{
		return TRUE;
	}

	if (uCmd == IDC_VISGROUP_MOVEUP)
	{
		pDoc->VisGroups_MoveUp(pVisGroup);
	}
	else
	{
		pDoc->VisGroups_MoveDown(pVisGroup);
	}

	UpdateGroupList();

	m_cGroupBox.EnsureVisible(pVisGroup);
	m_cGroupBox.SelectVisGroup(pVisGroup);

	pDoc->SetModifiedFlag();

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFilterControl::OnEditGroups(void)
{
	CEditGroups dlg;
	dlg.DoModal();

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		pDoc->SetModifiedFlag();
	}

	UpdateGroupList();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//			pInfo - 
// Output : Returns TRUE to continue enumerating, FALSE to stop.
//-----------------------------------------------------------------------------
static BOOL MarkMembersOfGroup(CMapClass *pObject, MARKMEMBERSINFO *pInfo)
{
	if (pObject->IsInVisGroup(pInfo->pGroup))
	{
		if (!pObject->IsVisible())
		{
			return TRUE;
		}

		CMapClass *pSelectObject = pObject->PrepareSelection(pInfo->eSelectMode);
		if (pSelectObject)
		{
			pInfo->pDoc->SelectObject(pSelectObject, scSelect);
		}
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Selects all objects that belong to the currently selected visgroup.
//-----------------------------------------------------------------------------
void CFilterControl::OnMarkMembers(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc)
	{
		CVisGroup *pVisGroup = m_cGroupBox.GetSelectedVisGroup();
		if (pVisGroup)
		{
			pDoc->GetSelection()->SetMode(selectObjects);

			// Clear the selection.
			pDoc->SelectObject(NULL, scClear|scSaveChanges);

			//
			// Select all objects that belong to the visgroup.
			//
			CMapWorld *pWorld = pDoc->GetMapWorld();
			EnumChildrenPos_t pos;
			CMapClass *pChild = pWorld->GetFirstDescendent(pos);
			while (pChild)
			{
				if (pChild->IsInVisGroup(pVisGroup))
				{
					if (pChild->IsVisible())
					{
						CMapClass *pSelectObject = pChild->PrepareSelection(pDoc->GetSelection()->GetMode());
						if (pSelectObject)
						{
							pDoc->SelectObject(pSelectObject, scSelect);
						}
					}
				}
				
				pChild = pWorld->GetNextDescendent(pos);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pPos - 
//-----------------------------------------------------------------------------
void CFilterControl::OnWindowPosChanged(WINDOWPOS *pPos)
{
	if (bInitialized && pPos->flags & SWP_SHOWWINDOW)
	{
		UpdateGroupList();
	}

	CHammerBar::OnWindowPosChanged(pPos);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bShow - 
//			nStatus - 
//-----------------------------------------------------------------------------
void CFilterControl::OnShowWindow(BOOL bShow, UINT nStatus)
{
	if (bShow)
	{
		UpdateGroupList();
	}

	CHammerBar::OnShowWindow(bShow, nStatus);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nState - 
//			pWnd - 
//			bMinimized - 
//-----------------------------------------------------------------------------
void CFilterControl::OnActivate(UINT nState, CWnd* pWnd, BOOL bMinimized)
{
	if (nState == WA_ACTIVE)
	{
		UpdateGroupList();
	}

	CHammerBar::OnActivate(nState, pWnd, bMinimized);
}


//-----------------------------------------------------------------------------
// Purpose: Called when the visibility of a group is toggled in the groups list.
// Input  : wParam - Index of item in the groups list that was toggled.
//			lParam - 0 to hide, 1 to show.
// Output : Returns zero.
//-----------------------------------------------------------------------------
LRESULT CFilterControl::OnListToggleState(WPARAM wParam, LPARAM lParam)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		//
		// Update the visibility of the group.
		//
		CVisGroup *pVisGroup = (CVisGroup *)wParam;
		if (pVisGroup != NULL)
		{
			pDoc->VisGroups_ShowVisGroup(pVisGroup, pVisGroup->GetVisible() == VISGROUP_HIDDEN);
		}

		UpdateGroupListChecks();
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wParam - 
//			lParam - 
// Output : LRESULT
//-----------------------------------------------------------------------------
LRESULT CFilterControl::OnListLeftDragDrop(WPARAM wParam, LPARAM lParam)
{
	if ( m_bShowingAuto == TRUE )
	{
		UpdateGroupList();
		return 0;
	}

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		CVisGroup *pDragGroup = (CVisGroup *)wParam;
		CVisGroup *pDropGroup = (CVisGroup *)lParam;

		if (pDropGroup != NULL)
		{
			if (pDragGroup->FindDescendent(pDropGroup))
			{
				CString str;
				str.Format("Cannot combine the groups because '%s' is a sub-group of '%s'.", pDropGroup->GetName(), pDragGroup->GetName());
				AfxMessageBox(str);
				UpdateGroupList();
				return 0;
			}

			CString str;
			str.Format("Combine group '%s' into group '%s'?", pDragGroup->GetName(), pDropGroup->GetName());
			if (AfxMessageBox(str, MB_YESNO | MB_ICONQUESTION) == IDNO)
			{
				UpdateGroupList();
				return 0;
			}

			pDoc->VisGroups_CombineGroups(pDragGroup, pDropGroup);
		}
		else
		{
			CString str;
			str.Format("Delete group '%s'?", pDragGroup->GetName());
			if (AfxMessageBox(str, MB_YESNO | MB_ICONQUESTION) == IDNO)
			{
				UpdateGroupList();
				return 0;
			}

			// Show the visgroup that's being deleted so that member objects are shown.
			pDoc->VisGroups_CheckMemberVisibility(pDragGroup);
			pDoc->VisGroups_RemoveGroup(pDragGroup);
		}
	}

		UpdateGroupList();

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wParam - 
//			lParam - 
// Output : LRESULT
//-----------------------------------------------------------------------------
LRESULT CFilterControl::OnListRightDragDrop(WPARAM wParam, LPARAM lParam)
{
	if ( m_bShowingAuto == TRUE )
	{
		UpdateGroupList();
		return 0;
	}

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		CVisGroup *pDragGroup = (CVisGroup *)wParam;
		CVisGroup *pDropGroup = (CVisGroup *)lParam;

		if (pDragGroup->FindDescendent(pDropGroup))
		{
			CString str;
			str.Format("Cannot move the group because '%s' is a sub-group of '%s'.", pDropGroup->GetName(), pDragGroup->GetName());
			AfxMessageBox(str);
			return 0;
		}

		pDoc->VisGroups_SetParent(pDragGroup, pDropGroup);
		UpdateGroupList();
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wParam - 
//			lParam - 
//			pResult - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CFilterControl::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT *pResult)
{
	NMHDR *pnmh = (NMHDR *)lParam;

	if (pnmh->idFrom == IDC_GROUPS)
	{
		switch (pnmh->code)
		{
			case TVN_SELCHANGED:
			{
				CVisGroup *pGroup = m_cGroupBox.GetSelectedVisGroup();
				CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
				if (pGroup && pDoc)
				{
					bool bCanMoveUp = pDoc->VisGroups_CanMoveUp(pGroup);
					bool bCanMoveDown = pDoc->VisGroups_CanMoveDown(pGroup);
					GetDlgItem(IDC_VISGROUP_MOVEUP)->EnableWindow(bCanMoveUp);
					GetDlgItem(IDC_VISGROUP_MOVEDOWN)->EnableWindow(bCanMoveDown);
				}

				return(TRUE);
			}
		}
	}

	return(CWnd::OnNotify(wParam, lParam, pResult));
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CFilterControl::OnInitDialog(void)
{
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFilterControl::UpdateGroupListChecks(void)
{
	int nCount = m_cGroupBox.GetVisGroupCount();
	for (int i = 0; i < nCount; i++)
	{
		CVisGroup *pVisGroup = m_cGroupBox.GetVisGroup(i);
		if (pVisGroup->GetVisible() == VISGROUP_HIDDEN)
		{
			m_cGroupBox.SetCheck(pVisGroup, 0);
		}
		else if (pVisGroup->GetVisible() == VISGROUP_SHOWN)
		{
			m_cGroupBox.SetCheck(pVisGroup, 1);
		}
		else
		{
			m_cGroupBox.SetCheck(pVisGroup, -1);
		}
	}
}

void CFilterControl::OnSelChangeTab(NMHDR *header, LRESULT *result) 
{
	if ( m_cTabControl.GetCurSel() == 0 ) 
	{
		m_bShowingAuto = FALSE;				
	} 
	else 
	{
		m_bShowingAuto = TRUE;		
	}
	UpdateGroupList();	
}
