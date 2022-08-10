//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "gamemovement.h"
#include "tf_gamerules.h"
#include "tf_shareddefs.h"
#include "in_buttons.h"
#include "movevars_shared.h"
#include "collisionutils.h"
#include "debugoverlay_shared.h"
#include "baseobject_shared.h"
#include "particle_parse.h"
#include "baseobject_shared.h"
#include "coordsize.h"

#ifdef CLIENT_DLL
	#include "c_tf_player.h"
	#include "c_world.h"
	#include "c_team.h"

	#define CTeam C_Team

#else
	#include "tf_player.h"
	#include "team.h"
#endif

ConVar	tf_maxspeed( "tf_maxspeed", "400", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_CHEAT  | FCVAR_DEVELOPMENTONLY);
ConVar	tf_showspeed( "tf_showspeed", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar	tf_avoidteammates( "tf_avoidteammates", "1", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar  tf_solidobjects( "tf_solidobjects", "1", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar	tf_clamp_back_speed( "tf_clamp_back_speed", "0.9", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar  tf_clamp_back_speed_min( "tf_clamp_back_speed_min", "100", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

#define TF_MAX_SPEED   400

#define TF_WATERJUMP_FORWARD  30
#define TF_WATERJUMP_UP       300
//ConVar	tf_waterjump_up( "tf_waterjump_up", "300", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
//ConVar	tf_waterjump_forward( "tf_waterjump_forward", "30", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

class CTFGameMovement : public CGameMovement
{
public:
	DECLARE_CLASS( CTFGameMovement, CGameMovement );

	CTFGameMovement(); 

	virtual void PlayerMove();
	virtual unsigned int PlayerSolidMask( bool brushOnly = false );
	virtual void ProcessMovement( CBasePlayer *pBasePlayer, CMoveData *pMove );
	virtual bool CanAccelerate();
	virtual bool CheckJumpButton();
	virtual bool CheckWater( void );
	virtual void WaterMove( void );
	virtual void FullWalkMove();
	virtual void WalkMove( void );
	virtual void AirMove( void );
	virtual void FullTossMove( void );
	virtual void CategorizePosition( void );
	virtual void CheckFalling( void );
	virtual void Duck( void );
	virtual Vector GetPlayerViewOffset( bool ducked ) const;

	virtual void	TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm );
	virtual CBaseHandle	TestPlayerPosition( const Vector& pos, int collisionGroup, trace_t& pm );
	virtual void	StepMove( Vector &vecDestination, trace_t &trace );
	virtual bool	GameHasLadders() const;
	virtual void SetGroundEntity( trace_t *pm );
	virtual void PlayerRoughLandingEffects( float fvol );
protected:

	virtual void CheckWaterJump(void );
	void		 FullWalkMoveUnderwater();

private:

	bool		CheckWaterJumpButton( void );
	void		AirDash( void );
	void		PreventBunnyJumping();

private:

	Vector		m_vecWaterPoint;
	CTFPlayer  *m_pTFPlayer;
};


// Expose our interface.
static CTFGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameMovement, IGameMovement,INTERFACENAME_GAMEMOVEMENT, g_GameMovement );


// ---------------------------------------------------------------------------------------- //
// CTFGameMovement.
// ---------------------------------------------------------------------------------------- //

CTFGameMovement::CTFGameMovement()
{
	m_pTFPlayer = NULL;
}

//---------------------------------------------------------------------------------------- 
// Purpose: moves the player
//----------------------------------------------------------------------------------------
void CTFGameMovement::PlayerMove()
{
	// call base class to do movement
	BaseClass::PlayerMove();

	// handle player's interaction with water
	int nNewWaterLevel = m_pTFPlayer->GetWaterLevel();
	if ( m_nOldWaterLevel != nNewWaterLevel )
	{
		if ( WL_NotInWater == m_nOldWaterLevel )
		{
			// The player has just entered the water.  Determine if we should play a splash sound.
			bool bPlaySplash = false;
					
			Vector vecVelocity = m_pTFPlayer->GetAbsVelocity();
			if ( vecVelocity.z <= -200.0f )
			{
				// If the player has significant downward velocity, play a splash regardless of water depth.  (e.g. Jumping hard into a puddle)
				bPlaySplash = true;
			}
			else
			{
				// Look at the water depth below the player.  If it's significantly deep, play a splash to accompany the sinking that's about to happen.
				Vector vecStart = m_pTFPlayer->GetAbsOrigin();
				Vector vecEnd = vecStart;
				vecEnd.z -= 20;	// roughly thigh deep
				trace_t tr;
				// see if we hit anything solid a little bit below the player
				UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID,m_pTFPlayer, COLLISION_GROUP_NONE, &tr );
				if ( tr.fraction >= 1.0f ) 
				{
					// some amount of water below the player, play a splash
					bPlaySplash = true;
				}
			}

			if ( bPlaySplash )
			{
				m_pTFPlayer->EmitSound( "Physics.WaterSplash" );
			}
		} 
	}
}

Vector CTFGameMovement::GetPlayerViewOffset( bool ducked ) const
{
	return ducked ? VEC_DUCK_VIEW : ( m_pTFPlayer->GetClassEyeHeight() );
}

//-----------------------------------------------------------------------------
// Purpose: Allow bots etc to use slightly different solid masks
//-----------------------------------------------------------------------------
unsigned int CTFGameMovement::PlayerSolidMask( bool brushOnly )
{
	unsigned int uMask = 0;

	if ( m_pTFPlayer )
	{
		switch( m_pTFPlayer->GetTeamNumber() )
		{
		case TF_TEAM_RED:
			uMask = CONTENTS_BLUETEAM;
			break;

		case TF_TEAM_BLUE:
			uMask = CONTENTS_REDTEAM;
			break;
		}
	}

	return ( uMask | BaseClass::PlayerSolidMask( brushOnly ) );
}

//-----------------------------------------------------------------------------
// Purpose: Overridden to allow players to run faster than the maxspeed
//-----------------------------------------------------------------------------
void CTFGameMovement::ProcessMovement( CBasePlayer *pBasePlayer, CMoveData *pMove )
{
	// Verify data.
	Assert( pBasePlayer );
	Assert( pMove );
	if ( !pBasePlayer || !pMove )
		return;

	// Reset point contents for water check.
	ResetGetPointContentsCache();

	// Cropping movement speed scales mv->m_fForwardSpeed etc. globally
	// Once we crop, we don't want to recursively crop again, so we set the crop
	// flag globally here once per usercmd cycle.
	m_iSpeedCropped = SPEED_CROPPED_RESET;

	// Get the current TF player.
	m_pTFPlayer = ToTFPlayer( pBasePlayer );
	player = m_pTFPlayer;
	mv = pMove;

	// The max speed is currently set to the scout - if this changes we need to change this!
	mv->m_flMaxSpeed = TF_MAX_SPEED; /*tf_maxspeed.GetFloat();*/

	// Run the command.
	PlayerMove();
	FinishMove();
}


bool CTFGameMovement::CanAccelerate()
{
	// Only allow the player to accelerate when in certain states.
	int nCurrentState = m_pTFPlayer->m_Shared.GetState();
	if ( nCurrentState == TF_STATE_ACTIVE )
	{
		return player->GetWaterJumpTime() == 0;
	}
	else if ( player->IsObserver() )
	{
		return true;
	}
	else
	{	
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if we are in water.  If so the jump button acts like a
// swim upward key.
//-----------------------------------------------------------------------------
bool CTFGameMovement::CheckWaterJumpButton( void )
{
	// See if we are water jumping.  If so, decrement count and return.
	if ( player->m_flWaterJumpTime )
	{
		player->m_flWaterJumpTime -= gpGlobals->frametime;
		if (player->m_flWaterJumpTime < 0)
		{
			player->m_flWaterJumpTime = 0;
		}

		return false;
	}

	// In water above our waist.
	if ( player->GetWaterLevel() >= 2 )
	{	
		// Swimming, not jumping.
		SetGroundEntity( NULL );

		// We move up a certain amount.
		if ( player->GetWaterType() == CONTENTS_WATER )
		{
			mv->m_vecVelocity[2] = 100;
		}
		else if ( player->GetWaterType() == CONTENTS_SLIME )
		{
			mv->m_vecVelocity[2] = 80;
		}

		// Play swiming sound.
		if ( player->m_flSwimSoundTime <= 0 )
		{
			// Don't play sound again for 1 second.
			player->m_flSwimSoundTime = 1000;
			PlaySwimSound();
		}

		return false;
	}

	return true;
}

void CTFGameMovement::AirDash( void )
{
	// Apply approx. the jump velocity added to an air dash.
	Assert( sv_gravity.GetFloat() == 800.0f );
	float flDashZ = 268.3281572999747f;

	// Get the wish direction.
	Vector vecForward, vecRight;
	AngleVectors( mv->m_vecViewAngles, &vecForward, &vecRight, NULL );
	vecForward.z = 0.0f;
	vecRight.z = 0.0f;		
	VectorNormalize( vecForward );
	VectorNormalize( vecRight );

	// Copy movement amounts
	float flForwardMove = mv->m_flForwardMove;
	float flSideMove = mv->m_flSideMove;

	// Find the direction,velocity in the x,y plane.
	Vector vecWishDirection( ( ( vecForward.x * flForwardMove ) + ( vecRight.x * flSideMove ) ),
		                     ( ( vecForward.y * flForwardMove ) + ( vecRight.y * flSideMove ) ), 
		                     0.0f );
	
	// Update the velocity on the scout.
	mv->m_vecVelocity = vecWishDirection;
	mv->m_vecVelocity.z += flDashZ;

	m_pTFPlayer->m_Shared.SetAirDash( true );

	// Play the gesture.
	m_pTFPlayer->DoAnimationEvent( PLAYERANIMEVENT_DOUBLEJUMP );
}

// Only allow bunny jumping up to 1.2x server / player maxspeed setting
#define BUNNYJUMP_MAX_SPEED_FACTOR 1.2f

void CTFGameMovement::PreventBunnyJumping()
{
	// Speed at which bunny jumping is limited
	float maxscaledspeed = BUNNYJUMP_MAX_SPEED_FACTOR * player->m_flMaxspeed;
	if ( maxscaledspeed <= 0.0f )
		return;

	// Current player speed
	float spd = mv->m_vecVelocity.Length();
	if ( spd <= maxscaledspeed )
		return;

	// Apply this cropping fraction to velocity
	float fraction = ( maxscaledspeed / spd );


	mv->m_vecVelocity *= fraction;
}

bool CTFGameMovement::CheckJumpButton()
{
	// Are we dead?  Then we cannot jump.
	if ( player->pl.deadflag )
		return false;

	// Check to see if we are in water.
	if ( !CheckWaterJumpButton() )
		return false;

	// Cannot jump while taunting
	if ( m_pTFPlayer->m_Shared.InCond( TF_COND_TAUNTING ) )
		return false;

	// Check to see if the player is a scout.
	bool bScout = m_pTFPlayer->GetPlayerClass()->IsClass( TF_CLASS_SCOUT );
	bool bAirDash = false;
	bool bOnGround = ( player->GetGroundEntity() != NULL );

	// Cannot jump will ducked.
	if ( player->GetFlags() & FL_DUCKING )
	{
		// Let a scout do it.
		bool bAllow = ( bScout && !bOnGround );

		if ( !bAllow )
			return false;
	}

	// Cannot jump while in the unduck transition.
	if ( ( player->m_Local.m_bDucking && (  player->GetFlags() & FL_DUCKING ) ) || ( player->m_Local.m_flDuckJumpTime > 0.0f ) )
		return false;

	// Cannot jump again until the jump button has been released.
	if ( mv->m_nOldButtons & IN_JUMP )
		return false;

	// In air, so ignore jumps (unless you are a scout).
	if ( !bOnGround )
	{
		if ( bScout && !m_pTFPlayer->m_Shared.IsAirDashing() )
		{
			bAirDash = true;
		}
		else
		{
			mv->m_nOldButtons |= IN_JUMP;
			return false;
		}
	}

	// Check for an air dash.
	if ( bAirDash )
	{
		AirDash();
		return true;
	}

	PreventBunnyJumping();

	// Start jump animation and player sound (specific TF animation and flags).
	m_pTFPlayer->DoAnimationEvent( PLAYERANIMEVENT_JUMP );
	player->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->m_pSurfaceData, 1.0, true );
	m_pTFPlayer->m_Shared.SetJumping( true );

	// Set the player as in the air.
	SetGroundEntity( NULL );

	// Check the surface the player is standing on to see if it impacts jumping.
	float flGroundFactor = 1.0f;
	if ( player->m_pSurfaceData )
	{
		flGroundFactor = player->m_pSurfaceData->game.jumpFactor; 
	}

	// fMul = sqrt( 2.0 * gravity * jump_height (21.0units) ) * GroundFactor
	Assert( sv_gravity.GetFloat() == 800.0f );
	float flMul = 268.3281572999747f * flGroundFactor;

	// Save the current z velocity.
	float flStartZ = mv->m_vecVelocity[2];

	// Acclerate upward
	if ( (  player->m_Local.m_bDucking ) || (  player->GetFlags() & FL_DUCKING ) )
	{
		// If we are ducking...
		// d = 0.5 * g * t^2		- distance traveled with linear accel
		// t = sqrt(2.0 * 45 / g)	- how long to fall 45 units
		// v = g * t				- velocity at the end (just invert it to jump up that high)
		// v = g * sqrt(2.0 * 45 / g )
		// v^2 = g * g * 2.0 * 45 / g
		// v = sqrt( g * 2.0 * 45 )
		mv->m_vecVelocity[2] = flMul;  // 2 * gravity * jump_height * ground_factor
	}
	else
	{
		mv->m_vecVelocity[2] += flMul;  // 2 * gravity * jump_height * ground_factor
	}

	// Apply gravity.
	FinishGravity();

	// Save the output data for the physics system to react to if need be.
	mv->m_outJumpVel.z += mv->m_vecVelocity[2] - flStartZ;
	mv->m_outStepHeight += 0.15f;

	// Flag that we jumped and don't jump again until it is released.
	mv->m_nOldButtons |= IN_JUMP;
	return true;
}

bool CTFGameMovement::CheckWater( void )
{
	Vector vecPlayerMin = GetPlayerMins();
	Vector vecPlayerMax = GetPlayerMaxs();

	Vector vecPoint( ( mv->GetAbsOrigin().x + ( vecPlayerMin.x + vecPlayerMax.x ) * 0.5f ),
				     ( mv->GetAbsOrigin().y + ( vecPlayerMin.y + vecPlayerMax.y ) * 0.5f ),
				     ( mv->GetAbsOrigin().z + vecPlayerMin.z + 1 ) );


	// Assume that we are not in water at all.
	int wl = WL_NotInWater;
	int wt = CONTENTS_EMPTY;

	// Check to see if our feet are underwater.
	int nContents = GetPointContentsCached( vecPoint, 0 );	
	if ( nContents & MASK_WATER )
	{
		// Clear our jump flag, because we have landed in water.
		m_pTFPlayer->m_Shared.SetJumping( false );

		// Set water type and level.
		wt = nContents;
		wl = WL_Feet;

		float flWaistZ = mv->GetAbsOrigin().z + ( vecPlayerMin.z + vecPlayerMax.z ) * 0.5f + 12.0f;

		// Now check eyes
		vecPoint.z = mv->GetAbsOrigin().z + player->GetViewOffset()[2];
		nContents = GetPointContentsCached( vecPoint, 1 );
		if ( nContents & MASK_WATER )
		{
			// In over our eyes
			wl = WL_Eyes;  
			VectorCopy( vecPoint, m_vecWaterPoint );
			m_vecWaterPoint.z = flWaistZ;
		}
		else
		{
			// Now check a point that is at the player hull midpoint (waist) and see if that is underwater.
			vecPoint.z = flWaistZ;
			nContents = GetPointContentsCached( vecPoint, 2 );
			if ( nContents & MASK_WATER )
			{
				// Set the water level at our waist.
				wl = WL_Waist;
				VectorCopy( vecPoint, m_vecWaterPoint );
			}
		}
	}

	player->SetWaterLevel( wl );
	player->SetWaterType( wt );

	// If we just transitioned from not in water to water, record the time for splashes, etc.
	if ( ( WL_NotInWater == m_nOldWaterLevel ) && ( wl >  WL_NotInWater ) )
	{
		m_flWaterEntryTime = gpGlobals->curtime;
	}

	return ( wl > WL_Feet );
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameMovement::WaterMove( void )
{
	int i;
	float	wishspeed;
	Vector	wishdir;
	Vector	start, dest;
	Vector  temp;
	trace_t	pm;
	float speed, newspeed, addspeed, accelspeed;

	// Determine movement angles.
	Vector vecForward, vecRight, vecUp;
	AngleVectors( mv->m_vecViewAngles, &vecForward, &vecRight, &vecUp );

	// Calculate the desired direction and speed.
	Vector vecWishVelocity;
	int iAxis;
	for ( iAxis = 0 ; iAxis < 3; ++iAxis )
	{
		vecWishVelocity[iAxis] = ( vecForward[iAxis] * mv->m_flForwardMove ) + ( vecRight[iAxis] * mv->m_flSideMove );
	}

	// Check for upward velocity (JUMP).
	if ( mv->m_nButtons & IN_JUMP )
	{
		if ( player->GetWaterLevel() == WL_Eyes )
		{
			vecWishVelocity[2] += mv->m_flClientMaxSpeed;
		}
	}
	// Sinking if not moving.
	else if ( !mv->m_flForwardMove && !mv->m_flSideMove && !mv->m_flUpMove )
	{
		vecWishVelocity[2] -= 60;
	}
	// Move up based on view angle.
	else
	{
		vecWishVelocity[2] += mv->m_flUpMove;
	}

	// Copy it over and determine speed
	VectorCopy( vecWishVelocity, wishdir );
	wishspeed = VectorNormalize( wishdir );

	// Cap speed.
	if (wishspeed > mv->m_flMaxSpeed)
	{
		VectorScale( vecWishVelocity, mv->m_flMaxSpeed/wishspeed, vecWishVelocity );
		wishspeed = mv->m_flMaxSpeed;
	}

	// Slow us down a bit.
	wishspeed *= 0.8;
	
	// Water friction
	VectorCopy( mv->m_vecVelocity, temp );
	speed = VectorNormalize( temp );
	if ( speed )
	{
		newspeed = speed - gpGlobals->frametime * speed * sv_friction.GetFloat() * player->m_surfaceFriction;
		if ( newspeed < 0.1f )
		{
			newspeed = 0;
		}

		VectorScale (mv->m_vecVelocity, newspeed/speed, mv->m_vecVelocity);
	}
	else
	{
		newspeed = 0;
	}

	// water acceleration
	if (wishspeed >= 0.1f)  // old !
	{
		addspeed = wishspeed - newspeed;
		if (addspeed > 0)
		{
			VectorNormalize(vecWishVelocity);
			accelspeed = sv_accelerate.GetFloat() * wishspeed * gpGlobals->frametime * player->m_surfaceFriction;
			if (accelspeed > addspeed)
			{
				accelspeed = addspeed;
			}

			for (i = 0; i < 3; i++)
			{
				float deltaSpeed = accelspeed * vecWishVelocity[i];
				mv->m_vecVelocity[i] += deltaSpeed;
				mv->m_outWishVel[i] += deltaSpeed;
			}
		}
	}

	VectorAdd (mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);

	// Now move
	// assume it is a stair or a slope, so press down from stepheight above
	VectorMA (mv->GetAbsOrigin(), gpGlobals->frametime, mv->m_vecVelocity, dest);
	
	TracePlayerBBox( mv->GetAbsOrigin(), dest, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );
	if ( pm.fraction == 1.0f )
	{
		VectorCopy( dest, start );
		if ( player->m_Local.m_bAllowAutoMovement )
		{
			start[2] += player->m_Local.m_flStepSize + 1;
		}
		
		TracePlayerBBox( start, dest, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );

		if (!pm.startsolid && !pm.allsolid)
		{	
#if 0
			float stepDist = pm.endpos.z - mv->GetAbsOrigin().z;
			mv->m_outStepHeight += stepDist;
			// walked up the step, so just keep result and exit

			Vector vecNewWaterPoint;
			VectorCopy( m_vecWaterPoint, vecNewWaterPoint );
			vecNewWaterPoint.z += ( dest.z - mv->GetAbsOrigin().z );
			bool bOutOfWater = !( enginetrace->GetPointContents( vecNewWaterPoint ) & MASK_WATER );
			if ( bOutOfWater && ( mv->m_vecVelocity.z > 0.0f ) && ( pm.fraction == 1.0f )  )
			{
				// Check the waist level water positions.
				trace_t traceWater;
				UTIL_TraceLine( vecNewWaterPoint, m_vecWaterPoint, CONTENTS_WATER, player, COLLISION_GROUP_NONE, &traceWater );
				if( traceWater.fraction < 1.0f )
				{
					float flFraction = 1.0f - traceWater.fraction;

//					Vector vecSegment;
//					VectorSubtract( mv->GetAbsOrigin(), dest, vecSegment );
//					VectorMA( mv->GetAbsOrigin(), flFraction, vecSegment, mv->GetAbsOrigin() );
					float flZDiff = dest.z - mv->GetAbsOrigin().z;
					float flSetZ = mv->GetAbsOrigin().z + ( flFraction * flZDiff );
					flSetZ -= 0.0325f;

					VectorCopy (pm.endpos, mv->GetAbsOrigin());
					mv->GetAbsOrigin().z = flSetZ;
					VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
					mv->m_vecVelocity.z = 0.0f;
				}

			}
			else
			{
				VectorCopy (pm.endpos, mv->GetAbsOrigin());
				VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
			}

			return;
#endif
			float stepDist = pm.endpos.z - mv->GetAbsOrigin().z;
			mv->m_outStepHeight += stepDist;
			// walked up the step, so just keep result and exit
			mv->SetAbsOrigin( pm.endpos );
			VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
			return;
		}

		// Try moving straight along out normal path.
		TryPlayerMove();
	}
	else
	{
		if ( !player->GetGroundEntity() )
		{
			TryPlayerMove();
			VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
			return;
		}

		StepMove( dest, pm );
	}
	
	VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameMovement::WalkMove( void )
{
	// Get the movement angles.
	Vector vecForward, vecRight, vecUp;
	AngleVectors( mv->m_vecViewAngles, &vecForward, &vecRight, &vecUp );
	vecForward.z = 0.0f;
	vecRight.z = 0.0f;		
	VectorNormalize( vecForward );
	VectorNormalize( vecRight );

	// Copy movement amounts
	float flForwardMove = mv->m_flForwardMove;
	float flSideMove = mv->m_flSideMove;
	
	// Find the direction,velocity in the x,y plane.
	Vector vecWishDirection( ( ( vecForward.x * flForwardMove ) + ( vecRight.x * flSideMove ) ),
		                     ( ( vecForward.y * flForwardMove ) + ( vecRight.y * flSideMove ) ), 
							 0.0f );

	// Calculate the speed and direction of movement, then clamp the speed.
	float flWishSpeed = VectorNormalize( vecWishDirection );
	flWishSpeed = clamp( flWishSpeed, 0.0f, mv->m_flMaxSpeed );

	// Accelerate in the x,y plane.
	mv->m_vecVelocity.z = 0;
	Accelerate( vecWishDirection, flWishSpeed, sv_accelerate.GetFloat() );
	Assert( mv->m_vecVelocity.z == 0.0f );

	// Clamp the players speed in x,y.
	float flNewSpeed = VectorLength( mv->m_vecVelocity );
	if ( flNewSpeed > mv->m_flMaxSpeed )
	{
		float flScale = ( mv->m_flMaxSpeed / flNewSpeed );
		mv->m_vecVelocity.x *= flScale;
		mv->m_vecVelocity.y *= flScale;
	}

	// Now reduce their backwards speed to some percent of max, if they are travelling backwards
	// unless they are under some minimum, to not penalize deployed snipers or heavies
	if ( tf_clamp_back_speed.GetFloat() < 1.0 && VectorLength( mv->m_vecVelocity ) > tf_clamp_back_speed_min.GetFloat() )
	{
		float flDot = DotProduct( vecForward, mv->m_vecVelocity );

		// are we moving backwards at all?
		if ( flDot < 0 )
		{
			Vector vecBackMove = vecForward * flDot;
			Vector vecRightMove = vecRight * DotProduct( vecRight, mv->m_vecVelocity );

			// clamp the back move vector if it is faster than max
			float flBackSpeed = VectorLength( vecBackMove );
			float flMaxBackSpeed = ( mv->m_flMaxSpeed * tf_clamp_back_speed.GetFloat() );

			if ( flBackSpeed > flMaxBackSpeed )
			{
				vecBackMove *= flMaxBackSpeed / flBackSpeed;
			}
			
			// reassemble velocity	
			mv->m_vecVelocity = vecBackMove + vecRightMove;
		}
	}

	// Add base velocity to the player's current velocity - base velocity = velocity from conveyors, etc.
	VectorAdd( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

	// Calculate the current speed and return if we are not really moving.
	float flSpeed = VectorLength( mv->m_vecVelocity );
	if ( flSpeed < 1.0f )
	{
		// I didn't remove the base velocity here since it wasn't moving us in the first place.
		mv->m_vecVelocity.Init();
		return;
	}

	// Calculate the destination.
	Vector vecDestination;
	vecDestination.x = mv->GetAbsOrigin().x + ( mv->m_vecVelocity.x * gpGlobals->frametime );
	vecDestination.y = mv->GetAbsOrigin().y + ( mv->m_vecVelocity.y * gpGlobals->frametime );	
	vecDestination.z = mv->GetAbsOrigin().z;

	// Try moving to the destination.
	trace_t trace;
	TracePlayerBBox( mv->GetAbsOrigin(), vecDestination, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	if ( trace.fraction == 1.0f )
	{
		// Made it to the destination (remove the base velocity).
		mv->SetAbsOrigin( trace.endpos );
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

		// Save the wish velocity.
		mv->m_outWishVel += ( vecWishDirection * flWishSpeed );

		// Try and keep the player on the ground.
		// NOTE YWB 7/5/07: Don't do this here, our version of CategorizePosition encompasses this test
		// StayOnGround();

		return;
	}

	// Now try and do a step move.
	StepMove( vecDestination, trace );

	// Remove base velocity.
	Vector baseVelocity = player->GetBaseVelocity();
	VectorSubtract( mv->m_vecVelocity, baseVelocity, mv->m_vecVelocity );

	// Save the wish velocity.
	mv->m_outWishVel += ( vecWishDirection * flWishSpeed );

	// Try and keep the player on the ground.
	// NOTE YWB 7/5/07: Don't do this here, our version of CategorizePosition encompasses this test
	// StayOnGround();

#if 0
	// Debugging!!!
	Vector vecTestVelocity = mv->m_vecVelocity;
	vecTestVelocity.z = 0.0f;
	float flTestSpeed = VectorLength( vecTestVelocity );
	if ( baseVelocity.IsZero() && ( flTestSpeed > ( mv->m_flMaxSpeed + 1.0f ) ) )
	{
		Msg( "Step Max Speed < %f\n", flTestSpeed );
	}

	if ( tf_showspeed.GetBool() )
	{
		Msg( "Speed=%f\n", flTestSpeed );
	}

#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameMovement::AirMove( void )
{
	int			i;
	Vector		wishvel;
	float		fmove, smove;
	Vector		wishdir;
	float		wishspeed;
	Vector forward, right, up;

	AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles

	// Copy movement amounts
	fmove = mv->m_flForwardMove;
	smove = mv->m_flSideMove;

	// Zero out z components of movement vectors
	forward[2] = 0;
	right[2]   = 0;
	VectorNormalize(forward);  // Normalize remainder of vectors
	VectorNormalize(right);    // 

	for (i=0 ; i<2 ; i++)       // Determine x and y parts of velocity
		wishvel[i] = forward[i]*fmove + right[i]*smove;
	wishvel[2] = 0;             // Zero out z part of velocity

	VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	//
	// clamp to server defined max speed
	//
	if ( wishspeed != 0 && (wishspeed > mv->m_flMaxSpeed))
	{
		VectorScale (wishvel, mv->m_flMaxSpeed/wishspeed, wishvel);
		wishspeed = mv->m_flMaxSpeed;
	}

	AirAccelerate( wishdir, wishspeed, sv_airaccelerate.GetFloat() );

	// Add in any base velocity to the current velocity.
	VectorAdd( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

	TryPlayerMove();

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
}

extern void TracePlayerBBoxForGround( const Vector& start, const Vector& end, const Vector& minsSrc,
							  const Vector& maxsSrc, IHandleEntity *player, unsigned int fMask,
							  int collisionGroup, trace_t& pm );


//-----------------------------------------------------------------------------
// This filter checks against buildable objects.
//-----------------------------------------------------------------------------
class CTraceFilterObject : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterObject, CTraceFilterSimple );

	CTraceFilterObject( const IHandleEntity *passentity, int collisionGroup );
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
};

CTraceFilterObject::CTraceFilterObject( const IHandleEntity *passentity, int collisionGroup ) :
BaseClass( passentity, collisionGroup )
{

}

bool CTraceFilterObject::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );

	if ( pEntity && pEntity->IsBaseObject() )
	{
		CBaseObject *pObject = static_cast<CBaseObject *>( pEntity );

		Assert( pObject );

		if ( pObject && pObject->ShouldPlayersAvoid() )
		{
			if ( pObject->GetOwner() == GetPassEntity() )
				return true;
		}
	}

	return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}

CBaseHandle CTFGameMovement::TestPlayerPosition( const Vector& pos, int collisionGroup, trace_t& pm )
{
	if( tf_solidobjects.GetBool() == false )
		return BaseClass::TestPlayerPosition( pos, collisionGroup, pm );

	Ray_t ray;
	ray.Init( pos, pos, GetPlayerMins(), GetPlayerMaxs() );
	
	CTraceFilterObject traceFilter( mv->m_nPlayerHandle.Get(), collisionGroup );
	enginetrace->TraceRay( ray, PlayerSolidMask(), &traceFilter, &pm );

	if ( (pm.contents & PlayerSolidMask()) && pm.m_pEnt )
	{
		return pm.m_pEnt->GetRefEHandle();
	}
	else
	{	
		return INVALID_EHANDLE_INDEX;
	}
}

//-----------------------------------------------------------------------------
// Traces player movement + position
//-----------------------------------------------------------------------------
void CTFGameMovement::TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm )
{
	if( tf_solidobjects.GetBool() == false )
		return BaseClass::TracePlayerBBox( start, end, fMask, collisionGroup, pm );

	Ray_t ray;
	ray.Init( start, end, GetPlayerMins(), GetPlayerMaxs() );
	
	CTraceFilterObject traceFilter( mv->m_nPlayerHandle.Get(), collisionGroup );

	enginetrace->TraceRay( ray, fMask, &traceFilter, &pm );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &input - 
//-----------------------------------------------------------------------------
void CTFGameMovement::CategorizePosition( void )
{
	// Observer.
	if ( player->IsObserver() )
		return;

	// Reset this each time we-recategorize, otherwise we have bogus friction when we jump into water and plunge downward really quickly
	player->m_surfaceFriction = 1.0f;

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	CheckWater();

	// If standing on a ladder we are not on ground.
	if ( player->GetMoveType() == MOVETYPE_LADDER )
	{
		SetGroundEntity( NULL );
		return;
	}

	// Check for a jump.
	if ( mv->m_vecVelocity.z > 250.0f )
	{
		SetGroundEntity( NULL );
		return;
	}

	// Calculate the start and end position.
	Vector vecStartPos = mv->GetAbsOrigin();
	Vector vecEndPos( mv->GetAbsOrigin().x, mv->GetAbsOrigin().y, ( mv->GetAbsOrigin().z - 2.0f ) );

	// NOTE YWB 7/5/07:  Since we're already doing a traceline here, we'll subsume the StayOnGround (stair debouncing) check into the main traceline we do here to see what we're standing on
	bool bUnderwater = ( player->GetWaterLevel() >= WL_Eyes );
	bool bMoveToEndPos = false;
	if ( player->GetMoveType() == MOVETYPE_WALK && 
		player->GetGroundEntity() != NULL && !bUnderwater )
	{
		// if walking and still think we're on ground, we'll extend trace down by stepsize so we don't bounce down slopes
		vecEndPos.z -= player->GetStepSize();
		bMoveToEndPos = true;
	}

	trace_t trace;
	TracePlayerBBox( vecStartPos, vecEndPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );

	// Steep plane, not on ground.
	if ( trace.plane.normal.z < 0.7f )
	{
		// Test four sub-boxes, to see if any of them would have found shallower slope we could actually stand on.
		TracePlayerBBoxForGround( vecStartPos, vecEndPos, GetPlayerMins(), GetPlayerMaxs(), mv->m_nPlayerHandle.Get(), PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );
		if ( trace.plane.normal[2] < 0.7f )
		{
			// Too steep.
			SetGroundEntity( NULL );
			if ( ( mv->m_vecVelocity.z > 0.0f ) && 
				( player->GetMoveType() != MOVETYPE_NOCLIP ) )
			{
				player->m_surfaceFriction = 0.25f;
			}
		}
		else
		{
			SetGroundEntity( &trace );
		}
	}
	else
	{
		// YWB:  This logic block essentially lifted from StayOnGround implementation
		if ( bMoveToEndPos &&
			!trace.startsolid &&				// not sure we need this check as fraction would == 0.0f?
			trace.fraction > 0.0f &&			// must go somewhere
			trace.fraction < 1.0f ) 			// must hit something
		{
			float flDelta = fabs( mv->GetAbsOrigin().z - trace.endpos.z );
			// HACK HACK:  The real problem is that trace returning that strange value 
			//  we can't network over based on bit precision of networking origins
			if ( flDelta > 0.5f * COORD_RESOLUTION )
			{
				Vector org = mv->GetAbsOrigin();
				org.z = trace.endpos.z;
				mv->SetAbsOrigin( org );
			}
		}
		SetGroundEntity( &trace );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameMovement::CheckWaterJump( void )
{
	Vector	flatforward;
	Vector	flatvelocity;
	float curspeed;

	// Jump button down?
	bool bJump = ( ( mv->m_nButtons & IN_JUMP ) != 0 );

	Vector forward, right;
	AngleVectors( mv->m_vecViewAngles, &forward, &right, NULL );  // Determine movement angles

	// Already water jumping.
	if (player->m_flWaterJumpTime)
		return;

	// Don't hop out if we just jumped in
	if (mv->m_vecVelocity[2] < -180)
		return; // only hop out if we are moving up

	// See if we are backing up
	flatvelocity[0] = mv->m_vecVelocity[0];
	flatvelocity[1] = mv->m_vecVelocity[1];
	flatvelocity[2] = 0;

	// Must be moving
	curspeed = VectorNormalize( flatvelocity );
	
#if 1
	// Copy movement amounts
	float fmove = mv->m_flForwardMove;
	float smove = mv->m_flSideMove;

	for ( int iAxis = 0; iAxis < 2; ++iAxis )
	{
		flatforward[iAxis] = forward[iAxis] * fmove + right[iAxis] * smove;
	}
#else
	// see if near an edge
	flatforward[0] = forward[0];
	flatforward[1] = forward[1];
#endif
	flatforward[2] = 0;
	VectorNormalize( flatforward );

	// Are we backing into water from steps or something?  If so, don't pop forward
	if ( curspeed != 0.0 && ( DotProduct( flatvelocity, flatforward ) < 0.0 ) && !bJump )
		return;

	Vector vecStart;
	// Start line trace at waist height (using the center of the player for this here)
 	vecStart= mv->GetAbsOrigin() + (GetPlayerMins() + GetPlayerMaxs() ) * 0.5;

	Vector vecEnd;
	VectorMA( vecStart, TF_WATERJUMP_FORWARD/*tf_waterjump_forward.GetFloat()*/, flatforward, vecEnd );
	
	trace_t tr;
	TracePlayerBBox( vecStart, vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, tr );
	if ( tr.fraction < 1.0 )		// solid at waist
	{
		IPhysicsObject *pPhysObj = tr.m_pEnt->VPhysicsGetObject();
		if ( pPhysObj )
		{
			if ( pPhysObj->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
				return;
		}

		vecStart.z = mv->GetAbsOrigin().z + player->GetViewOffset().z + WATERJUMP_HEIGHT; 
		VectorMA( vecStart, TF_WATERJUMP_FORWARD/*tf_waterjump_forward.GetFloat()*/, flatforward, vecEnd );
		VectorMA( vec3_origin, -50.0f, tr.plane.normal, player->m_vecWaterJumpVel );

		TracePlayerBBox( vecStart, vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, tr );
		if ( tr.fraction == 1.0 )		// open at eye level
		{
			// Now trace down to see if we would actually land on a standable surface.
			VectorCopy( vecEnd, vecStart );
			vecEnd.z -= 1024.0f;
			TracePlayerBBox( vecStart, vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, tr );
			if ( ( tr.fraction < 1.0f ) && ( tr.plane.normal.z >= 0.7 ) )
			{
				mv->m_vecVelocity[2] = TF_WATERJUMP_UP/*tf_waterjump_up.GetFloat()*/;		// Push up
				mv->m_nOldButtons |= IN_JUMP;		// Don't jump again until released
				player->AddFlag( FL_WATERJUMP );
				player->m_flWaterJumpTime = 2000.0f;	// Do this for 2 seconds
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameMovement::CheckFalling( void )
{
	// if we landed on the ground
	if ( player->GetGroundEntity() != NULL && !IsDead() )
	{
		// turn off the jumping flag if we're on ground after a jump
		if ( m_pTFPlayer->m_Shared.IsJumping() )
		{
			m_pTFPlayer->m_Shared.SetJumping( false );
		}
	}

	BaseClass::CheckFalling();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameMovement::Duck( void )
{
	// Don't allowing ducking in water.
	if ( ( ( player->GetWaterLevel() >= WL_Feet ) && ( player->GetGroundEntity() == NULL ) ) ||
		 player->GetWaterLevel() >= WL_Eyes )
	{
		mv->m_nButtons &= ~IN_DUCK;
	}

	BaseClass::Duck();
}
void CTFGameMovement::FullWalkMoveUnderwater()
{
	if ( player->GetWaterLevel() == WL_Waist )
	{
		CheckWaterJump();
	}

	// If we are falling again, then we must not trying to jump out of water any more.
	if ( ( mv->m_vecVelocity.z < 0.0f ) && player->m_flWaterJumpTime )
	{
		player->m_flWaterJumpTime = 0.0f;
	}

	// Was jump button pressed?
	if ( mv->m_nButtons & IN_JUMP )
	{
		CheckJumpButton();
	}
	else
	{
		mv->m_nOldButtons &= ~IN_JUMP;
	}

	// Perform regular water movement
	WaterMove();

	// Redetermine position vars
	CategorizePosition();

	// If we are on ground, no downward velocity.
	if ( player->GetGroundEntity() != NULL )
	{
		mv->m_vecVelocity[2] = 0;			
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameMovement::FullWalkMove()
{
	if ( !InWater() ) 
	{
		StartGravity();
	}

	// If we are leaping out of the water, just update the counters.
	if ( player->m_flWaterJumpTime )
	{
		// Try to jump out of the water (and check to see if we still are).
		WaterJump();
		TryPlayerMove();
		CheckWater();
		return;
	}

	// If we are swimming in the water, see if we are nudging against a place we can jump up out
	//  of, and, if so, start out jump.  Otherwise, if we are not moving up, then reset jump timer to 0
	if ( InWater() ) 
	{
		FullWalkMoveUnderwater();
		return;
	}

	if (mv->m_nButtons & IN_JUMP)
	{
		CheckJumpButton();
	}
	else
	{
		mv->m_nOldButtons &= ~IN_JUMP;
	}

	// Make sure velocity is valid.
	CheckVelocity();

	if (player->GetGroundEntity() != NULL)
	{
		mv->m_vecVelocity[2] = 0.0;
		Friction();
		WalkMove();
	}
	else
	{
		AirMove();
	}

	// Set final flags.
	CategorizePosition();

	// Add any remaining gravitational component if we are not in water.
	if ( !InWater() )
	{
		FinishGravity();
	}

	// If we are on ground, no downward velocity.
	if ( player->GetGroundEntity() != NULL )
	{
		mv->m_vecVelocity[2] = 0;
	}

	// Handling falling.
	CheckFalling();

	// Make sure velocity is valid.
	CheckVelocity();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameMovement::FullTossMove( void )
{
	trace_t pm;
	Vector move;

	// add velocity if player is moving 
	if ( (mv->m_flForwardMove != 0.0f) || (mv->m_flSideMove != 0.0f) || (mv->m_flUpMove != 0.0f))
	{
		Vector forward, right, up;
		float fmove, smove;
		Vector wishdir, wishvel;
		float wishspeed;
		int i;

		AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles

		// Copy movement amounts
		fmove = mv->m_flForwardMove;
		smove = mv->m_flSideMove;

		VectorNormalize (forward);  // Normalize remainder of vectors.
		VectorNormalize (right);    // 

		for (i=0 ; i<3 ; i++)       // Determine x and y parts of velocity
			wishvel[i] = forward[i]*fmove + right[i]*smove;

		wishvel[2] += mv->m_flUpMove;

		VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
		wishspeed = VectorNormalize(wishdir);

		//
		// Clamp to server defined max speed
		//
		if (wishspeed > mv->m_flMaxSpeed)
		{
			VectorScale (wishvel, mv->m_flMaxSpeed/wishspeed, wishvel);
			wishspeed = mv->m_flMaxSpeed;
		}

		// Set pmove velocity
		Accelerate ( wishdir, wishspeed, sv_accelerate.GetFloat() );
	}

	if ( mv->m_vecVelocity[2] > 0 )
	{
		SetGroundEntity( NULL );
	}

	// If on ground and not moving, return.
	if ( player->GetGroundEntity() != NULL )
	{
		if (VectorCompare(player->GetBaseVelocity(), vec3_origin) &&
			VectorCompare(mv->m_vecVelocity, vec3_origin))
			return;
	}

	CheckVelocity();

	// add gravity
	if ( player->GetMoveType() == MOVETYPE_FLYGRAVITY )
	{
		AddGravity();
	}

	// move origin
	// Base velocity is not properly accounted for since this entity will move again after the bounce without
	// taking it into account
	VectorAdd (mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);

	CheckVelocity();

	VectorScale (mv->m_vecVelocity, gpGlobals->frametime, move);
	VectorSubtract (mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);

	PushEntity( move, &pm );	// Should this clear basevelocity

	CheckVelocity();

	if (pm.allsolid)
	{	
		// entity is trapped in another solid
		SetGroundEntity( &pm );
		mv->m_vecVelocity.Init();
		return;
	}

	if ( pm.fraction != 1.0f )
	{
		PerformFlyCollisionResolution( pm, move );
	}

	// Check for in water
	CheckWater();
}

//-----------------------------------------------------------------------------
// Purpose: Does the basic move attempting to climb up step heights.  It uses
//          the mv->GetAbsOrigin() and mv->m_vecVelocity.  It returns a new
//          new mv->GetAbsOrigin(), mv->m_vecVelocity, and mv->m_outStepHeight.
//-----------------------------------------------------------------------------
void CTFGameMovement::StepMove( Vector &vecDestination, trace_t &trace )
{
	trace_t saveTrace;
	saveTrace = trace;

	Vector vecEndPos;
	VectorCopy( vecDestination, vecEndPos );

	Vector vecPos, vecVel;
	VectorCopy( mv->GetAbsOrigin(), vecPos );
	VectorCopy( mv->m_vecVelocity, vecVel );

	bool bLowRoad = false;
	bool bUpRoad = true;

	// First try the "high road" where we move up and over obstacles
	if ( player->m_Local.m_bAllowAutoMovement )
	{
		// Trace up by step height
		VectorCopy( mv->GetAbsOrigin(), vecEndPos );
		vecEndPos.z += player->m_Local.m_flStepSize + DIST_EPSILON;
		TracePlayerBBox( mv->GetAbsOrigin(), vecEndPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );
		if ( !trace.startsolid && !trace.allsolid )
		{
			mv->SetAbsOrigin( trace.endpos );
		}

		// Trace over from there
		TryPlayerMove();

		// Then trace back down by step height to get final position
		VectorCopy( mv->GetAbsOrigin(), vecEndPos );
		vecEndPos.z -= player->m_Local.m_flStepSize + DIST_EPSILON;
		TracePlayerBBox( mv->GetAbsOrigin(), vecEndPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );
		// If the trace ended up in empty space, copy the end over to the origin.
		if ( !trace.startsolid && !trace.allsolid )
		{
			mv->SetAbsOrigin( trace.endpos );
		}

		// If we are not on the standable ground any more or going the "high road" didn't move us at all, then we'll also want to check the "low road"
		if ( ( trace.fraction != 1.0f && 
			trace.plane.normal[2] < 0.7 ) || VectorCompare( mv->GetAbsOrigin(), vecPos ) )
		{
			bLowRoad = true;
			bUpRoad = false;
		}
	}
	else
	{
		bLowRoad = true;
		bUpRoad = false;
	}

	if ( bLowRoad )
	{
		// Save off upward results
		Vector vecUpPos, vecUpVel;
		if ( bUpRoad )
		{
			VectorCopy( mv->GetAbsOrigin(), vecUpPos );
			VectorCopy( mv->m_vecVelocity, vecUpVel );
		}

		// Take the "low" road
		mv->SetAbsOrigin( vecPos );
		VectorCopy( vecVel, mv->m_vecVelocity );
		VectorCopy( vecDestination, vecEndPos );
		TryPlayerMove( &vecEndPos, &saveTrace );

		// Down results.
		Vector vecDownPos, vecDownVel;
		VectorCopy( mv->GetAbsOrigin(), vecDownPos );
		VectorCopy( mv->m_vecVelocity, vecDownVel );

		if ( bUpRoad )
		{
			float flUpDist = ( vecUpPos.x - vecPos.x ) * ( vecUpPos.x - vecPos.x ) + ( vecUpPos.y - vecPos.y ) * ( vecUpPos.y - vecPos.y );
			float flDownDist = ( vecDownPos.x - vecPos.x ) * ( vecDownPos.x - vecPos.x ) + ( vecDownPos.y - vecPos.y ) * ( vecDownPos.y - vecPos.y );
	
			// decide which one went farther
			if ( flUpDist >= flDownDist )
			{
				mv->SetAbsOrigin( vecUpPos );
				VectorCopy( vecUpVel, mv->m_vecVelocity );

				// copy z value from the Low Road move
				mv->m_vecVelocity.z = vecDownVel.z;
			}
		}
	}

	float flStepDist = mv->GetAbsOrigin().z - vecPos.z;
	if ( flStepDist > 0 )
	{
		mv->m_outStepHeight += flStepDist;
	}
}

bool CTFGameMovement::GameHasLadders() const
{
	return false;
}

void CTFGameMovement::SetGroundEntity( trace_t *pm )
{
	BaseClass::SetGroundEntity( pm );
	if ( pm && pm->m_pEnt )
	{
		m_pTFPlayer->m_Shared.SetAirDash( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameMovement::PlayerRoughLandingEffects( float fvol )
{
	if ( m_pTFPlayer && m_pTFPlayer->IsPlayerClass(TF_CLASS_SCOUT) )
	{
		// Scouts don't play rumble unless they take damage.
		if ( fvol < 1.0 )
		{
			fvol = 0;
		}
	}

	BaseClass::PlayerRoughLandingEffects( fvol );
}
