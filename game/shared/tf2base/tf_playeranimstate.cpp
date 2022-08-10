//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"
#include "tf_playeranimstate.h"
#include "base_playeranimstate.h"
#include "datacache/imdlcache.h"

#ifdef CLIENT_DLL
#include "c_tf_player.h"
#else
#include "tf_player.h"
#endif

#define TF_RUN_SPEED			320.0f
#define TF_WALK_SPEED			75.0f
#define TF_CROUCHWALK_SPEED		110.0f

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
// Output : CMultiPlayerAnimState*
//-----------------------------------------------------------------------------
CTFPlayerAnimState* CreateTFPlayerAnimState( CTFPlayer *pPlayer )
{
	MDLCACHE_CRITICAL_SECTION();

	// Setup the movement data.
	MultiPlayerMovementData_t movementData;
	movementData.m_flBodyYawRate = 720.0f;
	movementData.m_flRunSpeed = TF_RUN_SPEED;
	movementData.m_flWalkSpeed = TF_WALK_SPEED;
	movementData.m_flSprintSpeed = -1.0f;

	// Create animation state for this player.
	CTFPlayerAnimState *pRet = new CTFPlayerAnimState( pPlayer, movementData );

	// Specific TF player initialization.
	pRet->InitTF( pPlayer );

	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CTFPlayerAnimState::CTFPlayerAnimState()
{
	m_pTFPlayer = NULL;

	// Don't initialize TF specific variables here. Init them in InitTF()
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			&movementData - 
//-----------------------------------------------------------------------------
CTFPlayerAnimState::CTFPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData )
: CMultiPlayerAnimState( pPlayer, movementData )
{
	m_pTFPlayer = NULL;

	// Don't initialize TF specific variables here. Init them in InitTF()
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CTFPlayerAnimState::~CTFPlayerAnimState()
{
}

//-----------------------------------------------------------------------------
// Purpose: Initialize Team Fortress specific animation state.
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CTFPlayerAnimState::InitTF( CTFPlayer *pPlayer )
{
	m_pTFPlayer = pPlayer;
	m_bInAirWalk = false;
	m_flHoldDeployedPoseUntilTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerAnimState::ClearAnimationState( void )
{
	m_bInAirWalk = false;

	BaseClass::ClearAnimationState();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : actDesired - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CTFPlayerAnimState::TranslateActivity( Activity actDesired )
{
	Activity translateActivity = BaseClass::TranslateActivity( actDesired );

	if ( GetTFPlayer()->GetActiveWeapon() )
	{
		translateActivity = GetTFPlayer()->GetActiveWeapon()->ActivityOverride( translateActivity, NULL );
	}

	return translateActivity;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerAnimState::Update( float eyeYaw, float eyePitch )
{
	// Profile the animation update.
	VPROF( "CMultiPlayerAnimState::Update" );

	// Get the TF player.
	CTFPlayer *pTFPlayer = GetTFPlayer();
	if ( !pTFPlayer )
		return;

	// Get the studio header for the player.
	CStudioHdr *pStudioHdr = pTFPlayer->GetModelPtr();
	if ( !pStudioHdr )
		return;

	// Check to see if we should be updating the animation state - dead, ragdolled?
	if ( !ShouldUpdateAnimState() )
	{
		ClearAnimationState();
		return;
	}

	// Store the eye angles.
	m_flEyeYaw = AngleNormalize( eyeYaw );
	m_flEyePitch = AngleNormalize( eyePitch );

	// Compute the player sequences.
	ComputeSequences( pStudioHdr );

	if ( SetupPoseParameters( pStudioHdr ) && !pTFPlayer->m_Shared.InCond( TF_COND_TAUNTING ) )
	{
		// Pose parameter - what direction are the player's legs running in.
		ComputePoseParam_MoveYaw( pStudioHdr );

		// Pose parameter - Torso aiming (up/down).
		ComputePoseParam_AimPitch( pStudioHdr );

		// Pose parameter - Torso aiming (rotation).
		ComputePoseParam_AimYaw( pStudioHdr );
	}

#ifdef CLIENT_DLL 
	if ( C_BasePlayer::ShouldDrawLocalPlayer() )
	{
		GetBasePlayer()->SetPlaybackRate( 1.0f );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : event - 
//-----------------------------------------------------------------------------
void CTFPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	Activity iGestureActivity = ACT_INVALID;

	switch( event )
	{
	case PLAYERANIMEVENT_ATTACK_PRIMARY:
		{
			CTFPlayer *pPlayer = GetTFPlayer();
			if ( !pPlayer )
				return;

			CTFWeaponBase *pWpn = pPlayer->GetActiveTFWeapon();
			bool bIsMinigun = ( pWpn && pWpn->GetWeaponID() == TF_WEAPON_MINIGUN );
			bool bIsSniperRifle = ( pWpn && pWpn->GetWeaponID() == TF_WEAPON_SNIPERRIFLE );

			// Heavy weapons primary fire.
			if ( bIsMinigun )
			{
				// Play standing primary fire.
				iGestureActivity = ACT_MP_ATTACK_STAND_PRIMARYFIRE;

				if ( m_bInSwim )
				{
					// Play swimming primary fire.
					iGestureActivity = ACT_MP_ATTACK_SWIM_PRIMARYFIRE;
				}
				else if ( GetBasePlayer()->GetFlags() & FL_DUCKING )
				{
					// Play crouching primary fire.
					iGestureActivity = ACT_MP_ATTACK_CROUCH_PRIMARYFIRE;
				}

				if ( !IsGestureSlotPlaying( GESTURE_SLOT_ATTACK_AND_RELOAD, TranslateActivity(iGestureActivity) ) )
				{
					RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, iGestureActivity );
				}
			}
			else if ( bIsSniperRifle && pPlayer->m_Shared.InCond( TF_COND_ZOOMED ) )
			{
				// Weapon primary fire, zoomed in
				if ( GetBasePlayer()->GetFlags() & FL_DUCKING )
				{
					RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_ATTACK_CROUCH_PRIMARYFIRE_DEPLOYED );
				}
				else
				{
					RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_ATTACK_STAND_PRIMARYFIRE_DEPLOYED );
				}

				iGestureActivity = ACT_VM_PRIMARYATTACK;

				// Hold our deployed pose for a few seconds
				m_flHoldDeployedPoseUntilTime = gpGlobals->curtime + 2.0;
			}
			else
			{
				// Weapon primary fire.
				if ( GetBasePlayer()->GetFlags() & FL_DUCKING )
				{
					RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_ATTACK_CROUCH_PRIMARYFIRE );
				}
				else
				{
					RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_ATTACK_STAND_PRIMARYFIRE );
				}

				iGestureActivity = ACT_VM_PRIMARYATTACK;
			}

			break;
		}

	case PLAYERANIMEVENT_VOICE_COMMAND_GESTURE:
		{
			if ( !IsGestureSlotActive( GESTURE_SLOT_ATTACK_AND_RELOAD ) )
			{
				RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, (Activity)nData );
			}
			break;
		}
	case PLAYERANIMEVENT_ATTACK_SECONDARY:
		{
			// Weapon secondary fire.
			if ( GetBasePlayer()->GetFlags() & FL_DUCKING )
			{
				RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_ATTACK_CROUCH_SECONDARYFIRE );
			}
			else
			{
				RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_ATTACK_STAND_SECONDARYFIRE );
			}

			iGestureActivity = ACT_VM_PRIMARYATTACK;
			break;
		}
	case PLAYERANIMEVENT_ATTACK_PRE:
		{
			CTFPlayer *pPlayer = GetTFPlayer();
			if ( !pPlayer )
				return;

			CTFWeaponBase *pWpn = pPlayer->GetActiveTFWeapon();
			bool bIsMinigun  = ( pWpn && pWpn->GetWeaponID() == TF_WEAPON_MINIGUN );

			bool bAutoKillPreFire = false;
			if ( bIsMinigun )
			{
				bAutoKillPreFire = true;
			}

			if ( m_bInSwim && bIsMinigun )
			{
				// Weapon pre-fire. Used for minigun windup while swimming
				iGestureActivity = ACT_MP_ATTACK_SWIM_PREFIRE;
			}
			else if ( GetBasePlayer()->GetFlags() & FL_DUCKING ) 
			{
				// Weapon pre-fire. Used for minigun windup, sniper aiming start, etc in crouch.
				iGestureActivity = ACT_MP_ATTACK_CROUCH_PREFIRE;
			}
			else
			{
				// Weapon pre-fire. Used for minigun windup, sniper aiming start, etc.
				iGestureActivity = ACT_MP_ATTACK_STAND_PREFIRE;
			}

			RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, iGestureActivity, bAutoKillPreFire );

			break;
		}
	case PLAYERANIMEVENT_ATTACK_POST:
		{
			CTFPlayer *pPlayer = GetTFPlayer();
			if ( !pPlayer )
				return;

			CTFWeaponBase *pWpn = pPlayer->GetActiveTFWeapon();
			bool bIsMinigun  = ( pWpn && pWpn->GetWeaponID() == TF_WEAPON_MINIGUN );

			if ( m_bInSwim && bIsMinigun )
			{
				// Weapon pre-fire. Used for minigun winddown while swimming
				iGestureActivity = ACT_MP_ATTACK_SWIM_POSTFIRE;
			}
			else if ( GetBasePlayer()->GetFlags() & FL_DUCKING ) 
			{
				// Weapon post-fire. Used for minigun winddown in crouch.
				iGestureActivity = ACT_MP_ATTACK_CROUCH_POSTFIRE;
			}
			else
			{
				// Weapon post-fire. Used for minigun winddown.
				iGestureActivity = ACT_MP_ATTACK_STAND_POSTFIRE;
			}

			RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, iGestureActivity );

			break;
		}

	case PLAYERANIMEVENT_RELOAD:
		{
			// Weapon reload.
			if ( m_bInAirWalk )
			{
				RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_RELOAD_AIRWALK );
			}
			else
			{
				BaseClass::DoAnimationEvent( event, nData );
			}
			break;
		}
	case PLAYERANIMEVENT_RELOAD_LOOP:
		{
			// Weapon reload.
			if ( m_bInAirWalk )
			{
				RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_RELOAD_AIRWALK_LOOP );
			}
			else
			{
				BaseClass::DoAnimationEvent( event, nData );
			}
			break;
		}
	case PLAYERANIMEVENT_RELOAD_END:
		{
			// Weapon reload.
			if ( m_bInAirWalk )
			{
				RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_RELOAD_AIRWALK_END );
			}
			else
			{
				BaseClass::DoAnimationEvent( event, nData );
			}
			break;
		}
	case PLAYERANIMEVENT_DOUBLEJUMP:
		{
			// Check to see if we are jumping!
			if ( !m_bJumping )
			{
				m_bJumping = true;
				m_bFirstJumpFrame = true;
				m_flJumpStartTime = gpGlobals->curtime;
				RestartMainSequence();
			}

			// Force the air walk off.
			m_bInAirWalk = false;

			// Player the air dash gesture.
			RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_DOUBLEJUMP );
			break;
		}
	default:
		{
			BaseClass::DoAnimationEvent( event, nData );
			break;
		}
	}

