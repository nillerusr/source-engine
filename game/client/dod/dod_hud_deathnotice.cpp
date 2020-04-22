//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Draws DoD:S's death notices
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "c_playerresource.h"
#include "iclientmode.h"
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <KeyValues.h>
#include "c_baseplayer.h"
#include "c_team.h"

#include "dod_shareddefs.h"
#include "clientmode_dod.h"
#include "c_dod_player.h"
#include "c_dod_playerresource.h"
#include "c_dod_objective_resource.h"
#include "dod_hud_freezepanel.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar hud_deathnotice_time( "hud_deathnotice_time", "6", 0 );
ConVar cl_deathicon_width( "cl_deathicon_width", "57" );
ConVar cl_deathicon_height( "cl_deathicon_height", "18" );

#define MAX_DEATHNOTICE_NAME_LENGTH		128	// to hold multiple player cappers

// a very useful function for getting the ideal scale factor of a sprite that's to be 
// scaled into a space
float GetScale( int nIconWidth, int nIconHeight, int nWidth, int nHeight );

// Player entries in a death notice
struct DeathNoticePlayer
{
	char		szName[MAX_DEATHNOTICE_NAME_LENGTH];
	int			iEntIndex;
};

// Contents of each entry in our list of death notices
struct DeathNoticeItem 
{
	DeathNoticeItem()
	{
		iconDeath = NULL;
		bSuicide = false;
		bCapMsg = false;
		bLocalPlayerInvolved = false;
		bDefense = false;
		bDominating = false;
	}

	DeathNoticePlayer	Killer;
	DeathNoticePlayer   Victim;
	CHudTexture *iconDeath;
	bool		bSuicide;
	float		flDisplayTime;

	// When I see a boolean like this, I know serious bullshit is afoot!
	bool		bCapMsg;	// if this is set, this is a flag cap msg.
							// Killer.szName is the list of players that capped
							// Victim.szName is the localized point name
							// iMaterial is the material index of the flag icon to show
							// iEntIndex in Killer is the capping team


	int			iMaterial;

	bool		bLocalPlayerInvolved;	// Is the local player a capper, killer or victim in this message

	bool		bDefense;

	bool		bDominating;
	wchar_t		wzInfoText[32];	// any additional text to display next to icon

};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudDeathNotice : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudDeathNotice, vgui::Panel );
public:
	CHudDeathNotice( const char *pElementName );

	void Init( void );
	void VidInit( void );
	virtual bool ShouldDraw( void );
	virtual void Paint( void );
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	void SetColorForNoticePlayer( int iTeamNumber );
	void RetireExpiredDeathNotices( void );
	
	void FireGameEvent( IGameEvent * event);

	void DrawBackgroundBox( int x, int y, int w, int h, bool bLocalPlayerInvolved );

	int DrawDefenseItem( DeathNoticeItem *pItem, int xRight, int y );
	int DrawDeathNoticeItem( DeathNoticeItem *pItem, int x, int y );
	int DrawDominationNoticeItem( DeathNoticeItem *pItem, int xRight, int y );

	virtual bool IsVisible( void );

	void AddAdditionalMsg( int iKillerID, int iVictimID, const char *pMsgKey );

	void PlayRivalrySounds( int iKillerIndex, int iVictimIndex, int iType );

private:

	CPanelAnimationVarAliasType( float, m_flLineHeight, "LineHeight", "15", "proportional_float" );

	CPanelAnimationVar( float, m_flMaxDeathNotices, "MaxDeathNotices", "4" );

	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "HudNumbersTimer" );

	CPanelAnimationVar( Color, m_BackgroundColor, "BackgroundColor", "255 255 255 100" );
	CPanelAnimationVar( Color, m_ActiveBackgroundColor, "ActiveBackgroundColor", "255 255 255 140" );

	// Special death notice icons
	CHudTexture		*m_iconD_skull;
	CHudTexture		*m_pIconDefended;
	CHudTexture		*m_iconDomination;

	CUtlVector<DeathNoticeItem> m_DeathNotices;

	int		m_iMaterialTexture;
};

