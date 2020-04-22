//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"

#include "vgui_controls/TreeView.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/SectionedListPanel.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/PropertyPage.h"

#include <ctype.h>

using namespace vgui;
extern IP4* p4;

// list of all tree view icons
enum
{
	IMAGE_FOLDER = 1,
	IMAGE_OPENFOLDER,
	IMAGE_FILE,
};


//-----------------------------------------------------------------------------
// Purpose: Handles file view
//-----------------------------------------------------------------------------
class CFileTreeView : public TreeView
{
	DECLARE_CLASS_SIMPLE( CFileTreeView, TreeView );
public:
	CFileTreeView(Panel *parent, const char *name) : BaseClass(parent, name)
	{
	}

	// override to incremental request and show p4 directories
	virtual void GenerateChildrenOfNode(int itemIndex)
	{
		KeyValues *pkv = GetItemData(itemIndex);
		if (!pkv->GetInt("dir"))
			return;

		const char *pFilePath = pkv->GetString("path", "");
		if (!pFilePath[0])
			return;

		surface()->SetCursor(dc_waitarrow);
		// get the list of files
		CUtlVector<P4File_t> &files = p4->GetFileList(pFilePath);

		// generate children
		// add all the items
		for (int i = 0; i < files.Count(); i++)
		{
			if (files[i].m_bDeleted)
				continue;

			KeyValues *kv = new KeyValues("node", "text", p4->String( files[i].m_sName ) );

			if (files[i].m_bDir)
			{
				kv->SetInt("expand", 1);
				kv->SetInt("dir", 1);
				kv->SetInt("image", IMAGE_FOLDER);
			}
			else
			{
				kv->SetInt("image", IMAGE_FILE);
			}

			// get the files path
			char szPath[MAX_PATH];
			if (files[i].m_bDir)
			{
				Q_snprintf(szPath, sizeof(szPath), "%s/%s/%%%%1", p4->String( files[i].m_sPath ), p4->String( files[i].m_sName ) );
			}
			else
			{
				Q_snprintf(szPath, sizeof(szPath), "%s/%s", p4->String( files[i].m_sPath ), p4->String( files[i].m_sName ));
			}

			// translate the files path from a depot path into a local path
			char szLocalPath[MAX_PATH];
			p4->GetLocalFilePath(szLocalPath, szPath, sizeof(szLocalPath));
			kv->SetString("path", szLocalPath);

			// now change the items text to match the local paths file name...
			char *pLocalName = Q_strrchr(szLocalPath, '\\');
			if (pLocalName)
			{
				*pLocalName = 0;
				++pLocalName;
			}
			kv->SetString("text", pLocalName);

			int itemID = AddItem(kv, itemIndex);

			// mark out of date files in red
			if (files[i].m_iHaveRevision < files[i].m_iHeadRevision)
			{
				SetItemFgColor(itemID, Color(255, 0, 0, 255));
			}
		}
	}

	// setup a context menu whenever a directory is clicked on
	virtual void GenerateContextMenu( int itemIndex, int x, int y ) 
	{
		KeyValues *pkv = GetItemData(itemIndex);
		const char *pFilePath = pkv->GetString("path", "");
		if (!pFilePath[0])
			return;

		Menu *pContext = new Menu(this, "FileContext");

		if (pkv->GetInt("dir"))
		{
			pContext->AddMenuItem("Cloak folder", new KeyValues("CloakFolder", "item", itemIndex), GetParent(), NULL);
		}
		else
		{
			pContext->AddMenuItem("Open for edit", new KeyValues("EditFile", "item", itemIndex), GetParent(), NULL);
			pContext->AddMenuItem("Open for delete", new KeyValues("DeleteFile", "item", itemIndex), GetParent(), NULL);
		}

		// show the context menu
		pContext->SetPos(x, y);
		pContext->SetVisible(true);
	}



