//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Create and display a win panel at the end of a round displaying interesting stats and info about the round.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "win_panel_round.h"
#include "vgui_controls/AnimationController.h"
#include "iclientmode.h"
#include "c_playerresource.h"
#include <vgui_controls/Label.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include "fmtstr.h"
#include "cs_gamestats_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_round_win_fade_time( "cl_round_win_fade_time", "1.5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );

DECLARE_HUDELEMENT_DEPTH( WinPanel_Round, 1 );	// 1 is foreground
extern const wchar_t *LocalizeFindSafe( const char *pTokenName );


// helper function for converting wstrings to upper-case inline
// NB: this returns a pointer to a static buffer
wchar_t* UpperCaseWideString( const wchar_t* wszSource )
{
	static wchar_t wszBuffer[256];
	V_wcsncpy(wszBuffer, wszSource, sizeof(wszBuffer));
	V_wcsupr(wszBuffer);
	return wszBuffer;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
WinPanel_Round::WinPanel_Round( const char *pElementName ) :
	BorderedPanel( NULL, pElementName ),
	CHudElement( pElementName ),
	m_bIsFading(false),
	m_fFadeBeginTime(0.0f)
{
	SetSize( 10, 10 ); // Quiet "parent not sized yet" spew
	SetParent(g_pClientMode->GetViewport());	

	SetScheme( "ClientScheme" );

	RegisterForRenderGroup( "hide_for_scoreboard" );

	m_bShouldBeVisible = false;
}

WinPanel_Round::~WinPanel_Round()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WinPanel_Round::Reset()
{
	Hide();	
}

void WinPanel_Round::Init()
{
	CHudElement::Init();

	// listen for events
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "cs_win_panel_round" );
	ListenForGameEvent( "cs_win_panel_match" );
	ListenForGameEvent( "round_mvp" );

	InitLayout();
	
	m_bShouldBeVisible = false;
	m_bShowTimerDefend = false;
	m_bShowTimerAttack = false;
}


void WinPanel_Round::InitLayout()
{
	// reload control settings when resolution changes to force update of proportional layout
	LoadControlSettings("Resource/UI/Win_Round.res");

	CAvatarImagePanel* pMVP_Avatar = dynamic_cast<CAvatarImagePanel*>(FindChildByName("MVP_Avatar"));
	pMVP_Avatar->SetDefaultAvatar(scheme()->GetImage( CSTRIKE_DEFAULT_AVATAR, true));
	pMVP_Avatar->SetShouldDrawFriendIcon(false);
}


void WinPanel_Round::VidInit()
{	
}

//=============================================================================
// HPE_BEGIN:
// [Forrest] Allow win panel to be turned off on client
//=============================================================================
ConVar cl_nowinpanel(
	"cl_nowinpanel",
	"0",
	FCVAR_ARCHIVE,
	"Turn on/off win panel on client"
	);
//=============================================================================
// HPE_END
//=============================================================================

