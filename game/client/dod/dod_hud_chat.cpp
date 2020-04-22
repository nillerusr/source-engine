//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "dod_hud_chat.h"
#include "c_dod_player.h"
#include "c_dod_playerresource.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "vgui/ILocalize.h"
#include "dodoverview.h"
#include <voice_status.h>
#include "menu.h" // for CHudMenu defs
#include "dod_hud_freezepanel.h"

ConVar cl_voicesubtitles( "cl_voicesubtitles", "1", 0, "Enable/disable subtitle printing on voice commands and hand signals." );

Color g_DoDColorGrey( 200, 200, 200, 255 );

#define DOD_MAX_CHAT_LENGTH	128

// Stuff for the Radio Menus
static void voicemenu1_f( void );
static void voicemenu2_f( void );
static void voicemenu3_f( void );

static ConCommand voicemenu1( "voicemenu1", voicemenu1_f, "Opens a voice menu" );
static ConCommand voicemenu2( "voicemenu2", voicemenu2_f, "Opens a voice menu" );
static ConCommand voicemenu3( "voicemenu3", voicemenu3_f, "Opens a voice menu" );


//
//--------------------------------------------------------------
//
// These methods will bring up the radio menus from the client side.
// They mimic the old server commands of the same name, which used
// to require a round-trip causing latency and unreliability in 
// menu responses.  Only 1 message is sent to the server now which
// includes both the menu name and the selected item.  The server
// is never informed that the menu has been displayed.
//
//--------------------------------------------------------------
//

static int g_ActiveVoiceMenu = 0;

void OpenVoiceMenu( int index )
{
	// do not show the menu if the player is dead or is an observer
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( !pPlayer )
		return;

	if ( !pPlayer->IsAlive() || pPlayer->IsObserver() )
		return;

	CHudMenu *pMenu = (CHudMenu *) gHUD.FindElement( "CHudMenu" );
	if ( !pMenu )
		return;

	// if they hit the key again, close the menu
	if ( g_ActiveVoiceMenu == index )
	{
		pMenu->HideMenu();
		g_ActiveVoiceMenu = 0;
		return;
	}

	switch( index )
	{
	case 1:
	case 2:
	case 3:
		{
			char *pszNation;

			if( pPlayer->GetTeamNumber() == TEAM_ALLIES )
			{
				pszNation = "Amer";
			}
			else
			{
				pszNation = "Ger";
			}

			char cMenuNumber = 'A' + index - 1;	// map 1,2,3 to A,B,C

			char szMenu[128];
			Q_snprintf( szMenu, sizeof(szMenu), "#Menu_%sVoice%c", pszNation, cMenuNumber );

			pMenu->ShowMenu( szMenu, 0x3FF );

			g_ActiveVoiceMenu = index;
		}
		break;
	case 0:
	default:
		g_ActiveVoiceMenu = 0;
		break;
	}
}

static void voicemenu1_f( void )
{
	OpenVoiceMenu( 1 );
}

static void voicemenu2_f( void )
{
	OpenVoiceMenu( 2 );
}

static void voicemenu3_f( void )
{
	OpenVoiceMenu( 3 );
}

CON_COMMAND( menuselect, "menuselect" )
{
	if ( args.ArgC() < 2 )
		return;

	int iSlot = atoi( args[1] );

	switch( g_ActiveVoiceMenu )
	{
	case 1: //RadioA
	case 2:
	case 3:
		{
			// check for cancel
			if( iSlot == 10 )
			{
				g_ActiveVoiceMenu = 0;
				return;
			}

			// find the voice command index from the menu and slot
			int iVoiceCommand = (g_ActiveVoiceMenu-1) * 9 + (iSlot-1);

			Assert( iVoiceCommand >= 0 && iVoiceCommand < (3*9) );

			// emit a voice command
			engine->ClientCmd( g_VoiceCommands[iVoiceCommand].pszCommandName );
		}
		break;
	case 0:
	default:
		// if we didn't have a menu open, maybe a plugin did.  send it on to the server.
		const char *cmd = VarArgs( "menuselect %d", iSlot );
		engine->ServerCmd( cmd );
		break;
	}

	// reset menu
	g_ActiveVoiceMenu = 0;
}

DECLARE_HUDELEMENT( CHudChat );

DECLARE_HUD_MESSAGE( CHudChat, SayText );
DECLARE_HUD_MESSAGE( CHudChat, TextMsg );
DECLARE_HUD_MESSAGE( CHudChat, VoiceSubtitle );
DECLARE_HUD_MESSAGE( CHudChat, HandSignalSubtitle );


//=====================
//CHudChatLine
//=====================

CHudChatLine::CHudChatLine( vgui::Panel *parent, const char *panelName ) : CBaseHudChatLine( parent, panelName )
{
	m_text = NULL;
}

void CHudChatLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );
}

//=====================
//CHudChatInputLine
//=====================

void CHudChatInputLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}



//=====================
//CHudChat
//=====================

CHudChat::CHudChat( const char *pElementName ) : BaseClass( pElementName )
{
	
}

