//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Panel to edit "rule_instance" nodes in layout system definitions.
//
//===============================================================================

#include "KeyValues.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/Tooltip.h"
#include "layout_system/tilegen_mission_preprocessor.h"
#include "layout_system/tilegen_rule.h"
#include "layout_system_editor/layout_system_kv_editor.h"
#include "layout_system_editor/rule_instance_parameter_panel.h"
#include "layout_system_editor/rule_instance_node_panel.h"

using namespace vgui;

//////////////////////////////////////////////////////////////////////////
// Forward Declarations
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// Is a single rule type name in a given list of allowed rule types?
//-----------------------------------------------------------------------------
bool IsRuleTypeInList( const char *pType, const RuleType_t *pTypeList, int nNumTypes );

//-----------------------------------------------------------------------------
// Does at least one of a rule's types match a list of allowed rule types?
//-----------------------------------------------------------------------------
bool RuleMatchExists( const RuleType_t *pTypes, int nNumTypes, const RuleType_t *pValidTypeList, int nNumValidTypes );

//////////////////////////////////////////////////////////////////////////
// Public Implementation
//////////////////////////////////////////////////////////////////////////

CRuleInstanceNodePanel::CRuleInstanceNodePanel( Panel *pParent, const char *pName ) : 
BaseClass( pParent, pName ),
m_pChangeRuleButton( NULL ),
m_pDisableRuleButton( NULL ),
m_bDisabled( false )
{ 	
}

void CRuleInstanceNodePanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBgColor( Color( 72, 72, 72, 255 ) );
}

void CRuleInstanceNodePanel::OnCommand( const char *pCommand )
{
	if ( Q_stricmp( pCommand, "ChangeRule" ) == 0 )
	{
		CRulePickerDialog *pRulePicker = new CRulePickerDialog( 
			this, 
			"RulePicker", 
			GetEditor()->GetPreprocessor(), 
			m_AllowableRuleTypes.Base(), 
			m_AllowableRuleTypes.Count() );
		pRulePicker->AddActionSignalTarget( this );
		pRulePicker->DoModal();
		return;
	}
	else if ( Q_stricmp( pCommand, "DisableRule" ) == 0 )
	{
		m_bDisabled = !m_bDisabled;
		m_pDisableRuleButton->SetText( m_bDisabled ? "Enable" : "Disable" );
		if ( m_bDisabled )
		{
			GetData()->SetBool( "disabled", true );
		}
		else
		{
			// Don't pollute the file with a bunch of "disabled" "0" nodes
			GetData()->RemoveSubKey( GetData()->FindKey( "disabled" ) );
		}
		
		RecreateControls();
	}

	BaseClass::OnCommand( pCommand );
}

void CRuleInstanceNodePanel::OnRuleChanged( KeyValues *pKeyValues )
{
	KeyValues *pNewKV = pKeyValues->GetFirstSubKey();
	if ( pNewKV != NULL )
	{
		pKeyValues->RemoveSubKey( pNewKV );
		KeyValues *pCurrentData = GetData();
		if ( HasDummyContainerNode() )
		{
			pCurrentData->SwapSubKey( pCurrentData->GetFirstSubKey(), pNewKV );
			RecreateControls();
		}
		else
		{
			SetData( pNewKV );
		}
	}
}

void CRuleInstanceNodePanel::AddAllowableRuleType( const char *pType )
{
	RuleType_t allowedRuleType;
	Q_strncpy( allowedRuleType.m_Name, pType, MAX_TILEGEN_IDENTIFIER_LENGTH );
	
	if ( !IsRuleTypeInList( allowedRuleType.m_Name, m_AllowableRuleTypes.Base(), m_AllowableRuleTypes.Count() ) )
	{
		m_AllowableRuleTypes.AddToTail( allowedRuleType );
	}
}

//////////////////////////////////////////////////////////////////////////
// Private Implementation
//////////////////////////////////////////////////////////////////////////

void CRuleInstanceNodePanel::CreatePanelContents()
{
	KeyValues *pNodeKV = GetData();

	// If this rule instance is wrapped in a dummy key values node, move down a level.
	if ( HasDummyContainerNode() )
	{
		pNodeKV = pNodeKV->GetFirstSubKey();
	}

	m_bDisabled = pNodeKV->GetBool( "disabled", false );
	m_pChangeRuleButton = new Button( this, "ChangeRule", "Change", this, "ChangeRule" );
	AddHeadingElement( m_pChangeRuleButton, 0.0f );
	m_pDisableRuleButton = new Button( this, "DisableRule", m_bDisabled ? "Enable" : "Disable", this, "DisableRule" );
	AddHeadingElement( m_pDisableRuleButton, 0.0f );
	
	Assert( pNodeKV != NULL );

	const char *pRuleName = pNodeKV->GetString( "name", NULL );
	
	if ( pRuleName == NULL )
	{
		Q_snprintf( m_NodeLabel, m_nNameLength, "No Rule specified" );
	}
	else
	{
		const CTilegenMissionPreprocessor *pPreprocessor = GetEditor()->GetPreprocessor();
		const CTilegenRule *pRule = pPreprocessor->FindRule( pRuleName );
		if ( pRule != NULL )
		{
			GetTooltip()->SetText( pRule->GetDescription() );

			if ( !m_bDisabled )
			{
				for ( int i = 0; i < pRule->GetParameterCount(); ++ i )
				{
					// @TODO: need to error if a non-optional parameter is missing
					const char *pParameterName = pRule->GetParameterName( i );
					KeyValues *pInstanceParameterKV = pNodeKV->FindKey( pParameterName );
					CRuleInstanceParameterPanel *pPanel = new CRuleInstanceParameterPanel( this, "text_entry", pRule, i );
					pPanel->SetEditor( GetEditor() );
					pPanel->SetData( pInstanceParameterKV );
					AddChild( pPanel, 0 );
				}
			}

			Q_snprintf( m_NodeLabel, m_nNameLength, "%s", pRule->GetFriendlyName() );
		}
		else
		{
			Q_snprintf( m_NodeLabel, m_nNameLength, "Rule '%s' not found!", pRuleName );
		}
	}
}

