//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Singleton dialog that generates and presents the entity report.
//
//===========================================================================//

#include "InfoTargetBrowserPanel.h"
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
#include "vcdblockdoc.h"
#include "vcdblocktool.h"
#include "datamodel/dmelement.h"
#include "vgui/keycode.h"

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
CInfoTargetBrowserPanel::CInfoTargetBrowserPanel( CVcdBlockDoc *pDoc, vgui::Panel* pParent, const char *pName )
	: BaseClass( pParent, pName ), m_pDoc( pDoc )
{
	SetPaintBackgroundEnabled( true );

	m_pEntities = new vgui::ListPanel( this, "Entities" );
	m_pEntities->AddColumnHeader( 0, "targetname", "Name", 52, ListPanel::COLUMN_RESIZEWITHWINDOW );
 	m_pEntities->AddColumnHeader( 1, "classname", "Class Name", 52, ListPanel::COLUMN_RESIZEWITHWINDOW );
	m_pEntities->SetColumnSortable( 0, true );
	m_pEntities->SetColumnSortable( 1, true );
	m_pEntities->SetEmptyListText( "No info_targets" );
 //	m_pEntities->SetDragEnabled( true );
 	m_pEntities->AddActionSignalTarget( this );
	m_pEntities->SetSortFunc( 0, TargetNameSortFunc );
	m_pEntities->SetSortFunc( 1, ClassNameSortFunc );
	m_pEntities->SetSortColumn( 0 );

	LoadControlSettingsAndUserConfig( "resource/infotargetbrowserpanel.res" );

	UpdateEntityList();
}

CInfoTargetBrowserPanel::~CInfoTargetBrowserPanel()
{
	SaveUserConfig();
}


//-----------------------------------------------------------------------------
// Purpose: Shows the most recent selected object in properties window
//-----------------------------------------------------------------------------
void CInfoTargetBrowserPanel::OnProperties(void)
{
	int iSel = m_pEntities->GetSelectedItem( 0 );
	KeyValues *kv = m_pEntities->GetItem( iSel );
	CDmeVMFEntity *pEntity = CastElement< CDmeVMFEntity >( (CDmElement *)kv->GetPtr( "entity" ) );
	g_pVcdBlockTool->ShowEntityInEntityProperties( pEntity );
}


//-----------------------------------------------------------------------------
// Purpose: Deletes the marked objects.
//-----------------------------------------------------------------------------
void CInfoTargetBrowserPanel::OnDeleteEntities(void)
{		
	int iSel = m_pEntities->GetSelectedItem( 0 );

	{
		// This is undoable
		CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Delete Entities", "Delete Entities" );

		//
		// Build a list of objects to delete.
		//
		int nCount = m_pEntities->GetSelectedItemsCount();
		for (int i = 0; i < nCount; i++)
		{
			int nItemID = m_pEntities->GetSelectedItem(i);
			KeyValues *kv = m_pEntities->GetItem( nItemID );
			CDmeVMFEntity *pEntity = (CDmeVMFEntity *)kv->GetPtr( "entity" );
			if ( pEntity )
			{
				m_pDoc->DeleteInfoTarget( pEntity );
			}
		}
	}

	// Update the list box selection.
	if (iSel >= m_pEntities->GetItemCount())
	{
		iSel = m_pEntities->GetItemCount() - 1;
	}
	m_pEntities->SetSingleSelectedItem( iSel );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoTargetBrowserPanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_DELETE ) 
	{
		OnDeleteEntities();
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoTargetBrowserPanel::OnItemSelected( void )
{
	OnProperties();
}


//-----------------------------------------------------------------------------
// Select a particular node
//-----------------------------------------------------------------------------
void CInfoTargetBrowserPanel::SelectNode( CDmeVMFEntity *pNode )
{
	m_pEntities->ClearSelectedItems();
	for ( int nItemID = m_pEntities->FirstItem(); nItemID != m_pEntities->InvalidItemID(); nItemID = m_pEntities->NextItem( nItemID ) )
	{
		KeyValues *kv = m_pEntities->GetItem( nItemID );
		CDmElement *pEntity = (CDmElement *)kv->GetPtr( "entity" );
		if ( pEntity == pNode )
		{
			m_pEntities->AddSelectedItem( nItemID );
			break;
		}
	}
}

	
//-----------------------------------------------------------------------------
// Called when buttons are clicked
//-----------------------------------------------------------------------------
void CInfoTargetBrowserPanel::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "delete" ) )
	{
		// Confirm we want to do it
		MessageBox *pConfirm = new MessageBox( "#VcdBlockDeleteObjects", "#VcdBlockDeleteObjectsMsg", g_pVcdBlockTool->GetRootPanel() ); 
		pConfirm->AddActionSignalTarget( this );
		pConfirm->SetOKButtonText( "Yes" );
		pConfirm->SetCommand( new KeyValues( "DeleteEntities" ) );
		pConfirm->SetCancelButtonVisible( true );
		pConfirm->SetCancelButtonText( "No" );
		pConfirm->DoModal();
		return;
	}

	if ( !Q_stricmp( pCommand, "Save" ) )
	{
		g_pVcdBlockTool->Save();
		return;
	}

	if ( !Q_stricmp( pCommand, "RestartMap" ) )
	{
		g_pVcdBlockTool->RestartMap();
		return;
	}

	if ( !Q_stricmp( pCommand, "DropInfoTargets" ) )
	{
		g_pVcdBlockTool->EnterTargetDropMode();
		return;
	}

	if ( !Q_stricmp( pCommand, "quicksave" ) )
	{
		g_pVcdBlockTool->QuickSave();
		return;
	}

	if ( !Q_stricmp( pCommand, "quickload" ) )
	{
		g_pVcdBlockTool->QuickLoad();
		return;
	}

	BaseClass::OnCommand( pCommand );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoTargetBrowserPanel::UpdateEntityList(void)
{
	m_pEntities->RemoveAll();

	const CDmrElementArray<CDmElement> entityList( m_pDoc->GetEntityList() );
	if ( !entityList.IsValid() )
		return;

	int nCount = entityList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pEntity = entityList[i];

		const char *pClassName = pEntity->GetValueString( "classname" );
		if ( !pClassName || !pClassName[0] )
		{
			pClassName = "<no class>";
		}

		KeyValues *kv = new KeyValues( "node" );
		kv->SetString( "classname", pClassName ); 
		kv->SetPtr( "entity", pEntity );

		const char *pTargetname = pEntity->GetValueString( "targetname" );
		if ( !pTargetname || !pTargetname[0] )
		{
			pTargetname = "<no targetname>";
		}
		kv->SetString( "targetname", pTargetname ); 

		int nItemID = m_pEntities->AddItem( kv, 0, false, false );

		// Hide everything that isn't an info_target
		m_pEntities->SetItemVisible( nItemID, !Q_stricmp( pClassName, "info_target" ) );
	}
	m_pEntities->SortList();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoTargetBrowserPanel::Refresh(void)
{
	for ( int nItemID = m_pEntities->FirstItem(); nItemID != m_pEntities->InvalidItemID(); nItemID = m_pEntities->NextItem( nItemID ) )
	{
		KeyValues *kv = m_pEntities->GetItem( nItemID );
		CDmElement *pEntity = (CDmElement *)kv->GetPtr( "entity" );

		const char *pTargetname = pEntity->GetValueString( "targetname" );
		if ( !pTargetname || !pTargetname[0] )
		{
			pTargetname = "<no targetname>";
		}
		kv->SetString( "targetname", pTargetname ); 
	}
}

