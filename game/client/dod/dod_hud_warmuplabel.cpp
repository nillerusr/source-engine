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
class CDODHudWarmupLabel : public vgui::Panel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CDODHudWarmupLabel, vgui::Panel );

public:
	CDODHudWarmupLabel( const char *pElementName );
	virtual void Reset();

	virtual void PerformLayout();

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();

private:
	vgui::HFont m_hFont;
	Color		m_bgColor;

	vgui::Label *m_pWarmupLabel;	// "Warmup Mode"

	vgui::Label *m_pBackground;		// black box

	bool m_bInWarmup;

	CPanelAnimationVarAliasType( int, m_iTextX, "text_xpos", "8", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iTextY, "text_ypos", "8", "proportional_int" );
};

DECLARE_HUDELEMENT( CDODHudWarmupLabel );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODHudWarmupLabel::CDODHudWarmupLabel( const char *pElementName ) : BaseClass(NULL, "WarmupLabel"), CHudElement( pElementName )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	SetVisible( false );
	SetAlpha( 0 );

	m_pBackground = new vgui::Label( this, "Background", "" );

	m_pWarmupLabel = new vgui::Label( this, "RoundState_warmup", g_pVGuiLocalize->Find( "#Clan_warmup_mode" ) );
	m_pWarmupLabel->SetPaintBackgroundEnabled( false );
	m_pWarmupLabel->SetPaintBorderEnabled( false );
	m_pWarmupLabel->SizeToContents();
	m_pWarmupLabel->SetContentAlignment( vgui::Label::a_west );
	m_pWarmupLabel->SetFgColor( GetFgColor() );

	m_bInWarmup = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODHudWarmupLabel::Reset()
{
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "WarmupLabelHide" );

	m_bInWarmup = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODHudWarmupLabel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetFgColor( Color(0,0,0,0) );	//GetSchemeColor("RoundStateFg", pScheme) );
	m_hFont = pScheme->GetFont( "HudHintText", true );

	m_pBackground->SetBgColor( GetSchemeColor("HintMessageBg", pScheme) );
	m_pBackground->SetPaintBackgroundType( 2 );

	SetAlpha( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Resizes the label
//-----------------------------------------------------------------------------
void CDODHudWarmupLabel::PerformLayout()
{
	BaseClass::PerformLayout();

	int wide, tall;
	GetSize( wide, tall );

	// find the widest line
	int labelWide = m_pWarmupLabel->GetWide();

	// find the total height
	int fontTall = vgui::surface()->GetFontTall( m_hFont );
	int labelTall = fontTall;

	labelWide += m_iTextX*2;
	labelTall += m_iTextY*2;

	m_pBackground->SetBounds( 0, 0, labelWide, labelTall );

	int xOffset = (labelWide - m_pWarmupLabel->GetWide())/2;
	m_pWarmupLabel->SetPos( 0 + xOffset, 0 + m_iTextY );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the label color each frame
//-----------------------------------------------------------------------------
void CDODHudWarmupLabel::OnThink()
{
	m_pBackground->SetFgColor(GetFgColor());
	m_pWarmupLabel->SetFgColor(GetFgColor());

	bool bInWarmup = DODGameRules()->IsInWarmup();

	if( bInWarmup != m_bInWarmup )
	{
		if( bInWarmup )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "WarmupLabelShow" );
			InvalidateLayout();
		}

		m_bInWarmup = bInWarmup;
	}
}