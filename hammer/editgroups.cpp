//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A dialog for adding, deleting, and renaming visgroups.
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "EditGroups.h"
#include "MainFrm.h"
#include "MapWorld.h"
#include "CustomMessages.h"
#include "GlobalFunctions.h"
#include "VisGroup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static const unsigned int g_uSelChangeMsg = ::RegisterWindowMessage(GROUPLIST_MSG_SEL_CHANGE);


BEGIN_MESSAGE_MAP(CEditGroups, CDialog)
	//{{AFX_MSG_MAP(CEditGroups)
	ON_BN_CLICKED(IDC_COLOR, OnColor)
	ON_EN_CHANGE(IDC_NAME, OnChangeName)
	ON_BN_CLICKED(IDC_NEW, OnNew)
	ON_BN_CLICKED(IDC_REMOVE, OnRemove)
	ON_WM_CLOSE()
	ON_REGISTERED_MESSAGE(g_uSelChangeMsg, OnSelChangeGroupList)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Constructor.
// Input  : pParent - Parent window.
//-----------------------------------------------------------------------------
CEditGroups::CEditGroups(CWnd* pParent /*=NULL*/)
	: CDialog(CEditGroups::IDD, pParent)
{
	//{{AFX_DATA_INIT(CEditGroups)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


//-----------------------------------------------------------------------------
// Purpose: Exchanges data between controls and data members.
// Input  : pDX - 
//-----------------------------------------------------------------------------
void CEditGroups::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditGroups)
	DDX_Control(pDX, IDC_NAME, m_cName);
	//}}AFX_DATA_MAP
}


