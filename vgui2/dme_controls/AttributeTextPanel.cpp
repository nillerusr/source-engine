//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeTextPanel.h"
#include "dme_controls/AttributeTextEntry.h"
#include "dme_controls/AttributeWidgetFactory.h"
#include "tier1/KeyValues.h"
#include "datamodel/dmelement.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "dme_controls/inotifyui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// CAttributeTextPanel constructor
//-----------------------------------------------------------------------------
CAttributeTextPanel::CAttributeTextPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info ), m_pData( 0 ), m_bShowMemoryUsage( info.m_bShowMemoryUsage )
{
	m_pData = new CAttributeTextEntry( this, "AttributeValue" );
	m_pData->SetEnabled( !HasFlag( READONLY ) );

	m_pData->AddActionSignalTarget(this);
	SetAllowKeyBindingChainToParent( false );
}

void CAttributeTextPanel::SetFont( HFont font )
{
	BaseClass::SetFont( font );
	m_pData->SetFont( font );
}

//-----------------------------------------------------------------------------
// Returns the text type
//-----------------------------------------------------------------------------
const char *CAttributeTextPanel::GetTextType()
{
	// If a specific text type is specified, then filter if it doesn't match
	CDmeEditorAttributeInfo *pInfo = GetEditorInfo();
	const char *pTextType = pInfo ? pInfo->GetValueString( "texttype" ) : NULL;
	return pTextType ? pTextType : "";
}


void CAttributeTextPanel::Apply()
{
	char txt[ 256 ];
	m_pData->GetText( txt, sizeof( txt ) );

	// Apply means we no longer look blue
	SetDirty( false );

	if ( GetAttributeType( ) == AT_UNKNOWN )
	{
		CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, GetNotify(), "Set Attribute Value", "Set Attribute Value" );
		SetAttributeValue( "" );
		return;
	}

	char curvalue[ 256 ];
	GetAttributeValueAsString( curvalue, sizeof( curvalue ) );

	// Only if differnt
	if ( Q_strcmp( curvalue, txt ) )
	{
		CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, GetNotify(), "Set Attribute Value", "Set Attribute Value" );
		SetAttributeValueFromString( txt );
	}
}

vgui::Panel *CAttributeTextPanel::GetDataPanel()
{
	return static_cast< vgui::Panel * >( m_pData );
}

void CAttributeTextPanel::Refresh()
{
	char buf[ 512 ];
	if ( IsArrayType( GetAttributeType() ) )
	{
		int count = GetAttributeArrayCount();
		if ( m_bShowMemoryUsage )
		{
			CDmAttribute *pAttr = GetPanelElement()->GetAttribute( GetAttributeName() );
			Q_snprintf( buf, sizeof( buf ), "%d items %.3fMB", count, pAttr->EstimateMemoryUsage( TD_DEEP )  / float( 1 << 20 ) );
		}
		else
		{
			Q_snprintf( buf, sizeof( buf ), "%d items", count );
		}
		m_pData->SetText( buf );
		m_pData->SetEnabled(false);
	}
	else if ( GetAttributeType() == AT_ELEMENT )
	{
		m_pData->SetText( "" );
	}
	else
	{
		GetAttributeValueAsString( buf, sizeof( buf ) );
		m_pData->SetText( buf );
	}
}

void CAttributeTextPanel::PostConstructor()
{
	Refresh();
}