bool CRuleInstanceNodePanel::HasDummyContainerNode()
{
	KeyValues *pNodeKV = GetData();
	if ( Q_stricmp( pNodeKV->GetName(), "rule_instance" ) != 0 )
	{
		// There is a dummy KV node wrapping this rule instance
		Assert( pNodeKV->GetFirstSubKey() != NULL && Q_stricmp( pNodeKV->GetFirstSubKey()->GetName(), "rule_instance" ) == 0 );
		return true;
	}
	else
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
// Helper Function Implementations
//////////////////////////////////////////////////////////////////////////

KeyValues *InstantiateRule( const CTilegenMissionPreprocessor *pPreprocessor, const char *pRuleName )
{
	const CTilegenRule *pRule = pPreprocessor->FindRule( pRuleName );
	if ( pRule != NULL )
	{
		KeyValues *pNewKV = new KeyValues( "rule_instance", "name", pRuleName );
		for ( int i = 0; i < pRule->GetParameterCount(); ++ i )
		{
			if ( !pRule->IsParameterOptional( i ) )
			{
				const KeyValues *pDefaultValue = pRule->GetDefaultValue( i );
				KeyValues *pRuleInstanceParameter = new KeyValues( pRule->GetParameterName( i ) );
				if ( pDefaultValue != NULL )
				{
					pRuleInstanceParameter->AddSubKey( pDefaultValue->MakeCopy() );
				}
				else
				{
					pRuleInstanceParameter->SetString( NULL, "<Enter Value>" );
				}
				pNewKV->AddSubKey( pRuleInstanceParameter );
			}
		}
		return pNewKV;
	}
	return NULL;
}


CRulePickerDialog::CRulePickerDialog( 
	vgui::Panel *pParent, 
	const char *pName, 
	const CTilegenMissionPreprocessor *pRulePreprocessor,
	RuleType_t *pAllowableRuleTypes,
	int nNumAllowableRuleTypes ) :
BaseClass( pParent, pName ),
m_pRulePreprocessor( pRulePreprocessor )
{
	m_pRulePanelList = new PanelListPanel( this, "RulePanelListPanel" );
	LoadControlSettings( "tilegen/RulePicker.res", "GAME" );
	SetDeleteSelfOnClose( true );
	MoveToCenterOfScreen();
	for ( int i = 0; i < m_pRulePreprocessor->GetRuleCount(); ++ i )
	{
		const CTilegenRule *pRule = m_pRulePreprocessor->GetRule( i );
		
		if ( !pRule->IsHidden() && RuleMatchExists( pRule->GetTypes(), pRule->GetTypeCount(), pAllowableRuleTypes, nNumAllowableRuleTypes ) )
		{
			CRuleDetailsPanel *pRuleDetails = new CRuleDetailsPanel( this, "RuleDetails", pRule );
			m_pRulePanelList->AddItem( NULL, pRuleDetails );
		}
	}
}

void CRulePickerDialog::OnRuleDetailsClicked( vgui::Panel *pPanel )
{
	CRuleDetailsPanel *pRuleDetails = dynamic_cast< CRuleDetailsPanel * >( pPanel );
	Assert( pPanel != NULL );

	KeyValues *pNewRuleInstance = InstantiateRule( m_pRulePreprocessor, pRuleDetails->GetRule()->GetName() );
	if ( pNewRuleInstance != NULL )
	{
		KeyValues *pMessage = new KeyValues( "RuleChanged" );
		pMessage->AddSubKey( pNewRuleInstance );
		PostActionSignal( pMessage );
	}
	OnClose();
}

CRuleDetailsPanel::CRuleDetailsPanel( vgui::Panel *pParent, const char *pName, const CTilegenRule *pRule ) :
BaseClass( pParent, pName ),
m_pRule( pRule )
{
	m_pRuleNameLabel = new Label( this, "RuleName", m_pRule->GetFriendlyName() );
	m_pRuleNameLabel->SetMouseInputEnabled( false );
	m_pRuleNameLabel->SetPaintBackgroundEnabled( false );
	m_pRuleNameLabel->SizeToContents();
	SetMouseInputEnabled( true );
	GetTooltip()->SetText( pRule->GetDescription() );
}

void CRuleDetailsPanel::OnMouseReleased( MouseCode code )
{
	PostActionSignal( new KeyValues( "RuleDetailsClicked" ) );
}

//////////////////////////////////////////////////////////////////////////
// Helper Functions
//////////////////////////////////////////////////////////////////////////

bool IsRuleTypeInList( const char *pType, const RuleType_t *pTypeList, int nNumTypes )
{
	for ( int i = 0; i < nNumTypes; ++ i )
	{
		if ( Q_stricmp( pTypeList[i].m_Name, pType ) == 0 )
		{
			return true;
		}
	}
	return false;
}

bool RuleMatchExists( const RuleType_t *pTypes, int nNumTypes, const RuleType_t *pValidTypeList, int nNumValidTypes )
{
	for ( int i = 0; i < nNumTypes; ++ i )
	{
		if ( IsRuleTypeInList( pTypes[i].m_Name, pValidTypeList, nNumValidTypes ) )
		{
			return true;
		}
	}
	return false;
}