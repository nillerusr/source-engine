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
#include "dod_gamerules.h"
#include "dod_shareddefs.h"
#include "in_buttons.h"
#include "movevars_shared.h"

#include "weapon_dodsniper.h"
#include "weapon_dodbaserpg.h"
#include "weapon_dodsemiauto.h"


#ifdef CLIENT_DLL
	#include "c_dod_player.h"
#else
	#include "dod_player.h"
#endif

extern bool g_bMovementOptimizations;

class CDODGameMovement : public CGameMovement
{
public:
	DECLARE_CLASS( CDODGameMovement, CGameMovement );

	CDODGameMovement();
	virtual ~CDODGameMovement();

	void SetPlayerSpeed( void );

	virtual void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );
	virtual bool CanAccelerate();
	virtual bool CheckJumpButton( void );
	virtual void ReduceTimers( void );
	virtual void WalkMove( void );
	virtual void AirMove( void );
	virtual void CheckParameters( void );
	virtual void CheckFalling( void );

	// Ducking
	virtual void Duck( void );
	virtual void FinishUnDuck( void );
	virtual void FinishDuck( void );
	virtual void HandleDuckingSpeedCrop();
	void SetDODDuckedEyeOffset( float duckFraction );
	void SetDeployedEyeOffset( void );

	// Prone
	void SetProneEyeOffset( float proneFraction );
	void FinishProne( void );
	void FinishUnProne( void );
	bool CanUnprone();

	virtual Vector	GetPlayerMins( void ) const; // uses local player
	virtual Vector	GetPlayerMaxs( void ) const; // uses local player

	// IGameMovement interface
	virtual Vector	GetPlayerMins( bool ducked ) const { return BaseClass::GetPlayerMins(ducked); }
	virtual Vector	GetPlayerMaxs( bool ducked ) const { return BaseClass::GetPlayerMaxs(ducked); }
	virtual Vector	GetPlayerViewOffset( bool ducked ) const { return BaseClass::GetPlayerViewOffset(ducked); }

	void ViewOffsetAnimation( Vector vecDest, float flTime, ViewAnimationType type );

	void SetViewOffset( Vector vecViewOffset );

	virtual unsigned int PlayerSolidMask( bool brushOnly = false );

protected:
	virtual void PlayerMove();

	void CheckForLadders( bool wasOnGround );
	bool ResolveStanding( void );
	void TracePlayerBBoxWithStep( const Vector &vStart, const Vector &vEnd, unsigned int fMask, int collisionGroup, trace_t &trace );

public:
	CDODPlayer *m_pDODPlayer;
	bool m_bUnProneToDuck;
};


// Expose our interface.
static CDODGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameMovement, IGameMovement,INTERFACENAME_GAMEMOVEMENT, g_GameMovement );


// ---------------------------------------------------------------------------------------- //
// CDODGameMovement.
// ---------------------------------------------------------------------------------------- //

CDODGameMovement::CDODGameMovement()
{
	// Don't set any member variables here, or you'll get an access
	// violation exception on LoadLibrary, and will have to stay up til 
	// 3 in the morning figuring out where you did bad things.

	m_bUnProneToDuck = false;
}

CDODGameMovement::~CDODGameMovement()
{
}

