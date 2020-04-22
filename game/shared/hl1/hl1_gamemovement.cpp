//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "gamemovement.h"
#include "in_buttons.h"
#include <stdarg.h>
#include "movevars_shared.h"
#include "engine/IEngineTrace.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "decals.h"
#include "tier0/vprof.h"
#include "hl1_gamemovement.h"


// Expose our interface.
static CHL1GameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameMovement, IGameMovement, INTERFACENAME_GAMEMOVEMENT, g_GameMovement );

#ifdef CLIENT_DLL
#include "hl1/c_hl1mp_player.h"
#else
#include "hl1mp_player.h"
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL1GameMovement::CheckJumpButton( void )
{
	m_pHL1Player = ToHL1Player( player );

	Assert( m_pHL1Player );

	if (m_pHL1Player->pl.deadflag)
	{
		mv->m_nOldButtons |= IN_JUMP ;	// don't jump again until released
		return false;
	}

	// See if we are waterjumping.  If so, decrement count and return.
	if (m_pHL1Player->m_flWaterJumpTime)
	{
		m_pHL1Player->m_flWaterJumpTime -= gpGlobals->frametime;
		if (m_pHL1Player->m_flWaterJumpTime < 0)
			m_pHL1Player->m_flWaterJumpTime = 0;
		
		return false;
	}

	// If we are in the water most of the way...
	if ( m_pHL1Player->GetWaterLevel() >= 2 )
	{	
		// swimming, not jumping
		SetGroundEntity( NULL );

		if(m_pHL1Player->GetWaterType() == CONTENTS_WATER)    // We move up a certain amount
			mv->m_vecVelocity[2] = 100;
		else if (m_pHL1Player->GetWaterType() == CONTENTS_SLIME)
			mv->m_vecVelocity[2] = 80;
		
		// play swiming sound
		if ( m_pHL1Player->m_flSwimSoundTime <= 0 )
		{
			// Don't play sound again for 1 second
			m_pHL1Player->m_flSwimSoundTime = 1000;
			PlaySwimSound();
		}

		return false;
	}

	// No more effect
 	if (m_pHL1Player->GetGroundEntity() == NULL)
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;		// in air, so no effect
	}

	if ( mv->m_nOldButtons & IN_JUMP )
		return false;		// don't pogo stick

	// In the air now.
	SetGroundEntity( NULL );
	
	m_pHL1Player->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->GetSurfaceData(), 1.0, true );
	
	MoveHelper()->PlayerSetAnimation( PLAYER_JUMP );

	float flGroundFactor = 1.0f;
	if ( player->GetSurfaceData() )
	{
		flGroundFactor = 1.0;//player->GetSurfaceData()->game.jumpFactor; 
	}

	// Acclerate upward
	// If we are ducking...
	float startz = mv->m_vecVelocity[2];
	if ( (  m_pHL1Player->m_Local.m_bDucking ) || (  m_pHL1Player->GetFlags() & FL_DUCKING ) )
	{
		// d = 0.5 * g * t^2		- distance traveled with linear accel
		// t = sqrt(2.0 * 45 / g)	- how long to fall 45 units
		// v = g * t				- velocity at the end (just invert it to jump up that high)
		// v = g * sqrt(2.0 * 45 / g )
		// v^2 = g * g * 2.0 * 45 / g
		// v = sqrt( g * 2.0 * 45 )
		
		// Adjust for super long jump module
		// UNDONE -- note this should be based on forward angles, not current velocity.
		if ( m_pHL1Player->m_bHasLongJump &&
			( mv->m_nButtons & IN_DUCK ) &&
			( m_pHL1Player->m_Local.m_flDucktime > 0 ) &&
			mv->m_vecVelocity.Length() > 50 )
		{
			m_pHL1Player->m_Local.m_vecPunchAngle.Set( PITCH, -5 );

			mv->m_vecVelocity = m_vecForward * PLAYER_LONGJUMP_SPEED * 1.6;
			mv->m_vecVelocity.z = sqrt(2 * 800 * 56.0);
		}
		else
		{
			mv->m_vecVelocity[2] = flGroundFactor * sqrt(2 * 800 * 45.0);  // 2 * gravity * height
		}
	}
	else
	{
		mv->m_vecVelocity[2] += flGroundFactor * sqrt(2 * 800 * 45.0);  // 2 * gravity * height
	}
	FinishGravity();

	mv->m_outWishVel.z += mv->m_vecVelocity[2] - startz;
	mv->m_outStepHeight += 0.1f;
	
	if ( gpGlobals->maxClients > 1 )
#ifdef CLIENT_DLL
		(dynamic_cast<C_HL1MP_Player*>(m_pHL1Player))->DoAnimationEvent( PLAYERANIMEVENT_JUMP );
#else
		(dynamic_cast<CHL1MP_Player*>(m_pHL1Player))->DoAnimationEvent( PLAYERANIMEVENT_JUMP );
#endif

	// Flag that we jumped.
	mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: See if duck button is pressed and do the appropriate things
