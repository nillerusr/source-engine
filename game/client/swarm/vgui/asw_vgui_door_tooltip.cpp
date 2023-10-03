#include "cbase.h"
#include "asw_vgui_door_tooltip.h"
#include "vgui/ISurface.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui/IInput.h>
#include "c_asw_marine.h"
#include "c_asw_door.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "c_asw_player.h"
#include "asw_weapon_medical_satchel_shared.h"
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#include "c_asw_game_resource.h"
#include <vgui_controls/AnimationController.h>
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#include "iasw_client_vehicle.h"
#include "iinput.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "datacache/imdlcache.h"
#include "asw_input.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_HEALTH_REPORT_FADE_TIME 0.1f

CASW_VGUI_Door_Tooltip::CASW_VGUI_Door_Tooltip( vgui::Panel *pParent, const char *pElementName ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel()
{	
	m_fNextUpdateTime = 0;
	m_hQueuedDoor = NULL;
	m_fDoorHealthFraction = 1.0f;
	m_hDoor = NULL;
	m_bQueuedDoor = false;
	m_bDoingSlowFade = false;
	m_iOldDoorHealth = 0;
	m_iDoorType = 0;
	m_bShowDoorUnderCursor = false;
}

CASW_VGUI_Door_Tooltip::~CASW_VGUI_Door_Tooltip()
{
}

void CASW_VGUI_Door_Tooltip::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(true);
	SetBgColor( Color(16,16,16,192) );
	if ( GetDoor() )
	{
		SetAlpha(200);
	}
	else
	{
		SetAlpha(0);
	}
	
	SetMouseInputEnabled(false);
}

void CASW_VGUI_Door_Tooltip::OnThink()
{
	MDLCACHE_CRITICAL_SECTION();
	
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer && m_bShowDoorUnderCursor )
	{
		C_ASW_Door* pDoorUnderCursor = C_ASW_Door::GetDoorNear( ASWInput()->GetCrosshairAimingPos() );
		if (pDoorUnderCursor && pDoorUnderCursor->IsOpen())	// don't show open doors
			pDoorUnderCursor = NULL;
		SetDoor(pDoorUnderCursor);		
	}
	// fade in/out as the door changes
	if (m_bQueuedDoor)
	{
		if (GetAlpha() <= 0)
		{
			m_hDoor = m_hQueuedDoor;			
			m_hQueuedDoor = NULL;
			m_bQueuedDoor = false;
			m_bDoingSlowFade = false;
			m_fNextUpdateTime = gpGlobals->curtime;
			if (GetDoor())
				vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 200, 0, ASW_HEALTH_REPORT_FADE_TIME, vgui::AnimationController::INTERPOLATOR_LINEAR);
		}
		else if (GetAlpha() >= 200)
		{
			if (m_hQueuedDoor.Get() == NULL)
			{
				if (!m_bDoingSlowFade)
				{
					m_bDoingSlowFade = true;
					float delay = 0; // 1.5f
					vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, delay, ASW_HEALTH_REPORT_FADE_TIME * 3, vgui::AnimationController::INTERPOLATOR_LINEAR);
				}
			}
			else
			{
				vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, 0, ASW_HEALTH_REPORT_FADE_TIME, vgui::AnimationController::INTERPOLATOR_LINEAR);
				m_bDoingSlowFade = false;
			}
		}
		// if we're doing a slow fade out but the player has now mouse overed another door, do the fade quickly
		if (m_bDoingSlowFade && m_hQueuedDoor.Get() != NULL)
		{
			vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, 0, ASW_HEALTH_REPORT_FADE_TIME, vgui::AnimationController::INTERPOLATOR_LINEAR);
			m_bDoingSlowFade = false;
		}
	}
	// check for updating health
	C_ASW_Door* pDoor = GetDoor();
	if (gpGlobals->curtime >= m_fNextUpdateTime && pDoor)
	{
		// set the health label accordingly
		int health = pDoor->GetHealth();

		if (health != m_iOldDoorHealth)
		{
			m_iOldDoorHealth = health;
			m_fDoorHealthFraction = pDoor->GetHealthFraction(m_iDoorType);
		}
		m_fNextUpdateTime = gpGlobals->curtime + 0.05f;
	}

	// reposition	
	if (pDoor)
	{	
		int mx, my, iDoorType;	
		if ( pDoor->IsAlive() && pDoor->GetHealthFraction(iDoorType) < 1.0f && !pDoor->IsOpen() && !pDoor->IsDormant() && GetDoorHealthBarPosition( pDoor, mx, my ) )
		{
			mx = clamp(mx, 0, ScreenWidth() - GetWide());
			my = clamp(my, 0, ScreenHeight() - GetTall());

			SetPos(mx, my);
		}
		else
		{
			SetDoor( NULL );
			return;
		}
	}
	
}