void CDODGameMovement::SetPlayerSpeed( void )
{
	if( DODGameRules()->State_Get() == STATE_PREROUND )
	{
		mv->m_flClientMaxSpeed = PLAYER_SPEED_FROZEN;
		return;
	}

	if ( m_pDODPlayer->m_Shared.IsPlanting() ||
		m_pDODPlayer->m_Shared.IsDefusing() )
	{
		mv->m_flClientMaxSpeed = PLAYER_SPEED_FROZEN;
		return;
	}

	bool bZoomed = ( m_pDODPlayer->GetFOV() < m_pDODPlayer->GetDefaultFOV() );
	bool bBazookaDeployed = false;
	bool bZoomingIn = false;
	
	CWeaponDODBase *pWpn = m_pDODPlayer->GetActiveDODWeapon();
	if( pWpn )
	{
		if( pWpn->GetDODWpnData().m_WeaponType == WPN_TYPE_BAZOOKA )
		{
			CDODBaseRocketWeapon *pBazooka = (CDODBaseRocketWeapon *)pWpn;
			bBazookaDeployed = pBazooka->ShouldPlayerBeSlow();
		}

		if ( pWpn->GetDODWpnData().m_WeaponType == WPN_TYPE_SNIPER )
		{
			CDODSniperWeapon *pSniper = dynamic_cast<CDODSniperWeapon *>( pWpn );
			if ( pSniper )
			{
				bZoomingIn = !bZoomed && pSniper->IsZoomingIn();
			}
		}
	}

	// are we zooming?

	if ( m_pDODPlayer->m_Shared.IsInMGDeploy() ) 
	{
		mv->m_flClientMaxSpeed = PLAYER_SPEED_FROZEN;
	}
	else if ( m_pDODPlayer->m_Shared.IsProne() && 
			 !m_pDODPlayer->m_Shared.IsGettingUpFromProne() &&
			 m_pDODPlayer->GetGroundEntity() != NULL )
	{
		if ( bZoomed )
			mv->m_flClientMaxSpeed = PLAYER_SPEED_PRONE_ZOOMED;
		else if ( bBazookaDeployed )
			mv->m_flClientMaxSpeed = PLAYER_SPEED_PRONE_BAZOOKA_DEPLOYED;
		else
			mv->m_flClientMaxSpeed = PLAYER_SPEED_PRONE;		//Base prone speed 
	}
	else	//not prone, dead or deployed - standing or crouching and possibly moving
	{
		float stamina = m_pDODPlayer->m_Shared.GetStamina();

		if ( bZoomed )
		{
			mv->m_flClientMaxSpeed = PLAYER_SPEED_ZOOMED;	
		}
		else if ( bBazookaDeployed )
		{
			mv->m_flClientMaxSpeed = PLAYER_SPEED_BAZOOKA_DEPLOYED;
		}
		else if ( mv->m_nButtons & IN_DUCK )
		{
			mv->m_flClientMaxSpeed = PLAYER_SPEED_RUN;	//gets cut in fraction later
		}
		// check for slowed from leg hit or firing a machine gun
		else if ( m_pDODPlayer->m_Shared.GetSlowedTime() > gpGlobals->curtime )
		{
			mv->m_flClientMaxSpeed = PLAYER_SPEED_SLOWED;
		}
		else
		{
			float flMaxSpeed;	

			if ( ( mv->m_nButtons & IN_SPEED ) && ( stamina > 0 ) && ( mv->m_nButtons & IN_FORWARD ) && !bZoomingIn )
			{
				flMaxSpeed = PLAYER_SPEED_SPRINT;	//sprinting
			}
			else
			{
				flMaxSpeed = PLAYER_SPEED_RUN;	//jogging
			}

			mv->m_flClientMaxSpeed = flMaxSpeed - 100 + stamina;
		}
	}

	if ( m_pDODPlayer->GetGroundEntity() != NULL )
	{
		if( m_pDODPlayer->m_Shared.IsGoingProne() )
		{
			float pronetime = m_pDODPlayer->m_Shared.m_flGoProneTime - gpGlobals->curtime;

			//interp to prone speed
			float flProneFraction = SimpleSpline( pronetime / TIME_TO_PRONE );

			float maxSpeed = mv->m_flClientMaxSpeed;

			if ( m_bUnProneToDuck )
				maxSpeed *= 0.33;
			
			mv->m_flClientMaxSpeed = ( ( 1 - flProneFraction ) * PLAYER_SPEED_PRONE ) + ( flProneFraction * maxSpeed );
		}
		else if( m_pDODPlayer->m_Shared.IsGettingUpFromProne() )
		{
			float pronetime = m_pDODPlayer->m_Shared.m_flUnProneTime - gpGlobals->curtime;

			//interp to regular speed speed
			float flProneFraction = SimpleSpline( pronetime / TIME_TO_PRONE );
			
			float maxSpeed = mv->m_flClientMaxSpeed;

			if ( m_bUnProneToDuck )
				maxSpeed *= 0.33;

			mv->m_flClientMaxSpeed = ( flProneFraction * PLAYER_SPEED_PRONE ) + ( ( 1 - flProneFraction ) * maxSpeed );
		}
	}	
}

