//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Singleton dialog that generates and presents the entity report.
//
//===========================================================================//

#include "EntityReportPanel.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "iregistry.h"
#include "vgui/ivgui.h"
#include "vgui_controls/listpanel.h"
#include "vgui_controls/textentry.h"
#include "vgui_controls/checkbutton.h"
#include "vgui_controls/combobox.h"
#include "vgui_controls/radiobutton.h"
#include "vgui_controls/messagebox.h"
#include "commeditdoc.h"
#include "commedittool.h"
#include "datamodel/dmelement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

	
//-----------------------------------------------------------------------------
// Sort by target name
//-----------------------------------------------------------------------------
static int __cdecl TargetNameSortFunc( vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("targetname");
	const char *string2 = item2.kv->GetString("targetname");
	int nRetVal = Q_stricmp( string1, string2 );
	if ( nRetVal != 0 )
		return nRetVal;

	string1 = item1.kv->GetString("classname");
	string2 = item2.kv->GetString("classname");
	return Q_stricmp( string1, string2 );
}

//-----------------------------------------------------------------------------
// Sort by class name
//-----------------------------------------------------------------------------
static int __cdecl ClassNameSortFunc( vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("classname");
	const char *string2 = item2.kv->GetString("classname");
	int nRetVal = Q_stricmp( string1, string2 );
	if ( nRetVal != 0 )
		return nRetVal;

	string1 = item1.kv->GetString("targetname");
	string2 = item2.kv->GetString("targetname");
	return Q_stricmp( string1, string2 );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CEntityReportPanel::CEntityReportPanel( CCommEditDoc *pDoc, vgui::Panel* pParent, const char *pName )
	: BaseClass( pParent, pName ), m_pDoc( pDoc )
{
	m_bSuppressEntityListUpdate = false;
	m_iFilterByType = FILTER_SHOW_EVERYTHING;
	m_bFilterByKeyvalue = false;
	m_bFilterByClass = false;
	m_bFilterByHidden = false;
	m_bFilterByKeyvalue = false;
	m_bExact = false;
	m_bFilterTextChanged = false;

	SetPaintBackgroundEnabled( true );

	m_pEntities = new vgui::ListPanel( this, "Entities" );
	m_pEntities->AddColumnHeader( 0, "targetname", "Name", 52, ListPanel::COLUMN_RESIZEWITHWINDOW );
 	m_pEntities->AddColumnHeader( 1, "classname", "Class Name", 52, ListPanel::COLUMN_RESIZEWITHWINDOW );
	m_pEntities->SetColumnSortable( 0, true );
	m_pEntities->SetColumnSortable( 1, true );
	m_pEntities->SetEmptyListText( "No Entities" );
 //	m_pEntities->SetDragEnabled( true );
 	m_pEntities->AddActionSignalTarget( this );
	m_pEntities->SetSortFunc( 0, TargetNameSortFunc );
	m_pEntities->SetSortFunc( 1, ClassNameSortFunc );
	m_pEntities->SetSortColumn( 0 );

	// Filtering checkboxes
	m_pFilterByClass = new vgui::CheckButton( this, "ClassnameCheck", "" );
	m_pFilterByClass->AddActionSignalTarget( this );
	m_pFilterByKeyvalue = new vgui::CheckButton( this, "KeyvalueCheck", "" );
	m_pFilterByKeyvalue->AddActionSignalTarget( this );
	m_pFilterByHidden = new vgui::CheckButton( this, "HiddenCheck", "" );
	m_pFilterByHidden->AddActionSignalTarget( this );
	m_pExact = new vgui::CheckButton( this, "ExactCheck", "" );
	m_pExact->AddActionSignalTarget( this );

	// Filtering text entries
	m_pFilterKey = new vgui::TextEntry( this, "KeyTextEntry" ); 
	m_pFilterValue = new vgui::TextEntry( this, "ValueTextEntry" );

	// Classname combobox
	m_pFilterClass = new vgui::ComboBox( this, "ClassNameComboBox", 16, true );

	// Filter by type radio buttons
	m_pFilterEverything = new vgui::RadioButton( this, "EverythingRadio", "" );
	m_pFilterPointEntities = new vgui::RadioButton( this, "PointRadio", "" );
	m_pFilterBrushModels = new vgui::RadioButton( this, "BrushRadio", "" );

	LoadControlSettings( "resource/entityreportpanel.res" );

	ReadSettingsFromRegistry();

	// Used for updating filter while changing text
	ivgui()->AddTickSignal( GetVPanel(), 300 );
}


//-----------------------------------------------------------------------------
// Reads settings from registry
//-----------------------------------------------------------------------------
void CEntityReportPanel::ReadSettingsFromRegistry()
{
	m_bSuppressEntityListUpdate = true;

	const char *pKeyBase = g_pCommEditTool->GetRegistryName();
	m_pFilterByKeyvalue->SetSelected( registry->ReadInt(pKeyBase, "FilterByKeyvalue", 0) );
	m_pFilterByClass->SetSelected( registry->ReadInt(pKeyBase, "FilterByClass", 0) );
	m_pFilterByHidden->SetSelected( registry->ReadInt(pKeyBase, "FilterByHidden", 1) );
	m_pExact->SetSelected( registry->ReadInt(pKeyBase, "Exact", 0) );

	m_iFilterByType = (FilterType_t)registry->ReadInt(pKeyBase, "FilterByType", FILTER_SHOW_EVERYTHING);
	m_pFilterEverything->SetSelected( m_iFilterByType == FILTER_SHOW_EVERYTHING );
	m_pFilterPointEntities->SetSelected( m_iFilterByType == FILTER_SHOW_POINT_ENTITIES );
	m_pFilterBrushModels->SetSelected( m_iFilterByType == FILTER_SHOW_BRUSH_ENTITIES );

	// Gotta call change functions manually since SetText doesn't post an action signal
	const char *pValue = registry->ReadString( pKeyBase, "FilterClass", "" );
	m_pFilterClass->SetText( pValue );
	OnChangeFilterclass( pValue );

	pValue = registry->ReadString( pKeyBase, "FilterKey", "" );
	m_pFilterKey->SetText( pValue );
	OnChangeFilterkey( pValue );

	pValue = registry->ReadString( pKeyBase, "FilterValue", "" );
	m_pFilterValue->SetText( pValue );
	OnChangeFiltervalue( pValue );

	m_bSuppressEntityListUpdate = false;
	UpdateEntityList();
}


//-----------------------------------------------------------------------------
// Writes settings to registry
//-----------------------------------------------------------------------------
void CEntityReportPanel::SaveSettingsToRegistry()
{
	const char *pKeyBase = g_pCommEditTool->GetRegistryName();
	registry->WriteInt(pKeyBase, "FilterByKeyvalue", m_bFilterByKeyvalue);
	registry->WriteInt(pKeyBase, "FilterByClass", m_bFilterByClass);
	registry->WriteInt(pKeyBase, "FilterByHidden", m_bFilterByHidden);
	registry->WriteInt(pKeyBase, "FilterByType", m_iFilterByType);
	registry->WriteInt(pKeyBase, "Exact", m_bExact);

	registry->WriteString(pKeyBase, "FilterClass", m_szFilterClass);
	registry->WriteString(pKeyBase, "FilterKey", m_szFilterKey);
	registry->WriteString(pKeyBase, "FilterValue", m_szFilterValue);
}


//-----------------------------------------------------------------------------
// Purpose: Shows the most recent selected object in properties window
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnProperties(void)
{
	int iSel = m_pEntities->GetSelectedItem( 0 );
	KeyValues *kv = m_pEntities->GetItem( iSel );
	CDmElement *pEntity = (CDmElement *)kv->GetPtr( "entity" );
	g_pCommEditTool->ShowEntityInEntityProperties( pEntity );
}


//-----------------------------------------------------------------------------
// Purpose: Deletes the marked objects.
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnDeleteEntities(void)
{		
	// This is undoable
	CUndoScopeGuard guard( g_pDataModel, "Delete Entities", "Delete Entities" );

	int iSel = m_pEntities->GetSelectedItem( 0 );

	//
	// Build a list of objects to delete.
	//
	int nCount = m_pEntities->GetSelectedItemsCount();
	for (int i = 0; i < nCount; i++)
	{
		int nItemID = m_pEntities->GetSelectedItem(i);
		KeyValues *kv = m_pEntities->GetItem( nItemID );
		CDmElement *pEntity = (CDmElement *)kv->GetPtr( "entity" );
		if ( pEntity )
		{
			m_pDoc->DeleteCommentaryNode( pEntity );
		}
	}

	UpdateEntityList();

	// Update the list box selection.
	if (iSel >= m_pEntities->GetItemCount())
	{
		iSel = m_pEntities->GetItemCount() - 1;
	}
	m_pEntities->SetSingleSelectedItem( iSel );
}


//-----------------------------------------------------------------------------
// Called when buttons are clicked
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "delete" ) )
	{
		// Confirm we want to do it
		MessageBox *pConfirm = new MessageBox( "#CommEditDeleteObjects", "#CommEditDeleteObjectsMsg", g_pCommEditTool->GetRootPanel() ); 
		pConfirm->AddActionSignalTarget( this );
		pConfirm->SetOKButtonText( "Yes" );
		pConfirm->SetCommand( new KeyValues( "DeleteEntities" ) );
		pConfirm->SetCancelButtonVisible( true );
		pConfirm->SetCancelButtonText( "No" );
		pConfirm->DoModal();
		return;
	}

	if ( !Q_stricmp( pCommand, "ShowProperties" ) )
	{
		OnProperties();
		return;
	}
}

	
//-----------------------------------------------------------------------------
// Call this when our settings are dirty
//-----------------------------------------------------------------------------
void CEntityReportPanel::MarkDirty( bool bFilterDirty )
{
	float flTime = Plat_FloatTime();
	m_bRegistrySettingsChanged = true;
	m_flRegistryTime = flTime;
	if ( bFilterDirty && !m_bFilterTextChanged )
	{
		m_bFilterTextChanged = true;
		m_flFilterTime = flTime;
	}
}

	
//-----------------------------------------------------------------------------
// Methods related to filtering
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnFilterByHidden( bool bState ) 
{
	m_bFilterByHidden = bState;
	UpdateEntityList();
	MarkDirty( false );
}

