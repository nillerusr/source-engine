
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dod_hud_winpanel.h"
#include "vgui_controls/AnimationController.h"
#include "iclientmode.h"
#include "c_dod_playerresource.h"
#include <vgui_controls/Label.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "vgui_avatarimage.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT_DEPTH( CDODWinPanel_Allies, 1 );	// 1 is foreground
DECLARE_HUDELEMENT_DEPTH( CDODWinPanel_Axis, 1 );

CDODWinPanel_Allies::CDODWinPanel_Allies( const char *pElementName ) : CDODWinPanel( "WinPanel_Allies", TEAM_ALLIES )
{
	LoadControlSettings("Resource/UI/Win_Allies.res");
}

void CDODWinPanel_Allies::OnScreenSizeChanged( int iOldWide, int iOldTall )
{
	LoadControlSettings( "resource/UI/Win_Allies.res" );
}

void CDODWinPanel_Allies::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	m_pIcon = gHUD.GetIcon( "icon_obj_allies" );

	LoadControlSettings( "resource/UI/Win_Allies.res" );

	BaseClass::ApplySchemeSettings( pScheme );
}

//============================

CDODWinPanel_Axis::CDODWinPanel_Axis( const char *pElementName ) : CDODWinPanel( "WinPanel_Axis", TEAM_AXIS )
{
	LoadControlSettings("Resource/UI/Win_Axis.res");
}

void CDODWinPanel_Axis::OnScreenSizeChanged( int iOldWide, int iOldTall )
{
	LoadControlSettings( "resource/UI/Win_Axis.res" );
}

void CDODWinPanel_Axis::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	m_pIcon = gHUD.GetIcon( "icon_obj_axis" );

	LoadControlSettings( "resource/UI/Win_Axis.res" );

	BaseClass::ApplySchemeSettings( pScheme );
}

