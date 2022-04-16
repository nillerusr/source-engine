//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dodclassmenu.h"

#include <KeyValues.h>
#include <filesystem.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/RichText.h>
#include <vgui/IVGui.h>

#include "hud.h" // for gEngfuncs
#include "c_dod_player.h"
#include "c_dod_team.h"

#include "imagemouseoverbutton.h"

#include "dodmouseoverpanelbutton.h"
#include "dodrandombutton.h"
#include "IconPanel.h"

#include "IGameUIFuncs.h" // for key bindings

extern IGameUIFuncs *gameuifuncs; // for key binding details

using namespace vgui;

extern ConVar hud_classautokill;
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Panel *CDODClassInfoPanel::CreateControlByName( const char *controlName )
{
	if( !Q_stricmp( "ProgressBar", controlName ) )
	{
		return new CDODProgressBar(this);
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

void CDODClassInfoPanel::ApplySchemeSettings( IScheme *pScheme )
{
	RichText *pClassInfo = dynamic_cast<RichText*>(FindChildByName("classInfo"));

	if ( pClassInfo )
	{
		pClassInfo->SetBorder(pScheme->GetBorder("NoBorder"));
		pClassInfo->SetBgColor(pScheme->GetColor("Blank", Color(0,0,0,0)));
	}

	BaseClass::ApplySchemeSettings( pScheme );
}

CDODClassMenu::CDODClassMenu(IViewPort *pViewPort) : CClassMenu(pViewPort)
{
	m_mouseoverButtons.RemoveAll();
	m_iClassMenuKey = BUTTON_CODE_INVALID;
	m_pInitialButton = NULL;

	m_pBackground = SETUP_PANEL( new CDODMenuBackground( this ) );

	m_pClassInfoPanel = new CDODClassInfoPanel( this, "ClassInfoPanel" );
	
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	m_iActivePlayerClass = -1;
	m_iLastPlayerClassCount = -1;

	m_pClassNumLabel[0] = new Label( this, "class_1_num", "" );
	m_pClassNumLabel[1] = new Label( this, "class_2_num", "" );
	m_pClassNumLabel[2] = new Label( this, "class_3_num", "" );
	m_pClassNumLabel[3] = new Label( this, "class_4_num", "" );
	m_pClassNumLabel[4] = new Label( this, "class_5_num", "" );
	m_pClassNumLabel[5] = new Label( this, "class_6_num", "" );

	m_pClassFullLabel[0] = new Label( this, "class_1_full", "" );
	m_pClassFullLabel[1] = new Label( this, "class_2_full", "" );
	m_pClassFullLabel[2] = new Label( this, "class_3_full", "" );
	m_pClassFullLabel[3] = new Label( this, "class_4_full", "" );
	m_pClassFullLabel[4] = new Label( this, "class_5_full", "" );
	m_pClassFullLabel[5] = new Label( this, "class_6_full", "" );

	m_pSuicideOption = new CheckButton( this, "suicide_option", "Sky is blue?" );
}

void CDODClassMenu::ShowPanel( bool bShow )
{
	if ( bShow )
	{
		engine->CheckPoint( "ClassMenu" );

		m_iClassMenuKey = gameuifuncs->GetButtonCodeForBind( "changeclass" );

		m_pSuicideOption->SetSelected( hud_classautokill.GetBool() );
	}

	for( int i = 0; i< GetChildCount(); i++ ) 
	{
		CImageMouseOverButton<CDODClassInfoPanel> *button =
			dynamic_cast<CImageMouseOverButton<CDODClassInfoPanel> *>(GetChild(i));

		if ( button )
		{
			if( button == m_pInitialButton && bShow == true )
				button->ShowPage();
			else
				button->HidePage();
		}
	}

	CDODRandomButton<CDODClassInfoPanel> *pRandom =
		dynamic_cast<CDODRandomButton<CDODClassInfoPanel> *>( FindChildByName("random") );

	if ( pRandom )
		pRandom->HidePage();

	// recalc position of checkbox, since it doesn't do right alignment
	m_pSuicideOption->SizeToContents();

	int x, y, wide, tall; 
	m_pSuicideOption->GetBounds( x, y, wide, tall );

	int parentW, parentH;
	GetSize( parentW, parentH );

	x = parentW / 2;	// - wide;
	m_pSuicideOption->SetPos( x, y );

	BaseClass::ShowPanel( bShow );
}

void CDODClassMenu::OnKeyCodePressed( KeyCode code )
{
#ifdef REFRESH_CLASSMENU_TOOL

	if ( code == KEY_PAD_MULTIPLY )
	{
		OnRefreshClassMenu();
	}
#endif

	if ( m_iClassMenuKey != BUTTON_CODE_INVALID && m_iClassMenuKey == code )
	{
		ShowPanel( false );
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}

void CDODClassMenu::Update()
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( pPlayer && pPlayer->m_Shared.DesiredPlayerClass() == PLAYERCLASS_UNDEFINED )
	{
		SetVisibleButton( "CancelButton", false );
	}
	else
	{
		SetVisibleButton( "CancelButton", true ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Panel *CDODClassMenu::CreateControlByName( const char *controlName )
{
	if ( !Q_stricmp( "DODMouseOverPanelButton", controlName ) )
	{
		 return new CDODMouseOverButton<CDODClassInfoPanel>( this, NULL, m_pClassInfoPanel );
	}
	else if( !Q_stricmp( "DODButton", controlName ) )
	{
		return new CDODButton(this);
	}
	else if( !Q_stricmp( "DODRandomButton", controlName ) )
	{
		return new CDODRandomButton<CDODClassInfoPanel>(this, NULL, m_pClassInfoPanel );
	}
	else if ( !Q_stricmp( "ImageButton", controlName ) )
	{
		CImageMouseOverButton<CDODClassInfoPanel> *newButton = new CImageMouseOverButton<CDODClassInfoPanel>( this, NULL, m_pClassInfoPanel );

		if( !m_pInitialButton )
		{
			m_pInitialButton = newButton;
		}

		return newButton;
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

//-----------------------------------------------------------------------------
// Catch the mouseover event and set the active class
//-----------------------------------------------------------------------------
void CDODClassMenu::OnShowPage( const char *pagename )
{
	// change which class we are counting based on class name

	// turn the button name into a classname

	char buf[64];

	Q_snprintf( buf, sizeof(buf), "cls_%s", pagename );

	C_DODTeam *pTeam = dynamic_cast<C_DODTeam *>( GetGlobalTeam(GetTeamNumber()) );

	if( !pTeam )
		return;

	// Pull the index of this class via IsClassOnTeam
	if ( !pTeam->IsClassOnTeam( buf, m_iActivePlayerClass ) )
	{
		Assert( !"bad class name on class button" );
	}

	UpdateNumClassLabel();
}

//-----------------------------------------------------------------------------
// Draw nothing
//-----------------------------------------------------------------------------
void CDODClassMenu::PaintBackground( void )
{
}

//-----------------------------------------------------------------------------
// Do things that should be done often, eg number of players in the 
// selected class
//-----------------------------------------------------------------------------
void CDODClassMenu::OnTick( void )
{
	//When a player changes teams, their class and team values don't get here 
	//necessarily before the command to update the class menu. This leads to the cancel button 
	//being visible and people cancelling before they have a class. check for class == DOD_CLASS_NONE and if so
	//hide the cancel button

	if ( !IsVisible() )
		return;

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if( pPlayer && pPlayer->m_Shared.PlayerClass() == PLAYERCLASS_UNDEFINED )
	{
		SetVisibleButton("CancelButton", false);
	}

	UpdateNumClassLabel();

	BaseClass::OnTick();
}

void CDODClassMenu::UpdateNumClassLabel( void )
{
	int iClassCount[NUM_CLASSES];
	// Initialize to zero. Was previously used uninitialized.
	int iClassLimit[NUM_CLASSES] = {};

	// count how many of this class there are
	C_DODTeam *pTeam = dynamic_cast<C_DODTeam *>( GetGlobalTeam(GetTeamNumber()) );

	if ( !pTeam )
		return;

	char buf[16];

	for( int i=0;i<NUM_CLASSES;i++ )
	{
		iClassCount[i] = pTeam->CountPlayersOfThisClass( i );

		if ( !m_pClassNumLabel[i] || !m_pClassFullLabel[i] )
			continue;

		if ( pTeam->IsClassOnTeam( i ) )
		{
			// FIXME - store pointers to these cvars
			const CDODPlayerClassInfo &pClassInfo = pTeam->GetPlayerClassInfo( i );
			ConVar *pLimitCvar = ( ConVar * )cvar->FindVar( pClassInfo.m_szLimitCvar );

			if ( pLimitCvar )
				iClassLimit[i] = MIN( 32, pLimitCvar->GetInt() );
		}	

		if ( iClassLimit[i] < 0 || iClassCount[i] < iClassLimit[i] )
			m_pClassFullLabel[i]->SetVisible( false );
		else
			m_pClassFullLabel[i]->SetVisible( true );

		if ( iClassLimit[i] > -1 )
		{
			// draw "3 / 4"
			Q_snprintf( buf, sizeof(buf), "%d / %d", iClassCount[i], iClassLimit[i] );
		}
		else
		{
			// just "3"
			Q_snprintf( buf, sizeof(buf), "x %d", iClassCount[i] );
		}

		m_pClassNumLabel[i]->SetText( buf );
	}
}

void CDODClassMenu::SetVisible( bool state )
{
	BaseClass::SetVisible( state );
}

void CDODClassMenu::OnSuicideOptionChanged( vgui::Panel *Panel )
{
	hud_classautokill.SetValue( m_pSuicideOption->IsSelected() );
}

#ifdef REFRESH_CLASSMENU_TOOL

	void CDODClassMenu::OnRefreshClassMenu( void )
	{
		for( int i = 0; i< GetChildCount(); i++ ) 
		{
			CImageMouseOverButton<CDODClassInfoPanel> *button =
				dynamic_cast<CImageMouseOverButton<CDODClassInfoPanel> *>(GetChild(i));

			if ( button )
			{
				button->RefreshClassPage();
			}
		}		
	}

#endif