void CEntityReportPanel::OnFilterByKeyvalue( bool bState ) 
{
	m_bFilterByKeyvalue = bState;
	UpdateEntityList();
	MarkDirty( false );

	m_pFilterKey->SetEnabled( bState );
	m_pFilterValue->SetEnabled( bState );
	m_pExact->SetEnabled( bState );
}

void CEntityReportPanel::OnFilterKeyValueExact( bool bState )
{
	m_bExact = bState;
	UpdateEntityList();
	MarkDirty( false );
}

void CEntityReportPanel::OnFilterByType( FilterType_t type )
{
	m_iFilterByType = type;
	UpdateEntityList();
	MarkDirty( false );
}

void CEntityReportPanel::OnFilterByClass( bool bState ) 
{
	m_bFilterByClass = bState;
	UpdateEntityList();
	MarkDirty( false );

	m_pFilterClass->SetEnabled( bState );
}

void CEntityReportPanel::OnChangeFilterkey( const char *pText ) 
{
	m_szFilterKey = pText;
	MarkDirty( true );
}

void CEntityReportPanel::OnChangeFiltervalue( const char *pText ) 
{
	m_szFilterValue = pText;
	MarkDirty( true );
}

void CEntityReportPanel::OnChangeFilterclass( const char *pText ) 
{
	m_szFilterClass = pText;
	MarkDirty( true );
}