void CASW_VGUI_Door_Tooltip::Paint()
{	
	BaseClass::Paint();

	if (GetAlpha() > 0)
	{
		int padding = (2.0f / 1024.0f) * ScreenWidth();
		if (padding < 1)
			padding = 1;

		int bar_width = (96.0f / 1024.0f) * ScreenWidth();
		int bar_height = (12.0f / 1024.0f) * ScreenWidth();
		DrawHorizontalBar( padding, padding, bar_width, bar_height, m_fDoorHealthFraction, 255, 255, 255, 255 );

		if (GetWide() < bar_width + padding + padding)
		{
			SetWide(bar_width + padding + padding - 1);
		}
		SetTall( bar_height + padding + padding - 1);
	}
}

bool CASW_VGUI_Door_Tooltip::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	return false;	// not interactive
}

void CASW_VGUI_Door_Tooltip::SetDoor(C_ASW_Door *pDoor)
{
	if (m_hDoor.Get() != pDoor && !(m_bQueuedDoor && pDoor == m_hQueuedDoor.Get()))
	{		
		m_hQueuedDoor = pDoor;
		m_bQueuedDoor = true;
	}
}

C_ASW_Door* CASW_VGUI_Door_Tooltip::GetDoor()
{
	return dynamic_cast<C_ASW_Door*>(m_hDoor.Get());
}

void CASW_VGUI_Door_Tooltip::DrawHorizontalBar(int x, int y, int width, int height, float fFraction, int r, int g, int b, int a)
{
	int iTexture = m_nNormalDoorHealthTexture;
	if (m_iDoorType == 1)
		iTexture = m_nReinforcedDoorHealthTexture;
	else if (m_iDoorType == 2)
		iTexture = m_nReinforcedDoorHealthTexture;
	
	if (iTexture == -1)
		return;
	vgui::surface()->DrawSetTexture(iTexture);
	vgui::surface()->DrawSetColor(Color(0,0,0,a));
	vgui::surface()->DrawTexturedRect(x, y, x + width, height);
	vgui::surface()->DrawSetColor(Color(r,g,b,a));

	vgui::Vertex_t apoints[4] = 
	{ 
	vgui::Vertex_t( Vector2D(x, y),									Vector2D(0,0) ), 
	vgui::Vertex_t( Vector2D(x + (fFraction * width), y),			Vector2D(fFraction,0) ), 
	vgui::Vertex_t( Vector2D(x + (fFraction * width), y + height),	Vector2D(fFraction,1) ), 
	vgui::Vertex_t( Vector2D(x, y + height),						Vector2D(0,1) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, apoints );
}

bool CASW_VGUI_Door_Tooltip::GetDoorHealthBarPosition( C_ASW_Door *pDoor, int &screen_x, int &screen_y )
{
	Vector vecDoorPos = pDoor->m_vecClosedPosition + (pDoor->WorldSpaceCenter() - pDoor->GetAbsOrigin());
	Vector vecScreenPos = vec3_origin;
	if (pDoor->CollisionProp())
		vecDoorPos.z += pDoor->CollisionProp()->OBBMaxs().z * 0.3f;

	debugoverlay->ScreenPosition( vecDoorPos , vecScreenPos );

	if ( vecScreenPos.x == 0 && vecScreenPos.y == 0 )
		return false;

	vecScreenPos.x -= (GetWide() * 0.5f);

	if ( vecScreenPos.x < 0 || vecScreenPos.y < 0
		|| vecScreenPos.x >= ScreenWidth()
		|| vecScreenPos.y >= ScreenHeight() )
		return false;

	screen_x = vecScreenPos.x;
	screen_y = vecScreenPos.y;

	return true;
}