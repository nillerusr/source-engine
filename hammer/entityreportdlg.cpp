//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Singleton dialog that generates and presents the entity report.
//
//=============================================================================//

#include "stdafx.h"
#include "EntityReportDlg.h"
#include "fgdlib/GameData.h"
#include "GlobalFunctions.h"
#include "History.h"
#include "MainFrm.h"
#include "MapEntity.h"
#include "MapInstance.h"
#include "MapView2D.h"
#include "MapWorld.h"
#include "ObjectProperties.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static CEntityReportDlg *s_pDlg = NULL;
static char *pszIniSection = "EntityReportDlg";


//-----------------------------------------------------------------------------
// Purpose: Static function 
//-----------------------------------------------------------------------------
void CEntityReportDlg::ShowEntityReport(CMapDoc *pDoc, CWnd *pwndParent)
{
	if (!s_pDlg)
	{
		s_pDlg = new CEntityReportDlg(pDoc, pwndParent);
		s_pDlg->Create(IDD, pwndParent);
	}

	s_pDlg->ShowWindow(SW_SHOW);
	s_pDlg->GenerateReport();
}

	
//-----------------------------------------------------------------------------
// Purpose: Private constructor.
//-----------------------------------------------------------------------------
CEntityReportDlg::CEntityReportDlg(CMapDoc *pDoc, CWnd* pParent /*=NULL*/)
	: CDialog(CEntityReportDlg::IDD, pParent)
{
	m_pDoc = pDoc;

	CWinApp *pApp = AfxGetApp();

	m_bFilterByKeyvalue = pApp->GetProfileInt(pszIniSection, "FilterByKeyvalue", FALSE);
	m_bFilterByClass = pApp->GetProfileInt(pszIniSection, "FilterByClass", FALSE);
	m_bFilterByHidden = pApp->GetProfileInt(pszIniSection, "FilterByHidden", TRUE);
	m_iFilterByType = pApp->GetProfileInt(pszIniSection, "FilterByType", 0);
	m_bExact = pApp->GetProfileInt(pszIniSection, "Exact", FALSE);

	m_szFilterClass = pApp->GetProfileString(pszIniSection, "FilterClass", "");
	m_szFilterKey = pApp->GetProfileString(pszIniSection, "FilterKey", "");
	m_szFilterValue = pApp->GetProfileString(pszIniSection, "FilterValue", "");

	m_bFilterTextChanged = FALSE;

	//{{AFX_DATA_INIT(CEntityReportDlg)
	//}}AFX_DATA_INIT
}

void CEntityReportDlg::SaveToIni()
{
	CWinApp *pApp = AfxGetApp();

	pApp->WriteProfileInt(pszIniSection, "FilterByKeyvalue", m_bFilterByKeyvalue);
	pApp->WriteProfileInt(pszIniSection, "FilterByClass", m_bFilterByClass);
	pApp->WriteProfileInt(pszIniSection, "FilterByHidden", m_bFilterByHidden);
	pApp->WriteProfileInt(pszIniSection, "FilterByType", m_iFilterByType);
	pApp->WriteProfileInt(pszIniSection, "Exact", m_bExact);

	pApp->WriteProfileString(pszIniSection, "FilterClass", m_szFilterClass);
	pApp->WriteProfileString(pszIniSection, "FilterKey", m_szFilterKey);
	pApp->WriteProfileString(pszIniSection, "FilterValue", m_szFilterValue);
}

void CEntityReportDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEntityReportDlg)
	DDX_Control(pDX, IDC_EXACTVALUE, m_cExact);
	DDX_Control(pDX, IDC_FILTERCLASS, m_cFilterClass);
	DDX_Control(pDX, IDC_FILTERBYCLASS, m_cFilterByClass);
	DDX_Control(pDX, IDC_ENTITIES, m_cEntities);
	DDX_Control(pDX, IDC_FILTERVALUE, m_cFilterValue);
	DDX_Control(pDX, IDC_FILTERKEY, m_cFilterKey);
	DDX_Control(pDX, IDC_FILTERBYTYPE, m_cFilterByType);
	DDX_Control(pDX, IDC_FILTERBYKEYVALUE, m_cFilterByKeyvalue);
	DDX_Control(pDX, IDC_FILTERBYHIDDEN, m_cFilterByHidden);
	//}}AFX_DATA_MAP

	DDX_Check(pDX, IDC_EXACTVALUE, m_bExact);
	DDX_Check(pDX, IDC_FILTERBYCLASS, m_bFilterByClass);
	DDX_Radio(pDX, IDC_FILTERBYTYPE, m_iFilterByType);
	DDX_Text(pDX, IDC_FILTERCLASS, m_szFilterClass);
	DDX_Text(pDX, IDC_FILTERVALUE, m_szFilterValue);
	DDX_Text(pDX, IDC_FILTERKEY, m_szFilterKey);
	DDX_Check(pDX, IDC_FILTERBYKEYVALUE, m_bFilterByKeyvalue);
	DDX_Check(pDX, IDC_FILTERBYHIDDEN, m_bFilterByHidden);
}


