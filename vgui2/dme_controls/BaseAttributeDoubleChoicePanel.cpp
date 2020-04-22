//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/BaseAttributeDoubleChoicePanel.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/ComboBox.h"
#include "datamodel/dmelement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;

CDoubleComboBoxContainerPanel::CDoubleComboBoxContainerPanel( vgui::Panel *parent, char const *name ) :
	BaseClass( parent, name )
{
	m_pBoxes[ 0 ] = m_pBoxes[ 1 ] = NULL;
}

void CDoubleComboBoxContainerPanel::AddComboBox( int slot, vgui::ComboBox *box )
{
	m_pBoxes[ slot ] = box;
}

void CDoubleComboBoxContainerPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	int w, h;
	GetSize( w, h );

	if ( m_pBoxes[ 0 ] )
	{
		m_pBoxes[ 0 ]->SetBounds( 0, 0, w/2, h );
	}
	if ( m_pBoxes[ 1 ] )
	{
		m_pBoxes[ 1 ]->SetBounds( w/2, 0, w/2, h );
	}
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CBaseAttributeDoubleChoicePanel::CBaseAttributeDoubleChoicePanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
	SetDropEnabled( false );

	m_pContainerPanel = new CDoubleComboBoxContainerPanel( this, "Container" );
	m_pData[0] = new vgui::ComboBox( m_pContainerPanel, "AttributeValue", 10, false );
	m_pData[0]->SetEnabled( !HasFlag( FATTRIB_READONLY ) );
	m_pData[0]->AddActionSignalTarget( this );
	m_pContainerPanel->AddComboBox( 0, m_pData[ 0 ] );
	m_pData[1] = new vgui::ComboBox( m_pContainerPanel, "AttributeValue", 10, false );
	m_pData[1]->SetEnabled( !HasFlag( FATTRIB_READONLY ) );
	m_pData[1]->AddActionSignalTarget( this );
	m_pContainerPanel->AddComboBox( 1, m_pData[ 1 ] );
}


//-----------------------------------------------------------------------------
// Called after the constructor is finished
//-----------------------------------------------------------------------------
void CBaseAttributeDoubleChoicePanel::PostConstructor()
{
	BaseClass::PostConstructor();
	PopulateComboBoxes( m_pData );
	Refresh();
}


void CBaseAttributeDoubleChoicePanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	HFont font = pScheme->GetFont( "DmePropertyVerySmall", IsProportional() );
	m_pData[0]->SetFont(font);
	m_pData[1]->SetFont(font);
}

vgui::Panel *CBaseAttributeDoubleChoicePanel::GetDataPanel()
{
	return static_cast< vgui::Panel * >( m_pContainerPanel );
}


//-----------------------------------------------------------------------------
// Called when it is time to set the attribute from the combo box state
//-----------------------------------------------------------------------------
void CBaseAttributeDoubleChoicePanel::Apply( )
{
	Assert( m_pData[ 0 ] && m_pData[ 1 ] );

	KeyValues *kv[ 2 ];
	kv[ 0 ] = m_pData[ 0 ]->GetActiveItemUserData();
	kv[ 1 ] = m_pData[ 1 ]->GetActiveItemUserData();

	SetAttributeFromComboBoxes( m_pData, kv );
}


//-----------------------------------------------------------------------------
// Called when it is time to set the combo box from the attribute 
//-----------------------------------------------------------------------------
void CBaseAttributeDoubleChoicePanel::Refresh()
{
	SetComboBoxesFromAttribute( m_pData );
}


//-----------------------------------------------------------------------------
// Called when the text in the panel changes 
//-----------------------------------------------------------------------------
void CBaseAttributeDoubleChoicePanel::OnTextChanged( Panel *panel )
{
	SetDirty(true);
	if ( IsAutoApply() )
	{
		Apply();
	}
}

