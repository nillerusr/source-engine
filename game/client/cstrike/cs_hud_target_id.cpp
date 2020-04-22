//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HUD Target ID element
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "c_cs_player.h"
#include "c_playerresource.h"
#include "c_cs_playerresource.h"
#include "vgui_entitypanel.h"
#include "iclientmode.h"
#include "vgui/ILocalize.h"

#include "c_cs_hostage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PLAYER_HINT_DISTANCE	150
#define PLAYER_HINT_DISTANCE_SQ	(PLAYER_HINT_DISTANCE*PLAYER_HINT_DISTANCE)

extern CUtlVector< C_CHostage* > g_Hostages;

static ConVar hud_showtargetpos( "hud_showtargetpos", "0", FCVAR_ARCHIVE, "0: center, 1: upper left, 2 upper right, 3: lower left, 4: lower right" );
static ConVar hud_showtargetid( "hud_showtargetid", "1", FCVAR_ARCHIVE, "Enables display of target names" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTargetID : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CTargetID, vgui::Panel );

public:
	CTargetID( const char *pElementName );
	void Init( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint( void );
	void VidInit( void );

private:
	Color			GetColorForTargetTeam( int iTeamNumber );

	vgui::HFont		m_hFont;
	int				m_iLastEntIndex;
	float			m_flLastChangeTime;

	Color			m_cCTColor;
	Color			m_cTerroristColor;
	Color			m_cHostageColor;
};

