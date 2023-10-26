#include "cbase.h"
#include "MissionCompleteFrame.h"
#include "MissionCompletePanel.h"
#include "vgui\CampaignPanel.h"
#include "vgui\asw_hud_chat.h"
#include "clientmode_asw.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

MissionCompleteFrame::MissionCompleteFrame(bool bSuccess, Panel *parent, const char *panelName, bool showTaskbarIcon) :
	vgui::Frame(parent, panelName, showTaskbarIcon)
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);
	SetSize( ScreenWidth() + 1, ScreenHeight() + 1 );
	SetTitle("Mission Complete", true );
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
	SetPaintBackgroundType(0);

	m_pMissionCompletePanel = new MissionCompletePanel( this, "MissionCompletePanel", bSuccess);
	m_pMissionCompletePanel->SetVisible( true );

	RequestFocus();
	SetVisible(true);
	SetEnabled(true);
	SetKeyBoardInputEnabled(false);
	m_bFadeInBackground = false;
	m_fBackgroundFade = 0;
	m_fWrongState = 0;

	// clear the currently visible part of the chat
	CHudChat *pChat = GET_HUDELEMENT( CHudChat );
	if (pChat)
		pChat->InsertBlankPage();

	if (GetClientModeASW() && GetClientModeASW()->m_bSpectator)
	{
		//Msg("MissionCompleteFrame constructor calling cl_spectating\n");
		engine->ServerCmd("cl_spectating");
	}
	else
	{
		//Msg("MissionCompleteFrame constructor not sending cl_spectating as player never wanted to be a spectator\n");
	}
}

void MissionCompleteFrame::PerformLayout()
{
	SetSize( ScreenWidth() + 1, ScreenHeight() + 1 );
	SetPos(0,0);
	
	m_pMissionCompletePanel->InvalidateLayout(true);
}

void MissionCompleteFrame::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0,0,0,m_fBackgroundFade * 255.0f));
}

void MissionCompleteFrame::OnThink()
{
	BaseClass::OnThink();
	if (m_bFadeInBackground)
	{
		m_fBackgroundFade += gpGlobals->frametime * 0.5f;
		if (m_fBackgroundFade > 1.0f)
		{
			m_fBackgroundFade = 1.0f;
			m_bFadeInBackground = false;
		}
		SetBgColor(Color(0,0,0,m_fBackgroundFade * 255.0f));
		SetPaintBackgroundEnabled(true);
	}
	if (ASWGameRules() && ASWGameRules()->GetGameState() != ASW_GS_DEBRIEF)
	{
		m_fWrongState += gpGlobals->frametime;
		if (m_fWrongState > 2.0f)
		{
			OnClose();
		}
	}
	else
	{
		m_fWrongState = 0;
	}
}