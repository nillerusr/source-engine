//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "asw_hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui/isurface.h>
#include <vgui/ILocalize.h>
#include "c_baseplayer.h"
#include "voice_status.h"
#include "clientmode_shared.h"
#include "c_playerresource.h"
#include "asw_hud_objective.h"
//#include "asw_hud_3dmarinenames.h"

ConVar *sv_alltalk = NULL;
extern ConVar asw_voice_side_icon;
extern ConVar asw_hud_alpha;

//=============================================================================
// Icon for the local player using voice
//=============================================================================
/*
class CASWHudVoiceSelfStatus : public CHudElement, public vgui::Panel
{
public:
DECLARE_CLASS_SIMPLE( CASWHudVoiceSelfStatus, vgui::Panel );

CASWHudVoiceSelfStatus( const char *name );

virtual bool ShouldDraw();	
virtual void Paint();
virtual void VidInit();
virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
CHudTexture *m_pVoiceIcon;

Color	m_clrIcon;
};


DECLARE_HUDELEMENT( CASWHudVoiceSelfStatus );

CASWHudVoiceSelfStatus::CASWHudVoiceSelfStatus( const char *pName ) :
vgui::Panel( NULL, "ASWHudVoiceSelfStatus" ), CHudElement( pName )
{
SetParent( GetClientMode()->GetViewport() );

m_pVoiceIcon = NULL;

SetHiddenBits( 0 );

m_clrIcon = Color(66,142,192,255);
}


void CASWHudVoiceSelfStatus::ApplySchemeSettings(vgui::IScheme *pScheme)
{
BaseClass::ApplySchemeSettings( pScheme );

SetBgColor( Color( 0, 0, 0, 0 ) );
}

void CASWHudVoiceSelfStatus::VidInit( void )
{
m_pVoiceIcon = HudIcons().GetIcon( "voice_player" );
}

bool CASWHudVoiceSelfStatus::ShouldDraw()
{
return GetClientVoiceMgr()->IsLocalPlayerSpeaking();
}

void CASWHudVoiceSelfStatus::Paint()
{
if( !m_pVoiceIcon )
return;

int x, y, w, h;
GetBounds( x, y, w, h );

m_pVoiceIcon->DrawSelf( 0, 0, w, h, m_clrIcon );
}
*/

//=============================================================================
// Icons for other players using voice
//=============================================================================
class CASWHudVoiceStatus : public CASW_HudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CASWHudVoiceStatus, vgui::Panel );

	CASWHudVoiceStatus( const char *name );

	virtual bool ShouldDraw();	
	virtual void Paint();
	virtual void VidInit();
	virtual void Init();
	virtual void OnThink();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	CHudTexture *m_pVoiceIcon;

	Color	m_clrIcon;

	CPlayerBitVec m_SpeakingList;

	CPanelAnimationVar( vgui::HFont, m_NameFont, "NameFont", "Default" );
	CPanelAnimationVar( vgui::HFont, m_NameGlowFont, "NameGlowFont", "DefaultBlur" );

	CPanelAnimationVarAliasType( float, item_tall, "item_tall", "32", "proportional_float" );
	CPanelAnimationVarAliasType( float, item_wide, "item_wide", "100", "proportional_float" );

	CPanelAnimationVarAliasType( float, item_spacing, "item_spacing", "2", "proportional_float" );

	CPanelAnimationVarAliasType( float, icon_ypos, "icon_ypos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_xpos, "icon_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_tall, "icon_tall", "32", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_wide, "icon_wide", "32", "proportional_float" );

	CPanelAnimationVarAliasType( float, text_ypos, "text_ypos", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, text_xpos, "text_xpos", "32", "proportional_float" );
};


DECLARE_HUDELEMENT( CASWHudVoiceStatus );


