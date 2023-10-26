//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VVoteOptions.h"
#include "VFooterPanel.h"
#include "VGenericConfirmation.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui_controls/Button.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
VoteOptions::VoteOptions(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetDeleteSelfOnClose(true);
	SetTitle("#L4D360UI_VoteOptions", false);

	m_BtnChangeScenario = new Button(this, "BtnChangeScenario", "#L4D360UI_ChangeScenario", this, "ChangeScenario");
	m_BtnChangeDifficulty = new Button(this, "BtnChangeDifficulty", "#L4D360UI_ChangeDifficulty", this, "ChangeDifficulty");
	m_BtnRestartScenario = new Button(this, "BtnRestartScenario", "#L4D360UI_RestartScenario", this, "RestartScenario");

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);

	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( 0, FF_NONE, true );
	}

	m_ActiveControl = m_BtnChangeScenario;
}

//=============================================================================
VoteOptions::~VoteOptions()
{
	delete m_BtnChangeScenario;
	delete m_BtnChangeDifficulty;
	delete m_BtnRestartScenario;
}

//=============================================================================
void VoteOptions::OnCommand(const char *command)
{
	if(!Q_strcmp(command, "ChangeScenario"))
	{
		CBaseModPanel::GetSingleton().OpenWindow(WT_INGAMECHAPTERSELECT, this);
	}
	else if(!Q_strcmp(command, "ChangeDifficulty"))
	{
		CBaseModPanel::GetSingleton().OpenWindow(WT_INGAMEDIFFICULTYSELECT, this, false);
	}
	else if(!Q_strcmp(command, "RestartScenario"))
	{
		int iUser = GetGameUIActiveSplitScreenPlayerSlot();
		GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( iUser );

		engine->ClientCmd("gameui_hide");
		engine->ClientCmd("callvote RestartGame;");
		Close();
	}
}