//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "CreateMultiplayerGameBotPage.h"

using namespace vgui;

#include <KeyValues.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>

#include "FileSystem.h"
#include "PanelListPanel.h"
#include "ScriptObject.h"
#include "tier1/convar.h"
#include "EngineInterface.h"
#include "CvarToggleCheckButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// for join team combo box
enum BotGUITeamType
{
	BOT_GUI_TEAM_RANDOM	= 0,
	BOT_GUI_TEAM_CT			= 1,
	BOT_GUI_TEAM_T			= 2
};

// these must correlate with above enum
static const char *joinTeamArg[] = { "any", "ct", "t", NULL };


// for bot chatter combo box
enum BotGUIChatterType
{
	BOT_GUI_CHATTER_NORMAL = 0,
	BOT_GUI_CHATTER_MINIMAL = 1,
	BOT_GUI_CHATTER_RADIO = 2,
	BOT_GUI_CHATTER_OFF = 3
};

// these must correlate with above enum
static const char *chatterArg[] = { "normal", "minimal", "radio", "off", NULL };


extern void UTIL_StripInvalidCharacters( char *pszInput );


//-----------------------------------------------------------------------------
void CCreateMultiplayerGameBotPage::SetJoinTeamCombo( const char *team )
{
	if (team)
	{
		for( int i=0; joinTeamArg[i]; ++i )
			if (!stricmp( team, joinTeamArg[i] ))
			{
				m_joinTeamCombo->ActivateItemByRow( i );
				return;
			}
	}
	else
	{
		m_joinTeamCombo->ActivateItemByRow( BOT_GUI_TEAM_RANDOM );
	}
}

