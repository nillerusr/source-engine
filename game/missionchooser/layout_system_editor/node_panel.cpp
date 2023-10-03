//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Base class for panels used to edit nodes in layout system definitions.
//
//===============================================================================

#include "vgui_controls/Panel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/TextEntry.h"
#include "KeyValues.h"
#include "layout_system_editor/layout_system_kv_editor.h"
#include "layout_system_editor/rule_instance_node_panel.h"
#include "layout_system_editor/state_node_panel.h"
#include "node_panel.h"

using namespace vgui;

CNodePanel::CNodePanel( Panel *pParent, const char *pName ) : 
BaseClass( pParent, pName ),
m_pNodeKV( NULL ),
m_pEditor( NULL ),
m_bShowNameEditBox( false ),
m_bShowDeleteButton( false )
{ 
	m_pLabel = new Label( this, "Label", "" );
	m_pLabel->SetMouseInputEnabled( false );
	m_pNameEditBox = new TextEntry( this, "TextEntry" );
	m_NodeLabel[0] = '\0';
	m_NodeName[0] = '\0';

	m_pRootSizer = new CBoxSizer( ESLD_VERTICAL );
	
	m_pLabelSizer = new CBoxSizer( ESLD_HORIZONTAL );
	m_pLabelSizer->AddPanel( m_pLabel, SizerAddArgs_t().Padding( 2 ) );
	m_pLabelSizer->AddPanel( m_pNameEditBox, SizerAddArgs_t().Padding( 2 ).MinSize( 200, 20 ) );
	m_pRootSizer->AddSizer( m_pLabelSizer, SizerAddArgs_t().Padding( 5 ) );

	m_pHeadingSizer = new CBoxSizer( ESLD_HORIZONTAL );
	m_pLabelSizer->AddSizer( m_pHeadingSizer, SizerAddArgs_t().Padding( 0 ).Expand( 1.0f ) );

	m_pDeleteSelfButton = new Button( this, "DeleteSelf", "X", this, "DeleteSelf" );
	m_pLabelSizer->AddPanel( m_pDeleteSelfButton, SizerAddArgs_t().Padding( 2 ) );

	m_pChildSizer = new CBoxSizer( ESLD_VERTICAL );
	m_pChildIndentSizer = new CBoxSizer( ESLD_HORIZONTAL );
	m_pChildIndentSizer->AddSpacer( SizerAddArgs_t().MinSize( 0, 0 ).Padding( 0 ) );
	m_pChildIndentSizer->AddSizer( m_pChildSizer, SizerAddArgs_t().Padding( 0 ).Expand( 1.0f ) );
	m_pRootSizer->AddSizer( m_pChildIndentSizer, SizerAddArgs_t().Padding( 0 ).Expand( 1.0f ) );
	
	SetSizer( m_pRootSizer );

	// We want a transparent background for the label.
	m_pLabel->SetPaintBackgroundEnabled( false );
}

void CNodePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBorder( pScheme->GetBorder( "RaisedBorder" ) );
}

void CNodePanel::OnCommand( const char *pCommand )
{
	if ( Q_stricmp( pCommand, "DeleteSelf" ) == 0 )
	{
		DeleteSelf();
		return;
	}

	BaseClass::OnCommand( pCommand );
}

void CNodePanel::OnAddRule( KeyValues *pKV )
{
	int nIndex = pKV->GetInt( "index", -1 );
	Assert( nIndex != -1 );
	InsertChildData( nIndex, new KeyValues( "rule_instance", NULL, "<null>" ) );
}

void CNodePanel::OnAddState( KeyValues *pKV )
{
	int nIndex = pKV->GetInt( "index", -1 );
	Assert( nIndex != -1 );
	InsertChildData( nIndex, new KeyValues( "state", "name", "MyNewState" ) );
}

