//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile: SelectEntityDlg.cpp $
// $Date: 8/03/99 6:57p $
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "SelectEntityDlg.h"
#include "GlobalFunctions.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapSolid.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CSelectEntityDlg::CSelectEntityDlg(const CMapObjectList *pList,
								   CWnd* pParent /*=NULL*/)
	: CDialog(CSelectEntityDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSelectEntityDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_pEntityList = pList;
}


void CSelectEntityDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSelectEntityDlg)
	DDX_Control(pDX, IDC_ENTITIES, m_cEntities);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSelectEntityDlg, CDialog)
	//{{AFX_MSG_MAP(CSelectEntityDlg)
	ON_LBN_SELCHANGE(IDC_ENTITIES, OnSelchangeEntities)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSelectEntityDlg::OnSelchangeEntities() 
{
	int iSel = m_cEntities.GetCurSel();

	CMapEntity *pEntity = (CMapEntity*) m_cEntities.GetItemData(iSel);

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	pDoc->SelectObject(pEntity, scClear|scSelect|scSaveChanges);
}


BOOL CSelectEntityDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	// add entities from our list of entities to the listbox
	FOR_EACH_OBJ( *m_pEntityList, pos )
	{
		CMapClass *pObject = m_pEntityList->Element(pos);
		if(!pObject->IsMapClass(MAPCLASS_TYPE(CMapEntity)))
			continue;
		CMapEntity *pEntity = (CMapEntity*) pObject;
		if(pEntity->IsPlaceholder())
			continue;
		int iIndex = m_cEntities.AddString(pEntity->GetClassName());
		m_cEntities.SetItemData(iIndex, DWORD(pEntity));
	}

	m_cEntities.SetCurSel(0);
	OnSelchangeEntities();

	return TRUE;
}

void CSelectEntityDlg::OnOK() 
{
	int iSel = m_cEntities.GetCurSel();
	m_pFinalEntity = (CMapEntity*) m_cEntities.GetItemData(iSel);
	
	CDialog::OnOK();
}
