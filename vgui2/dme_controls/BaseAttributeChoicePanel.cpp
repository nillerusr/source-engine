//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/BaseAttributeChoicePanel.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/ComboBox.h"
#include "datamodel/dmelement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CBaseAttributeChoicePanel::CBaseAttributeChoicePanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info ), m_pData( 0 )
{
	SetDropEnabled( false );
	m_pData = new vgui::ComboBox( this, "AttributeValue", 10, false );
	m_pData->SetEnabled( !HasFlag( FATTRIB_READONLY ) );
	m_pData->AddActionSignalTarget( this );
}


//-----------------------------------------------------------------------------
// Called after the constructor is finished
//-----------------------------------------------------------------------------
void CBaseAttributeChoicePanel::PostConstructor()
{
	BaseClass::PostConstructor();
	PopulateComboBox( m_pData );
	Refresh();
}


void CBaseAttributeChoicePanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	HFont font = pScheme->GetFont( "DmePropertyVerySmall", IsProportional() );
	m_pData->SetFont(font);
}

vgui::Panel *CBaseAttributeChoicePanel::GetDataPanel()
{
	return static_cast< vgui::Panel * >( m_pData );
}


//-----------------------------------------------------------------------------
// Called when it is time to set the attribute from the combo box state
//-----------------------------------------------------------------------------
void CBaseAttributeChoicePanel::Apply( )
{
	KeyValues *kv = m_pData->GetActiveItemUserData();
	SetAttributeFromComboBox( m_pData, kv );
}


//-----------------------------------------------------------------------------
// Called when it is time to set the combo box from the attribute 
//-----------------------------------------------------------------------------
void CBaseAttributeChoicePanel::Refresh()
{
	SetComboBoxFromAttribute( m_pData );
}


//-----------------------------------------------------------------------------
// Called when the text in the panel changes 
//-----------------------------------------------------------------------------
void CBaseAttributeChoicePanel::OnTextChanged( Panel *panel )
{
	if ( IsAutoApply() )
	{
		Apply();
	}
	else
	{
		SetDirty(true);
	}
}

