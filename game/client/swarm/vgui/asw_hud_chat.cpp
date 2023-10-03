//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  ASW version of hud_chat.cpp (exclude hud_chat.cpp from build)
//
//=============================================================================//

#include "cbase.h"
#include "asw_hud_chat.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "hud_basechat.h"
#include <vgui/ILocalize.h>
#include "asw_shareddefs.h"
#include "c_playerresource.h"
#include "asw_gamerules.h"
#include "nb_main_panel.h"
#include "IClientMode.h"
#include "vgui_controls/ScrollBar.h"

DECLARE_HUDELEMENT_FLAGS( CHudChat, HUDELEMENT_SS_FULLSCREEN_ONLY );

DECLARE_HUD_MESSAGE( CHudChat, SayText );
DECLARE_HUD_MESSAGE( CHudChat, SayText2 );
DECLARE_HUD_MESSAGE( CHudChat, TextMsg );

//Color g_ASWColorGrey( 66, 142, 192, 255 );
Color g_ASWColorGrey( 255, 255, 0, 255 );
Color g_ASWColorWhite( 255, 255, 255, 255 );
extern ConVar asw_hud_alpha;

//=====================
//CHudChat
//=====================

CHudChat::CHudChat( const char *pElementName ) : BaseClass( pElementName )
{
	SetProportional( false );
	m_bBriefingPosition = false;

	m_pSwarmBackground = new vgui::Panel( this, "SwarmBackground" );
	m_pSwarmBackgroundInner = new vgui::Panel( this, "SwarmBackgroundInner" );
}

void CHudChat::Init( void )
{
	BaseClass::Init();

	HOOK_HUD_MESSAGE( CHudChat, SayText );
	HOOK_HUD_MESSAGE( CHudChat, SayText2 );
	HOOK_HUD_MESSAGE( CHudChat, TextMsg );
}

