//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "client_pch.h"

#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>

#include <vgui_controls/BuildGroup.h>
#include <vgui_controls/Tooltip.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/TextEntry.h>
#include <vgui/IInput.h>

#include "cl_foguipanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// CFogUIPanel
//-----------------------------------------------------------------------------

CFogUIPanel *g_pFogUI = NULL;

void CFogUIPanel::InstallFogUI( vgui::Panel *parent )
{
	if ( g_pFogUI )
		return;

	g_pFogUI = new CFogUIPanel( parent );
	Assert( g_pFogUI );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFogUIPanel::CFogUIPanel( vgui::Panel *parent ) : vgui::Frame( parent, "FogUIPanel" )
{
	SetTitle( "FogUI", true );

	m_pFogOverride = new vgui::CheckButton( this, "FogOverride", "FogOverride" );

	//
	// World
	//
	m_pFogEnable = new vgui::CheckButton( this, "FogEnable", "FogEnable" );
	m_pFogEnable->SetSelected( true );
	// FogStart
	m_pFogStart = new vgui::Slider( this, "FogStart" );
	m_pFogStart->SetRange( -10000, 30000 );
	m_pFogStart->SetValue( 1 );
	m_pFogStart->AddActionSignalTarget( this );
	m_pFogStart->SetDragOnRepositionNob( true );
	m_pFogStartText = new vgui::TextEntry( this, "FogStartText" );
	m_pFogStartText->SendNewLine( true );
	m_pFogStartText->SetCatchEnterKey( true );
	// FogEnd
	m_pFogEnd = new vgui::Slider( this, "FogEnd" );
	m_pFogEnd->SetRange( -10000, 30000  );
	m_pFogEnd->SetValue( 1 );
	m_pFogEnd->AddActionSignalTarget( this );
	m_pFogEnd->SetDragOnRepositionNob( true );
	m_pFogEndText = new vgui::TextEntry( this, "FogEndText" );
	m_pFogEndText->SendNewLine( true );
	m_pFogEndText->SetCatchEnterKey( true );
	// Red
	m_pFogColorRed = new vgui::Slider( this, "FogColorRed" );
	m_pFogColorRed->SetRange( 1, 255 );
	m_pFogColorRed->SetValue( 1 );
	m_pFogColorRed->AddActionSignalTarget( this );
	m_pFogColorRed->SetDragOnRepositionNob( true );
	m_pFogColorRedText = new vgui::TextEntry( this, "FogColorRedText" );
	m_pFogColorRedText->SendNewLine( true );
	m_pFogColorRedText->SetCatchEnterKey( true );
	// Green
	m_pFogColorGreen = new vgui::Slider( this, "FogColorGreen" );
	m_pFogColorGreen->SetRange( 1, 255 );
	m_pFogColorGreen->SetValue( 1 );
	m_pFogColorGreen->AddActionSignalTarget( this );
	m_pFogColorGreen->SetDragOnRepositionNob( true );
	m_pFogColorGreenText = new vgui::TextEntry( this, "FogColorGreenText" );
	m_pFogColorGreenText->SendNewLine( true );
	m_pFogColorGreenText->SetCatchEnterKey( true );
	// Blue
	m_pFogColorBlue = new vgui::Slider( this, "FogColorBlue" );
	m_pFogColorBlue->SetRange( 1, 255 );
	m_pFogColorBlue->SetValue( 1 );
	m_pFogColorBlue->AddActionSignalTarget( this );
	m_pFogColorBlue->SetDragOnRepositionNob( true );
	m_pFogColorBlueText = new vgui::TextEntry( this, "FogColorBlueText" );
	m_pFogColorBlueText->SendNewLine( true );
	m_pFogColorBlueText->SetCatchEnterKey( true );

	//
	// Skybox
	//
	m_pFogEnableSky = new vgui::CheckButton( this, "FogEnableSky", "FogEnableSky" );
	m_pFogEnableSky->SetSelected( true );
	// Start
	m_pFogStartSky = new vgui::Slider( this, "FogStartSky" );
	m_pFogStartSky->SetRange( -10000, 30000  );
	m_pFogStartSky->SetValue( 1 );
	m_pFogStartSky->AddActionSignalTarget( this );
	m_pFogStartSky->SetDragOnRepositionNob( true );
	m_pFogStartTextSky = new vgui::TextEntry( this, "FogStartTextSky" );
	m_pFogStartTextSky->SendNewLine( true );
	m_pFogStartTextSky->SetCatchEnterKey( true );
	// End
	m_pFogEndSky = new vgui::Slider( this, "FogEndSky" );
	m_pFogEndSky->SetRange( -10000, 30000  );
	m_pFogEndSky->SetValue( 1 );
	m_pFogEndSky->AddActionSignalTarget( this );
	m_pFogEndSky->SetDragOnRepositionNob( true );
	m_pFogEndTextSky = new vgui::TextEntry( this, "FogEndTextSky" );
	m_pFogEndTextSky->SendNewLine( true );
	m_pFogEndTextSky->SetCatchEnterKey( true );
	// Red
	m_pFogColorRedSky = new vgui::Slider( this, "FogColorRedSky" );
	m_pFogColorRedSky->SetRange( 1, 255 );
	m_pFogColorRedSky->SetValue( 1 );
	m_pFogColorRedSky->AddActionSignalTarget( this );
	m_pFogColorRedSky->SetDragOnRepositionNob( true );
	m_pFogColorRedTextSky = new vgui::TextEntry( this, "FogColorRedTextSky" );
	m_pFogColorRedTextSky->SendNewLine( true );
	m_pFogColorRedTextSky->SetCatchEnterKey( true );
	// Green
	m_pFogColorGreenSky = new vgui::Slider( this, "FogColorGreenSky" );
	m_pFogColorGreenSky->SetRange( 1, 255 );
	m_pFogColorGreenSky->SetValue( 1 );
	m_pFogColorGreenSky->AddActionSignalTarget( this );
	m_pFogColorGreenSky->SetDragOnRepositionNob( true );
	m_pFogColorGreenTextSky = new vgui::TextEntry( this, "FogColorGreenTextSky" );
	m_pFogColorGreenTextSky->SendNewLine( true );
	m_pFogColorGreenTextSky->SetCatchEnterKey( true );
	// Blue
	m_pFogColorBlueSky = new vgui::Slider( this, "FogColorBlueSky" );
	m_pFogColorBlueSky->SetRange( 1, 255 );
	m_pFogColorBlueSky->SetValue( 1 );
	m_pFogColorBlueSky->AddActionSignalTarget( this );
	m_pFogColorBlueSky->SetDragOnRepositionNob( true );
	m_pFogColorBlueTextSky = new vgui::TextEntry( this, "FogColorBlueTextSky" );
	m_pFogColorBlueTextSky->SendNewLine( true );
	m_pFogColorBlueTextSky->SetCatchEnterKey( true );

	//
	// Far Z
	//
	m_pFarZOverride = new vgui::CheckButton( this, "FarZOverride", "FarZOverride" );
	m_pFarZOverride->SetSelected( false );
	m_pFarZ = new vgui::Slider( this, "FarZ" );
	m_pFarZ->SetRange( 1, 30000 );
	m_pFarZ->SetValue( 30000 );
	m_pFarZ->AddActionSignalTarget( this );
	m_pFarZ->SetDragOnRepositionNob( true );
	m_pFarZText = new vgui::TextEntry( this, "FarZText" );
	m_pFarZText->SendNewLine( true );
	m_pFarZText->SetCatchEnterKey( true );
	m_pFarZText->SetText( va( "%i", m_pFarZ->GetValue() ) );

	// TODO:
	// - fog_maxdensity
	// - fog_maxdensityskybox
	// - fog_fogvolue
	// - r_pixelfog?

	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );

	LoadControlSettings("Resource\\FogUIPanel.res");

	SetVisible( false );
	SetSizeable( false );
	SetMoveable( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFogUIPanel::~CFogUIPanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFogUIPanel::OnTick()
{
	BaseClass::OnTick();

	if ( !IsVisible() )
		return;

	bool bEnabled = m_pFogOverride->IsSelected();

	bool bFogEnabled = ( m_pFogEnable->IsSelected() && m_pFogOverride->IsSelected() );
	m_pFogEnable->SetEnabled( bEnabled );
	m_pFogStart->SetEnabled( bFogEnabled );
	m_pFogEnd->SetEnabled( bFogEnabled);  
	m_pFogStartText->SetEnabled( bFogEnabled );
	m_pFogEndText->SetEnabled( bFogEnabled );
	m_pFogColorRed->SetEnabled( bFogEnabled );
	m_pFogColorRedText->SetEnabled( bFogEnabled );
	m_pFogColorGreen->SetEnabled( bFogEnabled );
	m_pFogColorGreenText->SetEnabled( bFogEnabled );
	m_pFogColorBlue->SetEnabled( bFogEnabled );
	m_pFogColorBlueText->SetEnabled( bFogEnabled );

	bool bFogSkyEnabled = ( m_pFogEnableSky->IsSelected() && m_pFogOverride->IsSelected() );
	m_pFogEnableSky->SetEnabled( bEnabled );
	m_pFogStartSky->SetEnabled( bFogSkyEnabled );
	m_pFogEndSky->SetEnabled( bFogSkyEnabled);  
	m_pFogStartTextSky->SetEnabled( bFogSkyEnabled );
	m_pFogEndTextSky->SetEnabled( bFogSkyEnabled );
	m_pFogColorRedSky->SetEnabled( bFogSkyEnabled);
	m_pFogColorRedTextSky->SetEnabled( bFogSkyEnabled);
	m_pFogColorGreenSky->SetEnabled( bFogSkyEnabled );
	m_pFogColorGreenTextSky->SetEnabled( bFogSkyEnabled );
	m_pFogColorBlueSky->SetEnabled( bFogSkyEnabled );
	m_pFogColorBlueTextSky->SetEnabled( bFogSkyEnabled );

	bool bFarZEnabled = ( m_pFarZOverride->IsSelected() && m_pFogOverride->IsSelected() );
	m_pFarZOverride->SetEnabled( bEnabled );
	m_pFarZ->SetEnabled( bFarZEnabled );
	m_pFarZText->SetEnabled( bFarZEnabled );
}

//-----------------------------------------------------------------------------
// Purpose: Commands
//-----------------------------------------------------------------------------
void CFogUIPanel::OnCommand( const char *command )
{
	if ( !Q_strcasecmp( command, "FogOverride" ) )
	{
		if ( m_pFogOverride->IsSelected() == true )
		{
			Cbuf_AddText( "fog_override 1\n" );
		}
		else
		{
			Cbuf_AddText( "fog_override 0\n" );
		}
	}
	else if ( !Q_strcasecmp( command, "FogEnable" ) )
	{
		if ( m_pFogEnable->IsSelected() == true )
		{
			Cbuf_AddText( "fog_enable 1\n" );
		}
		else
		{
			Cbuf_AddText( "fog_enable 0\n" );
		}
	}
	else if ( !Q_strcasecmp( command, "FogEnableSky" ) )
	{
		if ( m_pFogEnableSky->IsSelected() == true )
		{
			Cbuf_AddText( "fog_enableskybox 1\n" );
		}
		else
		{
			Cbuf_AddText( "fog_enableskybox 0\n" );
		}
	}
	else if ( !Q_strcasecmp( command, "FarZOverride" ) )
	{
		if ( m_pFarZOverride->IsSelected() == true )
		{
			//m_pFarZText->SetText( va( "%i", m_pFarZ->GetValue() ) );
			Cbuf_AddText( va( "r_farz %i\n", m_pFarZ->GetValue() ) );
		}
		else
		{
			Cbuf_AddText( "r_farz -1\n" );
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Text
//-----------------------------------------------------------------------------
void CFogUIPanel::OnTextNewLine( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr("panel") );
	if ( !pPanel )
	{
		return;
	}

	vgui::TextEntry *pTextEntry = dynamic_cast<vgui::TextEntry *>( pPanel );
	if ( !pTextEntry )
	{
		return;
	}

	// World
	if ( pTextEntry == m_pFogStartText || pTextEntry == m_pFogStartTextSky ) 
	{
		UpdateFogStartSlider();
		return;
	}
	if ( pTextEntry == m_pFogEndText || pTextEntry == m_pFogEndTextSky ) 
	{
		UpdateFogEndSlider();
		return;
	}
	if ( pTextEntry == m_pFogColorRedText || pTextEntry == m_pFogColorRedTextSky ) 
	{
		UpdateFogColorRedSlider();
		return;
	}
	if ( pTextEntry == m_pFogColorGreenText || pTextEntry == m_pFogColorGreenTextSky ) 
	{
		UpdateFogColorGreenSlider();
		return;
	}
	if ( pTextEntry == m_pFogColorBlueText || pTextEntry == m_pFogColorBlueTextSky ) 
	{
		UpdateFogColorBlueSlider();
		return;
	}
	if ( pTextEntry == m_pFarZText ) 
	{
		UpdateFarZSlider();
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Text
//-----------------------------------------------------------------------------
void CFogUIPanel::OnTextKillFocus( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr("panel") );
	if ( !pPanel )
	{
		return;
	}
	
	vgui::TextEntry *pTextEntry = dynamic_cast<vgui::TextEntry *>( pPanel );
	if ( !pTextEntry )
	{
		return;
	}

	// World
	if ( pTextEntry == m_pFogStartText || pTextEntry == m_pFogStartTextSky ) 
	{
		UpdateFogStartSlider();
		return;
	}
	if ( pTextEntry == m_pFogEndText || pTextEntry == m_pFogEndTextSky ) 
	{
		UpdateFogEndSlider();
		return;
	}
	if ( pTextEntry == m_pFogColorRedText || pTextEntry == m_pFogColorRedTextSky ) 
	{
		UpdateFogColorRedSlider();
		return;
	}
	if ( pTextEntry == m_pFogColorGreenText || pTextEntry == m_pFogColorGreenTextSky ) 
	{
		UpdateFogColorGreenSlider();
		return;
	}
	if ( pTextEntry == m_pFogColorBlueText || pTextEntry == m_pFogColorBlueTextSky ) 
	{
		UpdateFogColorBlueSlider();
		return;
	}
	if ( pTextEntry == m_pFarZText ) 
	{
		UpdateFarZSlider();
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Messages
//-----------------------------------------------------------------------------
void CFogUIPanel::OnMessage( const KeyValues *params, VPANEL fromPanel )
{
	BaseClass::OnMessage( params, fromPanel );
	
	if ( !Q_strcmp( "SliderMoved", params->GetName() ) )
	{
		// World
		if ( fromPanel == m_pFogStart->GetVPanel() )
		{
			Cbuf_AddText( va( "fog_start %i\n", m_pFogStart->GetValue() ) );
			m_pFogStartText->SetText( va( "%i", m_pFogStart->GetValue() ) );
		}
		if ( fromPanel == m_pFogEnd->GetVPanel() )
		{
			Cbuf_AddText( va( "fog_end %i\n", m_pFogEnd->GetValue() ) );
			m_pFogEndText->SetText( va( "%i", m_pFogEnd->GetValue() ) );
		}
		if ( fromPanel == m_pFogColorRed->GetVPanel() )
		{
			m_pFogColorRedText->SetText( va( "%i", m_pFogColorRed->GetValue() ) );
			UpdateFogColors();
		}
		if ( fromPanel == m_pFogColorGreen->GetVPanel() )
		{
			m_pFogColorGreenText->SetText( va( "%i", m_pFogColorGreen->GetValue() ) );
			UpdateFogColors();
		}
		if ( fromPanel == m_pFogColorBlue->GetVPanel() )
		{
			m_pFogColorBlueText->SetText( va( "%i", m_pFogColorBlue->GetValue() ) );
			UpdateFogColors();
		}
		// Skybox
		if ( fromPanel == m_pFogStartSky->GetVPanel() )
		{
			Cbuf_AddText( va( "fog_startskybox %i\n", m_pFogStartSky->GetValue() ) );
			m_pFogStartTextSky->SetText( va( "%i", m_pFogStartSky->GetValue() ) );
		}
		if ( fromPanel == m_pFogEndSky->GetVPanel() )
		{
			Cbuf_AddText( va( "fog_endskybox %i\n", m_pFogEndSky->GetValue() ) );
			m_pFogEndTextSky->SetText( va( "%i", m_pFogEndSky->GetValue() ) );
		}
		if ( fromPanel == m_pFogColorRedSky->GetVPanel() )
		{
			m_pFogColorRedTextSky->SetText( va( "%i", m_pFogColorRedSky->GetValue() ) );
			UpdateFogColors();
		}
		if ( fromPanel == m_pFogColorGreenSky->GetVPanel() )
		{
			m_pFogColorGreenTextSky->SetText( va( "%i", m_pFogColorGreenSky->GetValue() ) );
			UpdateFogColors();
		}
		if ( fromPanel == m_pFogColorBlueSky->GetVPanel() )
		{
			m_pFogColorBlueTextSky->SetText( va( "%i", m_pFogColorBlueSky->GetValue() ) );
			UpdateFogColors();
		}
		// FarZ
		if ( fromPanel == m_pFarZ->GetVPanel() )
		{
			Cbuf_AddText( va( "r_farz %i\n", m_pFarZ->GetValue() ) );
			m_pFarZText->SetText( va( "%i", m_pFarZ->GetValue() ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update Sliders
//-----------------------------------------------------------------------------
void CFogUIPanel::UpdateFogStartSlider()
{
	m_pFogStart->SetValue( m_pFogStartText->GetValueAsInt() );
	m_pFogStartSky->SetValue( m_pFogStartTextSky->GetValueAsInt() );
}
void CFogUIPanel::UpdateFogEndSlider()
{
	m_pFogEnd->SetValue( m_pFogEndText->GetValueAsInt() );
	m_pFogEndSky->SetValue( m_pFogEndTextSky->GetValueAsInt() );
}
void CFogUIPanel::UpdateFogColorRedSlider()
{
	m_pFogColorRed->SetValue( m_pFogColorRedText->GetValueAsInt() );
	m_pFogColorRedSky->SetValue( m_pFogColorRedTextSky->GetValueAsInt() );
}
void CFogUIPanel::UpdateFogColorGreenSlider()
{
	m_pFogColorGreen->SetValue( m_pFogColorGreenText->GetValueAsInt() );
	m_pFogColorGreenSky->SetValue( m_pFogColorGreenTextSky->GetValueAsInt() );
}
void CFogUIPanel::UpdateFogColorBlueSlider()
{
	m_pFogColorBlue->SetValue( m_pFogColorBlueText->GetValueAsInt() );
	m_pFogColorBlueSky->SetValue( m_pFogColorBlueTextSky->GetValueAsInt() );
}
void CFogUIPanel::UpdateFarZSlider()
{
	m_pFarZ->SetValue( m_pFarZText->GetValueAsInt() );
}

//-----------------------------------------------------------------------------
// Purpose: Update Colors
//-----------------------------------------------------------------------------
void CFogUIPanel::UpdateFogColors()
{
	int iRed = m_pFogColorRed->GetValue();
	int iGreen = m_pFogColorGreen->GetValue();
	int iBlue = m_pFogColorBlue->GetValue();
	Cbuf_AddText( va( "fog_color %i %i %i\n", iRed, iGreen, iBlue ) );
	int iRedSky = m_pFogColorRedSky->GetValue();
	int iGreenSky = m_pFogColorGreenSky->GetValue();
	int iBlueSky = m_pFogColorBlueSky->GetValue();
	Cbuf_AddText( va( "fog_colorskybox %i %i %i\n", iRedSky, iGreenSky, iBlueSky ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void FogUI_f()
{
	if ( !g_pFogUI )
		return;

	if ( g_pFogUI->IsVisible() )
	{
		g_pFogUI->Close();
	}
	else
	{
		g_pFogUI->Activate();
	}
}

static ConCommand fogui( "fogui", FogUI_f, "Show/hide fog control UI.", FCVAR_DONTRECORD );