void WinPanel_Round::FireGameEvent( IGameEvent* event )
{
	const char *pEventName = event->GetName();

	if ( Q_strcmp( "round_end", pEventName ) == 0 )
	{
	}		
	else if ( Q_strcmp( "round_start", pEventName ) == 0 )
	{		
		Hide();		
	}
	else if( Q_strcmp( "cs_win_panel_match", pEventName ) == 0 )
	{	
		Hide();
	}
	else if( Q_strcmp( "round_mvp", pEventName ) == 0 )
	{		
		C_BasePlayer *basePlayer = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
		CSMvpReason_t mvpReason = (CSMvpReason_t)event->GetInt( "reason" );

		if( basePlayer )
		{
			SetMVP( ToCSPlayer( basePlayer ), mvpReason );
		}
	}
	else if ( Q_strcmp( "cs_win_panel_round", pEventName ) == 0 )
	{
		/*
		"show_timer_defend"	"bool"
		"show_timer_attack"	"bool"
		"timer_time"		"int"

		"final_event"		"byte"		// 0 - no event, 1 - bomb exploded, 2 - flag capped, 3 - timer expired

		"funfact_type"		"byte"		//WINPANEL_FUNFACT in cs_shareddef.h
		"funfact_player"	"byte"
		"funfact_data1"		"long"
		"funfact_data2"		"long"
		"funfact_data3"		"long"
		*/

		if ( !g_PR )
			return;

		//=============================================================================
		// HPE_BEGIN:
		// [Forrest] Check if win panel is disabled.
		//=============================================================================
		static ConVarRef sv_nowinpanel( "sv_nowinpanel" );
		if ( sv_nowinpanel.GetBool() || cl_nowinpanel.GetBool() )
			return;
		//=============================================================================
		// HPE_END
		//=============================================================================

		m_bShowTimerDefend = event->GetBool( "show_timer_defend" );
		m_bShowTimerAttack = event->GetBool( "show_timer_attack" );
		int iTimerTime = event->GetInt( "timer_time" );

		int minutes = clamp( iTimerTime / 60, 0, 99 );
		int seconds = clamp( iTimerTime % 60, 0, 59 );

		wchar_t time[8];
		_snwprintf( time, ARRAYSIZE( time ), L"%d:%02d", minutes, seconds );

		SetDialogVariable("TIMER_TEXT", time);

		// Final Fun Fact
		SetFunFactLabel( L"");
		int iFunFactPlayer = event->GetInt("funfact_player");
		const char* funfactToken = event->GetString("funfact_token", "");

		if (strlen(funfactToken) != 0)
		{
			wchar_t funFactText[256];
			wchar_t playerText[64];
			wchar_t dataText1[8], dataText2[8], dataText3[8];
			int param1 = event->GetInt("funfact_data1");
			int param2 = event->GetInt("funfact_data2");
			int param3 = event->GetInt("funfact_data3");
			if ( iFunFactPlayer >= 1 && iFunFactPlayer <= MAX_PLAYERS )
			{
				const char* playerName = g_PR->GetPlayerName( iFunFactPlayer );
				if( playerName && Q_strcmp( playerName, PLAYER_UNCONNECTED_NAME ) != 0 && Q_strcmp( playerName, PLAYER_ERROR_NAME ) != 0 )
				{
					V_strtowcs( g_PR->GetPlayerName( iFunFactPlayer ), 64, playerText, sizeof( playerText ) );
				}
				else
				{
#ifdef WIN32
					_snwprintf( playerText, ARRAYSIZE( playerText ), L"%s", LocalizeFindSafe( "#winpanel_former_player" ) );
#else
					_snwprintf( playerText, ARRAYSIZE( playerText ), L"%S", LocalizeFindSafe( "#winpanel_former_player" ) );
#endif
				}
			}
			else
			{
				_snwprintf( playerText, ARRAYSIZE( playerText ), L"" );
			}
			_snwprintf( dataText1, ARRAYSIZE( dataText1 ), L"%i", param1 );
			_snwprintf( dataText2, ARRAYSIZE( dataText2 ), L"%i", param2 );
			_snwprintf( dataText3, ARRAYSIZE( dataText3 ), L"%i", param3 );
			g_pVGuiLocalize->ConstructString( funFactText, sizeof(funFactText), (wchar_t *)LocalizeFindSafe(funfactToken), 4,
				playerText, dataText1, dataText2, dataText3 );
			SetFunFactLabel(funFactText);
		}

		int iEndEvent = event->GetInt( "final_event" );

		//Map the round end events onto localized strings
		const char* endEventToString[RoundEndReason_Count];
		V_memset(endEventToString, 0, sizeof(endEventToString));

		//terrorist win events
		endEventToString[Target_Bombed] = "#winpanel_end_target_bombed";
		endEventToString[VIP_Assassinated] = "#winpanel_end_vip_assassinated";
		endEventToString[Terrorists_Escaped] = "#winpanel_end_terrorists_escaped";
		endEventToString[Terrorists_Win] = "#winpanel_end_terrorists__kill";
		endEventToString[Hostages_Not_Rescued] = "#winpanel_end_hostages_not_rescued";
		endEventToString[VIP_Not_Escaped] = "#winpanel_end_vip_not_escaped";

		//CT win events
		endEventToString[VIP_Escaped] = "#winpanel_end_vip_escaped";
		endEventToString[CTs_PreventEscape] = "#winpanel_end_cts_prevent_escape";
		endEventToString[Escaping_Terrorists_Neutralized] = "#winpanel_end_escaping_terrorists_neutralized";
		endEventToString[Bomb_Defused] = "#winpanel_end_bomb_defused";
		endEventToString[CTs_Win] = "#winpanel_end_cts_win";
		endEventToString[All_Hostages_Rescued] = "#winpanel_end_all_hostages_rescued";
		endEventToString[Target_Saved] = "#winpanel_end_target_saved";
		endEventToString[Terrorists_Not_Escaped] = "#winpanel_end_terrorists_not_escaped";

		//We don't show a round end panel for these
		endEventToString[Game_Commencing] = "";
		endEventToString[Round_Draw] = "";

		const wchar_t* wszEventMessage = NULL;
		if(iEndEvent >=0 && iEndEvent < RoundEndReason_Count)
			wszEventMessage = LocalizeFindSafe(endEventToString[iEndEvent]);

		if ( wszEventMessage != NULL )
		{
			SetDialogVariable("WIN_DESCRIPTION", UpperCaseWideString(wszEventMessage));
		}
		else
		{
			SetDialogVariable("WIN_DESCRIPTION", "");
		}

		Label* pWinLabel = dynamic_cast<Label*>(FindChildByName("WinLabel"));
		switch(iEndEvent)
		{
		case Target_Bombed:
		case VIP_Assassinated:
		case Terrorists_Escaped:
		case Terrorists_Win:
		case Hostages_Not_Rescued:            
		case VIP_Not_Escaped:
			pWinLabel->SetText(UpperCaseWideString(LocalizeFindSafe("#winpanel_t_win")));
			pWinLabel->SetFgColor(Color(184,0,0,255));
			break;

		case VIP_Escaped:
		case CTs_PreventEscape:
		case Escaping_Terrorists_Neutralized:
		case Bomb_Defused:
		case CTs_Win:
		case All_Hostages_Rescued:
		case Target_Saved:
		case Terrorists_Not_Escaped:
			pWinLabel->SetText(UpperCaseWideString(LocalizeFindSafe("#winpanel_ct_win")));
			pWinLabel->SetFgColor(Color(71,152,237,255));
			break;

		case Round_Draw:
			pWinLabel->SetText(UpperCaseWideString(LocalizeFindSafe("#winpanel_draw")));
			pWinLabel->SetFgColor(Color(204,204,204,255));
			break;
		}

		//[tj]	We set the icon to the generic one right before we show it.
		//		The expected result is that we replace it immediately with
		//		the round MVP. if there is none, we just use the generic.
		SetMVP( NULL, CSMVP_UNDEFINED );

		Show();
	}
}