//-----------------------------------------------------------------------------
void CHL1GameMovement::Duck( void )
{
	int buttonsChanged	= ( mv->m_nOldButtons ^ mv->m_nButtons );	// These buttons have changed this frame
	int buttonsPressed	=  buttonsChanged & mv->m_nButtons;			// The changed ones still down are "pressed"
	int buttonsReleased	=  buttonsChanged & mv->m_nOldButtons;		// The changed ones which were previously down are "released"

	// Check to see if we are in the air.
	bool bInAir = ( player->GetGroundEntity() == NULL );
	bool bInDuck = ( player->GetFlags() & FL_DUCKING ) ? true : false;

	if ( mv->m_nButtons & IN_DUCK )
	{
		mv->m_nOldButtons |= IN_DUCK;
	}
	else
	{
		mv->m_nOldButtons &= ~IN_DUCK;
	}

	// Handle death.
	if ( IsDead() )
	{
		if ( bInDuck )
		{
			// Unduck
			FinishUnDuck();
		}
		return;
	}

	HandleDuckingSpeedCrop();

	// If the player is holding down the duck button, the player is in duck transition, ducking, or duck-jumping.
	if ( ( mv->m_nButtons & IN_DUCK ) || player->m_Local.m_bDucking  || bInDuck )
	{
		if ( ( mv->m_nButtons & IN_DUCK ) )
		{
			// Have the duck button pressed, but the player currently isn't in the duck position.
			if ( ( buttonsPressed & IN_DUCK ) && !bInDuck )
			{
				// Use 1 second so super long jump will work
				player->m_Local.m_flDucktime = GAMEMOVEMENT_DUCK_TIME;
				player->m_Local.m_bDucking    = true;
			}

			// The player is in duck transition and not duck-jumping.
			if ( player->m_Local.m_bDucking )
			{
				float flDuckMilliseconds = MAX( 0.0f, GAMEMOVEMENT_DUCK_TIME - ( float )player->m_Local.m_flDucktime );
				float flDuckSeconds = flDuckMilliseconds / GAMEMOVEMENT_DUCK_TIME;
				
				// Finish in duck transition when transition time is over, in "duck", in air.
				if ( ( flDuckSeconds > TIME_TO_DUCK ) || bInDuck || bInAir )
				{
					FinishDuck();
				}
				else
				{
					// Calc parametric time
					float flDuckFraction = SimpleSpline( flDuckSeconds / TIME_TO_DUCK );
					SetDuckedEyeOffset( flDuckFraction );
				}
			}
		}
		else
		{
			// Try to unduck unless automovement is not allowed
			// NOTE: When not onground, you can always unduck
			if ( player->m_Local.m_bAllowAutoMovement || bInAir )
			{
				if ( ( buttonsReleased & IN_DUCK ) && bInDuck )
				{
					// Use 1 second so super long jump will work
					player->m_Local.m_flDucktime = GAMEMOVEMENT_DUCK_TIME;
				}

				// Check to see if we are capable of unducking.
				if ( CanUnduck() )
				{
					// or unducking
  					if ( ( player->m_Local.m_bDucking || player->m_Local.m_bDucked ) )
					{
						float flDuckMilliseconds = MAX( 0.0f, GAMEMOVEMENT_DUCK_TIME - (float)player->m_Local.m_flDucktime );
						float flDuckSeconds = flDuckMilliseconds / GAMEMOVEMENT_DUCK_TIME;
						
						// Finish ducking immediately if duck time is over or not on ground
						if ( flDuckSeconds > TIME_TO_UNDUCK || ( bInAir ) )
						{
							FinishUnDuck();
						}
						else
						{
							// Calc parametric time
							float flDuckFraction = SimpleSpline( 1.0f - ( flDuckSeconds / TIME_TO_UNDUCK ) );
							SetDuckedEyeOffset( flDuckFraction );
							player->m_Local.m_bDucking = true;
						}
					}
				}
				else
				{
					// Still under something where we can't unduck, so make sure we reset this timer so
					//  that we'll unduck once we exit the tunnel, etc.
					player->m_Local.m_flDucktime = GAMEMOVEMENT_DUCK_TIME;
					SetDuckedEyeOffset( 1.0f );
				}
			}
		}
	}
}

void CHL1GameMovement::HandleDuckingSpeedCrop()
{
	if ( !( m_iSpeedCropped & SPEED_CROPPED_DUCK ) )
	{
		if ( player->GetFlags() & FL_DUCKING )
		{
			float frac = 0.33333333f;
			mv->m_flForwardMove	*= frac;
			mv->m_flSideMove	*= frac;
			mv->m_flUpMove		*= frac;
			m_iSpeedCropped		|= SPEED_CROPPED_DUCK;
		}
	}
}

void CHL1GameMovement::CheckParameters( void )
{
	if ( mv->m_nButtons & IN_SPEED )
	{
		mv->m_flClientMaxSpeed = 100;
	}
	else
	{
		mv->m_flClientMaxSpeed = mv->m_flMaxSpeed;
	}

	CHL1_Player* pHL1Player = dynamic_cast<CHL1_Player*>(player);
	if( pHL1Player && pHL1Player->IsPullingObject() )
	{
		mv->m_flClientMaxSpeed = mv->m_flMaxSpeed * 0.5f;
	}

	BaseClass::CheckParameters();
}
