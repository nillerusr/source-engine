//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cstrikeclassmenu.h"

#include <KeyValues.h>
#include <filesystem.h>
#include <vgui_controls/Button.h>
#include <vgui/IVGui.h>

#include "hud.h" // for gEngfuncs
#include "cs_gamerules.h"

using namespace vgui;


// ----------------------------------------------------------------------------- //
// Class image panels. These maintain a list of the class image panels so 
// it can render 3D images into them.
// ----------------------------------------------------------------------------- //

CUtlVector<CCSClassImagePanel*> g_ClassImagePanels;


CCSClassImagePanel::CCSClassImagePanel( vgui::Panel *pParent, const char *pName )
	: vgui::ImagePanel( pParent, pName )
{
	g_ClassImagePanels.AddToTail( this );
	m_ModelName[0] = 0;
}

CCSClassImagePanel::~CCSClassImagePanel()
{
	g_ClassImagePanels.FindAndRemove( this );
}

void CCSClassImagePanel::ApplySettings( KeyValues *inResourceData )
{
	const char *pName = inResourceData->GetString( "3DModel" );
	if ( pName )
	{
		Q_strncpy( m_ModelName, pName, sizeof( m_ModelName ) );
	}
	
	BaseClass::ApplySettings( inResourceData );
}


void CCSClassImagePanel::Paint()
{
	BaseClass::Paint();
}


// ----------------------------------------------------------------------------- //
// CClassMenu_TER
// ----------------------------------------------------------------------------- //

CClassMenu_TER::CClassMenu_TER(IViewPort *pViewPort) : CClassMenu(pViewPort, PANEL_CLASS_TER)
{
	LoadControlSettings( "Resource/UI/ClassMenu_TER.res" );
	CreateBackground( this );
	m_backgroundLayoutFinished = false;
}

const char *CClassMenu_TER::GetName( void ) 
{ 
	return PANEL_CLASS_TER; 
}

void CClassMenu_TER::ShowPanel(bool bShow)
{
	if ( bShow)
	{
		engine->CheckPoint( "ClassMenu" );
	}

	BaseClass::ShowPanel( bShow );

}

void CClassMenu_TER::SetVisible(bool state)
{
	BaseClass::SetVisible(state);

	if ( state )
	{
		Panel *pAutoButton = FindChildByName( "autoselect_t" );
		if ( pAutoButton )
		{
			pAutoButton->RequestFocus();
		}
	}
}

bool modelExists( const char *search, const CUtlVector< const char * > &names )
{
	for ( int i=0; i<names.Count(); ++i )
	{
		if ( Q_stristr( names[i], search ) != NULL )
		{
			return true;
		}
	}

	return false;
}

void CClassMenu_TER::Update()
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pLocalPlayer && pLocalPlayer->PlayerClass() >= FIRST_T_CLASS && pLocalPlayer->PlayerClass() <= LAST_T_CLASS )
	{
		SetVisibleButton( "CancelButton", true );
	}
	else
	{
		SetVisibleButton( "CancelButton", false ); 
	}

	// if we don't have the new models installed,
	// turn off the militia and spetsnaz buttons
	SetVisibleButton( "militia", false );
}


Panel *CClassMenu_TER::CreateControlByName(const char *controlName)
{
	if ( Q_stricmp( controlName, "CSClassImagePanel" ) == 0 )
	{
		return new CCSClassImagePanel( NULL, controlName );
	}

	return BaseClass::CreateControlByName( controlName );
}



// ----------------------------------------------------------------------------- //
// CClassMenu_CT
// ----------------------------------------------------------------------------- //

CClassMenu_CT::CClassMenu_CT(IViewPort *pViewPort) : CClassMenu(pViewPort, PANEL_CLASS_CT)
{
	LoadControlSettings( "Resource/UI/ClassMenu_CT.res" );
	CreateBackground( this );
	m_backgroundLayoutFinished = false;
}

Panel *CClassMenu_CT::CreateControlByName(const char *controlName)
{
	if ( Q_stricmp( controlName, "CSClassImagePanel" ) == 0 )
	{
		return new CCSClassImagePanel( NULL, controlName );
	}

	return BaseClass::CreateControlByName( controlName );
}

const char *CClassMenu_CT::GetName( void ) 
{ 
	return PANEL_CLASS_CT; 
}

void CClassMenu_CT::ShowPanel(bool bShow)
{
	if ( bShow)
	{
		engine->CheckPoint( "ClassMenu" );
	}

	BaseClass::ShowPanel( bShow );

}

void CClassMenu_CT::SetVisible(bool state)
{
	BaseClass::SetVisible(state);

	if ( state )
	{
		Panel *pAutoButton = FindChildByName( "autoselect_ct" );
		if ( pAutoButton )
		{
			pAutoButton->RequestFocus();
		}
	}
}

void CClassMenu_CT::Update()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pPlayer && pPlayer->PlayerClass() >= FIRST_CT_CLASS && pPlayer->PlayerClass() <= LAST_CT_CLASS )
	{
		SetVisibleButton( "CancelButton", true );
	}
	else
	{
		SetVisibleButton( "CancelButton", false ); 
	}

	// if we don't have the new models installed,
	// turn off the militia and spetsnaz buttons
	SetVisibleButton( "spetsnaz", false );
}


//-----------------------------------------------------------------------------
// Purpose: The CS background is painted by image panels, so we should do nothing
//-----------------------------------------------------------------------------
void CClassMenu_TER::PaintBackground()
{
}

//-----------------------------------------------------------------------------
// Purpose: Scale / center the window
//-----------------------------------------------------------------------------
void CClassMenu_TER::PerformLayout()
{
	BaseClass::PerformLayout();

	// stretch the window to fullscreen
	if ( !m_backgroundLayoutFinished )
		LayoutBackgroundPanel( this );
	m_backgroundLayoutFinished = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClassMenu_TER::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	ApplyBackgroundSchemeSettings( this, pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: The CS background is painted by image panels, so we should do nothing
//-----------------------------------------------------------------------------
void CClassMenu_CT::PaintBackground()
{
}

//-----------------------------------------------------------------------------
// Purpose: Scale / center the window
//-----------------------------------------------------------------------------
void CClassMenu_CT::PerformLayout()
{
	BaseClass::PerformLayout();

	// stretch the window to fullscreen
	if ( !m_backgroundLayoutFinished )
		LayoutBackgroundPanel( this );
	m_backgroundLayoutFinished = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClassMenu_CT::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	ApplyBackgroundSchemeSettings( this, pScheme );
}