CASWHudVoiceStatus::CASWHudVoiceStatus( const char *pName ) :
vgui::Panel( NULL, "ASWHudVoiceStatus" ), CASW_HudElement( pName )
{
	SetParent( GetClientMode()->GetViewport() );

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);

	m_pVoiceIcon = NULL;

	SetHiddenBits( 0 );

	m_clrIcon = Color(255,255,255,255);
}

void CASWHudVoiceStatus::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBgColor( Color( 0, 0, 0, 0 ) );
}

void CASWHudVoiceStatus::Init( void )
{
	m_SpeakingList.ClearAll();
}

void CASWHudVoiceStatus::VidInit( void )
{
	m_pVoiceIcon = HudIcons().GetIcon( "voice_player" );
}

void CASWHudVoiceStatus::OnThink( void )
{
	if ( !asw_voice_side_icon.GetBool() )
		return;

	CVoiceStatus *pVoiceMgr = GetClientVoiceMgr();

	for ( int i=0;i<gpGlobals->maxClients;++i )
	{
		bool bTalking = pVoiceMgr->IsPlayerSpeaking(i + 1);

		m_SpeakingList.Set( i, bTalking );
	}
}

extern ConVar asw_draw_hud;
bool CASWHudVoiceStatus::ShouldDraw()
{
	return asw_draw_hud.GetBool();
}

void CASWHudVoiceStatus::Paint()
{
	if( !m_pVoiceIcon )
		return;

	int x, y, w, h;
	GetBounds( x, y, w, h );

	// Heights to draw the current voice item at
	int xpos = 0;
	int ypos = 0;

	bool bAnySpeakers = !m_SpeakingList.IsAllClear();

	int iFontHeight = 0;	

	if( bAnySpeakers )
	{
		surface()->DrawSetTextFont( m_NameFont );
		surface()->DrawSetTextColor( Color(255,255,255,255) );
		iFontHeight = surface()->GetFontTall( m_NameFont );
	}

	item_tall = iFontHeight;
	icon_tall = iFontHeight;
	icon_wide = iFontHeight;
	text_xpos = icon_xpos + icon_wide + 2;

	if ( !sv_alltalk )
		sv_alltalk = cvar->FindVar( "sv_alltalk" );

	//draw everyone in the list!
	for( int si = 0; si < MAX_PLAYERS; ++si )
	{
		if ( !m_SpeakingList.IsBitSet( si ) )
			continue;

		int playerIndex = si + 1;

		Color c = Color(255,255,255,128);
		Color glow = Color(35,214,250,255);

		const char *pName = g_PR ? g_PR->GetPlayerName(playerIndex) : "unknown";
		wchar_t szconverted[ 64 ];

		// Add the location, if any
		bool usedLocation = false;
		/*
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
		}*/

		if ( !usedLocation )
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pName, szconverted, sizeof(szconverted)  );
		}

		// Draw the item background		
		surface()->DrawSetColor( Color(0,0,0,asw_hud_alpha.GetInt()));
		surface()->DrawFilledRect( xpos, ypos, xpos + item_wide, ypos + item_tall );

		// Draw the voice icon
		surface()->DrawSetColor( c );
		m_pVoiceIcon->DrawSelf( xpos + icon_xpos, ypos + icon_ypos, icon_wide, icon_tall, m_clrIcon );

		// Draw the player's name
		surface()->DrawSetTextPos( xpos + text_xpos, ypos + ( item_tall / 2 ) - ( iFontHeight / 2 ) );

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

		surface()->DrawSetColor( glow );
		surface()->DrawSetTextFont( m_NameGlowFont );
		surface()->DrawPrintText( szconverted, wcslen(szconverted) );

		surface()->DrawSetTextPos( xpos + text_xpos, ypos + ( item_tall / 2 ) - ( iFontHeight / 2 ) );
		surface()->DrawSetColor( c );
		surface()->DrawSetTextFont( m_NameFont );
		surface()->DrawPrintText( szconverted, wcslen(szconverted) );

		ypos += ( item_spacing + item_tall );
	}
}
