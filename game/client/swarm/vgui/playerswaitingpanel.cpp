#include "cbase.h"
#include "PlayersWaitingPanel.h"
#include "vgui/isurface.h"
#include <KeyValues.h>
#include <vgui_controls/Label.h>
#include "c_asw_game_resource.h"
#include "c_playerresource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


PlayersWaitingPanel::PlayersWaitingPanel(vgui::Panel *parent, const char *panelName) :
	vgui::Panel(parent, panelName)
{
	m_pTitle = new vgui::Label(this, "Title", "#asw_commanders");
	for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
	{
		m_pPlayerNameLabel[i] = new vgui::Label(this, "Ready", " ");
		m_pPlayerReadyLabel[i] = new vgui::Label(this, "Ready", " ");
	}
}


void PlayersWaitingPanel::PerformLayout()
{
	int w, t;
	//w = ScreenWidth() * 0.3f;
	//t = ScreenHeight() * 0.3f;
	//SetSize(w, t);
	GetSize(w, t);
	int row_height = 0.035f * ScreenHeight();

	float fScale = ScreenHeight() / 768.0f;
	int padding = fScale * 6.0f;
	int title_wide = w - padding * 2;
	m_pTitle->SetBounds(padding, padding, title_wide, row_height);
	int name_wide = title_wide * 0.6f;
	int ready_wide = title_wide * 0.4f;
	for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
	{
		m_pPlayerNameLabel[i]->SetBounds(padding, (i+1) * (row_height) + padding, name_wide, row_height);
		m_pPlayerReadyLabel[i]->SetBounds(padding + name_wide, (i+1) * (row_height) + padding, ready_wide, row_height);
	}
}

void PlayersWaitingPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);
	SetBgColor(Color(0,0,0,128));
	vgui::HFont DefaultFont = pScheme->GetFont( "Default", IsProportional() );
	m_pTitle->SetFont(DefaultFont);
	m_pTitle->SetFgColor(Color(255,255,255,255));
	for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
	{
		m_pPlayerReadyLabel[i]->SetFont(DefaultFont);
		m_pPlayerReadyLabel[i]->SetFgColor(pScheme->GetColor("LightBlue", Color(128,128,128,255)));
		m_pPlayerNameLabel[i]->SetFont(DefaultFont);
		m_pPlayerNameLabel[i]->SetFgColor(pScheme->GetColor("LightBlue", Color(128,128,128,255)));
	}

}

void PlayersWaitingPanel::OnThink()
{
	C_ASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	// set labels and ready status from players
	int iNumPlayersInGame = 0;
	for ( int j = 1; j <= gpGlobals->maxClients; j++ )
	{	
		if ( g_PR->IsConnected( j ) )
		{
			m_pPlayerNameLabel[iNumPlayersInGame]->SetText(g_PR->GetPlayerName(j));
			bool bReady = pGameResource->IsPlayerReady(j);
			if (bReady)
				m_pPlayerReadyLabel[iNumPlayersInGame]->SetText("#asw_campaign_ready");
			else
				m_pPlayerReadyLabel[iNumPlayersInGame]->SetText("#asw_campaign_waiting");
			iNumPlayersInGame++;
		}
	}
	// hide further panels in
	for (int j=iNumPlayersInGame;j<ASW_MAX_READY_PLAYERS;j++)
	{
		m_pPlayerNameLabel[j]->SetText("");
		m_pPlayerReadyLabel[j]->SetText("");
	}
}