DECLARE_HUDELEMENT( CTargetID );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTargetID::CTargetID( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "TargetID" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_hFont = g_hFontTrebuchet24;
	m_flLastChangeTime = 0;
	m_iLastEntIndex = 0;

	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: Setup
//-----------------------------------------------------------------------------
void CTargetID::Init( void )
{
};

void CTargetID::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_cTerroristColor = scheme->GetColor( "T_Red", Color( 255, 64, 64, 255 ) );
	m_cCTColor = scheme->GetColor( "CT_Blue", Color( 255, 64, 64, 255 ) );
	m_cHostageColor = scheme->GetColor( "Hostage_yellow", Color( 255, 160, 0, 255 ) );
	m_hFont = scheme->GetFont( "TargetID", true );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: clear out string etc between levels
//-----------------------------------------------------------------------------
void CTargetID::VidInit()
{
	CHudElement::VidInit();

	// set our size to the current viewport size
	SetSize(g_pClientMode->GetViewport()->GetWide(), g_pClientMode->GetViewport()->GetTall());

	m_flLastChangeTime = 0;
	m_iLastEntIndex = 0;
}

Color CTargetID::GetColorForTargetTeam( int iTeamNumber )
{
	switch( iTeamNumber )
	{
	case TEAM_CT:
		return m_cCTColor;
		break;

	case TEAM_TERRORIST:
		return m_cTerroristColor;
		break;

	default:
		return m_cHostageColor;
		break;
	}
} 

//-----------------------------------------------------------------------------
// Purpose: Draw function for the element
//-----------------------------------------------------------------------------
void CTargetID::Paint()
{
	if ( hud_showtargetid.GetBool() == false )
		return;

#define MAX_ID_STRING 256
	wchar_t sIDString[ MAX_ID_STRING ];
	sIDString[0] = 0;

	Color c = m_cHostageColor;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pPlayer )
		return;

	// don't show target IDs when flashed
	if ( pPlayer->m_flFlashBangTime > (gpGlobals->curtime+0.5) )
		return;

	//=============================================================================
	// HPE_BEGIN:
	// [menglish] Don't show target ID's when in freezecam mode
	//=============================================================================
	
	if ( pPlayer->GetObserverMode() == OBS_MODE_FREEZECAM )
		return;
	 
	//=============================================================================
	// HPE_END
	//=============================================================================

	// Get our target's ent index
	int iEntIndex = pPlayer->GetIDTarget();
	// Didn't find one?
	if ( !iEntIndex )
	{
		// Check to see if we should clear our ID
		if ( m_flLastChangeTime && (gpGlobals->curtime > (m_flLastChangeTime + 0.5)) )
		{
			m_flLastChangeTime = 0;
			sIDString[0] = 0;
			m_iLastEntIndex = 0;
		}
		else
		{
			// Keep re-using the old one
			iEntIndex = m_iLastEntIndex;
		}
	}
	else
	{
		m_flLastChangeTime = gpGlobals->curtime;
	}

	// Is this an entindex sent by the server?
	if ( iEntIndex )
	{
		C_BasePlayer *pPlayer = static_cast<C_BasePlayer*>(cl_entitylist->GetEnt( iEntIndex ));
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		const char *printFormatString = NULL;
		wchar_t wszClanTag[ MAX_PLAYER_NAME_LENGTH ];
		wchar_t wszPlayerName[ MAX_PLAYER_NAME_LENGTH ];
		wchar_t wszHealthText[ 10 ];
		bool bShowHealth = false;
		bool bShowPlayerName = false;

		// Some entities we always want to check, cause the text may change
		// even while we're looking at it
		// Is it a player?
		if ( IsPlayerIndex( iEntIndex ) )
		{
			if ( !pPlayer )
			{
				// This can happen because the object was destroyed
				sIDString[0] = 0;
				m_iLastEntIndex = 0;
			}
			else
			{
				c = GetColorForTargetTeam( pPlayer->GetTeamNumber() );

				bShowPlayerName = true;
 				g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(), wszPlayerName, sizeof(wszPlayerName) );

				C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );

				char szClan[MAX_PLAYER_NAME_LENGTH];
				if ( cs_PR && Q_strlen( cs_PR->GetClanTag( iEntIndex ) ) > 1 )
				{
					Q_snprintf( szClan, sizeof( szClan ), "%s ", cs_PR->GetClanTag( iEntIndex ) );
				}
				else
				{
					szClan[ 0 ] = 0;
				}
				g_pVGuiLocalize->ConvertANSIToUnicode( szClan, wszClanTag, sizeof( wszClanTag ) );
				
				if ( pPlayer->InSameTeam(pLocalPlayer) )
				{
					printFormatString = "#Cstrike_playerid_sameteam";
					bShowHealth = true;
				}
				else if ( pLocalPlayer->GetTeamNumber() != TEAM_CT && pLocalPlayer->GetTeamNumber() != TEAM_TERRORIST )
				{
					printFormatString = "#Cstrike_playerid_noteam";
					bShowHealth = true;
				}
				else
				{
					printFormatString = "#Cstrike_playerid_diffteam";
				}

				if ( bShowHealth )
				{
					_snwprintf( wszHealthText, ARRAYSIZE(wszHealthText) - 1, L"%.0f%%",  ((float)pPlayer->GetHealth() / (float)pPlayer->GetMaxHealth() ) * 100 );
					wszHealthText[ ARRAYSIZE(wszHealthText)-1 ] = '\0';
				}
			}
		}
		else
		{
			C_BaseEntity *pEnt = cl_entitylist->GetEnt( iEntIndex );

			//Hostages!

			//"Hostage : Health 100%"

			/*
			if( long range )
			{
				m_flDisplayHistory |= DHF_HOSTAGE_SEEN_FAR;
				switch ( pLocalPlayer->GetTeamNumber() )
				{
				case TERRORIST:
					HintMessage( "#Hint_prevent_hostage_rescue", TRUE );
					break;

				case CT:
					HintMessage( "#Hint_rescue_the_hostages", TRUE );
					break;
				}
			}
			else
			{
					m_flDisplayHistory |= DHF_HOSTAGE_SEEN_NEAR;
					m_flDisplayHistory |= DHF_HOSTAGE_SEEN_FAR;		// Don't want the other msg to appear now
					HintMessage( "#Hint_press_use_so_hostage_will_follow", FALSE );
			}
			*/

			C_CHostage *pHostage = NULL;

			for( int i=0;i<g_Hostages.Count();i++ )
			{
				// compare entity pointers			
				if( g_Hostages[i] == pEnt )
				{
					pHostage = g_Hostages[i];
					break;
				}
			}

			if( pHostage != NULL )
			{
				c = m_cHostageColor;
				printFormatString = "#Cstrike_playerid_hostage";
				_snwprintf( wszHealthText, ARRAYSIZE(wszHealthText) - 1, L"%.0f%%",  ((float)pHostage->GetHealth() / (float)pHostage->GetMaxHealth() ) * 100 );
				wszHealthText[ ARRAYSIZE(wszHealthText)-1 ] = '\0';
				bShowHealth = true;
			}
			else if ( !pEnt || !pEnt->InSameTeam(pLocalPlayer) )
			{
				// This can happen because the object was destroyed
				sIDString[0] = 0;
				m_iLastEntIndex = 0;
			}
			else
			{
				// Don't check validity if it's sent by the server
				c = m_cHostageColor;
				g_pVGuiLocalize->ConvertANSIToUnicode( pEnt->GetIDString(), sIDString, sizeof(sIDString) );
				m_iLastEntIndex = iEntIndex;
			}
		}

		if ( printFormatString )
		{
			if ( bShowPlayerName && bShowHealth )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 3, wszClanTag, wszPlayerName, wszHealthText );
			}
			else if ( bShowPlayerName )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 2, wszClanTag, wszPlayerName );
			}
			else if ( bShowHealth )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 1, wszHealthText );
			}
			else
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 0 );
			}
		}

		if ( sIDString[0] )
		{
			C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
			bool bObserverMode = pPlayer && pPlayer->IsObserver();

			int wide, tall;
			vgui::surface()->GetTextSize( m_hFont, sIDString, wide, tall );

			int ypos;
			int xpos;

			switch ( hud_showtargetpos.GetInt() )
			{
			case 0: // center
			default:
				xpos = (ScreenWidth() - wide) / 2;
				ypos = YRES(260) - tall / 2;
				break;
			case 1: // upper left
				xpos = XRES(10);
				ypos = bObserverMode ? YRES(55) : YRES(5);
				break;
			case 2: // upper right
				xpos = XRES(630) - wide;
				ypos = bObserverMode ? YRES(55) : YRES(5);
				break;
			case 3: // lower left
				xpos = XRES(10);
				ypos = bObserverMode ? YRES(415) : YRES(445) - tall;
				break;
			case 4: // lower right
				xpos = XRES(630) - wide;
				ypos = bObserverMode ? YRES(415) : YRES(410) - tall;
				break;
			}

			vgui::surface()->DrawSetTextFont( m_hFont );
			vgui::surface()->DrawSetTextPos( xpos, ypos );
			vgui::surface()->DrawSetTextColor( c );
			vgui::surface()->DrawPrintText( sIDString, wcslen(sIDString) );
		}
	}
}
