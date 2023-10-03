//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Panel to edit "state" nodes in layout system definitions.
//
//===============================================================================

#include "vgui_controls/Button.h"
#include "KeyValues.h"
#include "layout_system_editor/rule_instance_node_panel.h"
#include "layout_system_editor/state_node_panel.h"
 
using namespace vgui;

//////////////////////////////////////////////////////////////////////////
// Public Implementation
//////////////////////////////////////////////////////////////////////////

CStateNodePanel::CStateNodePanel( Panel *pParent, const char *pName ) : 
BaseClass( pParent, pName )
{
	EnableNameEditBox( true );
}

void CStateNodePanel::CreatePanelContents()
{
	KeyValues *pNodeKV = GetData();

	int nIndex = 0;
	for ( KeyValues *pSubKey = pNodeKV->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey() )
	{
		bool bIsState = ( Q_stricmp( pSubKey->GetName(), "state" ) == 0 );
		bool bIsRuleInstance = ( Q_stricmp( pSubKey->GetName(), "rule_instance" ) == 0 );
		bool bIsAction = ( Q_stricmp( pSubKey->GetName(), "action" ) == 0 );
		if ( bIsState || bIsRuleInstance || bIsAction )
		{
			AddNewElementPanel( nIndex, true, false );
			CNodePanel *pPanel = CreateChild( pSubKey );
			pPanel->EnableDeleteButton( true );
			AddChild( pPanel, 0 );
			
			if ( bIsRuleInstance )
			{
				CRuleInstanceNodePanel *pRuleInstancePanel = ( CRuleInstanceNodePanel * )pPanel;
				pRuleInstancePanel->AddAllowableRuleType( "state" );
				pRuleInstancePanel->AddAllowableRuleType( "action" );
			}
		}
		++ nIndex;
	}
	AddNewElementPanel( nIndex, true, false );
}

void CStateNodePanel::UpdateState()
{
	KeyValues *pNodeKV = GetData();
	if ( pNodeKV != NULL )
	{
		Q_strncpy( m_NodeLabel, "State", m_nNameLength );
		Q_strncpy( m_NodeName, pNodeKV->GetString( "name", "<unnamed>"), m_nNameLength );
	}

	SetBgColor( Color( 60, 60, 60, 255 ) );
	BaseClass::UpdateState();
}