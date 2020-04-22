//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the groups page of the Object Properties dialog. This
//			allows the user to edit which visgroups the selected objects belong to.
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "MapWorld.h"
#include "OP_Groups.h"
#include "EditGroups.h"
#include "GlobalFunctions.h"
#include "CustomMessages.h"
#include "ObjectProperties.h"
#include "VisGroup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

const char *NO_GROUP_STRING = "(no group)";
const DWORD NO_GROUP_ID = 0xffff;
const DWORD VALUE_DIFFERENT_ID = 0xfffe;


static const unsigned int g_uToggleStateMsg = ::RegisterWindowMessage(GROUPLIST_MSG_TOGGLE_STATE);


BEGIN_MESSAGE_MAP(COP_Groups, CObjectPage)
	//{{AFX_MSG_MAP(COP_Groups)
	ON_BN_CLICKED(IDC_EDITGROUPS, OnEditgroups)
	ON_REGISTERED_MESSAGE(g_uToggleStateMsg, OnListToggleState)
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()

IMPLEMENT_DYNCREATE(COP_Groups, CObjectPage)


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COP_Groups::COP_Groups()
	: CObjectPage(COP_Groups::IDD)
{
	//{{AFX_DATA_INIT(COP_Groups)
	//}}AFX_DATA_INIT
	m_pEditObjectRuntimeClass = RUNTIME_CLASS(editCMapClass);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COP_Groups::~COP_Groups()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDX - 
//-----------------------------------------------------------------------------
void COP_Groups::DoDataExchange(CDataExchange* pDX)
{
	CObjectPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COP_Model)
	DDX_Control(pDX, IDC_EDITGROUPS, m_EditGroupsControl);
	//}}AFX_DATA_MAP
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : b - 
//-----------------------------------------------------------------------------
void COP_Groups::SetMultiEdit(bool b)
{
	CObjectPage::SetMultiEdit(b);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool COP_Groups::SaveData(void)
{
	if (!IsWindow(m_hWnd))
	{
		return false;
	}

	int nCount = m_cGroups.GetVisGroupCount();
	for (int i = 0; i < nCount; i++)
	{
		CVisGroup *pVisGroup = m_cGroups.GetVisGroup(i);

		// Don't let users edit Auto VisGroup membership!
		if ( pVisGroup->IsAutoVisGroup() )
			continue;

		int nCheck = m_cGroups.GetCheck(pVisGroup);
		
		if (nCheck != -1)
		{
			FOR_EACH_OBJ( *m_pObjectList, pos )
			{
				CMapClass *pObject = m_pObjectList->Element(pos);
				if (nCheck)
				{
					pObject->AddVisGroup(pVisGroup);
				}
				else
				{
					pObject->RemoveVisGroup(pVisGroup);
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Mode - 
//			pData - 
//-----------------------------------------------------------------------------
void COP_Groups::UpdateData( int Mode, void *pData, bool bCanEdit )
{
	__super::UpdateData( Mode, pData, bCanEdit );

	if ( !IsWindow(m_hWnd) )
	{
		return;
	}

	static int s_checkState[128];

	if (Mode == LoadData || Mode == LoadFirstData)
	{
		CMapClass *pObject = (CMapClass *)pData;

		if (Mode == LoadFirstData)
		{
			UpdateGroupList();

			//
			// Loading the first object. check each group this object is in.
			//
			int nCount = m_cGroups.GetVisGroupCount();
			for (int i = 0; i < nCount; i++)
			{
				CVisGroup *pVisGroup = m_cGroups.GetVisGroup(i);
				s_checkState[i] = pObject->IsInVisGroup(pVisGroup);
			}
		}
		else
		{
			//
			// Loading subsequent objects. 
			//
			int nCount = m_cGroups.GetVisGroupCount();
			for (int i = 0; i < nCount; i++)
			{
				if ( s_checkState[i] != -1)
				{
					CVisGroup *pVisGroup = m_cGroups.GetVisGroup(i);
					
					if ( pObject->IsInVisGroup(pVisGroup) != s_checkState[i] )
					{
						s_checkState[i] = -1;
					}
				}
			}
		}
	}
	else if ( Mode == LoadFinished )
	{
		int nCount = m_cGroups.GetVisGroupCount();
		for (int i = 0; i < nCount; i++)
		{
			CVisGroup *pVisGroup = m_cGroups.GetVisGroup(i);
			m_cGroups.SetCheck(pVisGroup, s_checkState[i]);
		}
	}

	m_cGroups.EnableWindow( ( m_bCanEdit ? TRUE : FALSE ) );
	m_EditGroupsControl.EnableWindow( ( m_bCanEdit ? TRUE : FALSE ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Groups::UpdateGroupList(void)
{
	if (!IsWindow(m_hWnd))
	{
		return;
	}

	m_cGroups.SetRedraw(false);
	m_cGroups.DeleteAllItems();

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		int nCount = pDoc->VisGroups_GetCount();
		for (int i = 0; i < nCount; i++)
		{
			CVisGroup *pGroup = pDoc->VisGroups_GetVisGroup(i);
			if (!pGroup->GetParent())
			{
				m_cGroups.AddVisGroup(pGroup);
			}
		}
	}

	m_cGroups.ExpandAll();
	m_cGroups.SetRedraw(true);
	m_cGroups.Invalidate();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Groups::OnEditgroups() 
{
	CEditGroups dlg;
	dlg.DoModal();

	UpdateGroupList();
	// dvs: TODO: update the check state for all groups
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL COP_Groups::OnInitDialog() 
{
	CObjectPage::OnInitDialog();

	m_cGroups.SubclassDlgItem(IDC_GROUPS, this);
	m_cGroups.EnableChecks();

	CAnchorDef anchorDefs[] =
	{
		CAnchorDef( IDC_GROUPS, k_eSimpleAnchorAllSides ),
		CAnchorDef( IDC_EDITGROUPS, k_eSimpleAnchorBottomRight )
	};
	m_AnchorMgr.Init( GetSafeHwnd(), anchorDefs, ARRAYSIZE( anchorDefs ) );

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Called when the check state of a group is toggled in the groups list.
// Input  : wParam - Index of item in the groups list that was toggled.
//			lParam - New check state.
// Output : Returns zero.
//-----------------------------------------------------------------------------
LRESULT COP_Groups::OnListToggleState(WPARAM wParam, LPARAM lParam)
{
	CVisGroup *pVisGroup = (CVisGroup *)wParam;

	// Don't let users edit Auto VisGroup membership!
	if ( pVisGroup->IsAutoVisGroup() )
		return 0;

	m_cGroups.SetCheck(pVisGroup, (int)lParam);

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOld - 
//-----------------------------------------------------------------------------
void COP_Groups::OnSetFocus(CWnd *pOld)
{
	// fixme:
	//UpdateGrouplist();
	CPropertyPage::OnSetFocus(pOld);
}

void COP_Groups::OnSize( UINT nType, int cx, int cy )
{
	m_AnchorMgr.OnSize();
}
