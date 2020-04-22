//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// PrefabsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "PrefabsDlg.h"
#include "Prefabs.h"
#include "Prefab3D.h"
#include "EditPrefabDlg.h"
#include "MapDoc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CPrefabsDlg dialog

CPrefabsDlg::CPrefabsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CPrefabsDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CPrefabsDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CPrefabsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPrefabsDlg)
	DDX_Control(pDX, IDC_OBJECTS, m_Objects);
	DDX_Control(pDX, IDC_OBJECTNOTES, m_ObjectNotes);
	DDX_Control(pDX, IDC_LIBRARYNOTES, m_LibraryNotes);
	DDX_Control(pDX, IDC_LIBRARIES, m_Libraries);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPrefabsDlg, CDialog)
	//{{AFX_MSG_MAP(CPrefabsDlg)
	ON_BN_CLICKED(IDC_ADDLIBRARY, OnAddlibrary)
	ON_BN_CLICKED(IDC_ADDOBJECT, OnAddobject)
	ON_BN_CLICKED(IDC_EDITLIBRARY, OnEditlibrary)
	ON_BN_CLICKED(IDC_EDITOBJECT, OnEditobject)
	ON_BN_CLICKED(IDC_EXPORTOBJECT, OnExportobject)
	ON_CBN_SELCHANGE(IDC_LIBRARIES, OnSelchangeLibraries)
	ON_BN_CLICKED(IDC_REMOVELIBRARY, OnRemovelibrary)
	ON_BN_CLICKED(IDC_REMOVEOBJECT, OnRemoveobject)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_OBJECTS, OnItemchangedObjects)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_OBJECTS, OnEndlabeleditObjects)
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
	ON_COMMAND_EX(id_EditObjectInfo, HandleEditObjectPopup)
	ON_COMMAND_EX(id_EditObjectData, HandleEditObjectPopup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPrefabsDlg message handlers

void CPrefabsDlg::OnAddobject() 
{
	CPrefabLibrary *pLibrary = GetCurrentLibrary();
	if(!pLibrary)
		return;	// no lib, no add

	CFileDialog dlg(TRUE, NULL, NULL, OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST |
		OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_NOCHANGEDIR, 
		"Prefab files (*.map;*.rmf;*.os)|*.map; *.rmf; *.os|"
		"Game MAP files (*.map)|*.map|"
		"Worldcraft RMF files (*.rmf)|*.rmf||", this);

	if(dlg.DoModal() == IDCANCEL)
		return;	// aborted

	// add all these files .. 
	char szDir[MAX_PATH], szFiles[2048];
	memcpy(szFiles, dlg.m_ofn.lpstrFile, dlg.m_ofn.nMaxFile);
	strcpy(szDir, dlg.m_ofn.lpstrFile);

	BOOL bOneFile = FALSE;
	char *p = szFiles + strlen(szDir) + 1;
	if(!p[0])
	{
		bOneFile = TRUE;
		p = szDir;	// just one file
	}

	// disable caching of prefabs
	CPrefab::EnableCaching(FALSE);

	// get files
	char szFile[MAX_PATH];
	CString strFullPath;
	int iItem = m_Objects.GetItemCount();
	while(1)
	{
		strcpy(szFile, p);
		if(!szFile[0])
			break;
		p += strlen(szFile) + 1;

		if(!bOneFile)
			strFullPath.Format("%s\\%s", szDir, szFile);
		else
			strFullPath = szFile;

		// check file type
		CPrefab *pPrefab = NULL;

		switch(CPrefab::CheckFileType(strFullPath))
		{
			case CPrefab::pftUnknown:
			{
				continue;	// no.
			}

			case CPrefab::pftRMF:
			{
				CPrefabRMF *pNew = new CPrefabRMF;
				pNew->Init(strFullPath, TRUE, CPrefab::lsRMF);
				pPrefab = (CPrefab *)pNew;
				break;
			}

			case CPrefab::pftMAP:
			{
				CPrefabRMF *pNew = new CPrefabRMF;
				pNew->Init(strFullPath, TRUE, CPrefab::lsMAP);
				pPrefab = (CPrefab *)pNew;
				break;
			}

			case CPrefab::pftScript:
			{
				Assert(0);	// not supported yet
				break;
			}
		}

		if (!pPrefab)
		{
			continue;
		}

		// add to current library
		pLibrary->Add(pPrefab);
		// add to objects list
		AddToObjectList(pPrefab, iItem++);

		if(bOneFile)
			break;
	}

	// now rewrite library
	pLibrary->Sort();
	pLibrary->Save();

	CPrefab::FreeAllData();	// free memory
	// re-enable prefab caching
	CPrefab::EnableCaching(TRUE);

	bCurLibraryModified = FALSE;
}


void CPrefabsDlg::AddToObjectList(CPrefab *pPrefab, int iItem, BOOL bReplace)
{
	if(iItem == -1)
		iItem = m_Objects.GetItemCount();
	if(bReplace)	// replace existing item
	{
		m_Objects.DeleteItem(iItem);
	}
	iItem = m_Objects.InsertItem(iItem, pPrefab->GetName(),
		pPrefab->GetType() == CPrefab::pftScript ? 1 : 0);
	m_Objects.SetItemData(iItem, pPrefab->GetID());

	bCurLibraryModified = TRUE;
}

BOOL CPrefabsDlg::HandleEditObjectPopup(UINT nID)
{
	switch(nID)
	{
	case id_EditObjectInfo:
		EditObjectInfo();
		break;
	case id_EditObjectData:
		EditObjectData();
		break;
	}
	return TRUE;
}

static BOOL IsValidFilename(LPCTSTR pszString)
// check for valid windows filename. no drive/dirs allowed.
{
	LPCTSTR p = pszString;
	while(p[0])
	{
		BYTE ch = BYTE(p[0]);
		++p;
		if(ch > 127 || isalpha(ch) || isdigit(ch) || 
			strchr(" $%`-_@~'!(){}^#&", ch))
			continue;
		// not one of those chars - not correct
		return FALSE;
	}

	return TRUE;
}

void CPrefabsDlg::EditObjectInfo()
{
	int iSel;
	CPrefab *pPrefab = GetCurrentObject(&iSel);

	CEditPrefabDlg dlg;
	dlg.m_strName = pPrefab->GetName();
	dlg.m_strDescript = pPrefab->GetNotes();

	dlg.SetRanges(500, -1);

	if(dlg.DoModal() == IDCANCEL)
		return;

	pPrefab->SetName(dlg.m_strName);
	pPrefab->SetNotes(dlg.m_strDescript);

	AddToObjectList(pPrefab, iSel, TRUE);
	bCurLibraryModified = TRUE;
}


void CPrefabsDlg::EditObjectData()
{
	// get application
	CHammer *pApp = (CHammer*) AfxGetApp();

	if(bCurLibraryModified)
	{
		CPrefabLibrary *pLibrary = GetCurrentLibrary();
		if(pLibrary)
			pLibrary->Save();
	}

	CMapDoc *pDoc = (CMapDoc*) pApp->pMapDocTemplate->OpenDocumentFile(NULL);
	pDoc->EditPrefab3D(GetCurrentObject()->GetID());
	EndDialog(IDOK);
}


void CPrefabsDlg::OnEditobject() 
{
	if(!GetCurrentObject())	// nothing
		return;

	// two stages - name/description OR data itself
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, id_EditObjectInfo, "Name and Description");
	menu.AppendMenu(MF_STRING, id_EditObjectData, "Prefab Data");

	// track menu
	CWnd *pButton = GetDlgItem(IDC_EDITOBJECT);
	CRect r;
	pButton->GetWindowRect(r);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, r.left, r.bottom,
		this, NULL);
}

