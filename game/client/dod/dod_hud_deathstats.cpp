
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dod_hud_deathstats.h"
#include "vgui_controls/AnimationController.h"
#include "iclientmode.h"
#include <vgui_controls/Label.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "hud_macros.h"
#include "c_dod_playerresource.h"
#include "c_dod_team.h"
#include "c_dod_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar cl_deathicon_width;
extern ConVar cl_deathicon_height;

//DECLARE_HUDELEMENT( CDODDeathStatsPanel );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODDeathStatsPanel::CDODDeathStatsPanel( const char *pElementName )
: EditablePanel( NULL, "DeathStats" ), CHudElement( pElementName )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	SetVisible( false );
	SetAlpha( 0 );
	SetScheme( "ClientScheme" );

	m_pAttackerHistoryLabel = new vgui::Label( this, "AttackerDmgLabel", "..." );
	m_pSummaryLabel = new vgui::Label( this, "LifeSummaryLabel", "..." );

	memset( &m_DeathRecord, 0, sizeof(m_DeathRecord) );

	LoadControlSettings("Resource/UI/DeathStats.res");	
}

void CDODDeathStatsPanel::OnScreenSizeChanged( int iOldWide, int iOldTall )
{
	LoadControlSettings( "resource/UI/DeathStats.res" );
}

void CDODDeathStatsPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBgColor( GetSchemeColor("TransparentLightBlack", pScheme) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODDeathStatsPanel::Reset()
{
	Hide();
}

void CDODDeathStatsPanel::Init()
{
	Hide();

	ListenForGameEvent( "player_death" );

	m_iMaterialTexture = vgui::surface()->CreateNewTextureID();

	CHudElement::Init();
}

void CDODDeathStatsPanel::VidInit( void )
{
	m_iconD_skull	= gHUD.GetIcon( "d_skull_dod" );
	m_pIconKill		= gHUD.GetIcon( "stats_kill" );
	m_pIconWounded	= gHUD.GetIcon( "stats_wounded" );
	m_pIconCap		= gHUD.GetIcon( "stats_cap" );
	m_pIconDefended	= gHUD.GetIcon( "stats_defended" );
}

//-----------------------------------------------------------------------------
// Purpose: Handle player_death message
//-----------------------------------------------------------------------------
void CDODDeathStatsPanel::FireGameEvent( IGameEvent * event)
{
	if ( Q_strcmp( "player_death", event->GetName() ) == 0 )
	{	
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		if ( !pLocalPlayer )
			return;

		if ( !g_PR )
			return;

		int victim = engine->GetPlayerForUserID( event->GetInt("userid") );

		// only deal with deathnotices where we die
		if ( victim != pLocalPlayer->entindex() )
		{
			return;
		}

		int killer = engine->GetPlayerForUserID( event->GetInt("attacker") );
		
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

		// Get the names of the players
		const char *killer_name = g_PR->GetPlayerName( killer );
		const char *victim_name = g_PR->GetPlayerName( victim );

		if ( !killer_name )
			killer_name = "";
		if ( !victim_name )
			victim_name = "";

		m_DeathRecord.Killer.iEntIndex = killer;
		m_DeathRecord.Victim.iEntIndex = victim;
		Q_strncpy( m_DeathRecord.Killer.szName, killer_name, MAX_PLAYER_NAME_LENGTH );
		Q_strncpy( m_DeathRecord.Victim.szName, victim_name, MAX_PLAYER_NAME_LENGTH );
		m_DeathRecord.bSuicide = ( !killer || killer == victim );

		// Try and find the death identifier in the icon list
		m_DeathRecord.iconDeath = gHUD.GetIcon( fullkilledwith );

		if ( !m_DeathRecord.iconDeath )
		{
			// Can't find it, so use the default skull & crossbones icon
			m_DeathRecord.iconDeath = m_iconD_skull;
		}

		// Info we get from this message:
		// - who killed us with what

		// Info that is networked to the local player
		// - the hitgroups we hit the guy who killed us with
		// - life kills
		// - life woundings
		// - life caps
		// - life defenses

		Show();
	}	
}

const char *szHitgroupNames[] = 
{
	"head",
	"chest",
	"arm",
	"leg"
};

void CDODDeathStatsPanel::Paint( void )
{
	int deathNoticeWidth = 0;

	if ( m_DeathRecord.Victim.iEntIndex > 0 )
		deathNoticeWidth = DrawDeathNoticeItem( m_iDeathNoticeX, m_iDeathNoticeY );

	const int minWidth = XRES(160);

	int panelWidth = ( deathNoticeWidth < minWidth ? minWidth : deathNoticeWidth );

	int wide, tall;

	// set width of summary label to death notice width
	m_pSummaryLabel->GetSize( wide, tall );
	m_pSummaryLabel->SetSize( panelWidth, tall );

	C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( !pLocalPlayer )
		return;

	char buf[512];
	buf[0] = '\0';

	if ( !m_DeathRecord.bSuicide )
	{
		bool bStart = true;
		bool bHit = false;

		for ( int i=0;i<4;i++ )
		{
			if ( pLocalPlayer->m_iHitsOnAttacker[i] > 0 )
			{
				bHit = true;
				if ( bStart )
				{
					Q_snprintf( buf, sizeof(buf), "You hit %s %i %s in the %s\n",
						m_DeathRecord.Killer.szName,
						pLocalPlayer->m_iHitsOnAttacker[i],
						pLocalPlayer->m_iHitsOnAttacker[i] == 1 ? "time" : "times",
						szHitgroupNames[i] );

					bStart = false;
				}
				else
				{
					Q_snprintf( buf, sizeof(buf), "%s and %i %s in the %s\n",
						buf,
						pLocalPlayer->m_iHitsOnAttacker[i],
						pLocalPlayer->m_iHitsOnAttacker[i] == 1 ? "time" : "times",
						szHitgroupNames[i] );
				}
			}
		}
	}	

	Q_strncat( buf, "\n", sizeof(buf), COPY_ALL_CHARACTERS );

	if ( pLocalPlayer->m_iPerLifeKills )
	{
		Q_snprintf( buf, sizeof(buf), "%sKills: %i\n\n",
			buf,
			pLocalPlayer->m_iPerLifeKills );
	}

	if ( pLocalPlayer->m_iPerLifeWounded )
	{
		Q_snprintf( buf, sizeof(buf), "%sWounded: %i\n\n",
			buf,
			pLocalPlayer->m_iPerLifeWounded );
	}

	if ( pLocalPlayer->m_iPerLifeCaptures )
	{
		Q_snprintf( buf, sizeof(buf), "%sFlag Captures: %i\n\n",
			buf,
			pLocalPlayer->m_iPerLifeCaptures );
	}

	if ( pLocalPlayer->m_iPerLifeDefenses )
	{
		Q_snprintf( buf, sizeof(buf), "%sFlag Defenses: %i\n\n",
			buf,
			pLocalPlayer->m_iPerLifeDefenses );
	}

	m_pAttackerHistoryLabel->SetText( buf );
	m_pAttackerHistoryLabel->SizeToContents();

	int panel_h = YRES(160);

	SetSize( panelWidth, panel_h );
}

float GetScale( int nIconWidth, int nIconHeight, int nWidth, int nHeight );

int CDODDeathStatsPanel::DrawDeathNoticeItem( int x, int y )
{
	// Get the team numbers for the players involved
	int iKillerTeam = TEAM_UNASSIGNED;
	int iVictimTeam = TEAM_UNASSIGNED;

	if( g_PR )
	{
		iKillerTeam = g_PR->GetTeam( m_DeathRecord.Killer.iEntIndex );
		iVictimTeam = g_PR->GetTeam( m_DeathRecord.Victim.iEntIndex );
	}	

	wchar_t victim[ 256 ];
	wchar_t killer[ 256 ];

	g_pVGuiLocalize->ConvertANSIToUnicode( m_DeathRecord.Victim.szName, victim, sizeof( victim ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( m_DeathRecord.Killer.szName, killer, sizeof( killer ) );

	// Get the local position for this notice
	int len = UTIL_ComputeStringWidth( m_hTextFont, victim );

	int iconWide;
	int iconTall;

	CHudTexture *icon = m_DeathRecord.iconDeath;

	Assert( icon );

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

	//int x = xRight - len - spacerX - iconWide - XRES(10);

	surface()->DrawSetTextFont( m_hTextFont );
	int iFontTall = vgui::surface()->GetFontTall( m_hTextFont );
	int yText = y + ( iconTall - iFontTall ) / 2;

	int boxWidth = len + iconWide + spacerX;
	int boxHeight = MIN( iconTall, m_flLineHeight );	
	int boxBorder = XRES(2);

	// Only draw killers name if it wasn't a suicide
	if ( !m_DeathRecord.bSuicide )
	{
		int nameWidth = UTIL_ComputeStringWidth( m_hTextFont, killer ) + spacerX;	// gap

		//x -= nameWidth;
		boxWidth += nameWidth;

		Panel::DrawBox( x-boxBorder,
			y-boxBorder,
			boxWidth+2*boxBorder,
			boxHeight+2*boxBorder,
			m_ActiveBackgroundColor,
			1.0 );

		Color c = g_PR->GetTeamColor( iKillerTeam );
		surface()->DrawSetTextColor( c );

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
		Panel::DrawBox( x-boxBorder,
			y-boxBorder,
			boxWidth+2*boxBorder,
			boxHeight+2*boxBorder,
			m_ActiveBackgroundColor,
			1.0 );
	}

	Color iconColor( 255, 80, 0, 255 );

	// Draw death weapon
	//If we're using a font char, this will ignore iconTall and iconWide
	icon->DrawSelf( x, y, iconWide, iconTall, iconColor );
	x += iconWide + spacerX;

	Color c = g_PR->GetTeamColor( iVictimTeam );
	surface()->DrawSetTextColor( c );

	// Draw victims name
	surface()->DrawSetTextFont( m_hTextFont );	//reset the font, draw icon can change it
	surface()->DrawSetTextPos( x, yText );
	const wchar_t *p = victim;
	while ( *p )
	{
		surface()->DrawUnicodeChar( *p++ );
	}

	return boxWidth;
}
