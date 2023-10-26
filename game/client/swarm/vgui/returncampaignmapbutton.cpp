#include "cbase.h"
#include <KeyValues.h>
#include "ReturnCampaignMapButton.h"
#include "C_ASW_Player.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "controller_focus.h"
#include "asw_gamerules.h"
#include "vgui_controls/AnimationController.h"
#include "ForceReadyPanel.h"
#include "nb_button.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

ReturnCampaignMapButton::ReturnCampaignMapButton(Panel *parent, const char *panelName, const char *text) :
	ImageButton(parent, panelName, text)
{
	SetText("#asw_button_retcamp");	
	SetButtonTexture("swarm/Briefing/ShadedButton");
	SetButtonOverTexture("swarm/Briefing/ShadedButton_over");	
	SetContentAlignment(vgui::Label::a_center);
	SetEnabled(true);

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(this, false);
	}
	m_bCanReturn = false;
}

ReturnCampaignMapButton::ReturnCampaignMapButton(Panel *parent, const char *panelName, const wchar_t *wszText) :
	ImageButton(parent, panelName, wszText)
{
	SetText("#asw_button_retcamp");
	SetButtonTexture("swarm/Briefing/ShadedButton");
	SetButtonOverTexture("swarm/Briefing/ShadedButton_over");	
	SetContentAlignment(vgui::Label::a_center);
	SetEnabled(true);

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(this, false);
	}
	m_bCanReturn = false;
}


ReturnCampaignMapButton::~ReturnCampaignMapButton()
{
	if (GetControllerFocus())
	{
		GetControllerFocus()->RemoveFromFocusList(this);
	}
}

void ReturnCampaignMapButton::PerformLayout()
{
	SetPos(ScreenWidth()*0.50, 0.01f*ScreenHeight());
	SetSize(ScreenWidth()*0.18, ScreenHeight()*0.04);
}

void ReturnCampaignMapButton::OnMouseReleased(MouseCode code)
{
	BaseClass::OnMouseReleased(code);

	if ( code != MOUSE_LEFT )
		return;

	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer || GetAlpha() <= 0)
		return;

	if (m_bCanReturn)
	{
		// show a confirmation dialogue anyway, as the campaign button isn't especially clear
		ForceReadyPanel* pForceReady = new ForceReadyPanel(GetParent(), "ForceReady", "#asw_confirm_retcamp", ASW_FR_CAMPAIGN_MAP);
		if (pForceReady && pForceReady->m_pForceButton)
			pForceReady->m_pForceButton->SetText("#asw_force_yes");
	}
	else
	{
		// ForceReadyPanel* pForceReady = 
		engine->ClientCmd("cl_wants_returnmap"); // notify other players that we're waiting on them
		new ForceReadyPanel(GetParent(), "ForceReady", "#asw_force_retcamp", ASW_FR_CAMPAIGN_MAP);
	}
}

void ReturnCampaignMapButton::OnThink()
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
	if (bLeader && ASWGameRules()->IsCampaignGame() && ASWGameRules()->GetMissionFailed())
	{		
		if (GetAlpha() <= 0)
		{
			SetAlpha(1);
			vgui::GetAnimationController()->RunAnimationCommand(this, "alpha", 255.0f, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		}
		m_bCanReturn = pGameResource->AreAllOtherPlayersReady(player->entindex());

		if (m_bCanReturn)
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

void ReturnCampaignMapButton::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetAlpha(0);
}