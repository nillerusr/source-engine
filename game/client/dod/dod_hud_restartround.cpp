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
class CDODRestartRoundLabel : public vgui::Panel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CDODRestartRoundLabel, vgui::Panel );

public:
	CDODRestartRoundLabel( const char *pElementName );
	virtual void Reset();

	virtual void PerformLayout();

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();

private:
	vgui::HFont m_hFont;
	Color		m_bgColor;

	vgui::Label *m_pRestartLabel;	// "Round will restart in 0:00"

	vgui::Label *m_pBackground;

	CPanelAnimationVarAliasType( int, m_iTextX, "text_xpos", "8", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iTextY, "text_ypos", "8", "proportional_int" );

	float m_flLastRestartTime;
};

DECLARE_HUDELEMENT( CDODRestartRoundLabel );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODRestartRoundLabel::CDODRestartRoundLabel( const char *pElementName ) : BaseClass(NULL, "RestartRoundLabel"), CHudElement( pElementName )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	SetVisible( false );
	SetAlpha( 0 );

	m_pBackground = new vgui::Label( this, "Background", "" );

	wchar_t labelText[128];
	g_pVGuiLocalize->ConstructString( labelText, sizeof(labelText), g_pVGuiLocalize->Find( "#clan_game_restart" ), 2, L"0", L"00" );

	m_pRestartLabel = new vgui::Label( this, "restartroundlabel", labelText );
	m_pRestartLabel->SetPaintBackgroundEnabled( false );
	m_pRestartLabel->SetPaintBorderEnabled( false );
	m_pRestartLabel->SizeToContents();
	m_pRestartLabel->SetContentAlignment( vgui::Label::a_west );
	m_pRestartLabel->SetFgColor( GetFgColor() );

	m_flLastRestartTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODRestartRoundLabel::Reset()
{
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "RestartRoundLabelHide" );

	m_flLastRestartTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODRestartRoundLabel::ApplySchemeSettings( vgui::IScheme *pScheme )
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
void CDODRestartRoundLabel::PerformLayout()
{
	BaseClass::PerformLayout();

	int wide, tall;
	GetSize( wide, tall );

	// find the widest line
	int labelWide = m_pRestartLabel->GetWide();

	// find the total height
	int fontTall = vgui::surface()->GetFontTall( m_hFont );
	int labelTall = fontTall;

	labelWide += m_iTextX*2;
	labelTall += m_iTextY*2;

	m_pBackground->SetBounds( 0, 0, labelWide, labelTall );

	int xOffset = (labelWide - m_pRestartLabel->GetWide())/2;
	m_pRestartLabel->SetPos( 0 + xOffset, 0 + m_iTextY );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the label color each frame
//-----------------------------------------------------------------------------
void CDODRestartRoundLabel::OnThink()
{
	float flRoundRestartTime = DODGameRules()->GetRoundRestartTime();

	if( flRoundRestartTime != m_flLastRestartTime )
	{
		if( flRoundRestartTime > 0 )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "RestartRoundLabelShow" );
		}
		m_flLastRestartTime = flRoundRestartTime;
	}

	if( GetAlpha() > 0 )
	{
		wchar_t minutes[4];
		wchar_t seconds[4];

		int iSecondsRemaining = (int)( flRoundRestartTime - gpGlobals->curtime );

		iSecondsRemaining = MAX( 0, iSecondsRemaining );

		_snwprintf ( minutes, sizeof(minutes)/sizeof(wchar_t), L"%d", (iSecondsRemaining / 60) );
		_snwprintf ( seconds, sizeof(seconds)/sizeof(wchar_t), L"%02d", (iSecondsRemaining % 60) );

		wchar_t labelText[128];
		g_pVGuiLocalize->ConstructString( labelText, sizeof(labelText), g_pVGuiLocalize->Find( "#clan_game_restart" ), 2, minutes, seconds );

		m_pRestartLabel->SetText( labelText );
		m_pRestartLabel->SizeToContents();
		//InvalidateLayout();
	}

	m_pBackground->SetFgColor(GetFgColor());
	m_pRestartLabel->SetFgColor(GetFgColor());
}