	// setup a smaller font
	virtual void ApplySchemeSettings(IScheme *pScheme)
	{
		BaseClass::ApplySchemeSettings(pScheme);
		SetFont( pScheme->GetFont("DefaultSmall") );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Revision list
//-----------------------------------------------------------------------------
class CSmallTextListPanel : public ListPanel
{
	DECLARE_CLASS_SIMPLE( CSmallTextListPanel, ListPanel );
public:
	CSmallTextListPanel(Panel *parent, const char *name) : BaseClass(parent, name)
	{
	}

	virtual void ApplySchemeSettings(IScheme *pScheme)
	{
		BaseClass::ApplySchemeSettings(pScheme);

		SetFont( pScheme->GetFont("DefaultSmall") );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVP4Dialog::CVP4Dialog() : BaseClass(NULL, "vp4dialog"), m_Images(false)
{
	SetSize(1024, 768);
	SetTitle("VP4", true);

	// clientspec selection
	m_pClientCombo = new ComboBox(this, "ClientCombo", 16, false);

	// file browser tree controls
	m_pFileTree = new CFileTreeView(this, "FileTree");
	// build our list of images
	m_Images.AddImage(scheme()->GetImage("resource/icon_folder", false));
	m_Images.AddImage(scheme()->GetImage("resource/icon_folder_selected", false));
	m_Images.AddImage(scheme()->GetImage("resource/icon_file", false));
	m_pFileTree->SetImageList(&m_Images, false);

	// property sheet - revisions, changes, etc.
	m_pViewsSheet = new PropertySheet(this, "ViewsSheet");

	// changes
	m_pChangesPage = new PropertyPage(m_pViewsSheet, "ChangesPage");
	m_pViewsSheet->AddPage(m_pChangesPage, "Changes");
	m_pChangesList = new SectionedListPanel(m_pChangesPage, "ChangesList");

	// layout changes
	int x, y, wide, tall;
	m_pChangesPage->GetBounds(x, y, wide, tall);
	m_pChangesList->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 6, 6, -12, -12 );

	// revisions
	m_pRevisionsPage = new PropertyPage(m_pViewsSheet, "RevisionsPage");
	m_pViewsSheet->AddPage(m_pRevisionsPage, "History");
	m_pRevisionList = new CSmallTextListPanel(m_pRevisionsPage, "RevisionList");
	m_pRevisionList->SetEmptyListText("No file or directory currently selected.");
	m_pRevisionList->AddColumnHeader(0, "change", "change", 52, ListPanel::COLUMN_HIDDEN);
	m_pRevisionList->AddColumnHeader(1, "date", "date", 52, 0);
	m_pRevisionList->AddColumnHeader(2, "time", "time", 52, ListPanel::COLUMN_HIDDEN);
	m_pRevisionList->AddColumnHeader(3, "user", "user", 64, 0);
	m_pRevisionList->AddColumnHeader(4, "description", "description", 32, ListPanel::COLUMN_RESIZEWITHWINDOW);
	m_pRevisionList->SetAllowUserModificationOfColumns(true);

	// layout revisions
	m_pRevisionsPage->GetBounds(x, y, wide, tall);
	m_pRevisionList->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 6, 6, -12, -12 );

	LoadControlSettingsAndUserConfig("//PLATFORM/resource/vp4dialog.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CVP4Dialog::~CVP4Dialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: stops app on close
//-----------------------------------------------------------------------------
void CVP4Dialog::OnClose()
{
	BaseClass::OnClose();
	ivgui()->Stop();
}

//-----------------------------------------------------------------------------
// Purpose: called to open
//-----------------------------------------------------------------------------
void CVP4Dialog::Activate()
{
	BaseClass::Activate();

	char szTitle[256];
	Q_snprintf(szTitle, sizeof(szTitle), "VP4 - %s", p4->String( p4->GetActiveClient().m_sUser ));

	SetTitle(szTitle, true);

	RefreshFileList();
	RefreshClientList();
	RefreshChangesList();

	p4->GetClientList();
}

//-----------------------------------------------------------------------------
// Purpose: Refreshes the active file list
//-----------------------------------------------------------------------------
void CVP4Dialog::RefreshFileList()
{
	m_pFileTree->RemoveAll();
	m_pFileTree->SetFgColor(Color(216, 222, 211, 255));

	// add the base node
	KeyValues *pkv = new KeyValues("root");
	pkv->SetString("text", p4->String( p4->GetActiveClient().m_sLocalRoot ));
	pkv->SetString("path", p4->String( p4->GetActiveClient().m_sLocalRoot ));
	pkv->SetInt("dir", 1);
	int iRoot = m_pFileTree->AddItem(pkv, m_pFileTree->GetRootItemIndex());
	m_pFileTree->ExpandItem(iRoot, true);
}


//-----------------------------------------------------------------------------
// Purpose: utility function used in qsort
//-----------------------------------------------------------------------------
int __cdecl IntSortFunc(const void *elem1, const void *elem2)
{
	if ( *((int *)elem1) < *((int *)elem2) )
		return -1;
	else if ( *((int *)elem1) > *((int *)elem2) )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: rebuilds the list of changes
//-----------------------------------------------------------------------------
void CVP4Dialog::RefreshChangesList()
{
	m_pChangesList->RemoveAll();
	m_pChangesList->RemoveAllSections();

	CUtlVector<P4File_t> files;
	p4->GetOpenedFileList( files );
	CUtlVector<int> sections;

	// find out all the changelists
	for (int i = 0; i < files.Count(); i++)
	{
		if (!sections.IsValidIndex(sections.Find(files[i].m_iChangelist)))
		{
			// add a new section
			sections.AddToTail(files[i].m_iChangelist);
		}
	}

	// sort the changelists
	qsort(sections.Base(), sections.Count(), sizeof(int), &IntSortFunc);

	// add the changeslists
	{for (int i = 0; i < sections.Count(); i++)
	{
		m_pChangesList->AddSection(sections[i], "");

		char szChangelistName[256];
		if (sections[i] > 0)
		{
			Q_snprintf(szChangelistName, sizeof(szChangelistName), "CHANGE: %d", sections[i]);
		}
		else
		{
			Q_snprintf(szChangelistName, sizeof(szChangelistName), "CHANGE: DEFAULT");
		}

		m_pChangesList->AddColumnToSection(sections[i], "file", szChangelistName, SectionedListPanel::COLUMN_BRIGHT, 512);
	}}

	// add the files to the changelists
	{for (int i = 0; i < files.Count(); i++)
	{
		char szFile[_MAX_PATH];
		Q_snprintf(szFile, sizeof(szFile), "%s/%s", p4->String( files[i].m_sPath ) + p4->GetDepotRootLength(), p4->String( files[i].m_sName ));
		KeyValues *pkv = new KeyValues("node", "file", szFile);
		m_pChangesList->AddItem(files[i].m_iChangelist, pkv);
	}}
}

//-----------------------------------------------------------------------------
// Purpose: Refreshes the client selection combo
//-----------------------------------------------------------------------------
void CVP4Dialog::RefreshClientList()
{
	m_pClientCombo->RemoveAll();
	
	CUtlVector<P4Client_t> &clients = p4->GetClientList();
	P4Client_t &activeClient = p4->GetActiveClient();

	// go through all clients
	for (int i = 0; i < clients.Count(); i++)
	{
		// only add clients that are defined for this machine (host)
		if (clients[i].m_sHost != activeClient.m_sHost)
			continue;

		// add in the new item
		char szText[256];
		Q_snprintf(szText, sizeof(szText), "%s    %s", p4->String( clients[i].m_sName ), p4->String( clients[i].m_sLocalRoot ));
		m_pClientCombo->AddItem(szText, new KeyValues("client", "client", p4->String( clients[i].m_sName )));
	}

	m_pClientCombo->SetText( p4->String( activeClient.m_sName ));

	if (m_pClientCombo->GetItemCount() > 1)
	{
		m_pClientCombo->SetEnabled(true);
	}
	else
	{
		m_pClientCombo->SetEnabled(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose: refreshes dialog on text changing
//-----------------------------------------------------------------------------
void CVP4Dialog::OnTextChanged()
{
	KeyValues *pkv = m_pClientCombo->GetActiveItemUserData();
	if (!pkv)
		return;

	// set the new client
	p4->SetActiveClient(pkv->GetString("client"));

	// refresh the UI
	Activate();
	m_pRevisionList->RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Cloaks a folder
//-----------------------------------------------------------------------------
void CVP4Dialog::CloakFolder(int iItem)
{
	KeyValues *pkv = m_pFileTree->GetItemData(iItem);
	if (!pkv)
		return;

	// change the clientspec to remove the folder
	p4->RemovePathFromActiveClientspec(pkv->GetString("path"));

	// remove the folder from the tree view
	m_pFileTree->RemoveItem(0-iItem, false);
	m_pFileTree->InvalidateLayout();
	m_pFileTree->Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: open for edit
//-----------------------------------------------------------------------------
void CVP4Dialog::OpenFileForEdit(int iItem)
{
	KeyValues *pkv = m_pFileTree->GetItemData(iItem);
	if (!pkv)
		return;

	p4->OpenFileForEdit(pkv->GetString("path"));

	RefreshChangesList();
}

//-----------------------------------------------------------------------------
// Purpose: open for delete
//-----------------------------------------------------------------------------
void CVP4Dialog::OpenFileForDelete(int iItem)
{
	KeyValues *pkv = m_pFileTree->GetItemData(iItem);
	if (!pkv)
		return;

	p4->OpenFileForDelete(pkv->GetString("path"));

	RefreshChangesList();
}

//-----------------------------------------------------------------------------
// Purpose: updates revision view on a file being selected
//-----------------------------------------------------------------------------
void CVP4Dialog::OnFileSelected()
{
	m_pRevisionList->RemoveAll();

	// only update if reviews page is visible
	if (!m_pRevisionsPage->IsVisible())
		return;

	// update list
	int iItem = m_pFileTree->GetFirstSelectedItem();
	if (iItem < 0)
	{
		m_pRevisionList->SetEmptyListText("No file or directory currently selected.");
		return;
	}

	surface()->SetCursor(dc_waitarrow);

	m_pRevisionList->SetEmptyListText("There is no revision history for the selected file.");

	KeyValues *pkv = m_pFileTree->GetItemData(iItem);

	CUtlVector<P4Revision_t> &revisions = p4->GetRevisionList(pkv->GetString("path"), pkv->GetInt("dir"));

	// add all the revisions
	for (int i = 0; i < revisions.Count(); i++)
	{
		KeyValues *pkv = new KeyValues("node");
		pkv->SetString("user", p4->String( revisions[i].m_sUser ));
		pkv->SetInt("change", revisions[i].m_iChange);

		char szDate[256];
		Q_snprintf(szDate, sizeof(szDate), "%d/%d/%d", revisions[i].m_nYear, revisions[i].m_nMonth, revisions[i].m_nDay);
		pkv->SetString("date", szDate);

		 char szTime[256];
		 Q_snprintf(szTime, sizeof(szTime), "%d:%d:%d", revisions[i].m_nHour, revisions[i].m_nMinute, revisions[i].m_nSecond);
		 pkv->SetString("time", szTime);

		// take just the first line of the description
		char *pDesc = revisions[i].m_Description.GetForModify();
		// move to the first non-whitespace
		while (*pDesc && (isspace(*pDesc) || iscntrl(*pDesc)))
		{
			++pDesc;
		}

		char szShortDescription[256];
		Q_strncpy(szShortDescription, pDesc, sizeof(szShortDescription));
		
		// truncate to last terminator
		char *pTerm = szShortDescription;
		while (*pTerm && !iscntrl(*pTerm))
		{
			++pTerm;
		}
		*pTerm = 0;

		pkv->SetString("description", szShortDescription);

		m_pRevisionList->AddItem(pkv, 0, false, false);
	}
}
