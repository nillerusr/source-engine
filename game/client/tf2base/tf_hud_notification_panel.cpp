//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "c_tf_player.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>
#include "tf_shareddefs.h"
#include "tf_hud_notification_panel.h"
#include "tf_hud_freezepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

ConVar tf_hud_notification_duration( "tf_hud_notification_duration", "3.0", 0, "How long to display hud notification panels before fading them" );

DECLARE_HUDELEMENT( CHudNotificationPanel );

DECLARE_HUD_MESSAGE( CHudNotificationPanel, HudNotify );
DECLARE_HUD_MESSAGE( CHudNotificationPanel, HudNotifyCustom );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudNotificationPanel::CHudNotificationPanel( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "NotificationPanel" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	vgui::ivgui()->AddTickSignal( GetVPanel() );

	m_flFadeTime = 0;

	// listen for one version that just passes an int, for prebuilt notifications
	// and another that takes a res file to load

	m_pText = new Label( this, "Notification_Label", "" );
	m_pIcon = new CIconPanel( this, "Notification_Icon" );
	m_pBackground = new ImagePanel( this, "Notification_Background" );

	RegisterForRenderGroup( "mid" );
	RegisterForRenderGroup( "commentary" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudNotificationPanel::Init( void )
{
	CHudElement::Init();

	HOOK_HUD_MESSAGE( CHudNotificationPanel, HudNotify );
	HOOK_HUD_MESSAGE( CHudNotificationPanel, HudNotifyCustom );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudNotificationPanel::ApplySchemeSettings( IScheme *pScheme )
{
	// load control settings...
	LoadControlSettings( "resource/UI/notifications/base_notification.res" );

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudNotificationPanel::MsgFunc_HudNotify( bf_read &msg )
{
	// Ignore notifications in minmode
	ConVarRef cl_hud_minmode( "cl_hud_minmode", true );
	if ( cl_hud_minmode.IsValid() && cl_hud_minmode.GetBool() )
		return;

	int iType = msg.ReadByte();

	LoadControlSettings( GetNotificationByType( iType ) );

	// set up the fade time
	m_flFadeTime = gpGlobals->curtime + tf_hud_notification_duration.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudNotificationPanel::MsgFunc_HudNotifyCustom( bf_read &msg )
{
	// Ignore notifications in minmode
	ConVarRef cl_hud_minmode( "cl_hud_minmode", true );
	if ( cl_hud_minmode.IsValid() && cl_hud_minmode.GetBool() )
		return;

	// Reload the base
	LoadControlSettings( "resource/UI/notifications/base_notification.res" );

	char szText[256];
	char szIcon[256];

	msg.ReadString( szText, sizeof(szText) );
	msg.ReadString( szIcon, sizeof(szIcon) );
	int iBackgroundTeam = msg.ReadByte();

	SetupNotifyCustom( szText, szIcon, iBackgroundTeam );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudNotificationPanel::SetupNotifyCustom( const char *pszText, const char *pszIcon, int iBackgroundTeam )
{
	m_pIcon->SetIcon( pszIcon );
	m_pText->SetText( pszText );

	if ( iBackgroundTeam == TF_TEAM_RED )
	{
		m_pBackground->SetImage( "../hud/score_panel_red_bg" );
	}
	else if ( iBackgroundTeam == TF_TEAM_BLUE )
	{
		m_pBackground->SetImage( "../hud/score_panel_blue_bg" );
	}
	else
	{
		m_pBackground->SetImage( "../hud/notification_black" );
	}

	// set up the fade time
	m_flFadeTime = gpGlobals->curtime + tf_hud_notification_duration.GetFloat();

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudNotificationPanel::PerformLayout( void )
{
	BaseClass::PerformLayout();

	// re-layout the panel manually so we fit the length of the string!
	// **** this is super yucky, i'm going to cry myself to sleep tonight ****

	int iTextWide, iTextTall;
	m_pText->GetContentSize(iTextWide, iTextTall);

	m_pText->SetSize(iTextWide, m_pText->GetTall() );

	float flTextWide = m_pText->GetWide();
	float flIconWide = m_pIcon->GetWide();

	float flSpacer = XRES(5);
	float flEndSpacer = XRES(8) + XRES(3) * ( flTextWide / 184 );	// total hackery

	float flTotalWidth = flEndSpacer + flIconWide + flSpacer + flTextWide + flEndSpacer;

	float flLeftSide = ( GetWide() - flTotalWidth ) * 0.5f;

	// resize and position background
	m_pBackground->SetPos( flLeftSide, 0 );

	m_pBackground->SetWide( flTotalWidth );

	// reposition icon
	int iIconXPos, iIconYPos;
	m_pIcon->GetPos( iIconXPos, iIconYPos );

	m_pIcon->SetPos( flLeftSide + flEndSpacer, iIconYPos );

	// reposition text
	int iTextXPos, iTextYPos;
	m_pText->GetPos( iTextXPos, iTextYPos );

	m_pText->SetPos( flLeftSide + flEndSpacer + flIconWide + flSpacer, iTextYPos );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudNotificationPanel::ShouldDraw( void )
{
	// only if we have a valid message to draw
	if ( m_flFadeTime < gpGlobals->curtime )
		return false;

	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudNotificationPanel::OnTick( void )
{
	// set alpha based on time left

	// do this if we can fade the icons and the background

	/*
	float flLifeTime = m_flFadeTime - gpGlobals->curtime;

	if ( flLifeTime >= 1 )
	{
		SetAlpha( 255 );
	}
	else
	{
		SetAlpha( (float)( 255.0f * flLifeTime ) );
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CHudNotificationPanel::GetNotificationByType( int iType )
{
	bool bOnBlueTeam = false;

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pLocalPlayer )
	{
		bOnBlueTeam = ( pLocalPlayer->GetTeamNumber() == TF_TEAM_BLUE );
	}

	const char *pszResult = "";

	switch ( iType )
	{
	case HUD_NOTIFY_YOUR_FLAG_TAKEN:
		if ( bOnBlueTeam )
		{
			pszResult = "resource/UI/notifications/notify_your_flag_taken_blue.res";
		}
		else
		{
			pszResult = "resource/UI/notifications/notify_your_flag_taken_red.res";
		}
		break;
	case HUD_NOTIFY_YOUR_FLAG_DROPPED:
		if ( bOnBlueTeam )
		{
			pszResult = "resource/UI/notifications/notify_your_flag_dropped_blue.res";
		}
		else
		{
			pszResult = "resource/UI/notifications/notify_your_flag_dropped_red.res";
		}
		break;
	case HUD_NOTIFY_YOUR_FLAG_RETURNED:
		if ( bOnBlueTeam )
		{
			pszResult = "resource/UI/notifications/notify_your_flag_returned_blue.res";
		}
		else
		{
			pszResult = "resource/UI/notifications/notify_your_flag_returned_red.res";
		}
		break;
	case HUD_NOTIFY_YOUR_FLAG_CAPTURED:
		if ( bOnBlueTeam )
		{
			pszResult = "resource/UI/notifications/notify_your_flag_captured_blue.res";
		}
		else
		{
			pszResult = "resource/UI/notifications/notify_your_flag_captured_red.res";
		}
		break;
	case HUD_NOTIFY_ENEMY_FLAG_TAKEN:
		if ( bOnBlueTeam )
		{
			pszResult = "resource/UI/notifications/notify_enemy_flag_taken_blue.res";
		}
		else
		{
			pszResult = "resource/UI/notifications/notify_enemy_flag_taken_red.res";
		}
		break;
	case HUD_NOTIFY_ENEMY_FLAG_DROPPED:
		if ( bOnBlueTeam )
		{
			pszResult = "resource/UI/notifications/notify_enemy_flag_dropped_blue.res";
		}
		else
		{
			pszResult = "resource/UI/notifications/notify_enemy_flag_dropped_red.res";
		}
		break;
	case HUD_NOTIFY_ENEMY_FLAG_RETURNED:
		if ( bOnBlueTeam )
		{
			pszResult = "resource/UI/notifications/notify_enemy_flag_returned_blue.res";
		}
		else
		{
			pszResult = "resource/UI/notifications/notify_enemy_flag_returned_red.res";
		}
		break;
	case HUD_NOTIFY_ENEMY_FLAG_CAPTURED:
		if ( bOnBlueTeam )
		{
			pszResult = "resource/UI/notifications/notify_enemy_flag_captured_blue.res";
		}
		else
		{
			pszResult = "resource/UI/notifications/notify_enemy_flag_captured_red.res";
		}
		break;

	case HUD_NOTIFY_TOUCHING_ENEMY_CTF_CAP:
		if ( bOnBlueTeam )
		{
			pszResult = "resource/UI/notifications/notify_touching_enemy_ctf_cap_blue.res";
		}
		else
		{
			pszResult = "resource/UI/notifications/notify_touching_enemy_ctf_cap_red.res";
		}
		break;

	case HUD_NOTIFY_NO_INVULN_WITH_FLAG:
		if ( bOnBlueTeam )
		{
			pszResult = "resource/UI/notifications/notify_no_invuln_with_flag_blue.res";
		}
		else
		{
			pszResult = "resource/UI/notifications/notify_no_invuln_with_flag_red.res";
		}
		break;

	case HUD_NOTIFY_NO_TELE_WITH_FLAG:
		if ( bOnBlueTeam )
		{
			pszResult = "resource/UI/notifications/notify_no_tele_with_flag_blue.res";
		}
		else
		{
			pszResult = "resource/UI/notifications/notify_no_tele_with_flag_red.res";
		}
		break;


	case HUD_NOTIFY_SPECIAL:
		pszResult = "resource/UI/notifications/notify_special.res";
		break;

	default:
		break;
	}

	return pszResult;
}