void CPrefabsDlg::OnRemoveobject() 
{
	// remove marked objectsss
	int iIndex = m_Objects.GetNextItem(-1, LVNI_SELECTED);
	BOOL bConfirmed = FALSE;
	CPrefabLibrary *pLibrary = GetCurrentLibrary();
	while(iIndex != -1)
	{
		CPrefab *pPrefab = CPrefab::FindID(m_Objects.GetItemData(iIndex));
		if(pPrefab)
		{
			// delete it
			if(!bConfirmed)
			{
				// do confirmation.
				if(AfxMessageBox("Are you sure you want to delete these "
					"items?", MB_YESNO) == IDNO)
					return;	// nope!
				m_Objects.SetRedraw(FALSE);	// no redraw while doing this
			}
			
			bConfirmed = TRUE;

			// remove from lib & delete
			pLibrary->Remove(pPrefab);
			delete pPrefab;

			// delete from list and shift index down so we keep processing 
			// correctly
			m_Objects.DeleteItem(iIndex--);
		}
		// get next item
		iIndex = m_Objects.GetNextItem(iIndex, LVNI_SELECTED);
	}

	// save library
	pLibrary->Save();

	m_Objects.SetRedraw(TRUE);	// redraw objects
	m_Objects.Invalidate();
}

void CPrefabsDlg::OnExportobject() 
{
	int iIndex = m_Objects.GetNextItem(-1, LVNI_SELECTED);
	while(iIndex != -1)
	{
		CPrefab *pPrefab = CPrefab::FindID(m_Objects.GetItemData(iIndex));
		if(pPrefab)
		{
			// export it
			CString strFilename;
			strFilename = pPrefab->GetName();
			CFileDialog dlg(FALSE, "map", strFilename, OFN_HIDEREADONLY | 
				OFN_OVERWRITEPROMPT, "Map files|*.map;*.rmf|", this);
			if(dlg.DoModal() == IDCANCEL)
				return;	// nevermind
			strFilename = dlg.GetPathName();

			int iPos = strFilename.Find('.');
			DWORD dwFlags = CPrefab::lsRMF;
			if(iPos != -1)
			{
				char *p = strFilename.GetBuffer(0);
				if(!strnicmp(p+iPos+1, "map", 3))
					dwFlags = CPrefab::lsMAP;
			}

			pPrefab->Save(strFilename, dwFlags);
		}
		// get next item
		iIndex = m_Objects.GetNextItem(iIndex, LVNI_SELECTED);
	}
}

