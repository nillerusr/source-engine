#include "cbase.h"
#include <vgui_controls/ImagePanel.h>
#include "c_asw_marine_resource.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_campaign_save.h"
#include "MedalArea.h"
#include "MedalPanel.h"
#include "asw_gamerules.h"
#include "asw_medal_store.h"
#include "c_asw_game_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

float MedalArea::s_fLastMedalSound = 0;
#define ASW_MEDAL_SOUND_INTERVAL 0.4

MedalArea::MedalArea(vgui::Panel *parent, const char *name, int iMedalsAcross) :
	vgui::Panel(parent, name)
{
	m_hMI = NULL;
	m_iProfileIndex = -1;
	m_szMedalString[0] = '\0';
	m_iMedalsAcross = iMedalsAcross;
	m_bRightAlign = false;
	m_fStartShowingMedalsTime = -1;
	m_iLastFullyShown = 0;
}

void MedalArea::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetFgColor(Color(255,255,255,255));
}

void MedalArea::PerformLayout()
{
	int iMedalW = 32.0f * ScreenWidth() / 1024.0f;
	int iMedalH = 32.0f * ScreenWidth() / 1024.0f;

	// size and position each medal	
	int mx = 0;
	int my = 0;
	int wide = iMedalW * m_iMedalsAcross;
	/*
	if (bRightAlign)
	{
		mx = m_iMedalsAcross-1;
		for (int i=0;i<m_Medals.Count();i++)
		{
			m_Medals[i]->SetBounds(mx * iMedalW, my * iMedalH,
				iMedalW, iMedalH);
			mx--;
			if (mx < 0)
			{
				mx = m_iMedalsAcross-1;
				my++;
			}
		}

		if (mx == m_iMedalsAcross-1)
			my--;
		if (my < 0)
			my = 0;
	}
	else
	{		*/
		for (int i=0;i<m_Medals.Count();i++)
		{
			if (m_bRightAlign)
				m_Medals[i]->SetBounds(wide - (mx * iMedalW) - iMedalW, my * iMedalH,
					iMedalW, iMedalH);
			else
				m_Medals[i]->SetBounds(mx * iMedalW, my * iMedalH,
					iMedalW, iMedalH);
			mx++;
			if (mx >= m_iMedalsAcross)
			{
				mx = 0;
				my++;
			}
		}

		if (mx == 0)
			my--;
		if (my < 0)
			my = 0;
	//}
	SetSize(wide, iMedalH * (my+1));
}

void MedalArea::OnThink()
{
	BaseClass::OnThink();

	const char *pMedals = GetMedalString();
	
	// check to see if the medals awarded to our marine have changed
	if (Q_strcmp(m_szMedalString, pMedals))
	{		
		Q_strncpy( m_szMedalString, pMedals, sizeof(m_szMedalString) );

		// break up the medal string into medal numbers
		const char	*p = m_szMedalString;
		char		token[128];
		
		p = nexttoken( token, p, ' ' );
		int iMedalNum = 0;
		while ( Q_strlen( token ) > 0 )  
		{
			int iMedalIndex = atoi(token);
			if (m_Medals.Count() <= iMedalNum)
			{
				// make a new medal
				m_Medals.AddToTail(new MedalPanel(this, "MedalPanel", iMedalNum));
			}

			if (m_Medals.Count() > iMedalNum)
			{
				// notify the medal panel of its index (will set the image, tooltip and make it fade in)
				m_Medals[iMedalNum]->SetMedalIndex(iMedalIndex, gpGlobals->maxClients <= 1);
				m_Medals[iMedalNum]->SetFgColor(Color(255,255,255,255));
			}
			iMedalNum++;
			if (p)
				p = nexttoken( token, p, ' ' );
			else
				token[0] = '\0';
		}

		// hide any medals further down the list
		while (iMedalNum < m_Medals.Count())
		{
			m_Medals[iMedalNum]->SetMedalIndex(-1, gpGlobals->maxClients <= 1);
			iMedalNum++;
		}

		OnMedalStringChanged();

		InvalidateLayout(true);
	}

	// set alpha of medals according to whether we're showing them or not
	if (m_fStartShowingMedalsTime < 0)
	{
		// show all
		for (int i=0;i<m_Medals.Count();i++)
		{
			m_Medals[i]->SetAlpha(255);
		}
	}
	else
	{
		// calculate how many to show based on medal delay
		float fTimePassed = gpGlobals->curtime - m_fStartShowingMedalsTime;
		if (m_fStartShowingMedalsTime == 0 || fTimePassed < 0)
		{
			for (int i=0;i<m_Medals.Count();i++)
			{
				m_Medals[i]->SetAlpha(0);
			}
		}
		else
		{
			const float fMedalDelay = 0.6f;
			int iNumToShow = fTimePassed / fMedalDelay;
			if (iNumToShow > m_Medals.Count())
				iNumToShow = m_Medals.Count();
			// fully show this many
			for (int i=0;i<iNumToShow;i++)
			{
				m_Medals[i]->SetAlpha(255);
			}			
			int iFullyShown = iNumToShow;
			float fRemainder = fTimePassed - (iNumToShow * fMedalDelay);
			if (iNumToShow < m_Medals.Count() && fRemainder > 0)
			{
				int alpha = (fRemainder / fMedalDelay) * 255.0f;
				m_Medals[iNumToShow]->SetAlpha(alpha);
				if (alpha > 130)
					iFullyShown++;
			}
			if (iFullyShown > m_iLastFullyShown)
			{
				if (gpGlobals->curtime > s_fLastMedalSound + ASW_MEDAL_SOUND_INTERVAL)
				{
					CLocalPlayerFilter filter;
					C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.SkillUpgrade" );
					s_fLastMedalSound = gpGlobals->curtime;
				}
			}
			m_iLastFullyShown = iFullyShown;
		}
	}
}

