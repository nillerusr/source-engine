//========= Copyright Valve Corporation, All rights reserved. ============//
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
#include "portal_playeranimstate.h"
#include "base_playeranimstate.h"

#ifdef CLIENT_DLL
#include "c_portal_player.h"
#include "c_weapon_portalgun.h"
#else
#include "portal_player.h"
#include "weapon_portalgun.h"
#endif

#define PORTAL_RUN_SPEED			320.0f
#define PORTAL_CROUCHWALK_SPEED		110.0f

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
// Output : CMultiPlayerAnimState*
//-----------------------------------------------------------------------------
CPortalPlayerAnimState* CreatePortalPlayerAnimState( CPortal_Player *pPlayer )
{
	// Setup the movement data.
	MultiPlayerMovementData_t movementData;
	movementData.m_flBodyYawRate = 720.0f;
	movementData.m_flRunSpeed = PORTAL_RUN_SPEED;
	movementData.m_flWalkSpeed = -1;
	movementData.m_flSprintSpeed = -1.0f;

	// Create animation state for this player.
	CPortalPlayerAnimState *pRet = new CPortalPlayerAnimState( pPlayer, movementData );

	// Specific Portal player initialization.
	pRet->InitPortal( pPlayer );

	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CPortalPlayerAnimState::CPortalPlayerAnimState()
{
	m_pPortalPlayer = NULL;

	// Don't initialize Portal specific variables here. Init them in InitPortal()
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			&movementData - 
//-----------------------------------------------------------------------------
CPortalPlayerAnimState::CPortalPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData )
: CMultiPlayerAnimState( pPlayer, movementData )
{
	m_pPortalPlayer = NULL;

	// Don't initialize Portal specific variables here. Init them in InitPortal()
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CPortalPlayerAnimState::~CPortalPlayerAnimState()
{
}

//-----------------------------------------------------------------------------
// Purpose: Initialize Portal specific animation state.
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CPortalPlayerAnimState::InitPortal( CPortal_Player *pPlayer )
{
	m_pPortalPlayer = pPlayer;
	m_bInAirWalk = false;
	m_flHoldDeployedPoseUntilTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalPlayerAnimState::ClearAnimationState( void )
{
	m_bInAirWalk = false;

	BaseClass::ClearAnimationState();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : actDesired - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CPortalPlayerAnimState::TranslateActivity( Activity actDesired )
{
	Activity translateActivity = BaseClass::TranslateActivity( actDesired );

	if ( GetPortalPlayer()->GetActiveWeapon() )
	{
		translateActivity = GetPortalPlayer()->GetActiveWeapon()->ActivityOverride( translateActivity, NULL );
	}

	return translateActivity;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : event - 
//-----------------------------------------------------------------------------
void CPortalPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	Activity iWeaponActivity = ACT_INVALID;

	switch( event )
	{
	case PLAYERANIMEVENT_ATTACK_PRIMARY:
	case PLAYERANIMEVENT_ATTACK_SECONDARY:
		{
			CPortal_Player *pPlayer = GetPortalPlayer();
			if ( !pPlayer )
				return;

			CWeaponPortalBase *pWpn = pPlayer->GetActivePortalWeapon();

			if ( pWpn )
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

				iWeaponActivity = ACT_VM_PRIMARYATTACK;
			}
			else	// unarmed player
			{
				
			}
	
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
	if ( iWeaponActivity != ACT_INVALID )
	{
		CBaseCombatWeapon *pWeapon = GetPortalPlayer()->GetActiveWeapon();
		if ( pWeapon )
		{
			pWeapon->SendWeaponAnim( iWeaponActivity );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortalPlayerAnimState::Teleport( const Vector *pNewOrigin, const QAngle *pNewAngles, CPortal_Player* pPlayer )
{
	QAngle absangles = pPlayer->GetAbsAngles();
	m_angRender = absangles;
	m_angRender.x = m_angRender.z = 0.0f;
	if ( pPlayer )
	{
		// Snap the yaw pose parameter lerping variables to face new angles.
		m_flCurrentFeetYaw = m_flGoalFeetYaw = m_flEyeYaw = pPlayer->EyeAngles()[YAW];
	}
}




//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPortalPlayerAnimState::HandleMoving( Activity &idealActivity )
{
	float flSpeed = GetOuterXYSpeed();

	// If we move, cancel the deployed anim hold
	if ( flSpeed > MOVING_MINIMUM_SPEED )
	{
		m_flHoldDeployedPoseUntilTime = 0.0;
		idealActivity = ACT_MP_RUN;
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
bool CPortalPlayerAnimState::HandleDucking( Activity &idealActivity )
{
	if ( GetBasePlayer()->m_Local.m_bDucking || GetBasePlayer()->m_Local.m_bDucked )
	{
		if ( GetOuterXYSpeed() < MOVING_MINIMUM_SPEED )
		{
			idealActivity = ACT_MP_CROUCH_IDLE;		
		}
		else
		{
			idealActivity = ACT_MP_CROUCHWALK;		
		}

		return true;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
bool CPortalPlayerAnimState::HandleJumping( Activity &idealActivity )
{
	Vector vecVelocity;
	GetOuterAbsVelocity( vecVelocity );

	if ( ( vecVelocity.z > 300.0f || m_bInAirWalk ) )
	{
		// Check to see if we were in an airwalk and now we are basically on the ground.
		if ( GetBasePlayer()->GetFlags() & FL_ONGROUND )
		{				
			m_bInAirWalk = false;
			RestartMainSequence();
			RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_JUMP_LAND );	
		}
		else
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
			if ( m_bFirstJumpFrame )
			{
				m_bFirstJumpFrame = false;
				RestartMainSequence();	// Reset the animation.
			}

			// Don't check if he's on the ground for a sec.. sometimes the client still has the
			// on-ground flag set right when the message comes in.
			else if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f )
			{
				if ( GetBasePlayer()->GetFlags() & FL_ONGROUND )
				{
					m_bJumping = false;
					RestartMainSequence();
					RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_JUMP_LAND );					
				}
			}

			// if we're still jumping
			if ( m_bJumping )
			{
				idealActivity = ACT_MP_JUMP_START;
			}
		}	
	}	

	if ( m_bJumping || m_bInAirWalk )
		return true;

	return false;
}