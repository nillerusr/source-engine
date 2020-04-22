//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "SelectModeDlgBar.h"
#include "ControlBarIDs.h"
#include "MapDoc.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


BEGIN_MESSAGE_MAP(CSelectModeDlgBar, CHammerBar)
	//{{AFX_MSG_MAP(CSelectModeDlgBar)
	ON_BN_CLICKED(IDC_GROUPS, OnGroups)
	ON_BN_CLICKED(IDC_OBJECTS, OnObjects)
	ON_BN_CLICKED(IDC_SOLIDS, OnSolids)
	ON_UPDATE_COMMAND_UI(IDC_GROUPS, UpdateControlGroups)
	ON_UPDATE_COMMAND_UI(IDC_OBJECTS, UpdateControlObjects)
	ON_UPDATE_COMMAND_UI(IDC_SOLIDS, UpdateControlSolids)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pParentWnd - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CSelectModeDlgBar::Create(CWnd *pParentWnd)
{
	if (!CHammerBar::Create(pParentWnd, IDD_SELECT_MODE_BAR, CBRS_RIGHT, IDCB_SELECT_MODE_BAR))
	{
		return FALSE;
	}

	SetWindowText("Selection Mode");
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCmdUI - 
//-----------------------------------------------------------------------------
void CSelectModeDlgBar::UpdateControlGroups(CCmdUI *pCmdUI)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck(pDoc->GetSelection()->GetMode() == selectGroups);
	}
	else
	{
		pCmdUI->Enable(FALSE);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSelectModeDlgBar::UpdateControlObjects(CCmdUI *pCmdUI)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck(pDoc->GetSelection()->GetMode() == selectObjects);
	}
	else
	{
		pCmdUI->Enable(FALSE);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSelectModeDlgBar::UpdateControlSolids(CCmdUI *pCmdUI)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck(pDoc->GetSelection()->GetMode() == selectSolids);
	}
	else
	{
		pCmdUI->Enable(FALSE);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSelectModeDlgBar::OnGroups(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		pDoc->GetSelection()->SetMode(selectGroups);
		((CButton *)GetDlgItem(IDC_GROUPS))->SetCheck(TRUE);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSelectModeDlgBar::OnObjects(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		pDoc->GetSelection()->SetMode(selectObjects);
		((CButton *)GetDlgItem(IDC_OBJECTS))->SetCheck(TRUE);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSelectModeDlgBar::OnSolids(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		pDoc->GetSelection()->SetMode(selectSolids);
		((CButton *)GetDlgItem(IDC_SOLIDS))->SetCheck(TRUE);
	}
}

