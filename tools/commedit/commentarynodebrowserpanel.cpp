//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Singleton dialog that generates and presents the entity report.
//
//===========================================================================//

#include "CommentaryNodeBrowserPanel.h"
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
CCommentaryNodeBrowserPanel::CCommentaryNodeBrowserPanel( CCommEditDoc *pDoc, vgui::Panel* pParent, const char *pName )
	: BaseClass( pParent, pName ), m_pDoc( pDoc )
{
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

	LoadControlSettingsAndUserConfig( "resource/commentarynodebrowserpanel.res" );

	UpdateEntityList();
}

CCommentaryNodeBrowserPanel::~CCommentaryNodeBrowserPanel()
{
	SaveUserConfig();
}


//-----------------------------------------------------------------------------
// Purpose: Shows the most recent selected object in properties window
//-----------------------------------------------------------------------------
void CCommentaryNodeBrowserPanel::OnProperties( )
{
	if ( m_pEntities->GetSelectedItemsCount() == 0 )
	{
		g_pCommEditTool->ShowEntityInEntityProperties( NULL );
		return;
	}

	int iSel = m_pEntities->GetSelectedItem( 0 );
	KeyValues *kv = m_pEntities->GetItem( iSel );
	CDmeCommentaryNodeEntity *pEntity = CastElement< CDmeCommentaryNodeEntity >( (CDmElement *)kv->GetPtr( "entity" ) );
	g_pCommEditTool->ShowEntityInEntityProperties( pEntity );
}


//-----------------------------------------------------------------------------
// Purpose: Deletes the marked objects.
//-----------------------------------------------------------------------------
void CCommentaryNodeBrowserPanel::OnDeleteEntities(void)
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
			CDmElement *pEntity = (CDmElement *)kv->GetPtr( "entity" );
			if ( pEntity )
			{
				m_pDoc->DeleteCommentaryNode( pEntity );
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
void CCommentaryNodeBrowserPanel::OnKeyCodeTyped( vgui::KeyCode code )
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
void CCommentaryNodeBrowserPanel::OnItemSelected( void )
{
	OnProperties();
}


//-----------------------------------------------------------------------------
// Select a particular node
//-----------------------------------------------------------------------------
void CCommentaryNodeBrowserPanel::SelectNode( CDmeCommentaryNodeEntity *pNode )
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
void CCommentaryNodeBrowserPanel::OnCommand( const char *pCommand )
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

	if ( !Q_stricmp( pCommand, "Save" ) )
	{
		g_pCommEditTool->Save();
		return;
	}

	if ( !Q_stricmp( pCommand, "CenterView" ) )
	{
		if ( m_pEntities->GetSelectedItemsCount() == 1 )
		{
			int iSel = m_pEntities->GetSelectedItem( 0 );
			KeyValues *kv = m_pEntities->GetItem( iSel );
			CDmeCommentaryNodeEntity *pEntity = CastElement< CDmeCommentaryNodeEntity >( (CDmElement *)kv->GetPtr( "entity" ) );
			g_pCommEditTool->CenterView( pEntity );
		}
		return;
	}

	if ( !Q_stricmp( pCommand, "SaveAndTest" ) )
	{
		g_pCommEditTool->SaveAndTest();
		return;
	}

	if ( !Q_stricmp( pCommand, "DropNodes" ) )
	{
		g_pCommEditTool->EnterNodeDropMode();
		return;
	}

	BaseClass::OnCommand( pCommand );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommentaryNodeBrowserPanel::UpdateEntityList(void)
{
	m_pEntities->RemoveAll();

	CDmrCommentaryNodeEntityList entityList( m_pDoc->GetEntityList() );
	if ( !entityList.IsValid() )
		return;

	int nCount = entityList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pEntity = entityList[i];
		Assert( pEntity );
		if ( !pEntity )
			continue;

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

		m_pEntities->AddItem( kv, 0, false, false );
	}
	m_pEntities->SortList();
}

