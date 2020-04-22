//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HUD Target ID element
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "c_dod_player.h"
#include "c_playerresource.h"
#include "vgui_entitypanel.h"
#include "iclientmode.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PLAYER_HINT_DISTANCE	150
#define PLAYER_HINT_DISTANCE_SQ	(PLAYER_HINT_DISTANCE*PLAYER_HINT_DISTANCE)

static ConVar hud_centerid( "hud_centerid", "1", FCVAR_ARCHIVE );

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
	vgui::HFont		m_hFont;
	int				m_iLastEntIndex;
	float			m_flLastChangeTime;
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

	RegisterForRenderGroup( "winpanel" );
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

	m_hFont = scheme->GetFont( "TargetID", IsProportional() );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: clear out string etc between levels
//-----------------------------------------------------------------------------
void CTargetID::VidInit()
{
	CHudElement::VidInit();

	m_flLastChangeTime = 0;
	m_iLastEntIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Draw function for the element
//-----------------------------------------------------------------------------
void CTargetID::Paint()
{
#define MAX_ID_STRING 256
	wchar_t sIDString[ MAX_ID_STRING ];
	sIDString[0] = 0;

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( !pPlayer )
		return;

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
		C_DODPlayer *pLocalDODPlayer = C_DODPlayer::GetLocalDODPlayer();

		wchar_t wszPlayerName[ MAX_PLAYER_NAME_LENGTH ];
		wchar_t wszHealthText[ 10 ];

		// Some entities we always want to check, cause the text may change
		// even while we're looking at it
		if ( IsPlayerIndex( iEntIndex ) && pPlayer->InSameTeam(pLocalPlayer) )
		{
			// Construct the wide char name string
			g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(),  wszPlayerName, sizeof(wszPlayerName) );
			
			_snwprintf( wszHealthText, ARRAYSIZE(wszHealthText) - 1, L"%.0f", (float)pPlayer->GetHealth() );
			wszHealthText[ ARRAYSIZE(wszHealthText)-1 ] = '\0';

			// Construct the string to display
			g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), L"%s1 (%s2)", 2, wszPlayerName, wszHealthText );

			if ( sIDString[0] )
			{
				int wide, tall;
				int ypos = YRES(260);
				int xpos = XRES(10);

				vgui::surface()->GetTextSize( m_hFont, sIDString, wide, tall );

				if( hud_centerid.GetInt() == 0 )
				{
					ypos = YRES(420);
				}
				else
				{
					xpos = (ScreenWidth() - wide) / 2;
				}

				vgui::surface()->DrawSetTextFont( m_hFont );

				// draw a black dropshadow ( the default one looks horrible )
				vgui::surface()->DrawSetTextPos( xpos+1, ypos+1 );
				vgui::surface()->DrawSetTextColor( Color(0,0,0,255) );
				vgui::surface()->DrawPrintText( sIDString, wcslen(sIDString) );		

				vgui::surface()->DrawSetTextPos( xpos, ypos );
				vgui::surface()->DrawSetTextColor( g_PR->GetTeamColor( pPlayer->GetTeamNumber() ) );
				vgui::surface()->DrawPrintText( sIDString, wcslen(sIDString) );
			}

			pLocalDODPlayer->HintMessage( HINT_FRIEND_SEEN );
		}		
		else if( IsPlayerIndex( iEntIndex ) &&
			pPlayer->GetTeamNumber() != TEAM_SPECTATOR &&
			pPlayer->GetTeamNumber() != TEAM_UNASSIGNED &&
			pPlayer->IsAlive() )
		{
			// must not be in the same team, enemy
			pLocalDODPlayer->HintMessage( HINT_ENEMY_SEEN );
		}
	}
}
