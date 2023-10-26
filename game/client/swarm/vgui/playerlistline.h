#ifndef _INCLUDED_PLAYER_LIST_LINE_H
#define _INCLUDED_PLAYER_LIST_LINE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
	class ImagePanel;
	class CheckButton;
	class Label;
};

#define MAX_VOTE_ICONS 8

#define PLAYER_LIST_PLAYER_X 32
#define PLAYER_LIST_PLAYER_W 210
#define PLAYER_LIST_MARINES_X 260
#define PLAYER_LIST_MARINES_W 250
#define PLAYER_LIST_PING_X 540
#define PLAYER_LIST_PING_W 60	

#define PLAYER_LIST_LEADER_CHECK_X 200
#define PLAYER_LIST_LEADER_CHECK_W 128
#define PLAYER_LIST_KICK_CHECK_X 500
#define PLAYER_LIST_KICK_CHECK_W 128
#define PLAYER_LIST_MUTE_CHECK_X 630
#define PLAYER_LIST_MUTE_CHECK_W 128
#define PLAYER_LIST_KICK_ICON_X 260
#define PLAYER_LIST_KICK_ICON_W 16
#define PLAYER_LIST_LEADER_ICON_X 40
#define PLAYER_LIST_LEADER_ICON_W 16

class VoteCheck : public vgui::CheckButton
{
public:
	VoteCheck(Panel *parent, const char *panelName, const char *text);
	virtual void DoClick();
};

// a single line in the PlayerListPanel, showing info on a particular connected player along with buttons to vote on them
class PlayerListLine : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( PlayerListLine, vgui::Panel );
public:
	PlayerListLine(vgui::Panel *parent, const char *name);
	
	virtual void OnThink();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	bool SetPlayerIndex(int i);
	void UpdateVoteIcons();
	void UpdateCheckBoxes();

	vgui::Label *m_pPlayerLabel;
	vgui::Label *m_pMarinesLabel;
	vgui::Label *m_pPingLabel;
	VoteCheck *m_pKickCheck;
	VoteCheck *m_pLeaderCheck;
	VoteCheck *m_pMuteCheck;
	vgui::ImagePanel *m_pKickVoteIcon[MAX_VOTE_ICONS];
	vgui::ImagePanel *m_pLeaderVoteIcon[MAX_VOTE_ICONS];
	
	const char* GetPingString();
	wchar_t* GetMarineNames();
	
	bool m_bKickChecked;
	bool m_bLeaderChecked;
	int m_iPlayerIndex;
	char m_szPlayerName[32];
	char m_szPingString[12];
	wchar_t m_wszMarineNames[96];
	int m_iKickIconState[MAX_VOTE_ICONS];
	int m_iLeaderIconState[MAX_VOTE_ICONS];
};

#endif // _INCLUDED_PLAYER_LIST_LINE_H