//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "OptionsSubPortal.h"
#include "CvarToggleCheckButton.h"
#include "vgui_controls/ComboBox.h"

#include "EngineInterface.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include "tier1/convar.h"
#include <stdio.h>
#include <vgui_controls/TextEntry.h>
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

COptionsSubPortal::COptionsSubPortal(vgui::Panel *parent) : PropertyPage(parent, NULL)
{
	m_pPortalFunnelCheckBox = new CCvarToggleCheckButton( 
		this, 
		"PortalFunnel", 
		"#GameUI_PortalFunnel", 
		"sv_player_funnel_into_portals" );

	m_pPortalDepthCombo = new ComboBox( this, "PortalDepth", 6, false );
	m_pPortalDepthCombo->AddItem( "#GameUI_PortalDepth0", new KeyValues("PortalDepth", "depth", 0) );
	m_pPortalDepthCombo->AddItem( "#GameUI_PortalDepth1", new KeyValues("PortalDepth", "depth", 1) );
	m_pPortalDepthCombo->AddItem( "#GameUI_PortalDepth2", new KeyValues("PortalDepth", "depth", 2) );
	m_pPortalDepthCombo->AddItem( "#GameUI_PortalDepth3", new KeyValues("PortalDepth", "depth", 3) );
	m_pPortalDepthCombo->AddItem( "#GameUI_PortalDepth4", new KeyValues("PortalDepth", "depth", 4) );
	m_pPortalDepthCombo->AddItem( "#GameUI_PortalDepth5", new KeyValues("PortalDepth", "depth", 5) );
	m_pPortalDepthCombo->AddItem( "#GameUI_PortalDepth6", new KeyValues("PortalDepth", "depth", 6) );
	m_pPortalDepthCombo->AddItem( "#GameUI_PortalDepth7", new KeyValues("PortalDepth", "depth", 7) );
	m_pPortalDepthCombo->AddItem( "#GameUI_PortalDepth8", new KeyValues("PortalDepth", "depth", 8) );
	m_pPortalDepthCombo->AddItem( "#GameUI_PortalDepth9", new KeyValues("PortalDepth", "depth", 9) );

	LoadControlSettings("Resource\\OptionsSubPortal.res");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptionsSubPortal::~COptionsSubPortal()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubPortal::OnResetData()
{
	m_pPortalFunnelCheckBox->Reset();

	// Portal render depth
	ConVarRef r_portal_stencil_depth("r_portal_stencil_depth");
	if ( r_portal_stencil_depth.IsValid() )
	{
		m_pPortalDepthCombo->ActivateItem(r_portal_stencil_depth.GetInt());
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubPortal::OnApplyChanges()
{
	m_pPortalFunnelCheckBox->ApplyChanges();

	// Portal render depth
	if ( m_pPortalDepthCombo->IsEnabled() )
	{
		ConVarRef r_portal_stencil_depth( "r_portal_stencil_depth" );
		r_portal_stencil_depth.SetValue( m_pPortalDepthCombo->GetActiveItem() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets background color & border
//-----------------------------------------------------------------------------
void COptionsSubPortal::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubPortal::OnControlModified()
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}
