//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VInGameDifficultySelect.h"
#include "VFooterPanel.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "VHybridButton.h"
#include "vgui/ILocalize.h"
#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
InGameDifficultySelect::InGameDifficultySelect(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetDeleteSelfOnClose(true);
	SetProportional( true );

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);

	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( 0, FF_NONE, true );
	}
}

//=============================================================================
InGameDifficultySelect::~InGameDifficultySelect()
{
}

void InGameDifficultySelect::PaintBackground()
{
	BaseClass::DrawGenericBackground();
}

void InGameDifficultySelect::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled( true );
}


//=============================================================================
void InGameDifficultySelect::LoadLayout()
{
	BaseClass::LoadLayout();

	CGameUIConVarRef z_difficulty("z_difficulty");

	if ( z_difficulty.IsValid() )
	{
		// set a label that tells us what the current difficulty is
		char chBuffer[64];
		Q_snprintf( chBuffer, ARRAYSIZE( chBuffer ), "#L4D360UI_Difficulty_%s", z_difficulty.GetString() );

		const char *pszDifficultyLoc = chBuffer;
		wchar_t *pwcDifficulty = g_pVGuiLocalize->Find( pszDifficultyLoc );
		if ( pwcDifficulty )
		{
			wchar_t szWideBuff[200];		
			g_pVGuiLocalize->ConstructString( szWideBuff, sizeof( szWideBuff ), g_pVGuiLocalize->Find( "#L4D360UI_GameSettings_Current_Difficulty" ), 1, pwcDifficulty );

			SetControlString( "LblCurrentDifficulty", szWideBuff );
		}
	
		// Disable the current difficulty's button, and navigate to it.
		BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( FindChildByName( VarArgs( "Btn%s", z_difficulty.GetString() ) ) );
		if ( pButton )
		{
			pButton->SetEnabled( false );
			pButton->NavigateTo();
		}
	}
}

//=============================================================================
void InGameDifficultySelect::OnCommand(const char *command)
{
	if ( !Q_strcmp( command, "Easy" ) ||
		 !Q_strcmp( command, "Normal" ) ||
		 !Q_strcmp( command, "Hard" ) ||
		 !Q_strcmp( command, "Insane" ) ||
		 !Q_strcmp( command, "Imba" ) )
	{
		CGameUIConVarRef z_difficulty("z_difficulty");

		if ( Q_stricmp( command, z_difficulty.GetString() ) != 0 )
		{
			// Selected a new difficulty
			int iUser = GetGameUIActiveSplitScreenPlayerSlot();
			GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( iUser );

			static char voteDifficultyString[128];
			Q_snprintf( voteDifficultyString, 128, "callvote ChangeDifficulty %s;", command );

			engine->ClientCmd( "gameui_hide" );
			engine->ClientCmd( voteDifficultyString );
		}
		else
		{
			engine->ClientCmd( "gameui_hide" );
		}

		Close();
	}
	else if ( !Q_strcmp( command, "Cancel" ) )
	{
		engine->ClientCmd("gameui_hide");		
		Close();
	}
}