//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side implementation of CBaseCombatWeapon.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "history_resource.h"
#include "iclientmode.h"
#include "iinput.h"
#include "weapon_selection.h"
#include "hud_crosshair.h"
#include "engine/ivmodelinfo.h"
#include "tier0/vprof.h"
#include "hltvcamera.h"
#include "tier1/KeyValues.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::SetDormant( bool bDormant )
{
	// If I'm going from active to dormant and I'm carried by another player, holster me.
	if ( !IsDormant() && bDormant && !IsCarriedByLocalPlayer() )
	{
		Holster( NULL );
	}

	BaseClass::SetDormant( bDormant );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	BaseClass::NotifyShouldTransmit(state);

	if (state == SHOULDTRANSMIT_END)
	{
		if (m_iState == WEAPON_IS_ACTIVE)
		{
			m_iState = WEAPON_IS_CARRIED_BY_PLAYER;
		}
	}
	else if( state == SHOULDTRANSMIT_START )
	{
		if( m_iState == WEAPON_IS_CARRIED_BY_PLAYER )
		{
			if( GetOwner() && GetOwner()->GetActiveWeapon() == this )
			{
				// Restore the Activeness of the weapon if we client-twiddled it off in the first case above.
				m_iState = WEAPON_IS_ACTIVE;
			}
		}
	}
}



static inline bool ShouldDrawLocalPlayer( C_BasePlayer *pl )
{

	Assert( pl );
	return pl->ShouldDrawLocalPlayer();

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::OnRestore()
{
	BaseClass::OnRestore();

	// if the player is holding this weapon, 
	// mark it as just restored so it won't show as a new pickup
	if ( C_BasePlayer::IsLocalPlayer( GetOwner() ) )
	{
		m_bJustRestored = true;
	}
}

int C_BaseCombatWeapon::GetWorldModelIndex( void )
{
	return m_iWorldModelIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// If it's being carried by the *local* player, on the first update,
	// find the registered weapon for this ID

	C_BaseCombatCharacter *pOwner = GetOwner();
	C_BasePlayer *pPlayer = ToBasePlayer( pOwner );

	// check if weapon is carried by local player
	bool bIsLocalPlayer = C_BasePlayer::IsLocalPlayer( pPlayer );
	if ( bIsLocalPlayer )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( C_BasePlayer::GetSplitScreenSlotForPlayer( pPlayer ) );

		// If I was just picked up, or created & immediately carried, add myself to this client's list of weapons
		if ( ( m_iState != WEAPON_NOT_CARRIED ) && 
			 ( m_iOldState == WEAPON_NOT_CARRIED ) )
		{
			// Tell the HUD this weapon's been picked up
			if ( ShouldDrawPickup() )
			{
				CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
				if ( pHudSelection )
				{
					pHudSelection->OnWeaponPickup( this );
				}

				pPlayer->EmitSound( "Player.PickupWeapon" );
			}
		}
	}
	
	UpdateVisibility();

	m_iOldState = m_iState;

	m_bJustRestored = false;
}

//-----------------------------------------------------------------------------
// Is anyone carrying it?
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsBeingCarried() const
{
	return ( m_hOwner.Get() != NULL );
}

//-----------------------------------------------------------------------------
// Is the carrier alive?
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsCarrierAlive() const
{
	if ( !m_hOwner.Get() )
		return false;

	return m_hOwner.Get()->GetHealth() > 0;
}