void CHudChat::CreateChatInputLine( void )
{
	m_pChatInput = new CHudChatInputLine( this, "ChatInputLine" );
	m_pChatInput->SetVisible( false );
}

void CHudChat::CreateChatLines( void )
{
	m_ChatLine = new CHudChatLine( this, "ChatLine1" );
	m_ChatLine->SetVisible( false );	
}

void CHudChat::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
}


void CHudChat::Init( void )
{
	BaseClass::Init();

	HOOK_HUD_MESSAGE( CHudChat, SayText );
	HOOK_HUD_MESSAGE( CHudChat, TextMsg );
	HOOK_HUD_MESSAGE( CHudChat, VoiceSubtitle );
	HOOK_HUD_MESSAGE( CHudChat, HandSignalSubtitle );
}

//-----------------------------------------------------------------------------
// Purpose: Overrides base reset to not cancel chat at round restart
//-----------------------------------------------------------------------------
void CHudChat::Reset( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
//-----------------------------------------------------------------------------
void CHudChat::MsgFunc_SayText( bf_read &msg )
{
	// Got message during connection
	if ( !g_PR )
		return;

	char szString[DOD_MAX_CHAT_LENGTH];

	int client = msg.ReadByte();
	msg.ReadString( szString, sizeof(szString) );
	bool bWantsToChat = msg.ReadByte();

	if ( GetClientVoiceMgr()->IsPlayerBlocked( client ) )
		return;

	if ( client > 0 )
		GetDODOverview()->PlayerChat( client );

	if ( bWantsToChat )
	{
		int iFilter = CHAT_FILTER_NONE;

		if ( client > 0 && (g_PR->GetTeam( client ) != g_PR->GetTeam( GetLocalPlayerIndex() )) )
		{
			iFilter = CHAT_FILTER_PUBLICCHAT;
		}

		// Localize any words in the chat that start with #
		FindLocalizableSubstrings( szString, sizeof(szString) );		

		// print raw chat text
		 ChatPrintf( client, iFilter, "%c%s", COLOR_USEOLDCOLORS, szString );
	}
	else
	{
		// try to lookup translated string
		Printf( GetFilterForString( szString ), "%s", hudtextmessage->LookupString( szString ) );
	}

	Msg( "%s", szString );
}

void CHudChat::MsgFunc_VoiceSubtitle( bf_read &msg )
{
	// Got message during connection
	if ( !g_PR )
		return;

	if ( !cl_showtextmsg.GetInt() )
		return;

	if ( !cl_voicesubtitles.GetInt() )
		return;

	char szString[2048];
	char szPrefix[64];	//(Voice)
	wchar_t szBuf[128];

	int client = msg.ReadByte();

	if ( GetClientVoiceMgr()->IsPlayerBlocked( client ) )
		return;

	if ( client > 0 )
		GetDODOverview()->PlayerChat( client );

	int iTeam = msg.ReadByte();
	int iVoiceCommand = msg.ReadByte();

	//Assert( iVoiceCommand <= ARRAYSIZE(g_VoiceCommands) );
	Assert( iTeam == TEAM_ALLIES || iTeam == TEAM_AXIS );

	const char *pszSubtitle = g_VoiceCommands[iVoiceCommand].pszAlliedSubtitle;

	if( iTeam == TEAM_AXIS && g_VoiceCommands[iVoiceCommand].pszAxisSubtitle != NULL )
		pszSubtitle = g_VoiceCommands[iVoiceCommand].pszAxisSubtitle;

	const wchar_t *pBuf = g_pVGuiLocalize->Find( pszSubtitle );
	if ( pBuf )
	{
		// Copy pBuf into szBuf[i].
		int nMaxChars = sizeof( szBuf ) / sizeof( wchar_t );
		wcsncpy( szBuf, pBuf, nMaxChars );
		szBuf[nMaxChars-1] = 0;
	}
	else
	{
		g_pVGuiLocalize->ConvertANSIToUnicode( pszSubtitle, szBuf, sizeof(szBuf) );
	}

	int len;
	g_pVGuiLocalize->ConvertUnicodeToANSI( szBuf, szString, sizeof(szString) );
	len = strlen( szString );
	if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
	{
		Q_strncat( szString, "\n", sizeof(szString), 1 );
	}

	const wchar_t *pVoicePrefix = g_pVGuiLocalize->Find( "#Voice" );
	g_pVGuiLocalize->ConvertUnicodeToANSI( pVoicePrefix, szPrefix, sizeof(szPrefix) );

	ChatPrintf( client, CHAT_FILTER_NONE, "%c(%s) %s%c: %s", COLOR_PLAYERNAME, szPrefix, g_PR->GetPlayerName( client ), COLOR_NORMAL, ConvertCRtoNL( szString ) );
}

void CHudChat::MsgFunc_HandSignalSubtitle( bf_read &msg )
{
	// Got message during connection
	if ( !g_PR )
		return;

	if ( !cl_showtextmsg.GetInt() )
		return;

	if ( !cl_voicesubtitles.GetInt() )
		return;

	char szString[2048];
	char szPrefix[64];	//(HandSignal)
	wchar_t szBuf[128];

	int client = msg.ReadByte();
	int iSignal = msg.ReadByte();

	if ( GetClientVoiceMgr()->IsPlayerBlocked( client ) )
		return;

	if ( client > 0 )
		GetDODOverview()->PlayerChat( client );

	const char *pszSubtitle = g_HandSignals[iSignal].pszSubtitle;

	const wchar_t *pBuf = g_pVGuiLocalize->Find( pszSubtitle );
	if ( pBuf )
	{
		// Copy pBuf into szBuf[i].
		int nMaxChars = sizeof( szBuf ) / sizeof( wchar_t );
		wcsncpy( szBuf, pBuf, nMaxChars );
		szBuf[nMaxChars-1] = 0;
	}
	else
	{
		g_pVGuiLocalize->ConvertANSIToUnicode( pszSubtitle, szBuf, sizeof(szBuf) );
	}

	int len;
	g_pVGuiLocalize->ConvertUnicodeToANSI( szBuf, szString, sizeof(szString) );
	len = strlen( szString );
	if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
	{
		Q_strncat( szString, "\n", sizeof(szString), 1 );
	}

	const wchar_t *pVoicePrefix = g_pVGuiLocalize->Find( "#HandSignal" );
	g_pVGuiLocalize->ConvertUnicodeToANSI( pVoicePrefix, szPrefix, sizeof(szPrefix) );

	Assert( g_PR );

	ChatPrintf( client, CHAT_FILTER_NONE, "%c(%s) %s%c: %s", COLOR_USEOLDCOLORS, szPrefix, g_PR->GetPlayerName( client ), COLOR_NORMAL, ConvertCRtoNL( szString ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHudChat::GetChatInputOffset( void )
{
	if ( m_pChatInput->IsVisible() )
	{
		return m_iFontHeight;
	}
	else
	{
		return 0;
	}
}

wchar_t* ReadLocalizedVoiceCommandString( bf_read &msg, wchar_t *pOut, int outSize, bool bStripNewline )
{
	char szString[2048];
	msg.ReadString( szString, sizeof(szString) );

	const wchar_t *pBuf = g_pVGuiLocalize->Find( szString );
	if ( pBuf )
	{
		wcsncpy( pOut, pBuf, outSize/sizeof(wchar_t) );
		pOut[outSize/sizeof(wchar_t)-1] = 0;
	}
	else
	{
		g_pVGuiLocalize->ConvertANSIToUnicode( szString, pOut, outSize );
	}

	if ( bStripNewline )
		StripEndNewlineFromString( pOut );

	return pOut;
}

// Find words in the chat that can be localized and replace them with localized text
void CHudChat::FindLocalizableSubstrings( char *szChat, int chatLength )
{
	static char buf[DOD_MAX_CHAT_LENGTH];
	buf[0] = '\0';

	int pos = 0;

	static char szTemp[DOD_MAX_CHAT_LENGTH];

	wchar_t *pwcLocalized;

	for ( char *pSrc = szChat; pSrc != NULL && *pSrc != 0 && pos < chatLength-1; pSrc++ )
	{
		// if its a #
		if ( *pSrc == '#' )
		{
			// trim into a word
			char *pWord = pSrc;
			int iWordPos = 0;
			while ( *pWord != ' ' &&
				*pWord != '\0' &&
				*pWord != '\n' )
			{	
				szTemp[iWordPos] = *pWord;
				iWordPos++;
				pWord++;
			}

			szTemp[iWordPos] = '\0';

			pwcLocalized = g_pVGuiLocalize->Find( szTemp );

			// localize word	

			if ( pwcLocalized )
			{
				// print it into the buf
				g_pVGuiLocalize->ConvertUnicodeToANSI( pwcLocalized, szTemp, sizeof(szTemp) );
			}

			// copy the string to chat
			buf[pos] = '\0';
			Q_strncat( buf, szTemp, DOD_MAX_CHAT_LENGTH, COPY_ALL_CHARACTERS );

			int remainingSpace = chatLength - pos - 2;	// minus one for the NULL and one for the space
			pos += MIN( Q_strlen(szTemp), remainingSpace );

			pSrc += iWordPos-1;		// progress pSrc to the end of the word
		}
		else
		{
			// copy each char across
			buf[pos] = *pSrc;
			pos++;
		}
	}
	
	buf[pos] = '\0';
	pos++;

	//copy buf back to szChat
	Q_strncpy( szChat, buf, MIN(chatLength, pos) );
}

Color CHudChat::GetDefaultTextColor( void )
{
	return g_DoDColorGrey;
}

Color CHudChat::GetClientColor( int clientIndex )
{
	// If player resource is bogus, return some obvious colour
	// or if we want a generic colour
	if ( !g_PR || clientIndex < 1 )
		return GetDefaultTextColor();

	return g_PR->GetTeamColor( g_PR->GetTeam( clientIndex ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudChat::IsVisible( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return BaseClass::IsVisible();
}