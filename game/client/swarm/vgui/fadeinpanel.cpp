#include "cbase.h"
#include "FadeInPanel.h"
#include <vgui_controls/ImagePanel.h>
#include "vgui_controls/AnimationController.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

FadeInPanel::FadeInPanel(vgui::Panel *parent, const char *name) : vgui::Panel(parent, name)
{	
	//Msg("%f: FadeInPanel::FadeInPanel\n", gpGlobals->curtime);
	m_pBlackImage = new vgui::ImagePanel(this, "FadeInImage");
	m_pBlackImage->SetImage("swarm/HUD/ASWHUDBlackBar");
	m_pBlackImage->SetShouldScaleImage(true);
	m_pBlackImage->SetMouseInputEnabled(false);
	SetMouseInputEnabled(false);
	m_bSlowRemove = false;
	m_fIngameTime = 0;
	m_bFastRemove = false;
}

FadeInPanel::~FadeInPanel()
{
	//Msg("%f: FadeInPanel::~FadeInPanel\n", gpGlobals->curtime);
}

void FadeInPanel::PerformLayout()
{
	SetBounds(0, 0, ScreenWidth() + 1, ScreenHeight() + 1);
	m_pBlackImage->SetBounds(0, 0, ScreenWidth() + 1, ScreenHeight() + 1);
}

void FadeInPanel::StartSlowRemove()
{
	m_bSlowRemove = true;
}

void FadeInPanel::AllowFastRemove()
{
	m_bFastRemove = true;
}

void FadeInPanel::OnThink()
{
	// fade out if we're ingame for more than a second
	if (ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_INGAME)
	{
		//Msg("FadeInPanel ingame, m_fIngameTime = %f\n", m_fIngameTime);
		m_fIngameTime += gpGlobals->frametime;
		if (m_fIngameTime >= 2.0f || m_bFastRemove)
		{
			m_bSlowRemove = true;
		}
	}
	if (GetAlpha() >= 255 && m_bSlowRemove)
	{
		SetAlpha(254);
		vgui::GetAnimationController()->RunAnimationCommand(this, "alpha", 0, 0.0f, 1.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
	if (GetAlpha() <= 0)
	{
		MarkForDeletion();
	}
}
