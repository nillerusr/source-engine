#include "cbase.h"
#include <vgui_controls/ImagePanel.h>
#include "vgui_controls/AnimationController.h"
#include <vgui_controls/ImagePanel.h>
#include "BriefingTooltip.h"
#include "MedalPanel.h"
#include "controller_focus.h"
#include "asw_medals_shared.h"
#include "asw_gamerules.h"
#include "asw_achievements.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

MedalPanel::MedalPanel(vgui::Panel *parent, const char *name, int iSlot, BriefingTooltip *pTooltip) :
	vgui::Panel(parent, name)
{
	m_iSlot = iSlot;
	m_pMedalIcon = new vgui::ImagePanel(this, "MedalImage");
	m_pMedalIcon->SetAlpha(0);
	m_pMedalIcon->SetShouldScaleImage(true);

	m_bShowTooltip = true;
	m_iMedalIndex = -1;
	m_szMedalName[0] = '\0';
	m_szMedalDescription[0] = '\0';
	m_hTooltip = pTooltip;
}

BriefingTooltip* MedalPanel::GetTooltip()
{
	if ( m_hTooltip.Get() )
		return m_hTooltip.Get();

	return g_hBriefingTooltip.Get();
}

void MedalPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pMedalIcon->SetShouldScaleImage(true);
}

void MedalPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	
	m_pMedalIcon->SetBounds(0,0, GetWide(), GetTall());
}

void MedalPanel::OnThink()
{
	BaseClass::OnThink();

	if (m_iMedalIndex!= -1 && m_bShowTooltip && IsCursorOver()
		&& GetTooltip() && GetTooltip()->GetTooltipPanel()!= this
		&& IsFullyVisible())
	{	
		int tx, ty, w, h;
		tx = ty = 0;
		LocalToScreen(tx, ty);
		GetSize(w, h);
		tx += w * 0.5f;
		ty -= h * 0.01f;
		GetTooltip()->SetTooltip(this, m_szMedalName, m_szMedalDescription,
			tx, ty);
	}
}

void MedalPanel::SetMedalIndex(int i, bool bOffline)
{
	if (m_iMedalIndex != i || bOffline != m_bOffline)
	{
		m_bOffline = bOffline;
		if (i <= 0)
		{
			m_iMedalIndex = -1;
			m_szMedalName[0] = '\0';
			m_szMedalDescription[0] = '\0';
			SetAlpha(0);
			SetVisible(false);
		}
		else
		{
			m_iMedalIndex = i;
			// adjust image index to use for speedrun/outstandings
			if (i >= 44 && i <= 51)	// speedrun
				i = 44;
			else if (i>= 52)	// outstanding
				i = 52;
			Q_snprintf(m_szMedalName, sizeof(m_szMedalName), "#asw_medal%d", m_iMedalIndex);
			// set speedrun tooltip based on singleplayer/multiplayer
			if (m_iMedalIndex >= MEDAL_SPEED_RUN_LANDING_BAY && m_iMedalIndex <= MEDAL_SPEED_RUN_QUEEN_LAIR)
			{
				if (m_bOffline)
					Q_snprintf(m_szMedalDescription, sizeof(m_szMedalDescription), "#asw_medaltt%dsp", m_iMedalIndex);
				else
					Q_snprintf(m_szMedalDescription, sizeof(m_szMedalDescription), "#asw_medaltt%d", m_iMedalIndex);
			}
			else
			{
				Q_snprintf(m_szMedalDescription, sizeof(m_szMedalDescription), "#asw_medaltt%d", m_iMedalIndex);			
			}
			if ( ASWGameRules() )
			{
				int nAchievementIndex = GetAchievementIndexForMedal( i );
				if ( nAchievementIndex != -1 )
				{
					CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
					if ( pAchievementMgr )
					{
						CBaseAchievement *pAch = pAchievementMgr->GetAchievementByID( nAchievementIndex, 0 );
						if ( pAch )
						{
							char buffer[128];
							Q_snprintf( buffer, sizeof(buffer), "achievements/%s", pAch->GetName() );
							m_pMedalIcon->SetImage(buffer);
						}
					}
				}
			}

			SetAlpha(255);
			SetVisible(true);
			//if (GetAlpha() <= 255)	// fade us in, if needed
			//{
				//vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 255, m_iSlot * 0.1f, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			//}
		}
	}
}