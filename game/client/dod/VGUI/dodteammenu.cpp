//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dodteammenu.h"
#include <convar.h>
#include "hud.h" // for gEngfuncs
#include "c_dod_player.h"
#include "dod_gamerules.h"
#include <vgui/ILocalize.h>
#include <vgui/IVGui.h>
#include <vgui_controls/RichText.h>
#include "c_dod_team.h"
#include "IGameUIFuncs.h" // for key bindings

extern IGameUIFuncs *gameuifuncs; // for key binding details

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODTeamMenu::CDODTeamMenu(IViewPort *pViewPort) : CTeamMenu(pViewPort)
{
	m_pBackground = SETUP_PANEL( new CDODMenuBackground( this ) );
	
	m_pPanel = new EditablePanel( this, "TeamImagePanel" );// team image panel
	
	m_pFirstButton = NULL;

	LoadControlSettings("Resource/UI/TeamMenu.res");	// reload this to catch DODButtons

	vgui::ivgui()->AddTickSignal( GetVPanel() );

	m_iActiveTeam = TEAM_UNASSIGNED;
	m_iLastPlayerCount = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDODTeamMenu::~CDODTeamMenu()
{
}

void CDODTeamMenu::ShowPanel(bool bShow)
{
	if ( bShow )
	{
		engine->CheckPoint( "TeamMenu" );	//MATTTODO what is this?

		m_iTeamMenuKey = gameuifuncs->GetButtonCodeForBind( "changeteam" );
	}
	
	BaseClass::ShowPanel( bShow );
}

//-----------------------------------------------------------------------------
// Purpose: Make the first buttons page get displayed when the menu becomes visible
//-----------------------------------------------------------------------------
void CDODTeamMenu::SetVisible( bool state )
{
	BaseClass::SetVisible( state );

	for( int i = 0; i< GetChildCount(); i++ ) // get all the buy buttons to performlayout
	{
		CDODMouseOverButton<EditablePanel> *button = dynamic_cast<CDODMouseOverButton<EditablePanel> *>(GetChild(i));
		if ( button )
		{
			if( button == m_pFirstButton && state == true )
				button->ShowPage();
			else
				button->HidePage();

			button->InvalidateLayout();
		}
	}

	if ( state )
	{
		Panel *pAutoButton = FindChildByName( "autobutton" );
		if ( pAutoButton )
		{
			pAutoButton->RequestFocus();
		}
	}
}

void CDODTeamMenu::OnTick( void )
{
	C_DODTeam *pAllies = dynamic_cast<C_DODTeam *>( GetGlobalTeam(TEAM_ALLIES) );
	C_DODTeam *pAxis = dynamic_cast<C_DODTeam *>( GetGlobalTeam(TEAM_AXIS) );

	if ( !pAllies || !pAxis )
		return;

	static int iLastAlliesCount = -1;
	static int iLastAxisCount = -1;

	int iNumAllies = pAllies->Get_Number_Players();
	int iNumAxis = pAxis->Get_Number_Players();

	if ( iNumAllies != iLastAlliesCount )
	{
		iLastAlliesCount = iNumAllies;

		wchar_t wbuf[128];

		if ( iNumAllies == 1 )
		{
			g_pVGuiLocalize->ConstructString( wbuf, sizeof(wbuf), g_pVGuiLocalize->Find("#teammenu_numAllies_1"), 0 );		
		}
		else
		{
			wchar_t wnum[6];
			_snwprintf( wnum, ARRAYSIZE(wnum), L"%d", iNumAllies );
			g_pVGuiLocalize->ConstructString( wbuf, sizeof(wbuf), g_pVGuiLocalize->Find("#teammenu_numAllies"), 1, wnum );
		}

		Label *pLabel = dynamic_cast<Label *>( FindChildByName("num_allies") );

		if ( pLabel )
			pLabel->SetText( wbuf );
	}

	if ( iNumAxis != iLastAxisCount )
	{
		iLastAxisCount = iNumAxis;

		wchar_t wbuf[128];

		if ( iNumAxis == 1 )
		{
			g_pVGuiLocalize->ConstructString( wbuf, sizeof(wbuf), g_pVGuiLocalize->Find("#teammenu_numAxis_1"), 0 );		
		}
		else
		{
			wchar_t wnum[6];
			_snwprintf( wnum, ARRAYSIZE(wnum), L"%d", iNumAxis );
			g_pVGuiLocalize->ConstructString( wbuf, sizeof(wbuf), g_pVGuiLocalize->Find("#teammenu_numAxis"), 1, wnum );
		}

		Label *pLabel = dynamic_cast<Label *>( FindChildByName("num_axis") );

		if ( pLabel )
			pLabel->SetText( wbuf );
	}
}

//-----------------------------------------------------------------------------
// Purpose: called to update the menu with new information
//-----------------------------------------------------------------------------
void CDODTeamMenu::Update( void )
{
	BaseClass::Update();

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	Assert( pPlayer );

	const ConVar *allowspecs =  cvar->FindVar( "mp_allowspectators" );	

	if ( allowspecs && allowspecs->GetBool() )
	{
		if ( !pPlayer || !DODGameRules() )
			return;

		SetVisibleButton("specbutton", true);
	}
	else
	{
		SetVisibleButton("specbutton", false );
	}

	if( pPlayer->GetTeamNumber() == TEAM_UNASSIGNED ) // we aren't on a team yet
	{
		SetVisibleButton("CancelButton", false); 
	}
	else
	{
		SetVisibleButton("CancelButton", true); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: When a team button is pressed it triggers this function to 
//			cause the player to join a team
//-----------------------------------------------------------------------------
void CDODTeamMenu::OnCommand( const char *command )
{
	if ( !FStrEq( command, "vguicancel" ) )
	{
		engine->ClientCmd( command );
	}
		
	BaseClass::OnCommand( command );

	gViewPortInterface->ShowBackGround( false );
	OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the visibility of a button by name
//-----------------------------------------------------------------------------
void CDODTeamMenu::SetVisibleButton(const char *textEntryName, bool state)
{
	Button *entry = dynamic_cast<Button *>(FindChildByName(textEntryName));
	if (entry)
	{
		entry->SetVisible(state);
	}
}

void CDODTeamMenu::ApplySchemeSettings( IScheme *pScheme )
{

	BaseClass::ApplySchemeSettings(pScheme);
}

//-----------------------------------------------------------------------------
// Draw nothing
//-----------------------------------------------------------------------------
void CDODTeamMenu::PaintBackground( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Panel *CDODTeamMenu::CreateControlByName( const char *controlName )
{
	if( !Q_stricmp( "DODMouseOverPanelButton", controlName ) )
	{
		CDODMouseOverButton<EditablePanel> *newButton = new CDODMouseOverButton<EditablePanel>( this, NULL, m_pPanel );
		
		if( !m_pFirstButton )
		{
			m_pFirstButton = newButton;
		}
		return newButton;
	}
	else if( !Q_stricmp( "DODButton", controlName ) )
	{
		return new CDODButton(this);
	}
	else if ( !Q_stricmp( "CIconPanel", controlName ) )
	{
		return new CIconPanel(this, "icon_panel");
	}
	else
	{
		return BaseClass::CreateControlByName( controlName );
	}
}

void CDODTeamMenu::OnShowPage( char const *pagename )
{
	if ( !pagename || !pagename[ 0 ] )
		return;

	if ( !Q_stricmp( pagename, "allies") )
	{
		m_iActiveTeam = TEAM_ALLIES;
	}
	else if ( !Q_stricmp( pagename, "axis" ) )
	{
		m_iActiveTeam = TEAM_AXIS;
	}
}

void CDODTeamMenu::OnKeyCodePressed(KeyCode code)
{
	if ( m_iTeamMenuKey != BUTTON_CODE_INVALID && m_iTeamMenuKey == code )
	{
		ShowPanel( false );
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}