//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t C_BaseCombatWeapon::ShadowCastType()
{
	if ( IsEffectActive( /*EF_NODRAW |*/ EF_NOSHADOW ) )
		return SHADOWS_NONE;

	if (!IsBeingCarried())
		return SHADOWS_RENDER_TO_TEXTURE;

	if (IsCarriedByLocalPlayer())
		return SHADOWS_NONE;

	return (m_iState != WEAPON_IS_CARRIED_BY_PLAYER) ? SHADOWS_RENDER_TO_TEXTURE : SHADOWS_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: This weapon is the active weapon, and it should now draw anything
//			it wants to. This gets called every frame.
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::Redraw()
{
	if ( GetClientMode()->ShouldDrawCrosshair() )
	{
		DrawCrosshair();
	}

	// ammo drawing has been moved into hud_ammo.cpp
}
//-----------------------------------------------------------------------------
// Purpose: Draw the weapon's crosshair
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::DrawCrosshair()
{
#ifndef INFESTED_DLL
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	Color clr = GetHud().m_clrNormal;
/*

	// TEST: if the thing under your crosshair is on a different team, light the crosshair with a different color.
	Vector vShootPos, vShootAngles;
	GetShootPosition( vShootPos, vShootAngles );

	Vector vForward;
	AngleVectors( vShootAngles, &vForward );
	
	
	// Change the color depending on if we're looking at a friend or an enemy.
	CPartitionFilterListMask filter( PARTITION_ALL_CLIENT_EDICTS );	
	trace_t tr;
	traceline->TraceLine( vShootPos, vShootPos + vForward * 10000, COLLISION_GROUP_NONE, MASK_SHOT, &tr, true, ~0, &filter );

	if ( tr.index != 0 && tr.index != INVALID_CLIENTENTITY_HANDLE )
	{
		C_BaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( tr.index );
		if ( pEnt )
		{
			if ( pEnt->GetTeamNumber() != player->GetTeamNumber() )
			{
				g = b = 0;
			}
		}
	}		 
*/

	CHudCrosshair *crosshair = GET_HUDELEMENT( CHudCrosshair );
	if ( !crosshair )
		return;

	// Find out if this weapon's auto-aimed onto a target
	bool bOnTarget = ( m_iState == WEAPON_IS_ACTIVE ) && player->m_fOnTarget;
	
	if ( player->GetFOV() >= 90 )
	{ 
		// normal crosshairs
		if ( bOnTarget && GetWpnData().iconAutoaim )
		{
			clr[3] = 255;

			crosshair->SetCrosshair( GetWpnData().iconAutoaim, clr );
		}
		else if ( GetWpnData().iconCrosshair )
		{
			clr[3] = 255;
			crosshair->SetCrosshair( GetWpnData().iconCrosshair, clr );
		}
		else
		{
			crosshair->ResetCrosshair();
		}
	}
	else
	{ 
		Color white( 255, 255, 255, 255 );

		// zoomed crosshairs
		if (bOnTarget && GetWpnData().iconZoomedAutoaim)
			crosshair->SetCrosshair(GetWpnData().iconZoomedAutoaim, white);
		else if ( GetWpnData().iconZoomedCrosshair )
			crosshair->SetCrosshair( GetWpnData().iconZoomedCrosshair, white );
		else
			crosshair->ResetCrosshair();
	}
	#endif
}
//-----------------------------------------------------------------------------
// Purpose: This weapon is the active weapon, and the viewmodel for it was just drawn.
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::ViewModelDrawn( C_BaseViewModel *pViewModel )
{
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this client's carrying this weapon
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsCarriedByLocalPlayer( void )
{
	if ( !GetOwner() )
		return false;

	return ( C_BasePlayer::IsLocalPlayer( GetOwner() ) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this weapon is the local client's currently wielded weapon
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsActiveByLocalPlayer( void )
{
	if ( IsCarriedByLocalPlayer() )
	{
		return (m_iState == WEAPON_IS_ACTIVE);
	}

	return false;
}

bool C_BaseCombatWeapon::GetShootPosition( Vector &vOrigin, QAngle &vAngles )
{
	// Get the entity because the weapon doesn't have the right angles.
	C_BaseCombatCharacter *pEnt = ToBaseCombatCharacter( GetOwner() );
	if ( pEnt )
	{
		if ( pEnt == C_BasePlayer::GetLocalPlayer() )
		{
			vAngles = pEnt->EyeAngles();
		}
		else
		{
			vAngles = pEnt->GetRenderAngles();	
		}
	}
	else
	{
		vAngles.Init();
	}

	C_BasePlayer *player = ToBasePlayer( pEnt );
	bool bUseViewModel = false;
	if ( C_BasePlayer::IsLocalPlayer( pEnt ) )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( pEnt );
		bUseViewModel = !player->ShouldDrawLocalPlayer();
	}

	QAngle vDummy;
	if ( IsActiveByLocalPlayer() && bUseViewModel )
	{
		C_BaseViewModel *vm = player ? player->GetViewModel( 0 ) : NULL;
		if ( vm )
		{
			int iAttachment = vm->LookupAttachment( "muzzle" );
			if ( vm->GetAttachment( iAttachment, vOrigin, vDummy ) )
			{
				return true;
			}
		}
	}
	else
	{
		// Thirdperson
		int iAttachment = LookupAttachment( "muzzle" );
		if ( GetAttachment( iAttachment, vOrigin, vDummy ) )
		{
			return true;
		}
	}

	vOrigin = GetRenderOrigin();
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::ShouldDraw( void )
{
	if ( m_iWorldModelIndex == 0 )
		return false;

	// FIXME: All weapons with owners are set to transmit in CBaseCombatWeapon::UpdateTransmitState,
	// even if they have EF_NODRAW set, so we have to check this here. Ideally they would never
	// transmit except for the weapons owned by the local player.
	if ( IsEffectActive( EF_NODRAW ) )
		return false;

	C_BaseCombatCharacter *pOwner = GetOwner();

	// weapon has no owner, always draw it
	if ( !pOwner )
		return true;

	bool bIsActive = ( m_iState == WEAPON_IS_ACTIVE );
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	 // carried by local player?
	if ( pOwner == pLocalPlayer )
	{
		// Only ever show the active weapon
		if ( !bIsActive )
			return false;

		// 3rd person mode
		if ( pLocalPlayer->ShouldDrawLocalPlayer() )
			return true;

		// don't draw active weapon if not in some kind of 3rd person mode, the viewmodel will do that
		return false;
	}

	// If it's a player, then only show active weapons
	if ( pOwner->IsPlayer() )
	{
		// Show it if it's active...
		return bIsActive;
	}

	// FIXME: We may want to only show active weapons on NPCs
	// These are carried by AIs; always show them
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if a weapon-pickup icon should be displayed when this weapon is received
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::ShouldDrawPickup( void )
{
	if ( GetWeaponFlags() & ITEM_FLAG_NOITEMPICKUP )
		return false;

	if ( m_bJustRestored )
		return false;

	return true;
}
		   

//----------------------------------------------------------------------------
// Hooks into the fast path render system
//----------------------------------------------------------------------------
IClientModelRenderable*	C_BaseCombatWeapon::GetClientModelRenderable()
{
	if ( !m_bReadyToDraw )
		return 0;

	// check if local player chases owner of this weapon in first person
	C_BasePlayer *localplayer = C_BasePlayer::GetLocalPlayer();
	if ( localplayer && localplayer->IsObserver() && GetOwner() )
	{
		// don't draw weapon if chasing this guy as spectator
		// we don't check that in ShouldDraw() since this may change
		// without notification 
		if ( localplayer->GetObserverMode() == OBS_MODE_IN_EYE &&
			localplayer->GetObserverTarget() == GetOwner() ) 
			return NULL;
	}

	if ( !BaseClass::GetClientModelRenderable() )
		return NULL;

	EnsureCorrectRenderingModel();
	return this;
}


//-----------------------------------------------------------------------------
// Purpose: Render the weapon. Draw the Viewmodel if the weapon's being carried
//			by this player, otherwise draw the worldmodel.
//-----------------------------------------------------------------------------
int C_BaseCombatWeapon::DrawModel( int flags, const RenderableInstance_t &instance )
{
	VPROF_BUDGET( "C_BaseCombatWeapon::DrawModel", VPROF_BUDGETGROUP_MODEL_RENDERING );
	if ( !m_bReadyToDraw )
		return 0;

	if ( !IsVisible() )
		return 0;

	// check if local player chases owner of this weapon in first person
	C_BasePlayer *localplayer = C_BasePlayer::GetLocalPlayer();

	if ( localplayer && localplayer->IsObserver() && GetOwner() )
	{
		// don't draw weapon if chasing this guy as spectator
		// we don't check that in ShouldDraw() since this may change
		// without notification 
		
		if ( localplayer->GetObserverMode() == OBS_MODE_IN_EYE &&
			 localplayer->GetObserverTarget() == GetOwner() ) 
			return false;
	}

	// See comment below
	EnsureCorrectRenderingModel();

	return BaseClass::DrawModel( flags, instance );
}

// If the local player is visible (thirdperson mode, tf2 taunts, etc., then make sure that we are using the 
//  w_ (world) model not the v_ (view) model or else the model can flicker, etc.
// Otherwise, if we're not the local player, always use the world model
void C_BaseCombatWeapon::EnsureCorrectRenderingModel()
{
	C_BasePlayer *localplayer = C_BasePlayer::GetLocalPlayer();
	if ( localplayer && 
		localplayer == GetOwner() &&
		!localplayer->ShouldDrawLocalPlayer() )
	{
		return;
	}

	// BRJ 10/14/02
	// FIXME: Remove when Yahn's client-side prediction is done
	// It's a hacky workaround for the model indices fighting
	// (GetRenderBounds uses the model index, which is for the view model)
	SetModelIndex( GetWorldModelIndex() );

	// Validate our current sequence just in case ( in theory the view and weapon models should have the same sequences for sequences that overlap at least )
	CStudioHdr *pStudioHdr = GetModelPtr();
	if ( pStudioHdr && 
		GetSequence() >= pStudioHdr->GetNumSeq() )
	{
		SetSequence( 0 );
	}
}

//-----------------------------------------------------------------------------
// tool recording
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::GetToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	int nModelIndex = GetModelIndex();
	int nWorldModelIndex = GetWorldModelIndex();
	if ( nModelIndex != nWorldModelIndex )
	{
		SetModelIndex( nWorldModelIndex );
	}

	BaseClass::GetToolRecordingState( msg );

	if ( m_iState == WEAPON_NOT_CARRIED )
	{
		BaseEntityRecordingState_t *pBaseEntity = (BaseEntityRecordingState_t*)msg->GetPtr( "baseentity" );
		pBaseEntity->m_nOwner = -1;
	}
	else
	{
		msg->SetInt( "worldmodel", 1 );
		if ( m_iState == WEAPON_IS_ACTIVE )
		{
			BaseEntityRecordingState_t *pBaseEntity = (BaseEntityRecordingState_t*)msg->GetPtr( "baseentity" );
			pBaseEntity->m_bVisible = true;
		}
	}

	if ( nModelIndex != nWorldModelIndex )
	{
		SetModelIndex( nModelIndex );
	}
}