void CPrefabsDlg::SetCurObject(int iItem)
{
	iCurObject = iItem;

	if(iCurObject == -1)
	{
		m_ObjectNotes.SetWindowText("");
		return;
	}

	// update data..
	CPrefab *pPrefab = CPrefab::FindID(m_Objects.GetItemData(iCurObject));
	Assert(pPrefab);

	m_ObjectNotes.SetWindowText(pPrefab->GetNotes());
}

void CPrefabsDlg::OnItemchangedObjects(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	*pResult = 0;
	
	if(!(pNMListView->uChanged & LVIF_STATE))
		return;

	if(pNMListView->uNewState & LVIS_FOCUSED)
	{
		SetCurObject(pNMListView->iItem);
	}
}

//
// Library mgmt
//

CPrefabLibrary *CPrefabsDlg::GetCurrentLibrary(int *piSel)
{
	if(piSel)
		piSel[0] = -1;
	
	int iSel = m_Libraries.GetCurSel();
	if(iSel == CB_ERR)
		return NULL;
	CPrefabLibrary *pLibrary = CPrefabLibrary::FindID(
		m_Libraries.GetItemData(iSel));

	if(piSel)
		piSel[0] = iSel;
	return pLibrary;
}

CPrefab *CPrefabsDlg::GetCurrentObject(int *piSel)
{
	if(piSel)
		piSel[0] = -1;

	int iSel = iCurObject;
	if(iSel == -1)
		return NULL;
	CPrefab *pPrefab= CPrefab::FindID(m_Objects.GetItemData(iSel));

	if(piSel)
		piSel[0] = iSel;

	return pPrefab;
}

void CPrefabsDlg::OnSelchangeLibraries() 
{
	// get last library
	CPrefabLibrary *pLibrary = CPrefabLibrary::FindID(
		m_Libraries.GetItemData(iCurLibrary));

	// save its index
	if(bCurLibraryModified)
	{
		pLibrary->Save();
	}

	// update objects list
	m_Objects.DeleteAllItems();
	iCurLibrary = m_Libraries.GetCurSel();
	bCurLibraryModified = FALSE;
	pLibrary = GetCurrentLibrary();
	if(!pLibrary) return;

	// add objects to object list
	m_Objects.SetRedraw(FALSE);
	POSITION p = ENUM_START;
	CPrefab *pPrefab = pLibrary->EnumPrefabs(p);
	int iItem = 0;
	while(pPrefab)
	{
		AddToObjectList(pPrefab, iItem++);
		pPrefab = pLibrary->EnumPrefabs(p);
	}
	m_Objects.SetRedraw(TRUE);
	m_Objects.Invalidate();

	// set description window
	m_LibraryNotes.SetWindowText(pLibrary->GetNotes());
}

