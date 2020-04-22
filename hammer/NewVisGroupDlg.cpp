//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A dialog that is invoked when a new visgroup is created.
//			It lets the user pick an existing visgroup or create a new one.
//
//=============================================================================//

#include "stdafx.h"
#include "MapDoc.h"
#include "NewVisGroupDlg.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static const unsigned int g_uSelChangeMsg = ::RegisterWindowMessage(GROUPLIST_MSG_SEL_CHANGE);
static BOOL s_bLastHideObjects = TRUE;


BEGIN_MESSAGE_MAP(CNewVisGroupDlg, CDialog)
	//{{AFX_MSG_MAP(CNewVisGroupDlg)
	ON_REGISTERED_MESSAGE(g_uSelChangeMsg, OnSelChangeGroupList)
	ON_COMMAND(IDC_PLACE_IN_EXISTING_VISGROUP, OnPlaceInExistingVisGroup)
	ON_COMMAND(IDC_CREATE_NEW_VISGROUP, OnCreateNewVisGroup)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pParent - 
//-----------------------------------------------------------------------------
CNewVisGroupDlg::CNewVisGroupDlg(CString &str, CWnd *pParent)
	: CDialog(CNewVisGroupDlg::IDD, pParent)
{
	m_pPickedVisGroup = NULL;

	//{{AFX_DATA_INIT(CNewVisGroupDlg)
	m_strName = str;
	//}}AFX_DATA_INIT
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDX - 
//-----------------------------------------------------------------------------
void CNewVisGroupDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewVisGroupDlg)
	DDX_Check(pDX, IDC_REMOVE_FROM_ALL, m_bRemoveFromOtherGroups);
	DDX_Check(pDX, IDC_HIDE_OBJECTS, m_bHideObjects);
	DDX_Text(pDX, IDC_VISGROUP_NAME, m_strName);
	//}}AFX_DATA_MAP
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CNewVisGroupDlg::OnInitDialog(void)
{
	m_bHideObjects = s_bLastHideObjects;
	
	CDialog::OnInitDialog();

	CButton *pButton = (CButton *)GetDlgItem(IDC_CREATE_NEW_VISGROUP);
	pButton->SetCheck(1);
	
	m_cGroupList.SubclassDlgItem(IDC_GROUP_LIST, this);
	UpdateGroupList();

	CEdit *pEdit = (CEdit *)GetDlgItem(IDC_GROUP_LIST);
	pEdit->EnableWindow(FALSE);

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the visgroup name that was entered in the dialog.
//-----------------------------------------------------------------------------
void CNewVisGroupDlg::GetName(CString &str)
{
	str = m_strName;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNewVisGroupDlg::OnOK() 
{
	CDialog::OnOK();
	s_bLastHideObjects = m_bHideObjects;
}


//-----------------------------------------------------------------------------
// Purpose: Switches the mode of the dialog to pick an existing visgroup rather than
//			create a new one.
//-----------------------------------------------------------------------------
void CNewVisGroupDlg::OnPlaceInExistingVisGroup() 
{
	CEdit *pEdit = (CEdit *)GetDlgItem(IDC_VISGROUP_NAME);
	pEdit->EnableWindow(FALSE);

	pEdit = (CEdit *)GetDlgItem(IDC_GROUP_LIST);
	pEdit->EnableWindow(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Switches the mode of the dialog to create a new visgroup rather than
//			pick an existing one.
//-----------------------------------------------------------------------------
void CNewVisGroupDlg::OnCreateNewVisGroup() 
{
	CEdit *pEdit = (CEdit *)GetDlgItem(IDC_VISGROUP_NAME);
	pEdit->EnableWindow(TRUE);

	pEdit = (CEdit *)GetDlgItem(IDC_GROUP_LIST);
	pEdit->EnableWindow(FALSE);

	m_pPickedVisGroup = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Handles selection change in the visgroup list.
//-----------------------------------------------------------------------------
LRESULT CNewVisGroupDlg::OnSelChangeGroupList(WPARAM wParam, LPARAM lParam)
{
	m_pPickedVisGroup = m_cGroupList.GetSelectedVisGroup();
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNewVisGroupDlg::UpdateGroupList(void)
{
	m_cGroupList.SetRedraw(false);
	m_cGroupList.DeleteAllItems();

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		int nCount = pDoc->VisGroups_GetRootCount();
		for (int i = 0; i < nCount; i++)
		{
			CVisGroup *pGroup = pDoc->VisGroups_GetRootVisGroup(i);
			m_cGroupList.AddVisGroup(pGroup);
		}
	}

	m_cGroupList.ExpandAll();
	m_cGroupList.SetRedraw(true);
	m_cGroupList.Invalidate();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVisGroup *CNewVisGroupDlg::GetPickedVisGroup(void)
{
	return m_pPickedVisGroup;
}

