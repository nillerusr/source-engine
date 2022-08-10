//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: HUD Target ID element
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "c_tf_player.h"
#include "c_playerresource.h"
#include "iclientmode.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDisguiseStatus : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CDisguiseStatus, vgui::Panel );

public:
	CDisguiseStatus( const char *pElementName );
	void			Init( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint( void );

private:
	CPanelAnimationVar( vgui::HFont, m_hFont, "TextFont", "TargetID" );
};

DECLARE_HUDELEMENT( CDisguiseStatus );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDisguiseStatus::CDisguiseStatus( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "DisguiseStatus" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: Setup
//-----------------------------------------------------------------------------
void CDisguiseStatus::Init( void )
{
}

void CDisguiseStatus::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Draw function for the element
//-----------------------------------------------------------------------------
void CDisguiseStatus::Paint()
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pPlayer )
		return;

#ifdef _X360
	// We don't print anything on the xbox when we're fully disguised
	if ( !pPlayer->m_Shared.InCond( TF_COND_DISGUISING ) )
		return;
#endif


	int xpos = 0;
	int ypos = 0;

	#define MAX_DISGUISE_STATUS_LENGTH	128

	wchar_t szStatus[MAX_DISGUISE_STATUS_LENGTH];
	szStatus[0] = '\0';

	wchar_t *pszTemplate = NULL;
	int nDisguiseClass = TF_CLASS_UNDEFINED, nDisguiseTeam = TEAM_UNASSIGNED;
	if ( pPlayer->m_Shared.InCond( TF_COND_DISGUISING ) )
	{
		pszTemplate = g_pVGuiLocalize->Find( "#TF_Spy_Disguising" );
		nDisguiseClass = pPlayer->m_Shared.GetDesiredDisguiseClass();
		nDisguiseTeam = pPlayer->m_Shared.GetDesiredDisguiseTeam();
	}
	else if ( pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		pszTemplate = g_pVGuiLocalize->Find( "#TF_Spy_Disguised_as" );
		nDisguiseClass = pPlayer->m_Shared.GetDisguiseClass();
		nDisguiseTeam = pPlayer->m_Shared.GetDisguiseTeam();
	}

	wchar_t *pszClassName = g_pVGuiLocalize->Find( GetPlayerClassData( nDisguiseClass )->m_szLocalizableName );

	wchar_t *pszTeamName = NULL;
	if ( nDisguiseTeam == TF_TEAM_RED )
	{
		pszTeamName = g_pVGuiLocalize->Find("#TF_Spy_Disguise_Team_Red");
	}
	else
	{
		pszTeamName = g_pVGuiLocalize->Find("#TF_Spy_Disguise_Team_Blue");
	}

	if ( pszTemplate && pszClassName && pszTeamName )
	{
		wcsncpy( szStatus, pszTemplate, MAX_DISGUISE_STATUS_LENGTH );

		g_pVGuiLocalize->ConstructString( szStatus, MAX_DISGUISE_STATUS_LENGTH*sizeof(wchar_t), pszTemplate,
			2,
			pszTeamName,
			pszClassName );	
	}

	if ( szStatus[0] != '\0' )
	{
		vgui::surface()->DrawSetTextFont( m_hFont );

		Color cPlayerColor;

		if ( pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
		{
			cPlayerColor = g_PR->GetTeamColor( pPlayer->m_Shared.GetDisguiseTeam() );
		}
		else
		{
			cPlayerColor = g_PR->GetTeamColor( pPlayer->GetTeamNumber() );
		}

		// draw a black dropshadow ( the default one looks horrible )
		vgui::surface()->DrawSetTextPos( xpos+1, ypos+1 );
		vgui::surface()->DrawSetTextColor( Color(0,0,0,255) );
		vgui::surface()->DrawPrintText( szStatus, wcslen(szStatus) );		

		vgui::surface()->DrawSetTextPos( xpos, ypos );
		vgui::surface()->DrawSetTextColor( cPlayerColor );
		vgui::surface()->DrawPrintText( szStatus, wcslen(szStatus) );
	}
}