//-----------------------------------------------------------------------------
// Deals with all check buttons
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnTextChanged( KeyValues *kv )
{
	TextEntry *pPanel = (TextEntry*)kv->GetPtr( "panel", NULL );

	int nLength = pPanel->GetTextLength();
	char *pBuf = (char*)_alloca( nLength + 1 );
	pPanel->GetText( pBuf, nLength+1 );

	if ( pPanel == m_pFilterClass )
	{
		OnChangeFilterclass( pBuf );
		return;
	}
	if ( pPanel == m_pFilterKey )
	{
		OnChangeFilterkey( pBuf );
		return;
	}
	if ( pPanel == m_pFilterValue )
	{
		OnChangeFiltervalue( pBuf );
		return;
	}
}


//-----------------------------------------------------------------------------
// Deals with all check buttons
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnButtonToggled( KeyValues *kv )
{
	Panel *pPanel = (Panel*)kv->GetPtr( "panel", NULL );
	bool bState = kv->GetInt( "state", 0 ) != 0;

	if ( pPanel == m_pFilterByClass )
	{
		OnFilterByClass( bState );
		return;
	}
	if ( pPanel == m_pFilterByKeyvalue )
	{
		OnFilterByKeyvalue( bState );
		return;
	}
	if ( pPanel == m_pFilterByHidden )
	{
		OnFilterByHidden( bState );
		return;
	}
	if ( pPanel == m_pExact )
	{
		OnFilterKeyValueExact( bState );
		return;
	}
	if ( pPanel == m_pFilterEverything )
	{
		OnFilterByType( FILTER_SHOW_EVERYTHING );
		return;
	}
	if ( pPanel == m_pFilterPointEntities )
	{
		OnFilterByType( FILTER_SHOW_POINT_ENTITIES );
		return;
	}
	if ( pPanel == m_pFilterBrushModels )
	{
		OnFilterByType( FILTER_SHOW_BRUSH_ENTITIES );
		return;
	}
}


