
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"
#include "datacache/imdlcache.h"
#define PAIN_NAME "sprites/%d_pain.vmt"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>

#include <vgui/ILocalize.h>

using namespace vgui;

#include "asw_hudelement.h"
#include "hud_numericdisplay.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_door.h"
#include "asw_marine_profile.h"
#include "c_asw_marine_resource.h"
#include "asw_vgui_door_tooltip.h"
#include "ConVar.h"
#include "tier0/vprof.h"
#include "iasw_client_vehicle.h"
#include "engine/IVDebugOverlay.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "asw_input.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_door_healthbars("asw_door_healthbars", "2", FCVAR_NONE, "0=no health bars, 1=health bar at cursor, 2=show all door health bars");
extern ConVar asw_draw_hud;

#define MAX_DOOR_HEALTH_BARS 8

// This HUD element is responsible for creating door health panels
class CASWHudHealth : public CASW_HudElement, public CHudNumericDisplay
{
	DECLARE_CLASS_SIMPLE( CASWHudHealth, CHudNumericDisplay );

public:
	CASWHudHealth( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();		

	CASW_VGUI_Door_Tooltip *m_pDoorTooltip[MAX_DOOR_HEALTH_BARS];
};	

DECLARE_HUDELEMENT( CASWHudHealth );

CASWHudHealth::CASWHudHealth( const char *pElementName ) : CASW_HudElement( pElementName ), CHudNumericDisplay(NULL, "ASWHudHealth")
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
	for ( int i=0; i<MAX_DOOR_HEALTH_BARS; i++ )
	{
		m_pDoorTooltip[i] = NULL;
	}
}

void CASWHudHealth::Init()
{
	Reset();
}

void CASWHudHealth::Reset()
{
	SetPaintEnabled(false);
	SetPaintBackgroundEnabled(false);
	for (int i=0;i<MAX_DOOR_HEALTH_BARS;i++)
	{
		if ( m_pDoorTooltip[i] )
		{
			m_pDoorTooltip[i]->SetAlpha(0);
		}
	}
}

void CASWHudHealth::VidInit()
{
	Reset();
}

void CASWHudHealth::OnThink()
{
	if ( asw_door_healthbars.GetInt() == 0 )
		return;

	MDLCACHE_CRITICAL_SECTION();

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	if ( asw_door_healthbars.GetInt() == 1 )
	{
		if ( !m_pDoorTooltip[0] )
		{
			C_ASW_Door* pDoor = C_ASW_Door::GetDoorNear( ASWInput()->GetCrosshairAimingPos() );
			if (pDoor)	// check for creating the window to report a door's health if we mouse over it
			{						
				m_pDoorTooltip[0] = new CASW_VGUI_Door_Tooltip( GetParent(), "DoorTooltip");
				vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
				m_pDoorTooltip[0]->SetScheme(scheme);
				m_pDoorTooltip[0]->SetAlpha(0);
				m_pDoorTooltip[0]->m_bShowDoorUnderCursor = true;
			}
		}
		return;
	}

	// find all doors on-screen
	int iCurrentTooltip = 0;
	for ( int i=0; i<g_ClientDoorList.Count() && iCurrentTooltip < MAX_DOOR_HEALTH_BARS ;i++ )
	{
		C_ASW_Door *pDoor = g_ClientDoorList[i];
		int iDoorType = 0;
		if ( !pDoor || pDoor->GetHealthFraction(iDoorType) <= 0.0f|| pDoor->GetHealthFraction(iDoorType) >= 1.0f || pDoor->IsDormant() || pDoor->IsOpen() )
			continue;

		// assign a tooltip panel to show this door's health
		if ( m_pDoorTooltip[iCurrentTooltip] == NULL )
		{
			m_pDoorTooltip[iCurrentTooltip] = new CASW_VGUI_Door_Tooltip( GetParent(), "DoorTooltip");
			vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
			m_pDoorTooltip[iCurrentTooltip]->SetScheme(scheme);
			m_pDoorTooltip[iCurrentTooltip]->SetAlpha(0);
		}

		if ( !m_pDoorTooltip[iCurrentTooltip] )
			continue;

		int mx, my;
		if ( m_pDoorTooltip[iCurrentTooltip]->GetDoorHealthBarPosition( pDoor, mx, my ) )
		{
			m_pDoorTooltip[iCurrentTooltip]->SetDoor( pDoor );
			iCurrentTooltip++;
		}
	}
}