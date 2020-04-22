//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeStringChoicePanel.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/ComboBox.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "dme_controls/inotifyui.h"
#include "dme_controls/dmecontrols.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Expose DmeEditorAttributeInfo to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeEditorStringChoicesInfo, CDmeEditorStringChoicesInfo );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeEditorStringChoicesInfo::OnConstruction()
{
}

void CDmeEditorStringChoicesInfo::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Add a choice
//-----------------------------------------------------------------------------
CDmElement *CDmeEditorStringChoicesInfo::AddChoice( const char *pValueString, const char *pChoiceString )
{
	CDmElement *pChoice = CreateChoice( pChoiceString );
	pChoice->SetValue<CUtlString>( "value", pValueString );
	return pChoice;
}


//-----------------------------------------------------------------------------
// Gets the choices
//-----------------------------------------------------------------------------
const char *CDmeEditorStringChoicesInfo::GetChoiceValue( int nIndex ) const
{
	Assert( ( nIndex < GetChoiceCount() ) && ( nIndex >= 0 ) );
	CDmElement *pChoice = m_Choices[nIndex];
	if ( !pChoice )
		return 0;

	return pChoice->GetValue<CUtlString>( "value" );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAttributeStringChoicePanel::CAttributeStringChoicePanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
}


//-----------------------------------------------------------------------------
// Derived classes can re-implement this to fill the combo box however they like
//-----------------------------------------------------------------------------
void CAttributeStringChoicePanel::PopulateComboBox( vgui::ComboBox *pComboBox )
{
	pComboBox->DeleteAllItems();

	CDmeEditorStringChoicesInfo *pInfo = CastElement<CDmeEditorStringChoicesInfo>( GetEditorInfo() );
	if ( !pInfo )
		return;

	// Fill in the standard choices first
	int c = pInfo->GetChoiceCount();
	for ( int i = 0; i < c; ++i )
	{
		KeyValues *kv = new KeyValues( "entry" );
		kv->SetString( "value", pInfo->GetChoiceValue( i ) );
		pComboBox->AddItem( pInfo->GetChoiceString( i ) , kv );
	}

	// Add the dynamic choices next
	if ( pInfo->HasChoiceType() )
	{
		StringChoiceList_t choices;
		if ( ElementPropertiesChoices()->GetStringChoiceList( pInfo->GetChoiceType(), GetPanelElement(), GetAttributeName(), IsArrayEntry(), choices ) )
		{
			c = choices.Count();
			for ( int i = 0; i < c; ++i )
			{
				KeyValues *kv = new KeyValues( "entry" );
				kv->SetString( "value", choices[i].m_pValue );
				pComboBox->AddItem( choices[i].m_pChoiceString, kv );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Sets the attribute based on the combo box
//-----------------------------------------------------------------------------
void CAttributeStringChoicePanel::SetAttributeFromComboBox( vgui::ComboBox *pComboBox, KeyValues *pKeyValues )
{
	const char *pOldString = GetAttributeValue<CUtlString>();
	const char *pNewString = pKeyValues->GetString( "value", "" );
	if ( pOldString == pNewString )
		return;

	CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, GetNotify(), "Set Attribute Value", "Set Attribute Value" );
	SetAttributeValue( pNewString );
}


//-----------------------------------------------------------------------------
// Sets the combo box from the attribute 
//-----------------------------------------------------------------------------
void CAttributeStringChoicePanel::SetComboBoxFromAttribute( vgui::ComboBox *pComboBox )
{
	CDmeEditorStringChoicesInfo *pInfo = CastElement<CDmeEditorStringChoicesInfo>( GetEditorInfo() );
	if ( !pInfo )
		return;

	const char *pValue = GetAttributeValue<CUtlString>();
	int c = pInfo->GetChoiceCount();
	for ( int i = 0; i < c; ++i )
	{
		if ( !Q_stricmp( pValue, pInfo->GetChoiceValue( i ) ) )
		{
			pComboBox->SetText( pInfo->GetChoiceString( i ) );
			return;
		}
	}

	// Check the dynamic choices next
	if ( pInfo->HasChoiceType() )
	{
		StringChoiceList_t choices;
		if ( ElementPropertiesChoices()->GetStringChoiceList( pInfo->GetChoiceType(), GetPanelElement(), GetAttributeName(), IsArrayEntry(), choices ) )
		{
			c = choices.Count();
			for ( int i = 0; i < c; ++i )
			{
				if ( !Q_stricmp( pValue, choices[i].m_pValue ) )
				{
					pComboBox->SetText( choices[i].m_pChoiceString );
					return;
				}
			}
		}
	}

	pComboBox->SetText( "Unknown value" );
}
