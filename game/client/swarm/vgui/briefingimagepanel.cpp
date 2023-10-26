#include "cbase.h"
#include "BriefingImagePanel.h"
#include "asw_gamerules.h"
#include "c_asw_player.h"
#include "vgui_controls/Frame.h"
#include "hud_element_helper.h"
#include "hud_basechat.h"
#include "vgui\ChatEchoPanel.h"
#include "inputsystem/ButtonCode.h"
#include "clientmode_shared.h"
#include "clientmode_asw.h"
#include "vgui\asw_hud_chat.h"
#include <vgui/IInput.h>
#include "vgui\FadeInPanel.h"
#include <vgui_controls/PHandle.h>
#include "c_asw_game_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

class CHudChat;

using namespace vgui;

#define ASW_BRIEFING_MUSIC_DURATION 120

BriefingImagePanel::BriefingImagePanel(Panel *parent, const char *name) : ImagePanel(parent, name)
{
//	float f = random->RandomFloat();

// 	if (f < 0.33f)
// 		SetImage("swarm/BriefingCorridor1");
// 	else if (f < 0.66f)
// 		SetImage("swarm/BriefingReactor");
// 	else
// 		SetImage("swarm/BriefingPlant");

	// set to something small as this image is hidden
	SetImage( "swarm/missionpics/addonmissionpic" );

	SetShouldScaleImage(true);
	SetPos(0,0);
	/*SetSize(parent->GetWide(),parent->GetWide() + 1);*/
	m_pOldParent = NULL;

	//m_pChatEcho = new ChatEchoPanel(this, "ChatEchoPanel");	

	StartMusic();
}

BriefingImagePanel::~BriefingImagePanel()
{
	// double check our spectator bool is okay as we leave the briefing
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer && ASWGameResource() && GetClientModeASW())
	{
		if (ASWGameResource()->GetNumMarines(pPlayer) > 0)
		{
			GetClientModeASW()->m_bSpectator = false;
		}
	}
}

void BriefingImagePanel::PerformLayout()
{
	int width = ScreenWidth();
	int height = ScreenHeight();

	// briefing image should always be square
	/*SetSize(width + 1, width + 1);*/
	SetSize(width + 1, height + 1);

// 	if (m_pChatEcho)
// 	{		
// 		m_pChatEcho->SetPos(width * 0.22f, height * 0.87f);
// 		m_pChatEcho->InvalidateLayout(true);
// 		m_pChatEcho->SetZPos(240);
// 		m_pChatEcho->MoveToFront();
// 	}	
}

void BriefingImagePanel::OnThink()
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
	{
		CloseBriefingFrame();
		return;
	}

	if (!ASWGameRules())
		return;

	if (ASWGameRules()->GetGameState() >= ASW_GS_LAUNCHING
		 && gpGlobals->curtime - pPlayer->GetLastRestartTime() > 5.0f)	// don't close the briefing frame within X seconds of reset, to give the server time to update our gamerules state
	{
		CloseBriefingFrame();
		return;
	}

	if (gpGlobals->curtime >= m_fRestartMusicTime && m_fRestartMusicTime != 0)
	{
		StartMusic();
	}
}

void BriefingImagePanel::StartMusic()
{
	if (GetClientModeASW() && ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_BRIEFING)
	{
		GetClientModeASW()->StopBriefingMusic(true);	// make sure to stop any currently finished music
		GetClientModeASW()->StartBriefingMusic();
		m_fRestartMusicTime = gpGlobals->curtime + ASW_BRIEFING_MUSIC_DURATION;
	}
}

extern vgui::DHANDLE<vgui::Frame> g_hBriefingFrame;
void BriefingImagePanel::CloseBriefingFrame()
{
	if (g_hBriefingFrame.Get())
	{
		if (GetClientModeASW())
			GetClientModeASW()->StopBriefingMusic();
		g_hBriefingFrame->SetDeleteSelfOnClose(true);
		g_hBriefingFrame->Close();

		FadeInPanel *pFadeIn = dynamic_cast<FadeInPanel*>(GetClientMode()->GetViewport()->FindChildByName("FadeIn", true));
		if (pFadeIn)
		{
			pFadeIn->AllowFastRemove();
		}

		// clear the currently visible part of the chat
		CHudChat *pChat = GET_HUDELEMENT( CHudChat );
		if (pChat && pChat->GetChatHistory())
			pChat->GetChatHistory()->ResetAllFades( false, false, 0 );
// 		if (m_pChatEcho)
// 		{
// 			m_pChatEcho->SetVisible(false);
// 		}
	}

	g_hBriefingFrame = NULL;
}