//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeBasePickerPanel.h"
#include "filesystem.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/FileOpenDialog.h"
#include "dme_controls/AttributeTextEntry.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAttributeBasePickerPanel::CAttributeBasePickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
	m_pOpen = new vgui::Button( this, "Open", "...", this, "open" );
}

void CAttributeBasePickerPanel::OnCommand( char const *cmd )
{
	if ( !Q_stricmp( cmd, "open" ) )
	{
		ShowPickerDialog();
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}

void CAttributeBasePickerPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, w, h;
	m_pType->GetBounds( x, y, w, h );

	int inset = 25;
	m_pType->SetWide( w - inset );

	x += w;
	x -= inset;

	h -= 2;

	m_pOpen->SetBounds( x, y, inset, h );
}