//-----------------------------------------------------------------------------
// FIXME: Necessary because SetSelected doesn't cause a ButtonToggled message to trigger
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnCheckButtonChecked( KeyValues *kv )
{
	OnButtonToggled( kv );
}

void CEntityReportPanel::OnRadioButtonChecked( KeyValues *kv )
{
	OnButtonToggled( kv );
}


#if 0

//-----------------------------------------------------------------------------
// Purpose: Centers the 2D and 3D views on the selected entities.
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnGoto() 
{
	MarkSelectedEntities();
	m_pDoc->CenterViewsOnSelection();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportPanel::MarkSelectedEntities() 
{
	m_pDoc->SelectObject(NULL, CMapDoc::scClear);

	for(int i = 0; i < m_cEntities.GetCount(); i++)
	{
		if(!m_cEntities.GetSel(i))
			continue;
		CMapEntity *pEntity = (CMapEntity*) m_cEntities.GetItemDataPtr(i);
		m_pDoc->SelectObject(pEntity, CMapDoc::scSelect);
	}

	m_pDoc->SelectObject(NULL, CMapDoc::scUpdateDisplay);
}

#endif

void CEntityReportPanel::OnTick( ) 
{
	BaseClass::OnTick();

	// check filters
	float flTime = Plat_FloatTime();
	if ( m_bFilterTextChanged )
	{
		if ( (flTime - m_flFilterTime) > 1e-3 )
		{
			m_bFilterTextChanged = false;
			m_flFilterTime = flTime;
			UpdateEntityList();
		}
	}
	if ( m_bRegistrySettingsChanged )
	{
		if ( (flTime - m_flRegistryTime) > 1e-3 )
		{
			m_bRegistrySettingsChanged = false;
			m_flRegistryTime = flTime;
			SaveSettingsToRegistry();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportPanel::UpdateEntityList(void)
{
	if ( m_bSuppressEntityListUpdate )
		return;

	m_bFilterTextChanged = false;

	m_pEntities->RemoveAll();

	CDmAttribute *pEntityList = m_pDoc->GetEntityList();
	int nCount = pEntityList->Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pEntity = GetElement< CDmElement >( pEntityList->Get(i) );

		const char *pClassName = pEntity->GetAttributeValueString( "classname" );
		if ( !pClassName || !pClassName[0] )
		{
			pClassName = "<no class>";
		}

		KeyValues *kv = new KeyValues( "node" );
		kv->SetString( "classname", pClassName ); 
		kv->SetPtr( "entity", pEntity );

		m_pEntities->AddItem( kv, 0, false, false );
	}
	m_pEntities->SortList();
}


#if 0
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportPanel::GenerateReport()
{
	POSITION p = pGD->Classes.GetHeadPosition();
	CString str;
	while(p)
	{
		GDclass *pc = pGD->Classes.GetNext(p);
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


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnSelChangeEntityList()
{
	MarkSelectedEntities();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnDblClkEntityList()
{
	m_pDoc->CenterViewsOnSelection();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnOK()
{
	DestroyWindow();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnClose()
{
	DestroyWindow();
}


//-----------------------------------------------------------------------------
// Purpose: Called when our window is being destroyed.
//-----------------------------------------------------------------------------
void CEntityReportPanel::OnDestroy()
{
	SaveToIni();
	s_pDlg = NULL;
	delete this;
}
#endif