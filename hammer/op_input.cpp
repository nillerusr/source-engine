//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a dialog for showing the input connections of an entity
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "GlobalFunctions.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapWorld.h"
#include "ObjectProperties.h"
#include "OP_Input.h"
#include "MainFrm.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//
// Column indices for the list control.
//
const int ICON_COLUMN = 0;
const int SOURCE_NAME_COLUMN = 1;
const int OUTPUT_NAME_COLUMN = 2;
const int INPUT_NAME_COLUMN = 3;
const int PARAMETER_COLUMN = 4;
const int DELAY_COLUMN = 5;
const int ONLY_ONCE_COLUMN = 6;

#define ICON_CONN_BAD		0
#define ICON_CONN_GOOD		1
#define ICON_CONN_BAD_GREY	2
#define ICON_CONN_GOOD_GREY	3

IMPLEMENT_DYNCREATE(COP_Input, CObjectPage)


BEGIN_MESSAGE_MAP(COP_Input, CObjectPage)
	//{{AFX_MSG_MAP(COP_Input)
	ON_BN_CLICKED(IDC_MARK, OnMark)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
// Static vars
//-----------------------------------------------------------------------------
CImageList*			   COP_Input::m_pImageList = NULL;

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
COP_Input::COP_Input(void)
	: CObjectPage(COP_Input::IDD)
{
	m_pObjectList = NULL;
	m_pEntityList = new CMapEntityList;
	m_pEditObjectRuntimeClass = RUNTIME_CLASS(editCMapClass);
	m_nSortColumn = OUTPUT_NAME_COLUMN;

	//
	// All columns initially sort in ascending order.
	//
	for (int i = 0; i < OUTPUT_LIST_NUM_COLUMNS; i++)
	{
		m_eSortDirection[i] = Sort_Ascending;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
COP_Input::~COP_Input(void)
{
	// Delete my list
	delete m_pEntityList;
}

//-----------------------------------------------------------------------------
// Purpose: Compares by delays. Used as a secondary sort by all other columns.
//-----------------------------------------------------------------------------
static int CALLBACK InputCompareDelaysSecondary(CInputConnection *pInputConn1, CInputConnection *pInputConn2, SortDirection_t eDirection)
{
	CEntityConnection *pConn1 = pInputConn1->m_pConnection;
	CEntityConnection *pConn2 = pInputConn2->m_pConnection;
	return(CEntityConnection::CompareDelaysSecondary(pConn1,pConn2,eDirection));
}

//-----------------------------------------------------------------------------
// Purpose: Compares by delays, does a secondary compare by output name.
//-----------------------------------------------------------------------------
static int CALLBACK InputCompareDelays(CInputConnection *pInputConn1, CInputConnection *pInputConn2, SortDirection_t eDirection)
{
	CEntityConnection *pConn1 = pInputConn1->m_pConnection;
	CEntityConnection *pConn2 = pInputConn2->m_pConnection;
	return(CEntityConnection::CompareDelays(pConn1, pConn2,eDirection));
}


//-----------------------------------------------------------------------------
// Purpose: Compares by output name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
static int CALLBACK InputCompareOutputNames(CInputConnection *pInputConn1, CInputConnection *pInputConn2, SortDirection_t eDirection)
{
	CEntityConnection *pConn1 = pInputConn1->m_pConnection;
	CEntityConnection *pConn2 = pInputConn2->m_pConnection;
	return(CEntityConnection::CompareOutputNames(pConn1,pConn2,eDirection));
}


//-----------------------------------------------------------------------------
// Purpose: Compares by input name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
static int CALLBACK InputCompareInputNames(CInputConnection *pInputConn1, CInputConnection *pInputConn2, SortDirection_t eDirection)
{
	CEntityConnection *pConn1 = pInputConn1->m_pConnection;
	CEntityConnection *pConn2 = pInputConn2->m_pConnection;
	return(CEntityConnection::CompareInputNames(pConn1,pConn2,eDirection));
}

//-----------------------------------------------------------------------------
// Purpose: Compares by source name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
static int CALLBACK InputCompareSourceNames(CInputConnection *pInputConn1, CInputConnection *pInputConn2, SortDirection_t eDirection)
{
	CEntityConnection *pConn1 = pInputConn1->m_pConnection;
	CEntityConnection *pConn2 = pInputConn2->m_pConnection;
	return(CEntityConnection::CompareSourceNames(pConn1,pConn2,eDirection));
}

//-----------------------------------------------------------------------------
// Purpose: Compares by target name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
static int CALLBACK InputCompareTargetNames(CInputConnection *pInputConn1, CInputConnection *pInputConn2, SortDirection_t eDirection)
{
	CEntityConnection *pConn1 = pInputConn1->m_pConnection;
	CEntityConnection *pConn2 = pInputConn2->m_pConnection;
	return(CEntityConnection::CompareTargetNames(pConn1,pConn2,eDirection));
}

//------------------------------------------------------------------------------
// Purpose : Returns true if given item number from list control is a valid
//			 connection type
// Input   :
// Output  :
//------------------------------------------------------------------------------
bool COP_Input::ValidateConnections(int nItem)
{
	CInputConnection *pInputConn = (CInputConnection *)m_ListCtrl.GetItemData(nItem);

	// Early out
	if (!pInputConn)
	{
		return false;
	}

	CEntityConnection *pConnection = pInputConn->m_pConnection;
	if (pConnection != NULL)
	{
		// Validate input
		if (!MapEntityList_HasInput(m_pEntityList, pConnection->GetInputName()))
		{
			return false;
		}
		
		// Validate output
		CMapEntity *pEntity = pInputConn->m_pEntity;
		if (!CEntityConnection::ValidateOutput(pEntity,pConnection->GetOutputName()))
		{
			return false;
		}
	}
	return true;
}

//------------------------------------------------------------------------------
// Purpose : Updates the validity flag on the given item in the list control
// Input   :
// Output  :
//------------------------------------------------------------------------------
void COP_Input::UpdateItemValidity(int nItem)
{
	int nIcon;
	if (ValidateConnections(nItem))
	{
		nIcon = (m_bMultipleTargetNames ? ICON_CONN_GOOD_GREY : ICON_CONN_GOOD);
	}
	else
	{
		nIcon = (m_bMultipleTargetNames ? ICON_CONN_BAD_GREY : ICON_CONN_BAD);
	}
	m_ListCtrl.SetItem(nItem,0,LVIF_IMAGE, 0, nIcon, 0, 0, 0 );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEntity - 
//			bFirst - 
//-----------------------------------------------------------------------------
void COP_Input::AddEntityConnections(const char *pTargetName, CMapEntity *pTestEntity)
{
	int nConnCount = pTestEntity->Connections_GetCount();

	if (nConnCount != 0)
	{
		int nItemCount = m_ListCtrl.GetItemCount();	
		m_ListCtrl.SetItemCount(nItemCount + nConnCount);

		for (int i = 0; i < nConnCount; i++)
		{
			CEntityConnection *pConnection = pTestEntity->Connections_Get(i);
			if (pConnection != NULL && !CompareEntityNames(pConnection->GetTargetName(), pTargetName))
			{
				// Update source name for correct sorting
				const char *pszTestName = pTestEntity->GetKeyValue("targetname");
				if (pszTestName == NULL)
				{
					pszTestName = pTestEntity->GetClassName();
				}
				pConnection->SetSourceName(pszTestName);

				m_ListCtrl.InsertItem(LVIF_IMAGE, nItemCount, "", 0, 0, ICON_CONN_GOOD, 0);

				m_ListCtrl.SetItemText(nItemCount, OUTPUT_NAME_COLUMN, pConnection->GetOutputName());
				m_ListCtrl.SetItemText(nItemCount, SOURCE_NAME_COLUMN, pConnection->GetSourceName());
				m_ListCtrl.SetItemText(nItemCount, INPUT_NAME_COLUMN, pConnection->GetInputName());
			
				// Build the string for the delay.
				float fDelay = pConnection->GetDelay();
				char szTemp[MAX_PATH];
				sprintf(szTemp, "%.2f", fDelay);
				m_ListCtrl.SetItemText(nItemCount, DELAY_COLUMN, szTemp);
				m_ListCtrl.SetItemText(nItemCount, ONLY_ONCE_COLUMN, (pConnection->GetTimesToFire() == EVENT_FIRE_ALWAYS) ? "No" : "Yes");
				m_ListCtrl.SetItemText(nItemCount, PARAMETER_COLUMN, pConnection->GetParam());
			
				// Set list ctrl data 
				CInputConnection *pInputConn	= new CInputConnection;
				pInputConn->m_pConnection		= pConnection;
				pInputConn->m_pEntity			= pTestEntity;
				m_ListCtrl.SetItemData(nItemCount, (DWORD)pInputConn);

				nItemCount++;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDX - 
//-----------------------------------------------------------------------------
void COP_Input::DoDataExchange(CDataExchange *pDX)
{
	CObjectPage::DoDataExchange(pDX);

	//{{AFX_DATA_MAP(COP_Input)
	DDX_Control(pDX, IDC_LIST, m_ListCtrl);
	//}}AFX_DATA_MAP
}

//------------------------------------------------------------------------------
// Purpose : Take the user to the output page of the selected entity that
//			 targets me.  
// Input   :
// Output  :
//------------------------------------------------------------------------------
void COP_Input::OnMark(void)
{
	// This should always be 1 as dialog is set up to be single select
	if (m_ListCtrl.GetSelectedCount() == 1)
	{
		int nCount = m_ListCtrl.GetItemCount();
		CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
		if (nCount > 0 && pDoc)
		{
			for (int nItem = nCount - 1; nItem >= 0; nItem--)
			{
				if (m_ListCtrl.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
				{
					CInputConnection *pInputConn = (CInputConnection *)m_ListCtrl.GetItemData(nItem);
					CMapEntity *pEntity = pInputConn->m_pEntity;
					CEntityConnection *pConnection = pInputConn->m_pConnection;

					// Shouldn't happen but just in case
					if (!pEntity || !pConnection)
					{
						return;
					}

					// Switch to object selection mode so we only select the entity.
					pDoc->GetSelection()->SetMode(selectObjects);

					// Now switch to the output page with the selected connection
					CMapObjectList Select;
					Select.AddToTail(pEntity);
					pDoc->SelectObjectList(&Select);

					// (a bit squirly way of doing this)
					GetMainWnd()->pObjectProperties->SetPageToOutput(pConnection);
					pDoc->Center2DViewsOnSelection();
					return;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets up the list view columns, initial sort column.
//-----------------------------------------------------------------------------
BOOL COP_Input::OnInitDialog(void)
{
	CObjectPage::OnInitDialog();

	m_ListCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_HEADERDRAGDROP);
	
	m_ListCtrl.InsertColumn(ICON_COLUMN, "", LVCFMT_CENTER, 20);
	m_ListCtrl.InsertColumn(SOURCE_NAME_COLUMN, "Source", LVCFMT_LEFT, 70);
	m_ListCtrl.InsertColumn(OUTPUT_NAME_COLUMN, "Output", LVCFMT_LEFT, 70);
	m_ListCtrl.InsertColumn(INPUT_NAME_COLUMN, "My Input", LVCFMT_LEFT, 70);
	m_ListCtrl.InsertColumn(DELAY_COLUMN, "Delay", LVCFMT_LEFT, 70);
	m_ListCtrl.InsertColumn(ONLY_ONCE_COLUMN, "Once", LVCFMT_LEFT, 70);
	m_ListCtrl.InsertColumn(PARAMETER_COLUMN, "Parameter", LVCFMT_LEFT, 70);

	UpdateConnectionList();

	SetSortColumn(m_nSortColumn, m_eSortDirection[m_nSortColumn]);

	// Force an update of the column header text so that the sort indicator is shown.
	UpdateColumnHeaderText(m_nSortColumn, true, m_eSortDirection[m_nSortColumn]);

	if (m_ListCtrl.GetItemCount() > 0)
	{
		m_ListCtrl.SetColumnWidth(OUTPUT_NAME_COLUMN, LVSCW_AUTOSIZE);
		m_ListCtrl.SetColumnWidth(SOURCE_NAME_COLUMN, LVSCW_AUTOSIZE);
		m_ListCtrl.SetColumnWidth(INPUT_NAME_COLUMN, LVSCW_AUTOSIZE);
		m_ListCtrl.SetColumnWidth(DELAY_COLUMN, LVSCW_AUTOSIZE_USEHEADER);
		m_ListCtrl.SetColumnWidth(ONLY_ONCE_COLUMN, LVSCW_AUTOSIZE_USEHEADER);
		m_ListCtrl.SetColumnWidth(PARAMETER_COLUMN, LVSCW_AUTOSIZE);
	}

	// Create image list.  Is deleted automatically when listctrl is deleted
	if (!m_pImageList)
	{
		CWinApp *pApp = AfxGetApp();
		m_pImageList = new CImageList();
		Assert(m_pImageList != NULL);    // serious allocation failure checking
		m_pImageList->Create(16, 16, TRUE,   1, 0);
		m_pImageList->Add(pApp->LoadIcon( IDI_INPUTBAD ));
		m_pImageList->Add(pApp->LoadIcon( IDI_INPUT ));
		m_pImageList->Add(pApp->LoadIcon( IDI_INPUTBAD_GREY ));
		m_pImageList->Add(pApp->LoadIcon( IDI_INPUT_GREY ));
	}
	m_ListCtrl.SetImageList(m_pImageList, LVSIL_SMALL );

	CAnchorDef anchorDefs[] =
	{
		CAnchorDef( IDC_LIST, k_eSimpleAnchorAllSides ),
		CAnchorDef( IDC_MARK, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_INFO_TEXT, k_eSimpleAnchorBottomSide )
	};
	m_AnchorMgr.Init( GetSafeHwnd(), anchorDefs, ARRAYSIZE( anchorDefs ) );

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wParam - 
//			lParam - 
//			pResult - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL COP_Input::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT *pResult)
{
	NMHDR *pnmh = (NMHDR *)lParam;

	if (pnmh->idFrom == IDC_LIST)
	{
		switch (pnmh->code)
		{
			case LVN_COLUMNCLICK:
			{
				NMLISTVIEW *pnmv = (NMLISTVIEW *)lParam;
				if (pnmv->iSubItem < OUTPUT_LIST_NUM_COLUMNS)
				{
					SortDirection_t eSortDirection = m_eSortDirection[pnmv->iSubItem];

					//
					// If they clicked on the current sort column, reverse the sort direction.
					//
					if (pnmv->iSubItem == m_nSortColumn)
					{
						if (m_eSortDirection[m_nSortColumn] == Sort_Ascending)
						{
							eSortDirection = Sort_Descending;
						}
						else
						{
							eSortDirection = Sort_Ascending;
						}
					}
					
					//
					// Update the sort column and sort the list.
					//
					SetSortColumn(pnmv->iSubItem, eSortDirection);
				}
				return(TRUE);
			}
			case NM_DBLCLK:
			{
				OnMark();
				return(TRUE);
			}
		}
	}

	return(CObjectPage::OnNotify(wParam, lParam, pResult));
}

//-----------------------------------------------------------------------------
// Purpose: Empties the contents of the connections list control, freeing the
//			connection list hanging off of each row.
//-----------------------------------------------------------------------------
void COP_Input::RemoveAllEntityConnections(void)
{
	int nCount = m_ListCtrl.GetItemCount();
	if (nCount > 0)
	{
		for (int nItem = nCount - 1; nItem >= 0; nItem--)
		{
			CInputConnection *pInputConnection		= (CInputConnection *)m_ListCtrl.GetItemData(nItem);
			m_ListCtrl.DeleteItem(nItem);
			delete pInputConnection;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Mode - 
//			pData - 
//-----------------------------------------------------------------------------
void COP_Input::UpdateData( int Mode, PVOID pData, bool bCanEdit )
{
	__super::UpdateData( Mode, pData, bCanEdit );

	if (!IsWindow(m_hWnd))
	{
		return;
	}

	switch (Mode)
	{
		case LoadFirstData:
		{
//			m_ListCtrl.DeleteAllItems();
//			UpdateConnectionList();
			break;
		}

		case LoadData:
		{
//			m_ListCtrl.DeleteAllItems();
//			UpdateConnectionList();

			break;
		}

		case LoadFinished:
		{
			m_ListCtrl.DeleteAllItems();
			UpdateConnectionList();
			SortListByColumn(m_nSortColumn, m_eSortDirection[m_nSortColumn]);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nColumn - 
//			eDirection - 
//-----------------------------------------------------------------------------
void COP_Input::SetSortColumn(int nColumn, SortDirection_t eDirection)
{
	Assert(nColumn < OUTPUT_LIST_NUM_COLUMNS);

	//
	// If the sort column changed, update the old sort column header text.
	//
	if (m_nSortColumn != nColumn)
	{
		UpdateColumnHeaderText(m_nSortColumn, false, eDirection);
	}

	//
	// If the sort column or direction changed, update the new sort column header text.
	//
	if ((m_nSortColumn != nColumn) || (m_eSortDirection[m_nSortColumn] != eDirection))
	{
		UpdateColumnHeaderText(nColumn, true, eDirection);
	}

	m_nSortColumn = nColumn;
	m_eSortDirection[m_nSortColumn] = eDirection;

	SortListByColumn(m_nSortColumn, m_eSortDirection[m_nSortColumn]);
}


//-----------------------------------------------------------------------------
// Purpose: Sorts the outputs list by column.
// Input  : nColumn - Index of column by which to sort.
//-----------------------------------------------------------------------------
void COP_Input::SortListByColumn(int nColumn, SortDirection_t eDirection)
{
	PFNLVCOMPARE pfnSort = NULL;

	switch (nColumn)
	{
		case ONLY_ONCE_COLUMN:
		{
			// No sort
			break;
		}

		case PARAMETER_COLUMN:
		{
			// No sort
			break;
		}
		case OUTPUT_NAME_COLUMN:
		{
			pfnSort = (PFNLVCOMPARE)InputCompareOutputNames;
			break;
		}

		case SOURCE_NAME_COLUMN:
		{
			pfnSort = (PFNLVCOMPARE)InputCompareSourceNames;
			break;
		}

		case INPUT_NAME_COLUMN:
		{
			pfnSort = (PFNLVCOMPARE)InputCompareInputNames;
			break;
		}

		case DELAY_COLUMN:
		{
			pfnSort = (PFNLVCOMPARE)InputCompareDelays;
			break;
		}

		default:
		{
			Assert(FALSE);
			break;
		}
	}

	if (pfnSort != NULL)
	{
		m_ListCtrl.SortItems(pfnSort, (DWORD)eDirection);
	}
}

//------------------------------------------------------------------------------
// Purpose : Generates list of map entites that are being edited from the
//			 m_pObject list
// Input   : 
// Output  :
//------------------------------------------------------------------------------
void COP_Input::UpdateEntityList()
{
	// Clear old entity list
	m_pEntityList->RemoveAll();

	if (m_pObjectList != NULL)
	{
		FOR_EACH_OBJ( *m_pObjectList, pos )
		{
			CMapClass *pObject = m_pObjectList->Element(pos);
	
			if ((pObject != NULL) && (pObject->IsMapClass(MAPCLASS_TYPE(CMapEntity))))
			{
				CMapEntity *pEntity = (CMapEntity *)pObject;
				m_pEntityList->AddToTail(pEntity);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Input::UpdateConnectionList(void)
{
	UpdateEntityList();
	RemoveAllEntityConnections();
	m_bMultipleTargetNames = false;

	const char *pszTargetName = NULL;
	
	FOR_EACH_OBJ( *m_pEntityList, pos )
	{
		CMapEntity *pInEntity = m_pEntityList->Element(pos);

		if (!pszTargetName)
		{
			pszTargetName = pInEntity->GetKeyValue("targetname");
		}
		else if (pszTargetName != pInEntity->GetKeyValue("targetname"))
		{
			pszTargetName = pInEntity->GetKeyValue("targetname");
			m_bMultipleTargetNames = true;
		}

		if (pszTargetName)
		{
			CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
			CMapWorld *pWorld = pDoc->GetMapWorld();
			const CMapEntityList *pEntityList = pWorld->EntityList_GetList();
			
			FOR_EACH_OBJ( *pEntityList, pos2 )
			{
				CMapEntity *pTestEntity = pEntityList->Element(pos2);
				if (pTestEntity != NULL)
				{
					AddEntityConnections(pszTargetName, pTestEntity);
				}
			}
		}
	}

	// Update validity flag on all items
	for (int nItem = 0; nItem < m_ListCtrl.GetItemCount(); nItem++)
	{
		UpdateItemValidity(nItem);
	}

	m_ListCtrl.EnableWindow(true);
}

//-----------------------------------------------------------------------------
// Purpose: Adds or removes the little 'V' or '^' sort indicator as appropriate.
// Input  : nColumn - Index of column to update.
//			bSortColumn - true if this column is the sort column, false if not.
//			eDirection - Direction of sort, Sort_Ascending or Sort_Descending.
//-----------------------------------------------------------------------------
void COP_Input::UpdateColumnHeaderText(int nColumn, bool bIsSortColumn, SortDirection_t eDirection)
{
	char szHeaderText[MAX_PATH];

	LVCOLUMN Column;
	memset(&Column, 0, sizeof(Column));
	Column.mask = LVCF_TEXT;
	Column.pszText = szHeaderText;
	Column.cchTextMax = sizeof(szHeaderText);
	m_ListCtrl.GetColumn(nColumn, &Column);

	int nMarker = 0;

	if (szHeaderText[0] != '\0')
	{
		nMarker = strlen(szHeaderText) - 1;
		char chMarker = szHeaderText[nMarker];

		if ((chMarker == '>') || (chMarker == '<'))
		{
			nMarker -= 2;
		}
		else
		{
			nMarker++;
		}
	}

	if (bIsSortColumn)
	{
		if (nMarker != 0)
		{
			szHeaderText[nMarker++] = ' ';
			szHeaderText[nMarker++] = ' ';
		}

		szHeaderText[nMarker++] = (eDirection == Sort_Ascending) ? '>' : '<';
	}

	szHeaderText[nMarker] = '\0';

	m_ListCtrl.SetColumn(nColumn, &Column);
}


//-----------------------------------------------------------------------------
// Purpose: Called when our window is being destroyed.
//-----------------------------------------------------------------------------
void COP_Input::OnDestroy(void)
{
	RemoveAllEntityConnections();
}

//------------------------------------------------------------------------------
// Purpose: Set the selected item in the listbox
// Input  : pConnection
//------------------------------------------------------------------------------
void COP_Input::SetSelectedConnection(CEntityConnection *pConnection)
{
	m_ListCtrl.SetRedraw(FALSE);

	// Set selected item to be active and all others to false
	int nItemCount = m_ListCtrl.GetItemCount();	
	for (int nItem = 0; nItem < nItemCount; nItem++)
	{
		CInputConnection *pOutputConn = (CInputConnection *)m_ListCtrl.GetItemData(nItem);
		
		if ( pOutputConn->m_pConnection == pConnection)
		{
			m_ListCtrl.SetItemState(nItem,LVIS_SELECTED,LVIS_SELECTED);
		}
		else
		{
			m_ListCtrl.SetItemState(nItem, (unsigned int)~LVIS_SELECTED, (unsigned int)LVIS_SELECTED);
		}
	}

	m_ListCtrl.SetRedraw(TRUE);
}

void COP_Input::OnSize( UINT nType, int cx, int cy )
{
	m_AnchorMgr.OnSize();
}


