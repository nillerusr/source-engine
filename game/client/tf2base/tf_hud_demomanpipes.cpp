//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "c_tf_player.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/EditablePanel.h>
//#include <vgui_controls/ProgressBar.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudDemomanPipes : public CHudElement, public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CHudDemomanPipes, EditablePanel );

public:
	CHudDemomanPipes( const char *pElementName );

	virtual void	ApplySchemeSettings( IScheme *scheme );
	virtual bool	ShouldDraw( void );
	virtual void	OnTick( void );

private:
	vgui::EditablePanel *m_pPipesPresent;
	vgui::EditablePanel *m_pNoPipesPresent;
};

DECLARE_HUDELEMENT( CHudDemomanPipes );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudDemomanPipes::CHudDemomanPipes( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudDemomanPipes" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_pPipesPresent = new EditablePanel( this, "PipesPresentPanel" );
	m_pNoPipesPresent = new EditablePanel( this, "NoPipesPresentPanel" );

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDemomanPipes::ApplySchemeSettings( IScheme *pScheme )
{
	// load control settings...
	LoadControlSettings( "resource/UI/HudDemomanPipes.res" );

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudDemomanPipes::ShouldDraw( void )
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pPlayer || !pPlayer->IsPlayerClass( TF_CLASS_DEMOMAN ) )
	{
		return false;
	}

	if ( !pPlayer->IsAlive() )
	{
		return false;
	}

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDemomanPipes::OnTick( void )
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pPlayer )
		return;

	int iPipes = pPlayer->GetNumActivePipebombs();

	m_pPipesPresent->SetDialogVariable( "activepipes", iPipes );
	m_pNoPipesPresent->SetDialogVariable( "activepipes", iPipes );

	m_pPipesPresent->SetVisible( iPipes > 0 );
	m_pNoPipesPresent->SetVisible( iPipes <= 0 );
}