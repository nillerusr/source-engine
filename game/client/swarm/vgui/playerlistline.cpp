#include "cbase.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ImagePanel.h>
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_campaign_save.h"
#include "c_asw_game_resource.h"
#include "PlayerListPanel.h"
#include "PlayerListLine.h"
#include "c_playerresource.h"
#include <vgui/ILocalize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

extern ConVar asw_vote_kick_fraction;
extern ConVar asw_vote_leader_fraction;

PlayerListLine::PlayerListLine(vgui::Panel *parent, const char *name) :
	vgui::Panel(parent, name)
{
	m_iPlayerIndex = -1;
	m_pPlayerLabel = new vgui::Label(this, "PlayerLabel", " ");
	m_pMarinesLabel = new vgui::Label(this, "MarinesLabel", " ");
	m_pPingLabel = new vgui::Label(this, "PingLabel", " ");
	m_pKickCheck = new VoteCheck(this, "KickCheck", "#asw_player_list_kick_check");
	m_pKickCheck->AddActionSignalTarget(this);
	m_pLeaderCheck = new VoteCheck(this, "KickCheck", "#asw_player_list_leader_check");
	m_pLeaderCheck->AddActionSignalTarget(this);
	m_szPlayerName[0]='\0';
	m_szPingString[0]='\0';
	m_wszMarineNames[0]='\0';
	for (int i=0;i<MAX_VOTE_ICONS;i++)
	{
		m_pKickVoteIcon[i] = new vgui::ImagePanel(this, "BootIcon");
		m_pKickVoteIcon[i]->SetVisible(false);
		m_pKickVoteIcon[i]->SetShouldScaleImage(true);
		m_pKickVoteIcon[i]->SetImage("swarm/PlayerList/BootIcon");
		m_pLeaderVoteIcon[i] = new vgui::ImagePanel(this, "LeaderIcon");
		m_pLeaderVoteIcon[i]->SetVisible(false);
		m_pLeaderVoteIcon[i]->SetShouldScaleImage(true);
		m_pLeaderVoteIcon[i]->SetImage("swarm/PlayerList/LeaderIcon");
		m_iKickIconState[i] = 0;
		m_iLeaderIconState[i] = 0;
	}
	m_bKickChecked = false;
	m_bLeaderChecked = false;
}

void PlayerListLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	vgui::HFont DefaultFont = pScheme->GetFont( "Default", IsProportional() );
	vgui::HFont Verdana = pScheme->GetFont( "Default", IsProportional() );
	m_pPlayerLabel->SetFont(DefaultFont);
	m_pPlayerLabel->SetFgColor(Color(255,255,255,255));
	m_pMarinesLabel->SetFont(DefaultFont);
	m_pMarinesLabel->SetFgColor(Color(255,255,255,255));
	m_pPingLabel->SetFont(DefaultFont);
	m_pPingLabel->SetFgColor(Color(255,255,255,255));
	m_pKickCheck->SetFont(Verdana);
	m_pLeaderCheck->SetFont(Verdana);
	//m_pMuteCheck->SetFont(Verdana);
	//for (int i=0;i<MAX_VOTE_ICONS;i++)
	//{
		//m_pKickVoteIcon[i]->SetDrawColor(Color(65,74,96,255));
		//m_pLeaderVoteIcon[i]->SetDrawColor(Color(65,74,96,255));
	//}
	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);
	SetBgColor(Color(0,0,0,128));	
}

