//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a dialog for editing the input/output connections of a
//			list of entities. The connections are displayed in a grid control,
//			each row of the grid control representing a connection that is
//			common to all entities being edited. For example, given these two ents:
//
//			Button01
//				OnDamaged Sound01 PlaySound 0 0
//				OnPressed Door01 Open 0 0
//
//			Button02
//				OnPressed Door01 Open 0 0
//
//			If these two entities were selected, the grid control would show:
//
//				OnPressed Door01 Open 0 0
//
//			because it is the only connection that is common to both entities.
//			Editing an entry in the grid control modifies the corresponding 
//			connection in all selected entities.
//
// TODO: persist sort column index, sort directions, and column sizes
// TODO: implement an external mode, where the grid shows all connections to unselected ents
//
//=============================================================================//

#include "stdafx.h"
#include "GlobalFunctions.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapWorld.h"
#include "ObjectProperties.h"
#include "OP_Output.h"
#include "ToolManager.h"
#include "MainFrm.h"
#include "utlrbtree.h"
#include "options.h"
#include ".\op_output.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning( disable : 4355 )


#define ICON_CONN_BAD		0
#define ICON_CONN_GOOD		1
#define ICON_CONN_BAD_GREY	2
#define ICON_CONN_GOOD_GREY	3


const char *PARAM_STRING_NONE = "<none>";


//
// Column indices for the list control.
//
const int ICON_COLUMN			= 0;
const int OUTPUT_NAME_COLUMN	= 1;
const int TARGET_NAME_COLUMN	= 2;
const int INPUT_NAME_COLUMN		= 3;
const int PARAMETER_COLUMN		= 4;
const int DELAY_COLUMN			= 5;
const int ONLY_ONCE_COLUMN		= 6;

IMPLEMENT_DYNCREATE(COP_Output, CObjectPage)


BEGIN_MESSAGE_MAP(COP_Output, CObjectPage)
	//{{AFX_MSG_MAP(COP_Output)
	ON_BN_CLICKED(IDC_ADD,		OnAdd)
	ON_BN_CLICKED(IDC_DELETE,	OnDelete)
	ON_BN_CLICKED(IDC_COPY,		OnCopy)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_PASTE,	OnPaste)
	ON_BN_CLICKED(IDC_MARK,		OnMark)
	ON_BN_CLICKED(IDC_PICK_ENTITY, OnPickEntity)
	ON_BN_CLICKED(IDC_PICK_ENTITY_PARAM,	OnPickEntityParam)
	ON_CBN_SELCHANGE(IDC_EDIT_CONN_INPUT,	OnSelChangeInput)
	ON_CBN_EDITUPDATE(IDC_EDIT_CONN_INPUT,	OnEditUpdateInput)
	ON_CBN_SELCHANGE(IDC_EDIT_CONN_OUTPUT,	OnSelChangeOutput)
	ON_CBN_EDITUPDATE(IDC_EDIT_CONN_OUTPUT, OnEditUpdateOutput)
	ON_CBN_SELCHANGE(IDC_EDIT_CONN_PARAM,	OnSelChangeParam)
	ON_CBN_EDITUPDATE(IDC_EDIT_CONN_PARAM,	OnEditUpdateParam)
	ON_EN_CHANGE(IDC_EDIT_CONN_DELAY,		OnEditDelay)
	ON_BN_CLICKED(IDC_EDIT_CONN_FIRE_ONCE,	OnFireOnce)
	ON_BN_CLICKED(IDC_SHOWHIDDENTARGETS,	OnShowHiddenTargetsAsBroken)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//	ON_CBN_SELCHANGE(IDC_EDIT_CONN_TARGET,	OnSelChangeTarget)
//	ON_CBN_EDITUPDATE(IDC_EDIT_CONN_TARGET, OnEditUpdateTarget)


//
// Static data.
//
CEntityConnectionList *COP_Output::m_pConnectionBuffer = new CEntityConnectionList;
CImageList *COP_Output::m_pImageList = NULL;


