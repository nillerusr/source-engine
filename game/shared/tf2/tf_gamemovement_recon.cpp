//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_gamemovement_recon.h"
#include "tf_movedata.h"
#include "in_buttons.h"
			   
#define TIME_WALL_SUPPRESSION_JUMP	    100
#define TIME_WALL_SUPPRESSION_IMPACT	400
#define TIME_WALL_STICK					 50
#define TIME_STRAFE_STICK				 50
#define TIME_LEAP_STICK					300
#define TIME_WALL_ACTIVATE_JUMP			300
#define TIME_WALL_INVALID				-99999
#define MAX_VERTICAL_SPEED				400


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGameMovementRecon::CTFGameMovementRecon()
{
	m_pReconData = NULL;

	m_vStandMins = RECONCLASS_HULL_STAND_MIN;
	m_vStandMaxs = RECONCLASS_HULL_STAND_MAX;
	m_vStandViewOffset = RECONCLASS_VIEWOFFSET_STAND;

	m_vDuckMins = RECONCLASS_HULL_DUCK_MIN;
	m_vDuckMaxs = RECONCLASS_HULL_DUCK_MAX;
	m_vDuckViewOffset = RECONCLASS_VIEWOFFSET_DUCK;

	m_bPerformingAirMove = false;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementRecon::ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMove )
{
	// Get the class specific data from the TFMoveData structure
	Assert( PlayerClassReconData_t::PLAYERCLASS_ID == pTFMove->m_nClassID );
	m_pReconData = &pTFMove->ReconData();

	// to test pass it through!!
	BaseClass::ProcessMovement( (CBasePlayer *)pPlayer, pTFMove );

	// Set the jump count appropriately...
	// If we've hit the ground, we can double jump again...
	if ((player->GetGroundEntity() != NULL) && (pTFMove->ReconData().m_flStickTime == TIME_WALL_INVALID))
	{
		m_pReconData->m_nJumpCount = 0;
		ResetWallImpact( (CTFMoveData*)mv );
	}
}

