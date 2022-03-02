//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "hud_radar.h"
#include "cs_hud_chat.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "vgui/ILocalize.h"
#include "engine/IEngineSound.h"
#include "radio_status.h"
#include "cstrike/bot/shared_util.h"
#include "ihudlcd.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudChat );

DECLARE_HUD_MESSAGE( CHudChat, RadioText );
DECLARE_HUD_MESSAGE( CHudChat, SayText );
DECLARE_HUD_MESSAGE( CHudChat, SayText2 );
DECLARE_HUD_MESSAGE( CHudChat, TextMsg );
DECLARE_HUD_MESSAGE( CHudChat, RawAudio );


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

	vgui::HFont hFont = pScheme->GetFont( "ChatFont" );

	m_pPrompt->SetFont( hFont );
	m_pInput->SetFont( hFont );

	m_pInput->SetFgColor( pScheme->GetColor( "Chat.TypingText", pScheme->GetColor( "Panel.FgColor", Color( 255, 255, 255, 255 ) ) ) );
}



//=====================
//CHudChat
//=====================

CHudChat::CHudChat( const char *pElementName ) : BaseClass( pElementName )
{
	//=============================================================================
	// HPE_BEGIN:
	// [tj] Add this to the render group that disappears when the scoreboard is up
	//
	// [pmf] Removed from render group so that chat still works during intermission
	// (when the scoreboard is forced to be up). The downside is that chat shows
	// over the scoreboard during regular play, but this might be less of an issue
	// if we reduce the need to display it constantly by adding HUD support for
	// live player counts.
	//=============================================================================
//	RegisterForRenderGroup("hide_for_scoreboard");
	//=============================================================================
	// HPE_END
	//=============================================================================
}

void CHudChat::CreateChatInputLine( void )
{
	m_pChatInput = new CHudChatInputLine( this, "ChatInputLine" );
	m_pChatInput->SetVisible( false );
}

void CHudChat::CreateChatLines( void )
{
#ifndef _XBOX
	m_ChatLine = new CHudChatLine( this, "ChatLine1" );
	m_ChatLine->SetVisible( false );		

#endif
}

void CHudChat::Init( void )
{
	BaseClass::Init();

	HOOK_HUD_MESSAGE( CHudChat, RadioText );
	HOOK_HUD_MESSAGE( CHudChat, SayText );
	HOOK_HUD_MESSAGE( CHudChat, SayText2 );
	HOOK_HUD_MESSAGE( CHudChat, TextMsg );
	HOOK_HUD_MESSAGE( CHudChat, RawAudio );
}