void PlayerListLine::PerformLayout()
{
	float fScale = (ScreenHeight() / 768.0f);
	int top = 6.0f * fScale;
	int top_line_height = 16.0f * fScale;
	int bottom_line_top = top_line_height + top;
	int bottom_line_height = 16.0f * fScale;	

	m_pPlayerLabel->SetBounds(PLAYER_LIST_PLAYER_X * fScale, top, PLAYER_LIST_PLAYER_W * fScale, top_line_height);
	m_pMarinesLabel->SetBounds(PLAYER_LIST_MARINES_X * fScale, top, PLAYER_LIST_MARINES_W * fScale, top_line_height);
	m_pPingLabel->SetBounds(PLAYER_LIST_PING_X * fScale, top, PLAYER_LIST_PING_W * fScale, top_line_height);
	m_pLeaderCheck->SetBounds(PLAYER_LIST_LEADER_CHECK_X * fScale, bottom_line_top, PLAYER_LIST_LEADER_CHECK_W * fScale, bottom_line_height);
	m_pKickCheck->SetBounds(PLAYER_LIST_KICK_CHECK_X * fScale, bottom_line_top, PLAYER_LIST_KICK_CHECK_W * fScale, bottom_line_height);
	//m_pMuteCheck->SetBounds(PLAYER_LIST_MUTE_CHECK_X * fScale, bottom_line_top, PLAYER_LIST_MUTE_CHECK_W * fScale, bottom_line_height);

	for (int i=0;i<MAX_VOTE_ICONS;i++)
	{
		m_pKickVoteIcon[i]->SetBounds((PLAYER_LIST_KICK_ICON_X + (PLAYER_LIST_KICK_ICON_W * i)) * fScale, bottom_line_top,
							PLAYER_LIST_KICK_ICON_W * fScale, bottom_line_height);
		m_pLeaderVoteIcon[i]->SetBounds((PLAYER_LIST_LEADER_ICON_X + (PLAYER_LIST_LEADER_ICON_W * i)) * fScale, bottom_line_top,
							PLAYER_LIST_LEADER_ICON_W * fScale, bottom_line_height);
	}	
}

void PlayerListLine::OnThink()
{
	if (m_iPlayerIndex != -1 && ASWGameResource())
	{
		const char *name = g_PR->GetPlayerName(m_iPlayerIndex);
		char buffer[64];
		// check name is the same, update label if not
		if (m_iPlayerIndex == ASWGameResource()->GetLeaderEntIndex())
		{
			Q_snprintf(buffer, sizeof(buffer), "%s (leader)", name);
		}
		else
		{
			Q_snprintf(buffer, sizeof(buffer), "%s", name);
		}
		if (Q_strcmp(buffer, m_szPlayerName))
		{
			Q_strcpy(m_szPlayerName, buffer);
			//Msg("Setting player label to %s\n", buffer);
			m_pPlayerLabel->SetText(buffer);
		}
		// check ping (todo: only every so often?)
		const char *ping = GetPingString();
		if (Q_strcmp(ping, m_szPingString))
		{
			Q_strcpy(m_szPingString, ping);
			m_pPingLabel->SetText(ping);
		}
		
		// check marines
		wchar_t *marines = GetMarineNames();
		if ( wcscmp( marines, m_wszMarineNames ) )
		{
			_snwprintf( m_wszMarineNames, sizeof( m_wszMarineNames ), L"%s", marines );
			m_pMarinesLabel->SetText(marines);
		}
	}
	UpdateCheckBoxes();
	UpdateVoteIcons();
}

wchar_t* PlayerListLine::GetMarineNames()
{
	static wchar_t marines[96];
	wchar_t buffer[96];
	marines[0] = '\0';

	if ( !ASWGameResource() )
		return marines;

	int iMarines = 0;
	for (int i=0;i<ASWGameResource()->GetMaxMarineResources();i++)
	{
		C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(i);
		if (pMR && pMR->GetCommanderIndex() == m_iPlayerIndex && pMR->GetProfile())
		{
			if (iMarines == 0)
				_snwprintf(marines, sizeof(marines), L"%s", g_pVGuiLocalize->FindSafe( pMR->GetProfile()->m_ShortName ) );
			else
			{
				_snwprintf(buffer, sizeof(buffer), L"%s, %s", marines, g_pVGuiLocalize->FindSafe( pMR->GetProfile()->m_ShortName) );
				_snwprintf(marines, sizeof(marines), L"%s", buffer);
			}
			iMarines++;
		}
	}
	return marines;
}