void CNodePanel::OnTextChanged( vgui::Panel *pPanel )
{
	Assert( pPanel == m_pNameEditBox );
	const int nMaxTextLength = 2048;
	char buf[nMaxTextLength];
	m_pNameEditBox->GetText( buf, nMaxTextLength );
	buf[nMaxTextLength - 1] = '\0';
	SetNodeName( buf );
}

void CNodePanel::SetData( KeyValues *pNodeKV )
{
	if ( m_pNodeKV != NULL && m_pNodeKV != pNodeKV )
	{
		CNodePanel *pParentNodePanel = dynamic_cast< CNodePanel * >( GetParent() );
		if ( pParentNodePanel != NULL )
		{
			Assert( pParentNodePanel->m_pNodeKV->ContainsSubKey( m_pNodeKV ) );

			if ( pNodeKV != NULL )
			{
				// Must replace parent's sub key with ours
				pParentNodePanel->m_pNodeKV->SwapSubKey( m_pNodeKV, pNodeKV );
			}
			else
			{
				// Just remove the parent's sub key
				pParentNodePanel->m_pNodeKV->RemoveSubKey( m_pNodeKV );
			}
			
		}
		m_pNodeKV->deleteThis();
	}

	m_pNodeKV = pNodeKV;
	
	RecreateControls();
}

void CNodePanel::RecreateControls()
{
	ClearHeading();
	ClearChildren();

	CreatePanelContents();

	UpdateState();
}

void CNodePanel::UpdateState()
{
	if ( m_NodeLabel[0] != '\0' )
	{
		m_pLabel->SetText( m_NodeLabel );
		m_pLabel->SetVisible( true );
	}
	else
	{
		m_pLabel->SetText( "" );
		m_pLabel->SetVisible( false );
	}

	if ( m_bShowNameEditBox )
	{
		m_pNameEditBox->SetText( m_NodeName );
	}
	m_pNameEditBox->SetVisible( m_bShowNameEditBox );
	m_pDeleteSelfButton->SetVisible( m_bShowDeleteButton );

	// Show or hide the "add" buttons on the CNewElementPanel objects
	for ( int i = 0; i < m_pChildSizer->GetElementCount(); ++ i )
	{
		if ( m_pChildSizer->GetElementType( i ) == ESET_PANEL )
		{
			CNewElementPanel *pNewElementPanel = dynamic_cast< CNewElementPanel * >( m_pChildSizer->GetPanel( i ) );
			if ( pNewElementPanel != NULL )
			{
				pNewElementPanel->SetVisible( GetEditor()->ShouldShowAddButtons() );
			}
		}
	}

	// Update state on all children
	for ( int i = 0; i < m_ChildPanels.Count(); ++ i )
	{
		m_ChildPanels[i]->UpdateState();
	}
}

void CNodePanel::InvalidateEditorLayout()
{
	if ( m_pEditor != NULL )
	{
		m_pEditor->InvalidateLayout();
	}
}

void CNodePanel::ClearHeading()
{
	m_pHeadingSizer->RemoveAllMembers( true );
	InvalidateEditorLayout();
}

void CNodePanel::AddHeadingElement( Panel *pPanel, float flExpandFactor, int nPadding )
{
	m_pHeadingSizer->AddPanel( pPanel, SizerAddArgs_t().Padding( nPadding ).Expand( flExpandFactor ) );
	InvalidateEditorLayout();
}

void CNodePanel::AddHeadingSpacer()
{
	m_pHeadingSizer->AddSpacer( SizerAddArgs_t().Padding( 0 ).Expand( 1.0f ) );
	InvalidateEditorLayout();
}

void CNodePanel::SetChildIndent( int nIndent )
{
	m_pChildIndentSizer->SetElementArgs( 0, SizerAddArgs_t().MinSize( nIndent, 0 ).Padding( 0 ) );
	InvalidateEditorLayout();
}

void CNodePanel::ClearChildren()
{
	m_ChildPanels.RemoveAll();
	m_pChildSizer->RemoveAllMembers( true );
	InvalidateEditorLayout();
}

