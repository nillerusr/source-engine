//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cs_hud_freezepanel.h"
#include <vgui/IVGui.h>
#include "vgui_controls/AnimationController.h"
#include "iclientmode.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include <vgui_controls/Label.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "VGUI/bordered_panel.h"
#include "fmtstr.h"
#include "cs_gamerules.h"
#include "view.h"
#include "ivieweffects.h"
#include "viewrender.h"
#include "usermessages.h"
#include "hud_macros.h"
#include "c_baseanimating.h"
#include "backgroundpanel.h"	// rounded border support

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT_DEPTH( CCSFreezePanel, 1 );
// DECLARE_HUD_MESSAGE( CCSFreezePanel, Damage );
// DECLARE_HUD_MESSAGE( CCSFreezePanel, DroppedEquipment );

#define CALLOUT_WIDE		(XRES(100))
#define CALLOUT_TALL		(XRES(50))


ConVar cl_disablefreezecam(
	"cl_disablefreezecam",
	"0",
	FCVAR_ARCHIVE,
	"Turn on/off freezecam on client"
	);


Color LerpColors( Color cStart, Color cEnd, float flPercent )
{
	float r = (float)((float)(cStart.r()) + (float)(cEnd.r() - cStart.r()) * flPercent);
	float g = (float)((float)(cStart.g()) + (float)(cEnd.g() - cStart.g()) * flPercent);
	float b = (float)((float)(cStart.b()) + (float)(cEnd.b() - cStart.b()) * flPercent);
	float a = (float)((float)(cStart.a()) + (float)(cEnd.a() - cStart.a()) * flPercent);
	return Color( r, g, b, a );
}


//-----------------------------------------------------------------------------
// Purpose:  Clips the health image to the appropriate percentage
//-----------------------------------------------------------------------------
class HorizontalGauge : public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( HorizontalGauge, vgui::Panel );

	HorizontalGauge( Panel *parent, const char *name ) : 
		vgui::Panel( parent, name ), 
		m_fPercent(0.0f)
	{
	}

/*
	void ApplySettings(KeyValues *inResourceData)
	{
		BaseClass::ApplySettings(inResourceData);

		Color color0 = inResourceData->GetColor( "color0");
		Color color1 = inResourceData->GetColor( "color1");
	}
*/

	void PaintBackground()
	{
		int wide, tall;
		GetSize(wide, tall);

		surface()->DrawSetColor( Color(0, 0, 0, 128) );
		surface()->DrawFilledRect(0, 0, wide, tall);

		// do the border explicitly here
		surface()->DrawSetColor( Color(0,0,0,255));
		surface()->DrawOutlinedRect(0, 0, wide, tall);
	}

	virtual void Paint()
	{
		int wide, tall;
		GetSize(wide, tall);

		Color lowHealth(192, 32, 32, 255);
		Color highHealth(32, 255, 32, 255);

		surface()->DrawSetColor( LerpColors(lowHealth, highHealth, m_fPercent) );
		surface()->DrawFilledRect(1, 1, (int)((wide - 1) * m_fPercent), tall - 1);
	}
		
	void SetPercent( float fPercent ) { m_fPercent = fPercent; }

private:
	float m_fPercent;
};

