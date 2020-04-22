//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "in_buttons.h"
#include "tf_gamemovement_commando.h"

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGameMovementCommando::CTFGameMovementCommando()
{
	m_pCommandoData = NULL;

	m_vStandMins = COMMANDOCLASS_HULL_STAND_MIN;
	m_vStandMaxs = COMMANDOCLASS_HULL_STAND_MAX;
	m_vStandViewOffset = COMMANDOCLASS_VIEWOFFSET_STAND;

	m_vDuckMins = COMMANDOCLASS_HULL_DUCK_MIN;
	m_vDuckMaxs = COMMANDOCLASS_HULL_DUCK_MAX;
	m_vDuckViewOffset = COMMANDOCLASS_VIEWOFFSET_DUCK;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementCommando::ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData )
{
	// Get the class specific data from the TFMoveData structure
	Assert( PlayerClassCommandoData_t::PLAYERCLASS_ID == pTFMoveData->m_nClassID );

	// Is this how I want to handle this???
//	m_pCommandoData = &pTFMoveData->CommandoData();

	// to test pass it through!!
	BaseClass::ProcessMovement( (CBasePlayer *)pPlayer, static_cast<CMoveData*>( pTFMoveData ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementCommando::GetPlayerMins( bool bDucked ) const
{
	return bDucked ? m_vDuckMins : m_vStandMins; 
}

	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementCommando::GetPlayerMaxs( bool bDucked ) const
{
	return bDucked ? m_vDuckMaxs : m_vStandMaxs;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementCommando::GetPlayerViewOffset( bool bDucked ) const
{
	return bDucked ? m_vDuckViewOffset : m_vStandViewOffset;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFGameMovementCommando::CheckDoubleTapForward( void )
{
	// Check for other movement keys!!!
	if ( ( TFMove()->m_nButtons & IN_MOVELEFT ) || ( TFMove()->m_nButtons & IN_MOVERIGHT ) ||
		 ( TFMove()->m_nButtons & IN_BACK ) || ( TFMove()->m_nButtons & IN_JUMP ) )
	{
		TFMove()->CommandoData().m_flDoubleTapForwardTime = COMMANDO_TIME_INVALID;
		return false;
	}


	if ( ( TFMove()->m_nButtons & IN_FORWARD ) && !( TFMove()->m_nOldButtons & IN_FORWARD ) )
	{
		// Start timer.
		if ( TFMove()->CommandoData().m_flDoubleTapForwardTime == COMMANDO_TIME_INVALID )
		{
			TFMove()->CommandoData().m_flDoubleTapForwardTime = COMMANDO_DOUBLETAP_TIME;
		}
		// Check for a double tap.
		else
		{
			if ( TFMove()->CommandoData().m_flDoubleTapForwardTime > 0.0f )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementCommando::CheckBullRush( void )
{
	// Don't check for bullrush if we are dead!
	if ( IsDead() )
		return;

	// Don't go into bullrush if noclipping
	if ( player->GetMoveType() == MOVETYPE_NOCLIP )
		return;

	// Cannot bullrush inside of a vehicle (manned <gun>).
#if !defined (CLIENT_DLL)
	IServerVehicle *pVehicle = player->GetVehicle();
	if ( pVehicle )
		return;
#else
	IClientVehicle *pVehicle = player->GetVehicle();
	if ( pVehicle )
		return;
#endif

	if ( CheckDoubleTapForward() && !TFMove()->CommandoData().m_bBullRush && 
		 TFMove()->CommandoData().m_bCanBullRush && !( player->GetFlags() & FL_DUCKING ) )
	{
		// Set in a bull rush.
		TFMove()->CommandoData().m_bBullRush = true;

		// Set timers.
		TFMove()->CommandoData().m_flBullRushTime = COMMANDO_BULLRUSH_TIME;

		// Lock view/move angles
		Vector vBullrushDir;
		AngleVectors( TFMove()->m_vecViewAngles, &vBullrushDir, NULL, NULL );
		TFMove()->CommandoData().m_vecBullRushDir = vBullrushDir;
		TFMove()->CommandoData().m_vecBullRushViewDir = TFMove()->m_vecViewAngles;
		TFMove()->CommandoData().m_vecBullRushViewGoalDir.Init();
		TFMove()->CommandoData().m_vecBullRushViewGoalDir.SetY( TFMove()->m_vecViewAngles.y );

		// Set movement type.
		player->SetMoveType( (MoveType_t)COMMANDO_MOVETYPE_BULLRUSH );
		player->SetMoveCollide( MOVECOLLIDE_DEFAULT );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFGameMovementCommando::PrePlayerMove( void )
{
	// Assume we don't touch anything (Reset the touch list).
	MoveHelper()->ResetTouchList();

	// Check to see if we are stuck.
	if ( CheckStuck() )
		return false;

	CheckBullRush();

	// Update (reduce) movement timers.
	UpdateTimers();

	// Check to see if the player is dead and setup death data, otherwise setup 
	// the players view angles.
	if ( !CheckDeath() )
	{
		SetupViewAngles();
	}

	// Handle ducking.
	HandleDuck();

	// Handle ladder.
	HandleLadder();

	// Categorize the player's position.
	CategorizePosition();

	// Calculate the player's movement speed (has to happen after categorize position)
	SetupSpeed();

	// Update our stepping sound (based on the player's location).
	player->UpdateStepSound( m_pSurfaceData, mv->m_vecAbsOrigin, mv->m_vecVelocity );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementCommando::HandlePlayerMove( void )
{
	// Handle the specific bull rush movement type.
	if ( player->GetMoveType() == COMMANDO_MOVETYPE_BULLRUSH )
	{
		BullRushMove();
		return;
	}

	// Let the default TF2 player movement code handle the move.
	BaseClass::HandlePlayerMove();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementCommando::HandleDuck( void )
{
	if ( player->GetMoveType() == COMMANDO_MOVETYPE_BULLRUSH )
		return;

	BaseClass::HandleDuck();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementCommando::UpdateTimers( void )
{
	BaseClass::UpdateTimers();

	CTFMoveData *pMoveData = TFMove();
	if ( !pMoveData )
		return;

	float frame_msec = 1000.0f * gpGlobals->frametime;

	// Decrement the bull rush time.
	if ( pMoveData->CommandoData().m_flBullRushTime != COMMANDO_TIME_INVALID )
	{
		if ( pMoveData->CommandoData().m_flBullRushTime > 0.0f )
		{
			pMoveData->CommandoData().m_flBullRushTime -= frame_msec;
		}
		else
		{
			TFMove()->CommandoData().m_bBullRush = false;
			TFMove()->CommandoData().m_flBullRushTime = COMMANDO_TIME_INVALID;
			player->SetMoveType( MOVETYPE_WALK );
			player->SetMoveCollide( MOVECOLLIDE_DEFAULT );
		}
	}

	if ( pMoveData->CommandoData().m_flDoubleTapForwardTime != COMMANDO_TIME_INVALID )
	{
		if ( pMoveData->CommandoData().m_flDoubleTapForwardTime > 0.0f )
		{
			pMoveData->CommandoData().m_flDoubleTapForwardTime -= frame_msec;
		}
		else
		{
			pMoveData->CommandoData().m_flDoubleTapForwardTime = COMMANDO_TIME_INVALID;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementCommando::SetupViewAngles( void )
{

	BaseClass::SetupViewAngles();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementCommando::SetupSpeed( void )
{
	BaseClass::SetupSpeed();

	if ( player->GetMoveType() == COMMANDO_MOVETYPE_BULLRUSH )
	{
		mv->m_flMaxSpeed = sv_maxspeed.GetFloat();

		// Slow down by the speed factor
		if (m_pSurfaceData)
		{
			mv->m_flMaxSpeed *= m_pSurfaceData->game.maxSpeedFactor;
		}

		mv->m_flForwardMove = TFMove()->m_flClientMaxSpeed * 4.0f;
		mv->m_flUpMove = 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFGameMovementCommando::CalcWishVelocityAndPosition( Vector &vWishPos, Vector &vWishDir, float &flWishSpeed )
{
	//
	// Determine the movement angles.
	//
	Vector vForward, vRight, vUp;
	if ( player->GetMoveType() == COMMANDO_MOVETYPE_BULLRUSH )
	{
		vForward = TFMove()->CommandoData().m_vecBullRushDir;
		// Cross the bullrush direction with the z-axis to get the right vector.
		CrossProduct( vForward, Vector( 0.0f, 0.0f, 1.0f ), vRight );
		vUp.Init();
	}
	else
	{
		AngleVectors( mv->m_vecViewAngles, &vForward, &vRight, &vUp );

		//
		// Zero out the z component of the movement vectors and renormalize.
		//
		vForward.z = 0.0f;
		VectorNormalize( vForward );
		vRight.z = 0.0f;
		VectorNormalize( vRight );
	}

	//
	// Determine the xy parts of the velocity.
	//
	Vector vWishVel( 0.0f, 0.0f, 0.0f );
	for ( int axis = 0; axis < 2; axis++ )
	{
		vWishVel[axis] = ( vForward[axis] * mv->m_flForwardMove ) +
			             ( vRight[axis] * mv->m_flSideMove );
	}
	vWishVel.z = 0.0f;

	//
	// Componentize the velocity into direction and speed.
	//
	VectorCopy( vWishVel, vWishDir );
	flWishSpeed = VectorNormalize( vWishDir );

	if ( flWishSpeed > mv->m_flMaxSpeed )
	{
		VectorScale( vWishVel, ( mv->m_flMaxSpeed / flWishSpeed ), vWishVel );
		flWishSpeed = mv->m_flMaxSpeed;
	}

	//
	// Accelerate (in the plane).
	//
	mv->m_vecVelocity.z = 0.0f;
	Accelerate( vWishDir, flWishSpeed, sv_accelerate.GetFloat() );
	mv->m_vecVelocity.z = 0.0f;

	// Add in any base velocity (from conveyers, etc.) to the current velocity.
	VectorAdd( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

	//
	// Stop the player (zero out velocity) if the player's speed is below a
	// given threshold.
	//
	float flSpeed = VectorLength( mv->m_vecVelocity );
	if ( flSpeed < 1.0f /*SPEED_STOP_THRESHOLD*/ )
	{
		mv->m_vecVelocity.Init();
		return false;
	}

	//
	// Calculate the wish position.
	//
	vWishPos.x = mv->m_vecAbsOrigin.x + ( mv->m_vecVelocity.x * gpGlobals->frametime );
	vWishPos.y = mv->m_vecAbsOrigin.y + ( mv->m_vecVelocity.y * gpGlobals->frametime );
	vWishPos.z = mv->m_vecAbsOrigin.z;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementCommando::BullRushMove( void )
{
	CTFMoveData *pMoveData = TFMove();
	if ( !pMoveData )
		return;

	// Ignoring water for now!!!!
	StartGravity();

	// Fricion is handled before we add in any base velocity. That way, if we are on a conveyor, 
	//  we don't slow when standing still, relative to the conveyor.
	if (player->GetGroundEntity() != NULL)
	{
		mv->m_vecVelocity[2] = 0.0;
		Friction();
	}

	// Make sure velocity is valid.
	CheckVelocity();

	if (player->GetGroundEntity() != NULL)
	{
		WalkMove2();
	}
	else
	{
		AirMove();  // Take into account movement when in air.
	}

	// Set final flags.
	CategorizePosition();

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like
	//  a conveyor (or maybe another monster?)
	VectorSubtract (mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

	// Make sure velocity is valid.
	CheckVelocity();

	// Add any remaining gravitational component.
	FinishGravity();

	// If we are on ground, no downward velocity.
	if ( player->GetGroundEntity() != NULL )
	{
		mv->m_vecVelocity[2] = 0;
	}

	CheckFalling();
}