ConVar cl_show_speed( "cl_show_speed", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "spam console with local player speed" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODGameMovement::CheckParameters( void )
{
	QAngle	v_angle;

	SetPlayerSpeed();

	if ( player->GetMoveType() != MOVETYPE_ISOMETRIC &&
		 player->GetMoveType() != MOVETYPE_NOCLIP &&
		 player->GetMoveType() != MOVETYPE_OBSERVER	)
	{
		float spd;
		float maxspeed;

		spd = ( mv->m_flForwardMove * mv->m_flForwardMove ) +
			  ( mv->m_flSideMove * mv->m_flSideMove ) +
			  ( mv->m_flUpMove * mv->m_flUpMove );

		maxspeed = mv->m_flClientMaxSpeed;
		if ( maxspeed != 0.0 )
		{
			mv->m_flMaxSpeed = MIN( maxspeed, mv->m_flMaxSpeed );
		}

		// Slow down by the speed factor
		float flSpeedFactor = 1.0f;
		if ( player->GetSurfaceData() )
		{
			flSpeedFactor = player->GetSurfaceData()->game.maxSpeedFactor;
		}

		// If we have a constraint, slow down because of that too.
		float flConstraintSpeedFactor = ComputeConstraintSpeedFactor();
		if (flConstraintSpeedFactor < flSpeedFactor)
			flSpeedFactor = flConstraintSpeedFactor;

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
		SetViewOffset( VEC_DEAD_VIEWHEIGHT_SCALED( player ) );
	}

	// Adjust client view angles to match values used on server.
	if ( mv->m_vecAngles[YAW] > 180.0f )
	{
		mv->m_vecAngles[YAW] -= 360.0f;
	}

	if ( cl_show_speed.GetBool() )
	{
		Vector vel = m_pDODPlayer->GetAbsVelocity();
		float actual_speed = sqrt( vel.x * vel.x + vel.y * vel.y );
		Msg( "player speed %.1f\n",actual_speed );
	}
}

void CDODGameMovement::CheckFalling( void )
{
	// if we landed on the ground
	if ( player->GetGroundEntity() != NULL && !IsDead() )
	{
		if ( player->m_Local.m_flFallVelocity >= 300 )
		{
			CPASFilter filter( player->GetAbsOrigin() );
			filter.UsePredictionRules();
			player->EmitSound( filter, player->entindex(), "Player.JumpLanding" );
		}

		// turn off the jumping flag if we're on ground after a jump
		if ( m_pDODPlayer->m_Shared.IsJumping() )
		{
			m_pDODPlayer->m_Shared.SetJumping( false );

			// if we landed from a jump, slow us
			if ( player->m_Local.m_flFallVelocity > 50 )
                m_pDODPlayer->m_Shared.SetSlowedTime( 0.5 );
		}
	}

	BaseClass::CheckFalling();
}


void CDODGameMovement::ProcessMovement( CBasePlayer *pBasePlayer, CMoveData *pMove )
{
	//Store the player pointer
	m_pDODPlayer = ToDODPlayer( pBasePlayer );
	Assert( m_pDODPlayer );

	m_pDODPlayer->m_Shared.ViewAnimThink();

	BaseClass::ProcessMovement( pBasePlayer, pMove );
}

bool CDODGameMovement::CanAccelerate()
{
	// Only allow the player to accelerate when in certain states.
	DODPlayerState curState = m_pDODPlayer->State_Get();
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

void CDODGameMovement::PlayerMove()
{
	BaseClass::PlayerMove();

	if ( player->GetMoveType() != MOVETYPE_ISOMETRIC &&
		 player->GetMoveType() != MOVETYPE_NOCLIP &&
		 player->GetMoveType() != MOVETYPE_OBSERVER	)
	{

		// Cap actual player speed, fix wall running
		float spd = mv->m_vecVelocity[0] * mv->m_vecVelocity[0] + 
					mv->m_vecVelocity[1] * mv->m_vecVelocity[1];

		if( spd > 0.0 && spd > ( mv->m_flMaxSpeed * mv->m_flMaxSpeed ) )
		{
			float fRatio = mv->m_flMaxSpeed / sqrt( spd );
			mv->m_vecVelocity[0] *= fRatio;
			mv->m_vecVelocity[1] *= fRatio;
		}
	}
}


void CDODGameMovement::WalkMove( void )
{
	float flSpeed = m_pDODPlayer->GetAbsVelocity().Length2D();

	bool bSprintButtonPressed = ( mv->m_nButtons & IN_SPEED ) > 0;

	if( bSprintButtonPressed && 
		( mv->m_nButtons & IN_FORWARD ) &&
		!m_pDODPlayer->m_Shared.IsProne() && 
		!m_pDODPlayer->m_Shared.IsDucking() &&
		flSpeed > 80 )
	{
		m_pDODPlayer->SetSprinting( true );
	}
	else
	{
		m_pDODPlayer->SetSprinting( false );
	}

	BaseClass::WalkMove();

	//CheckForLadders( true );
	//ResolveStanding();
}

//-----------------------------------------------------------------------------
// Purpose: Allow bots etc to use slightly different solid masks
//-----------------------------------------------------------------------------
unsigned int CDODGameMovement::PlayerSolidMask( bool brushOnly )
{
	int mask = 0;

	switch ( m_pDODPlayer->GetTeamNumber() )
	{
	case TEAM_ALLIES:
		mask = CONTENTS_TEAM1;
		break;

	case TEAM_AXIS:
		mask = CONTENTS_TEAM2;
		break;
	}

	return ( mask | BaseClass::PlayerSolidMask( brushOnly ) );
}

void CDODGameMovement::AirMove( void )
{
	BaseClass::AirMove();

	//CheckForLadders( false );
}

void CDODGameMovement::CheckForLadders( bool wasOnGround )
{
	if ( !wasOnGround || !ResolveStanding() )
	{
		trace_t trace;
		TracePlayerBBox( mv->GetAbsOrigin(), mv->GetAbsOrigin() + Vector( 0, 0, -5), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
		if ( trace.fraction == 1.0f )
		{
			Vector vel = -m_pDODPlayer->m_lastStandingPos + mv->GetAbsOrigin();
			if ( !vel.x && !vel.y )
			{
				return;
			}
			vel.NormalizeInPlace();
			vel *= 5.0f;
			Vector vecStartPos( mv->GetAbsOrigin().x + vel.x, mv->GetAbsOrigin().y + vel.y, mv->GetAbsOrigin().z );
			vel *= 5.0f;
			Vector vecStandPos( mv->GetAbsOrigin().x - vel.x, mv->GetAbsOrigin().y - vel.y, mv->GetAbsOrigin().z - ( player->m_Local.m_flStepSize * 1.0f ) );

			TracePlayerBBoxWithStep( vecStartPos, vecStandPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );

			if ( trace.fraction != 1.0f && OnLadder( trace ) && trace.plane.normal.z != 1.0f )
			{
				if ( trace.fraction > 0.6 )
				{
					vel.NormalizeInPlace();
					Vector org = mv->GetAbsOrigin();
					org -= vel*5;
					mv->SetAbsOrigin( org );
				}
				player->SetMoveType( MOVETYPE_LADDER );
				player->SetMoveCollide( MOVECOLLIDE_DEFAULT );

				player->SetLadderNormal( trace.plane.normal );
				mv->m_vecVelocity.Init();
			}
		}
	}
	else
	{
		m_pDODPlayer->m_lastStandingPos = mv->GetAbsOrigin();
	}
}

inline void CDODGameMovement::TracePlayerBBoxWithStep( const Vector &vStart, const Vector &vEnd, 
							unsigned int fMask, int collisionGroup, trace_t &trace )
{
	VPROF( "CDODGameMovement::TracePlayerBBoxWithStep" );

	Vector vHullMin = GetPlayerMins( player->m_Local.m_bDucked );
	vHullMin.z += player->m_Local.m_flStepSize;
	Vector vHullMax = GetPlayerMaxs( player->m_Local.m_bDucked );

	Ray_t ray;
	ray.Init( vStart, vEnd, vHullMin, vHullMax );
	UTIL_TraceRay( ray, fMask, mv->m_nPlayerHandle.Get(), collisionGroup, &trace );
}

// Taken from TF2 to prevent bouncing down slopes
bool CDODGameMovement::ResolveStanding( void )
{
	VPROF( "CDODGameMovement::ResolveStanding" );

	//
	// Attempt to move down twice your step height.  Anything between 0.5 and 1.0
	// is a valid "stand" value.
	//

	// Matt - don't move twice your step height, only check a little bit down
	// this will keep us relatively glued to stairs without feeling too snappy
	float flMaxStepDrop = 8.0f;

	Vector vecStandPos( mv->GetAbsOrigin().x, mv->GetAbsOrigin().y, mv->GetAbsOrigin().z - ( flMaxStepDrop ) );

	trace_t trace;
	TracePlayerBBoxWithStep( mv->GetAbsOrigin(), vecStandPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );

	// Anything between 0.5 and 1.0 is a valid stand value
	if ( fabs( trace.fraction - 0.5 ) < 0.0004f )
	{
		return true;
	}

	if ( trace.fraction > 0.5f )
	{
		trace.fraction -= 0.5f;
		Vector vecNewOrigin;
		vecNewOrigin = mv->GetAbsOrigin() + trace.fraction * ( vecStandPos - mv->GetAbsOrigin() );
		mv->SetAbsOrigin( vecNewOrigin );
		return false;
	}

	// Less than 0.5 mean we need to attempt to push up the difference.
	vecStandPos.z = ( mv->GetAbsOrigin().z + ( ( 0.5f - trace.fraction ) * ( player->m_Local.m_flStepSize * 2.0f ) ) );
	TracePlayerBBoxWithStep( mv->GetAbsOrigin(), vecStandPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	
	// A fraction of 1.0 means we stood up fine - done.
	if ( trace.fraction == 1.0f )
	{
		mv->SetAbsOrigin( trace.endpos );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Recover stamina
//-----------------------------------------------------------------------------
void CDODGameMovement::ReduceTimers( void )
{
	Vector vecPlayerVelocity = m_pDODPlayer->GetAbsVelocity();
	float flStamina = m_pDODPlayer->m_Shared.GetStamina();
	float fl2DVelocitySquared = vecPlayerVelocity.x * vecPlayerVelocity.x + 
								vecPlayerVelocity.y * vecPlayerVelocity.y;	
	
	if ( !( mv->m_nButtons & IN_SPEED ) )
	{
		m_pDODPlayer->m_Shared.ResetSprintPenalty();
	}

	// Can only sprint in forward direction.
	bool bSprinting = ( (mv->m_nButtons & IN_SPEED) && ( mv->m_nButtons & IN_FORWARD ) );

	// If we're holding the sprint key and also actually moving, remove some stamina
	Vector vel = m_pDODPlayer->GetAbsVelocity();
	if ( bSprinting && fl2DVelocitySquared > 10000 ) //speed > 100
	{
		//flStamina -= 30 * gpGlobals->frametime;	//reduction for sprinting
		//flStamina += 10 * gpGlobals->frametime;	//addition for recovering

		flStamina -= 20 * gpGlobals->frametime;

		m_pDODPlayer->m_Shared.SetStamina( flStamina );
	}
	else
	{
		//gain some back		
		if ( fl2DVelocitySquared <= 0 )
		{
			flStamina += 60 * gpGlobals->frametime;
		}
		else if ( ( m_pDODPlayer->GetFlags() & FL_ONGROUND ) && 
					( mv->m_nButtons & IN_DUCK ) &&
					( m_pDODPlayer->GetFlags() & FL_DUCKING ) )
		{
			flStamina += 50 * gpGlobals->frametime;
		}
		else
		{
			flStamina += 10 * gpGlobals->frametime;
		}

		m_pDODPlayer->m_Shared.SetStamina( flStamina );	
	}

#ifdef CLIENT_DLL
	if ( bSprinting && flStamina < 25 )
	{
		m_pDODPlayer->HintMessage( HINT_LOW_STAMINA );
	}
#endif

	BaseClass::ReduceTimers();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDODGameMovement::CheckJumpButton( void )
{
	if (m_pDODPlayer->pl.deadflag)
	{
		mv->m_nOldButtons |= IN_JUMP ;	// don't jump again until released
		return false;
	}
	
	if( m_pDODPlayer->m_Shared.IsProne() ||
		m_pDODPlayer->m_Shared.IsGettingUpFromProne() || 
		m_pDODPlayer->m_Shared.IsGoingProne() )
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;
	}

	// See if we are waterjumping.  If so, decrement count and return.
	float flWaterJumpTime = player->GetWaterJumpTime();

	if ( flWaterJumpTime > 0 )
	{
		flWaterJumpTime -= gpGlobals->frametime;
		if (flWaterJumpTime < 0)
			flWaterJumpTime = 0;

		player->SetWaterJumpTime( flWaterJumpTime );
		
		return false;
	}

	// If we are in the water most of the way...
	if ( m_pDODPlayer->GetWaterLevel() >= 2 )
	{	
		// swimming, not jumping
		SetGroundEntity( NULL );

		if(m_pDODPlayer->GetWaterType() == CONTENTS_WATER)    // We move up a certain amount
			mv->m_vecVelocity[2] = 100;
		else if (m_pDODPlayer->GetWaterType() == CONTENTS_SLIME)
			mv->m_vecVelocity[2] = 80;
		
		// play swiming sound
		if ( player->GetSwimSoundTime() <= 0 )
		{
			// Don't play sound again for 1 second
			player->SetSwimSoundTime( 1000 );
			PlaySwimSound();
		}

		return false;
	}

	// No more effect
 	if (m_pDODPlayer->GetGroundEntity() == NULL)
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;		// in air, so no effect
	}

	if ( mv->m_nOldButtons & IN_JUMP )
		return false;		// don't pogo stick

	if( m_pDODPlayer->m_Shared.IsInMGDeploy() )
	{
		return false;
	}

	// In the air now.
	SetGroundEntity( NULL );
	
	m_pDODPlayer->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->GetSurfaceData(), 1.0, true );
	
	m_pDODPlayer->DoAnimationEvent( PLAYERANIMEVENT_JUMP );

	// make the jump sound
	CPASFilter filter( m_pDODPlayer->GetAbsOrigin() );
	filter.UsePredictionRules();
	m_pDODPlayer->EmitSound( filter, m_pDODPlayer->entindex(), "Player.Jump" );

	float flGroundFactor = 1.0f;
	if ( player->GetSurfaceData() )
	{
		flGroundFactor = player->GetSurfaceData()->game.jumpFactor; 
	}	

	/*
	// old and busted

	float flStamina = m_pDODPlayer->m_Shared.GetStamina();

	//15.0 is the base height. the player will always jump this high
	//regardless of stamina. Also the player will be able to jump max height
	//until he is below 60 stamina. Then the height will decrease proportionately

	float flJumpSpeed = 15.0;	//base jump height
	
	if( flStamina >= 60.0f )
	{
		flJumpSpeed += 30.0;
	}
	else
	{
		flJumpSpeed += (30.0 * ( flStamina / 60.0f ) );
	}

	//Remove stamina for a successful jump
	m_pDODPlayer->m_Shared.SetStamina( flStamina - 40 );

	*/

	// new hotness - constant jumpspeed of 45
	//m_pDODPlayer->m_Shared.SetSlowedTime( 1.0f );

	Assert( GetCurrentGravity() == 800.0f );

	// Accelerate upward
	// If we are ducking...
	float startz = mv->m_vecVelocity[2];
	if ( (  m_pDODPlayer->m_Local.m_bDucking ) || (  m_pDODPlayer->GetFlags() & FL_DUCKING ) )
	{
		// d = 0.5 * g * t^2		- distance traveled with linear accel
		// t = sqrt(2.0 * 45 / g)	- how long to fall 45 units
		// v = g * t				- velocity at the end (just invert it to jump up that high)
		// v = g * sqrt(2.0 * 45 / g )
		// v^2 = g * g * 2.0 * 45 / g
		// v = sqrt( g * 2.0 * 45 )
							
		mv->m_vecVelocity[2] = flGroundFactor * 268.3281572999747f;		// flJumpSpeed of 45
		//mv->m_vecVelocity[2] = flGroundFactor * sqrt(2 * 800 * flJumpSpeed);  // 2 * gravity * height
	}
	else
	{
		mv->m_vecVelocity[2] += flGroundFactor * 268.3281572999747f;	// flJumpSpeed of 45
		//mv->m_vecVelocity[2] += flGroundFactor * sqrt(2 * 800 * flJumpSpeed);  // 2 * gravity * height
	}
	
	FinishGravity();

	mv->m_outWishVel.z += mv->m_vecVelocity[2] - startz;
	mv->m_outStepHeight += 0.1f;

	// Flag that we jumped.
	mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released

	m_pDODPlayer->m_Shared.SetJumping( true );

	return true;
}



//-----------------------------------------------------------------------------
// Purpose: Limit speed if we are ducking
//-----------------------------------------------------------------------------
void CDODGameMovement::HandleDuckingSpeedCrop()
{
	if ( !( m_iSpeedCropped & SPEED_CROPPED_DUCK ) )
	{
		if ( ( mv->m_nButtons & IN_DUCK ) || ( player->m_Local.m_bDucking ) || ( player->GetFlags() & FL_DUCKING ) )
		{
			float frac = 0.33333333f;
			mv->m_flForwardMove	*= frac;
			mv->m_flSideMove	*= frac;
			mv->m_flUpMove		*= frac;
			m_iSpeedCropped		|= SPEED_CROPPED_DUCK;
		}
	}
}

bool CDODGameMovement::CanUnprone()
{
	int i;
	trace_t trace;
	Vector newOrigin;

	VectorCopy( mv->GetAbsOrigin(), newOrigin );

	Vector vecMins, vecMaxs;

	if ( mv->m_nButtons & IN_DUCK )
	{
		vecMins = VEC_DUCK_HULL_MIN_SCALED( player );
		vecMaxs = VEC_DUCK_HULL_MAX_SCALED( player );
	}
	else
	{
		vecMins = VEC_HULL_MIN_SCALED( player );
		vecMaxs = VEC_HULL_MAX_SCALED( player );
	}

	if ( player->GetGroundEntity() != NULL )
	{
		for ( i = 0; i < 3; i++ )
		{
			newOrigin[i] += ( VEC_PRONE_HULL_MIN_SCALED( player )[i] - vecMins[i] );
		}
	}
	else
	{
		// If in air an letting go of crouch, make sure we can offset origin to make
		//  up for uncrouching

		Vector hullSizeNormal = vecMaxs - vecMins;
		Vector hullSizeProne = VEC_PRONE_HULL_MAX_SCALED( player ) - VEC_PRONE_HULL_MIN_SCALED( player );

		Vector viewDelta = -0.5f * ( hullSizeNormal - hullSizeProne );

		VectorAdd( newOrigin, viewDelta, newOrigin );
	}

	bool saveprone = m_pDODPlayer->m_Shared.IsProne();
	bool saveducked = player->m_Local.m_bDucked;

	// pretend we're not prone
	m_pDODPlayer->m_Shared.SetProne( false );
	if ( mv->m_nButtons & IN_DUCK )
		player->m_Local.m_bDucked = true;

	TracePlayerBBox( mv->GetAbsOrigin(), newOrigin, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );

	// revert to reality
	m_pDODPlayer->m_Shared.SetProne( saveprone );
	player->m_Local.m_bDucked = saveducked;

	if ( trace.startsolid || ( trace.fraction != 1.0f ) )
		return false;	

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Stop ducking
//-----------------------------------------------------------------------------
void CDODGameMovement::FinishUnDuck( void )
{
	int i;
	trace_t trace;
	Vector newOrigin;

	VectorCopy( mv->GetAbsOrigin(), newOrigin );

	if ( player->GetGroundEntity() != NULL )
	{
		for ( i = 0; i < 3; i++ )
		{
			newOrigin[i] += ( VEC_DUCK_HULL_MIN_SCALED( player )[i] - VEC_HULL_MIN_SCALED( player )[i] );
		}
	}
	else
	{
		// If in air an letting go of crouch, make sure we can offset origin to make
		//  up for uncrouching
		// orange box patch - made this match the check in CanUnduck()
 		Vector hullSizeNormal = VEC_HULL_MAX_SCALED( player ) - VEC_HULL_MIN_SCALED( player );
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX_SCALED( player ) - VEC_DUCK_HULL_MIN_SCALED( player );
		Vector viewDelta = ( hullSizeNormal - hullSizeCrouch );
		viewDelta.Negate();
		VectorAdd( newOrigin, viewDelta, newOrigin );
	}

	player->m_Local.m_bDucked = false;
	player->RemoveFlag( FL_DUCKING );
	player->m_Local.m_bDucking  = false;
	SetViewOffset( GetPlayerViewOffset( false ) );
	player->m_Local.m_flDucktime = 0;
	
	mv->SetAbsOrigin( newOrigin );

	// Recategorize position since ducking can change origin
	CategorizePosition();
}

//-----------------------------------------------------------------------------
// Purpose: Finish ducking
//-----------------------------------------------------------------------------
void CDODGameMovement::FinishDuck( void )
{
	Vector hullSizeNormal = VEC_HULL_MAX_SCALED( player ) - VEC_HULL_MIN_SCALED( player );
	Vector hullSizeCrouch = VEC_DUCK_HULL_MAX_SCALED( player ) - VEC_DUCK_HULL_MIN_SCALED( player );

	Vector viewDelta = 0.5f * ( hullSizeNormal - hullSizeCrouch );

	SetViewOffset( GetPlayerViewOffset( true ) );
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
			VectorAdd( org, viewDelta, org );
		}
		mv->SetAbsOrigin( org );

		player->m_Local.m_bDucked = true;
	}

	// See if we are stuck?
	FixPlayerCrouchStuck( true );

	// Recategorize position since ducking can change origin
	CategorizePosition();
}

// Being deployed or undeploying totally stomps the duck view offset
void CDODGameMovement::SetDeployedEyeOffset( void )
{
	if ( m_pDODPlayer->m_Shared.IsProne() || m_pDODPlayer->m_Shared.IsGettingUpFromProne() )
        return;

	if ( !m_pDODPlayer->IsAlive() )
		return;

	float flTimeSinceDeployChange = gpGlobals->curtime - m_pDODPlayer->m_Shared.m_flDeployChangeTime;

	if ( m_pDODPlayer->m_Shared.IsInMGDeploy() || flTimeSinceDeployChange < TIME_TO_DEPLOY )
	{
		if ( m_pDODPlayer->m_Shared.IsInMGDeploy() )
		{
			// anim to deployed
			if ( m_pDODPlayer->m_Shared.GetLastViewAnimTime() < m_pDODPlayer->m_Shared.m_flDeployChangeTime.m_Value )
			{
				// Deployed height
				float flViewOffset = clamp( m_pDODPlayer->m_Shared.GetDeployedHeight(),
					CROUCHING_DEPLOY_HEIGHT,
					STANDING_DEPLOY_HEIGHT );

				Vector vecView = player->GetViewOffset();
				vecView.z = flViewOffset;

				ViewOffsetAnimation( vecView, TIME_TO_DEPLOY, VIEW_ANIM_LINEAR_Z_ONLY );
				m_pDODPlayer->m_Shared.SetLastViewAnimTime( gpGlobals->curtime );
			}
		}
		else
		{
			// anim to undeployed
			if ( m_pDODPlayer->m_Shared.GetLastViewAnimTime() < m_pDODPlayer->m_Shared.m_flDeployChangeTime.m_Value )
			{
				ViewOffsetAnimation( GetPlayerViewOffset( player->m_Local.m_bDucked ), TIME_TO_DEPLOY, VIEW_ANIM_LINEAR_Z_ONLY );
				m_pDODPlayer->m_Shared.SetLastViewAnimTime( gpGlobals->curtime );
			}
		}

		if ( flTimeSinceDeployChange >= TIME_TO_DEPLOY )
		{
			player->m_Local.m_bDucked = false;
			player->RemoveFlag( FL_DUCKING );
			player->m_Local.m_bDucking  = false;
			player->m_Local.m_flDucktime = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : duckFraction - 
//-----------------------------------------------------------------------------
void CDODGameMovement::SetDODDuckedEyeOffset( float duckFraction )
{
	// Different from CGameMovement in that 
	Vector vDuckHullMin = GetPlayerMins( true );
	Vector vStandHullMin = GetPlayerMins( false );

	float fMore = ( vDuckHullMin.z - vStandHullMin.z );

	Vector vecStandViewOffset = GetPlayerViewOffset( false );

	Vector vecDuckViewOffset = GetPlayerViewOffset( true );
	Vector temp = player->GetViewOffset();
	temp.z = ( ( vecDuckViewOffset.z - fMore ) * duckFraction ) +
		( vecStandViewOffset.z * ( 1 - duckFraction ) );
	SetViewOffset( temp );
}

void CDODGameMovement::SetProneEyeOffset( float proneFraction )
{
	Vector vecPropViewOffset = VEC_PRONE_VIEW;
	Vector vecStandViewOffset = GetPlayerViewOffset( player->m_Local.m_bDucked );

	Vector temp = player->GetViewOffset();
	temp.z = SimpleSplineRemapVal( proneFraction, 1.0, 0.0, vecPropViewOffset.z, vecStandViewOffset.z );

	SetViewOffset( temp );
}

void CDODGameMovement::FinishUnProne( void )
{
	m_pDODPlayer->m_Shared.m_flUnProneTime = 0.0f;
	
	SetProneEyeOffset( 0.0 );

	Vector vHullMin = GetPlayerMins( player->m_Local.m_bDucked );
	Vector vHullMax = GetPlayerMaxs( player->m_Local.m_bDucked );

	if ( m_bUnProneToDuck )
	{
		FinishDuck();
	}
	else
	{
		CategorizePosition();

		if ( mv->m_nButtons & IN_DUCK && !( player->GetFlags() & FL_DUCKING ) )
		{
			// Use 1 second so super long jump will work
			player->m_Local.m_flDucktime = 1000;
			player->m_Local.m_bDucking    = true;
		}
	}
}

void CDODGameMovement::FinishProne( void )
{	
	m_pDODPlayer->m_Shared.SetProne( true );
	m_pDODPlayer->m_Shared.m_flGoProneTime = 0.0f;

#ifndef CLIENT_DLL
	m_pDODPlayer->HintMessage( HINT_PRONE );
#endif

	FinishUnDuck();	// clear ducking

	SetProneEyeOffset( 1.0 );

	FixPlayerCrouchStuck(true);

	CategorizePosition();
}

//-----------------------------------------------------------------------------
// Purpose: See if duck button is pressed and do the appropriate things
//-----------------------------------------------------------------------------
void CDODGameMovement::Duck( void )
{
	int buttonsChanged	= ( mv->m_nOldButtons ^ mv->m_nButtons );	// These buttons have changed this frame
	int buttonsPressed	=  buttonsChanged & mv->m_nButtons;			// The changed ones still down are "pressed"
	int buttonsReleased	=  buttonsChanged & mv->m_nOldButtons;		// The changed ones which were previously down are "released"

	if ( mv->m_nButtons & IN_DUCK )
	{
		mv->m_nOldButtons |= IN_DUCK;
	}
	else
	{
		mv->m_nOldButtons &= ~IN_DUCK;
	}

	if ( !player->IsAlive() )
	{
		if( m_pDODPlayer->m_Shared.IsProne() )
		{
			FinishUnProne();
		}

		// Unduck
		if ( player->m_Local.m_bDucking || player->m_Local.m_bDucked )
		{
			FinishUnDuck();
		}
		return;
	}

	static int iState = 0;

	// Prone / UnProne - we don't duck or deploy if this is happening
	if( m_pDODPlayer->m_Shared.m_flUnProneTime > 0.0f )
	{
		float pronetime = m_pDODPlayer->m_Shared.m_flUnProneTime - gpGlobals->curtime;

		if( pronetime < 0 )
		{
			FinishUnProne();

			if ( !m_bUnProneToDuck && ( mv->m_nButtons & IN_DUCK ) )
			{
				buttonsPressed |= IN_DUCK;
				mv->m_nOldButtons &= ~IN_DUCK;
			}
		}

		// Set these, so that as soon as we stop unproning, we don't pop to standing
		// the information that we let go of the duck key has been lost by now.
		if ( m_bUnProneToDuck )
		{
            player->m_Local.m_flDucktime = 1000;
			player->m_Local.m_bDucking    = true;
		}

		//don't deal with ducking while we're proning
		return;
	}
	else if( m_pDODPlayer->m_Shared.m_flGoProneTime > 0.0f )
	{
		float pronetime = m_pDODPlayer->m_Shared.m_flGoProneTime - gpGlobals->curtime;

		if( pronetime < 0 )
		{
			FinishProne();
		}

		//don't deal with ducking while we're proning
		return;
	}

	if ( gpGlobals->curtime > m_pDODPlayer->m_Shared.m_flNextProneCheck )
	{
		if ( buttonsPressed & IN_ALT1 && m_pDODPlayer->m_Shared.CanChangePosition() )
		{
			if( m_pDODPlayer->m_Shared.IsProne() == false &&
				m_pDODPlayer->m_Shared.IsGettingUpFromProne() == false )
			{
				m_pDODPlayer->m_Shared.StartGoingProne();

				// do unprone anim
				ViewOffsetAnimation( VEC_PRONE_VIEW, TIME_TO_PRONE,	VIEW_ANIM_EXPONENTIAL_Z_ONLY );
			}
			else if( CanUnprone() )
			{
				m_pDODPlayer->m_Shared.SetProne( false );
				m_pDODPlayer->m_Shared.StandUpFromProne();

				// do unprone anim
				ViewOffsetAnimation( GetPlayerViewOffset( m_bUnProneToDuck ),
						TIME_TO_PRONE,
						VIEW_ANIM_EXPONENTIAL_Z_ONLY );

				m_bUnProneToDuck = ( mv->m_nButtons & IN_DUCK ) > 0;
			}
			
			m_pDODPlayer->m_Shared.m_flNextProneCheck = gpGlobals->curtime + 1.0f;
			return;
		}
	}

	SetDeployedEyeOffset();

	if ( m_pDODPlayer->m_Shared.IsProne() &&
		m_pDODPlayer->m_Shared.CanChangePosition() && 
		!m_pDODPlayer->m_Shared.IsGettingUpFromProne() &&
		( buttonsPressed & IN_DUCK ) && 
		CanUnprone() )	// BUGBUG - even calling this will unzoom snipers.
	{
		// If the player presses duck while prone,
		// unprone them to the duck position
		m_pDODPlayer->m_Shared.SetProne( false );
		m_pDODPlayer->m_Shared.StandUpFromProne();

		m_bUnProneToDuck = true;

		// do unprone anim
		ViewOffsetAnimation( GetPlayerViewOffset( m_bUnProneToDuck ),
			TIME_TO_PRONE,
			VIEW_ANIM_EXPONENTIAL_Z_ONLY );

		// simulate a duck that was pressed while we were prone
		player->AddFlag( FL_DUCKING );
		player->m_Local.m_bDucked = true;
		player->m_Local.m_flDucktime = 1000;
		player->m_Local.m_bDucking    = true;
	}

	// no ducking or unducking while deployed or prone
	if( m_pDODPlayer->m_Shared.IsProne() ||
		m_pDODPlayer->m_Shared.IsGettingUpFromProne() ||
		!m_pDODPlayer->m_Shared.CanChangePosition() )
	{
		return;
	}

	HandleDuckingSpeedCrop();

	if ( !( player->GetFlags() & FL_DUCKING ) && ( player->m_Local.m_bDucked ) )
	{
		player->m_Local.m_bDucked = false;
	}

	/*
	Msg( "duck button %s  ducking %s  ducked %s  duck flags %s\n",
		( mv->m_nButtons & IN_DUCK ) ? "down" : "up",
		( player->m_Local.m_bDucking ) ? "yes" : "no",
		( player->m_Local.m_bDucked ) ? "yes" : "no",
		( player->GetFlags() & FL_DUCKING ) ? "set" : "not set" );*/
	
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
					float flDuckFraction = SimpleSpline( duckseconds / TIME_TO_DUCK );
					SetDODDuckedEyeOffset( flDuckFraction );
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
							SetDODDuckedEyeOffset( duckFraction );
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
Vector CDODGameMovement::GetPlayerMins( void ) const
{
	if ( !player )
	{
		return vec3_origin;
	}

	if ( player->IsObserver() )
	{
		return VEC_OBS_HULL_MIN_SCALED( player );	
	}
	else
	{
		if ( player->m_Local.m_bDucked )
			return VEC_DUCK_HULL_MIN_SCALED( player );
		else if ( m_pDODPlayer->m_Shared.IsProne() )
			return VEC_PRONE_HULL_MIN_SCALED( player );
		else
			return VEC_HULL_MIN_SCALED( player );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
Vector CDODGameMovement::GetPlayerMaxs( void ) const
{	
	if ( !player )
	{
		return vec3_origin;
	}
	if ( player->IsObserver() )
	{
		return VEC_OBS_HULL_MAX_SCALED( player );	
	}
	else
	{
		if ( player->m_Local.m_bDucked )
			return VEC_DUCK_HULL_MAX_SCALED( player );
		else if ( m_pDODPlayer->m_Shared.IsProne() )
            return VEC_PRONE_HULL_MAX_SCALED( player );
		else
			return VEC_HULL_MAX_SCALED( player );
	}
}

void CDODGameMovement::SetViewOffset( Vector vecViewOffset )
{
	// call this instead of player->SetViewOffset directly, so we can cancel any
	// animation in progress

	m_pDODPlayer->m_Shared.ResetViewOffsetAnimation();

	player->SetViewOffset( vecViewOffset );
}

void CDODGameMovement::ViewOffsetAnimation( Vector vecDest, float flTime, ViewAnimationType type )
{
	m_pDODPlayer->m_Shared.ViewOffsetAnimation( vecDest, flTime, type );
}