void WinPanel_Round::SetMVP( C_CSPlayer* pPlayer, CSMvpReason_t reason )
{
	CAvatarImagePanel* pMVP_Avatar = dynamic_cast<CAvatarImagePanel*>(FindChildByName("MVP_Avatar"));
	
	if ( pMVP_Avatar )
	{
		pMVP_Avatar->ClearAvatar();
	}

	//First set the text to the name of the player
	//=============================================================================
	// HPE_BEGIN:
	// [Forrest] Allow MVP to be turned off for a server
	//=============================================================================
	bool isThereAnMVP = ( pPlayer != NULL );
	if ( isThereAnMVP )
	//=============================================================================
	// HPE_END
	//=============================================================================
	{

		const char* mvpReasonToken = NULL;
		switch ( reason )
		{
		case CSMVP_ELIMINATION:
			mvpReasonToken = "winpanel_mvp_award_kills";
			break;
		case CSMVP_BOMBPLANT:
			mvpReasonToken = "winpanel_mvp_award_bombplant";
			break;
		case CSMVP_BOMBDEFUSE:
			mvpReasonToken = "winpanel_mvp_award_bombdefuse";
			break;
		case CSMVP_HOSTAGERESCUE:
			mvpReasonToken = "winpanel_mvp_award_rescue";
			break;
		default:
			mvpReasonToken = "winpanel_mvp_award";
			break;
		}

		wchar_t wszBuf[256], wszPlayerName[64];
		g_pVGuiLocalize->ConvertANSIToUnicode(UTIL_SafeName(pPlayer->GetPlayerName()), wszPlayerName, sizeof(wszPlayerName));

		wchar_t *pReason = (wchar_t *)LocalizeFindSafe( mvpReasonToken );
		if ( !pReason )
		{
			pReason = L"%s1";
		}

		g_pVGuiLocalize->ConstructString( wszBuf, sizeof( wszBuf ), pReason, 1, wszPlayerName );
		SetDialogVariable( "MVP_TEXT", wszBuf );

		player_info_t pi;
		if ( engine->GetPlayerInfo(pPlayer->entindex(), &pi) )
		{
			if ( pMVP_Avatar )
			{
				pMVP_Avatar->SetDefaultAvatar( GetDefaultAvatarImage( pPlayer ) );
				pMVP_Avatar->SetPlayer( pPlayer, k_EAvatarSize64x64 );
			}
		}
	}
	else
	{
		SetDialogVariable( "MVP_TEXT", "");
	}

	//=============================================================================
	// HPE_BEGIN:
	// [Forrest] Allow MVP to be turned off for a server
	//=============================================================================
	// The avatar image and its accompanying elements should be hidden if there is no MVP for the round.
	if ( pMVP_Avatar )
	{
		pMVP_Avatar->SetVisible( isThereAnMVP );
	}
	ImagePanel* pMVP_AvatarGlow = dynamic_cast<ImagePanel*>(FindChildByName("MVP_AvatarGlow"));
	if ( pMVP_AvatarGlow )
	{
		pMVP_AvatarGlow->SetVisible( isThereAnMVP );
	}
	ImagePanel* pMVP_Foreground_Star = dynamic_cast<ImagePanel*>(FindChildByName("MVP_Foreground_Star"));
	if ( pMVP_Foreground_Star )
	{
		pMVP_Foreground_Star->SetVisible( isThereAnMVP );
	}
	//=============================================================================
	// HPE_END
	//=============================================================================
}

