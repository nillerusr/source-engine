#include "cbase.h"
#include "BriefingFrame.h"
#include "vgui\BriefingImagePanel.h"
#include "vgui\nb_mission_panel.h"
#include "c_asw_player.h"
#include "controller_focus.h"
#include <vgui_controls/Button.h>
#include "asw_hud_chat.h"
#include "FadeInPanel.h"
#include "iclientmode.h"
#include "vgui_controls/AnimationController.h"
#include "c_user_message_register.h"
#include "clientmode_asw.h"
#include "nb_main_panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BriefingFrame::BriefingFrame(Panel *parent, const char *panelName, bool showTaskbarIcon) :
	vgui::Frame(parent, panelName, showTaskbarIcon)
{
	vgui::HScheme scheme;
	scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme( scheme );
	
	SetSize( ScreenWidth() + 1, ScreenHeight() + 1 );
	SetTitle("Briefing", true );
	SetPos(0,0);
	SetMoveable(false);
	SetSizeable(false);
	SetMenuButtonVisible(false);
	SetMaximizeButtonVisible(false);
	SetMinimizeToSysTrayButtonVisible(false);
	SetCloseButtonVisible(false);
	SetTitleBarVisible(false);
	SetAlpha(0.3f);
	SetPaintBackgroundEnabled(false);

	// our backdrop image panel
	m_pBackdrop = new BriefingImagePanel( this, "BriefingBackdrop" );
	m_pBackdrop->SetVisible( true );	

	m_pMainPanel = new CNB_Main_Panel( this, "MainPanel" );

	RequestFocus();
	SetVisible(true);
	SetEnabled(true);
	// ASWTODO: this causes an assert, but without it, F1 doesn't work
	SetKeyBoardInputEnabled(false);
	SetZPos(200);
	SetAlpha(0);

	// clear the currently visible part of the chat
	CHudChat *pChat = GET_HUDELEMENT( CHudChat );
	if (pChat)
	{
		pChat->InsertBlankPage();
		pChat->ShowChatPanel();	// chat up all the time during briefing
	}

	if (GetClientModeASW() && GetClientModeASW()->m_bSpectator)
	{
		Msg("BriefingFrame constructor calling cl_spectating\n");
		engine->ServerCmd("cl_spectating");
	}
}

BriefingFrame::~BriefingFrame()
{
	FadeInPanel *pFadeIn = dynamic_cast<FadeInPanel*>(GetClientMode()->GetViewport()->FindChildByName("FadeIn", true));
	if (pFadeIn)
	{
		pFadeIn->AllowFastRemove();
	}
}

void BriefingFrame::OnClose()
{
	BaseClass::OnClose();

	// make the fade out slower
	vgui::GetAnimationController()->RunAnimationCommand(this, "alpha", 0.0f, 0.0f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
}

void BriefingFrame::PerformLayout()
{
	SetSize( ScreenWidth() + 1, ScreenHeight() + 1 );
	SetPos(0,0);
}

BriefingPreLaunchPanel::BriefingPreLaunchPanel(Panel *parent, const char *panelName) : vgui::Panel(parent, panelName)
{
	m_fFadeAlpha = 0;
	SetBgColor(Color(0,0,0,m_fFadeAlpha));
}

void BriefingPreLaunchPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	SetBounds(0, 0, ScreenWidth() + 1, ScreenHeight() + 1);
}

void BriefingPreLaunchPanel::OnThink()
{
	m_fFadeAlpha += (gpGlobals->frametime * 255.0f);	// 1 second fade out
	if (m_fFadeAlpha > 255.0f)
		m_fFadeAlpha = 255.0f;
	SetBgColor(Color(0,0,0,m_fFadeAlpha));
}

void BriefingPreLaunchPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);
	SetBgColor(Color(0,0,0,m_fFadeAlpha));
}

extern vgui::DHANDLE<vgui::Frame> g_hBriefingFrame;