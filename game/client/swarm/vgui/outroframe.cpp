#include "cbase.h"
#include "OutroFrame.h"
#include "vgui\CampaignPanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

OutroFrame::OutroFrame(Panel *parent, const char *panelName, bool showTaskbarIcon) :
	vgui::Frame(parent, panelName, showTaskbarIcon)
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);
	SetSize( ScreenWidth() + 1, ScreenHeight() + 1 );
	SetTitle("Outro", true );
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

	CampaignPanel *campaignpanel = new CampaignPanel( this, "CampaignPanel" );	
	campaignpanel->SetVisible(true);
	campaignpanel->InvalidateLayout();

	RequestFocus();
	SetVisible(true);
	SetEnabled(true);
	SetKeyBoardInputEnabled(false);
}

void OutroFrame::PerformLayout()
{
	SetSize( ScreenWidth() + 1, ScreenHeight() + 1 );
	SetPos(0,0);	
}