#include "cbase.h"
#include <KeyValues.h>
#include "RestartMissionButton.h"
#include "C_ASW_Player.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "controller_focus.h"
#include "asw_gamerules.h"
#include "vgui_controls/AnimationController.h"
#include "ForceReadyPanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

RestartMissionButton::RestartMissionButton(Panel *parent, const char *panelName, const char *text) :
	ImageButton(parent, panelName, text)
{
	SetText("#asw_button_restart");
	//SetContentAlignment(vgui::Label::a_center);
	SetButtonTexture("swarm/Briefing/ShadedButton");
	SetButtonOverTexture("swarm/Briefing/ShadedButton_over");	
	SetContentAlignment(vgui::Label::a_center);
	SetEnabled(true);

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(this, false);
	}
	m_bCanStart = false;
}

RestartMissionButton::RestartMissionButton(Panel *parent, const char *panelName, const wchar_t *wszText) :
	ImageButton(parent, panelName, wszText)
{
	SetText("#asw_button_restart");
	SetButtonTexture("swarm/Briefing/ShadedButton");
	SetButtonOverTexture("swarm/Briefing/ShadedButton_over");	
	SetContentAlignment(vgui::Label::a_center);
	SetEnabled(true);

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(this, false);
	}
	m_bCanStart = false;
}


RestartMissionButton::~RestartMissionButton()
{
	if (GetControllerFocus())
	{
		GetControllerFocus()->RemoveFromFocusList(this);
	}
}

void RestartMissionButton::PerformLayout()
{
	SetPos(ScreenWidth()*0.70, 0.01f*ScreenHeight());
	SetSize(ScreenWidth()*0.12, ScreenHeight()*0.04);
}

void RestartMissionButton::OnMouseReleased(MouseCode code)
{
	BaseClass::OnMouseReleased(code);

	if ( code != MOUSE_LEFT )
		return;

	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer || GetAlpha() <= 0)
		return;

	if (m_bCanStart)
	{
		pPlayer->RequestMissionRestart();
	}
	else
	{
		// ForceReadyPanel* pForceReady = 
		engine->ClientCmd("cl_wants_restart"); // notify other players that we're waiting on them
		new ForceReadyPanel(GetParent(), "ForceReady", "#asw_force_restartm", ASW_FR_RESTART);
	}
}

void RestartMissionButton::OnThink()
{
	BaseClass::OnThink();

	C_ASW_Player* player = C_ASW_Player::GetLocalASWPlayer();
	if (!player || !ASWGameRules())
		return;

	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	bool bLeader = (player == pGameResource->GetLeader());

	// only show this button if you successfully complete a campaign mission - otherwise the start/ready button in the corner will offer restart options for the leader
	if (bLeader)
	{
		if (ASWGameRules()->IsCampaignGame() && ASWGameRules()->GetMissionSuccess())
		{
			if (GetAlpha() <= 0)
			{
				SetAlpha(1);
				vgui::GetAnimationController()->RunAnimationCommand(this, "alpha", 255.0f, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			}
			m_bCanStart = pGameResource->AreAllOtherPlayersReady(player->entindex());

			if (m_bCanStart)
			{
				SetButtonEnabled(true);
			}
			else
			{
				SetButtonEnabled(false);
			}
		}
		else
		{
			if (GetAlpha() > 0)
				SetAlpha(0);
		}
	}
	else
	{
		if (GetAlpha() > 0)
			SetAlpha(0);
	}
}

void RestartMissionButton::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	//SetDefaultColor(Color(68,140,203,255), Color(0,0,0,255));
	// Set armed button colors
	//SetArmedColor(Color(255,255,255,255), Color(65,74,96,255));
	// Set depressed button colors
	//SetDepressedColor(Color(255,255,255,255), Color(65,74,96,255));
	//SetDefaultBorder(pScheme->GetBorder("ASWBriefingButtonBorder"));
	//SetDepressedBorder(pScheme->GetBorder("ASWBriefingButtonBorder"));
	//SetKeyFocusBorder(pScheme->GetBorder("ASWBriefingButtonBorder"));
	//SetPaintBackgroundType(1);
	SetAlpha(0);
}