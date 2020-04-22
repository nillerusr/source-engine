//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "tf_hud_chat.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "vgui/ILocalize.h"
#include "engine/IEngineSound.h"
#include "c_tf_team.h"
#include "c_playerresource.h"
#include "c_tf_player.h"
#include "tf_gamerules.h"
#include "ihudlcd.h"
#include "tf_hud_freezepanel.h"
#if defined( REPLAY_ENABLED )
#include "replay/ienginereplay.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudChat );
DECLARE_HUD_MESSAGE( CHudChat, SayText );
DECLARE_HUD_MESSAGE( CHudChat, SayText2 );
DECLARE_HUD_MESSAGE( CHudChat, TextMsg );
DECLARE_HUD_MESSAGE( CHudChat, VoiceSubtitle );


//=====================
//CHudChatLine
//=====================

void CHudChatLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hFont = pScheme->GetFont( "ChatFont" );
	SetBorder( NULL );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetFgColor( Color( 0, 0, 0, 0 ) );

	SetFont( m_hFont );
}

CHudChatLine::CHudChatLine( vgui::Panel *parent, const char *panelName ) : CBaseHudChatLine( parent, panelName )
{
	m_text = NULL;
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
#if defined ( _X360 )
	RegisterForRenderGroup( "mid" );
#endif
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

	HOOK_HUD_MESSAGE( CHudChat, SayText );
	HOOK_HUD_MESSAGE( CHudChat, SayText2 );
	HOOK_HUD_MESSAGE( CHudChat, TextMsg );
	HOOK_HUD_MESSAGE( CHudChat, VoiceSubtitle );
}

//-----------------------------------------------------------------------------
// Purpose: Hides us when our render group is hidden
// move render group to base chat when its safer
//-----------------------------------------------------------------------------
bool CHudChat::ShouldDraw( void )
{
#if defined( REPLAY_ENABLED )
	extern IEngineClientReplay *g_pEngineClientReplay;
	if ( g_pEngineClientReplay->IsPlayingReplayDemo() )
		return false;
#endif

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: Overrides base reset to not cancel chat at round restart
//-----------------------------------------------------------------------------
void CHudChat::Reset( void )
{
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

int CHudChat::GetFilterForString( const char *pString )
{
	int iFilter = BaseClass::GetFilterForString( pString );

	if ( iFilter == CHAT_FILTER_NONE )
	{
		if ( !Q_stricmp( pString, "#TF_Name_Change" ) ) 
		{
			return CHAT_FILTER_NAMECHANGE;
		}
	}

	return iFilter;
}

//-----------------------------------------------------------------------------
Color CHudChat::GetClientColor( int clientIndex )
{
	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );

	if ( pScheme == NULL )
		return Color( 255, 255, 255, 255 );

	if ( clientIndex == 0 ) // console msg
	{
		return g_ColorGreen;
	}
	else if( g_PR )
	{
		int iTeam = g_PR->GetTeam( clientIndex );

		C_TFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( clientIndex ) );
		C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

		if ( IsVoiceSubtitle() == true )
		{
			// if this player is on the other team, disguised as my team, show disguised color
			if ( pPlayer && pLocalPlayer && pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) &&
				pPlayer->m_Shared.GetDisguiseTeam() == pLocalPlayer->GetTeamNumber() )
			{
				iTeam = pPlayer->m_Shared.GetDisguiseTeam();
			}
		}

		switch ( iTeam )
		{
		case TF_TEAM_RED	: return pScheme->GetColor( "TFColors.ChatTextRed", g_ColorRed );
		case TF_TEAM_BLUE	: return pScheme->GetColor( "TFColors.ChatTextBlue", g_ColorBlue );
		default	: return g_ColorGrey;
		}
	}

	return g_ColorYellow;
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

const char *CHudChat::GetDisplayedSubtitlePlayerName( int clientIndex )
{
	C_TFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( clientIndex ) );
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pPlayer || !pLocalPlayer )
		return BaseClass::GetDisplayedSubtitlePlayerName( clientIndex );

	// If they are disguised as the enemy, and not on our team
	if ( pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) &&
		pPlayer->m_Shared.GetDisguiseTeam() != pPlayer->GetTeamNumber() && 
		!pLocalPlayer->InSameTeam( pPlayer ) )
	{
		C_TFPlayer *pDisguiseTarget = ToTFPlayer( pPlayer->m_Shared.GetDisguiseTarget() );

		Assert( pDisguiseTarget );

		if ( !pDisguiseTarget )
		{
			return BaseClass::GetDisplayedSubtitlePlayerName( clientIndex );
		}

		return pDisguiseTarget->GetPlayerName();
	}

	return BaseClass::GetDisplayedSubtitlePlayerName( clientIndex );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Color CHudChat::GetTextColorForClient( TextColor colorNum, int clientIndex )
{
	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );

	if ( pScheme == NULL )
		return Color( 255, 255, 255, 255 );

	Color c;
	switch ( colorNum )
	{
	case COLOR_CUSTOM:
		c = m_ColorCustom;
		break;

	case COLOR_PLAYERNAME:
		c = GetClientColor( clientIndex );
		break;

	case COLOR_LOCATION:
		c = g_ColorDarkGreen;
		break;

	case COLOR_ACHIEVEMENT:
		{
			IScheme *pSourceScheme = scheme()->GetIScheme( scheme()->GetScheme( "SourceScheme" ) ); 
			if ( pSourceScheme )
			{
				c = pSourceScheme->GetColor( "SteamLightGreen", GetBgColor() );
			}
			else
			{
				c = pScheme->GetColor( "TFColors.ChatTextYellow", GetBgColor() );
			}
		}
		break;

	default:
		c = pScheme->GetColor( "TFColors.ChatTextYellow", GetBgColor() );
	}

	return Color( c[0], c[1], c[2], 255 );
}

int CHudChat::GetFilterFlags( void )
{
//=============================================================================
// HPE_BEGIN:
// [msmith]	We don't want to be displaying these chat messages when we're in training.
//			This is because we don't want the player seeing when bots join etc.
//=============================================================================
	if ( TFGameRules() && TFGameRules()->IsInTraining() )
		return CHAT_FILTER_PUBLICCHAT;
//=============================================================================
// HPE_END
//=============================================================================

	int iFlags = BaseClass::GetFilterFlags();

	if ( TFGameRules() && TFGameRules()->IsInArenaMode() == true )
	{
		return iFlags &= ~CHAT_FILTER_TEAMCHANGE;
	}
	
	return iFlags;
}