static int AskAboutInvalidFilename()
{
	return AfxMessageBox("That's not a valid name - some of the characters aren't\n"
			"acceptable. Try using a name with only A-Z, 0-9, space,\n"
			"and these characters: $%`-_@~'!(){}^#&", MB_OKCANCEL);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrefabsDlg::OnAddlibrary() 
{
Again:
	CEditPrefabDlg dlg;
	if(dlg.DoModal() == IDCANCEL)
		return;

	// check name
	if(!IsValidFilename(dlg.m_strName))
	{
		if(AskAboutInvalidFilename() == IDOK)
			goto Again;
		return;	// nevermind.
	}
	
	CPrefabLibraryRMF *pLibrary = new CPrefabLibraryRMF;

	pLibrary->SetName(dlg.m_strName);
	pLibrary->SetNotes(dlg.m_strDescript);

	// add to list
	int iIndex = m_Libraries.AddString(pLibrary->GetName());
	m_Libraries.SetItemData(iIndex, pLibrary->GetID());

	m_Libraries.SetCurSel(iIndex);
	OnSelchangeLibraries();	// to redraw description window
	bCurLibraryModified = TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrefabsDlg::OnEditlibrary() 
{
	// get selection
	int iSel;
	CPrefabLibrary *pLibrary = GetCurrentLibrary(&iSel);
	if(!pLibrary) return;

	CEditPrefabDlg dlg;
	dlg.m_strName = pLibrary->GetName();
	dlg.m_strDescript = pLibrary->GetNotes();

Again:
	if(dlg.DoModal() == IDCANCEL)
		return;

	// check name
	if(!IsValidFilename(dlg.m_strName))
	{
		if(AskAboutInvalidFilename() == IDOK)
			goto Again;
		return;	// nevermind.
	}

	pLibrary->SetName(dlg.m_strName);
	pLibrary->SetNotes(dlg.m_strDescript);

	// set in list
	m_Libraries.SetRedraw(FALSE);
	m_Libraries.DeleteString(iSel);
	int iIndex = m_Libraries.InsertString(iSel, pLibrary->GetName());
	m_Libraries.SetItemData(iIndex, pLibrary->GetID());
	m_Libraries.SetRedraw(TRUE);
	m_Libraries.Invalidate();

	m_Libraries.SetCurSel(iSel);
	OnSelchangeLibraries();	// to redraw description window
	bCurLibraryModified = TRUE;
}


void CPrefabsDlg::OnRemovelibrary() 
{
	// get cur library
	int iSel;
	CPrefabLibrary *pLibrary = GetCurrentLibrary(&iSel);
	if (pLibrary == NULL)
	{
		return;
	}

	if (AfxMessageBox("Are you sure you want to delete this library from your hard drive?", MB_YESNO) == IDYES)
	{
		pLibrary->DeleteFile();
		delete pLibrary;

		bCurLibraryModified = FALSE;

		m_Libraries.DeleteString(iSel);
		m_Libraries.SetCurSel(0);
		OnSelchangeLibraries();	// to redraw description window
	}
}


BOOL CPrefabsDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	SetCurObject(-1);

	// make imagelist
	PrefabImages.Create(IDB_PREFABS, 32, 1, RGB(0, 255, 255));
	PrefabImages.SetBkColor(m_Objects.GetBkColor());
	m_Objects.SetImageList(&PrefabImages, LVSIL_NORMAL);

	// add libraries to list
	POSITION p = ENUM_START;
	CPrefabLibrary *pLibrary = CPrefabLibrary::EnumLibraries(p);
	while(pLibrary)
	{
		int iIndex = m_Libraries.AddString(pLibrary->GetName());
		m_Libraries.SetItemData(iIndex, pLibrary->GetID());
		pLibrary = CPrefabLibrary::EnumLibraries(p);
	}

	iCurLibrary = 0;
	bCurLibraryModified = FALSE;
	m_Libraries.SetCurSel(0);
	OnSelchangeLibraries();
	
	return TRUE;
}


void CPrefabsDlg::OnEndlabeleditObjects(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
	LV_ITEM &item = pDispInfo->item;
	
	*pResult = 0;

	if(item.pszText == NULL)
		return;

	CPrefab *pPrefab = CPrefab::FindID(m_Objects.GetItemData(item.iItem));
	pPrefab->SetName(item.pszText);
	m_Objects.SetItemText(item.iItem, 0, item.pszText);
	bCurLibraryModified = TRUE;
}

void CPrefabsDlg::OnClose() 
{
	// get library
	CPrefabLibrary *pLibrary = GetCurrentLibrary();

	// save it
	if(bCurLibraryModified && pLibrary)
	{
		pLibrary->Save();
	}
	
	CDialog::OnClose();
}