void CTFGameMovementRecon::PostPlayerMove( void )
{
	BaseClass::PostPlayerMove( );

	if (m_pReconData->m_flStickTime != TIME_WALL_INVALID)
	{
		// We're stuck, so stick!
		mv->m_vecVelocity.Init( 0.0f, 0.0f, 0.0f );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementRecon::UpdateTimers( void )
{
	BaseClass::UpdateTimers();

	CTFMoveData *pTFMove = TFMove();
	if ( !pTFMove )
		return;

	float frame_msec = 1000.0f * gpGlobals->frametime;

	// Decrement the recon timers.
	if ( pTFMove->ReconData().m_flSuppressionJumpTime != TIME_WALL_INVALID )
	{
		pTFMove->ReconData().m_flSuppressionJumpTime -= frame_msec;
		if ( pTFMove->ReconData().m_flSuppressionJumpTime <= 0.0f )
		{
			pTFMove->ReconData().m_flSuppressionJumpTime = TIME_WALL_INVALID;
		}
	}

	if ( pTFMove->ReconData().m_flSuppressionImpactTime != TIME_WALL_INVALID )
	{
		pTFMove->ReconData().m_flSuppressionImpactTime -= frame_msec;
		if ( pTFMove->ReconData().m_flSuppressionImpactTime <= 0.0f )
		{
			pTFMove->ReconData().m_flSuppressionImpactTime = TIME_WALL_INVALID;
		}
	}

	if ( pTFMove->ReconData().m_flActiveJumpTime != TIME_WALL_INVALID )
	{
		pTFMove->ReconData().m_flActiveJumpTime -= frame_msec;
		if ( pTFMove->ReconData().m_flActiveJumpTime <= 0.0f )
		{
			pTFMove->ReconData().m_flActiveJumpTime = TIME_WALL_INVALID;
		}
	}

	if ( pTFMove->ReconData().m_flStickTime != TIME_WALL_INVALID )
	{
		pTFMove->ReconData().m_flStickTime -= frame_msec;
		if ( pTFMove->ReconData().m_flStickTime <= 0.0f )
		{
			pTFMove->ReconData().m_flStickTime = TIME_WALL_INVALID;

			// Restore velocity at this time
			pTFMove->m_vecVelocity = pTFMove->ReconData().m_vecUnstickVelocity;
		}
	}
}

//-----------------------------------------------------------------------------
// Implement this if you want to know when the player collides during OnPlayerMove
//-----------------------------------------------------------------------------
void CTFGameMovementRecon::OnTryPlayerMoveCollision( trace_t &tr )
{
	if ( !m_bPerformingAirMove )
		return;
	
	// Only keep track of world collisions
	if ( tr.DidHitWorld() )
	{
		CTFMoveData *pTFMove = TFMove();
		if ( pTFMove )
		{
			if ( ( pTFMove->ReconData().m_flSuppressionJumpTime == TIME_WALL_INVALID ) &&
				 ( pTFMove->ReconData().m_flSuppressionImpactTime == TIME_WALL_INVALID ) )
			{
				// No walljumps off of mostly horizontal surfaces...
				if ( fabs( tr.plane.normal.z ) > 0.9f )
					return;
			
				// No walljumps off of the same plane as the last one...
				if ( (pTFMove->ReconData().m_flImpactDist == tr.plane.dist) && 
					(VectorsAreEqual(pTFMove->ReconData().m_vecImpactNormal, tr.plane.normal, 1e-2) ) )
				{
					return;
				}

				// If you hit a wall, no double jumps for you
				pTFMove->ReconData().m_nJumpCount = 2;
				
				// Play an impact sound
				MoveHelper()->StartSound( pTFMove->m_vecAbsOrigin, "Recon.WallJump" );

				pTFMove->ReconData().m_vecImpactNormal = tr.plane.normal;
				pTFMove->ReconData().m_flImpactDist = tr.plane.dist;

				pTFMove->ReconData().m_flActiveJumpTime = TIME_WALL_ACTIVATE_JUMP;
				pTFMove->ReconData().m_flSuppressionImpactTime = TIME_WALL_SUPPRESSION_IMPACT;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementRecon::AirMove()
{
	m_bPerformingAirMove = true;

	// When in the air, recon travels ballistically
	if ( TFMove()->ReconData().m_nJumpCount )
	{
		// Add in any base velocity to the current velocity.
		VectorAdd( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

		TryPlayerMove();
	}
	else
	{
		// But if we're falling (or coming up off ladders), treat it normally
		BaseClass::AirMove();
	}

	m_bPerformingAirMove = false;
}

//-----------------------------------------------------------------------------
// Purpose: Check the jump button to make various jumps
//-----------------------------------------------------------------------------
bool CTFGameMovementRecon::CheckWaterJump()
{
	// See if we are waterjumping.  If so, decrement count and return.
	if (player->m_flWaterJumpTime)
	{
		player->m_flWaterJumpTime -= gpGlobals->frametime;
		if (player->m_flWaterJumpTime < 0)
			player->m_flWaterJumpTime = 0;
		
		return true;
	}

	// If we are in the water most of the way...
	if ( player->GetWaterLevel() >= 2 )
	{	
		// swimming, not jumping
		SetGroundEntity( NULL );

		if(player->GetWaterType() == CONTENTS_WATER)    // We move up a certain amount
			mv->m_vecVelocity[2] = 100;
		else if (player->GetWaterType() == CONTENTS_SLIME)
			mv->m_vecVelocity[2] = 80;
		
		// play swiming sound
		if ( player->m_flSwimSoundTime <= 0 )
		{
			// Don't play sound again for 1 second
			player->m_flSwimSoundTime = 1000;
			PlaySwimSound();
		}
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Resets the impact time
//-----------------------------------------------------------------------------
void CTFGameMovementRecon::ResetWallImpact( CTFMoveData *pTFMove )
{
	if ( pTFMove->ReconData().m_flActiveJumpTime != TIME_WALL_INVALID )
	{
		pTFMove->ReconData().m_flActiveJumpTime = TIME_WALL_INVALID;
		pTFMove->ReconData().m_flSuppressionImpactTime = TIME_WALL_INVALID;
		pTFMove->ReconData().m_vecImpactNormal.Init( 9999, 9999, 9999 );
		pTFMove->ReconData().m_flImpactDist = -9999.0f;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Check the jump button to make various jumps
//-----------------------------------------------------------------------------
bool CTFGameMovementRecon::CheckWallJump( CTFMoveData *pTFMove )
{
	if ( player->GetGroundEntity() != NULL )
		return false;

	if ( pTFMove->ReconData().m_flActiveJumpTime == TIME_WALL_INVALID )
		return false;

	// Play a jump sound
	PlayStepSound( m_pSurfaceData, 1.0, true );

	Vector jumpDir;
	if ( ( pTFMove->m_nButtons & ( IN_MOVELEFT | IN_MOVERIGHT ) ) )
	{
		AngleVectors( pTFMove->m_vecViewAngles, NULL, &jumpDir, NULL );

		// Apply strafe jump...
		jumpDir *= ( pTFMove->m_nButtons & IN_MOVELEFT ) ? -1.0f : 1.0f;
		jumpDir.z = 0.0f;

		if ( pTFMove->m_nButtons & ( IN_FORWARD | IN_BACK ) )
		{
			Vector forward;
			AngleVectors( pTFMove->m_vecViewAngles, &forward, NULL, NULL );
			forward *= 0.5f;
			forward *= ( pTFMove->m_nButtons & IN_BACK ) ? -1.0f : 1.0f;
			forward.z = 0.0;
			jumpDir += forward;
		}

		VectorNormalize( jumpDir );
		jumpDir *= 400;
	}
	else
	{
		AngleVectors( pTFMove->m_vecViewAngles, &jumpDir, NULL, NULL );
		jumpDir *= ( pTFMove->m_nButtons & IN_BACK ) ? -1.0f : 1.0f;
		jumpDir.z = 0.0;

		VectorNormalize( jumpDir );
		jumpDir *= 400;
	}

	pTFMove->ReconData().m_flStickTime = TIME_WALL_STICK;
	pTFMove->ReconData().m_vecUnstickVelocity.Init( jumpDir.x, jumpDir.y, 
		pTFMove->m_vecVelocity[2] + 1.5 * sqrt(2 * 800 * 45.0) );
	if (pTFMove->ReconData().m_vecUnstickVelocity.GetZ() > MAX_VERTICAL_SPEED)
		pTFMove->ReconData().m_vecUnstickVelocity.SetZ( MAX_VERTICAL_SPEED );

	pTFMove->m_vecVelocity.Init( 0, 0, 0 );

	// Don't allow jump into wall
	float normalComponent = DotProduct( pTFMove->ReconData().m_vecUnstickVelocity, pTFMove->ReconData().m_vecImpactNormal );
	if ( normalComponent < 0 )
	{
		Vector vUnstickVel;
		VectorMA( pTFMove->ReconData().m_vecUnstickVelocity, -normalComponent, 
			pTFMove->ReconData().m_vecImpactNormal, vUnstickVel );
		pTFMove->ReconData().m_vecUnstickVelocity = vUnstickVel;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Check the jump button to make various jumps
//-----------------------------------------------------------------------------
bool CTFGameMovementRecon::CheckBackJump( bool bWasInAir )
{
	if ((mv->m_nButtons & IN_BACK) == 0)
		return false;

	Vector jumpDir, right;
	AngleVectors( mv->m_vecViewAngles, &jumpDir, &right, NULL );
	jumpDir.z = 0.0f;
	jumpDir *= -1.0f;

	if (mv->m_nButtons & (IN_MOVELEFT | IN_MOVERIGHT))
	{
		// Apply strafe jump...
		right *= (mv->m_nButtons & IN_MOVELEFT) ? -1.0f : 1.0f;
		right.z = 0.0f;

		// Make us not jump quite at a 45% angle if both are selected
		right *= 0.5f;
		jumpDir += right;
	}

	VectorNormalize( jumpDir );

	float flGroundFactor = 1.0f;
	if ((m_pSurfaceData) /*&& (!bWasInAir)*/ )
	{
		flGroundFactor = m_pSurfaceData->game.jumpFactor;
	}

	jumpDir *= 150 * flGroundFactor;

	// Dampen current motion
	mv->m_vecVelocity[0] *= 0.5f;
	mv->m_vecVelocity[1] *= 0.5f;

	float flSideFactor = (bWasInAir) ? 2.0f : 1.0f;
	float flUpFactor = (bWasInAir) ? 0.5f : 1.5f;
	flSideFactor *= flGroundFactor;
	flUpFactor *= flGroundFactor;

	mv->m_vecVelocity[0] += flSideFactor * jumpDir.x;
	mv->m_vecVelocity[1] += flSideFactor * jumpDir.y;
	mv->m_vecVelocity[2] += flUpFactor * sqrt(2 * 800 * 45.0); 

	mv->m_vecVelocity[0] = clamp( mv->m_vecVelocity[0], -200, 200 );
	mv->m_vecVelocity[1] = clamp( mv->m_vecVelocity[1], -200, 200 );
	if (mv->m_vecVelocity[2] > MAX_VERTICAL_SPEED)
		mv->m_vecVelocity[2] = MAX_VERTICAL_SPEED;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Check the jump button to make various jumps
//-----------------------------------------------------------------------------
bool CTFGameMovementRecon::CheckStrafeJump( bool bWasInAir )
{
	if ( (mv->m_nButtons & (IN_MOVELEFT | IN_MOVERIGHT)) == 0 )
		return false;

	if (mv->m_nButtons & IN_FORWARD)
		return false;

	Vector jumpDir;
	AngleVectors( mv->m_vecViewAngles, NULL, &jumpDir, NULL );

	// Apply strafe jump...
	jumpDir *= (mv->m_nButtons & IN_MOVELEFT) ? -1.0f : 1.0f;
	jumpDir.z = 0.0f;
	VectorNormalize( jumpDir );

	float flGroundFactor = 1.0f;
	if ((m_pSurfaceData) /*&& (!bWasInAir)*/ )
	{
		flGroundFactor = m_pSurfaceData->game.jumpFactor;
	}

	jumpDir *= 300 * flGroundFactor;

	// Dampen current motion
	mv->m_vecVelocity[0] *= 0.5f;
	mv->m_vecVelocity[1] *= 0.5f;
	mv->m_vecVelocity[0] += jumpDir.x;
	mv->m_vecVelocity[1] += jumpDir.y;

	if (!bWasInAir)
		mv->m_vecVelocity[2] += flGroundFactor * sqrt(2 * 800 * 45.0);  // 2 * gravity * height
	else
		mv->m_vecVelocity[2] += 0.5f * sqrt(2 * 800 * 45.0);  // 2 * gravity * height

	mv->m_vecVelocity[0] = clamp( mv->m_vecVelocity[0], -400, 400 );
	mv->m_vecVelocity[1] = clamp( mv->m_vecVelocity[1], -400, 400 );
	if (mv->m_vecVelocity[2] > MAX_VERTICAL_SPEED)
		mv->m_vecVelocity[2] = MAX_VERTICAL_SPEED;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Check the jump button to make various jumps
//-----------------------------------------------------------------------------
bool CTFGameMovementRecon::CheckForwardJump( bool bWasInAir )
{
	// If we are ducking...
	if ( ( player->m_Local.m_bDucking ) || ( player->GetFlags() & FL_DUCKING ) )
	{
		// d = 0.5 * g * t^2		- distance traveled with linear accel
		// t = sqrt(2.0 * 45 / g)	- how long to fall 45 units
		// v = g * t				- velocity at the end (just invert it to jump up that high)
		// v = g * sqrt(2.0 * 45 / g )
		// v^2 = g * g * 2.0 * 45 / g
		// v = sqrt( g * 2.0 * 45 )
		mv->m_vecVelocity[2] = sqrt(2 * 800 * 45.0);  // 2 * gravity * height
	}
	else
	{
		Vector forward, right;
		AngleVectors( mv->m_vecViewAngles, &forward, &right, NULL );
 		forward.z = 0.0;

		if ((mv->m_nButtons & IN_FORWARD) == 0)
		{
			forward.x = forward.y = 0.0f;
		}

		if (mv->m_nButtons & (IN_MOVELEFT | IN_MOVERIGHT))
		{
			// Apply strafe jump...
			right *= (mv->m_nButtons & IN_MOVELEFT) ? -1.0f : 1.0f;
			right.z = 0.0f;

			// Make us not jump quite at a 45% angle if both are selected
			right *= 0.5f;
			forward += right;
		}

		VectorNormalize( forward );

		// Slow down by the speed factor
		float flGroundFactor = 1.0f;
		if ((m_pSurfaceData) /* && (!bWasInAir) */ )
		{
			flGroundFactor = m_pSurfaceData->game.jumpFactor;
		}

		forward *= 400 * flGroundFactor;	  

		// Dampen current motion
		mv->m_vecVelocity[0] *= 0.5f;
		mv->m_vecVelocity[1] *= 0.5f;
		mv->m_vecVelocity[0] += forward.x;
		mv->m_vecVelocity[1] += forward.y;

		float flUpFactor = (bWasInAir) ? 0.7f : 1.0f;
		flUpFactor *= flGroundFactor;

		mv->m_vecVelocity[2] += flUpFactor * MAX_VERTICAL_SPEED;

		// Limit their velocity in X and Y. We don't want to just clamp because that will change the
		// direction we're moving in.		
		for ( int i=0; i < 2; i++ )
		{
			float flAbs = fabs( mv->m_vecVelocity[i] );
			if ( flAbs > 400 )
			{
				mv->m_vecVelocity[0] *= (400.0f / flAbs);
				mv->m_vecVelocity[1] *= (400.0f / flAbs);
			}
		}

		if (mv->m_vecVelocity[2] > MAX_VERTICAL_SPEED)
 			mv->m_vecVelocity[2] = MAX_VERTICAL_SPEED;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Check the jump button to make various jumps
//-----------------------------------------------------------------------------
bool CTFGameMovementRecon::CheckJumpButton()
{
	// FIXME: Refactor this so we don't have this complicated duplicate
	// code here + in gamemovement.cpp

	if ( player->pl.deadflag )
	{
		mv->m_nOldButtons |= IN_JUMP ;	// don't jump again until released
		return false;
	}

	// Water jumps!
	if ( CheckWaterJump() )
		return false;

	if ( mv->m_nOldButtons & IN_JUMP )
		return false;		// don't pogo stick

	CTFMoveData *pTFMove = static_cast<CTFMoveData*>( mv );

	// Check for wall jump...
	if ( !CheckWallJump( pTFMove ) )
	{
		// If we already did one air jump, can't do another 
 		if ( (player->GetGroundEntity() == NULL ) && ( pTFMove->ReconData().m_nJumpCount > 1) )
		{
			mv->m_nOldButtons |= IN_JUMP;
			return false;		// in air, so no effect
		}

		pTFMove->ReconData().m_nJumpCount += 1;

		// Am I doing a double-jump?
		bool bWasInAir = (player->GetGroundEntity() == NULL);

		// In the air now.
		SetGroundEntity( NULL );
		
		PlayStepSound( m_pSurfaceData, 1.0, true );

		if (!CheckBackJump(bWasInAir))
		{
			if (CheckStrafeJump(bWasInAir))
			{
				// Can't double jump out of a roll....
				pTFMove->ReconData().m_nJumpCount += 1;
			}
			else
			{
				CheckForwardJump(bWasInAir);
			}
		}
	}

	pTFMove->ReconData().m_flSuppressionJumpTime = TIME_WALL_SUPPRESSION_JUMP;	

	FinishGravity();

	mv->m_outWishVel = mv->m_vecVelocity;
	mv->m_outStepHeight += 0.1f;

	// Flag that we jumped.
	mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released
	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementRecon::GetPlayerMins( bool bDucked ) const
{
	return bDucked ? m_vDuckMins : m_vStandMins; 
}
	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementRecon::GetPlayerMaxs( bool bDucked ) const
{
	return bDucked ? m_vDuckMaxs : m_vStandMaxs;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementRecon::GetPlayerViewOffset( bool bDucked ) const
{
	return bDucked ? m_vDuckViewOffset : m_vStandViewOffset;
}
