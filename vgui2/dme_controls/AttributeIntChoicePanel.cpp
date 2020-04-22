//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeIntChoicePanel.h"
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
IMPLEMENT_ELEMENT_FACTORY( DmeEditorIntChoicesInfo, CDmeEditorIntChoicesInfo );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeEditorIntChoicesInfo::OnConstruction()
{
}

void CDmeEditorIntChoicesInfo::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Add a choice
//-----------------------------------------------------------------------------
void CDmeEditorIntChoicesInfo::AddChoice( int nValue, const char *pChoiceString )
{
	CDmElement *pChoice = CreateChoice( pChoiceString );
	pChoice->SetValue( "value", nValue );
}


//-----------------------------------------------------------------------------
// Gets the choices
//-----------------------------------------------------------------------------
int CDmeEditorIntChoicesInfo::GetChoiceValue( int nIndex ) const
{
	Assert( ( nIndex < GetChoiceCount() ) && ( nIndex >= 0 ) );
	CDmElement *pChoice = m_Choices[nIndex];
	if ( !pChoice )
		return 0;

	return pChoice->GetValue<int>( "value" );
}


//-----------------------------------------------------------------------------
//
// Constructor
//
//-----------------------------------------------------------------------------
CAttributeIntChoicePanel::CAttributeIntChoicePanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
}


//-----------------------------------------------------------------------------
// Derived classes can re-implement this to fill the combo box however they like
//-----------------------------------------------------------------------------
void CAttributeIntChoicePanel::PopulateComboBox( vgui::ComboBox *pComboBox )
{
	pComboBox->DeleteAllItems();

	CDmeEditorIntChoicesInfo *pInfo = CastElement<CDmeEditorIntChoicesInfo>( GetEditorInfo() );
	if ( !pInfo )
		return;

	// Fill in the choices
	int c = pInfo->GetChoiceCount();
	for ( int i = 0; i < c; ++i )
	{
		KeyValues *kv = new KeyValues( "entry" );
		kv->SetInt( "value", pInfo->GetChoiceValue( i ) );
		pComboBox->AddItem( pInfo->GetChoiceString( i ) , kv );
	}

	// Add the dynamic choices next
	if ( pInfo->HasChoiceType() )
	{
		IntChoiceList_t choices;
		if ( ElementPropertiesChoices()->GetIntChoiceList( pInfo->GetChoiceType(), GetPanelElement(), GetAttributeName(), IsArrayEntry(), choices ) )
		{
			c = choices.Count();
			for ( int i = 0; i < c; ++i )
			{
				KeyValues *kv = new KeyValues( "entry" );
				kv->SetInt( "value", choices[i].m_nValue );
				pComboBox->AddItem( choices[i].m_pChoiceString, kv );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Sets the attribute based on the combo box
//-----------------------------------------------------------------------------
void CAttributeIntChoicePanel::SetAttributeFromComboBox( vgui::ComboBox *pComboBox, KeyValues *pKeyValues )
{
	int nOldValue = GetAttributeValue<int>();
	int nValue = pKeyValues->GetInt( "value", 0 );
	if ( nOldValue == nValue )
		return;

	CUndoScopeGuard guard( NOTIFY_SOURCE_PROPERTIES_TREE, NOTIFY_SETDIRTYFLAG, "Set Attribute Value", "Set Attribute Value" );
	SetAttributeValue( nValue );
}


//-----------------------------------------------------------------------------
// Sets the combo box from the attribute 
//-----------------------------------------------------------------------------
void CAttributeIntChoicePanel::SetComboBoxFromAttribute( vgui::ComboBox *pComboBox )
{
	CDmeEditorIntChoicesInfo *pInfo = CastElement<CDmeEditorIntChoicesInfo>( GetEditorInfo() );
	if ( !pInfo )
		return;

	int nValue = GetAttributeValue<int>();
	int c = pInfo->GetChoiceCount();
	for ( int i = 0; i < c; ++i )
	{
		if ( nValue == pInfo->GetChoiceValue( i ) )
		{
			pComboBox->SetText( pInfo->GetChoiceString( i ) );
			return;
		}
	}

	// Check the dynamic choices next
	if ( pInfo->HasChoiceType() )
	{
		IntChoiceList_t choices;
		if ( ElementPropertiesChoices()->GetIntChoiceList( pInfo->GetChoiceType(), GetPanelElement(), GetAttributeName(), IsArrayEntry(), choices ) )
		{
			c = choices.Count();
			for ( int i = 0; i < c; ++i )
			{
				if ( nValue == choices[i].m_nValue )
				{
					pComboBox->SetText( choices[i].m_pChoiceString );
					return;
				}
			}
		}
	}

	pComboBox->SetText( "Unknown value" );
}