//-----------------------------------------------------------------------------
// Purpose: Sets the object's color as the visgroup color if the object belongs
//			to the given visgroup.
// Input  : pObject - Object to evaluate.
//			pGroup - Visgroup to check against.
// Output : Returns TRUE to continue enumerating.
//-----------------------------------------------------------------------------
static BOOL UpdateObjectColor(CMapClass *pObject, CVisGroup *pGroup)
{
	pObject->UpdateObjectColor();
	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Invokes the color picker dialog to modify the selected visgroup.
//-----------------------------------------------------------------------------
void CEditGroups::OnColor(void) 
{
	CVisGroup *pGroup = m_cGroupList.GetSelectedVisGroup();

	if (pGroup != NULL)
	{
		color32 rgbColor = pGroup->GetColor();
		CColorDialog dlg(RGB(rgbColor.r, rgbColor.g, rgbColor.b), CC_FULLOPEN);

		if (dlg.DoModal() == IDOK)
		{
			// change group color
			pGroup->SetColor(GetRValue(dlg.m_cc.rgbResult), GetGValue(dlg.m_cc.rgbResult), GetBValue(dlg.m_cc.rgbResult));
			m_cColorBox.SetColor(dlg.m_cc.rgbResult, TRUE);

			// change all object colors
			GetActiveWorld()->EnumChildren(ENUMMAPCHILDRENPROC(UpdateObjectColor), DWORD(pGroup));

			CMapDoc::GetActiveMapDoc()->UpdateAllViews( MAPVIEW_UPDATE_COLOR );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when the contents of the name edit control changes. Renames
//			the currently selected group with the new edit control contents.
//-----------------------------------------------------------------------------
void CEditGroups::OnChangeName(void)
{
	CVisGroup *pGroup = m_cGroupList.GetSelectedVisGroup();

	CString szName;
	m_cName.GetWindowText(szName);
	pGroup->SetName(szName);
	m_cGroupList.UpdateVisGroup(pGroup);
}


//-----------------------------------------------------------------------------
// Purpose: Creates a new visgroup and adds it to the list.
//-----------------------------------------------------------------------------
void CEditGroups::OnNew(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	CVisGroup *pGroup = pDoc->VisGroups_AddGroup("new group");
	pGroup->SetVisible(VISGROUP_SHOWN);
	UpdateGroupList();
	m_cGroupList.SelectVisGroup(pGroup);
	m_cName.EnableWindow(TRUE);
	m_cName.SetActiveWindow();
}


//-----------------------------------------------------------------------------
// Purpose: Called when the remove button is pressed from the visgroup editor.
//			Deletes the selected visgroup and removes all references to it.
//-----------------------------------------------------------------------------
void CEditGroups::OnRemove(void) 
{
	CVisGroup *pGroup = m_cGroupList.GetSelectedVisGroup();
	if (!pGroup)
		return;
	//Don't allow user to delete autovisgroups.
	if ( pGroup->IsAutoVisGroup() )
		return;

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		pDoc->VisGroups_RemoveGroup(pGroup);

		if (pGroup->GetVisible() != VISGROUP_SHOWN)
		{
			pDoc->VisGroups_UpdateAll();
			pDoc->UpdateVisibilityAll();
			pDoc->UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
		}
		else
		{
			pDoc->UpdateAllViews( MAPVIEW_UPDATE_COLOR );
		}
	}

	UpdateGroupList();
}


//-----------------------------------------------------------------------------
// Purpose: Handles selection change in the visgroup list. Updates the static
//			text controls with the name and colot of the selected visgroup.
//-----------------------------------------------------------------------------
LRESULT CEditGroups::OnSelChangeGroupList(WPARAM wParam, LPARAM lParam)
{
	CVisGroup *pVisGroup = m_cGroupList.GetSelectedVisGroup();
	if (!pVisGroup)
		return 0;

	UpdateControlsForVisGroup(pVisGroup);
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEditGroups::UpdateControlsForVisGroup(CVisGroup *pVisGroup)
{
	if (!pVisGroup)
		return;

	//
	// Update the name and color controls.
	//
	m_cName.SetWindowText(pVisGroup->GetName());
	color32 rgbColor = pVisGroup->GetColor();
	m_cColorBox.SetColor(RGB(rgbColor.r, rgbColor.g, rgbColor.b), TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Sets up initial state of dialog.
//-----------------------------------------------------------------------------
BOOL CEditGroups::OnInitDialog(void)
{
	CDialog::OnInitDialog();

	m_cGroupList.SubclassDlgItem(IDC_GROUPS, this);
	m_cColorBox.SubclassDlgItem(IDC_COLORBOX, this);

	//
	// Fill the listbox with the visgroup names.
	//
	UpdateGroupList();

	//
	// Disable the edit name window if there are no visgroups in the list.
	//
	if (m_cGroupList.GetVisGroupCount())
	{
		CVisGroup *pVisGroup = m_cGroupList.GetVisGroup(0);
		m_cGroupList.SelectVisGroup(pVisGroup);
		UpdateControlsForVisGroup(pVisGroup);
	}
	else
	{
		m_cName.EnableWindow(FALSE);
	}

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEditGroups::UpdateGroupList()
{
	if (!IsWindow(m_hWnd))
	{
		return;
	}

	m_cGroupList.SetRedraw(false);
	m_cGroupList.DeleteAllItems();

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		int nCount = pDoc->VisGroups_GetCount();
		for (int i = 0; i < nCount; i++)
		{
			CVisGroup *pGroup = pDoc->VisGroups_GetVisGroup(i);
			if (!pGroup->GetParent())
			{
				m_cGroupList.AddVisGroup(pGroup);
			}
		}
	}

	m_cGroupList.ExpandAll();
	m_cGroupList.SetRedraw(true);
	m_cGroupList.Invalidate();
}


//-----------------------------------------------------------------------------
// Purpose: Called when the dialog is closing. Sends a message to update the
//			visgroup dialog bar in case any visgroup changes were made.
//-----------------------------------------------------------------------------
void CEditGroups::OnClose(void)
{
	GetMainWnd()->GlobalNotify(WM_MAPDOC_CHANGED);
	CDialog::OnClose();
}


//-----------------------------------------------------------------------------
// Purpose: Called when the dialog is closing. Sends a message to update the
//			visgroup dialog bar in case any visgroup changes were made.
//-----------------------------------------------------------------------------
BOOL CEditGroups::DestroyWindow(void)
{
	GetMainWnd()->GlobalNotify(WM_MAPDOC_CHANGED);
	return(CDialog::DestroyWindow());
}


BEGIN_MESSAGE_MAP(CColorBox, CStatic)
	ON_WM_PAINT()
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Sets the color of the color box.
// Input  : c - RGB color to set.
//			bRedraw - TRUE repaints, FALSE does now.
//-----------------------------------------------------------------------------
void CColorBox::SetColor(COLORREF c, BOOL bRedraw)
{
	m_c = c;
	if (bRedraw)
	{
		RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fills the colorbox window with the current color.
//-----------------------------------------------------------------------------
void CColorBox::OnPaint(void)
{
	CPaintDC dc(this);
	CRect r;
	GetClientRect(r);
	CBrush brush(m_c);
	dc.FillRect(r, &brush);
}

