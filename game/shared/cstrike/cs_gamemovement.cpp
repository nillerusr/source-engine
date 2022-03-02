//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "gamemovement.h"
#include "cs_gamerules.h"
#include "in_buttons.h"
#include "movevars_shared.h"
#include "weapon_csbase.h"

#ifdef CLIENT_DLL
	#include "c_cs_player.h"
#else
	#include "cs_player.h"
	#include "KeyValues.h"
#endif

#define STAMINA_MAX				100.0
#define STAMINA_COST_JUMP		25.0
#define STAMINA_COST_FALL		20.0
#define STAMINA_RECOVER_RATE	19.0

extern bool g_bMovementOptimizations;

ConVar sv_timebetweenducks( "sv_timebetweenducks", "0", FCVAR_REPLICATED, "Minimum time before recognizing consecutive duck key", true, 0.0, true, 2.0 );
ConVar sv_enableboost( "sv_enableboost", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Allow boost exploits");


class CCSGameMovement : public CGameMovement
{
public:
	DECLARE_CLASS( CCSGameMovement, CGameMovement );

	CCSGameMovement();

	virtual void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );
	virtual bool CanAccelerate();
	virtual bool CheckJumpButton( void );
	virtual void PreventBunnyJumping( void );
	virtual void ReduceTimers( void );
	virtual void WalkMove( void );
	virtual void AirMove( void );
	virtual bool LadderMove( void );
	virtual void DecayPunchAngle( void );
	virtual void CheckParameters( void );

	// allow overridden versions to respond to jumping
	virtual void	OnJump( float fImpulse );
	virtual void	OnLand( float fVelocity );

	// Ducking
	virtual void Duck( void );
	virtual void FinishUnDuck( void );
	virtual void FinishDuck( void );
	virtual bool CanUnduck();
	virtual void HandleDuckingSpeedCrop();


	virtual bool OnLadder( trace_t &trace );
	virtual float LadderDistance( void ) const
	{
		if ( player->GetMoveType() == MOVETYPE_LADDER )
			return 10.0f;
		return 2.0f;
	}

	virtual unsigned int LadderMask( void ) const
	{
		return MASK_PLAYERSOLID & ( ~CONTENTS_PLAYERCLIP );
	}

	virtual float ClimbSpeed( void ) const;
	virtual float LadderLateralMultiplier( void ) const;

	virtual void  TryTouchGround( const Vector& start, const Vector& end, const Vector& mins, const Vector& maxs, unsigned int fMask, int collisionGroup, trace_t& pm );


protected:
	virtual void PlayerMove();

	void CheckForLadders( bool wasOnGround );
	virtual unsigned int PlayerSolidMask( bool brushOnly = false );	///< returns the solid mask for the given player, so bots can have a more-restrictive set

	float m_fTimeLastUnducked;

public:
	CCSPlayer *m_pCSPlayer;
};

//-----------------------------------------------------------------------------
// Purpose: used by the TryTouchGround function to exclude non-standables from 
// consideration
//-----------------------------------------------------------------------------

bool CheckForStandable( IHandleEntity *pHandleEntity, int contentsMask )
{
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );

	if ( !pEntity )
		return false;

	return ( pEntity->IsPlayer() && pEntity->GetGroundEntity() != NULL ) || pEntity->IsStandable();
}


// Expose our interface.
static CCSGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameMovement, IGameMovement,INTERFACENAME_GAMEMOVEMENT, g_GameMovement );


// ---------------------------------------------------------------------------------------- //
// CCSGameMovement.
// ---------------------------------------------------------------------------------------- //

