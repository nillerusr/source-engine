#include "cbase.h"
#include "nb_promotion_panel.h"
#include "nb_button.h"
#include <vgui/ISurface.h>
#include "c_asw_player.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui/ILocalize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Promotion_Panel::CNB_Promotion_Panel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pTitle = new vgui::Label( this, "Title", "" );
	m_pBanner = new vgui::Panel( this, "Banner" );
	m_pDescriptionLabel = new vgui::Label( this, "DescriptionLabel", "" );
	m_pMedalLabel = new vgui::Label( this, "MedalLabel", "" );
	m_pWarningLabel = new vgui::Label( this, "WarningLabel", "" );
	m_pMedalIcon = new vgui::ImagePanel( this, "MedalIcon" );
	m_pBackButton = new CNB_Button( this, "BackButton", "", this, "BackButton" );
	m_pAcceptButton = new CNB_Button( this, "AcceptButton", "", this, "AcceptButton" );
}

CNB_Promotion_Panel::~CNB_Promotion_Panel()
{

}

void CNB_Promotion_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_promotion_panel.res" );

	SetAlpha( 0 );
	vgui::GetAnimationController()->RunAnimationCommand( this, "alpha", 255, 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR );
}

void CNB_Promotion_Panel::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Promotion_Panel::OnThink()
{
	BaseClass::OnThink();


	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;
	
	m_pMedalLabel->SetText( g_pVGuiLocalize->FindSafe( VarArgs( "#nb_promotion_medal_%d", pPlayer->GetPromotion() + 1 ) ) );

	m_pMedalIcon->SetImage( VarArgs( "briefing/promotion_%d_LG", pPlayer->GetPromotion() + 1 ) );
}

void CNB_Promotion_Panel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "BackButton" ) )
	{
		MarkForDeletion();
		return;
	}
	else if ( !Q_stricmp( command, "AcceptButton" ) )
	{
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		pPlayer->AcceptPromotion();
		MarkForDeletion();
		return;
	}
	BaseClass::OnCommand( command );
}

void CNB_Promotion_Panel::PaintBackground()
{
	// fill background
	vgui::surface()->DrawSetColor( Color( 16, 32, 46, 230 ) );
	vgui::surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );

	// get banner position
	int x, y, w, t;
	m_pBanner->GetBounds( x, y, w, t );
	vgui::surface()->DrawSetColor( Color( 53, 86, 117, 255 ) );
	vgui::surface()->DrawFilledRect( x, y, x + w, y + t );

	// draw highlights
	int nBarHeight = YRES( 2 );
	int iHalfWide = w * 0.5f;
	int nBarPosY = y - YRES( 4 );
	vgui::surface()->DrawSetColor( Color( 97, 210, 255, 255 ) );
	vgui::surface()->DrawFilledRectFade( iHalfWide, nBarPosY, iHalfWide + YRES( 320 ), nBarPosY + nBarHeight, 255, 0, true );
	vgui::surface()->DrawFilledRectFade( iHalfWide - YRES( 320 ), nBarPosY, iHalfWide, nBarPosY + nBarHeight, 0, 255, true );

	nBarPosY = y + t + YRES( 2 );
	vgui::surface()->DrawFilledRectFade( iHalfWide, nBarPosY, iHalfWide + YRES( 320 ), nBarPosY + nBarHeight, 255, 0, true );
	vgui::surface()->DrawFilledRectFade( iHalfWide - YRES( 320 ), nBarPosY, iHalfWide, nBarPosY + nBarHeight, 0, 255, true );
}