void WinPanel_Round::SetFunFactLabel( const wchar *szFunFact )
{
	SetDialogVariable( "FUNFACT", szFunFact );
}

void WinPanel_Round::Show( void )
{
	int iRenderGroup = gHUD.LookupRenderGroupIndexByName( "hide_for_round_panel" );
	if ( iRenderGroup >= 0)
	{
		gHUD.LockRenderGroup( iRenderGroup );
	}

	m_bShouldBeVisible = true;
	SetAlpha(255);
	m_bIsFading = false;
}

void WinPanel_Round::Hide( void )
{
	if ( m_bShouldBeVisible && !m_bIsFading )
	{
		m_bIsFading = true;
		m_fFadeBeginTime = gpGlobals->realtime;
	}
}

void WinPanel_Round::OnThink()
{
	if ( m_bShouldBeVisible && m_bIsFading )
	{
		float fAlpha = 1.0f - (gpGlobals->realtime - m_fFadeBeginTime) / cl_round_win_fade_time.GetFloat();

		if (fAlpha >= 0.0f)
		{
			SetAlpha(RoundFloatToInt(fAlpha * 255.0f));
		}
		else
		{
			int iRenderGroup = gHUD.LookupRenderGroupIndexByName( "hide_for_round_panel" );
			if ( iRenderGroup >= 0 )
			{
				gHUD.UnlockRenderGroup( iRenderGroup );
			}
			m_bShouldBeVisible = false;
			SetAlpha(0);			
			m_bIsFading = false;
		}
	}
}

void WinPanel_Round::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetFgColor(Color(251,176,59,255));
	SetBgColor(Color(0,0,0,212));
}

void WinPanel_Round::OnScreenSizeChanged( int nOldWide, int nOldTall )
{
	BaseClass::OnScreenSizeChanged(nOldWide, nOldTall);

	InitLayout();


}

bool WinPanel_Round::ShouldDraw( void )
{
	return ( m_bShouldBeVisible && CHudElement::ShouldDraw());
}