CCSGameMovement::CCSGameMovement()
{
	m_fTimeLastUnducked = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Allow bots etc to use slightly different solid masks
//-----------------------------------------------------------------------------
unsigned int CCSGameMovement::PlayerSolidMask( bool brushOnly )
{
	bool isBot = !player || player->IsBot();

	if ( brushOnly )
	{
		if ( isBot )
		{
			return MASK_PLAYERSOLID_BRUSHONLY | CONTENTS_MONSTERCLIP;
		}
		else
		{
			return MASK_PLAYERSOLID_BRUSHONLY;
		}
	}
	else
	{
		if ( isBot )
		{
			return MASK_PLAYERSOLID | CONTENTS_MONSTERCLIP;
		}
		else
		{
			return MASK_PLAYERSOLID;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameMovement::CheckParameters( void )
{
	QAngle	v_angle;

	// maintaining auto-duck during jumps
	if ( m_pCSPlayer->m_duckUntilOnGround )
	{
		if ( !player->GetGroundEntity() )
		{
			if ( mv->m_nButtons & IN_DUCK )
			{
				m_pCSPlayer->m_duckUntilOnGround = false; // player hit +duck, so cancel our auto duck
			}
			else
			{
				mv->m_nButtons |= IN_DUCK;
			}
		}
	}

	// it would be nice to put this into the player->GetPlayerMaxSpeed() method, but
	// this flag is only stored in the move!
	if ( mv->m_nButtons & IN_SPEED )
	{
		mv->m_flMaxSpeed *= CS_PLAYER_SPEED_WALK_MODIFIER;
	}

	if ( player->GetMoveType() != MOVETYPE_ISOMETRIC &&
		 player->GetMoveType() != MOVETYPE_NOCLIP &&
		 player->GetMoveType() != MOVETYPE_OBSERVER	)
	{
		float spd;

		spd = ( mv->m_flForwardMove * mv->m_flForwardMove ) +
			  ( mv->m_flSideMove * mv->m_flSideMove ) +
			  ( mv->m_flUpMove * mv->m_flUpMove );


		// Slow down by the speed factor
		float flSpeedFactor = 1.0f;
		if (player->m_pSurfaceData)
		{
			flSpeedFactor = player->m_pSurfaceData->game.maxSpeedFactor;
		}

		// If we have a constraint, slow down because of that too.
		float flConstraintSpeedFactor = ComputeConstraintSpeedFactor();
		if (flConstraintSpeedFactor < flSpeedFactor)
			flSpeedFactor = flConstraintSpeedFactor;

		// Take the player's velocity modifier into account
		if ( FBitSet( m_pCSPlayer->GetFlags(), FL_ONGROUND ) )
		{
			flSpeedFactor *= m_pCSPlayer->m_flVelocityModifier;
		}


		mv->m_flMaxSpeed *= flSpeedFactor;

		if ( g_bMovementOptimizations )
		{
			// Same thing but only do the sqrt if we have to.
			if ( ( spd != 0.0 ) && ( spd > mv->m_flMaxSpeed*mv->m_flMaxSpeed ) )
			{
				float fRatio = mv->m_flMaxSpeed / sqrt( spd );
				mv->m_flForwardMove *= fRatio;
				mv->m_flSideMove    *= fRatio;
				mv->m_flUpMove      *= fRatio;
			}
		}
		else
		{
			spd = sqrt( spd );
			if ( ( spd != 0.0 ) && ( spd > mv->m_flMaxSpeed ) )
			{
				float fRatio = mv->m_flMaxSpeed / spd;
				mv->m_flForwardMove *= fRatio;
				mv->m_flSideMove    *= fRatio;
				mv->m_flUpMove      *= fRatio;
			}
		}
	}


	if ( player->GetFlags() & FL_FROZEN ||
		 player->GetFlags() & FL_ONTRAIN || 
		 IsDead() )
	{
		mv->m_flForwardMove = 0;
		mv->m_flSideMove    = 0;
		mv->m_flUpMove      = 0;
	}

	DecayPunchAngle();

	// Take angles from command.
	if ( !IsDead() )
	{
		v_angle = mv->m_vecAngles;
		v_angle = v_angle + player->m_Local.m_vecPunchAngle;

		// Now adjust roll angle
		if ( player->GetMoveType() != MOVETYPE_ISOMETRIC  &&
			 player->GetMoveType() != MOVETYPE_NOCLIP )
		{
			mv->m_vecAngles[ROLL]  = CalcRoll( v_angle, mv->m_vecVelocity, sv_rollangle.GetFloat(), sv_rollspeed.GetFloat() );
		}
		else
		{
			mv->m_vecAngles[ROLL] = 0.0; // v_angle[ ROLL ];
		}
		mv->m_vecAngles[PITCH] = v_angle[PITCH];
		mv->m_vecAngles[YAW]   = v_angle[YAW];
	}
	else
	{
		mv->m_vecAngles = mv->m_vecOldAngles;
	}

	// Set dead player view_offset
	if ( IsDead() )
	{
		player->SetViewOffset( VEC_DEAD_VIEWHEIGHT );
	}

	// Adjust client view angles to match values used on server.
	if ( mv->m_vecAngles[YAW] > 180.0f )
	{
		mv->m_vecAngles[YAW] -= 360.0f;
	}

	// If we're standing on a player, then force them off.
	if ( !player->IsObserver()  && ( player->GetMoveType() != MOVETYPE_LADDER ) )
	{
		int nLevels = 0;
		CBaseEntity *pCurGround = player->GetGroundEntity();
		while ( pCurGround && pCurGround->IsPlayer() && nLevels < 1000 )
		{
			pCurGround = pCurGround->GetGroundEntity();
			++nLevels;
		}
		if ( nLevels == 1000 )
			Warning( "BUG: CCSGameMovement::CheckParameters - too many stacking levels.\n" );

		// If they're stacked too many levels deep, slide them off.
		if ( nLevels > 1 )
		{
			mv->m_flForwardMove = mv->m_flMaxSpeed * 3;
			mv->m_flSideMove = 0;
			mv->m_nButtons = 0;
			mv->m_nImpulseCommand = 0;
		}
	}
}


void CCSGameMovement::ProcessMovement( CBasePlayer *pBasePlayer, CMoveData *pMove )
{
	m_pCSPlayer = ToCSPlayer( pBasePlayer );
	Assert( m_pCSPlayer );

	BaseClass::ProcessMovement( pBasePlayer, pMove );
}


bool CCSGameMovement::CanAccelerate()
{
	// Only allow the player to accelerate when in certain states.
	CSPlayerState curState = m_pCSPlayer->State_Get();
	if ( curState == STATE_ACTIVE )
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


void CCSGameMovement::PlayerMove()
{
	if ( !m_pCSPlayer->CanMove() )
	{
		mv->m_flForwardMove = 0;
		mv->m_flSideMove = 0;
		mv->m_flUpMove = 0;
		mv->m_nButtons &= ~(IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);
	}

	BaseClass::PlayerMove();

	if ( FBitSet( m_pCSPlayer->GetFlags(), FL_ONGROUND ) )
	{
		if ( m_pCSPlayer->m_flVelocityModifier < 1.0 )
		{
			m_pCSPlayer->m_flVelocityModifier += gpGlobals->frametime / 3.0f;
		}

		if ( m_pCSPlayer->m_flVelocityModifier > 1.0 )
			 m_pCSPlayer->m_flVelocityModifier = 1.0;
	}

#if !defined(CLIENT_DLL)
	if ( m_pCSPlayer->IsAlive() )
	{
		// Check if our eye height is too close to the ceiling and lower it.
		// This is needed because we have taller models with the old collision bounds.

		const float eyeClearance = 12.0f; // eye pos must be this far below the ceiling

		Vector offset = player->GetViewOffset();

		Vector vHullMin = GetPlayerMins( player->m_Local.m_bDucked );
		vHullMin.z = 0.0f;
		Vector vHullMax = GetPlayerMaxs( player->m_Local.m_bDucked ); 

		Vector start = player->GetAbsOrigin();
		start.z += vHullMax.z;
		Vector end = start;
		end.z += eyeClearance - vHullMax.z; 
		end.z += player->m_Local.m_bDucked ? VEC_DUCK_VIEW_SCALED( player ).z : VEC_VIEW_SCALED( player ).z;

		vHullMax.z = 0.0f;

		Vector fudge( 1, 1, 0 );
		vHullMin += fudge;
		vHullMax -= fudge;

		trace_t trace;
		Ray_t ray;
		ray.Init( start, end, vHullMin, vHullMax );
		UTIL_TraceRay( ray, PlayerSolidMask(), mv->m_nPlayerHandle.Get(), COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

		if ( trace.fraction < 1.0f )
		{
			float est = start.z + trace.fraction * (end.z - start.z) - player->GetAbsOrigin().z - eyeClearance;
			if ( ( player->GetFlags() & FL_DUCKING ) == 0 && !player->m_Local.m_bDucking && !player->m_Local.m_bDucked )
			{
				offset.z = est;
			}
			else
			{
				offset.z = MIN( est, offset.z );
			}
			player->SetViewOffset( offset );
		}
		else
		{
			if ( ( player->GetFlags() & FL_DUCKING ) == 0 && !player->m_Local.m_bDucking && !player->m_Local.m_bDucked )
			{

				player->SetViewOffset( VEC_VIEW_SCALED( player ) );
			}
			else if ( m_pCSPlayer->m_duckUntilOnGround )
			{
				// Duck Hull, but we're in the air.  Calculate where the view would be.
				Vector hullSizeNormal = VEC_HULL_MAX_SCALED( player ) - VEC_HULL_MIN_SCALED( player );
				Vector hullSizeCrouch = VEC_DUCK_HULL_MAX_SCALED( player ) - VEC_DUCK_HULL_MIN_SCALED( player );

				// We've got the duck hull, pulled up to the top of where the player should be
				Vector lowerClearance = hullSizeNormal - hullSizeCrouch;
				Vector duckEyeHeight = GetPlayerViewOffset( false ) - lowerClearance;
				player->SetViewOffset( duckEyeHeight );
			}
			else if( player->m_Local.m_bDucked && !player->m_Local.m_bDucking )
			{
				player->SetViewOffset( VEC_DUCK_VIEW );
			}
		}
	}
#endif	
}


void CCSGameMovement::WalkMove( void )
{
	if ( m_pCSPlayer->m_flStamina > 0 )
	{
		float flRatio;

		flRatio = ( STAMINA_MAX - ( ( m_pCSPlayer->m_flStamina / 1000.0 ) * STAMINA_RECOVER_RATE ) ) / STAMINA_MAX;
		
		// This Goldsrc code was run with variable timesteps and it had framerate dependencies.
		// People looking at Goldsrc for reference are usually 
		// (these days) measuring the stoppage at 60fps or greater, so we need
		// to account for the fact that Goldsrc was applying more stopping power
		// since it applied the slowdown across more frames.
		float flReferenceFrametime = 1.0f / 70.0f;
		float flFrametimeRatio = gpGlobals->frametime / flReferenceFrametime;

		flRatio = pow( flRatio, flFrametimeRatio );

		mv->m_vecVelocity.x *= flRatio;
		mv->m_vecVelocity.y *= flRatio;
	}

	BaseClass::WalkMove();

	CheckForLadders( player->GetGroundEntity() != NULL );
}


//-------------------------------------------------------------------------------------------------------------------------------
void CCSGameMovement::AirMove( void )
{
	BaseClass::AirMove();

	CheckForLadders( false );
}


//-------------------------------------------------------------------------------------------------------------------------------
bool CCSGameMovement::OnLadder( trace_t &trace )
{
	if ( trace.plane.normal.z == 1.0f )
		return false;

	return BaseClass::OnLadder( trace );
}


//-------------------------------------------------------------------------------------------------------------------------------
bool CCSGameMovement::LadderMove( void )
{
	bool isOnLadder = BaseClass::LadderMove();
	if ( isOnLadder && m_pCSPlayer )
	{
		m_pCSPlayer->SurpressLadderChecks( mv->GetAbsOrigin(), m_pCSPlayer->m_vecLadderNormal );
	}

	return isOnLadder;
}


//-------------------------------------------------------------------------------------------------------------------------------
/**
 * In CS, crouching up ladders goes slowly and doesn't make a sound.
 */
float CCSGameMovement::ClimbSpeed( void ) const
{
	if ( mv->m_nButtons & IN_DUCK )
	{
		return BaseClass::ClimbSpeed() * CS_PLAYER_SPEED_CLIMB_MODIFIER;
	}
	else
	{
		return BaseClass::ClimbSpeed();
	}
}


//-------------------------------------------------------------------------------------------------------------------------------
/**
* In CS, strafing on ladders goes slowly.
*/
float CCSGameMovement::LadderLateralMultiplier( void ) const
{
	if ( mv->m_nButtons & IN_DUCK )
	{
		return 1.0f;
	}
	else
	{
		return 0.5f;
	}
}


//-------------------------------------------------------------------------------------------------------------------------------
/**
 * Looks behind and beneath the player in the air, in case he skips out over the top of a ladder.  If the
 * trace hits a ladder, the player is snapped to the ladder.
 */
void CCSGameMovement::CheckForLadders( bool wasOnGround )
{
	if ( !wasOnGround )
	{
		// If we're higher than the last place we were on the ground, bail - obviously we're not dropping
		// past a ladder we might want to grab.
		if ( mv->GetAbsOrigin().z > m_pCSPlayer->m_lastStandingPos.z )
			return;

		Vector dir = -m_pCSPlayer->m_lastStandingPos + mv->GetAbsOrigin();
		if ( !dir.x && !dir.y )
		{
			// If we're dropping straight down, we don't know which way to look for a ladder.  Oh well.
			return;
		}

		dir.z = 0.0f;
		float dist = dir.NormalizeInPlace();
		if ( dist > 64.0f )
		{
			// Don't grab ladders too far behind us.
			return;
		}

		trace_t trace;

		TracePlayerBBox(
			mv->GetAbsOrigin(),
			m_pCSPlayer->m_lastStandingPos - dir*(5+dist),
			(PlayerSolidMask() & (~CONTENTS_PLAYERCLIP)), COLLISION_GROUP_PLAYER_MOVEMENT, trace );

		if ( trace.fraction != 1.0f && OnLadder( trace ) && trace.plane.normal.z != 1.0f )
		{
			if ( m_pCSPlayer->CanGrabLadder( trace.endpos, trace.plane.normal ) )
			{
				player->SetMoveType( MOVETYPE_LADDER );
				player->SetMoveCollide( MOVECOLLIDE_DEFAULT );

				player->SetLadderNormal( trace.plane.normal );
				mv->m_vecVelocity.Init();

				// The ladder check ignored playerclips, to fix a bug exposed by de_train, where a clipbrush is
				// flush with a ladder.  This causes the above tracehull to fail unless we ignore playerclips.
				// However, we have to check for playerclips before we snap to that pos, so we don't warp a
				// player into a clipbrush.
				TracePlayerBBox(
					mv->GetAbsOrigin(),
					m_pCSPlayer->m_lastStandingPos - dir*(5+dist),
					PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );

				mv->SetAbsOrigin( trace.endpos );
			}
		}
	}
	else
	{
		m_pCSPlayer->m_lastStandingPos = mv->GetAbsOrigin();
	}
}


void CCSGameMovement::ReduceTimers( void )
{
	float frame_msec = 1000.0f * gpGlobals->frametime;

	if ( m_pCSPlayer->m_flStamina > 0 )
	{
		m_pCSPlayer->m_flStamina -= frame_msec;

		if ( m_pCSPlayer->m_flStamina < 0 )
		{
			m_pCSPlayer->m_flStamina = 0;
		}
	}

	BaseClass::ReduceTimers();
}

ConVar sv_enablebunnyhopping( "sv_enablebunnyhopping", "0", FCVAR_REPLICATED | FCVAR_NOTIFY );

// Only allow bunny jumping up to 1.1x server / player maxspeed setting
#define BUNNYJUMP_MAX_SPEED_FACTOR 1.1f

// taken from TF2 but changed BUNNYJUMP_MAX_SPEED_FACTOR from 1.1 to 1.0
void CCSGameMovement::PreventBunnyJumping()
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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGameMovement::CheckJumpButton( void )
{
	if (m_pCSPlayer->pl.deadflag)
	{
		mv->m_nOldButtons |= IN_JUMP ;	// don't jump again until released
		return false;
	}

	// See if we are waterjumping.  If so, decrement count and return.
	if (m_pCSPlayer->m_flWaterJumpTime)
	{
		m_pCSPlayer->m_flWaterJumpTime -= gpGlobals->frametime;
		if (m_pCSPlayer->m_flWaterJumpTime < 0)
			m_pCSPlayer->m_flWaterJumpTime = 0;
		
		return false;
	}

	// If we are in the water most of the way...
	if ( m_pCSPlayer->GetWaterLevel() >= 2 )
	{	
		// swimming, not jumping
		SetGroundEntity( NULL );

		if(m_pCSPlayer->GetWaterType() == CONTENTS_WATER)    // We move up a certain amount
			mv->m_vecVelocity[2] = 100;
		else if (m_pCSPlayer->GetWaterType() == CONTENTS_SLIME)
			mv->m_vecVelocity[2] = 80;
		
		// play swiming sound
		if ( m_pCSPlayer->m_flSwimSoundTime <= 0 )
		{
			// Don't play sound again for 1 second
			m_pCSPlayer->m_flSwimSoundTime = 1000;
			PlaySwimSound();
		}

		return false;
	}

	// No more effect
 	if (m_pCSPlayer->GetGroundEntity() == NULL)
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;		// in air, so no effect
	}

	if ( mv->m_nOldButtons & IN_JUMP )
		return false;		// don't pogo stick

	if ( !sv_enablebunnyhopping.GetBool() )
	{
		PreventBunnyJumping();
	}

	// In the air now.
	SetGroundEntity( NULL );
	
	m_pCSPlayer->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->m_pSurfaceData, 1.0, true );
	
	//MoveHelper()->PlayerSetAnimation( PLAYER_JUMP );
	m_pCSPlayer->DoAnimationEvent( PLAYERANIMEVENT_JUMP );

	float flGroundFactor = 1.0f;
	if (player->m_pSurfaceData)
	{
		flGroundFactor = player->m_pSurfaceData->game.jumpFactor; 
	}

	// if we weren't ducking, bots and hostages do a crouchjump programatically
	if ( (!player || player->IsBot()) && !(mv->m_nButtons & IN_DUCK) )
	{
		m_pCSPlayer->m_duckUntilOnGround = true;
		FinishDuck();
	}

	// Acclerate upward
	// If we are ducking...
	float startz = mv->m_vecVelocity[2];
	if ( m_pCSPlayer->m_duckUntilOnGround || (  m_pCSPlayer->m_Local.m_bDucking ) || (  m_pCSPlayer->GetFlags() & FL_DUCKING ) )
	{
		// d = 0.5 * g * t^2		- distance traveled with linear accel
		// t = sqrt(2.0 * 45 / g)	- how long to fall 45 units
		// v = g * t				- velocity at the end (just invert it to jump up that high)
		// v = g * sqrt(2.0 * 45 / g )
		// v^2 = g * g * 2.0 * 45 / g
		// v = sqrt( g * 2.0 * 45 )
		
		mv->m_vecVelocity[2] = flGroundFactor * sqrt(2 * 800 * 57.0);  // 2 * gravity * height
	}
	else
	{
		mv->m_vecVelocity[2] += flGroundFactor * sqrt(2 * 800 * 57.0);  // 2 * gravity * height
	}

	if ( m_pCSPlayer->m_flStamina > 0 )
	{
		float flRatio;

		flRatio = ( STAMINA_MAX - ( ( m_pCSPlayer->m_flStamina  / 1000.0 ) * STAMINA_RECOVER_RATE ) ) / STAMINA_MAX;

		mv->m_vecVelocity[2] *= flRatio;
	}

	m_pCSPlayer->m_flStamina = ( STAMINA_COST_JUMP / STAMINA_RECOVER_RATE ) * 1000.0;
	
	FinishGravity();

	mv->m_outWishVel.z += mv->m_vecVelocity[2] - startz;
	mv->m_outStepHeight += 0.1f;

	OnJump(mv->m_outWishVel.z);

#ifndef CLIENT_DLL
	// allow bots to react
	IGameEvent * event = gameeventmanager->CreateEvent( "player_jump" );
	if ( event )
	{
		event->SetInt( "userid", m_pCSPlayer->GetUserID() );
		gameeventmanager->FireEvent( event );
	}
#endif

	// Flag that we jumped.
	mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released
	return true;
}


void CCSGameMovement::DecayPunchAngle( void )
{
	float	len;

	Vector vPunchAngle;

	vPunchAngle.x = m_pCSPlayer->m_Local.m_vecPunchAngle->x;
	vPunchAngle.y = m_pCSPlayer->m_Local.m_vecPunchAngle->y;
	vPunchAngle.z = m_pCSPlayer->m_Local.m_vecPunchAngle->z;
	
	len = VectorNormalize ( vPunchAngle );
	len -= (10.0 + len * 0.5) * gpGlobals->frametime;
	len = MAX( len, 0.0 );
	VectorScale ( vPunchAngle, len, vPunchAngle );

	m_pCSPlayer->m_Local.m_vecPunchAngle.Set( 0, vPunchAngle.x );
	m_pCSPlayer->m_Local.m_vecPunchAngle.Set( 1, vPunchAngle.y );
	m_pCSPlayer->m_Local.m_vecPunchAngle.Set( 2, vPunchAngle.z );
}


void CCSGameMovement::HandleDuckingSpeedCrop()
{
	//=============================================================================
	// HPE_BEGIN:
	// [Forrest] 
	//=============================================================================
	// Movement speed in free look camera mode is unaffected by ducking state.
	if ( player->GetObserverMode() == OBS_MODE_ROAMING )
		return;
	//=============================================================================
	// HPE_END
	//=============================================================================

	if ( !( m_iSpeedCropped & SPEED_CROPPED_DUCK ) )
	{
		if ( ( mv->m_nButtons & IN_DUCK ) || ( player->m_Local.m_bDucking ) || ( player->GetFlags() & FL_DUCKING ) )
		{
			mv->m_flForwardMove	*= CS_PLAYER_SPEED_DUCK_MODIFIER;
			mv->m_flSideMove	*= CS_PLAYER_SPEED_DUCK_MODIFIER;
			mv->m_flUpMove		*= CS_PLAYER_SPEED_DUCK_MODIFIER;
			m_iSpeedCropped		|= SPEED_CROPPED_DUCK;
		}
	}
}

bool CCSGameMovement::CanUnduck()
{
	trace_t trace;
	Vector newOrigin;

	VectorCopy( mv->GetAbsOrigin(), newOrigin );

	if ( player->GetGroundEntity() != NULL )
	{
		newOrigin += VEC_DUCK_HULL_MIN_SCALED( player ) - VEC_HULL_MIN_SCALED( player );
	}
	else
	{
		// If in air an letting go of croush, make sure we can offset origin to make
		//  up for uncrouching
 		Vector hullSizeNormal = VEC_HULL_MAX_SCALED( player ) - VEC_HULL_MIN_SCALED( player );
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX_SCALED( player ) - VEC_DUCK_HULL_MIN_SCALED( player );
		
		newOrigin += -0.5f * ( hullSizeNormal - hullSizeCrouch );
	}

	UTIL_TraceHull( mv->GetAbsOrigin(), newOrigin, VEC_HULL_MIN_SCALED( player ), VEC_HULL_MAX_SCALED( player ), PlayerSolidMask(), player, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

	if ( trace.startsolid || ( trace.fraction != 1.0f ) )
		return false;	

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Stop ducking
//-----------------------------------------------------------------------------
void CCSGameMovement::FinishUnDuck( void )
{
	trace_t trace;
	Vector newOrigin;

	VectorCopy( mv->GetAbsOrigin(), newOrigin );

	if ( player->GetGroundEntity() != NULL )
	{
		newOrigin += VEC_DUCK_HULL_MIN_SCALED( player ) - VEC_HULL_MIN_SCALED( player );
	}
	else
	{
		// If in air an letting go of croush, make sure we can offset origin to make
		//  up for uncrouching
 		Vector hullSizeNormal = VEC_HULL_MAX_SCALED( player ) - VEC_HULL_MIN_SCALED( player );
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX_SCALED( player ) - VEC_DUCK_HULL_MIN_SCALED( player );

		Vector viewDelta = -0.5f * ( hullSizeNormal - hullSizeCrouch );

		VectorAdd( newOrigin, viewDelta, newOrigin );
	}

	player->m_Local.m_bDucked = false;
	player->RemoveFlag( FL_DUCKING );
	player->m_Local.m_bDucking  = false;
	player->SetViewOffset( GetPlayerViewOffset( false ) );
	player->m_Local.m_flDucktime = 0;
	
	mv->SetAbsOrigin( newOrigin );

	// Recategorize position since ducking can change origin
	CategorizePosition();
}

//-----------------------------------------------------------------------------
// Purpose: Finish ducking
//-----------------------------------------------------------------------------
void CCSGameMovement::FinishDuck( void )
{
	
	Vector hullSizeNormal = VEC_HULL_MAX_SCALED( player ) - VEC_HULL_MIN_SCALED( player );
	Vector hullSizeCrouch = VEC_DUCK_HULL_MAX_SCALED( player ) - VEC_DUCK_HULL_MIN_SCALED( player );

	Vector viewDelta = 0.5f * ( hullSizeNormal - hullSizeCrouch );

	player->SetViewOffset( GetPlayerViewOffset( true ) );
	player->AddFlag( FL_DUCKING );
	player->m_Local.m_bDucking = false;

	if ( !player->m_Local.m_bDucked )
	{
	
		Vector org = mv->GetAbsOrigin();

		if ( player->GetGroundEntity() != NULL )
		{
			org -= VEC_DUCK_HULL_MIN_SCALED( player ) - VEC_HULL_MIN_SCALED( player );
		}
		else
		{
			org += viewDelta;
		}
		mv->SetAbsOrigin( org );

		player->m_Local.m_bDucked = true;
	}

	// See if we are stuck?
	FixPlayerCrouchStuck( true );

	// Recategorize position since ducking can change origin
	CategorizePosition();
}

//-----------------------------------------------------------------------------
// Purpose: See if duck button is pressed and do the appropriate things
//-----------------------------------------------------------------------------
void CCSGameMovement::Duck( void )
{

	// Fix taken from zblock for rapid crouch/stand not showing stand on other clients

	if ( player->GetFlags() & FL_ONGROUND )
	{
		// if prevent crouch
		if ( !( mv->m_nButtons & IN_DUCK ) && ( mv->m_nOldButtons & IN_DUCK ) )
		{
			// Player has released crouch and moving to standing
			m_fTimeLastUnducked = gpGlobals->curtime;
		}
		else if ( ( mv->m_nButtons & IN_DUCK ) && !( mv->m_nOldButtons & IN_DUCK ) )
		{
			// Crouch from standing
			if ( ( player->GetFlags() & FL_DUCKING )
				&& ( m_fTimeLastUnducked > (gpGlobals->curtime - sv_timebetweenducks.GetFloat() ) ) )
			{
				// if the server thinks the player is still crouched
				// AND the time the player started to stand (from being ducked) was less than 300ms ago
				// prevent the player from ducking again
				mv->m_nButtons &= ~IN_DUCK;
			}
		}
	}

	int buttonsChanged	= ( mv->m_nOldButtons ^ mv->m_nButtons );	// These buttons have changed this frame
	int buttonsPressed	=  buttonsChanged & mv->m_nButtons;			// The changed ones still down are "pressed"
	int buttonsReleased	=  buttonsChanged & mv->m_nOldButtons;		// The changed ones which were previously down are "released"

	// Check to see if we are in the air.
	bool bInAir = player->GetGroundEntity() == NULL && player->GetMoveType() != MOVETYPE_LADDER;

	if ( mv->m_nButtons & IN_DUCK )
	{
		mv->m_nOldButtons |= IN_DUCK;
	}
	else
	{
		mv->m_nOldButtons &= ~IN_DUCK;
	}

	if ( IsDead() )
	{
		// Unduck
		if ( player->GetFlags() & FL_DUCKING )
		{
			FinishUnDuck();
		}
		return;
	}

	HandleDuckingSpeedCrop();

	if ( m_pCSPlayer->m_duckUntilOnGround )
	{
		if ( !bInAir )
		{
			m_pCSPlayer->m_duckUntilOnGround = false;
			if ( CanUnduck() )
			{
				FinishUnDuck();
			}
			return;
		}
		else
		{
			if ( mv->m_vecVelocity.z > 0.0f )
				return;

			// Check if we can un-duck.  We want to unduck if we have space for the standing hull, and
			// if it is less than 2 inches off the ground.
			trace_t trace;
			Vector newOrigin;
			Vector groundCheck;

			VectorCopy( mv->GetAbsOrigin(), newOrigin );
			Vector hullSizeNormal = VEC_HULL_MAX_SCALED( player ) - VEC_HULL_MIN_SCALED( player );
			Vector hullSizeCrouch = VEC_DUCK_HULL_MAX_SCALED( player ) - VEC_DUCK_HULL_MIN_SCALED( player );
			newOrigin -= ( hullSizeNormal - hullSizeCrouch );
			groundCheck = newOrigin;
			groundCheck.z -= player->GetStepSize();

			UTIL_TraceHull( newOrigin, groundCheck, VEC_HULL_MIN_SCALED( player ), VEC_HULL_MAX_SCALED( player ), PlayerSolidMask(), player, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

			if ( trace.startsolid || trace.fraction == 1.0f )
				return; // Can't even stand up, or there's no ground underneath us

			m_pCSPlayer->m_duckUntilOnGround = false;
			if ( CanUnduck() )
			{
				FinishUnDuck();
			}
			return;
		}
	}

	// Holding duck, in process of ducking or fully ducked?
	if ( ( mv->m_nButtons & IN_DUCK ) || ( player->m_Local.m_bDucking ) || ( player->GetFlags() & FL_DUCKING ) )
	{
		if ( mv->m_nButtons & IN_DUCK )
		{
			bool alreadyDucked = ( player->GetFlags() & FL_DUCKING ) ? true : false;

			if ( (buttonsPressed & IN_DUCK ) && !( player->GetFlags() & FL_DUCKING ) )
			{
				// Use 1 second so super long jump will work
				player->m_Local.m_flDucktime = 1000;
				player->m_Local.m_bDucking    = true;
			}

			float duckmilliseconds = MAX( 0.0f, 1000.0f - (float)player->m_Local.m_flDucktime );
			float duckseconds = duckmilliseconds / 1000.0f;

			//time = MAX( 0.0, ( 1.0 - (float)player->m_Local.m_flDucktime / 1000.0 ) );
			
			if ( player->m_Local.m_bDucking )
			{
				if ( !( player->GetFlags() & FL_ANIMDUCKING ) )
					player->AddFlag( FL_ANIMDUCKING );

				// Finish ducking immediately if duck time is over or not on ground
				if ( ( duckseconds > TIME_TO_DUCK ) || 
					( player->GetGroundEntity() == NULL ) ||
					alreadyDucked)
				{
					FinishDuck();
				}
				else
				{
					// Calc parametric time
					float duckFraction = SimpleSpline( duckseconds / TIME_TO_DUCK );
					SetDuckedEyeOffset( duckFraction );
				}
			}
		}
		else
		{
			// Try to unduck unless automovement is not allowed
			// NOTE: When not onground, you can always unduck
			if ( player->m_Local.m_bAllowAutoMovement || player->GetGroundEntity() == NULL )
			{
				if ( (buttonsReleased & IN_DUCK ) && ( player->GetFlags() & FL_DUCKING ) )
				{
					// Use 1 second so super long jump will work
					player->m_Local.m_flDucktime = 1000;
					player->m_Local.m_bDucking    = true;  // or unducking
				}

				float duckmilliseconds = MAX( 0.0f, 1000.0f - (float)player->m_Local.m_flDucktime );
				float duckseconds = duckmilliseconds / 1000.0f;

				if ( CanUnduck() )
				{
					if ( player->m_Local.m_bDucking || 
						 player->m_Local.m_bDucked ) // or unducking
					{
						if ( player->GetFlags() & FL_ANIMDUCKING )
							player->RemoveFlag( FL_ANIMDUCKING );

						// Finish ducking immediately if duck time is over or not on ground
						if ( ( duckseconds > TIME_TO_UNDUCK ) ||
							 ( player->GetGroundEntity() == NULL ) )
						{
							FinishUnDuck();
						}
						else
						{
							// Calc parametric time
							float duckFraction = SimpleSpline( 1.0f - ( duckseconds / TIME_TO_UNDUCK ) );
							SetDuckedEyeOffset( duckFraction );
						}
					}
				}
				else
				{
					// Still under something where we can't unduck, so make sure we reset this timer so
					//  that we'll unduck once we exit the tunnel, etc.
					player->m_Local.m_flDucktime	= 1000;
				}
			}
		}
	}
}


void CCSGameMovement::OnJump( float fImpulse )
{
	m_pCSPlayer->OnJump( fImpulse );
}	

void CCSGameMovement::OnLand( float fVelocity )
{
	m_pCSPlayer->OnLand( fVelocity );
}

//-----------------------------------------------------------------------------
// Purpose: Essentially the same as TracePlayerBBox, but adds a callback to 
// exclude entities that are not standable (except for other players)
//-----------------------------------------------------------------------------
void  CCSGameMovement::TryTouchGround( const Vector& start, const Vector& end, const Vector& mins, const Vector& maxs, unsigned int fMask, int collisionGroup, trace_t& pm )
{
	VPROF( "CCSGameMovement::TryTouchGround" );

	Ray_t ray;
	ray.Init( start, end, mins, maxs );

	ShouldHitFunc_t pStandingTestCallback = sv_enableboost.GetBool() ? NULL : CheckForStandable;

	UTIL_TraceRay( ray, fMask, mv->m_nPlayerHandle.Get(), collisionGroup, &pm,  pStandingTestCallback );

}