//============================

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODWinPanel::CDODWinPanel( const char *pElementName, int iTeam )
	: EditablePanel( NULL, pElementName ), CHudElement( pElementName )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	SetVisible( false );
	SetAlpha( 0 );
	SetScheme( "ClientScheme" );

	m_iTeam = iTeam;

	m_pTimerStatusLabel = new vgui::Label( this, "TimerInfo", "" );

	m_pLastCapperHeader = new vgui::Label( this, "LastCapperHeader", "" );
	m_pLastBomberHeader = new vgui::Label( this, "LastBomberHeader", "" );

	m_pLastCapperLabel = new vgui::Label( this, "LastCapper", "" );
	m_pLastCapperLabel_Avatar = new vgui::Label( this, "LastCapper_Avatar", "" );

	m_pLeftCategoryHeader = new vgui::Label( this, "LeftCategoryHeader", "..." );
	m_pRightCategoryHeader = new vgui::Label( this, "RightCategoryHeader", "..." );

	m_pLeftCategoryLabels[0] = new vgui::Label( this, "LeftCategory1", "" );
	m_pLeftCategoryLabels[1] = new vgui::Label( this, "LeftCategory2", "" );
	m_pLeftCategoryLabels[2] = new vgui::Label( this, "LeftCategory3", "" );

	m_pRightCategoryLabels[0] = new vgui::Label( this, "RightCategory1", "" );
	m_pRightCategoryLabels[1] = new vgui::Label( this, "RightCategory2", "" );
	m_pRightCategoryLabels[2] = new vgui::Label( this, "RightCategory3", "" );

	RegisterForRenderGroup( "winpanel" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODWinPanel::Reset()
{
	Hide();
}

void CDODWinPanel::Init()
{
	// listen for events
	ListenForGameEvent( "dod_round_win" );
	ListenForGameEvent( "dod_round_start" );
	ListenForGameEvent( "dod_point_captured" );
	ListenForGameEvent( "dod_win_panel" );

	Hide();

	SetFinalCaptureLabel( "", false );

	m_pTimerStatusLabel->SetText( "" );

	m_bShowTimerDefend = false;
	m_bShowTimerAttack = false;
	m_iTimerTime = 0;

	m_iFinalEventType = CAP_EVENT_NONE;

	m_iLeftCategory = WINPANEL_TOP3_NONE;
	m_iRightCategory = WINPANEL_TOP3_NONE;

	for ( int i=0;i<3;i++ )
	{
		m_iLeftCategoryScores[i] = 0;
		m_iRightCategoryScores[i] = 0;
	}

	CHudElement::Init();
}

void CDODWinPanel::VidInit()
{
	m_pIconCap = gHUD.GetIcon( "stats_cap" );
	m_pIconDefended	= gHUD.GetIcon( "stats_defended" );
	m_pIconBomb	= gHUD.GetIcon( "icon_c4" );
	m_pIconKill	= gHUD.GetIcon( "stats_kill" );
}

void SetPlayerNameLabel( vgui::Label *pLabel, int clientIndex )
{
	if ( !pLabel )
		return;

	if ( clientIndex >= 1 && clientIndex <= MAX_PLAYERS )
	{
		char buf[48];
		Q_snprintf( buf, sizeof(buf), "%s:", g_PR->GetPlayerName(clientIndex) );
		pLabel->SetText( buf );
	}

	pLabel->SetVisible( clientIndex > 0 );
}

void CDODWinPanel::FireGameEvent( IGameEvent * event )
{
	const char *pEventName = event->GetName();

	if ( Q_strcmp( "dod_round_win", pEventName ) == 0 )
	{
		if ( event->GetInt( "team" ) == m_iTeam )
		{
			Show();
		}		
	}
	else if ( Q_strcmp( "dod_round_start", pEventName ) == 0 )
	{
		Hide();

		m_pLastCapperHeader->SetVisible( false );
		m_pLastBomberHeader->SetVisible( false );
	}
	else if ( Q_strcmp( "dod_point_captured", pEventName ) == 0 )
	{
		if ( !g_PR )
			return;

		// Array of capper indeces
		const char *cappers = event->GetString("cappers");

		char szCappers[256];
		szCappers[0] = '\0';

		int len = Q_strlen(cappers);

		bool bShowAvatar = ( len == 1 );

		if ( !bShowAvatar )
		{
			SetupAvatar( "top", 1, 0 );	// hide it
		}

		for( int i=0;i<len;i++ )
		{
			int iPlayerIndex = (int)cappers[i];

			Assert( iPlayerIndex > 0 && iPlayerIndex <= gpGlobals->maxClients );

			const char *pPlayerName = g_PR->GetPlayerName( iPlayerIndex );

			if ( bShowAvatar )
			{
				SetupAvatar( "top", 1, iPlayerIndex );
			}

			if ( i > 0 )
			{
				Q_strncat( szCappers, ", ", sizeof(szCappers), 2 );
			}

			Q_strncat( szCappers, pPlayerName, sizeof(szCappers), COPY_ALL_CHARACTERS );
		}	

		if ( event->GetBool( "bomb" ) )
		{
			m_pLastCapperHeader->SetVisible( false );
			m_pLastBomberHeader->SetVisible( true );
		}
		else
		{
			m_pLastCapperHeader->SetVisible( true );
			m_pLastBomberHeader->SetVisible( false );
		}

		SetFinalCaptureLabel( szCappers, bShowAvatar );
	}
	else if ( Q_strcmp( "dod_win_panel", pEventName ) == 0 )
	{
		/*
		"show_timer_defend"	"bool"
		"show_timer_attack"	"bool"
		"timer_time"		"int"

		"final_event"		"byte"		// 0 - no event, 1 - bomb exploded, 2 - flag capped, 3 - timer expired

		"category_left"		"byte"		// 0-4: none, bombers, cappers, defenders, killers
		"left_1"			"byte"		// player index if first 
		"left_score_1"		"byte"
		"left_2"			"byte"
		"left_score_2"		"byte"
		"left_3"			"byte"
		"left_score_3"		"byte"

		"right_1"			"byte"
		"right_score_1"		"byte"
		"right_2"			"byte"
		"right_score_2"		"byte"
		"right_3"			"byte"
		"right_score_3"		"byte"
		*/

		if ( !g_PR )
			return;

		m_bShowTimerDefend = event->GetBool( "show_timer_defend" );
		m_bShowTimerAttack = event->GetBool( "show_timer_attack" );
		m_iTimerTime = event->GetInt( "timer_time" );

		int minutes = clamp( m_iTimerTime / 60, 0, 99 );
		int seconds = clamp( m_iTimerTime % 60, 0, 59 );

		if ( m_bShowTimerDefend )
		{
			// defenders win, show total time defended
			//		"Total Time Defended: 4:28"

			wchar_t time[8];
			_snwprintf( time, ARRAYSIZE( time ), L"%d:%02d", minutes, seconds );

			wchar_t timerText[128];
			g_pVGuiLocalize->ConstructString( timerText, sizeof( timerText ), g_pVGuiLocalize->Find( "#winpanel_total_time" ), 1, time );

			m_pTimerStatusLabel->SetText( timerText );

			// zero out the final capture label, they won by timer
			m_pLastCapperHeader->SetVisible( false );
			m_pLastBomberHeader->SetVisible( false );
			SetFinalCaptureLabel( "", false );

			SetupAvatar( "top", 1, 0 );	// hide it
		}
		else if ( m_bShowTimerAttack )
		{
			// attackers win, show time elapsed
			//		"Time Elapsed: 4:12"  

			wchar_t time[8];
			_snwprintf( time, ARRAYSIZE( time ), L"%d:%02d", minutes, seconds );

			wchar_t timerText[128];
			g_pVGuiLocalize->ConstructString( timerText, sizeof( timerText ), g_pVGuiLocalize->Find( "#winpanel_attack_time" ), 1, time );

			m_pTimerStatusLabel->SetText( timerText );
		}
		else
		{
			m_pTimerStatusLabel->SetText( "" );
		}

		m_iFinalEventType = event->GetInt( "final_event" );
		// up to client to fill in who completed the final event

		m_iLeftCategory = event->GetInt( "category_left" );
		m_iRightCategory = event->GetInt( "category_right" );

		m_pLeftCategoryHeader->SetText( g_pVGuiLocalize->Find( pszWinPanelCategoryHeaders[m_iLeftCategory] ) );
		m_pRightCategoryHeader->SetText( g_pVGuiLocalize->Find( pszWinPanelCategoryHeaders[m_iRightCategory] ) );

		int iPlayer;

		// Left Top 3 Category
		iPlayer = event->GetInt( "left_1" );
		SetPlayerNameLabel( m_pLeftCategoryLabels[0], iPlayer );
		SetupAvatar( "left", 1, iPlayer );

		iPlayer = event->GetInt( "left_2" );
		SetPlayerNameLabel( m_pLeftCategoryLabels[1], iPlayer );
		SetupAvatar( "left", 2, iPlayer );

		iPlayer = event->GetInt( "left_3" );
		SetPlayerNameLabel( m_pLeftCategoryLabels[2], iPlayer );
		SetupAvatar( "left", 3, iPlayer );

		m_iLeftCategoryScores[0] = event->GetInt( "left_score_1" );
		m_iLeftCategoryScores[1] = event->GetInt( "left_score_2" );
		m_iLeftCategoryScores[2] = event->GetInt( "left_score_3" );

		// Right Top 3 Category
		iPlayer = event->GetInt( "right_1" );
		SetPlayerNameLabel( m_pRightCategoryLabels[0], iPlayer );
		SetupAvatar( "right", 1, iPlayer );

		iPlayer = event->GetInt( "right_2" );
		SetPlayerNameLabel( m_pRightCategoryLabels[1], iPlayer );
		SetupAvatar( "right", 2, iPlayer );

		iPlayer = event->GetInt( "right_3" );
		SetPlayerNameLabel( m_pRightCategoryLabels[2], iPlayer );
		SetupAvatar( "right", 3, iPlayer );

		m_iRightCategoryScores[0] = event->GetInt( "right_score_1" );
		m_iRightCategoryScores[1] = event->GetInt( "right_score_2" );
		m_iRightCategoryScores[2] = event->GetInt( "right_score_3" );

		m_pRightCategoryHeader->SetVisible( ( m_iRightCategoryScores[0] > 0 ) );
	}
}

void CDODWinPanel::SetupAvatar( const char *pSide, int pos, int iPlayerIndex )
{
#if !defined( _X360 )

	bool bVisible = ( iPlayerIndex > 0 );

	CAvatarImagePanel *pPlayerAvatar = dynamic_cast<CAvatarImagePanel *>( FindChildByName( CFmtStr( "%s_%d_avatar", pSide, pos ) ) );

	if ( pPlayerAvatar )
	{
		pPlayerAvatar->SetShouldScaleImage( true );
		pPlayerAvatar->SetShouldDrawFriendIcon( false );

		if ( bVisible )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
			pPlayerAvatar->SetPlayer( pPlayer );
		}

		pPlayerAvatar->SetVisible( bVisible );
	}
#endif
}