void CHudChat::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetFgColor( Color( 0, 0, 0, 0 ) );

	if (m_pFiltersButton)
	{
		m_pFiltersButton->SetFont( pScheme->GetFont( "DefaultSmall", IsProportional() ) );
		m_pFiltersButton->SetVisible( false );
	}
	if ( GetChatHistory() && GetChatHistory()->GetScrollBar() )
	{
		GetChatHistory()->GetScrollBar()->UseImages( "scroll_up", "scroll_down", "scroll_line", "scroll_box" );
		GetChatHistory()->GetScrollBar()->SetVisible( false );
	}
	m_pSwarmBackground->SetBgColor( Color( 12, 23, 37, 64 ) );
	m_pSwarmBackground->SetPaintBackgroundType( 2 );
	m_pSwarmBackgroundInner->SetBgColor( Color( 16, 39, 63, 200 ) );
	m_pSwarmBackgroundInner->SetPaintBackgroundType( 2 );
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a player's Chat text from the server
//-----------------------------------------------------------------------------
void CHudChat::MsgFunc_SayText2( bf_read &msg )
{
	int client = msg.ReadByte();
	bool bWantsToChat = msg.ReadByte() ? true : false;

	wchar_t szBuf[6][256];
	char untranslated_msg_text[256];
	wchar_t *msg_text = ReadLocalizedString( msg, szBuf[0], sizeof( szBuf[0] ), false, untranslated_msg_text, sizeof( untranslated_msg_text ) );

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	ReadChatTextString ( msg, szBuf[1], sizeof( szBuf[1] ) );		// player name
	ReadChatTextString ( msg, szBuf[2], sizeof( szBuf[2] ) );		// chat text
	ReadLocalizedString( msg, szBuf[3], sizeof( szBuf[3] ), true );
	ReadLocalizedString( msg, szBuf[4], sizeof( szBuf[4] ), true );

	g_pVGuiLocalize->ConstructString( szBuf[5], sizeof( szBuf[5] ), msg_text, 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );

	char ansiString[512];
	g_pVGuiLocalize->ConvertUnicodeToANSI( ConvertCRtoNL( szBuf[5] ), ansiString, sizeof( ansiString ) );

	if ( bWantsToChat )
	{
		int iFilter = CHAT_FILTER_NONE;

		if ( client > 0 && g_PR && (g_PR->GetTeam( client ) != g_PR->GetTeam( GetLocalPlayerIndex() )) )
		{
			iFilter = CHAT_FILTER_PUBLICCHAT;
		}

		// print raw chat text
		ChatPrintf( client, iFilter, "%s", ansiString );

		Msg( "%s\n", RemoveColorMarkup(ansiString) );

		CLocalPlayerFilter filter;
		//C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message" );
	}
	else
	{
		// print raw chat text
		ChatPrintf( client, GetFilterForString( untranslated_msg_text), "%s", ansiString );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
//-----------------------------------------------------------------------------
void CHudChat::MsgFunc_SayText( bf_read &msg )
{
	char szString[256];

	int iPlayerID = msg.ReadByte(); // client ID
	msg.ReadString( szString, sizeof(szString) );
	//Printf( CHAT_FILTER_NONE, "%s", szString );
	ChatPrintf( iPlayerID, CHAT_FILTER_PUBLICCHAT, "%s", szString );
}


// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message direction  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message 
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in titles.txt
// the next (optional) one to four strings are parameters for that string (which can also be message names if they begin with '#')

void CHudChat::MsgFunc_TextMsg( bf_read &msg )
{
	char szString[2048];
	int msg_dest = msg.ReadByte();

	wchar_t szBuf[5][128];
	wchar_t outputBuf[256];

	for ( int i=0; i<5; ++i )
	{
		msg.ReadString( szString, sizeof(szString) );
		if ( szString[0] == '^' )
		{
			wchar_t wbuffer[ 64 ];
			g_pVGuiLocalize->ConvertANSIToUnicode( szString, wbuffer, sizeof( wbuffer ) );
			UTIL_ReplaceKeyBindings( wbuffer + 1, sizeof( wbuffer ) - sizeof(*wbuffer), szBuf[i], sizeof( szBuf[i] ) );
		}
		else
		{
			char *tmpStr = hudtextmessage->LookupString( szString, &msg_dest );
			const wchar_t *pBuf = g_pVGuiLocalize->Find( tmpStr );
			if ( pBuf )
			{
				// Copy pBuf into szBuf[i].
				int nMaxChars = sizeof( szBuf[i] ) / sizeof( wchar_t );
				wcsncpy( szBuf[i], pBuf, nMaxChars );
				szBuf[i][nMaxChars-1] = 0;
			}
			else
			{
				if ( i )
				{
					StripEndNewlineFromString( tmpStr );  // these strings are meant for subsitution into the main strings, so cull the automatic end newlines
				}
				g_pVGuiLocalize->ConvertANSIToUnicode( tmpStr, szBuf[i], sizeof(szBuf[i]) );
			}
		}
	}

	if ( !cl_showtextmsg.GetInt() )
		return;

	int len;
	switch ( msg_dest )
	{
	case HUD_PRINTCENTER:
		g_pVGuiLocalize->ConstructString( outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		GetCenterPrint()->Print( ConvertCRtoNL( outputBuf ) );
		break;

	case HUD_PRINTNOTIFY:
		szString[0] = 1;  // mark this message to go into the notify buffer
		g_pVGuiLocalize->ConstructString( outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		g_pVGuiLocalize->ConvertUnicodeToANSI( outputBuf, szString+1, sizeof(szString)-1 );
		len = strlen( szString );
		if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
		{
			Q_strncat( szString, "\n", sizeof(szString), 1 );
		}
		Msg( "%s", ConvertCRtoNL( szString ) );
		break;

	case HUD_PRINTTALK:
		g_pVGuiLocalize->ConstructString( outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		g_pVGuiLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof(szString) );
		len = strlen( szString );
		if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
		{
			Q_strncat( szString, "\n", sizeof(szString), 1 );
		}
		ChatPrintf( 0, CHAT_FILTER_SERVERMSG, "%s", ConvertCRtoNL( szString ) );
		break;

	case HUD_PRINTCONSOLE:
		g_pVGuiLocalize->ConstructString( outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		g_pVGuiLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof(szString) );
		len = strlen( szString );
		if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
		{
			Q_strncat( szString, "\n", sizeof(szString), 1 );
		}
		Msg( "%s", ConvertCRtoNL( szString ) );
		break;
	case ASW_HUD_PRINTTALKANDCONSOLE:
		g_pVGuiLocalize->ConstructString( outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		g_pVGuiLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof(szString) );
		len = strlen( szString );
		if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
		{
			Q_strncat( szString, "\n", sizeof(szString), 1 );
		}
		Msg( "%s", ConvertCRtoNL( szString ) );
		ChatPrintf( 0, CHAT_FILTER_SERVERMSG, "%s", ConvertCRtoNL( szString ) );
		break;
	}
	/*
	char szString[2048];
	int msg_dest = msg.ReadByte();
	static char szBuf[6][128];

	msg.ReadString( szString, sizeof(szString) );
	char *msg_text = hudtextmessage->LookupString( szString, &msg_dest );
	Q_strncpy( szBuf[0], msg_text, sizeof( szBuf[0] ) );
	msg_text = szBuf[0];

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	msg.ReadString( szString, sizeof(szString) );
	char *sstr1 = hudtextmessage->LookupString( szString );
	Q_strncpy( szBuf[1], sstr1, sizeof( szBuf[1] ) );
	sstr1 = szBuf[1];

	StripEndNewlineFromString( sstr1 );  // these strings are meant for subsitution into the main strings, so cull the automatic end newlines
	msg.ReadString( szString, sizeof(szString) );
	char *sstr2 = hudtextmessage->LookupString( szString );
	Q_strncpy( szBuf[2], sstr2, sizeof( szBuf[2] ) );
	sstr2 = szBuf[2];

	StripEndNewlineFromString( sstr2 );
	msg.ReadString( szString, sizeof(szString) );
	char *sstr3 = hudtextmessage->LookupString( szString );
	Q_strncpy( szBuf[3], sstr3, sizeof( szBuf[3] ) );
	sstr3 = szBuf[3];

	StripEndNewlineFromString( sstr3 );
	msg.ReadString( szString, sizeof(szString) );
	char *sstr4 = hudtextmessage->LookupString( szString );
	Q_strncpy( szBuf[4], sstr4, sizeof( szBuf[4] ) );
	sstr4 = szBuf[4];

	StripEndNewlineFromString( sstr4 );
	char *psz = szBuf[5];

	if ( !cl_showtextmsg.GetInt() )
	return;

	switch ( msg_dest )
	{
	case HUD_PRINTCENTER:
	Q_snprintf( psz, sizeof( szBuf[5] ), msg_text, sstr1, sstr2, sstr3, sstr4 );
	internalCenterPrint->Print( ConvertCRtoNL( psz ) );
	break;

	case HUD_PRINTNOTIFY:
	psz[0] = 1;  // mark this message to go into the notify buffer
	Q_snprintf( psz+1, sizeof( szBuf[5] ) - 1, msg_text, sstr1, sstr2, sstr3, sstr4 );
	Msg( "%s", ConvertCRtoNL( psz ) );
	break;

	case HUD_PRINTTALK:
	Q_snprintf( psz, sizeof( szBuf[5] ), msg_text, sstr1, sstr2, sstr3, sstr4 );
	Printf( CHAT_FILTER_NONE, "%s", ConvertCRtoNL( psz ) );
	break;

	case HUD_PRINTCONSOLE:
	Q_snprintf( psz, sizeof( szBuf[5] ), msg_text, sstr1, sstr2, sstr3, sstr4 );
	Msg( "%s", ConvertCRtoNL( psz ) );
	break;
	// asw added
	#ifdef INFESTED_DLL
	case ASW_HUD_PRINTTALKANDCONSOLE:		
	Q_snprintf( psz, sizeof( szBuf[5] ), msg_text, sstr1, sstr2, sstr3, sstr4 );
	// talk
	Printf( CHAT_FILTER_NONE, "%s", ConvertCRtoNL( psz ) );
	// console		
	Msg( "%s", ConvertCRtoNL( psz ) );
	break;
	#endif
	}
	*/
}


int CHudChat::GetChatInputOffset( void )
{
	if ( m_pChatInput->IsVisible() )
	{
		return m_iFontHeight;
	}
	else
		return 0;
}

void CHudChat::InsertBlankPage()
{
	for (int i=0;i<7;i++)
	{
		ChatPrintf(0, CHAT_FILTER_NONE, " ");
	}
}

Color CHudChat::GetTextColorForClient( TextColor colorNum, int clientIndex )
{
	Color c;
	switch ( colorNum )
	{
	case COLOR_PLAYERNAME:
		c = GetClientColor( clientIndex );
		break;

	case COLOR_LOCATION:
		c = g_ASWColorGrey;
		break;

	default:
		c = GetClientColor( clientIndex );
		//c = g_ASWColorWhite;
	}

	return Color( c[0], c[1], c[2], 255 );
}

//-----------------------------------------------------------------------------
Color CHudChat::GetClientColor( int clientIndex )
{
	if ( clientIndex == 0 ) // console msg
	{
		return g_ASWColorGrey;
	}
	else if( g_PR )
	{
		return g_ASWColorWhite;
	}

	return g_ASWColorWhite;
}

void CHudChat::OnTick()
{
	BaseClass::OnTick();

	if ( !GetChatHistory() )
		return;

	if ( ASWGameRules() && 
		( ASWGameRules()->GetGameState() == ASW_GS_BRIEFING || ASWGameRules()->GetGameState() == ASW_GS_DEBRIEF || ASWGameRules()->GetGameState() == ASW_GS_CAMPAIGNMAP ) )
	{
		//m_flHistoryFadeTime = gpGlobals->curtime;
		GetChatHistory()->SetVerticalScrollbar( false );
	}	

 	int iLines = 7;
	GetChatHistory()->SetBounds( YRES( 10 ), YRES( 10 ), YRES( 300 ), m_iFontHeight * iLines );

	// hack to fix visibility of the scrollbar
	//GetChatHistory()->SetVerticalScrollbar( IsKeyBoardInputEnabled() );   	// scroll bar always visible if we're hijacked into the debrief
}

void CHudChat::StartMessageMode( int iMessageModeType )
{
	BaseClass::StartMessageMode( iMessageModeType );
}

void CHudChat::StopMessageMode( bool bFade )
{
	BaseClass::StopMessageMode( bFade );
}

void CHudChat::ShowChatPanel()
{
	m_pChatInput->SetVisible( true );
	if ( GetChatHistory() )
	{
		GetChatHistory()->SetVisible( true );
	}
	m_pChatInput->SetPaintBorderEnabled( true );
}

extern int g_asw_iPlayerListOpen;

void CHudChat::FadeChatHistory()
{
	// don't fade out chat during the briefing/debrief
	if ( ASWGameRules() && 
		( ASWGameRules()->GetGameState() == ASW_GS_BRIEFING || ASWGameRules()->GetGameState() == ASW_GS_LAUNCHING ) && GetChatHistory() )
	{
		SetBriefingPosition( true );
		/*
		CNB_Main_Panel *pMainPanel = dynamic_cast<CNB_Main_Panel*>( GetClientMode()->GetViewport()->FindChildByName( "MainPanel", true ) );
		if ( pMainPanel && !pMainPanel->m_hSubScreen.Get() && g_asw_iPlayerListOpen == 0 )
		{
			// fade in
			SetAlpha( 255 );
			GetChatHistory()->SetBgColor( Color( 0, 0, 0, 255 ) );
			//SetBgColor( Color( GetBgColor().r(), GetBgColor().g(), GetBgColor().b(), 255 ) );
			m_pChatInput->GetPrompt()->SetAlpha( 255 );
			m_pChatInput->GetInputPanel()->SetAlpha( 255 );
			m_pFiltersButton->SetAlpha( 255 );
			SetBriefingPosition( true );
			//SetKeyBoardInputEnabled( true );
			SetMouseInputEnabled( true );
			return;
		}
		*/
	}
	else
	{
		SetBriefingPosition( false );
	}	

	BaseClass::FadeChatHistory();

	SetPaintBorderEnabled( false );
	m_pSwarmBackground->SetAlpha( m_pChatInput->GetPrompt()->GetAlpha() );
	m_pSwarmBackgroundInner->SetAlpha( m_pChatInput->GetPrompt()->GetAlpha() );
}

void CHudChat::SetBriefingPosition( bool bUseBriefingPosition )
{
	if ( m_bBriefingPosition != bUseBriefingPosition )
	{
		m_bBriefingPosition = bUseBriefingPosition;
		InvalidateLayout();
	}
}

void CHudChat::PerformLayout( void )
{
	BaseClass::PerformLayout();

	if ( m_bBriefingPosition )
	{
		int x = ( ScreenWidth() * 0.5f ) - YRES( 264 );		
		SetPos( x, YRES( 349 ) );

		int iLines = 7;
		int iHistoryHeight = m_iFontHeight * iLines;
		SetSize( YRES( 319 ), YRES( 20 ) + iHistoryHeight + m_iFontHeight );
	}
	else
	{
		SetPos( YRES( 110 ), YRES( 345 ) );
	}

	m_pSwarmBackground->SetBounds( 0, 0, GetWide(), GetTall() );
	m_pSwarmBackground->SetZPos( 0 );
	int border = YRES( 2 );
	m_pSwarmBackgroundInner->SetBounds( border, border, GetWide() - border * 2, GetTall() - border * 2 );
	m_pSwarmBackgroundInner->SetZPos( 1 );
	if ( GetChatHistory() )
	{
		GetChatHistory()->SetZPos( 2 );
	}
	if ( GetChatInput() )
	{
		GetChatInput()->SetZPos( 3 );
	}
}