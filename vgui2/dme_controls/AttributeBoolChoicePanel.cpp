//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeBoolChoicePanel.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/ComboBox.h"
#include "datamodel/dmelement.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "dme_controls/inotifyui.h"
#include "dme_controls/dmecontrols.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Expose DmeEditorAttributeInfo to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeEditorBoolChoicesInfo, CDmeEditorBoolChoicesInfo );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeEditorBoolChoicesInfo::OnConstruction()
{
	// Add a true + false method, default
	CreateChoice( "false" );
	CreateChoice( "true" );
}

void CDmeEditorBoolChoicesInfo::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Add a choice
//-----------------------------------------------------------------------------
void CDmeEditorBoolChoicesInfo::SetFalseChoice( const char *pChoiceString )
{
	m_Choices[0]->SetValue<CUtlString>( "string", pChoiceString );
}

void CDmeEditorBoolChoicesInfo::SetTrueChoice( const char *pChoiceString )
{
	m_Choices[1]->SetValue<CUtlString>( "string", pChoiceString );
}


//-----------------------------------------------------------------------------
// Gets the choices
//-----------------------------------------------------------------------------
const char *CDmeEditorBoolChoicesInfo::GetFalseChoiceString( ) const
{
	return GetChoiceString( 0 );
}

const char *CDmeEditorBoolChoicesInfo::GetTrueChoiceString( ) const
{
	return GetChoiceString( 1 );
}


//-----------------------------------------------------------------------------
//
// Constructor
//
//-----------------------------------------------------------------------------
CAttributeBoolChoicePanel::CAttributeBoolChoicePanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
}


//-----------------------------------------------------------------------------
// Derived classes can re-implement this to fill the combo box however they like
//-----------------------------------------------------------------------------
void CAttributeBoolChoicePanel::PopulateComboBox( vgui::ComboBox *pComboBox )
{
	pComboBox->DeleteAllItems();

	CDmeEditorBoolChoicesInfo *pInfo = CastElement<CDmeEditorBoolChoicesInfo>( GetEditorInfo() );
	if ( !pInfo )
		return;

	// Fill in the choices
	const char *pFalseChoice = pInfo->GetFalseChoiceString( );
	const char *pTrueChoice = pInfo->GetTrueChoiceString( );

	// Add the dynamic choices next
	if ( pInfo->HasChoiceType() )
	{
		const char *choices[2];
		if ( ElementPropertiesChoices()->GetBoolChoiceList( pInfo->GetChoiceType(), GetPanelElement(), GetAttributeName(), IsArrayEntry(), choices ) )
		{
			pFalseChoice = choices[0];
			pTrueChoice = choices[1];
		}
	}

	KeyValues *kv = new KeyValues( "entry" );
	kv->SetInt( "value", false );
	pComboBox->AddItem( pFalseChoice, kv );

	kv = new KeyValues( "entry" );
	kv->SetInt( "value", true );
	pComboBox->AddItem( pTrueChoice, kv );
}


//-----------------------------------------------------------------------------
// Sets the attribute based on the combo box
//-----------------------------------------------------------------------------
void CAttributeBoolChoicePanel::SetAttributeFromComboBox( vgui::ComboBox *pComboBox, KeyValues *pKeyValues )
{
	bool bOldValue = GetAttributeValue<bool>();
	bool bValue = pKeyValues->GetInt( "value", 0 ) != 0;
	if ( bOldValue == bValue )
		return;

	CUndoScopeGuard guard( NOTIFY_SOURCE_PROPERTIES_TREE, NOTIFY_SETDIRTYFLAG, "Set Attribute Value", "Set Attribute Value" );
	SetAttributeValue( bValue );
}


//-----------------------------------------------------------------------------
// Sets the combo box from the attribute 
//-----------------------------------------------------------------------------
void CAttributeBoolChoicePanel::SetComboBoxFromAttribute( vgui::ComboBox *pComboBox )
{
	CDmeEditorBoolChoicesInfo *pInfo = CastElement<CDmeEditorBoolChoicesInfo>( GetEditorInfo() );
	if ( !pInfo )
		return;

	bool bValue = GetAttributeValue<bool>();

	// Check the dynamic choices next
	if ( pInfo->HasChoiceType() )
	{
		const char *choices[2];
		if ( ElementPropertiesChoices()->GetBoolChoiceList( pInfo->GetChoiceType(), GetPanelElement(), GetAttributeName(), IsArrayEntry(), choices ) )
		{
			pComboBox->SetText( choices[ bValue ] );
			return;
		}
	}

	pComboBox->SetText( pInfo->GetChoiceString( bValue ) );
}