void CDODWinPanel::SetFinalCaptureLabel( const char *szCappers, bool bShowAvatar )
{
	SetDialogVariable( "lastcappers", szCappers );

	m_pLastCapperLabel->SetVisible( !bShowAvatar );
	m_pLastCapperLabel_Avatar->SetVisible( bShowAvatar );
}

void CDODWinPanel::Show( void )
{
	SetAlpha( 255 );

	int iRenderGroup = gHUD.LookupRenderGroupIndexByName( "winpanel" );
	if ( iRenderGroup >= 0 )
	{
		gHUD.LockRenderGroup( iRenderGroup );
	}
}

void CDODWinPanel::Hide( void )
{
	SetAlpha( 0 );

	int iRenderGroup = gHUD.LookupRenderGroupIndexByName( "winpanel" );
	if ( iRenderGroup >= 0 )
	{
		gHUD.UnlockRenderGroup( iRenderGroup );
	}
}

void CDODWinPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBgColor( GetSchemeColor("TransparentLightBlack", pScheme) );
}

bool CDODWinPanel::ShouldDraw( void )
{
	return ( GetAlpha() > 0 );
}

CHudTexture *CDODWinPanel::GetIconForCategory( int category )
{
	CHudTexture *pTex = NULL;

	switch( category )
	{
	case WINPANEL_TOP3_BOMBERS:
		pTex = m_pIconBomb;
		break;
	case WINPANEL_TOP3_CAPPERS:
		pTex = m_pIconCap;
		break;
	case WINPANEL_TOP3_DEFENDERS:
		pTex = m_pIconDefended;
		break;
	case WINPANEL_TOP3_KILLERS:
		pTex = m_pIconKill;
		break;
	default:
		break;
	}

	return pTex;
}

