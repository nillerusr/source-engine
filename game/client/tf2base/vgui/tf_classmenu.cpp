//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_classmenu.h"

#include <KeyValues.h>
#include <filesystem.h>
#include <vgui_controls/Button.h>
#include <vgui/IVGui.h>

#include "hud.h" // for gEngfuncs
#include "c_tf_player.h"
#include "c_tf_team.h"
#include "c_tf_playerresource.h"

#include "tf_controls.h"
#include "vguicenterprint.h"
#include "imagemouseoverbutton.h"
#include "IconPanel.h"

#include "IGameUIFuncs.h" // for key bindings

#include "tf_hud_notification_panel.h"

extern IGameUIFuncs *gameuifuncs; // for key binding details

using namespace vgui;

ConVar _cl_classmenuopen( "_cl_classmenuopen", "0", 0, "internal cvar used to tell server when class menu is open" );

// menu buttons are not in the same order as the defines
static int iRemapIndexToClass[TF_CLASS_MENU_BUTTONS] = 
{
	0,
	TF_CLASS_SCOUT,
	TF_CLASS_SOLDIER,
	TF_CLASS_PYRO,
	TF_CLASS_DEMOMAN,
	TF_CLASS_HEAVYWEAPONS,
	TF_CLASS_ENGINEER,
	TF_CLASS_MEDIC,
	TF_CLASS_SNIPER,
	TF_CLASS_SPY,
	0,
	0,
	TF_CLASS_RANDOM
};