void CNodePanel::AddChild( CNodePanel *pPanel, int nPadding )
{
	m_ChildPanels.AddToTail( pPanel );
	AddChild( ( Panel * )pPanel, nPadding );
}

void CNodePanel::AddChild( Panel *pPanel, int nPadding )
{
	m_pChildSizer->AddPanel( pPanel, SizerAddArgs_t().Padding( nPadding ) );
	InvalidateEditorLayout();
}

void CNodePanel::InsertChildData( int nIndex, KeyValues *pKeyValues )
{
	KeyValues *pNodeKV = GetData();
	pNodeKV->InsertSubKey( nIndex, pKeyValues );
	RecreateControls();
}

// Register new panel types here.  There probably won't be that many kinds,
// so there's no reason to have a general purpose factory function.
CNodePanel *CNodePanel::CreateChild( KeyValues *pKey )
{
	CNodePanel *pNodePanel = NULL;
	const char *pPanelType = pKey->GetName();

	if ( Q_stricmp( pPanelType, "state" ) == 0 )
	{
		pNodePanel = new CStateNodePanel( this, "state_panel" );
	}
	else if ( Q_stricmp( pPanelType, "rule_instance" ) == 0 )
	{
		pNodePanel = new CRuleInstanceNodePanel( this, "rule_instance_panel" );
	}

	if ( pNodePanel == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Unrecognized node type (cannot create panel): %s.\n", pPanelType );
		return NULL;
	}

	pNodePanel->SetEditor( m_pEditor );
	pNodePanel->SetData( pKey );
	
	return pNodePanel;
}

void CNodePanel::DeleteSelf()
{
	CNodePanel *pParent = dynamic_cast< CNodePanel * >( GetParent() );
	Assert( pParent != NULL );

	KeyValues *pParentKV = pParent->GetData();
	Assert( pParentKV->ContainsSubKey( m_pNodeKV ) );
	pParentKV->RemoveSubKey( m_pNodeKV );
	m_pNodeKV->deleteThis();
	m_pNodeKV = NULL;

	// Refresh parent's panel contents.
	pParent->RecreateControls();
}

void CNodePanel::AddNewElementPanel( int nIndex, bool bAllowRules, bool bAllowStates )
{
	CNewElementPanel *pNewElementPanel = new CNewElementPanel( this, "NewElement", nIndex );
	if ( bAllowRules )
	{
		pNewElementPanel->AddButton( "Add Rule", "AddRule" );
	}
	if ( bAllowStates )
	{
		pNewElementPanel->AddButton( "Add State", "AddState" );
	}
	AddChild( pNewElementPanel, 5 );
}

void CNodePanel::SetNodeName( const char *pNodeName )
{
	KeyValues *pNodeKV = GetData();
	if ( pNodeKV != NULL )
	{
		pNodeKV->SetString( "name", pNodeName );
	}
}

CNewElementPanel::CNewElementPanel( Panel *pParent, const char *pName, int nIndex ) :
BaseClass( pParent, pName ),
m_nIndex( nIndex )
{
	m_pHorizontalSizer = new CBoxSizer( ESLD_HORIZONTAL );
	SetSizer( m_pHorizontalSizer );
}

void CNewElementPanel::AddButton( const char *pButtonText, const char *pActionName )
{
	Button *pButton = new Button( this, pActionName, pButtonText, GetParent(), pActionName );

	KeyValues *pMessageKV = new KeyValues( pActionName );
	KeyValues *pIndexKV = new KeyValues( "index" );
	pIndexKV->SetInt( NULL, m_nIndex );
	pMessageKV->AddSubKey( pIndexKV );
	pButton->SetCommand( pMessageKV );

	m_pHorizontalSizer->AddPanel( pButton, SizerAddArgs_t().MinSize( 50, 20 ).Padding( 0 ) );
	InvalidateLayout();
}