void CDODWinPanel::Paint( void )
{
	if ( m_pIcon )
	{
		Color c(255,255,255,255);
		m_pIcon->DrawSelf( m_iIconX_left, m_iIconY, m_iIconW, m_iIconH, c );
		m_pIcon->DrawSelf( m_iIconX_right, m_iIconY, m_iIconW, m_iIconH, c );
	}

	int i;
	int x, y, w, h;
	Color c(255,255,255,255);

	// Draw Left Category Icons
	CHudTexture *pIcon = GetIconForCategory( m_iLeftCategory );

	if ( pIcon )
	{
		for ( i=0;i<3;i++ )
		{
			if ( m_iLeftCategoryScores[i] > 0 )
			{
				m_pLeftCategoryLabels[i]->GetBounds( x, y, w, h );

				x = x + w + XRES(2);
				y = y + ( h - m_iIconSize ) * 0.5;
				
				// too many, do a "(icon) 8"
				pIcon->DrawSelf( x, y, m_iIconSize, m_iIconSize, c );
				x += m_iIconSize;

				char buf[10];
				Q_snprintf( buf, sizeof(buf), " %d", m_iLeftCategoryScores[i] );
				DrawText( buf, x, y, c );
			}
		}
	}

	// Draw Right Category Icons
	pIcon = GetIconForCategory( m_iRightCategory );

	if ( pIcon )
	{
		for ( i=0;i<3;i++ )
		{
			if ( m_iRightCategoryScores[i] > 0 )
			{
				m_pRightCategoryLabels[i]->GetBounds( x, y, w, h );

				x = x + w + XRES(2);
				y = y + ( h - m_iIconSize ) * 0.5;

				// too many, do a "(icon) 8"
				pIcon->DrawSelf( x, y, m_iIconSize, m_iIconSize, c );
				x += m_iIconSize;

				char buf[10];
				Q_snprintf( buf, sizeof(buf), " %d", m_iRightCategoryScores[i] );
				DrawText( buf, x, y, c );
			}
		}
	}
}

void CDODWinPanel::DrawText( char *text, int x, int y, Color clrText )
{
	vgui::surface()->DrawSetTextColor( clrText );
	vgui::surface()->DrawSetTextFont( m_hNumberFont );
	vgui::surface()->DrawSetTextPos( x, y );

	for (char *pch = text; *pch != 0; pch++)
	{
		vgui::surface()->DrawUnicodeChar(*pch);
	}
}