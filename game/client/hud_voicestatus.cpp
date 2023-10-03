//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui/isurface.h>
#include <vgui/ILocalize.h>
#include "c_baseplayer.h"
#include "voice_status.h"
#include "clientmode_shared.h"
#include "c_playerresource.h"
#include "voice_common.h"
#include "bitvec.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar *sv_alltalk = NULL;

//=============================================================================
// Icon for the local player using voice
//=============================================================================
class CHudVoiceSelfStatus : public CHudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudVoiceSelfStatus, vgui::Panel );

	CHudVoiceSelfStatus( const char *name );

	virtual bool ShouldDraw();	
	virtual void Paint();
	virtual void VidInit();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	CHudTexture *m_pVoiceIcon;


	Color	m_clrIcon;
};


DECLARE_HUDELEMENT( CHudVoiceSelfStatus );


CHudVoiceSelfStatus::CHudVoiceSelfStatus( const char *pName ) :
	vgui::Panel( NULL, "HudVoiceSelfStatus" ), CHudElement( pName )
{
	SetParent( GetClientMode()->GetViewport() );

	m_pVoiceIcon = NULL;

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	m_clrIcon = Color(255,255,255,255);
}

	
void CHudVoiceSelfStatus::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

}

void CHudVoiceSelfStatus::VidInit( void )
{
	m_pVoiceIcon = HudIcons().GetIcon( "voice_self" );
}

bool CHudVoiceSelfStatus::ShouldDraw()
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( !player )
		return false;

	if ( GetClientVoiceMgr()->IsLocalPlayerSpeaking( player->GetSplitScreenPlayerSlot() ) == false )
		return false;

	return CHudElement::ShouldDraw();	
}

void CHudVoiceSelfStatus::Paint()
{
   if( !m_pVoiceIcon )
		return;
	
	int x, y, w, h;
	GetBounds( x, y, w, h );

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( player &&
		GetClientVoiceMgr()->IsLocalPlayerSpeakingAboveThreshold( player->GetSplitScreenPlayerSlot() ) )
	{
		m_clrIcon[3] = 255;
	}
	else
	{
		// NOTE: Merge issue. This number should either be 0 or 255, dunno!
		m_clrIcon[3] = 0;
	}
	m_pVoiceIcon->DrawSelf( 0, 0, w, h, m_clrIcon );
}


//=============================================================================
// Icons for other players using voice
//=============================================================================
class CHudVoiceStatus : public CHudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudVoiceStatus, vgui::Panel );

	CHudVoiceStatus( const char *name );

	virtual bool ShouldDraw();	
	virtual void Paint();
	virtual void VidInit();
	virtual void Init();
	virtual void OnThink();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	CHudTexture *m_pVoiceIcon;
	int m_iDeadImageID;

	Color	m_clrIcon;

	CPlayerBitVec m_SpeakingList;
	
	CPanelAnimationVar( vgui::HFont, m_NameFont, "text_font", "Default" );

	CPanelAnimationVarAliasType( float, item_tall, "item_tall", "32", "proportional_float" );
	CPanelAnimationVarAliasType( float, item_wide, "item_wide", "100", "proportional_float" );

	CPanelAnimationVarAliasType( float, item_spacing, "item_spacing", "2", "proportional_float" );

	CPanelAnimationVarAliasType( float, icon_ypos, "icon_ypos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_xpos, "icon_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_tall, "icon_tall", "32", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_wide, "icon_wide", "32", "proportional_float" );

	CPanelAnimationVarAliasType( float, text_ypos, "text_ypos", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, text_xpos, "text_xpos", "32", "proportional_float" );
	
	CPanelAnimationVar( int, m_isInverted, "inverted", "0" );
};


DECLARE_HUDELEMENT( CHudVoiceStatus );


CHudVoiceStatus::CHudVoiceStatus( const char *pName ) :
	vgui::Panel( NULL, "HudVoiceStatus" ), CHudElement( pName )
{
	SetParent( GetClientMode()->GetViewport() );

	m_pVoiceIcon = NULL;

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	m_clrIcon = Color(255,255,255,255);

	m_iDeadImageID = surface()->DrawGetTextureId( "hud/leaderboard_dead" );
	if ( m_iDeadImageID == -1 ) // we didn't find it, so create a new one
	{
		m_iDeadImageID = surface()->CreateNewTextureID();	
	}

	surface()->DrawSetTextureFile( m_iDeadImageID, "hud/leaderboard_dead", true, false );
}

void CHudVoiceStatus::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );
}

void CHudVoiceStatus::Init( void )
{
	m_SpeakingList.ClearAll();
}

void CHudVoiceStatus::VidInit( void )
{
	m_pVoiceIcon = HudIcons().GetIcon( "voice_player" );
}

void CHudVoiceStatus::OnThink( void )
{
	CVoiceStatus *pVoiceMgr = GetClientVoiceMgr();

	for ( int i=0;i<gpGlobals->maxClients;++i )
	{
		bool bTalking = pVoiceMgr->IsPlayerSpeaking(i + 1);

		m_SpeakingList.Set( i, bTalking );
	}
}

bool CHudVoiceStatus::ShouldDraw()
{
	return false;
}

