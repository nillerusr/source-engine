//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/Label.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "text_message.h"
#include "dod_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Displays current ammunition level
//-----------------------------------------------------------------------------
class CDODReadyRestartLabel : public vgui::Panel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CDODReadyRestartLabel, vgui::Panel );

public:
	CDODReadyRestartLabel( const char *pElementName );

	virtual void Reset();
	virtual void Init();

	virtual void PerformLayout();

	void FireGameEvent( IGameEvent * event);

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();

private:
	vgui::HFont m_hFont;
	Color		m_bgColor;

	vgui::Label *m_pRestartLabel;	// "Round will restart in 0:00"
	vgui::Label *m_pAlliesReady;	// "Allies are READY"
	vgui::Label *m_pAxisReady;		// "Axis are NOT READY"

	vgui::Label *m_pBackground;

	float m_flLastRestartTime;

	CPanelAnimationVarAliasType( int, m_iTextX, "text_xpos", "8", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iTextY, "text_ypos", "8", "proportional_int" );
};

DECLARE_HUDELEMENT( CDODReadyRestartLabel );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODReadyRestartLabel::CDODReadyRestartLabel( const char *pElementName ) : BaseClass(NULL, "ReadyRestartLabel"), CHudElement( pElementName )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	SetVisible( false );
	SetAlpha( 0 );

	m_pBackground = new vgui::Label( this, "Background", "" );

	wchar_t labelText[128];
	g_pVGuiLocalize->ConstructString( labelText, sizeof(labelText), g_pVGuiLocalize->Find( "#clan_game_restart" ), 2, L"0", L"00" );

	m_pRestartLabel = new vgui::Label( this, "RoundState_waiting", g_pVGuiLocalize->Find( "#Clan_awaiting_ready" ) );
	m_pRestartLabel->SetFgColor( GetFgColor() );
	m_pRestartLabel->SizeToContents();

	m_pAlliesReady = new vgui::Label( this, "RoundState_allies", L"" );
	m_pAlliesReady->SetFgColor( GetFgColor() );

	m_pAxisReady = new vgui::Label( this, "RoundState_axis", L"" );
	m_pAxisReady->SetFgColor( GetFgColor() );

	m_flLastRestartTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODReadyRestartLabel::Reset()
{
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "ReadyRestartLabelHide" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODReadyRestartLabel::Init()
{
	// listen for events
	ListenForGameEvent( "dod_round_start" );
	ListenForGameEvent( "dod_ready_restart" );
	ListenForGameEvent( "dod_allies_ready" );
	ListenForGameEvent( "dod_axis_ready" );

	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "ReadyRestartLabelHide" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODReadyRestartLabel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetFgColor( Color(0,0,0,0) );
	m_hFont = pScheme->GetFont( "HudHintText", true );

	m_pBackground->SetBgColor( GetSchemeColor("HintMessageBg", pScheme) );
	m_pBackground->SetPaintBackgroundType( 2 );

	SetAlpha( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Resizes the label
//-----------------------------------------------------------------------------
void CDODReadyRestartLabel::PerformLayout()
{
	BaseClass::PerformLayout();

	int wide, tall;
	GetSize( wide, tall );

	// find the widest line
	int labelWide = m_pRestartLabel->GetWide();

	// find the total height
	int fontTall = vgui::surface()->GetFontTall( m_hFont );

	labelWide += m_iTextX*2;
	int labelTall = fontTall*3 + m_iTextY*2;

	m_pBackground->SetBounds( 0, 0, labelWide, labelTall );

	int xOffset = (labelWide - m_pRestartLabel->GetWide())/2;

	m_pRestartLabel->SetPos( xOffset, m_iTextY );
	m_pAlliesReady->SetPos( xOffset, m_iTextY + fontTall );
	m_pAxisReady->SetPos( xOffset, m_iTextY + 2 *fontTall );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the label color each frame
//-----------------------------------------------------------------------------
void CDODReadyRestartLabel::OnThink()
{
	float flRoundRestartTime = DODGameRules()->GetRoundRestartTime();

	if( flRoundRestartTime != m_flLastRestartTime )
	{
		if( flRoundRestartTime > 0 )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "ReadyRestartLabelHide" );
		}
		m_flLastRestartTime = flRoundRestartTime;
	}

	m_pBackground->SetFgColor(GetFgColor());
	m_pRestartLabel->SetFgColor(GetFgColor());
	m_pAlliesReady->SetFgColor(GetFgColor());
	m_pAxisReady->SetFgColor(GetFgColor());
}

//-----------------------------------------------------------------------------
// Purpose: Activates the hint display upon receiving a hint
//-----------------------------------------------------------------------------
void CDODReadyRestartLabel::FireGameEvent( IGameEvent * event)
{
	if( Q_strcmp( "dod_round_start", event->GetName() ) == 0 )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "ReadyRestartLabelHide" );
	}
	else if( Q_strcmp( "dod_ready_restart", event->GetName() ) == 0 )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "ReadyRestartLabelShow" );

		m_pAlliesReady->SetText( g_pVGuiLocalize->Find( "#clan_allies_not_ready" ) );
		m_pAlliesReady->SizeToContents();

		m_pAxisReady->SetText( g_pVGuiLocalize->Find( "#clan_axis_not_ready" ) );
		m_pAxisReady->SizeToContents();
	}
	else if( Q_strcmp( "dod_allies_ready", event->GetName() ) == 0 )
	{
		m_pAlliesReady->SetText( g_pVGuiLocalize->Find( "#clan_allies_ready" ) );
		m_pAlliesReady->SizeToContents();
	}
	else if( Q_strcmp( "dod_axis_ready", event->GetName() ) == 0 )
	{
		m_pAxisReady->SetText( g_pVGuiLocalize->Find( "#clan_axis_ready" ) );
		m_pAxisReady->SizeToContents();
	}
}