using namespace vgui;

DECLARE_HUDELEMENT( CHudDeathNotice );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudDeathNotice::CHudDeathNotice( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "HudDeathNotice" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_iconD_skull = NULL;
	m_iconDomination = NULL;

	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDeathNotice::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDeathNotice::Init( void )
{
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "dod_point_captured" );
	ListenForGameEvent( "dod_capture_blocked" );

	m_iMaterialTexture = vgui::surface()->CreateNewTextureID();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDeathNotice::VidInit( void )
{
	m_iconD_skull = gHUD.GetIcon( "d_skull_dod" );
	m_pIconDefended = gHUD.GetIcon( "icon_defended" );
	m_iconDomination = gHUD.GetIcon( "leaderboard_dominated" );
	m_DeathNotices.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Draw if we've got at least one death notice in the queue
//-----------------------------------------------------------------------------
bool CHudDeathNotice::ShouldDraw( void )
{
	return ( CHudElement::ShouldDraw() && ( m_DeathNotices.Count() ) );
}

//-----------------------------------------------------------------------------
// Purpose: Hide if we just took a freezecam screenshot
//-----------------------------------------------------------------------------
bool CHudDeathNotice::IsVisible( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return BaseClass::IsVisible();

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDeathNotice::SetColorForNoticePlayer( int iTeamNumber )
{
	Color c = g_PR->GetTeamColor( iTeamNumber );
	surface()->DrawSetTextColor( c );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDeathNotice::Paint()
{
	int yStart = GetClientModeDODNormal()->GetDeathMessageStartHeight();

	surface()->DrawSetTextFont( m_hTextFont );

	int y = yStart;
	int x = GetWide();

	int iCount = m_DeathNotices.Count();
	for ( int i = 0; i < iCount; i++ )
	{
		if ( m_DeathNotices[i].bDefense )
			y += DrawDefenseItem( &m_DeathNotices[i], x, y );
		else
            y += DrawDeathNoticeItem( &m_DeathNotices[i], x, y );
	}

	// Now retire any death notices that have expired
	RetireExpiredDeathNotices();
}

int CHudDeathNotice::DrawDefenseItem( DeathNoticeItem *pItem, int xRight, int y )
{
	// Get the team numbers for the players involved
	int iKillerTeam = pItem->Killer.iEntIndex;
	int iVictimTeam = TEAM_UNASSIGNED;
	
	wchar_t victim[ 256 ];
	wchar_t killer[ 256 ];

	g_pVGuiLocalize->ConvertANSIToUnicode( pItem->Victim.szName, victim, sizeof( victim ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( pItem->Killer.szName, killer, sizeof( killer ) );

	// Get the local position for this notice
	int len = UTIL_ComputeStringWidth( m_hTextFont, victim );

	int iconWide;
	int iconTall;

	float scale = ( (float)ScreenHeight() / 480.0f ) * 0.6;	//scale based on 800x600
	iconWide = iconTall = (int)( scale * 16.0 );

	int iconDefSize = (int)( scale * 32.0 );
	
	int spacerX = XRES(5);

	int x = xRight - len - spacerX - iconWide - XRES(10);

	x -= iconDefSize;

	surface()->DrawSetTextFont( m_hTextFont );
	int iFontTall = vgui::surface()->GetFontTall( m_hTextFont );
	int yText = y + ( iconDefSize - iFontTall ) / 2;

	int boxWidth = len + iconWide + spacerX;

	boxWidth += iconDefSize;

	int boxHeight = m_flLineHeight;	
	int boxBorder = XRES(2);

	// Draw Defender's name
	int nameWidth = UTIL_ComputeStringWidth( m_hTextFont, killer ) + spacerX;	// gap

	x -= nameWidth;
	boxWidth += nameWidth;

	DrawBackgroundBox( x-boxBorder, y-boxBorder, boxWidth+2*boxBorder, boxHeight+2*boxBorder, pItem->bLocalPlayerInvolved );

	SetColorForNoticePlayer( iKillerTeam );

	// Draw killer's name			
	surface()->DrawSetTextPos( x, yText );
	surface()->DrawUnicodeString( killer );
	surface()->DrawGetTextPos( x, yText );

	x += spacerX;

	Color iconColor( 255, 80, 0, 255 );

	// Draw shield + cap icon
	m_pIconDefended->DrawSelf( x, y, iconDefSize, iconDefSize, Color(255,255,255,255) );
	x += iconDefSize + spacerX;

	const char *szMatName = GetMaterialNameFromIndex( pItem->iMaterial );

	vgui::surface()->DrawSetColor( Color(255,255,255,255) );
	vgui::surface()->DrawSetTextureFile( m_iMaterialTexture, szMatName, true, false);

	int iconY = y + iconDefSize / 2 - iconTall / 2;
	vgui::surface()->DrawTexturedRect( x, iconY, x + iconWide, iconY + iconTall );
	x += iconWide;

	SetColorForNoticePlayer( iVictimTeam );

	// Draw location name
	surface()->DrawSetTextFont( m_hTextFont );	//reset the font, draw icon can change it
	surface()->DrawSetTextPos( x, yText );

	surface()->DrawUnicodeString( victim );

	// return height of this item
	// base spacing on the height of the background box
	return boxHeight + boxBorder*2 + YRES(4);
}

// X is right side, do a right align!
int CHudDeathNotice::DrawDeathNoticeItem( DeathNoticeItem *pItem, int xRight, int y )
{
	if ( pItem->bDominating )
	{
		return DrawDominationNoticeItem( pItem, xRight, y );
	}

	bool bCapMsg = pItem->bCapMsg;

	// Get the team numbers for the players involved
	int iKillerTeam = TEAM_UNASSIGNED;
	int iVictimTeam = TEAM_UNASSIGNED;

	if ( bCapMsg )
	{
		iKillerTeam = pItem->Killer.iEntIndex;
		iVictimTeam = TEAM_UNASSIGNED;
	}
	else
	{
		if( g_PR )
		{
			iKillerTeam = g_PR->GetTeam( pItem->Killer.iEntIndex );
			iVictimTeam = g_PR->GetTeam( pItem->Victim.iEntIndex );
		}
	}		

	wchar_t victim[ 256 ];
	wchar_t killer[ 256 ];

	g_pVGuiLocalize->ConvertANSIToUnicode( pItem->Victim.szName, victim, sizeof( victim ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( pItem->Killer.szName, killer, sizeof( killer ) );

	// Get the local position for this notice
	int len = UTIL_ComputeStringWidth( m_hTextFont, victim );

	int iconWide;
	int iconTall;

	CHudTexture *icon = pItem->iconDeath;

	Assert( icon );

	if ( bCapMsg )
	{
		float scale = ( (float)ScreenHeight() / 480.0f ) * 0.6;	//scale based on 800x600
		iconWide = iconTall = (int)( scale * 32.0 );
	}
	else
	{
		if ( !icon )
			return 0;

		if( icon->bRenderUsingFont )
		{
			iconWide = surface()->GetCharacterWidth( icon->hFont, icon->cCharacterInFont );
			iconTall = surface()->GetFontTall( icon->hFont );
		}
		else
		{
			float scale = GetScale( icon->Width(), icon->Height(), XRES(cl_deathicon_width.GetInt()), YRES(cl_deathicon_height.GetInt()) );
			iconWide = (int)( scale * (float)icon->Width() );
			iconTall = (int)( scale * (float)icon->Height() );
		}
	}		

	int spacerX = XRES(5);

	int x = xRight - len - spacerX - iconWide - XRES(10);

	if ( pItem->bDefense )
	{
		x -= iconWide;		//m_iDefendedIconSize;
	}

	surface()->DrawSetTextFont( m_hTextFont );
	int iFontTall = vgui::surface()->GetFontTall( m_hTextFont );
	int boxWidth = len + iconWide + spacerX;

	if ( pItem->bDefense )
	{
		boxWidth += iconWide;	//m_iDefendedIconSize;
	}

	int boxHeight = m_flLineHeight;	//MIN( iconTall, m_flLineHeight );	
	int boxBorder = XRES(2);

	int yText = y + ( m_flLineHeight - iFontTall ) / 2;
	int yIcon = y + ( m_flLineHeight - iconTall ) / 2;

	// Only draw killers name if it wasn't a suicide
	if ( !pItem->bSuicide )
	{
		int nameWidth = UTIL_ComputeStringWidth( m_hTextFont, killer ) + spacerX;	// gap

		x -= nameWidth;
		boxWidth += nameWidth;

		DrawBackgroundBox( x-boxBorder, y-boxBorder, boxWidth+2*boxBorder, boxHeight+2*boxBorder, pItem->bLocalPlayerInvolved );

		SetColorForNoticePlayer( iKillerTeam );

		// Draw killer's name			
		surface()->DrawSetTextPos( x, yText );
		const wchar_t *p = killer;
		while ( *p )
		{
			surface()->DrawUnicodeChar( *p++ );
		}
		surface()->DrawGetTextPos( x, yText );

		x += spacerX;
	}
	else
	{
		DrawBackgroundBox( x-boxBorder, y-boxBorder, boxWidth+2*boxBorder, boxHeight+2*boxBorder, pItem->bLocalPlayerInvolved );
	}

	Color iconColor( 255, 80, 0, 255 );

	// Draw death weapon or cap icon
	if ( bCapMsg )
	{
		const char *szMatName = GetMaterialNameFromIndex( pItem->iMaterial );

		vgui::surface()->DrawSetColor( Color(255,255,255,255) );
		vgui::surface()->DrawSetTextureFile( m_iMaterialTexture, szMatName, true, false);
		vgui::surface()->DrawTexturedRect( x, yIcon, x + iconWide, yIcon + iconTall );
		x += iconWide + spacerX;
	}
	else
	{
		//If we're using a font char, this will ignore iconTall and iconWide
		icon->DrawSelf( x, yIcon, iconWide, iconTall, iconColor );
		x += iconWide + spacerX;
	}

	SetColorForNoticePlayer( iVictimTeam );

	// Draw victims name
	surface()->DrawSetTextFont( m_hTextFont );	//reset the font, draw icon can change it
	surface()->DrawSetTextPos( x, yText );
	const wchar_t *p = victim;
	while ( *p )
	{
		surface()->DrawUnicodeChar( *p++ );
	}

	// return height of this item
	// base spacing on the height of the background box
	return boxHeight + boxBorder*2 + YRES(4);
}

int CHudDeathNotice::DrawDominationNoticeItem( DeathNoticeItem *pItem, int xRight, int y )
{
	// Get the team numbers for the players involved
	int iKillerTeam = TEAM_UNASSIGNED;
	int iVictimTeam = TEAM_UNASSIGNED;

	if( g_PR )
	{
		iKillerTeam = g_PR->GetTeam( pItem->Killer.iEntIndex );
		iVictimTeam = g_PR->GetTeam( pItem->Victim.iEntIndex );
	}	

	wchar_t victim[ 256 ];
	wchar_t killer[ 256 ];

	g_pVGuiLocalize->ConvertANSIToUnicode( pItem->Victim.szName, victim, sizeof( victim ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( pItem->Killer.szName, killer, sizeof( killer ) );

	// Get the local position for this notice
	int len = UTIL_ComputeStringWidth( m_hTextFont, victim );

	int iconWide;
	int iconTall;

	Assert( pItem->iconDeath );

	CHudTexture *icon = pItem->iconDeath;

	if ( !icon )
		return 0;

	if( icon->bRenderUsingFont )
	{
		iconWide = surface()->GetCharacterWidth( icon->hFont, icon->cCharacterInFont );
		iconTall = surface()->GetFontTall( icon->hFont );
	}
	else
	{
		float scale = GetScale( icon->Width(), icon->Height(), XRES(cl_deathicon_width.GetInt()), YRES(cl_deathicon_height.GetInt()) );
		iconWide = (int)( scale * (float)icon->Width() );
		iconTall = (int)( scale * (float)icon->Height() );
	}	

	int spacerX = XRES(5);

	int x = xRight - len - spacerX - iconWide - XRES(10);

	surface()->DrawSetTextFont( m_hTextFont );
	int iFontTall = vgui::surface()->GetFontTall( m_hTextFont );
	int boxWidth = len + iconWide + spacerX;

	int iDominatingLen = UTIL_ComputeStringWidth( m_hTextFont, pItem->wzInfoText ) + XRES(2);
	x -= iDominatingLen;
	boxWidth += iDominatingLen;

	int boxHeight = m_flLineHeight;	//MIN( iconTall, m_flLineHeight );	
	int boxBorder = XRES(2);

	int yText = y + ( m_flLineHeight - iFontTall ) / 2;
	int yIcon = y + ( m_flLineHeight - iconTall ) / 2;

	int nameWidth = UTIL_ComputeStringWidth( m_hTextFont, killer ) + spacerX;	// gap

	x -= nameWidth;
	boxWidth += nameWidth;

	DrawBackgroundBox( x-boxBorder, y-boxBorder, boxWidth+2*boxBorder, boxHeight+2*boxBorder, pItem->bLocalPlayerInvolved );

	SetColorForNoticePlayer( iKillerTeam );

	// Draw killer's name			
	surface()->DrawSetTextPos( x, yText );
	const wchar_t *p = killer;
	while ( *p )
	{
		surface()->DrawUnicodeChar( *p++ );
	}
	surface()->DrawGetTextPos( x, yText );

	x += spacerX;

	Color iconColor( 255, 80, 0, 255 );

	//If we're using a font char, this will ignore iconTall and iconWide
	icon->DrawSelf( x, yIcon, iconWide, iconTall, iconColor );
	x += iconWide + spacerX;

	surface()->DrawSetTextColor( Color(255,255,255,255) );

	// Draw dominating string
	surface()->DrawSetTextFont( m_hTextFont );	//reset the font, draw icon can change it
	surface()->DrawSetTextPos( x, yText );
	p = pItem->wzInfoText;
	while ( *p )
	{
		surface()->DrawUnicodeChar( *p++ );
	}
	x += iDominatingLen;

	SetColorForNoticePlayer( iVictimTeam );

	// Draw victims name
	//surface()->DrawSetTextFont( m_hTextFont );	//reset the font, draw icon can change it
	surface()->DrawSetTextPos( x, yText );
	p = victim;
	while ( *p )
	{
		surface()->DrawUnicodeChar( *p++ );
	}

	// return height of this item
	// base spacing on the height of the background box
	return boxHeight + boxBorder*2 + YRES(4);
}

ConVar cl_deathicon_bg_alpha( "cl_deathicon_bg_alpha", "1.0" );

void CHudDeathNotice::DrawBackgroundBox( int x, int y, int w, int h, bool bLocalPlayerInvolved )
{
	Panel::DrawBox( x, y, w, h,
		bLocalPlayerInvolved ? m_ActiveBackgroundColor : m_BackgroundColor,
		cl_deathicon_bg_alpha.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: This message handler may be better off elsewhere
//-----------------------------------------------------------------------------
void CHudDeathNotice::RetireExpiredDeathNotices( void )
{
	// Loop backwards because we might remove one
	int iSize = m_DeathNotices.Size();
	for ( int i = iSize-1; i >= 0; i-- )
	{
		if ( m_DeathNotices[i].flDisplayTime < gpGlobals->curtime )
		{
			m_DeathNotices.Remove(i);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Server's told us that someone's died
//-----------------------------------------------------------------------------
void CHudDeathNotice::FireGameEvent( IGameEvent * event)
{
	if (!g_PR)
		return;

	if ( hud_deathnotice_time.GetFloat() == 0 )
		return;

	C_DODPlayer *pLocal = C_DODPlayer::GetLocalDODPlayer();

	Assert( pLocal );

	if ( !pLocal )
		return;

	int iLocalPlayerIndex = pLocal->entindex();

	const char *pEventName = event->GetName();

	if ( Q_strcmp( "dod_point_captured", pEventName ) == 0 )
	{
		// Cap point index
		int cp = event->GetInt( "cp", -1 );
		Assert( cp >= 0 );

		// Cap point name ( MATTTODO: can't we find this from the point index ? )
		const char *pName = event->GetString( "cpname", "Unnamed Control Point" );
		const wchar_t *pBuf = g_pVGuiLocalize->Find( pName );

		// Array of capper indeces
		const char *cappers = event->GetString("cappers");

		DeathNoticeItem capMsg;
		capMsg.bCapMsg = true;
		capMsg.bSuicide = false;
		capMsg.bDefense = false;
		capMsg.flDisplayTime = gpGlobals->curtime + hud_deathnotice_time.GetFloat();
		capMsg.bLocalPlayerInvolved	= false;

		char szCappers[256];
		szCappers[0] = '\0';
		
		int len = Q_strlen(cappers);
		for( int i=0;i<len;i++ )
		{
			int iPlayerIndex = (int)cappers[i];

			if ( iPlayerIndex == iLocalPlayerIndex )
				capMsg.bLocalPlayerInvolved = true;

			Assert( iPlayerIndex > 0 && iPlayerIndex <= gpGlobals->maxClients );

			const char *pPlayerName = g_PR->GetPlayerName( iPlayerIndex );

			if ( i == 0 )
			{
				// use first player as the team
				capMsg.Killer.iEntIndex = g_PR->GetTeam( iPlayerIndex );
				capMsg.iMaterial = g_pObjectiveResource->GetIconForTeam( cp, capMsg.Killer.iEntIndex );

				if ( g_pObjectiveResource->GetBombsRequired( cp ) > 0 )
				{
					capMsg.iMaterial = g_pObjectiveResource->GetCPBombedIcon( cp );
				}
			}
			else
			{
				Q_strncat( szCappers, ", ", sizeof(szCappers), 2 );
			}

			Q_strncat( szCappers, pPlayerName, sizeof(szCappers), COPY_ALL_CHARACTERS );
		}		

		Q_strncpy( capMsg.Killer.szName, szCappers, sizeof(capMsg.Killer.szName) );

		if ( pBuf )
		{
			g_pVGuiLocalize->ConvertUnicodeToANSI( pBuf, capMsg.Victim.szName, sizeof(capMsg.Victim.szName) );
		}
		else
		{
			Q_strncpy( capMsg.Victim.szName, pName, sizeof(capMsg.Victim.szName) );
		}

		// Do we have too many death messages in the queue?
		if ( m_DeathNotices.Count() > 0 &&
			m_DeathNotices.Count() >= (int)m_flMaxDeathNotices )
		{
			// Remove the oldest one in the queue, which will always be the first
			m_DeathNotices.Remove(0);
		}

		m_DeathNotices.AddToTail( capMsg );

		// print a log message

		char szLogMsg[512];

		Q_snprintf( szLogMsg, sizeof( szLogMsg ), "%s captured %s for the %s\n",
			capMsg.Killer.szName,
			capMsg.Victim.szName,
			capMsg.Killer.iEntIndex == TEAM_ALLIES ? "U.S. Army" : "Wermacht" );

		Msg( "%s",szLogMsg );
	}
	else if ( Q_strcmp( "dod_capture_blocked", pEventName ) == 0 )
	{
		// Cap point index
		int cp = event->GetInt( "cp", -1 );
		Assert( cp >= 0 );

		// Cap point name
		const char *pName = event->GetString( "cpname", "Unnamed Control Point" );
		const wchar_t *pBuf = g_pVGuiLocalize->Find( pName );

		// A single blocker entindex
		int iBlocker = event->GetInt("blocker");

		DeathNoticeItem capMsg;
		capMsg.bCapMsg = true;
		capMsg.bSuicide = false;
		capMsg.bDefense = true;
		capMsg.flDisplayTime = gpGlobals->curtime + hud_deathnotice_time.GetFloat();
		capMsg.bLocalPlayerInvolved	= false;

		capMsg.Killer.iEntIndex = g_PR->GetTeam( iBlocker );
		capMsg.iMaterial = g_pObjectiveResource->GetIconForTeam( cp, capMsg.Killer.iEntIndex );

		if ( iBlocker == iLocalPlayerIndex )
			capMsg.bLocalPlayerInvolved = true;

		Q_strncpy( capMsg.Killer.szName, g_PR->GetPlayerName( iBlocker ), sizeof(capMsg.Killer.szName) );	

		char buf[128];

		if ( pBuf )
		{
			g_pVGuiLocalize->ConvertUnicodeToANSI( pBuf, buf, sizeof(buf) );
			pName = buf;
		}

		Q_snprintf( capMsg.Victim.szName, sizeof(capMsg.Victim.szName), " - %s", pName );

		// Do we have too many death messages in the queue?
		if ( m_DeathNotices.Count() > 0 &&
			m_DeathNotices.Count() >= (int)m_flMaxDeathNotices )
		{
			// Remove the oldest one in the queue, which will always be the first
			m_DeathNotices.Remove(0);
		}

		m_DeathNotices.AddToTail( capMsg );
	}
	else if ( Q_strcmp( "player_death", pEventName ) == 0 )
	{		
		int killer = engine->GetPlayerForUserID( event->GetInt("attacker") );
		int victim = engine->GetPlayerForUserID( event->GetInt("userid") );
		const char *killedwith = event->GetString( "weapon" );

		char fullkilledwith[128];
		if ( killedwith && *killedwith )
		{
			Q_snprintf( fullkilledwith, sizeof(fullkilledwith), "d_%s", killedwith );
		}
		else
		{
			fullkilledwith[0] = 0;
		}

		// Do we have too many death messages in the queue?
		if ( m_DeathNotices.Count() > 0 &&
			m_DeathNotices.Count() >= (int)m_flMaxDeathNotices )
		{
			// Remove the oldest one in the queue, which will always be the first
			m_DeathNotices.Remove(0);
		}

		// Get the names of the players
		const char *killer_name = g_PR->GetPlayerName( killer );
		const char *victim_name = g_PR->GetPlayerName( victim );

		if ( !killer_name )
			killer_name = "";
		if ( !victim_name )
			victim_name = "";

		// Make a new death notice
		DeathNoticeItem deathMsg;
		deathMsg.Killer.iEntIndex = killer;
		deathMsg.Victim.iEntIndex = victim;
		Q_strncpy( deathMsg.Killer.szName, killer_name, MAX_PLAYER_NAME_LENGTH );
		Q_strncpy( deathMsg.Victim.szName, victim_name, MAX_PLAYER_NAME_LENGTH );
		deathMsg.flDisplayTime = gpGlobals->curtime + hud_deathnotice_time.GetFloat();
		deathMsg.bSuicide = ( !killer || killer == victim );
		deathMsg.bCapMsg = false;
		deathMsg.bDefense = false;
		deathMsg.iMaterial = -1;
		deathMsg.bLocalPlayerInvolved = ( killer == iLocalPlayerIndex || victim == iLocalPlayerIndex );

		// Try and find the death identifier in the icon list
		deathMsg.iconDeath = gHUD.GetIcon( fullkilledwith );

		if ( !deathMsg.iconDeath )
		{
			// Can't find it, so use the default skull & crossbones icon
			deathMsg.iconDeath = m_iconD_skull;
		}

		// Add it to our list of death notices
		m_DeathNotices.AddToTail( deathMsg );

		if ( event->GetInt( "dominated" ) > 0 )
		{
			AddAdditionalMsg( killer, victim, "#Msg_Dominating" );
			PlayRivalrySounds( killer, victim, DOD_DEATHFLAG_DOMINATION );
		}
		if ( event->GetInt( "revenge" ) > 0 ) 
		{
			AddAdditionalMsg( killer, victim, "#Msg_Revenge" );
			PlayRivalrySounds( killer, victim, DOD_DEATHFLAG_REVENGE );
		}

		char sDeathMsg[512];

		// Record the death notice in the console
		if ( deathMsg.bSuicide )
		{
			if ( !strcmp( fullkilledwith, "d_worldspawn" ) )
			{
				Q_snprintf( sDeathMsg, sizeof( sDeathMsg ), "%s died.\n", deathMsg.Victim.szName );
			}
			else	//d_world
			{
				Q_snprintf( sDeathMsg, sizeof( sDeathMsg ), "%s suicided.\n", deathMsg.Victim.szName );
			}
		}
		else
		{
			Q_snprintf( sDeathMsg, sizeof( sDeathMsg ), "%s killed %s", deathMsg.Killer.szName, deathMsg.Victim.szName );

			if ( fullkilledwith && *fullkilledwith && (*fullkilledwith > 13 ) )
			{
				Q_strncat( sDeathMsg, VarArgs( " with %s.\n", fullkilledwith+2 ), sizeof( sDeathMsg ), COPY_ALL_CHARACTERS );
			}
		}

		Msg( "%s",sDeathMsg );
	}	
}

//-----------------------------------------------------------------------------
// Purpose: Adds an additional death message
//-----------------------------------------------------------------------------
void CHudDeathNotice::AddAdditionalMsg( int iKillerID, int iVictimID, const char *pMsgKey )
{
	int iMsg = m_DeathNotices.AddToTail();
	DeathNoticeItem &msg = m_DeathNotices[iMsg];

	msg.Killer.iEntIndex = iKillerID;
	msg.Victim.iEntIndex = iVictimID;
	Q_strncpy( msg.Killer.szName, g_PR->GetPlayerName( iKillerID ), ARRAYSIZE( msg.Killer.szName ) );
	Q_strncpy( msg.Victim.szName, g_PR->GetPlayerName( iVictimID ), ARRAYSIZE( msg.Victim.szName ) );
	msg.flDisplayTime = gpGlobals->curtime + hud_deathnotice_time.GetFloat();
	msg.bSuicide = false;
	msg.bCapMsg = false;
	msg.bDefense = false;
	msg.iMaterial = -1;

	msg.bDominating = true;
	const wchar_t *wzMsg =  g_pVGuiLocalize->Find( pMsgKey );
	if ( wzMsg )
	{
		V_wcsncpy( msg.wzInfoText, wzMsg, sizeof( msg.wzInfoText ) );
	}
	msg.iconDeath = m_iconDomination;

	int iLocalPlayerIndex = GetLocalPlayerIndex();
	if ( iLocalPlayerIndex == iVictimID || iLocalPlayerIndex == iKillerID )
	{
		msg.bLocalPlayerInvolved = true;
	}
}

ConVar dod_playrivalrysounds( "dod_playrivalrysounds", "1", FCVAR_ARCHIVE );

void CHudDeathNotice::PlayRivalrySounds( int iKillerIndex, int iVictimIndex, int iType )
{
	if ( dod_playrivalrysounds.GetBool() == false )
		return;

	int iLocalPlayerIndex = GetLocalPlayerIndex();

	//We're not involved in this kill
	if ( iKillerIndex != iLocalPlayerIndex && iVictimIndex != iLocalPlayerIndex )
		return;

	const char *pszSoundName = NULL;

	if ( iType == DOD_DEATHFLAG_DOMINATION )
	{
		if ( iKillerIndex == iLocalPlayerIndex )
		{
			pszSoundName = "Game.Domination";
		}
		else if ( iVictimIndex == iLocalPlayerIndex )
		{
			pszSoundName = "Game.Nemesis";
		}
	}
	else if ( iType == DOD_DEATHFLAG_REVENGE )
	{
		pszSoundName = "Game.Revenge";
	}

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, pszSoundName );
}