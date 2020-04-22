//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeElementPanel.h"
#include "dme_controls/AttributeTextEntry.h"
#include "dme_controls/AttributeWidgetFactory.h"
#include "tier1/KeyValues.h"
#include "datamodel/dmelement.h"
#include "movieobjects/dmeeditortypedictionary.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;

// ----------------------------------------------------------------------------
CAttributeElementPanel::CAttributeElementPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info ), m_pData( 0 )
{
	m_pData = new CAttributeTextEntry( this, "AttributeValue" );
	m_pData->SetEnabled( !HasFlag( READONLY ) );
	m_pData->AddActionSignalTarget(this);
	m_pType->SetText( "element" );

	m_bShowMemoryUsage = info.m_bShowMemoryUsage;
}

void CAttributeElementPanel::Apply()
{
	// FIXME: Implement when needed
	Assert( 0 );
}

vgui::Panel *CAttributeElementPanel::GetDataPanel()
{
	return static_cast< vgui::Panel * >( m_pData );
}

void CAttributeElementPanel::OnCreateDragData( KeyValues *msg )
{
	if ( GetPanelElement() )
	{
		char txt[ 256 ];
		m_pData->GetText( txt, sizeof( txt ) );

		msg->SetString( "text", txt );
		CDmElement *element = NULL;
		if ( GetPanelElement()->HasAttribute( GetAttributeName() ) )
		{
			element = GetElement<CDmElement>( GetAttributeValue<DmElementHandle_t>( ) );
			msg->SetInt( "dmeelement", element->GetHandle() );
		}
		
	}
}

void CAttributeElementPanel::Refresh()
{
	char elemText[ 512 ];
	elemText[0] = 0;

	CDmElement *element = NULL;
	if ( !GetEditorInfo() || !GetEditorInfo()->GetValue<bool>( "hideText" ) )
	{
		if ( HasAttribute( ) )
		{
			element = GetAttributeValueElement( );
		}
		else
		{
			element = GetPanelElement();
		}
	}

	if ( element )
	{
		char idstr[ 37 ];
		UniqueIdToString( element->GetId(), idstr, sizeof( idstr ) );
		if ( m_bShowMemoryUsage )
		{
			Q_snprintf( elemText, sizeof( elemText ), "%s %s %.3fMB", element->GetTypeString(), idstr, element->EstimateMemoryUsage() / float( 1 << 20 ) );
		}
		else
		{
			Q_snprintf( elemText, sizeof( elemText ), "%s %s", element->GetTypeString(), idstr );
		}
	}

	m_pData->SetText( elemText );
	m_pData->SetEnabled(false);
}

void CAttributeElementPanel::PostConstructor()
{
	Refresh();
}

// ----------------------------------------------------------------------------