int GetIndexForClass( int iClass )
{
	int iIndex = 0;

	for ( int i = 0 ; i < TF_CLASS_MENU_BUTTONS ; i++ )
	{
		if ( iRemapIndexToClass[i] == iClass )
		{
			iIndex = i;
			break;
		}
	}

	return iIndex;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFClassMenu::CTFClassMenu( IViewPort *pViewPort ) : CClassMenu( pViewPort )
{
	m_mouseoverButtons.RemoveAll();

	m_iClassMenuKey = BUTTON_CODE_INVALID;
	m_iCurrentClassIndex = TF_CLASS_HEAVYWEAPONS;

#ifdef _X360
	m_pFooter = new CTFFooter( this, "Footer" );
#endif

	m_pClassInfoPanel = new CTFClassInfoPanel( this, "ClassInfoPanel" );
	LoadControlSettings( "Resource/UI/ClassInfoPanel.res" );

	Q_memset( m_pClassButtons, 0, sizeof( m_pClassButtons ) );

#ifndef _X360
	char tempName[MAX_PATH];
	for ( int i = 0 ; i < CLASS_COUNT_IMAGES ; ++i )
	{
		Q_snprintf( tempName, sizeof( tempName ), "countImage%d", i );
		m_ClassCountImages[i] = new CTFImagePanel( this, tempName );
	}

	m_pCountLabel = NULL;
#endif

	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClassMenu::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/ClassMenu.res" );
}

void CTFClassMenu::PerformLayout()
{
	BaseClass::PerformLayout();

#ifndef _X360
	m_pCountLabel = dynamic_cast< CTFLabel * >( FindChildByName( "CountLabel" ) );

	if ( m_pCountLabel )
	{
		m_pCountLabel->SizeToContents();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CImageMouseOverButton<CTFClassInfoPanel> *CTFClassMenu::GetCurrentClassButton()
{
	int iClass = TF_CLASS_HEAVYWEAPONS;
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pLocalPlayer && pLocalPlayer->m_Shared.GetDesiredPlayerClassIndex() != TF_CLASS_UNDEFINED )
	{
		iClass = pLocalPlayer->m_Shared.GetDesiredPlayerClassIndex();
	}

	m_iCurrentClassIndex = GetIndexForClass( iClass );
	return m_pClassButtons[iClass];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClassMenu::ShowPanel( bool bShow )
{
	if ( bShow )
	{
		// can't change class if you're on the losing team during the "bonus time" after a team has won the round
		if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN && 
			 C_TFPlayer::GetLocalTFPlayer() && 
			 C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() != TFGameRules()->GetWinningTeam()
			 && C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() != TEAM_SPECTATOR 
			 && C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() != TEAM_UNASSIGNED
			 && GetSpectatorMode() == OBS_MODE_NONE )
		{
			SetVisible( false );
			SetMouseInputEnabled( false );

			CHudNotificationPanel *pNotifyPanel = GET_HUDELEMENT( CHudNotificationPanel );
			if ( pNotifyPanel )
			{
				pNotifyPanel->SetupNotifyCustom( "#TF_CantChangeClassNow", "ico_notify_flag_moving", C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() );
			}

			return;
		}

		engine->CheckPoint( "ClassMenu" );

		Activate();
		SetMouseInputEnabled( true );

		m_iClassMenuKey = gameuifuncs->GetButtonCodeForBind( "changeclass" );
		m_iScoreBoardKey = gameuifuncs->GetButtonCodeForBind( "showscores" );

		CImageMouseOverButton<CTFClassInfoPanel> *pInitialButton = GetCurrentClassButton();

		for( int i = 0; i < GetChildCount(); i++ ) 
		{
			CImageMouseOverButton<CTFClassInfoPanel> *button = dynamic_cast<CImageMouseOverButton<CTFClassInfoPanel> *>( GetChild( i ) );

			if ( button )
			{
				if ( button == pInitialButton )
				{
					button->ShowPage();
					button->SetArmed( true );
					button->SetAsDefaultButton( 1 );

					g_lastButton = button;
				}
				else
				{
					button->HidePage();
					button->SetArmed( false );
				}
			}
		}
	}
	else
	{
		// turn off all of our ClassInfo panels so the VCDs will stop playing
		for( int i = 0; i < GetChildCount(); i++ ) 
		{
			CImageMouseOverButton<CTFClassInfoPanel> *button = dynamic_cast<CImageMouseOverButton<CTFClassInfoPanel> *>( GetChild( i ) );

			if ( button )
			{
				button->HidePage();
			}
		}

		// everything is off so just reset these for next time
		g_lastButton = NULL;
		g_lastPanel = NULL;

		SetVisible( false );
		SetMouseInputEnabled( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClassMenu::OnKeyCodePressed( KeyCode code )
{
	m_KeyRepeat.KeyDown( code );

	if ( ( m_iClassMenuKey != BUTTON_CODE_INVALID && m_iClassMenuKey == code ) ||
		code == KEY_XBUTTON_BACK || 
		code == KEY_XBUTTON_B )
	{
		C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

		if ( pLocalPlayer && ( pLocalPlayer->GetPlayerClass()->GetClassIndex() != TF_CLASS_UNDEFINED ) )
		{
			ShowPanel( false );
		}
	}
	else if( code == KEY_SPACE || code == KEY_XBUTTON_A || code == KEY_XBUTTON_RTRIGGER )
	{
		ipanel()->SendMessage( GetFocusNavGroup().GetDefaultButton(), new KeyValues( "PressButton" ), GetVPanel() );
	}
	else if( code == KEY_XBUTTON_RIGHT || code == KEY_XSTICK1_RIGHT )
	{
		int loopCheck = 0;

		do 
		{
			loopCheck++;
			m_iCurrentClassIndex++;
			m_iCurrentClassIndex = ( m_iCurrentClassIndex % TF_CLASS_MENU_BUTTONS );
		} while( ( m_pClassButtons[ iRemapIndexToClass[m_iCurrentClassIndex] ] == NULL ) && ( loopCheck < TF_CLASS_MENU_BUTTONS ) );
		
		CImageMouseOverButton<CTFClassInfoPanel> *pButton = m_pClassButtons[ iRemapIndexToClass[m_iCurrentClassIndex] ];
		if ( pButton )
		{
			pButton->OnCursorEntered();
		}
	}
	else if( code == KEY_XBUTTON_LEFT || code == KEY_XSTICK1_LEFT )
	{
		int loopCheck = 0;

		do 
		{
			loopCheck++;
			m_iCurrentClassIndex--;
			if ( m_iCurrentClassIndex <= 0 )
			{
				m_iCurrentClassIndex = TF_CLASS_RANDOM;
			}
		} while( ( m_pClassButtons[ iRemapIndexToClass[m_iCurrentClassIndex] ] == NULL ) && ( loopCheck < TF_CLASS_MENU_BUTTONS ) );

		CImageMouseOverButton<CTFClassInfoPanel> *pButton = m_pClassButtons[ iRemapIndexToClass[m_iCurrentClassIndex] ];
		if ( pButton )
		{
			pButton->OnCursorEntered();
		}
	}
	else if( code == KEY_XBUTTON_UP || code == KEY_XSTICK1_UP )
	{
		// Scroll class info text up
		if ( g_lastPanel )
		{
			CTFRichText *pRichText = dynamic_cast< CTFRichText * >( g_lastPanel->FindChildByName( "classInfo" ) );

			if ( pRichText )
			{
				PostMessage( pRichText, new KeyValues("MoveScrollBarDirect", "delta", 1) );
			}
		}
	}
	else if( code == KEY_XBUTTON_DOWN || code == KEY_XSTICK1_DOWN )
	{
		// Scroll class info text up
		if ( g_lastPanel )
		{
			CTFRichText *pRichText = dynamic_cast< CTFRichText * >( g_lastPanel->FindChildByName( "classInfo" ) );

			if ( pRichText )
			{
				PostMessage( pRichText, new KeyValues("MoveScrollBarDirect", "delta", -1) );
			}
		}
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClassMenu::OnKeyCodeReleased( vgui::KeyCode code )
{
	m_KeyRepeat.KeyUp( code );

	BaseClass::OnKeyCodeReleased( code );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClassMenu::OnThink()
{
	vgui::KeyCode code = m_KeyRepeat.KeyRepeated();
	if ( code )
	{
		OnKeyCodePressed( code );
	}

	BaseClass::OnThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClassMenu::Update()
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	// Force them to pick a class if they haven't picked one yet.
	if ( ( pLocalPlayer && pLocalPlayer->m_Shared.GetDesiredPlayerClassIndex() != TF_CLASS_UNDEFINED ) )
	{
#ifdef _X360
		if ( m_pFooter )
		{
			m_pFooter->ShowButtonLabel( "cancel", true );
		}
#else
		SetVisibleButton( "CancelButton", true );
#endif
	}
	else
	{
#ifdef _X360
		if ( m_pFooter )
		{
			m_pFooter->ShowButtonLabel( "cancel", false );
		}
#else
		SetVisibleButton( "CancelButton", false );
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Panel *CTFClassMenu::CreateControlByName( const char *controlName )
{
	if ( !Q_stricmp( "CIconPanel", controlName ) )
	{
		return new CIconPanel( this, "icon_panel" );
	}
	else
	{
		return BaseClass::CreateControlByName( controlName );
	}
}

//-----------------------------------------------------------------------------
// Catch the mouseover event and set the active class
//-----------------------------------------------------------------------------
void CTFClassMenu::OnShowPage( const char *pagename )
{
}

//-----------------------------------------------------------------------------
// Draw nothing
//-----------------------------------------------------------------------------
void CTFClassMenu::PaintBackground( void )
{
}

//-----------------------------------------------------------------------------
// Do things that should be done often, eg number of players in the 
// selected class
//-----------------------------------------------------------------------------
void CTFClassMenu::OnTick( void )
{
	//When a player changes teams, their class and team values don't get here 
	//necessarily before the command to update the class menu. This leads to the cancel button 
	//being visible and people cancelling before they have a class. check for class == TF_CLASS_UNDEFINED and if so
	//hide the cancel button

	if ( !IsVisible() )
		return;

#ifndef _X360
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	// Force them to pick a class if they haven't picked one yet.
	if ( pLocalPlayer && pLocalPlayer->m_Shared.GetDesiredPlayerClassIndex() == TF_CLASS_UNDEFINED )
	{
		SetVisibleButton( "CancelButton", false );
	}

	UpdateClassCounts();

#endif

	BaseClass::OnTick();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClassMenu::OnClose()
{
	ShowPanel( false );

	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClassMenu::SetVisible( bool state )
{
	BaseClass::SetVisible( state );

	m_KeyRepeat.Reset();

	if ( state )
	{
		engine->ServerCmd( "menuopen" );			// to the server
		engine->ClientCmd( "_cl_classmenuopen 1" );	// for other panels
	}
	else
	{
		engine->ServerCmd( "menuclosed" );	
		engine->ClientCmd( "_cl_classmenuopen 0" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Console command to select a class
//-----------------------------------------------------------------------------
void CTFClassMenu::Join_Class( const CCommand &args )
{
	if ( args.ArgC() > 1 )
	{
		char cmd[256];
		Q_snprintf( cmd, sizeof( cmd ), "joinclass %s", args.Arg( 1 ) );
		OnCommand( cmd );
		ShowPanel( false );
	}
}

static const char *g_sDialogVariables[] = {
	"",
	"numScout",
	"numSoldier",
	"numPyro",

	"numDemoman",
	"numHeavy",
	"numEngineer",
	
	"numMedic",
	"numSniper",
	"numSpy",
	"",
};

static const char *g_sClassImagesBlue[] = {
	"",
	"class_sel_sm_scout_blu",
	"class_sel_sm_soldier_blu",
	"class_sel_sm_pyro_blu",

	"class_sel_sm_demo_blu",
	"class_sel_sm_heavy_blu",
	"class_sel_sm_engineer_blu",

	"class_sel_sm_medic_blu",
	"class_sel_sm_sniper_blu",
	"class_sel_sm_spy_blu",
	"",
};

static const char *g_sClassImagesRed[] = {
	"",
	"class_sel_sm_scout_red",
	"class_sel_sm_soldier_red",
	"class_sel_sm_pyro_red",
	
	"class_sel_sm_demo_red",
	"class_sel_sm_heavy_red",
	"class_sel_sm_engineer_red",
	
	"class_sel_sm_medic_red",
	"class_sel_sm_sniper_red",
	"class_sel_sm_spy_red",
	"",
};

static int g_sClassDefines[] = {
	0,
	TF_CLASS_SCOUT,	
	TF_CLASS_SOLDIER,
	TF_CLASS_PYRO,

	TF_CLASS_DEMOMAN,
	TF_CLASS_HEAVYWEAPONS,
	TF_CLASS_ENGINEER,

	TF_CLASS_MEDIC,
	TF_CLASS_SNIPER,
	TF_CLASS_SPY,
	0,
};

void CTFClassMenu::UpdateNumClassLabels( int iTeam )
{
#ifndef _X360
	int nTotalCount = 0;

	// count how many of each class there are
	C_TF_PlayerResource *tf_PR = dynamic_cast<C_TF_PlayerResource *>( g_PR );

	if ( !tf_PR )
		return;

	if ( iTeam < FIRST_GAME_TEAM || iTeam >= TF_TEAM_COUNT ) // invalid team number
		return;

	for( int i = TF_FIRST_NORMAL_CLASS ; i <= TF_LAST_NORMAL_CLASS ; i++ )
	{
		int classCount = tf_PR->GetCountForPlayerClass( iTeam, g_sClassDefines[i], true );

		if ( classCount > 0 )
		{
			SetDialogVariable( g_sDialogVariables[i], classCount );
		}
		else
		{
			SetDialogVariable( g_sDialogVariables[i], "" );
		}


		if ( nTotalCount < CLASS_COUNT_IMAGES )
		{
			for ( int j = 0 ; j < classCount ; ++j )
			{
				CTFImagePanel *pImage = m_ClassCountImages[nTotalCount];
				if ( pImage )
				{
					pImage->SetVisible( true );
					pImage->SetImage( iTeam == TF_TEAM_BLUE ? g_sClassImagesBlue[i] : g_sClassImagesRed[i] );
				}

				nTotalCount++;
				if ( nTotalCount >= CLASS_COUNT_IMAGES )
				{
					break;
				}
			}
		}
	}

	if ( nTotalCount == 0 )
	{
		// no classes for our team yet
		if ( m_pCountLabel && m_pCountLabel->IsVisible() )
		{
			m_pCountLabel->SetVisible( false );
		}
	}
	else
	{
		if ( m_pCountLabel && !m_pCountLabel->IsVisible() )
		{
			m_pCountLabel->SetVisible( true );
		}
	}

	// turn off any unused images
	while ( nTotalCount < CLASS_COUNT_IMAGES )
	{
		CTFImagePanel *pImage = m_ClassCountImages[nTotalCount];
		if ( pImage )
		{
			pImage->SetVisible( false );
		}

		nTotalCount++;
	}
#endif
}