//-----------------------------------------------------------------------------
// Purpose: Overrides base reset to not cancel chat at round restart
//-----------------------------------------------------------------------------
void CHudChat::Reset( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a player's Radio text from the server
//-----------------------------------------------------------------------------
void CHudChat::MsgFunc_RadioText( bf_read &msg )
{
	int msg_dest = msg.ReadByte();
	NOTE_UNUSED( msg_dest );
	int client = msg.ReadByte();

	wchar_t szBuf[6][128];
	wchar_t *msg_text = ReadLocalizedString( msg, szBuf[0], sizeof( szBuf[0] ), false );

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	ReadChatTextString ( msg, szBuf[1], sizeof( szBuf[1] ) );		// player name
	ReadLocalizedString( msg, szBuf[2], sizeof( szBuf[2] ), true );	// location
	ReadLocalizedString( msg, szBuf[3], sizeof( szBuf[3] ), true );	// radio text
	ReadLocalizedString( msg, szBuf[4], sizeof( szBuf[4] ), true );	// unused :(

	g_pVGuiLocalize->ConstructString( szBuf[5], sizeof( szBuf[5] ), msg_text, 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );

	char ansiString[512];
	g_pVGuiLocalize->ConvertUnicodeToANSI( ConvertCRtoNL( szBuf[5] ), ansiString, sizeof( ansiString ) );
	ChatPrintf( client, CHAT_FILTER_TEAMCHANGE, "%s", ansiString );

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message" );
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a player's Chat text from the server
//-----------------------------------------------------------------------------
void CHudChat::MsgFunc_SayText2( bf_read &msg )
{
	// Got message during connection
	if ( !g_PR )
		return;

	int client = msg.ReadByte();
	bool bWantsToChat = msg.ReadByte();

	wchar_t szBuf[6][256];
	char untranslated_msg_text[256];
	wchar_t *msg_text = ReadLocalizedString( msg, szBuf[0], sizeof( szBuf[0] ), false, untranslated_msg_text, sizeof( untranslated_msg_text ) );

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	ReadChatTextString ( msg, szBuf[1], sizeof( szBuf[1] ) );		// player name
	ReadChatTextString ( msg, szBuf[2], sizeof( szBuf[2] ) );		// chat text
	ReadLocalizedString( msg, szBuf[3], sizeof( szBuf[3] ), true );	// location
	ReadLocalizedString( msg, szBuf[4], sizeof( szBuf[4] ), true );	// unused :(

	g_pVGuiLocalize->ConstructString( szBuf[5], sizeof( szBuf[5] ), msg_text, 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );

	char ansiString[512];
	g_pVGuiLocalize->ConvertUnicodeToANSI( ConvertCRtoNL( szBuf[5] ), ansiString, sizeof( ansiString ) );

	// flash speaking player dot
	if ( client > 0 )
		Radar_FlashPlayer( client );

	if ( bWantsToChat )
	{
		int iFilter = CHAT_FILTER_NONE;
		bool playChatSound = true;

		if ( client > 0 && (g_PR->GetTeam( client ) != g_PR->GetTeam( GetLocalPlayerIndex() )) )
		{
			iFilter = CHAT_FILTER_PUBLICCHAT;
			if ( !( iFilter & GetFilterFlags() ) )
			{
				playChatSound = false;
			}
		}

		// print raw chat text
		ChatPrintf( client, iFilter, "%s", ansiString );

		Msg( "%s\n", RemoveColorMarkup(ansiString) );

		if ( playChatSound )
		{
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message" );
		}
	}
	else
	{
		// print raw chat text
		ChatPrintf( client, GetFilterForString( untranslated_msg_text), "%s", ansiString );
	}
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

//-----------------------------------------------------------------------------
// Purpose: Reads in an Audio message from the server (wav file to be played
//          via the player's voice, i.e. for bot chatter)
//-----------------------------------------------------------------------------
void CHudChat::MsgFunc_RawAudio( bf_read &msg )
{
	char szString[2048];
	int pitch = msg.ReadByte();
	int playerIndex = msg.ReadByte();
	float feedbackDuration = msg.ReadFloat();
	msg.ReadString( szString, sizeof(szString) );

	EmitSound_t ep;
	ep.m_nChannel = CHAN_VOICE;
	ep.m_pSoundName = szString;
	ep.m_flVolume = 1.0f;
	ep.m_SoundLevel = SNDLVL_NORM;
	ep.m_nPitch = pitch;

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, ep );

	if ( feedbackDuration > 0.0f )
	{
		//Flash them on the radar
		C_CSPlayer *pPlayer = static_cast<C_CSPlayer*>( cl_entitylist->GetEnt(playerIndex) );

		if ( pPlayer )
		{
			// Create the flashy above player's head
			RadioManager()->UpdateVoiceStatus( playerIndex, feedbackDuration );
		}
	}
}


//-----------------------------------------------------------------------------
Color CHudChat::GetClientColor( int clientIndex )
{
	if ( clientIndex == 0 ) // console msg
	{
		return g_ColorGreen;
	}
	else if( g_PR )
	{
		switch ( g_PR->GetTeam( clientIndex ) )
		{
		case 2	: return g_ColorRed;
		case 3	: return g_ColorBlue;
		default	: return g_ColorGrey;
		}
	}

	return g_ColorYellow;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Color CHudChat::GetTextColorForClient( TextColor colorNum, int clientIndex )
{
	Color c;
	switch ( colorNum )
	{
	case COLOR_PLAYERNAME:
		c = GetClientColor( clientIndex );
		break;

	case COLOR_LOCATION:
		c = g_ColorDarkGreen;
		break;

    //=============================================================================
    // HPE_BEGIN:
    // [tj] Adding support for achievement coloring. 
    //      Just doing what all the other games do
    //=============================================================================
     
    case COLOR_ACHIEVEMENT:
        {
            vgui::IScheme *pSourceScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "SourceScheme" ) ); 
            if ( pSourceScheme )
            {
                c = pSourceScheme->GetColor( "SteamLightGreen", GetBgColor() );
            }
            else
            {
                c = GetDefaultTextColor();
            }
        }
        break;
     
    //=============================================================================
    // HPE_END
    //=============================================================================
    

	default:
		c = g_ColorYellow;
	}

	return Color( c[0], c[1], c[2], 255 );
}

int CHudChat::GetFilterForString( const char *pString )
{
	int iFilter = BaseClass::GetFilterForString( pString );

	if ( iFilter == CHAT_FILTER_NONE )
	{
		if ( !Q_stricmp( pString, "#CStrike_Name_Change" ) ) 
		{
			return CHAT_FILTER_NAMECHANGE;
		}
	}

	return iFilter;
}

void CHudChat::StartMessageMode( int iMessageModeType )
{
	BaseClass::StartMessageMode(iMessageModeType);

	vgui::ipanel()->SetTopmostPopup(GetVPanel(), true);
}

void CHudChat::StopMessageMode( void )
{
	vgui::ipanel()->SetTopmostPopup(GetVPanel(), false);

	BaseClass::StopMessageMode();
}