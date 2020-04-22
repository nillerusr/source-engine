//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Create and display a win panel at the end of a round displaying interesting stats and info about the round.
//
// $NoKeywords: $
//=============================================================================//
 
#include "cbase.h"
#include "stat_card.h"
#include "vgui_controls/AnimationController.h"
#include "iclientmode.h"
#include "c_playerresource.h"
#include <vgui_controls/Label.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "fmtstr.h"
#include "../../public/steam/steam_api.h"
#include "c_cs_player.h"
#include "cs_gamestats_shared.h"
#include "cs_client_gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
StatCard::StatCard(vgui::Panel *parent, const char *name) : BaseClass(parent, "CSStatCard")
{
	//m_pAvatarDefault = new ImagePanel(this, "");
	//m_pBackgroundArt = new ImagePanel(this, "BackgroundArt");
	m_pAvatar = new CAvatarImagePanel(this, "Avatar");
	m_pAvatar->SetShouldScaleImage(true);
	m_pAvatar->SetShouldDrawFriendIcon(false);
	m_pAvatar->SetSize(64,64);
	
	m_pName= new Label(this, "Name", "Name");
	m_pKillToDeathRatio = new Label(this, "KillToDeath", "KillToDeath");
	m_pStars = new Label(this, "Stars", "Stars");
}

StatCard::~StatCard()
{

}


void StatCard::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings("Resource/UI/CSStatCard.res");

	//m_pBackgroundArt->SetShouldScaleImage(true);
	//m_pBackgroundArt->SetImage("../VGUI/achievements/achievement-btn-up");
	
	SetBgColor(Color(0,0,0,0));

	UpdateInfo();
}

void StatCard::UpdateInfo()
{
	const StatsCollection_t& personalLifetimeStats = g_CSClientGameStats.GetLifetimeStats();

	int stars = personalLifetimeStats[CSSTAT_MVPS];
	float kills = personalLifetimeStats[CSSTAT_KILLS];
	float deaths = personalLifetimeStats[CSSTAT_DEATHS];
	wchar_t buf[64], numBuf[64];

	if (deaths > 0)
	{
		float killToDeath = kills / deaths;
		
		_snwprintf( numBuf, ARRAYSIZE( numBuf ), L"%.2f", killToDeath);
		g_pVGuiLocalize->ConstructString( buf, sizeof(buf),
			g_pVGuiLocalize->Find( "#GameUI_Stats_LastMatch_KDRatio" ), 1, numBuf );		
		m_pKillToDeathRatio->SetText( buf );
	}
	else
	{
		m_pKillToDeathRatio->SetText("");
	}

	_snwprintf( numBuf, ARRAYSIZE( numBuf ), L"%i", stars);
	g_pVGuiLocalize->ConstructString( buf, sizeof(buf),
		g_pVGuiLocalize->Find( "#GameUI_Stats_LastMatch_MVPS" ), 1, numBuf );		
	m_pStars->SetText( buf );

	if (steamapicontext)
	{
		ISteamFriends* friends = steamapicontext->SteamFriends();
		if (friends)
		{
			m_pName->SetText(friends->GetPersonaName());
		}
	}

	// Display the player avatar
	if (m_pAvatar && steamapicontext && steamapicontext->SteamUser())
	{
		m_pAvatar->SetPlayer( steamapicontext->SteamUser()->GetSteamID(), k_EAvatarSize64x64 );	
		m_pAvatar->SetVisible( true );
	}
}