#ifdef CLIENT_DLL
	// Make the weapon play the animation as well
	if ( iGestureActivity != ACT_INVALID )
	{
		CBaseCombatWeapon *pWeapon = GetTFPlayer()->GetActiveWeapon();
		if ( pWeapon )
		{
			pWeapon->SendWeaponAnim( iGestureActivity );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
//-----------------------------------------------------------------------------
bool CTFPlayerAnimState::HandleSwimming( Activity &idealActivity )
{
	bool bInWater = BaseClass::HandleSwimming( idealActivity );

	if ( bInWater )
	{
		if ( m_pTFPlayer->m_Shared.InCond( TF_COND_AIMING ) )
		{
			CTFWeaponBase *pWpn = m_pTFPlayer->GetActiveTFWeapon();
			if ( pWpn && pWpn->GetWeaponID() == TF_WEAPON_MINIGUN )
			{
				idealActivity = ACT_MP_SWIM_DEPLOYED;
			}
			// Check for sniper deployed underwater - should only be when standing on something
			else if ( pWpn && pWpn->GetWeaponID() == TF_WEAPON_SNIPERRIFLE )
			{
				if ( m_pTFPlayer->m_Shared.InCond( TF_COND_ZOOMED ) )
				{
					idealActivity = ACT_MP_SWIM_DEPLOYED;
				}
			}
		}
	}

	return bInWater;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFPlayerAnimState::HandleMoving( Activity &idealActivity )
{
	float flSpeed = GetOuterXYSpeed();

	// If we move, cancel the deployed anim hold
	if ( flSpeed > MOVING_MINIMUM_SPEED )
	{
		m_flHoldDeployedPoseUntilTime = 0.0;
	}

	if ( m_pTFPlayer->m_Shared.InCond( TF_COND_AIMING ) ) 
	{
		if ( flSpeed > MOVING_MINIMUM_SPEED )
		{
			idealActivity = ACT_MP_DEPLOYED;
		}
		else
		{
			idealActivity = ACT_MP_DEPLOYED_IDLE;
		}
	}
	else if ( m_flHoldDeployedPoseUntilTime > gpGlobals->curtime )
	{
		// Unless we move, hold the deployed pose for a number of seconds after being deployed
		idealActivity = ACT_MP_DEPLOYED_IDLE;
	}
	else 
	{
		return BaseClass::HandleMoving( idealActivity );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFPlayerAnimState::HandleDucking( Activity &idealActivity )
{
	if ( GetBasePlayer()->GetFlags() & FL_DUCKING )
	{
		if ( GetOuterXYSpeed() < MOVING_MINIMUM_SPEED )
		{
			idealActivity = ACT_MP_CROUCH_IDLE;		
			if ( m_pTFPlayer->m_Shared.InCond( TF_COND_AIMING ) || m_flHoldDeployedPoseUntilTime > gpGlobals->curtime )
			{
				idealActivity = ACT_MP_CROUCH_DEPLOYED_IDLE;
			}
		}
		else
		{
			idealActivity = ACT_MP_CROUCHWALK;		

			if ( m_pTFPlayer->m_Shared.InCond( TF_COND_AIMING ) )
			{
				// Don't do this for the heavy! we don't usually let him deployed crouch walk
				bool bIsMinigun = false;

				CTFPlayer *pPlayer = GetTFPlayer();
				if ( pPlayer && pPlayer->GetActiveTFWeapon() )
				{
					bIsMinigun = ( pPlayer->GetActiveTFWeapon()->GetWeaponID() == TF_WEAPON_MINIGUN );
				}

				if ( !bIsMinigun )
				{
					idealActivity = ACT_MP_CROUCH_DEPLOYED;
				}
			}
		}

		return true;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
bool CTFPlayerAnimState::HandleJumping( Activity &idealActivity )
{
	Vector vecVelocity;
	GetOuterAbsVelocity( vecVelocity );

	// Don't allow a firing heavy to jump or air walk.
	if ( m_pTFPlayer->GetPlayerClass()->IsClass( TF_CLASS_HEAVYWEAPONS ) && m_pTFPlayer->m_Shared.InCond( TF_COND_AIMING ) )
		return false;
		
	// Handle air walking before handling jumping - air walking supersedes jump
	TFPlayerClassData_t *pData = m_pTFPlayer->GetPlayerClass()->GetData();
	bool bValidAirWalkClass = ( pData && pData->m_bDontDoAirwalk == false );

	if ( bValidAirWalkClass && ( vecVelocity.z > 300.0f || m_bInAirWalk ) )
	{
		// Check to see if we were in an airwalk and now we are basically on the ground.
		if ( ( GetBasePlayer()->GetFlags() & FL_ONGROUND ) && m_bInAirWalk )
		{				
			m_bInAirWalk = false;
			RestartMainSequence();
			RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_JUMP_LAND );	
		}
		else if ( GetBasePlayer()->GetWaterLevel() >= WL_Waist )
		{
			// Turn off air walking and reset the animation.
			m_bInAirWalk = false;
			RestartMainSequence();
		}
		else if ( ( GetBasePlayer()->GetFlags() & FL_ONGROUND ) == 0 )
		{
			// In an air walk.
			idealActivity = ACT_MP_AIRWALK;
			m_bInAirWalk = true;
		}
	}
	// Jumping.
	else
	{
		if ( m_bJumping )
		{
			// Remove me once all classes are doing the new jump
			TFPlayerClassData_t *pData = m_pTFPlayer->GetPlayerClass()->GetData();
			bool bNewJump = ( pData && pData->m_bDontDoNewJump == false );

			if ( m_bFirstJumpFrame )
			{
				m_bFirstJumpFrame = false;
				RestartMainSequence();	// Reset the animation.
			}

			// Reset if we hit water and start swimming.
			if ( GetBasePlayer()->GetWaterLevel() >= WL_Waist )
			{
				m_bJumping = false;
				RestartMainSequence();
			}
			// Don't check if he's on the ground for a sec.. sometimes the client still has the
			// on-ground flag set right when the message comes in.
			else if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f )
			{
				if ( GetBasePlayer()->GetFlags() & FL_ONGROUND )
				{
					m_bJumping = false;
					RestartMainSequence();

					if ( bNewJump )
					{
						RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_JUMP_LAND );					
					}
				}
			}

			// if we're still jumping
			if ( m_bJumping )
			{
				if ( bNewJump )
				{
					if ( gpGlobals->curtime - m_flJumpStartTime > 0.5 )
					{
						idealActivity = ACT_MP_JUMP_FLOAT;
					}
					else
					{
						idealActivity = ACT_MP_JUMP_START;
					}
				}
				else
				{
					idealActivity = ACT_MP_JUMP;
				}
			}
		}	
	}	

	if ( m_bJumping || m_bInAirWalk )
		return true;

	return false;
}

