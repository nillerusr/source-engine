#include "cbase.h"
#include "CampaignFrame.h"
#include "vgui\CampaignPanel.h"
#include "vgui\FadeInPanel.h"
#include "asw_gamerules.h"
#include "iclientmode.h"
#include "clientmode_asw.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CampaignFrame::CampaignFrame(Panel *parent, const char *panelName, bool showTaskbarIcon) :
	vgui::Frame(parent, panelName, showTaskbarIcon)
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);
	SetSize( ScreenWidth() + 1, ScreenHeight() + 1 );
	SetTitle("Campaign", true );
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

	m_pCampaignPanel = new CampaignPanel( this, "CampaignPanel" );

	RequestFocus();
	SetVisible(true);
	SetEnabled(true);
	SetKeyBoardInputEnabled(false);

	if (GetClientModeASW() && GetClientModeASW()->m_bSpectator)
	{
		engine->ServerCmd("cl_spectating");
	}
}

CampaignFrame::~CampaignFrame()
{
	FadeInPanel *pFadeIn = dynamic_cast<FadeInPanel*>(GetClientMode()->GetViewport()->FindChildByName("FadeIn", true));
	if (pFadeIn)
	{
		pFadeIn->MarkForDeletion();
	}
}

void CampaignFrame::PerformLayout()
{
	BaseClass::PerformLayout();

	SetSize( ScreenWidth() + 1, ScreenHeight() + 1 );
	SetPos(0,0);
	Msg("CampaignFrame::PerformLayout w=%d\n", ScreenWidth());
	m_pCampaignPanel->InvalidateLayout(true);
}

void CampaignFrame::ApplySchemeSettings(vgui::IScheme *scheme)
{
	BaseClass::ApplySchemeSettings(scheme);

	SetPaintBackgroundEnabled(false);
	if (ASWGameRules() && ASWGameRules()->IsIntroMap())
	{
		SetBgColor(Color(0,0,0,0));
	}
}