void CHudVoiceStatus::Paint()
{
   	if( !m_pVoiceIcon )
		return;
	
	int x, y, w, h;
	GetBounds( x, y, w, h );

	// Heights to draw the current voice item at
	int xpos = 0;
	int ypos = h - item_tall;

	bool bAnySpeakers = !m_SpeakingList.IsAllClear();

	int iFontHeight = 0;

	if( bAnySpeakers )
	{
		surface()->DrawSetTextFont( m_NameFont );
		surface()->DrawSetTextColor( Color(255,255,255,255) );
		iFontHeight = surface()->GetFontTall( m_NameFont );
	}

	if ( !sv_alltalk )
		sv_alltalk = cvar->FindVar( "sv_alltalk" );

	//draw everyone in the list!
	for( int si = 0; si < MAX_PLAYERS; ++si )
	{
		if ( !m_SpeakingList.IsBitSet( si ) )
			continue;

		int playerIndex = si + 1;

		bool bIsAlive = g_PR->IsAlive( playerIndex );

		Color c = g_PR->GetTeamColor( g_PR ? g_PR->GetTeam(playerIndex) : TEAM_UNASSIGNED );

		c[3] = 255;
	
		const char *pName = g_PR ? g_PR->GetPlayerName(playerIndex) : "unknown";
		wchar_t szconverted[ 64 ];

		// Add the location, if any
		bool usedLocation = false;
		if ( sv_alltalk && !sv_alltalk->GetBool() )
		{
			C_BasePlayer *pPlayer = UTIL_PlayerByIndex( playerIndex );
			if ( pPlayer )
			{
				const char *asciiLocation = pPlayer->GetLastKnownPlaceName();
				if ( asciiLocation && *asciiLocation )
				{
					const wchar_t *unicodeLocation = g_pVGuiLocalize->Find( asciiLocation );
					if ( unicodeLocation && *unicodeLocation )
					{
						wchar_t *formatStr = g_pVGuiLocalize->Find( "#Voice_UseLocation" );
						if ( formatStr )
						{
							wchar_t unicodeName[ 64 ];
							g_pVGuiLocalize->ConvertANSIToUnicode( pName, unicodeName, sizeof( unicodeName ) );

							g_pVGuiLocalize->ConstructString( szconverted, sizeof( szconverted ),
								formatStr, 2, unicodeName, unicodeLocation );

							usedLocation = true;
						}
					}
				}
			}
		}

		if ( !usedLocation )
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pName, szconverted, sizeof(szconverted)  );
		}

		// Draw the item background
		surface()->DrawSetColor( c );
		if ( !m_isInverted )
		{
			surface()->DrawFilledRect( xpos, ypos, xpos + item_wide, ypos + item_tall );
		}
	
		int iDeathIconWidth = 0;

		if ( bIsAlive == false && m_iDeadImageID != -1 )
		{
			Vertex_t vert[4];	
			float uv1 = 0.0f;
			float uv2 = 1.0f;

			// Draw the dead material
			surface()->DrawSetTexture( m_iDeadImageID );

			vert[0].Init( Vector2D( xpos, ypos ), Vector2D( uv1, uv1 ) );
			vert[1].Init( Vector2D( xpos + icon_wide, ypos ), Vector2D( uv2, uv1 ) );
			vert[2].Init( Vector2D( xpos + icon_wide, ypos + icon_tall ), Vector2D( uv2, uv2 ) );				
			vert[3].Init( Vector2D( xpos, ypos + icon_tall ), Vector2D( uv1, uv2 ) );

			surface()->DrawSetColor( Color(255,255,255,255) );

			surface()->DrawTexturedPolygon( 4, vert );

			iDeathIconWidth = icon_wide;
		}

		// Draw the voice icon
		m_pVoiceIcon->DrawSelf( xpos + icon_xpos + iDeathIconWidth, ypos + icon_ypos, icon_wide, icon_tall, m_clrIcon );

		// Draw the player's name
		surface()->DrawSetTextFont( m_NameFont );
		if ( m_isInverted )
		{
			surface()->DrawSetTextColor( c );
		}
		else
		{
			surface()->DrawSetTextColor( Color(255,255,255,255) );
		}
		surface()->DrawSetTextPos( xpos + text_xpos + iDeathIconWidth, ypos + ( item_tall / 2 ) - ( iFontHeight / 2 ) );

		int iTextSpace = item_wide - text_xpos;

		// write as much of the name as will fit, truncate the rest and add ellipses
		int iNameLength = wcslen(szconverted);
		const wchar_t *pszconverted = szconverted;
		int iTextWidthCounter = 0;
		for( int j=0;j<iNameLength;j++ )
		{
			iTextWidthCounter += surface()->GetCharacterWidth( m_NameFont, pszconverted[j] );

			if( iTextWidthCounter > iTextSpace )
			{	
				if( j > 3 )
				{
					szconverted[j-2] = '.';
					szconverted[j-1] = '.';
					szconverted[j] = '\0';
				}
				break;
			}
		}

		surface()->DrawPrintText( szconverted, wcslen(szconverted) );
			
		ypos -= ( item_spacing + item_tall );
	}
}