//-----------------------------------------------------------------------------
// Returns true if any of the target entities in the connection list are visible.
//-----------------------------------------------------------------------------
static bool AreAnyTargetEntitiesVisible( CEntityConnectionList *pList )
{
	for ( int i=0; i < pList->Count(); i++ )
	{
		if ( pList->Element(i)->AreAnyTargetEntitiesVisible() )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Compares by delays. Used as a secondary sort by all other columns.
//-----------------------------------------------------------------------------
static int CALLBACK ListCompareDelaysSecondary(COutputConnection *pOutputConn1, COutputConnection *pOutputConn2, SortDirection_t eDirection)
{
	CEntityConnectionList *pConnList1 = pOutputConn1->m_pConnList;
	CEntityConnectionList *pConnList2 = pOutputConn2->m_pConnList;
	CEntityConnection *pConn1 = pConnList1->Element(0);
	CEntityConnection *pConn2 = pConnList2->Element(0);
	return CEntityConnection::CompareDelaysSecondary(pConn1,pConn2,eDirection);
}


//-----------------------------------------------------------------------------
// Purpose: Compares by delays, does a secondary compare by output name.
//-----------------------------------------------------------------------------
static int CALLBACK ListCompareDelays(COutputConnection *pOutputConn1, COutputConnection *pOutputConn2, SortDirection_t eDirection)
{
	CEntityConnectionList *pConnList1 = pOutputConn1->m_pConnList;
	CEntityConnectionList *pConnList2 = pOutputConn2->m_pConnList;
	CEntityConnection *pConn1 = pConnList1->Element(0);
	CEntityConnection *pConn2 = pConnList2->Element(0);
	return CEntityConnection::CompareDelays(pConn1,pConn2,eDirection);
}


//-----------------------------------------------------------------------------
// Purpose: Compares by output name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
static int CALLBACK ListCompareOutputNames(COutputConnection *pOutputConn1, COutputConnection *pOutputConn2, SortDirection_t eDirection)
{
	CEntityConnectionList *pConnList1 = pOutputConn1->m_pConnList;
	CEntityConnectionList *pConnList2 = pOutputConn2->m_pConnList;
	CEntityConnection *pConn1 = pConnList1->Element(0);
	CEntityConnection *pConn2 = pConnList2->Element(0);
	return CEntityConnection::CompareOutputNames(pConn1,pConn2,eDirection);
}


//-----------------------------------------------------------------------------
// Purpose: Compares by input name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
static int CALLBACK ListCompareInputNames(COutputConnection *pOutputConn1, COutputConnection *pOutputConn2, SortDirection_t eDirection)
{
	CEntityConnectionList *pConnList1 = pOutputConn1->m_pConnList;
	CEntityConnectionList *pConnList2 = pOutputConn2->m_pConnList;
	CEntityConnection *pConn1 = pConnList1->Element(0);
	CEntityConnection *pConn2 = pConnList2->Element(0);
	return	(CEntityConnection::CompareInputNames(pConn1,pConn2,eDirection));
}


//-----------------------------------------------------------------------------
// Purpose: Compares by source name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
static int CALLBACK ListCompareSourceNames(COutputConnection *pOutputConn1, COutputConnection *pOutputConn2, SortDirection_t eDirection)
{
	CEntityConnectionList *pConnList1 = pOutputConn1->m_pConnList;
	CEntityConnectionList *pConnList2 = pOutputConn2->m_pConnList;
	CEntityConnection *pConn1 = pConnList1->Element(0);
	CEntityConnection *pConn2 = pConnList2->Element(0);
	return	(CEntityConnection::CompareSourceNames(pConn1,pConn2,eDirection));
}


//-----------------------------------------------------------------------------
// Purpose: Compares by target name, does a secondary compare by delay.
//-----------------------------------------------------------------------------
static int CALLBACK ListCompareTargetNames(COutputConnection *pOutputConn1, COutputConnection *pOutputConn2, SortDirection_t eDirection)
{
	CEntityConnectionList *pConnList1 = pOutputConn1->m_pConnList;
	CEntityConnectionList *pConnList2 = pOutputConn2->m_pConnList;
	CEntityConnection *pConn1 = pConnList1->Element(0);
	CEntityConnection *pConn2 = pConnList2->Element(0);
	return	(CEntityConnection::CompareTargetNames(pConn1,pConn2,eDirection));
}


//-----------------------------------------------------------------------------
// Purpose: Called by the entity picker tool when an entity is picked. This
//			stuffs the entity name into the smartedit control.
//-----------------------------------------------------------------------------
void COP_OutputPickEntityTarget::OnNotifyPickEntity(CToolPickEntity *pTool)
{
	//
	// Update the edit control text with the entity name. This text will be
	// stuffed into the local keyvalue storage in OnChangeSmartControl.
	//
	CMapEntityList Full;
	CMapEntityList Partial;
	pTool->GetSelectedEntities(Full, Partial);
	CMapEntity *pEntity = Full.Element(0);
	if (pEntity)
	{
		const char *pszName = pEntity->GetKeyValue("targetname");
		if (!pszName)
		{
			pszName = "";
		}

		switch ( m_nDlgItem )
		{
			case IDC_EDIT_CONN_TARGET:
			{
				// FIXME: this should be called automatically, but it isn't
				m_pDlg->m_ComboTarget.SelectItem(pszName);
				break;
			}

			case IDC_EDIT_CONN_PARAM:
			{
				// FIXME: this should be called automatically, but it isn't
				m_pDlg->GetDlgItem(m_nDlgItem)->SetWindowText(pszName);
				m_pDlg->OnEditUpdateParam();
				break;
			}
			
			default:
			{
				m_pDlg->GetDlgItem(m_nDlgItem)->SetWindowText(pszName);
				break;
			}
		}
	}

	m_pDlg->StopPicking();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
COP_Output::COP_Output(void)
	: CObjectPage(COP_Output::IDD), m_ComboTarget( this )
{
	m_bIgnoreTextChanged = false;
	m_pObjectList = NULL;
	m_pEditObjectRuntimeClass = RUNTIME_CLASS(editCMapClass);
	m_nSortColumn = OUTPUT_NAME_COLUMN;
	m_pMapEntityList = NULL;
	m_fDelay = 0;
	m_bPickingEntities = false;

	bSkipEditControlRefresh = false;
	//
	// All columns initially sort in ascending order.
	//
	for (int i = 0; i < OUTPUT_LIST_NUM_COLUMNS; i++)
	{
		m_eSortDirection[i] = Sort_Ascending;
	}

	m_PickEntityTarget.AttachEntityDlg(this);
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
COP_Output::~COP_Output(void)
{
}


void COP_Output::OnTextChanged( const char *pText )
{
	if ( m_bIgnoreTextChanged )
		return;
	
	// Updating the listbox data, will trigger the edit
	// controls to update.  They don't need to be
	bSkipEditControlRefresh = true;

	// Target has changed so we need to update for list of inputs
	// that are valid for this target
	FillInputList();
	FilterInputList();

	m_ComboInput.SetWindowText(m_strInput);

	UpdateEditedTargets();
}


//------------------------------------------------------------------------------
// Purpose: Updates the validity flag on the given item in the list control
// Input  : nItem - 
//------------------------------------------------------------------------------
void COP_Output::UpdateItemValidity(int nItem)
{
	COutputConnection *pOutputConn = (COutputConnection *)m_ListCtrl.GetItemData(nItem);
	CEntityConnectionList *pConnectionList = pOutputConn->m_pConnList;
	bool bShared = (m_EntityList.Count() == pConnectionList->Count());

	bool bShowHiddenTargets = ShouldShowHiddenTargets();

	int nIcon;
	if (ValidateConnections(pOutputConn, bShowHiddenTargets))
	{
		if ( !bShowHiddenTargets && !AreAnyTargetEntitiesVisible( pConnectionList ) )
			nIcon = ICON_CONN_GOOD_GREY;
		else if ( bShared )
			nIcon = ICON_CONN_GOOD;
		else
			nIcon = ICON_CONN_GOOD_GREY;
		
		pOutputConn->m_bIsValid = true;
	}
	else
	{
		nIcon = (bShared ? ICON_CONN_BAD : ICON_CONN_BAD_GREY);
		pOutputConn->m_bIsValid = false;
	}
	m_ListCtrl.SetItem(nItem,0,LVIF_IMAGE, 0, nIcon, 0, 0, 0 );
}


//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void COP_Output::UpdateValidityButton(void)
{
	CObjectProperties *pParent = (CObjectProperties*) GetParent();

	// Get status of all connections
	int nItemCount = m_ListCtrl.GetItemCount();	

	if (nItemCount == 0)
	{
		pParent->SetOutputButtonState(CONNECTION_NONE);
		return;
	}

	for (int nItem = 0; nItem < nItemCount; nItem++)
	{
		COutputConnection *pOutputConn = (COutputConnection *)m_ListCtrl.GetItemData(nItem);
		if (!pOutputConn->m_bIsValid)
		{
			pParent->SetOutputButtonState(CONNECTION_BAD);
			return;
		}
	}
	pParent->SetOutputButtonState(CONNECTION_GOOD);
}


//------------------------------------------------------------------------------
// Purpose: Return true if all connections entries are valid for the given
//			 output connection.  Return false otherwise
//------------------------------------------------------------------------------
bool COP_Output::ValidateConnections(COutputConnection *pOutputConn, bool bVisibilityCheck)
{
	int nCount = pOutputConn->m_pConnList->Count();
	for (int i = 0; i < nCount; i++)
	{
		CEntityConnection *pConnection = pOutputConn->m_pConnList->Element(i);
		if (pConnection != NULL)
		{
			// Check validity of output for the list of entities
			if (!CEntityConnection::ValidateOutput(pOutputConn->m_pEntityList,pConnection->GetOutputName()))
			{
				return false;
			}

			// Check validity of target entity (is it in the map?)
			if (!CEntityConnection::ValidateTarget(m_pMapEntityList, bVisibilityCheck, pConnection->GetTargetName()))
			{
				return false;
			}

			// Check validity of input
			if (!CEntityConnection::ValidateInput(pConnection->GetTargetName(), pConnection->GetInputName(), bVisibilityCheck))
			{
				return false;
			}
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEntity - 
//			bFirst - 
//-----------------------------------------------------------------------------
void COP_Output::AddEntityConnections(CMapEntity *pEntity, bool bFirst)
{
	m_ListCtrl.SetRedraw(FALSE);

	//
	// The first entity simply adds its connections to the list.
	//
	int nConnCount = pEntity->Connections_GetCount();
	for (int i = 0; i < nConnCount; i++)
	{
		CEntityConnection *pConnection = pEntity->Connections_Get(i);
		if (pConnection != NULL)
		{
			// First check if the connection already exists, if so just add to it
			bool bFound = false;
			int nItemCount = m_ListCtrl.GetItemCount();	

			if (nItemCount > 0)
			{
				for (int nItem = nItemCount - 1; nItem >= 0; nItem--)
				{
					COutputConnection		*pOutputConn = (COutputConnection *)m_ListCtrl.GetItemData(nItem);
					CEntityConnectionList	*pConnList	 = pOutputConn->m_pConnList;
					CEntityConnection		*pTestConn   = pConnList->Element(0);
					if (pTestConn->CompareConnection(pConnection))
					{
						// Don't consolidate duplicate connections in the same entity
						// Show them twice so the user will see
						if ( pOutputConn->m_pEntityList->Find(pEntity) == -1)
						{
							pConnList->AddToTail(pConnection);
							pOutputConn->m_pEntityList->AddToTail(pEntity);
							bFound = true;
							break;
						}

					}
				}
			}
			
			if (!bFound)
			{
				m_ListCtrl.SetItemCount(nItemCount + 1);

				m_ListCtrl.InsertItem(LVIF_IMAGE, nItemCount, "", 0, 0, ICON_CONN_GOOD, 0);

				m_ListCtrl.SetItemText(nItemCount, OUTPUT_NAME_COLUMN, pConnection->GetOutputName());
				m_ListCtrl.SetItemText(nItemCount, TARGET_NAME_COLUMN, pConnection->GetTargetName());
				m_ListCtrl.SetItemText(nItemCount, INPUT_NAME_COLUMN, pConnection->GetInputName());

				// Build the string for the delay.
				float fDelay = pConnection->GetDelay();
				char szTemp[MAX_PATH];
				sprintf(szTemp, "%.2f", fDelay);
				m_ListCtrl.SetItemText(nItemCount, DELAY_COLUMN, szTemp);

				// Fire once
				m_ListCtrl.SetItemText(nItemCount, ONLY_ONCE_COLUMN, (pConnection->GetTimesToFire() == EVENT_FIRE_ALWAYS) ? "No" : "Yes");
				m_ListCtrl.SetItemText(nItemCount, PARAMETER_COLUMN, pConnection->GetParam());

				
				// Set list ctrl data 
				COutputConnection* pOutputConn	= new COutputConnection;
				pOutputConn->m_pConnList		= new CEntityConnectionList;
				pOutputConn->m_pEntityList		= new CMapEntityList;
				pOutputConn->m_pConnList->AddToTail(pConnection);
				pOutputConn->m_pEntityList->AddToTail(pEntity);
				pOutputConn->m_bOwnedByAll		= true;
				m_ListCtrl.SetItemData(nItemCount, (DWORD)pOutputConn);
				
				nItemCount++;
			}
		}
	}

	m_ListCtrl.SetRedraw(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDX - 
//-----------------------------------------------------------------------------
void COP_Output::DoDataExchange(CDataExchange *pDX)
{
	CObjectPage::DoDataExchange(pDX);

	//{{AFX_DATA_MAP(COP_Output)
	DDX_Control(pDX, IDC_LIST, m_ListCtrl);
	DDX_Text(pDX, IDC_EDIT_CONN_DELAY, m_fDelay);
	DDX_CBString(pDX, IDC_EDIT_CONN_OUTPUT, m_strOutput);
	DDX_CBString(pDX, IDC_EDIT_CONN_TARGET, m_strTarget);
	DDX_CBString(pDX, IDC_EDIT_CONN_INPUT, m_strInput);
	DDX_CBString(pDX, IDC_EDIT_CONN_PARAM, m_strParam);
	DDX_Check(pDX, IDC_EDIT_CONN_FIRE_ONCE, m_bFireOnce);
	DDX_Control(pDX, IDC_SHOWHIDDENTARGETS, m_ctlShowHiddenTargetsAsBroken);
	DDX_Control(pDX, IDC_ADD, m_AddControl);
	DDX_Control(pDX, IDC_PASTE, m_PasteControl);
	DDX_Control(pDX, IDC_DELETE, m_DeleteControl);

	//}}AFX_DATA_MAP
}


bool COP_Output::ShouldShowHiddenTargets()
{
	return (Options.general.bShowHiddenTargetsAsBroken == TRUE);
}


//------------------------------------------------------------------------------
// Purpose: Enables or Disables all edit controls
// Input  : bValue - 
//------------------------------------------------------------------------------
void COP_Output::EnableEditControls(bool bValue)
{
	m_ComboOutput.EnableWindow(bValue);
	EnableTarget(bValue);
	m_ComboInput.EnableWindow(bValue);

	CButton *pButton = (CButton *)GetDlgItem(IDC_EDIT_CONN_FIRE_ONCE);
	pButton->EnableWindow(bValue);

	CEdit *pDelayEdit = (CEdit *)GetDlgItem(IDC_EDIT_CONN_DELAY);
	pDelayEdit->EnableWindow(bValue);

	CComboBox *pParamCombo = (CComboBox *)GetDlgItem(IDC_EDIT_CONN_PARAM);
	pParamCombo->EnableWindow(bValue);
	GetDlgItem(IDC_PICK_ENTITY_PARAM)->EnableWindow( bValue );

	// Clear any values
	if (!bValue)
	{
		m_ComboTarget.ForceEditControlText( "" );
		m_ComboInput.SetWindowText("");
		m_ComboOutput.SetWindowText("");
		pParamCombo->SetCurSel(0);
		pDelayEdit->SetWindowText("0.0");
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMapEntityList - 
//-----------------------------------------------------------------------------
void COP_Output::SetMapEntityList(const CMapEntityList *pMapEntityList)
{
	m_pMapEntityList = pMapEntityList;
	FillTargetList();
}


//------------------------------------------------------------------------------
// Purpose: Updates data displayed in edit controls
//------------------------------------------------------------------------------
void COP_Output::UpdateEditControls(void)
{
	//
	// Build a list of connections to edit.
	//
	m_EditList.RemoveAll();

	m_AddControl.EnableWindow( ( m_bCanEdit ? TRUE : FALSE ) );
	m_PasteControl.EnableWindow( ( m_bCanEdit ? TRUE : FALSE ) );
	m_DeleteControl.EnableWindow( ( m_bCanEdit ? TRUE : FALSE ) );

	// If nothing is selected, disable edit controls
	if (!m_ListCtrl.IsWindowEnabled() || m_ListCtrl.GetSelectedCount() == 0)
	{
		EnableEditControls(false);
		return;
	}

	for (int nItem = 0; nItem < m_ListCtrl.GetItemCount(); nItem++)
	{

		if (m_ListCtrl.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
		{
			COutputConnection *pOutputConn = (COutputConnection *)m_ListCtrl.GetItemData(nItem);
			m_EditList.AddVectorToTail(*pOutputConn->m_pConnList);
		}
	}

	if (m_EditList.Count() > 0)
	{
		SetConnection(&m_EditList);

		FillOutputList();
		FillInputList();

		// We must ignore the text changed event here or else it'll set all selected outputs to the same value.
		m_bIgnoreTextChanged = true;
		m_ComboTarget.SelectItem(m_strTarget);
		m_bIgnoreTextChanged = false;
		
		m_ComboInput.SetWindowText(m_strInput);
		m_ComboOutput.SetWindowText(m_strOutput);
		m_CheckBoxFireOnce.SetCheck(m_bFireOnce);

		CEdit *pDelayEdit = ( CEdit* )GetDlgItem( IDC_EDIT_CONN_DELAY );
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%.2f", m_fDelay);
		pDelayEdit->SetWindowText(szTemp);
		
		CComboBox* pParamEdit = ( CComboBox* )GetDlgItem( IDC_EDIT_CONN_PARAM );
		pParamEdit->SetWindowText(m_strParam);

		FilterInputList();

		//
		// Update the UI state based on our current data.
		//
		char szBuf[MAX_IO_NAME_LEN];

		CClassOutput *pOutput = GetOutput(szBuf, sizeof(szBuf));
		UpdateCombosForSelectedOutput(pOutput);

		CClassInput *pInput = GetInput(szBuf, sizeof(szBuf));
		UpdateCombosForSelectedInput(pInput);

		//CMapEntityList *pTarget = GetTarget(szBuf, sizeof(szBuf));
		//UpdateCombosForSelectedTarget(pTarget);
	}

	if ( m_bCanEdit == false )
	{
		EnableEditControls( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a connection to all entities being edited.
//-----------------------------------------------------------------------------
void COP_Output::OnAdd(void)
{
	FOR_EACH_OBJ( m_EntityList, pos)
	{
		CMapEntity *pEntity = m_EntityList.Element(pos);
		if (pEntity != NULL)
		{
			CEntityConnection *pConnection = new CEntityConnection;
			pEntity->Connections_Add(pConnection);
		}
	}

	UpdateConnectionList();

	// Set selection to new item, and move the focus to the output combo
	// so they can just start editing.
	int nCount = m_ListCtrl.GetItemCount();
	SetSelectedItem(nCount - 1);
	m_ListCtrl.EnsureVisible(nCount - 1, FALSE);
	GetDlgItem(IDC_EDIT_CONN_OUTPUT)->SetFocus();
}


//------------------------------------------------------------------------------
// Purpose: Clear copy buffer
//------------------------------------------------------------------------------
void COP_Output::EmptyCopyBuffer(void)
{
	// Delete any old connections
	int nConnCount = m_pConnectionBuffer->Count();
	for (int i = 0; i < nConnCount; i++)
	{
		CEntityConnection *pConnection = m_pConnectionBuffer->Element(i);
		if (pConnection != NULL)
		{
			delete pConnection;
		}
	}
	m_pConnectionBuffer->RemoveAll();
	
}


//-----------------------------------------------------------------------------
// Purpose: Copies list of selected connections into copy buffer
//-----------------------------------------------------------------------------
void COP_Output::OnCopy(void)
{
	EmptyCopyBuffer();

	if (m_ListCtrl.GetSelectedCount() != 0)
	{
		int nCount = m_ListCtrl.GetItemCount();
		if (nCount > 0)
		{
			for (int nItem = nCount - 1; nItem >= 0; nItem--)
			{
				if (m_ListCtrl.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
				{
					//
					// Each item in the list control is a list of identical connections that are contained
					// in multiple entities. Add each selected connection to the selected entities.
					//
					COutputConnection *pOutputConn = (COutputConnection *)m_ListCtrl.GetItemData(nItem);
					CEntityConnectionList *pConnList = pOutputConn->m_pConnList;
					if (pConnList != NULL)
					{
						CEntityConnection *pConnection = pConnList->Element(0);
						if (pConnection)
						{
							CEntityConnection *pNewConnection = new CEntityConnection;
							*pNewConnection = *pConnection;
							m_pConnectionBuffer->AddToTail(pNewConnection);
						}
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a connection to all entities being edited.
//-----------------------------------------------------------------------------
void COP_Output::OnPaste(void)
{
	// Early out
	if (!m_pConnectionBuffer->Count())
	{
		return;
	}

	CUtlVector<CEntityConnection *> NewConnections;

	// Add connections from copy buffer to all selected entities
	FOR_EACH_OBJ( m_EntityList, pos )
	{
		CMapEntity *pEntity = m_EntityList.Element(pos);
		if (pEntity != NULL)
		{
			int nConnCount = m_pConnectionBuffer->Count();
			for (int i = 0; i < nConnCount; i++)
			{
				CEntityConnection *pConnection = m_pConnectionBuffer->Element(i);
				if (pConnection != NULL)
				{
					CEntityConnection *pNewConnection = new CEntityConnection;
					*pNewConnection = *pConnection;
					pEntity->Connections_Add(pNewConnection);

					NewConnections.AddToTail(pNewConnection);
				}
			}
		}
	}
	UpdateConnectionList();
	SortListByColumn(m_nSortColumn, m_eSortDirection[m_nSortColumn]);
	SetSelectedConnections(NewConnections);
	GetDlgItem(IDC_EDIT_CONN_OUTPUT)->SetFocus();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Output::OnPickEntity(void)
{
	CButton *pButton = (CButton *)GetDlgItem(IDC_PICK_ENTITY);
	Assert(pButton != NULL);

	if (pButton != NULL)
	{
		if (pButton->GetCheck())
		{
			//
			// Activate the entity picker tool.
			//
			m_bPickingEntities = true;
			m_PickEntityTarget.AttachDlgItem( IDC_EDIT_CONN_TARGET );
			CToolPickEntity *pTool = (CToolPickEntity *)ToolManager()->GetToolForID(TOOL_PICK_ENTITY);
			pTool->Attach(&m_PickEntityTarget);
			ToolManager()->SetTool(TOOL_PICK_ENTITY);
			GetDlgItem(IDC_PICK_ENTITY_PARAM)->EnableWindow( false );
		}
		else
		{
			StopPicking();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Output::OnPickEntityParam(void)
{
	CButton *pButton = (CButton *)GetDlgItem(IDC_PICK_ENTITY_PARAM);
	Assert(pButton != NULL);

	if (pButton != NULL)
	{
		if (pButton->GetCheck())
		{
			//
			// Activate the entity picker tool.
			//
			m_bPickingEntities = true;
			m_PickEntityTarget.AttachDlgItem( IDC_EDIT_CONN_PARAM );
			CToolPickEntity *pTool = (CToolPickEntity *)ToolManager()->GetToolForID(TOOL_PICK_ENTITY);
			pTool->Attach(&m_PickEntityTarget);
			ToolManager()->SetTool(TOOL_PICK_ENTITY);
			GetDlgItem(IDC_PICK_ENTITY)->EnableWindow( false );
		}
		else
		{
			StopPicking();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Deletes all selected items from the connection list, and removes the
//			corresponding connections from the list of entities being edited.
//-----------------------------------------------------------------------------
void COP_Output::OnDelete(void)
{
	if (m_ListCtrl.GetSelectedCount() != 0)
	{
		int nCount		= m_ListCtrl.GetItemCount();
		int nLastItem	= 0;
		if (nCount > 0)
		{
			for (int nItem = nCount - 1; nItem >= 0; nItem--)
			{
				if (m_ListCtrl.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
				{
					//
					// Each item in the list control is a list of identical connections that are contained
					// in multiple entities. Since we don't store the containing entity along with the connection,
					// just try to remove all the connections in the list from all the selected entities.
					//
					COutputConnection *pOutputConn = (COutputConnection *)m_ListCtrl.GetItemData(nItem);
					CEntityConnectionList *pConnList = pOutputConn->m_pConnList;
					m_ListCtrl.DeleteItem(nItem);

					if (pConnList != NULL)
					{
						int nConnCount = pConnList->Count();
						for (int nConn = 0; nConn < nConnCount; nConn++)
						{
							CEntityConnection *pConnection = pConnList->Element(nConn);
							if (pConnection != NULL)
							{
								//
								// Remove the connection from all entities being edited.
								//
								FOR_EACH_OBJ( m_EntityList, pos )
								{
									CMapEntity *pEntity = m_EntityList.Element(pos);
									if (pEntity != NULL)
									{
										pEntity->Connections_Remove(pConnection);
									}
								}

								//								
								// Remove the connection from the upstream list of all entities it targets.
								//
								CMapEntityList *pTargetList = pConnection->GetTargetEntityList();
								if ( pTargetList )
								{
									FOR_EACH_OBJ( *pTargetList, pos2 )
									{
										CMapEntity *pEntity = pTargetList->Element( pos2 );
										pEntity->Upstream_Remove( pConnection );
									}
								}
							}
							
							delete pConnection;
						}
						
						delete pConnList;
					}
					// Keep track of last item so can set selection focus
					nLastItem = nItem;
				}
			}
		}

		// Set selection focus as point of deletion or on last item 
		int nNumItems = m_ListCtrl.GetItemCount()-1;
		if (nLastItem > nNumItems)
		{
			nLastItem = nNumItems;
		}
		SetSelectedItem(nLastItem);
		UpdateValidityButton();
	}
}


//------------------------------------------------------------------------------
// Purpose : Take the user to the output page of the selected entity that
//			 targets me.  
// Input   :
// Output  :
//------------------------------------------------------------------------------
void COP_Output::OnMark(void)
{
	int			nCount	= m_ListCtrl.GetItemCount();
	CMapDoc*	pDoc	= CMapDoc::GetActiveMapDoc();

	CEntityConnection *pConnection = NULL; 

	if (nCount > 0 && pDoc)
	{
		CMapObjectList Select;

		for (int nItem = nCount - 1; nItem >= 0; nItem--)
		{
			if (m_ListCtrl.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
			{
				COutputConnection *pOutputConn = (COutputConnection *)m_ListCtrl.GetItemData(nItem);
				pConnection = pOutputConn->m_pConnList->Element(0);

				CMapDoc *pDocActive = CMapDoc::GetActiveMapDoc();
				if ( pDocActive != NULL)
				{
					CMapEntityList Found;
					pDocActive->FindEntitiesByName(Found, m_ListCtrl.GetItemText(nItem, TARGET_NAME_COLUMN), false);

					FOR_EACH_OBJ( Found, pos )
					{
						CMapEntity *pEntity = Found.Element(pos);
						Select.AddToTail(pEntity);
					}
				}
			}
		}
		if (Select.Count()>0)
		{
			pDoc->SelectObjectList(&Select);

			// (a bit squirly way of doing this)
			if ( Select.Count()==1 )
				GetMainWnd()->pObjectProperties->SetPageToInput(pConnection);
		
			pDoc->Center2DViewsOnSelection();
		}
		else
		{
			MessageBox("No entities were found with that targetname.", "No entities found", MB_ICONINFORMATION | MB_OK);
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets up the list view columns, initial sort column.
//-----------------------------------------------------------------------------
BOOL COP_Output::OnInitDialog(void)
{
	CObjectPage::OnInitDialog();

	m_ComboOutput.SubclassDlgItem(IDC_EDIT_CONN_OUTPUT, this);
	m_ComboInput.SubclassDlgItem(IDC_EDIT_CONN_INPUT, this);
	m_ComboTarget.SubclassDlgItem(IDC_EDIT_CONN_TARGET, this);
	m_CheckBoxFireOnce.SubclassDlgItem(IDC_EDIT_CONN_FIRE_ONCE, this);

	m_ListCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_HEADERDRAGDROP);
	
	m_ListCtrl.InsertColumn(ICON_COLUMN, "", LVCFMT_CENTER, 20);
	m_ListCtrl.InsertColumn(OUTPUT_NAME_COLUMN, "My Output", LVCFMT_LEFT, 70);
	m_ListCtrl.InsertColumn(TARGET_NAME_COLUMN, "Target Entity", LVCFMT_LEFT, 70);
	m_ListCtrl.InsertColumn(INPUT_NAME_COLUMN, "Target Input", LVCFMT_LEFT, 70);
	m_ListCtrl.InsertColumn(DELAY_COLUMN, "Delay", LVCFMT_LEFT, 70);
	m_ListCtrl.InsertColumn(ONLY_ONCE_COLUMN, "Only Once", LVCFMT_LEFT, 70);
	m_ListCtrl.InsertColumn(PARAMETER_COLUMN, "Parameter", LVCFMT_LEFT, 70);

	UpdateConnectionList();

	SetSortColumn(m_nSortColumn, m_eSortDirection[m_nSortColumn]);

	// Force an update of the column header text so that the sort indicator is shown.
	UpdateColumnHeaderText(m_nSortColumn, true, m_eSortDirection[m_nSortColumn]);

	ResizeColumns();

	m_strLastParam.Empty();

	// Select the first item in the combo box
	SetSelectedItem(0);
	   
	// Create image list.  Is deleted automatically when listctrl is deleted
	if (!m_pImageList)
	{
		CWinApp *pApp = AfxGetApp();
		m_pImageList = new CImageList();
		Assert(m_pImageList != NULL);    // serious allocation failure checking
		m_pImageList->Create(16, 16, TRUE,   1, 0);
		m_pImageList->Add(pApp->LoadIcon( IDI_OUTPUTBAD ));
		m_pImageList->Add(pApp->LoadIcon( IDI_OUTPUT ));
		m_pImageList->Add(pApp->LoadIcon( IDI_OUTPUTBAD_GREY ));
		m_pImageList->Add(pApp->LoadIcon( IDI_OUTPUT_GREY ));

	}
	m_ListCtrl.SetImageList(m_pImageList, LVSIL_SMALL );

	// Apply the eyedropper image to the picker buttons.
	CButton *pButton = (CButton *)GetDlgItem(IDC_PICK_ENTITY);
	if (pButton)
	{
		CWinApp *pApp = AfxGetApp();
		HICON hIcon = pApp->LoadIcon(IDI_EYEDROPPER);
		pButton->SetIcon(hIcon);
	}

	pButton = (CButton *)GetDlgItem(IDC_PICK_ENTITY_PARAM);
	if (pButton)
	{
		CWinApp *pApp = AfxGetApp();
		HICON hIcon = pApp->LoadIcon(IDI_EYEDROPPER);
		pButton->SetIcon(hIcon);
	}

	CAnchorDef anchorDefs[] = 
	{
		CAnchorDef( IDC_LIST, k_eSimpleAnchorAllSides ),
		CAnchorDef( IDC_OUTPUTS_STATIC_PANEL, k_eAnchorLeft, k_eAnchorBottom, k_eAnchorRight, k_eAnchorBottom ),
		CAnchorDef( IDC_OUTPUT_LABEL, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_TARGETS_LABEL, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_VIA_INPUT_LABEL, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_PARAMETER_LABEL, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_DELAY_LABEL, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_EDIT_CONN_DELAY, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_EDIT_CONN_FIRE_ONCE, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_EDIT_CONN_PARAM, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_EDIT_CONN_INPUT, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_EDIT_CONN_TARGET, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_EDIT_CONN_OUTPUT, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_PICK_ENTITY, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_PICK_ENTITY_PARAM, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_MARK, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_ADD, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_COPY, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_PASTE, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_DELETE, k_eSimpleAnchorBottomSide ),
		CAnchorDef( IDC_SHOWHIDDENTARGETS, k_eSimpleAnchorBottomRight )		
	};
	m_AnchorMgr.Init( GetSafeHwnd(), anchorDefs, ARRAYSIZE( anchorDefs ) );

	// Set the last state this was at.
	m_ctlShowHiddenTargetsAsBroken.SetCheck( ShouldShowHiddenTargets() );
	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wParam - 
//			lParam - 
//			pResult - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL COP_Output::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT *pResult)
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

			case LVN_ITEMCHANGED:
			{
				NMLISTVIEW *pnmv = (NMLISTVIEW *)lParam;
				if ( ( pnmv->uNewState & LVIS_SELECTED ) != ( pnmv->uOldState & LVIS_SELECTED ) )
				{
					// Listbox selection has changed so update edit controls
					if (!bSkipEditControlRefresh)
					{
						UpdateEditControls();
					}
					bSkipEditControlRefresh = false;

					// Forget the saved param, because it was for a different I/O connection.
					m_strLastParam.Empty();
				}
				
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
void COP_Output::RemoveAllEntityConnections(void)
{
	m_ListCtrl.SetRedraw(FALSE);

	int nCount = m_ListCtrl.GetItemCount();
	if (nCount > 0)
	{
		for (int nItem = nCount - 1; nItem >= 0; nItem--)
		{
			COutputConnection *pOutputConn = (COutputConnection *)m_ListCtrl.GetItemData(nItem);
			CEntityConnectionList *pConnList = pOutputConn->m_pConnList;
			CMapEntityList *pEntityList = pOutputConn->m_pEntityList;

			m_ListCtrl.DeleteItem(nItem);

			delete pOutputConn;
			delete pConnList;
			delete pEntityList;
		}
	}

	m_ListCtrl.SetRedraw(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Mode - 
//			pData - 
//-----------------------------------------------------------------------------
void COP_Output::UpdateData( int Mode, PVOID pData, bool bCanEdit )
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
//			SetSelectedItem(0);
			break;
		}

		case LoadFinished:
		{
			m_ListCtrl.DeleteAllItems();
			UpdateConnectionList();
			SetSelectedItem(0);
			SortListByColumn(m_nSortColumn, m_eSortDirection[m_nSortColumn]);
		}
	}

	UpdateEditControls();
}


//------------------------------------------------------------------------------
// Purpose: Generates list of map entites that are being edited from the
//			 m_pObject list
//------------------------------------------------------------------------------
void COP_Output::UpdateEntityList(void)
{
	// Clear old entity list
	m_EntityList.RemoveAll();

	if (m_pObjectList != NULL)
	{
		FOR_EACH_OBJ( *m_pObjectList, pos )
		{
			CMapClass *pObject = m_pObjectList->Element(pos);
	
			if ((pObject != NULL) && (pObject->IsMapClass(MAPCLASS_TYPE(CMapEntity))) )
			{
				CMapEntity *pEntity = (CMapEntity *)pObject;
				m_EntityList.AddToTail(pEntity);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nColumn - 
//			eDirection - 
//-----------------------------------------------------------------------------
void COP_Output::SetSortColumn(int nColumn, SortDirection_t eDirection)
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
void COP_Output::SortListByColumn(int nColumn, SortDirection_t eDirection)
{
	PFNLVCOMPARE pfnSort = NULL;

	switch (nColumn)
	{
		case ONLY_ONCE_COLUMN:
		{
			//No Sort
			break;
		}

		case PARAMETER_COLUMN:
		{
			//No Sort
			break;
		}

		case OUTPUT_NAME_COLUMN:
		{
			pfnSort = (PFNLVCOMPARE)ListCompareOutputNames;
			break;
		}

		case TARGET_NAME_COLUMN:
		{
			pfnSort = (PFNLVCOMPARE)ListCompareTargetNames;
			break;
		}

		case INPUT_NAME_COLUMN:
		{
			pfnSort = (PFNLVCOMPARE)ListCompareInputNames;
			break;
		}

		case DELAY_COLUMN:
		{
			pfnSort = (PFNLVCOMPARE)ListCompareDelays;
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


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Output::ResizeColumns(void)
{
	if (m_ListCtrl.GetItemCount() > 0)
	{
		m_ListCtrl.SetColumnWidth(OUTPUT_NAME_COLUMN, LVSCW_AUTOSIZE);
		m_ListCtrl.SetColumnWidth(TARGET_NAME_COLUMN, LVSCW_AUTOSIZE);
		m_ListCtrl.SetColumnWidth(INPUT_NAME_COLUMN, LVSCW_AUTOSIZE);
		m_ListCtrl.SetColumnWidth(DELAY_COLUMN, LVSCW_AUTOSIZE_USEHEADER);
		m_ListCtrl.SetColumnWidth(ONLY_ONCE_COLUMN, LVSCW_AUTOSIZE_USEHEADER);
		m_ListCtrl.SetColumnWidth(PARAMETER_COLUMN, LVSCW_AUTOSIZE);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Output::UpdateConnectionList(void)
{	
	// Get list of all entities in the world
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	Assert(pDoc != NULL);
	if (!pDoc)
		return;

	CMapWorld *pWorld = pDoc->GetMapWorld();
	Assert(pWorld != NULL); // dvs: I've seen pWorld be NULL on app shutdown, not sure why we ended up here though
	if (!pWorld)
		return;

	SetMapEntityList(pWorld->EntityList_GetList());

	UpdateEntityList();
	RemoveAllEntityConnections();

	bool bFirst = true;

	FOR_EACH_OBJ( m_EntityList, pos )
	{
		CMapEntity *pEntity = m_EntityList.Element(pos);
		if (pEntity != NULL)
		{
			AddEntityConnections(pEntity, bFirst);
			bFirst = false;
		}
	}
	
	// Update validity flag on all items
	for (int nItem = 0; nItem < m_ListCtrl.GetItemCount(); nItem++)
	{
		UpdateItemValidity(nItem);
	}
	UpdateValidityButton();

	ResizeColumns();
}


//------------------------------------------------------------------------------
// Purpose: Set the selected item in the listbox by index.
// Input  : nSelectItem - 
//------------------------------------------------------------------------------
void COP_Output::SetSelectedItem(int nSelectItem)
{
	m_ListCtrl.SetRedraw(FALSE);

	// Set selected item to be active and all others to false
	int nItemCount = m_ListCtrl.GetItemCount();	
	for (int nItem = 0; nItem < nItemCount; nItem++)
	{
		if (nItem == nSelectItem)
		{
			m_ListCtrl.SetItemState(nItem, (unsigned int)LVIS_SELECTED, (unsigned int)LVIS_SELECTED);
		}
		else
		{
			m_ListCtrl.SetItemState(nItem, (unsigned int)~LVIS_SELECTED, (unsigned int)LVIS_SELECTED);
		}
	}

	m_ListCtrl.SetRedraw(TRUE);

	// Selected item has changed so update edit controls
	UpdateEditControls();
}


//------------------------------------------------------------------------------
// Purpose: Set the selected item in the listbox
// Input  : pConnection
//------------------------------------------------------------------------------
void COP_Output::SetSelectedConnection(CEntityConnection *pConnection)
{
	m_ListCtrl.SetRedraw(FALSE);

	// Set selected item to be active and all others to false
	int nItemCount = m_ListCtrl.GetItemCount();	
	for (int nItem = 0; nItem < nItemCount; nItem++)
	{
		COutputConnection *pOutputConn = (COutputConnection *)m_ListCtrl.GetItemData(nItem);
		CEntityConnectionList *pTestList = pOutputConn->m_pConnList;

		if (pTestList->Element(0) == pConnection)
		{
			m_ListCtrl.SetItemState(nItem,LVIS_SELECTED,LVIS_SELECTED);
		}
		else
		{
			m_ListCtrl.SetItemState(nItem, (unsigned int)~LVIS_SELECTED, (unsigned int)LVIS_SELECTED);
		}
	}

	m_ListCtrl.SetRedraw(TRUE);

	// Selected item has changed so update edit controls
	UpdateEditControls();
}


//-----------------------------------------------------------------------------
// Purpose: Selects the list box entries that correspond to the connections in
//			the given list.
//-----------------------------------------------------------------------------
void COP_Output::SetSelectedConnections(CEntityConnectionList &List)
{
	m_ListCtrl.SetRedraw(FALSE);

	int nConnCount = List.Count();

	int nItemCount = m_ListCtrl.GetItemCount();
	for (int nItem = 0; nItem < nItemCount; nItem++)
	{
		COutputConnection *pOutputConn = (COutputConnection *)m_ListCtrl.GetItemData(nItem);
		CEntityConnectionList *pConnList = pOutputConn->m_pConnList;

		// See if this row's list holds any of the connections in the given list.
		bool bFound = false;
		for (int nConn = 0; nConn < nConnCount; nConn++)
		{
			CEntityConnection *pConn = List.Element(nConn);
			if (pConnList->Find(pConn) != -1)
			{
				bFound = true;
				break;
			}
		}

		m_ListCtrl.SetItemState(nItem, bFound ? LVIS_SELECTED : ~LVIS_SELECTED, LVIS_SELECTED);
	}

	m_ListCtrl.SetRedraw(TRUE);

	UpdateEditControls();
}


//-----------------------------------------------------------------------------
// Purpose: Adds or removes the little 'V' or '^' sort indicator as appropriate.
// Input  : nColumn - Index of column to update.
//			bSortColumn - true if this column is the sort column, false if not.
//			eDirection - Direction of sort, Sort_Ascending or Sort_Descending.
//-----------------------------------------------------------------------------
void COP_Output::UpdateColumnHeaderText(int nColumn, bool bIsSortColumn, SortDirection_t eDirection)
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
void COP_Output::OnDestroy(void)
{
	m_ListCtrl.EnableWindow(false);
	RemoveAllEntityConnections();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void COP_Output::UpdateEditedFireOnce(void)
{
	// Get new delay
	CButton *pButton = ( CButton* )GetDlgItem( IDC_EDIT_CONN_FIRE_ONCE );

	if (pButton->IsWindowEnabled())
	{
		int nChecked = (pButton->GetState()&0x0003);  // Checked state

		// Update the connections
		int nConnCount = m_EditList.Count();
		for (int nConn = 0; nConn < nConnCount; nConn++)
		{
			CEntityConnection *pConnection = m_EditList.Element(nConn);
			if (pConnection != NULL)
			{
				pConnection->SetTimesToFire(nChecked?1:EVENT_FIRE_ALWAYS);
			}
		}

		// Update the list box
		for (int nItem = 0; nItem < m_ListCtrl.GetItemCount(); nItem++)
		{
			if (m_ListCtrl.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
			{
				m_ListCtrl.SetItemText(nItem, ONLY_ONCE_COLUMN, nChecked ? "Yes" : "No");
			}
		}
		ResizeColumns();
	}
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void COP_Output::UpdateEditedDelays(void)
{
	// Get new delay
	CEdit *pDelayEdit = ( CEdit* )GetDlgItem( IDC_EDIT_CONN_DELAY );

	if (pDelayEdit->IsWindowEnabled())
	{
		char strDelay[MAX_IO_NAME_LEN];
		pDelayEdit->GetWindowText(strDelay, sizeof(strDelay));
		float flDelay = atof(strDelay);

		// Update the connections
		int nConnCount = m_EditList.Count();
		for (int nConn = 0; nConn < nConnCount; nConn++)
		{
			CEntityConnection *pConnection = m_EditList.Element(nConn);
			if (pConnection != NULL)
			{
				pConnection->SetDelay(flDelay);
			}
		}

		// Update the list box
		for (int nItem = 0; nItem < m_ListCtrl.GetItemCount(); nItem++)
		{
			if (m_ListCtrl.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
			{
				m_ListCtrl.SetItemText(nItem, DELAY_COLUMN, strDelay);	
			}
		}
		ResizeColumns();
	}
}


//------------------------------------------------------------------------------
// Purpose: Parameters have changed.  Update connections and listbox
//------------------------------------------------------------------------------
void COP_Output::UpdateEditedParams(void)
{
	CComboBox *pParamEdit = ( CComboBox* )GetDlgItem( IDC_EDIT_CONN_PARAM );

	if (pParamEdit->IsWindowEnabled())
	{
		char strParam[MAX_IO_NAME_LEN];
		pParamEdit->GetWindowText(strParam, sizeof(strParam));
		if (!strcmp(strParam, PARAM_STRING_NONE))
		{
			strParam[0] = '\0';
		}

		// Update the connections
		int nConnCount = m_EditList.Count();
		for (int nConn = 0; nConn < nConnCount; nConn++)
		{
			CEntityConnection *pConnection = m_EditList.Element(nConn);
			if (pConnection != NULL)
			{
				pConnection->SetParam(strParam);
			}
		}

		// Update the list box
		for (int nItem = 0; nItem < m_ListCtrl.GetItemCount(); nItem++)
		{
			if (m_ListCtrl.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
			{
				m_ListCtrl.SetItemText(nItem, PARAMETER_COLUMN, strParam);	
			}
		}
		ResizeColumns();
	}
}


//------------------------------------------------------------------------------
// Purpose: Inputs have changed.  Update connections and listbox
//------------------------------------------------------------------------------
void COP_Output::UpdateEditedInputs(void)
{
	// Get the new name
	char strInput[MAX_IO_NAME_LEN];
	GetInput(strInput, sizeof(strInput));

	// Update the connections
	int nConnCount = m_EditList.Count();
	for (int nConn = 0; nConn < nConnCount; nConn++)
	{
		CEntityConnection *pConnection = m_EditList.Element(nConn);
		if (pConnection != NULL)
		{
			pConnection->SetInputName(strInput);
		}
	}

	// Update the list box
	for (int nItem = 0; nItem < m_ListCtrl.GetItemCount(); nItem++)
	{
		if (m_ListCtrl.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
		{
			m_ListCtrl.SetItemText(nItem, INPUT_NAME_COLUMN, strInput);	
			UpdateItemValidity(nItem);
		}
	}
	UpdateValidityButton();
	ResizeColumns();
}


//------------------------------------------------------------------------------
// Purpose: Outputs have changed.  Update connections and listbox
//------------------------------------------------------------------------------
void COP_Output::UpdateEditedOutputs()
{
	// Get the new name
	char strOutput[MAX_IO_NAME_LEN];
	GetOutput(strOutput, sizeof(strOutput));

	// Update the connections
	int nConnCount = m_EditList.Count();
	for (int nConn = 0; nConn < nConnCount; nConn++)
	{
		CEntityConnection *pConnection = m_EditList.Element(nConn);
		if (pConnection != NULL)
		{
			pConnection->SetOutputName(strOutput);
		}
	}

	// Update the list box
	for (int nItem = 0; nItem < m_ListCtrl.GetItemCount(); nItem++)
	{
		if (m_ListCtrl.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
		{
			m_ListCtrl.SetItemText(nItem, OUTPUT_NAME_COLUMN, strOutput);
			UpdateItemValidity(nItem);
		}
	}
	UpdateValidityButton();
	ResizeColumns();
}


//------------------------------------------------------------------------------
// Purpose: Targets have changed.  Update connections and listbox
//------------------------------------------------------------------------------
void COP_Output::UpdateEditedTargets(void)
{
	// Get the new target name
	char strTarget[MAX_IO_NAME_LEN];
	GetTarget(strTarget, sizeof(strTarget));

	// Update the connections
	int nConnCount = m_EditList.Count();
	for (int nConn = 0; nConn < nConnCount; nConn++)
	{
		CEntityConnection *pConnection = m_EditList.Element(nConn);
		if (pConnection != NULL)
		{
			pConnection->SetTargetName(strTarget);
		}
	}

	// Update the list box
	for (int nItem = 0; nItem < m_ListCtrl.GetItemCount(); nItem++)
	{
		if (m_ListCtrl.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
		{
			m_ListCtrl.SetItemText(nItem, TARGET_NAME_COLUMN, strTarget);	
			UpdateItemValidity(nItem);
		}
	}
	UpdateValidityButton();
	ResizeColumns();
}


//-----------------------------------------------------------------------------
// Purpose: Enables or diables the target combo box and the eyedropper button.
//-----------------------------------------------------------------------------
void COP_Output::EnableTarget(bool bEnable)
{
	m_ComboTarget.EnableWindow(bEnable);
	GetDlgItem(IDC_PICK_ENTITY)->EnableWindow(bEnable);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pConnection - 
//-----------------------------------------------------------------------------
void COP_Output::SetConnection(CEntityConnectionList *pConnectionList)
{
	Assert(pConnectionList != NULL);
	
	// Fill edit boxes.  Disable for multiple connections have incompatible data
	bool		bFirst		= true;
	CButton*	pFireEdit	= ( CButton* )GetDlgItem( IDC_EDIT_CONN_FIRE_ONCE );
	CEdit*		pDelayEdit	= ( CEdit* )GetDlgItem( IDC_EDIT_CONN_DELAY );
	CComboBox*	pParamEdit	= ( CComboBox* )GetDlgItem( IDC_EDIT_CONN_PARAM );

	m_ComboOutput.EnableWindow(true);
	EnableTarget(true);
	m_ComboInput.EnableWindow(true);
	pFireEdit->EnableWindow(true);
	pDelayEdit->EnableWindow(true);
	pParamEdit->EnableWindow(true);  
	GetDlgItem(IDC_PICK_ENTITY_PARAM)->EnableWindow( false );
	m_bEntityParamTarget = false;

	int nConnCount = pConnectionList->Count();
	for (int nConn = 0; nConn < nConnCount; nConn++)
	{
		CEntityConnection *pConnection = (CEntityConnection *)pConnectionList->Element(nConn);
		if (pConnection == NULL)
			continue;

		// Fill in output name, disable for non-compatible connections
		if (m_ComboOutput.IsWindowEnabled())
		{
			if (bFirst)
			{
				m_strOutput = pConnection->GetOutputName();
			}
			else if (m_strOutput != pConnection->GetOutputName())
			{
				m_strOutput.Empty();
				m_ComboOutput.EnableWindow(false);
			}
		}

		// Fill in target name, disable for non-compatible connections
		if (m_ComboTarget.IsWindowEnabled())
		{
			if (bFirst)
			{
				m_strTarget = pConnection->GetTargetName();
			}
			else if (m_strTarget != pConnection->GetTargetName())
			{
				m_strTarget.Empty();
				EnableTarget(false);
			}
		}

		// Fill in input name, disable for non-compatible connections
		if (m_ComboInput.IsWindowEnabled())
		{
			if (bFirst)
			{
				m_strInput = pConnection->GetInputName();
			}
			else if (m_strInput != pConnection->GetInputName())
			{
				m_strInput.Empty();
				m_ComboInput.EnableWindow(false);
			}
		}

		// Fill in parameters, disable for non-compatible connections
		if (pParamEdit->IsWindowEnabled())
		{
			if (bFirst)
			{
				m_strParam = pConnection->GetParam();
				m_bNoParamEdit = false;
			}
			else if (m_strParam != pConnection->GetParam())
			{
				m_strParam.Empty();
				pParamEdit->EnableWindow(false);
				GetDlgItem(IDC_PICK_ENTITY_PARAM)->EnableWindow( false );
				m_bNoParamEdit = true;
			}
		}

		// Fill in delay, disable for non-compatible connections
		if (pDelayEdit->IsWindowEnabled())
		{
			if (bFirst)
			{
				m_fDelay = pConnection->GetDelay();
			}
			else if (m_fDelay != pConnection->GetDelay())
			{
				m_fDelay = 0;
				pDelayEdit->EnableWindow(false);
			}
		}

		// Set fire once flag, disable for non-compatible connections
		if (pFireEdit->IsWindowEnabled())
		{
			if (bFirst)
			{
				m_bFireOnce = (pConnection->GetTimesToFire() == -1) ? false : true;
			}
			else if (m_bFireOnce != pConnection->GetTimesToFire())
			{
				m_bFireOnce = false;
				pFireEdit->EnableWindow(false);
			}
		}

		bFirst = false;
	}

	// Put a <none> in param box if no param
	if (strlen(m_strParam) == 0)
	{
		m_strParam = PARAM_STRING_NONE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds all of an entity's outputs from its class definition to the
//			outputs combo box.
// Input  : pEntity - Entity whose outputs are to be added to the combo box.
//-----------------------------------------------------------------------------
void COP_Output::AddEntityOutputs(CMapEntity *pEntity)
{
	GDclass *pClass = pEntity->GetClass();
	if (pClass != NULL)
	{
		int nCount = pClass->GetOutputCount();
		for (int i = 0; i < nCount; i++)
		{
			CClassOutput *pOutput = pClass->GetOutput(i);
			int nIndex = m_ComboOutput.AddString(pOutput->GetName());
			if (nIndex >= 0)
			{
				m_ComboOutput.SetItemDataPtr(nIndex, pOutput);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Output::FillInputList(void)
{
	if (!m_pMapEntityList)
	{
		return;
	}

	//
	// Add all entity inputs to the inputs combo box.
	//
	m_ComboInput.SetRedraw(FALSE);
	m_ComboInput.ResetContent();

	// CUtlVector<GDclass*> classCache;
	CUtlRBTree<int,int> classCache;
	SetDefLessFunc( classCache );
	
	FOR_EACH_OBJ( *m_pMapEntityList, pos )
	{
		CMapEntity *pEntity = m_pMapEntityList->Element(pos);
		Assert(pEntity != NULL);

		if (pEntity == NULL)
			continue;
		
		//
		// Get the entity's class, which contains the list of inputs that this entity exposes.
		//
		GDclass *pClass = pEntity->GetClass();

        if (pClass == NULL)
			continue;

		// check if class was already added
		if ( classCache.Find( (int)pClass ) != -1 )
			continue;

		classCache.Insert( (int)pClass );
			
		//
		// Add this class' inputs to the list.
		//
		int nCount = pClass->GetInputCount();
		for (int i = 0; i < nCount; i++)
		{
			CClassInput *pInput = pClass->GetInput(i);
			bool bAddInput = true;

			//
			// Don't add the input to the combo box if another input with the same name
			// and type is already there.
			//
			int nIndex = m_ComboInput.FindStringExact(-1, pInput->GetName());
			if (nIndex != CB_ERR)
			{
				CClassInput *pExistingInput = (CClassInput *)m_ComboInput.GetItemDataPtr(nIndex);
				if (pExistingInput->GetType() == pInput->GetType())
				{
					bAddInput = false;
				}
			}

			if (bAddInput)
			{
				nIndex = m_ComboInput.AddString(pInput->GetName());
				if (nIndex >= 0)
				{
					m_ComboInput.SetItemDataPtr(nIndex, pInput);
				}
			}
		}
	}

	m_ComboInput.SetRedraw(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Fills the list of outputs with outputs common to all the selected entities.
//-----------------------------------------------------------------------------
void COP_Output::FillOutputList(void)
{
	if ( m_EntityList.Count() == 0 )
	{
		return;
	}

	//
	// Determine what the currently selected output is (if any).
	//
	CClassOutput *pSelectedOutput;
	int nOutput = m_ComboOutput.GetCurSel();
	if (nOutput != CB_ERR)
	{
		pSelectedOutput = (CClassOutput *)m_ComboOutput.GetItemDataPtr(nOutput);
	}
	else
	{
		pSelectedOutput = NULL;
	}

	//
	// Add the entity outputs to the outputs combo box.
	//
	m_ComboOutput.SetRedraw(FALSE);
	m_ComboOutput.ResetContent();

	bool bFirst = true;
	
	FOR_EACH_OBJ( m_EntityList, pos )
	{
		CMapEntity *pEntity = m_EntityList.Element(pos);

		if (bFirst)
		{
			//
			// The first entity adds its outputs to the list.
			//
			AddEntityOutputs(pEntity);
			bFirst = false;
		}
		else
		{
			//
			// All subsequent entities filter the output list.
			//
			FilterEntityOutputs(pEntity);	
		}
	}

	if (m_ComboOutput.GetCount() == 0)
	{
		m_ComboOutput.EnableWindow(false);
	}

	m_ComboOutput.SetRedraw(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Fills the list of targets with entities that have "targetname" keys.
//-----------------------------------------------------------------------------
void COP_Output::FillTargetList(void)
{
	m_bIgnoreTextChanged = true;
	m_ComboTarget.SetEntityList(m_pMapEntityList);
	m_bIgnoreTextChanged = false;
}


//-----------------------------------------------------------------------------
// Purpose: Removes all outputs from the outputs combo box that are NOT present
//			in the given entity's output list. Used when multiple entities are
//			selected into the Entity Properties dialog.
// Input  : pEntity - Entity to use for filter.
//-----------------------------------------------------------------------------
void COP_Output::FilterEntityOutputs(CMapEntity *pEntity)
{
	//
	// Make sure that this entity has a valid class to use for filtering.
	//
	GDclass *pClass = pEntity->GetClass();
	if (pClass == NULL)
	{
		return;
	}

	//
	// Remove any outputs from the combo box that are not in the class.
	//
	char szText[MAX_PATH];

	int nCount = m_ComboOutput.GetCount();
	if (nCount > 0)
	{
		for (int i = nCount - 1; i >= 0; i--)
		{
			if (m_ComboOutput.GetLBText(i, szText) != CB_ERR)
			{
				if (pClass->FindOutput(szText) == NULL)
				{
					m_ComboOutput.DeleteString(i);
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Output::FilterOutputList(void)
{
	// dvs: Possibly unnecessary. For example, if they choose an input, then
	// choose an incompatible output, the input will become red to indicate
	// the incompatibilty. So maybe the outputs can always contain the set of
	// all outputs common to the selected entities.
}


//-----------------------------------------------------------------------------
// Purpose: Filters the list of inputs based on the current selected target.
//-----------------------------------------------------------------------------
void COP_Output::FilterInputList(void)
{
	char szTarget[MAX_ENTITY_NAME_LEN];
	CMapEntityList *pTargets = GetTarget(szTarget, sizeof(szTarget));

	if (pTargets != NULL)
	{
		//
		// Remove all items from the inputs combo that:
		//
		// 1) Are not compatible with the currently selected output, OR
		// 2) Are not found in the currently selected targets list.
		//
		int nCount = m_ComboInput.GetCount();
		if (nCount > 0)
		{
			for (int i = nCount - 1; i >= 0; i--)
			{
				CClassInput *pInput = (CClassInput *)m_ComboInput.GetItemDataPtr(i);
				if (!MapEntityList_HasInput(pTargets, pInput->GetName(), pInput->GetType()))
				{
					m_ComboInput.DeleteString(i);
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Output::FilterTargetList(void)
{
#if 0 // Not used...
	char szInput[MAX_IO_NAME_LEN];
	CClassInput *pInput = GetInput(szInput, sizeof(szInput));

	//
	// Remove all items from the targets combo that:
	//
	// 1) Do not have the selected input name OR
	// 2) Do not have inputs that are compatible with the selected output.
	//
	int nCount = m_ComboTarget.GetCount();
	if (nCount > 0)
	{
		for (int i = nCount - 1; i >= 0; i--)
		{
			CMapEntityList *pTargets = (CMapEntityList *)m_ComboTarget.GetItemDataPtr(i);
						
			if (!MapEntityList_HasInput(pTargets, pInput->GetName(), pInput->GetType()))
			{
				m_ComboTarget.DeleteString(i);
			}
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Returns the currently selected input, NULL if unknown.
// Input  : szInput - Receives the text in the Input combo edit control.
//			nSize - Size of buffer pointed to by szInput.
//-----------------------------------------------------------------------------
CClassInput *COP_Output::GetInput(char *szInput, int nSize)
{
	szInput[0] = '\0';

	int nCurSel = m_ComboInput.GetCurSel();
	if (nCurSel == CB_ERR)
	{
		if (m_ComboInput.GetWindowText(szInput, nSize) > 0)
		{
			nCurSel = m_ComboInput.FindStringExact(-1, szInput);
		}
	}

	CClassInput *pInput = NULL;
	if (nCurSel != CB_ERR)
	{
		m_ComboInput.GetLBText(nCurSel, szInput);
		pInput = (CClassInput *)m_ComboInput.GetItemDataPtr(nCurSel);
	}

	return(pInput);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the currently selected output, NULL if unknown.
// Input  : szOutput - Receives the text in the Output combo edit control.
//			nSize - Size of buffer pointed to by szOutput.
//-----------------------------------------------------------------------------
CClassOutput *COP_Output::GetOutput(char *szOutput, int nSize)
{
	szOutput[0] = '\0';

	int nCurSel = m_ComboOutput.GetCurSel();
	if (nCurSel == CB_ERR)
	{
		if (m_ComboOutput.GetWindowText(szOutput, nSize) > 0)
		{
			nCurSel = m_ComboOutput.FindStringExact(-1, szOutput);
		}
	}

	CClassOutput *pOutput = NULL;
	if (nCurSel != CB_ERR)
	{
		m_ComboOutput.GetLBText(nCurSel, szOutput);
		pOutput = (CClassOutput *)m_ComboOutput.GetItemDataPtr(nCurSel);
	}

	return(pOutput);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the currently selected target list, NULL if unknown.
// Input  : szTarget - Receives the text in the Target combo edit control.
//			nSize - Size of buffer pointed to by szTarget.
//-----------------------------------------------------------------------------
CMapEntityList *COP_Output::GetTarget(char *szTarget, int nSize)
{
	szTarget[0] = '\0';

	CString str = m_ComboTarget.GetCurrentItem();
	Q_strncpy( szTarget, str, nSize );

	return m_ComboTarget.GetSubEntityList( szTarget );
}


//-----------------------------------------------------------------------------
// Purpose: Called when the contents of the delay edit box change.
//-----------------------------------------------------------------------------
void COP_Output::OnEditDelay(void)
{
	UpdateEditedDelays();
}


//-----------------------------------------------------------------------------
// Purpose: Called when the contents of the target combo edit box change.
//-----------------------------------------------------------------------------
void COP_Output::OnFireOnce(void)
{
	UpdateEditedFireOnce();
}


//-----------------------------------------------------------------------------
// Purpose: Called when they change the "Show Hidden Targets" checkbox.
//-----------------------------------------------------------------------------
void COP_Output::OnShowHiddenTargetsAsBroken()
{
	// Remember the last state of this checkbox.
	Options.general.bShowHiddenTargetsAsBroken = (m_ctlShowHiddenTargetsAsBroken.GetCheck() != FALSE);
	
	// Refresh.
	int nCount = m_ListCtrl.GetItemCount();
	for ( int i=0; i < nCount; i++ )
	{
		UpdateItemValidity( i );
	}
	//UpdateConnectionList();
}


//-----------------------------------------------------------------------------
// Purpose: React to the input combo box being changed
//-----------------------------------------------------------------------------
void COP_Output::InputChanged(void)
{
	// Updating the listbox data, will trigger the edit
	// controls to update.  They don't need to be
	bSkipEditControlRefresh = true;

	char szInput[MAX_IO_NAME_LEN];
	CClassInput *pInput = GetInput(szInput, sizeof(szInput));
	UpdateCombosForSelectedInput(pInput);
	UpdateEditedInputs();
}


//-----------------------------------------------------------------------------
// Purpose: Called when selection of input combo box chages
//-----------------------------------------------------------------------------
void COP_Output::OnSelChangeInput(void)
{
	InputChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Called when the contents of the input combo edit box change.
//-----------------------------------------------------------------------------
void COP_Output::OnEditUpdateInput(void)
{
	InputChanged();
}


//------------------------------------------------------------------------------
// Purpose: React to the output combo box being changed
//------------------------------------------------------------------------------
void COP_Output::OutputChanged(void)
{
	// Updating the listbox data, will trigger the edit
	// controls to update.  They don't need to be
	bSkipEditControlRefresh = true;

	char szOutput[MAX_IO_NAME_LEN];
	CClassOutput *pOutput = GetOutput(szOutput, sizeof(szOutput));
	UpdateCombosForSelectedOutput(pOutput);
	UpdateEditedOutputs();
}


//-----------------------------------------------------------------------------
// Purpose: Called when selection of output combo box chages
//-----------------------------------------------------------------------------
void COP_Output::OnSelChangeOutput(void)
{
	OutputChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Called when the contents of the output combo edit box change.
//-----------------------------------------------------------------------------
void COP_Output::OnEditUpdateOutput(void)
{
	OutputChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Called when selection of parameter combo box chages 
//-----------------------------------------------------------------------------
void COP_Output::OnSelChangeParam(void)
{
	// If user picked <none> selection (the only valid one) clear window text
	CComboBox *pParamEdit = ( CComboBox* )GetDlgItem( IDC_EDIT_CONN_PARAM );
	if (pParamEdit->GetCurSel() != CB_ERR)
	{
		pParamEdit->SetWindowText("");
	}

	UpdateEditedParams();
}


//-----------------------------------------------------------------------------
// Purpose: Called when the contents of the parameter combo edit box change.
//-----------------------------------------------------------------------------
void COP_Output::OnEditUpdateParam(void)
{
	UpdateEditedParams();
}


//-----------------------------------------------------------------------------
// Purpose: Updates the dialog based on the currently selected input.
// Input  : pInput - Pointer to the input that is selected, NULL if none or
//			ambiguous/unresolved.
//-----------------------------------------------------------------------------
void COP_Output::UpdateCombosForSelectedInput(CClassInput *pInput)
{
	// Enable / Disable param box based on input type if allowed
	if (!m_bNoParamEdit)
	{
		CComboBox *pParamCombo = (CComboBox *)GetDlgItem(IDC_EDIT_CONN_PARAM);
		bool bEnable = ((!pInput) || (pInput && (pInput->GetType() != iotVoid)));
		if (!bEnable)
		{
			// Save the param so we can restore it if they switch right back.
			CString strTemp;
			pParamCombo->GetWindowText(strTemp);
			if (strTemp.Compare(PARAM_STRING_NONE))
			{
				m_strLastParam = strTemp;
			}

			// Switch back to <none> if we're disabling the parameter combo.
			pParamCombo->SetCurSel(0);
		}
		else if (!m_strLastParam.IsEmpty())
		{
			pParamCombo->SetWindowText(m_strLastParam);
		}

		UpdateEditedParams();
		pParamCombo->EnableWindow(bEnable);
		m_bEntityParamTarget = pInput && (pInput->GetType() == iotEHandle);
		GetDlgItem(IDC_PICK_ENTITY_PARAM)->EnableWindow( m_bEntityParamTarget );
	}

	if (pInput != NULL)
	{
		//
		// Known input, render it in black.
		//
		m_ComboInput.SetTextColor(RGB(0, 0, 0));
	}
	else
	{
		//
		// Unknown input, render it in red.
		//
		m_ComboInput.SetTextColor(RGB(255, 0, 0));
	}
	m_ComboInput.RedrawWindow();
}


//-----------------------------------------------------------------------------
// Purpose: Updates the dialog based on the currently selected output.
// Input  : pOutput - Pointer to the output that is selected, NULL if none or
//			ambiguous/unresolved.
//-----------------------------------------------------------------------------
void COP_Output::UpdateCombosForSelectedOutput(CClassOutput *pOutput)
{
	if (pOutput != NULL)
	{
		//
		// Known output, render it in black.
		//
		m_ComboOutput.SetTextColor(RGB(0, 0, 0));
	}
	else
	{
		//
		// Unknown output, render it in red.
		//
		m_ComboOutput.SetTextColor(RGB(255, 0, 0));
	}
	m_ComboOutput.RedrawWindow();
}


//-----------------------------------------------------------------------------
// Purpose: Stops entity picking.
//-----------------------------------------------------------------------------
void COP_Output::StopPicking(void)
{
	if (m_bPickingEntities)
	{
		m_bPickingEntities = false;
		ToolManager()->SetTool(TOOL_POINTER);

		CButton *pButton = (CButton *)GetDlgItem(IDC_PICK_ENTITY);
		if (pButton)
		{
			pButton->SetCheck(0);
		}

		pButton = (CButton *)GetDlgItem(IDC_PICK_ENTITY_PARAM);
		if (pButton)
		{
			pButton->SetCheck(0);
		}

		if ( m_ComboTarget.IsWindowEnabled() )
		{
			GetDlgItem(IDC_PICK_ENTITY)->EnableWindow( true );
		}

		CComboBox* pParamEdit = ( CComboBox* )GetDlgItem( IDC_EDIT_CONN_PARAM );
		if ( pParamEdit->IsWindowEnabled() )
		{
			GetDlgItem(IDC_PICK_ENTITY_PARAM)->EnableWindow( m_bEntityParamTarget );
		}
	}
}

void COP_Output::OnSize( UINT nType, int cx, int cy )
{
	m_AnchorMgr.OnSize();
}