//-----------------------------------------------------------------------------
void CCreateMultiplayerGameBotPage::SetChatterCombo( const char *chatter )
{
	if (chatter)
	{
		for( int i=0; chatterArg[i]; ++i )
			if (!stricmp( chatter, chatterArg[i] ))
			{
				m_chatterCombo->ActivateItemByRow( i );
				return;
			}
	}
	else
	{
		m_joinTeamCombo->ActivateItemByRow( BOT_GUI_CHATTER_NORMAL );
	}
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameBotPage::CCreateMultiplayerGameBotPage( vgui::Panel *parent, const char *name, KeyValues *botKeys ) : PropertyPage( parent, name )
{
	m_pSavedData = botKeys;

	m_allowRogues = new CCvarToggleCheckButton( this, "BotAllowRogueCheck", "", "bot_allow_rogues" );
	m_allowPistols = new CCvarToggleCheckButton( this, "BotAllowPistolsCheck", "", "bot_allow_pistols" );
	m_allowShotguns = new CCvarToggleCheckButton( this, "BotAllowShotgunsCheck", "", "bot_allow_shotguns" );
	m_allowSubmachineGuns = new CCvarToggleCheckButton( this, "BotAllowSubmachineGunsCheck", "", "bot_allow_sub_machine_guns" );
	m_allowRifles = new CCvarToggleCheckButton( this, "BotAllowRiflesCheck", "", "bot_allow_rifles" );
	m_allowMachineGuns = new CCvarToggleCheckButton( this, "BotAllowMachineGunsCheck", "", "bot_allow_machine_guns" );
	m_allowGrenades = new CCvarToggleCheckButton( this, "BotAllowGrenadesCheck", "", "bot_allow_grenades" );
	m_allowSnipers = new CCvarToggleCheckButton( this, "BotAllowSnipersCheck", "", "bot_allow_snipers" );
#ifdef CS_SHIELD_ENABLED
	m_allowShields = new CCvarToggleCheckButton( this, "BotAllowShieldCheck", "", "bot_allow_shield" );
#endif // CS_SHIELD_ENABLED

	m_joinAfterPlayer = new CCvarToggleCheckButton( this, "BotJoinAfterPlayerCheck", "", "bot_join_after_player" );

	m_deferToHuman = new CCvarToggleCheckButton( this, "BotDeferToHumanCheck", "", "bot_defer_to_human" );

	// set up team join combo box
	// NOTE: If order of AddItem is changed, update the associated enum
	m_joinTeamCombo = new ComboBox( this, "BotJoinTeamCombo", 3, false );
	m_joinTeamCombo->AddItem( "#Cstrike_Random", NULL );
	m_joinTeamCombo->AddItem( "#Cstrike_ScoreBoard_CT", NULL );
	m_joinTeamCombo->AddItem( "#Cstrike_ScoreBoard_Ter", NULL );

	// set up chatter combo box
	// NOTE: If order of AddItem is changed, update the associated enum
	m_chatterCombo = new ComboBox( this, "BotChatterCombo", 4, false );
	m_chatterCombo->AddItem( "#Cstrike_Bot_Chatter_Normal", NULL );
	m_chatterCombo->AddItem( "#Cstrike_Bot_Chatter_Minimal", NULL );
	m_chatterCombo->AddItem( "#Cstrike_Bot_Chatter_Radio", NULL );
	m_chatterCombo->AddItem( "#Cstrike_Bot_Chatter_Off", NULL );

	// create text entry fields for quota and prefix
	m_prefixEntry = new TextEntry( this, "BotPrefixEntry" );

	// set positions and sizes from resources file
	LoadControlSettings( "Resource/CreateMultiplayerGameBotPage.res" );

	// get initial values from bot keys
	m_joinAfterPlayer->SetSelected( botKeys->GetBool( "bot_join_after_player", true ) );
	m_allowRogues->SetSelected( botKeys->GetBool( "bot_allow_rogues", true ) );
	m_allowPistols->SetSelected( botKeys->GetBool( "bot_allow_pistols", true ) );
	m_allowShotguns->SetSelected( botKeys->GetBool( "bot_allow_shotguns", true ) );
	m_allowSubmachineGuns->SetSelected( botKeys->GetBool( "bot_allow_sub_machine_guns", true ) );
	m_allowMachineGuns->SetSelected( botKeys->GetBool( "bot_allow_machine_guns", true ) );
	m_allowRifles->SetSelected( botKeys->GetBool( "bot_allow_rifles", true ) );
	m_allowSnipers->SetSelected( botKeys->GetBool( "bot_allow_snipers", true ) );
	m_allowGrenades->SetSelected( botKeys->GetBool( "bot_allow_grenades", true ) );
#ifdef CS_SHIELD_ENABLED
	m_allowShields->SetSelected( botKeys->GetBool( "bot_allow_shield", true ) );
#endif // CS_SHIELD_ENABLED
	m_deferToHuman->SetSelected( botKeys->GetBool( "bot_defer_to_human", true ) );

	SetJoinTeamCombo( botKeys->GetString( "bot_join_team", "any" ) );
	SetChatterCombo( botKeys->GetString( "bot_chatter", "normal" ) );

	// set bot_prefix
	const char *prefix = botKeys->GetString( "bot_prefix" );
	if (prefix)
		SetControlString( "BotPrefixEntry", prefix );
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameBotPage::~CCreateMultiplayerGameBotPage()
{
	// vgui handles deletion of children automatically through the hierarchy
}

//-----------------------------------------------------------------------------
// Reset values
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameBotPage::OnResetChanges()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void UpdateValue( KeyValues *data, const char *cvarName, int value )
{
	data->SetInt( cvarName, value );

	ConVarRef var( cvarName );
	var.SetValue( value );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void UpdateValue( KeyValues *data, const char *cvarName, const char *value )
{
	data->SetString( cvarName, value );

	ConVarRef var( cvarName );
	var.SetValue( value );
}

//-----------------------------------------------------------------------------
// Called to get data from the page
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameBotPage::OnApplyChanges()
{
	UpdateValue( m_pSavedData, "bot_join_after_player", m_joinAfterPlayer->IsSelected() );
	UpdateValue( m_pSavedData, "bot_allow_rogues", m_allowRogues->IsSelected() );
	UpdateValue( m_pSavedData, "bot_allow_pistols", m_allowPistols->IsSelected() );
	UpdateValue( m_pSavedData, "bot_allow_shotguns", m_allowShotguns->IsSelected() );
	UpdateValue( m_pSavedData, "bot_allow_sub_machine_guns", m_allowSubmachineGuns->IsSelected() );
	UpdateValue( m_pSavedData, "bot_allow_machine_guns", m_allowMachineGuns->IsSelected() );
	UpdateValue( m_pSavedData, "bot_allow_rifles", m_allowRifles->IsSelected() );
	UpdateValue( m_pSavedData, "bot_allow_snipers", m_allowSnipers->IsSelected() );
	UpdateValue( m_pSavedData, "bot_allow_grenades", m_allowGrenades->IsSelected() );
#ifdef CS_SHIELD_ENABLED
	UpdateValue( m_pSavedData, "bot_allow_shield", m_allowShields->IsSelected() );
#endif // CS_SHIELD_ENABLED
	UpdateValue( m_pSavedData, "bot_defer_to_human", m_deferToHuman->IsSelected() );

	// set bot_join_team
	UpdateValue( m_pSavedData, "bot_join_team", joinTeamArg[ m_joinTeamCombo->GetActiveItem() ] );

	// set bot_chatter
	UpdateValue( m_pSavedData, "bot_chatter", chatterArg[ m_chatterCombo->GetActiveItem() ] );

	// set bot_prefix
	#define BUF_LENGTH 256
	char entryBuffer[ BUF_LENGTH ];
	m_prefixEntry->GetText( entryBuffer, BUF_LENGTH );
	UpdateValue( m_pSavedData, "bot_prefix", entryBuffer );
}