void MedalArea::HideMedals()
{
	m_fStartShowingMedalsTime = 0;
}

void MedalArea::StartShowingMedals(float time)
{
	m_fStartShowingMedalsTime = time;
}

const char * MedalArea::GetMedalString()
{
	if (m_hMI.Get())
	{
		return m_hMI->m_MedalsAwarded;		
	}
	return "";
}

// check for updating our collected medals.  Medals are only added to the collection if the mission was a success.
void MedalArea::OnMedalStringChanged()
{
#ifdef USE_MEDAL_STORE
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer && m_hMI.Get() && m_hMI->GetCommanderIndex() == pPlayer->entindex()
			&& ASWGameRules() && ASWGameRules()->GetMissionSuccess()
			&& !engine->IsPlayingDemo())
	{
		// have this client update his clientside record of which medals he's collected so far
		// with any new medals in this string
		GetMedalStore()->OnAwardedMedals(m_hMI->m_MedalsAwarded, m_hMI->GetProfileIndex(), gpGlobals->maxClients > 1);
	}
#endif
}


// =============== Campaign Medal Area ================== (used on roster bio area)
MedalAreaCampaign::MedalAreaCampaign(vgui::Panel *parent, const char *name, int iMedalsAcross) :
	MedalArea(parent, name, iMedalsAcross)
{
}

const char * MedalAreaCampaign::GetMedalString()
{
	if (ASWGameRules() && ASWGameRules()->IsCampaignGame() && ASWGameRules()->GetCampaignSave()
		&& m_iProfileIndex >= 0 && m_iProfileIndex < ASW_NUM_MARINE_PROFILES)
	{
		C_ASW_Campaign_Save *pSave = ASWGameRules()->GetCampaignSave();
		return pSave->m_Medals[m_iProfileIndex];
	}
	return "";
}

void MedalAreaCampaign::OnMedalStringChanged()
{
	// don't need to do any saving
}

// =============== Player Medal Area ==================
MedalAreaPlayer::MedalAreaPlayer(vgui::Panel *parent, const char *name, int iMedalsAcross) :
	MedalArea(parent, name, iMedalsAcross)
{
}

const char * MedalAreaPlayer::GetMedalString()
{
	if (m_iProfileIndex >= 0 && m_iProfileIndex <=ASW_MAX_READY_PLAYERS && ASWGameResource())
	{		
		return ASWGameResource()->m_iszPlayerMedals[m_iProfileIndex];
	}	
	return "";
}

void MedalAreaPlayer::OnMedalStringChanged()
{
#ifdef USE_MEDAL_STORE
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer && ASWGameRules() && ASWGameResource() && m_iProfileIndex >= 0 && m_iProfileIndex < ASW_MAX_READY_PLAYERS
			&& GetMedalStore() && ASWGameRules()->GetMissionSuccess() && pPlayer->entindex() == m_iProfileIndex+1
			&& !engine->IsPlayingDemo())
	{
		GetMedalStore()->OnAwardedPlayerMedals(m_iProfileIndex, ASWGameResource()->m_iszPlayerMedals[m_iProfileIndex], gpGlobals->maxClients > 1);
	}
#endif
}
