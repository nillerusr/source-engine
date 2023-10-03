//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Root panel of a layout system definition (aka mission)
//
//===============================================================================

#include "KeyValues.h"
#include "vgui_controls/Button.h"
#include "layout_system_editor/state_node_panel.h"
#include "layout_system_editor/rule_instance_node_panel.h"
#include "layout_system_editor/mission_panel.h"

using namespace vgui;

CMissionPanel::CMissionPanel( Panel *pParent, const char *pName ) : 
BaseClass( pParent, pName ) 
{ 
	EnableNameEditBox( true );
	SetChildIndent( 10 );
}

void CMissionPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	
	int x, y;
	GetSizer()->GetMinSize( x, y );
	SetSize( MAX( x, 450 ), y );
	SetBgColor( Color( 48, 48, 48, 255 ) );
}

void CMissionPanel::CreatePanelContents()
{
	KeyValues *pNodeKV = GetData();

	if ( pNodeKV == NULL )
	{
		return;
	}

	int nIndex = 0;
	for ( KeyValues *pSubKey = pNodeKV->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey() )
	{
		bool bIsState = ( Q_stricmp( pSubKey->GetName(), "state" ) == 0 );
		bool bIsRuleInstance = ( Q_stricmp( pSubKey->GetName(), "rule_instance" ) == 0 );
		if ( bIsState || bIsRuleInstance )
		{
			AddNewElementPanel( nIndex, true, false );
			CNodePanel *pPanel = CreateChild( pSubKey );
			pPanel->EnableDeleteButton( true );
			AddChild( pPanel, 10 );

			if ( bIsRuleInstance )
			{
				CRuleInstanceNodePanel *pRuleInstancePanel = ( CRuleInstanceNodePanel * )pPanel;
				pRuleInstancePanel->AddAllowableRuleType( "state" );
				pRuleInstancePanel->AddAllowableRuleType( "global_action" );
				pRuleInstancePanel->AddAllowableRuleType( "option_block" );
			}
		}
		++ nIndex;
	}
	AddNewElementPanel( nIndex, true, false );
}

void CMissionPanel::UpdateState()
{
	KeyValues *pNodeKV = GetData();
	if ( pNodeKV != NULL )
	{
		Q_strncpy( m_NodeLabel, "Mission", m_nNameLength );
		Q_strncpy( m_NodeName, pNodeKV->GetString( "name", "<unnamed>"), m_nNameLength );
	}

	BaseClass::UpdateState();
}
