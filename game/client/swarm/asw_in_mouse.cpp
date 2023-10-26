#include "cbase.h"
#include "asw_input.h"
#include "vgui/asw_vgui_ingame_panel.h"
#include "asw_hud_crosshair.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_weapon.h"
#include "c_asw_pickup.h"
#include "kbutton.h"
#include "cdll_int.h"
#include "vgui/isurface.h"
#include "iasw_client_aim_target.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_controls;		// asw: whether to use swarm mouse controls or not

//-----------------------------------------------------------------------------
// Purpose: make sure cursor isn't reset to 0 by the accumulation
//-----------------------------------------------------------------------------
void CASWInput::ActivateMouse (void)
{
	if ( m_fMouseInitialized )
	{
		// asw store mouse pos
		int current_posx, current_posy;
		GetMousePos(current_posx, current_posy);

		CInput::ActivateMouse();

		// asw - move it back to original position
		SetMousePos(current_posx, current_posy);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Don't allow recentering the mouse
//-----------------------------------------------------------------------------
void CASWInput::ResetMouse( void )
{
	int x, y;
	HACK_GETLOCALPLAYER_GUARD( "Mouse behavior is tied to a specific player's status - splitscreen player would depend on which player (if any) is using mouse control" );
	if (MarineControllingTurret() || !asw_controls.GetBool())
	{
		GetWindowCenter( x,  y );
		SetMousePos( x, y );
	}
	else
	{		
		GetMousePos( x, y );	// asw instead of GetWindowCenter, so mouse doesn't move
		SetMousePos( x, y );
	}
}

//-----------------------------------------------------------------------------
// Purpose: AccumulateMouse - asw: stop mouse from being moved back to the centre of the screen
//-----------------------------------------------------------------------------
void CASWInput::AccumulateMouse( int nSlot )
{
	// asw store mouse pos
	int current_posx, current_posy;
	GetMousePos(current_posx, current_posy);

	CInput::AccumulateMouse( nSlot );

	// asw - move it back to original position
	SetMousePos(current_posx, current_posy);
}

//-----------------------------------------------------------------------------
// Purpose: ApplyMouse -- applies mouse deltas to CUserCmd
// Input  : viewangles - 
//			*cmd - 
//			mouse_x - 
//			mouse_y - 
//-----------------------------------------------------------------------------
void CASWInput::ApplyMouse( int nSlot, QAngle& viewangles, CUserCmd *cmd, float mouse_x, float mouse_y )
{
	int current_posx, current_posy;	
	GetMousePos(current_posx, current_posy);


	if ( ASWInput()->ControllerModeActive() )
		return;
		
	if ( asw_controls.GetBool() && !MarineControllingTurret() )
	{
		TurnTowardMouse( viewangles, cmd );

		// Re-center the mouse.

		// force the mouse to the center, so there's room to move
		ResetMouse();
		SetMousePos( current_posx, current_posy );	// asw - swarm wants it unmoved (have to reset to stop buttons locking)
	}
	else
	{
		if ( MarineControllingTurret() )
		{					
			// accelerate up the mouse intertia
			static float mouse_x_accumulated = 0;
			static float mouse_y_accumulated = 0;

			// decay it
			mouse_x_accumulated *= 0.95f;
			mouse_y_accumulated *= 0.95f;

			mouse_x_accumulated += mouse_x * 0.04f;
			mouse_y_accumulated += mouse_y * 0.04f;

			// clamp it
			mouse_x_accumulated = clamp(mouse_x_accumulated, -500.0f,500.0f);
			mouse_y_accumulated = clamp(mouse_y_accumulated, -500.0f,500.0f);

			// move with our inertia style
			mouse_x = mouse_x_accumulated;
			mouse_y = mouse_y_accumulated;
		}
		CInput::ApplyMouse( nSlot, viewangles, cmd, mouse_x, mouse_y );

		// force the mouse to the center, so there's room to move
		ResetMouse();
	}
}

void CASWInput::GetFullscreenMousePos( int *mx, int *my, int *unclampedx /*=NULL*/, int *unclampedy /*=NULL*/ )
{
	Assert( mx );
	Assert( my );

	int x, y;
	GetWindowCenter( x,  y );

	int		current_posx, current_posy;

	GetMousePos(current_posx, current_posy);

	current_posx -= x;
	current_posy -= y;

	// Now need to add back in mid point of viewport
	int w, h;
	vgui::surface()->GetScreenSize( w, h );
	current_posx += w  / 2;
	current_posy += h / 2;

	if ( unclampedx )
	{
		*unclampedx = current_posx;
	}

	if ( unclampedy )
	{
		*unclampedy = current_posy;
	}

	// Clamp
	current_posx = MAX( 0, current_posx );
	current_posx = MIN( ScreenWidth(), current_posx );

	current_posy = MAX( 0, current_posy );
	current_posy = MIN( ScreenHeight(), current_posy );

	*mx = current_posx;
	*my = current_posy;
}

void CASWInput::SetMouseOverEntity( C_BaseEntity* pEnt )
{	
	// highlight the next entity
	m_hMouseOverEntity = pEnt;

	//m_MouseOverGlowObject.SetEntity( pEnt );

	if ( !pEnt )
		return;

	C_ASW_Marine *pOtherMarine = C_ASW_Marine::AsMarine( pEnt );
	if ( pOtherMarine )
		return;

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();	
	C_ASW_Marine *pMarine = pPlayer ? pPlayer->GetMarine() : NULL;
	if ( !pMarine )
		return;

	IASW_Client_Aim_Target* pAimEnt = dynamic_cast<IASW_Client_Aim_Target*>( pEnt );
	if ( pAimEnt )
	{
		// check we have LOS to the target
		CTraceFilterLOS traceFilter( pMarine, COLLISION_GROUP_NONE );
		trace_t tr2;
		Vector vecWeaponPos = pMarine->GetRenderOrigin() + Vector( 0,0, ASW_MARINE_GUN_OFFSET_Z );
		UTIL_TraceLine( vecWeaponPos, pAimEnt->GetAimTargetRadiusPos( vecWeaponPos ), MASK_OPAQUE, &traceFilter, &tr2 );
		//C_BaseEntity *pEnt = pAimEnt->GetEntity();
		//bool bHasLOS = (!tr2.startsolid && (tr2.fraction >= 1.0 || tr2.m_pEnt == pEnt));
		// we can't shoot it, so skip it
// 		if ( bHasLOS )
// 		{
// 			m_MouseOverGlowObject.SetRenderFlags( true, true );
// 			m_MouseOverGlowObject.SetColor( Vector( 0.65f, 0.45f, 0.15f ) );
// 			m_MouseOverGlowObject.SetAlpha( 0.875f );
// 		}
// 		else
// 		{
// 			m_MouseOverGlowObject.SetRenderFlags( true, true );
// 			m_MouseOverGlowObject.SetColor( Vector( 0.4f, 0.35f, 0.3f ) );
// 			m_MouseOverGlowObject.SetAlpha( 0.8f );
// 		}

	}
}

void CASWInput::SetHighlightEntity( C_BaseEntity* pEnt, bool bGlow )
{	
	// if we're currently highlighting something, stop
	if ( m_hHighlightEntity.Get() )
	{
		C_BaseAnimating *pAnimating = dynamic_cast<C_BaseAnimating*>( m_hHighlightEntity.Get() );
		if (pAnimating)
		{
			// ASWTODO - put this back in when we have a material proxy that supports lighting a specific marine
			//pAnimating->SetHighlight(false);
		}
	}
	// highlight the next entity
	m_hHighlightEntity = pEnt;
	m_HighLightGlowObject.SetEntity( pEnt );

	if ( m_hHighlightEntity.Get() )
	{
		if ( bGlow )
		{
			m_HighLightGlowObject.SetColor( Vector( 0.6f, 0.6f, 0.8f ) );
			m_HighLightGlowObject.SetAlpha( 0.7f );
		}
		else
		{
			m_HighLightGlowObject.SetColor( Vector( 0.3f, 0.3f, 0.3f ) );
			m_HighLightGlowObject.SetAlpha( 0.5f );
		}
	}
}

C_BaseEntity* CASWInput::GetHighlightEntity() const
{
	return m_hHighlightEntity.Get();
}

void CASWInput::UpdateHighlightEntity()
{
	// if we're currently brightening any entity, stop
	SetHighlightEntity( NULL, false );
	// clear any additional cursor icons
	CASWHudCrosshair *pCrosshair = GET_HUDELEMENT( CASWHudCrosshair );
	if ( pCrosshair )
	{
		pCrosshair->SetShowGiveAmmo(false, -1);
		pCrosshair->SetShowGiveHealth( false );
	}

	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	C_ASW_Marine* pMarine = pPlayer->GetMarine();
	if ( !pMarine )
		return;

	// see if the marine and his weapons want to highlight the current entity, or something near the cursor
	pMarine->MouseOverEntity( GetMouseOverEntity(), GetCrosshairAimingPos() );
}

void CASWInput::SetUseGlowEntity( C_BaseEntity* pEnt )
{	
	// if we're currently highlighting something, stop
	if ( m_hUseGlowEntity.Get() )
	{
		C_BaseAnimating *pAnimating = dynamic_cast<C_BaseAnimating*>( m_hUseGlowEntity.Get() );
		if ( pAnimating )
		{
			// ASWTODO - put this back in when we have a material proxy that supports lighting a specific marine
			//pAnimating->SetHighlight(false);
		}
	}
	// highlight the next entity
	m_hUseGlowEntity = pEnt;
	bool bIsAllowed = true;
	if ( m_hUseGlowEntity.Get() )
	{
		C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if ( !pPlayer )
			return;

		C_ASW_Marine* pMarine = pPlayer->GetMarine();
		if ( !pMarine )
			return;

		C_ASW_Pickup *pPickup = dynamic_cast< C_ASW_Pickup * >( pEnt );
		if ( pPickup )
		{
			bIsAllowed = pPickup->AllowedToPickup( pMarine );
		}
		else
		{
			C_ASW_Weapon *pWeapon = dynamic_cast< C_ASW_Weapon * >( pEnt );
			if ( pWeapon )
			{
				bIsAllowed = pWeapon->AllowedToPickup( pMarine );
			}
		}
	}

	if ( bIsAllowed )
		m_UseGlowObject.SetEntity( pEnt );
	else
		m_UseGlowObject.SetEntity( NULL );
}