BEGIN_MESSAGE_MAP(CEntityReportDlg, CDialog)
	//{{AFX_MSG_MAP(CEntityReportDlg)
	ON_BN_CLICKED(IDC_DELETE, OnDelete)
	ON_BN_CLICKED(IDC_FILTERBYHIDDEN, OnFilterbyhidden)
	ON_BN_CLICKED(IDC_FILTERBYKEYVALUE, OnFilterbykeyvalue)
	ON_BN_CLICKED(IDC_FILTERBYTYPE, OnFilterbytype)
	ON_EN_CHANGE(IDC_FILTERKEY, OnChangeFilterkey)
	ON_EN_CHANGE(IDC_FILTERVALUE, OnChangeFiltervalue)
	ON_BN_CLICKED(IDC_GOTO, OnGoto)
	ON_BN_CLICKED(IDC_PROPERTIES, OnProperties)
	ON_WM_TIMER()
	ON_CBN_EDITCHANGE(IDC_FILTERCLASS, OnEditchangeFilterclass)
	ON_BN_CLICKED(IDC_FILTERBYCLASS, OnFilterbyclass)
	ON_CBN_SELCHANGE(IDC_FILTERCLASS, OnSelchangeFilterclass)
	ON_BN_CLICKED(IDC_RADIO2, OnFilterbytype)
	ON_BN_CLICKED(IDC_RADIO3, OnFilterbytype)
	ON_BN_CLICKED(IDC_EXACTVALUE, OnExactvalue)
	ON_LBN_SELCHANGE(IDC_ENTITIES, OnSelChangeEntityList)
	ON_LBN_DBLCLK(IDC_ENTITIES, OnDblClkEntityList)
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Deletes the marked objects.
//-----------------------------------------------------------------------------
void CEntityReportDlg::OnDelete(void)
{
	if (AfxMessageBox("Delete Objects?", MB_YESNO) == IDNO)
	{
		return;
	}

	GetHistory()->MarkUndoPosition(NULL, "Delete Objects");
		
	int iSel = m_cEntities.GetCurSel();

	//
	// Build a list of objects to delete.
	//
	CMapObjectList Objects;
	for (int i = 0; i < m_cEntities.GetCount(); i++)
	{
		if (!m_cEntities.GetSel(i))
		{
			continue;
		}

		CMapEntity *pEntity = (CMapEntity *)m_cEntities.GetItemDataPtr(i);
		Objects.AddToTail(pEntity);
		m_cEntities.DeleteString(i);
		--i;
	}

	m_pDoc->DeleteObjectList(Objects);

	//
	// Update the list box selection.
	//
	if (iSel >= m_cEntities.GetCount())
	{
		iSel = m_cEntities.GetCount() - 1;
	}
	m_cEntities.SetCurSel(iSel);
}


void CEntityReportDlg::OnFilterbyhidden() 
{
	m_bFilterByHidden = m_cFilterByHidden.GetCheck();
	UpdateEntityList();
}

void CEntityReportDlg::OnFilterbykeyvalue() 
{
	m_bFilterByKeyvalue = m_cFilterByKeyvalue.GetCheck();
	UpdateEntityList();

	m_cFilterKey.EnableWindow(m_bFilterByKeyvalue);
	m_cFilterValue.EnableWindow(m_bFilterByKeyvalue);
	m_cExact.EnableWindow(m_bFilterByKeyvalue);
}

void CEntityReportDlg::OnFilterbytype() 
{
	// walk all children in group
	int iButton = 0;
	HWND hWndCtrl = ::GetDlgItem(m_hWnd, IDC_FILTERBYTYPE);
	do	{
		// control in group is a radio button
		if(::SendMessage(hWndCtrl, BM_GETCHECK, 0, 0L) != 0)
			break;
		iButton++;
		hWndCtrl = ::GetWindow(hWndCtrl, GW_HWNDNEXT);
	} while(hWndCtrl != NULL && !(GetWindowLong(hWndCtrl, GWL_STYLE) & WS_GROUP));

	m_iFilterByType = iButton;
	UpdateEntityList();
}

void CEntityReportDlg::OnChangeFilterkey() 
{
	m_cFilterKey.GetWindowText(m_szFilterKey);
	m_szFilterKey.MakeUpper();
	m_dwFilterTime = time(NULL);
	m_bFilterTextChanged = TRUE;
}