DECLARE_BUILD_FACTORY( HorizontalGauge );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSFreezePanel::CCSFreezePanel( const char *pElementName ) :
	EditablePanel( NULL, "FreezePanel" ), 
	CHudElement( pElementName ),
	m_pBackgroundPanel(NULL),
	m_pKillerHealth(NULL),
	m_pAvatar(NULL),
	m_pDominationIcon(NULL)
{
	SetSize( 10, 10 ); // Quiet "parent not sized yet" spew
	SetParent(g_pClientMode->GetViewport());
	m_bShouldBeVisible = false;
	SetScheme( "ClientScheme" );
	RegisterForRenderGroup( "hide_for_scoreboard" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSFreezePanel::Reset()
{
	Hide();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSFreezePanel::Init()
{
	CHudElement::Init();

	// listen for events
	ListenForGameEvent( "show_freezepanel" );
	ListenForGameEvent( "hide_freezepanel" );
	ListenForGameEvent( "freezecam_started" );
	ListenForGameEvent( "player_death" );

	Hide();

	InitLayout();

}

void CCSFreezePanel::InitLayout()
{
	LoadControlSettings( "resource/UI/FreezePanel_Basic.res" );

	m_pBackgroundPanel = dynamic_cast<BorderedPanel*>( FindChildByName("FreezePanelBG"));
	m_pAvatar = dynamic_cast<CAvatarImagePanel*>( m_pBackgroundPanel->FindChildByName("AvatarImage"));
	m_pKillerHealth	= dynamic_cast<HorizontalGauge*>( m_pBackgroundPanel->FindChildByName("KillerHealth"));
	m_pDominationIcon = dynamic_cast<ImagePanel*>( m_pBackgroundPanel->FindChildByName("DominationIcon"));

	m_pAvatar->SetDefaultAvatar(scheme()->GetImage( CSTRIKE_DEFAULT_AVATAR, true ));
	m_pAvatar->SetShouldScaleImage(true);
	m_pAvatar->SetShouldDrawFriendIcon(false);
}

//-----------------------------------------------------------------------------
// Purpose: Applies scheme settings
//-----------------------------------------------------------------------------
void CCSFreezePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSFreezePanel::FireGameEvent( IGameEvent * event )
{
	const char *pEventName = event->GetName();

	if ( Q_strcmp( "player_death", pEventName ) == 0 )
	{
		// see if the local player died
		int iPlayerIndexVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
		int iPlayerIndexKiller = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		CCSPlayer* pKiller =  ToCSPlayer(ClientEntityList().GetBaseEntity(iPlayerIndexKiller));

		if ( pLocalPlayer && iPlayerIndexVictim == pLocalPlayer->entindex() )
		{
			// the local player is dead, see if this is a new nemesis or a revenge
			if ( event->GetInt( "dominated" ) > 0)
			{
				m_pDominationIcon->SetImage("../hud/freeze_nemesis");
				m_pDominationIcon->SetVisible(true);

				m_pBackgroundPanel->SetDialogVariable( "InfoLabel1", g_pVGuiLocalize->Find("#FreezePanel_NewNemesis1"));
				m_pBackgroundPanel->SetDialogVariable( "InfoLabel2", g_pVGuiLocalize->Find("#FreezePanel_NewNemesis2"));
			}
			// was the killer your pre-existing nemesis?
			else if ( pKiller != NULL && pKiller->IsPlayerDominated(iPlayerIndexVictim) )
			{
				m_pDominationIcon->SetImage("../hud/freeze_nemesis");
				m_pDominationIcon->SetVisible(true);

				m_pBackgroundPanel->SetDialogVariable( "InfoLabel1", g_pVGuiLocalize->Find("#FreezePanel_OldNemesis1"));
				m_pBackgroundPanel->SetDialogVariable( "InfoLabel2", g_pVGuiLocalize->Find("#FreezePanel_OldNemesis2"));
			}
			else if ( event->GetInt( "revenge" ) > 0 )
			{
				m_pDominationIcon->SetImage("../hud/freeze_revenge");
				m_pDominationIcon->SetVisible(true);

				m_pBackgroundPanel->SetDialogVariable( "InfoLabel1", g_pVGuiLocalize->Find("#FreezePanel_Revenge1"));
				m_pBackgroundPanel->SetDialogVariable( "InfoLabel2", g_pVGuiLocalize->Find("#FreezePanel_Revenge2"));
			}
			else
			{
  				m_pDominationIcon->SetVisible(false);
				m_pBackgroundPanel->SetDialogVariable( "InfoLabel1", g_pVGuiLocalize->Find("#FreezePanel_Killer1"));
				m_pBackgroundPanel->SetDialogVariable( "InfoLabel2", g_pVGuiLocalize->Find("#FreezePanel_Killer2"));
			}
		}
	}
	else if ( Q_strcmp( "hide_freezepanel", pEventName ) == 0 )
	{
		Hide();
	}
	else if ( Q_strcmp( "freezecam_started", pEventName ) == 0 )
	{
	}
	else if ( Q_strcmp( "show_freezepanel", pEventName ) == 0 )
	{
		C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
		if ( !cs_PR )
			return;

		Show();

		// Get the entity who killed us
		int iKillerIndex = event->GetInt( "killer" );
		CCSPlayer* pKiller =  ToCSPlayer(ClientEntityList().GetBaseEntity(iKillerIndex));
		m_pAvatar->ClearAvatar();

		if ( pKiller )
		{
			int iMaxHealth = pKiller->GetMaxHealth();
			int iKillerHealth = pKiller->GetHealth();
			if ( !pKiller->IsAlive() )
			{
				iKillerHealth = 0;
			}

			m_pKillerHealth->SetPercent( (float)iKillerHealth / iMaxHealth );

			char killerName[128];
			V_snprintf( killerName, sizeof(killerName), "%s", g_PR->GetPlayerName(iKillerIndex) );
//			V_strupr( killerName );

			m_pBackgroundPanel->SetDialogVariable( "killername", killerName);

			int iKillerIndex = pKiller->entindex();
			player_info_t pi;

			m_pAvatar->SetDefaultAvatar( GetDefaultAvatarImage( pKiller ) );

			if ( engine->GetPlayerInfo(iKillerIndex, &pi) )
			{
				m_pAvatar->SetPlayer( (C_BasePlayer*)pKiller, k_EAvatarSize64x64);
				m_pAvatar->SetVisible(true);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSFreezePanel::ShouldDraw( void )
{
	//=============================================================================
	// HPE_BEGIN:
	// [Forrest] Added sv_disablefreezecam check
	//=============================================================================
	static ConVarRef sv_disablefreezecam( "sv_disablefreezecam" );
	return ( m_bShouldBeVisible && !cl_disablefreezecam.GetBool() && !sv_disablefreezecam.GetBool() && CHudElement::ShouldDraw() );
	//=============================================================================
	// HPE_END
	//=============================================================================
}

void CCSFreezePanel::OnScreenSizeChanged( int nOldWide, int nOldTall )
{
	BaseClass::OnScreenSizeChanged(nOldWide, nOldTall);

	InitLayout();
}

void CCSFreezePanel::SetActive( bool bActive )
{
	CHudElement::SetActive( bActive );

	if ( bActive )
	{
		// Setup replay key binding in UI
		const char *pKey = engine->Key_LookupBinding( "save_replay" );
		if ( pKey == NULL || FStrEq( pKey, "(null)" ) )
		{
			pKey = "<NOT BOUND>";
		}

		char szKey[16];
		Q_snprintf( szKey, sizeof(szKey), "%s", pKey );
		wchar_t wKey[16];
		wchar_t wLabel[256];

		g_pVGuiLocalize->ConvertANSIToUnicode( szKey, wKey, sizeof( wKey ) );
		g_pVGuiLocalize->ConstructString( wLabel, sizeof( wLabel ), g_pVGuiLocalize->Find("#FreezePanel_SaveReplay" ), 1, wKey );

		m_pBackgroundPanel->SetDialogVariable( "savereplay", wLabel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSFreezePanel::Show()
{
	m_bShouldBeVisible = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSFreezePanel::Hide()
{
	m_bShouldBeVisible = false;
}