void PlayerListLine::UpdateCheckBoxes()
{
	// make sure our selected/unselected status matches the selected index from our parent
	PlayerListPanel *pPanel = dynamic_cast<PlayerListPanel*>(GetParent());
	if (pPanel)
	{
		bool bKick = (pPanel->m_iKickVoteIndex == m_iPlayerIndex) && m_iPlayerIndex != -1;
		bool bLeader = (pPanel->m_iLeaderVoteIndex == m_iPlayerIndex) && m_iPlayerIndex != -1;
		if (m_pKickCheck->IsSelected() != bKick)
			m_pKickCheck->SetSelected(bKick);
		if (m_pLeaderCheck->IsSelected() != bLeader)
			m_pLeaderCheck->SetSelected(bLeader);
	}
/*
	if (m_bKickChecked != m_pKickCheck->IsSelected())
	{
		m_bKickChecked = m_pKickCheck->IsSelected();
		
		if (m_bKickChecked)
		{
			PlayerListPanel* pPanel = dynamic_cast<PlayerListPanel*>(GetParent());
			if (pPanel)
			{
				pPanel->KickChecked(this);	// notify parent when we change 
			}
		}
		else
		{
			Msg("Clearing kickvote\n");
			char buffer[64];
			Q_snprintf(buffer, sizeof(buffer), "cl_kickvote -1");
			engine->ClientCmd(buffer);
		}
	}

	if (m_bLeaderChecked != m_pLeaderCheck->IsSelected())
	{
		m_bLeaderChecked = m_pLeaderCheck->IsSelected();
		
		if (m_bLeaderChecked)
		{
			PlayerListPanel* pPanel = dynamic_cast<PlayerListPanel*>(GetParent());
			if (pPanel)
			{
				pPanel->LeaderChecked(this);
			}
		}
		else
		{
			Msg("Clearing leadervote\n");
			char buffer[64];
			Q_snprintf(buffer, sizeof(buffer), "cl_leadervote -1");
			engine->ClientCmd(buffer);
		}
	}
	*/
}