void CEntityReportDlg::OnChangeFiltervalue() 
{
	m_cFilterValue.GetWindowText(m_szFilterValue);
	m_szFilterValue.MakeUpper();
	m_dwFilterTime = time(NULL);
	m_bFilterTextChanged = TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Centers the 2D and 3D views on the selected entities.
//-----------------------------------------------------------------------------
void CEntityReportDlg::OnGoto() 
{
	CMapDoc	*pMapDoc = MarkSelectedEntities();

	if ( pMapDoc )
	{
		pMapDoc->ShowWindow( true );
		pMapDoc->CenterViewsOnSelection();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapDoc *CEntityReportDlg::MarkSelectedEntities() 
{
	CUtlVector< CMapDoc * >	FoundMaps;

	for(int i = 0; i < m_cEntities.GetCount(); i++)
	{
		if(!m_cEntities.GetSel(i))
			continue;
		CMapEntity *pEntity = (CMapEntity*) m_cEntities.GetItemDataPtr(i);
		CMapClass *pTopMapClass = pEntity;
		while( pTopMapClass->GetParent() )
		{
			pTopMapClass = pTopMapClass->GetParent();
		}
		CMapWorld	*pMapWorld = dynamic_cast< CMapWorld * >( pTopMapClass );
		if ( pMapWorld )
		{
			CMapDoc	*pMapDoc = pMapWorld->GetOwningDocument();

			if ( FoundMaps.Find( pMapDoc ) == -1 )
			{
				FoundMaps.AddToTail( pMapDoc );
				pMapDoc->SelectObject( NULL, scClear|scSaveChanges );
			}

			pMapDoc->SelectObject( pEntity, scSelect );
		}
	}

	if ( FoundMaps.Count() == 1 )
	{
		return FoundMaps[ 0 ];
	}

	return NULL;
}

void CEntityReportDlg::OnProperties() 
{
	CMapDoc	*pMapDoc = MarkSelectedEntities();

	if ( pMapDoc )
	{
		pMapDoc->ShowWindow( true );
		GetMainWnd()->pObjectProperties->ShowWindow(SW_SHOW);
	}
}

void CEntityReportDlg::OnTimer(UINT nIDEvent) 
{
	CDialog::OnTimer(nIDEvent);

	// check filters
	if(!m_bFilterTextChanged)
		return;

	if((time(NULL) - m_dwFilterTime) > 1)
	{
		m_bFilterTextChanged = FALSE;
		m_dwFilterTime = time(NULL);
		UpdateEntityList();
	}
}

BOOL AddEntityToList(CMapEntity *pEntity, CEntityReportDlg *pDlg)
{
	char szString[256];
	
	// nope.
	if (!pDlg->m_bFilterByHidden && !pEntity->IsVisible())
	{
		return TRUE;
	}

	/*
	if (!pDlg->m_pDoc->selection.IsEmpty() && !pEntity->IsSelected())
	{
		return TRUE;
	}
	*/

	if (pDlg->m_iFilterByType)
	{
		if (pDlg->m_iFilterByType == 1 && pEntity->IsPlaceholder())
		{
			return TRUE;
		}
		if (pDlg->m_iFilterByType == 2 && !pEntity->IsPlaceholder())
		{
			return TRUE;
		}
	}
		
	const char* pszClassName = pEntity->GetClassName();

	if ( pEntity && stricmp( pszClassName, "func_instance" ) == 0 )
	{
		CMapInstance	*pMapInstance = pEntity->GetChildOfType( ( CMapInstance * )NULL );
		if ( pMapInstance )
		{
			CMapDoc		*pMapDoc = pMapInstance->GetInstancedMap();
			if ( pMapDoc )
			{
				CMapWorld	*pWorld = pMapDoc->GetMapWorld();

				pWorld->EnumChildren(ENUMMAPCHILDRENPROC(AddEntityToList), DWORD(pDlg), MAPCLASS_TYPE(CMapEntity));
			}
		}
	}

	if (pDlg->m_bFilterByClass)
	{
		if (pDlg->m_szFilterClass.IsEmpty())
		{
			if (pszClassName[0])
			{
				return(TRUE);
			}
		}
		else
		{
			V_strcpy_safe( szString, pEntity->GetClassName() );
			strupr(szString);
			if (!strstr(szString, pDlg->m_szFilterClass))
			{
				return(TRUE);
			}
		}
	}

	strcpy(szString, pszClassName);

	BOOL bAdd = TRUE;

	if(pDlg->m_bFilterByKeyvalue)
		bAdd = FALSE;

	MDkeyvalue tmpkv;
	for (int i = pEntity->GetFirstKeyValue(); i != pEntity->GetInvalidKeyValue(); i=pEntity->GetNextKeyValue( i ) )
	{
		// if filtering by keyvalue, check!
		if (pDlg->m_bFilterByKeyvalue && !bAdd && !pDlg->m_szFilterValue.IsEmpty())
		{
			// first, check key
			if (pDlg->m_szFilterKey.IsEmpty() || !strcmpi(pDlg->m_szFilterKey, pEntity->GetKey(i)))
			{
				// now, check value
				char szTmp1[128], szTmp2[128];
				V_strcpy_safe( szTmp1, pEntity->GetKeyValue( i ) );
				strupr(szTmp1);
				strcpy(szTmp2, pDlg->m_szFilterValue);
				if ((!pDlg->m_bExact && strstr(szTmp1, szTmp2)) || !strcmpi(szTmp1, szTmp2))
				{
					bAdd = TRUE;
				}
			}
		}

		const char* pszName = pEntity->GetKey(i);
		
		GDclass *pClass = pEntity->GetClass();
		if (pClass != NULL)
		{
			GDinputvariable *pVar = pClass->VarForName(pEntity->GetKey(i));
			if (!pVar || !pVar->IsReportable())
			{
				continue;
			}
			pszName = pVar->GetLongName();
		}

		sprintf(szString + strlen(szString), "\t%s \"%s\"", pszName, pEntity->GetKeyValue(i));

		if (pClass == NULL)
		{
			break;	// just do first if no class
		}
	}
	
	if(bAdd)
	{
		int iIndex = pDlg->m_cEntities.AddString(szString);
		pDlg->m_cEntities.SetItemDataPtr(iIndex, PVOID(pEntity));
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportDlg::UpdateEntityList(void)
{
	m_bFilterTextChanged = FALSE;

	m_cEntities.SetRedraw(FALSE);
	m_cEntities.ResetContent();

	int x = 80;
	m_cEntities.SetTabStops(x);

	m_szFilterValue.MakeUpper();
	m_szFilterKey.MakeUpper();
	m_szFilterClass.MakeUpper();

	// add items to list
	CMapWorld *pWorld = m_pDoc->GetMapWorld();
	pWorld->EnumChildren(ENUMMAPCHILDRENPROC(AddEntityToList), DWORD(this), MAPCLASS_TYPE(CMapEntity));

	m_cEntities.SetRedraw(TRUE);
	m_cEntities.Invalidate();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportDlg::GenerateReport()
{
	CString str;
	int nCount = pGD->GetClassCount();
	for (int i = 0; i < nCount; i++)
	{
		GDclass *pc = pGD->GetClass(i);
		if(!pc->IsBaseClass())
		{
			str = pc->GetName();
			if(str != "worldspawn")
				m_cFilterClass.AddString(str);
		}
	}

	SetTimer(1, 500, NULL);

	OnFilterbykeyvalue();
	OnFilterbytype();
	OnFilterbyclass();
}

void CEntityReportDlg::OnEditchangeFilterclass() 
{
	m_cFilterClass.GetWindowText(m_szFilterClass);
	m_szFilterClass.MakeUpper();
	m_dwFilterTime = time(NULL);
	m_bFilterTextChanged = TRUE;
}

void CEntityReportDlg::OnFilterbyclass() 
{
	m_bFilterByClass = m_cFilterByClass.GetCheck();
	UpdateEntityList();

	m_cFilterClass.EnableWindow(m_bFilterByClass);
}

void CEntityReportDlg::OnSelchangeFilterclass() 
{
	int iSel = m_cFilterClass.GetCurSel();
	m_cFilterClass.GetLBText(iSel, m_szFilterClass);
	m_szFilterClass.MakeUpper();
	UpdateEntityList();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportDlg::OnSelChangeEntityList()
{
	MarkSelectedEntities();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportDlg::OnDblClkEntityList()
{
	CMapDoc *pMapDoc = MarkSelectedEntities();

	if ( pMapDoc )
	{
		pMapDoc->ShowWindow( true );
		pMapDoc->CenterViewsOnSelection();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportDlg::OnExactvalue() 
{
	m_bExact = m_cExact.GetCheck();
	UpdateEntityList();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportDlg::OnOK()
{
	DestroyWindow();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportDlg::OnClose()
{
	DestroyWindow();
}


//-----------------------------------------------------------------------------
// Purpose: Called when our window is being destroyed.
//-----------------------------------------------------------------------------
void CEntityReportDlg::OnDestroy()
{
	SaveToIni();
	s_pDlg = NULL;
	delete this;
}