void PlayerListLine::UpdateVoteIcons()
{
	// count how many players are online
	int iPlayers = 0;
	for (int i=1;i<=gpGlobals->maxClients;i++)
	{
		if (g_PR->IsConnected(i))
			iPlayers++;
	}
	//Msg("%d players connected ", iPlayers);

	int iMaxLeaderVotes = asw_vote_leader_fraction.GetFloat() * iPlayers;
	// make sure we're not rounding down the number of needed players
	if ((float(iPlayers) * asw_vote_leader_fraction.GetFloat()) > iMaxLeaderVotes)
		iMaxLeaderVotes++;
	if (iMaxLeaderVotes < 2)
		iMaxLeaderVotes = 2;
	int iMaxKickVotes = asw_vote_kick_fraction.GetFloat() * iPlayers;
	// make sure we're not rounding down the number of needed players
	if ((float(iPlayers) * asw_vote_kick_fraction.GetFloat()) > iMaxKickVotes)
		iMaxKickVotes++;
	if (iMaxKickVotes < 2)
		iMaxKickVotes = 2;
	int iKickVotes = 0;
	if ( ASWGameResource() && m_iPlayerIndex < ASW_MAX_READY_PLAYERS )
		iKickVotes = ASWGameResource()->m_iKickVotes[m_iPlayerIndex-1];
	int iLeaderVotes = 0;
	if ( ASWGameResource() && m_iPlayerIndex < ASW_MAX_READY_PLAYERS )
		iLeaderVotes = ASWGameResource()->m_iLeaderVotes[m_iPlayerIndex-1];

	// position the checkbox immediately to the right of our number of votes
	float fScale = ScreenHeight() / 768.0f;
	int top = 6.0f * fScale;
	int top_line_height = 16.0f * fScale;	
	m_pLeaderCheck->SetPos((PLAYER_LIST_LEADER_ICON_X + PLAYER_LIST_LEADER_ICON_W * iMaxLeaderVotes) * fScale, top + top_line_height);
	m_pKickCheck->SetPos((PLAYER_LIST_KICK_ICON_X + PLAYER_LIST_KICK_ICON_W * iMaxKickVotes) * fScale, top + top_line_height);

	//Msg(" maxleader = %d maxkick = %d leader = %d kick = %d\n", iMaxLeaderVotes, iMaxKickVotes, iLeaderVotes, iKickVotes);
	
	// decide upon states for the icons
	int iKickIconState[MAX_VOTE_ICONS];
	int iLeaderIconState[MAX_VOTE_ICONS];
	for (int i=0;i<MAX_VOTE_ICONS;i++)
	{
		if (i >= iMaxKickVotes)
		{
			iKickIconState[i] = 0;			
		}
		else
		{
			if (i >= iKickVotes)
			{
				iKickIconState[i] = 1;
			}
			else
			{
				iKickIconState[i] = 2;
			}
		}
		if (i >= iMaxLeaderVotes)
		{
			iLeaderIconState[i] = 0;			
		}
		else
		{
			if (i >= iLeaderVotes)
			{
				iLeaderIconState[i] = 1;
			}
			else
			{
				iLeaderIconState[i] = 2;
			}
		}
	}

	// make sure visibility and images match up with the decided upon states for each icon
	for (int i=0;i<MAX_VOTE_ICONS;i++)
	{
		if (m_iKickIconState[i] != iKickIconState[i])
		{
			m_iKickIconState[i] = iKickIconState[i];
			if (m_iKickIconState[i] == 0)
			{
				m_pKickVoteIcon[i]->SetVisible(false);
			}
			else if (m_iKickIconState[i] == 1)
			{
				m_pKickVoteIcon[i]->SetVisible(true);
				//m_pKickVoteIcon[i]->SetImage("swarm/PlayerList/DashIcon");
				m_pKickVoteIcon[i]->SetDrawColor(Color(65,74,96,255));		
			}
			else if (m_iKickIconState[i] == 2)
			{
				m_pKickVoteIcon[i]->SetVisible(true);
				m_pKickVoteIcon[i]->SetDrawColor(Color(255,255,255,255));
				//m_pKickVoteIcon[i]->SetImage("swarm/PlayerList/BootIcon");
			}
		}
		if (m_iLeaderIconState[i] != iLeaderIconState[i])
		{
			m_iLeaderIconState[i] = iLeaderIconState[i];
			if (m_iLeaderIconState[i] == 0)
			{
				m_pKickVoteIcon[i]->SetVisible(false);
			}
			else if (m_iLeaderIconState[i] == 1)
			{
				m_pLeaderVoteIcon[i]->SetVisible(true);
				//m_pLeaderVoteIcon[i]->SetImage("swarm/PlayerList/DashIcon");
				m_pLeaderVoteIcon[i]->SetDrawColor(Color(65,74,96,255));
			}
			else if (m_iLeaderIconState[i] == 2)
			{
				m_pLeaderVoteIcon[i]->SetVisible(true);
				//m_pLeaderVoteIcon[i]->SetImage("swarm/PlayerList/LeaderIcon");
				m_pLeaderVoteIcon[i]->SetDrawColor(Color(255,255,255,255));
			}
		}
	}
}

bool PlayerListLine::SetPlayerIndex(int i)
{
	if (i != m_iPlayerIndex)
	{
		m_iPlayerIndex = i;
		// todo: update with his marines etc?
		return true;
	}
	return false;
}

const char* PlayerListLine::GetPingString()
{
	static char buffer[12];
	if (g_PR->GetPing( m_iPlayerIndex ) < 1)
	{
		if ( g_PR->IsFakePlayer( m_iPlayerIndex ) )
		{
			return "BOT";
		}
		else
		{
			return "";
		}
	}
	else
	{
		Q_snprintf(buffer, sizeof(buffer), "%d", g_PR->GetPing(m_iPlayerIndex));
		return buffer;
	}
	return "";
}

//======================================================

VoteCheck::VoteCheck(Panel *parent, const char *panelName, const char *text) :
	vgui::CheckButton(parent, panelName, text)
{

}

void VoteCheck::DoClick()
{
	if (GetParent() && GetParent()->GetParent())
	{
		PlayerListLine *pLine = dynamic_cast<PlayerListLine*>(GetParent());
		PlayerListPanel *pPanel = dynamic_cast<PlayerListPanel*>(GetParent()->GetParent());
		if (pLine && pPanel)
		{
			if (pLine->m_pKickCheck == this)
				pPanel->KickClicked(pLine);
			else if (pLine->m_pLeaderCheck == this)
				pPanel->LeaderClicked(pLine);
		}
	}
}