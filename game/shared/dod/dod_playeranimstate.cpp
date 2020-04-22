//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_playeranimstate.h"
#include "base_playeranimstate.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"
#include "weapon_dodbase.h"
#include "dod_shareddefs.h"

#ifdef CLIENT_DLL
	#include "c_dod_player.h"
	#include "engine/ivdebugoverlay.h"
	#include "filesystem.h"

	ConVar anim_showmainactivity( "anim_showmainactivity", "0", FCVAR_CHEAT, "Show the idle, walk, run, and/or sprint activities." );
#else
	#include "dod_player.h"
#endif

ConVar anim_showstate( "anim_showstate", "-1", FCVAR_CHEAT | FCVAR_REPLICATED, "Show the (client) animation state for the specified entity (-1 for none)." );
ConVar anim_showstatelog( "anim_showstatelog", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "1 to output anim_showstate to Msg(). 2 to store in AnimState.log. 3 for both." );
ConVar dod_bodyheightoffset( "dod_bodyheightoffset", "4", FCVAR_CHEAT | FCVAR_REPLICATED, "Deploy height offset." );

#define ANIMPART_STAND "stand"
#define ANIMPART_PRONE "prone"
#define ANIMPART_CROUCH "crouch"
#define ANIMPART_SPRINT "sprint"
#define ANIMPART_SANDBAG "sandbag"
#define ANIMPART_BIPOD "bipod"

// When moving this fast, he plays run anim.
#define ARBITRARY_RUN_SPEED		300.0f
#define DOD_BODYYAW_RATE		720.0f

#define DOD_WALK_SPEED		60.0f
#define DOD_RUN_SPEED		120.0f
#define DOD_SPRINT_SPEED	260.0f

class CDODPlayerAnimState : public CBasePlayerAnimState, public IDODPlayerAnimState
{
public:

	DECLARE_CLASS( CDODPlayerAnimState, CBasePlayerAnimState );
	friend IDODPlayerAnimState* CreatePlayerAnimState( CDODPlayer *pPlayer );

	CDODPlayerAnimState();

	virtual void ShowDebugInfo( void );

	// This is called by both the client and the server in the same way to trigger events for
	// players firing, jumping, throwing grenades, etc.
	virtual void DoAnimationEvent( PlayerAnimEvent_t event, int nData );
	virtual void ClearAnimationState();
	virtual Activity CalcMainActivity();	
	virtual void Update( float eyeYaw, float eyePitch );

	virtual void DebugShowAnimState( int iStartLine );

	virtual int CalcAimLayerSequence( float *flCyle, float *flAimSequenceWeight, bool bForceIdle ) { return 0; }

	virtual float GetCurrentMaxGroundSpeed();
	virtual void ComputeSequences( CStudioHdr *pStudioHdr );
	virtual void ClearAnimationLayers();

	virtual void RestartMainSequence();
	virtual float CalcMovementPlaybackRate( bool *bIsMoving );

	Activity TranslateActivity( Activity actDesired );
	void CancelGestures( void );

protected:

	// Pose paramters.
	bool				SetupPoseParameters( CStudioHdr *pStudioHdr );
	virtual void		ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr );
	virtual void		ComputePoseParam_AimPitch( CStudioHdr *pStudioHdr );
	virtual void		ComputePoseParam_AimYaw( CStudioHdr *pStudioHdr );
	void				ComputePoseParam_BodyHeight( CStudioHdr *pStudioHdr );
	virtual void		EstimateYaw( void );
	void				ConvergeYawAngles( float flGoalYaw, float flYawRate, float flDeltaTime, float &flCurrentYaw );

	void ComputeFireSequence();
	void ComputeDeployedSequence();

	void ComputeGestureSequence( CStudioHdr *pStudioHdr );

	void RestartGesture( int iGestureType, Activity act, bool bAutoKill = true );

	void UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd, float flWeight = 1.0 );

	void				DebugShowAnimStateForPlayer( bool bIsServer );
	void				DebugShowEyeYaw( void );

// Client specific.
#ifdef CLIENT_DLL

	// Debug.
	void				DebugShowActivity( Activity activity );

#endif

private:

	void InitDOD( CDODPlayer *pPlayer );
	
	bool HandleJumping( Activity *idealActivity );
	bool HandleProne( Activity *idealActivity );
	bool HandleProneDown( CDODPlayer *pPlayer, Activity *idealActivity );
	bool HandleProneUp( CDODPlayer *pPlayer, Activity *idealActivity );
	bool HandleDucked( Activity *idealActivity );

	bool IsGettingDown( CDODPlayer *pPlayer );
	bool IsGettingUp( CDODPlayer *pPlayer );

	CDODPlayer* GetOuterDOD() const;

	bool IsPlayingGesture( int type )
	{
		return ( m_bPlayingGesture && m_iGestureType == type );
	}

private:
	// Current state variables.
	bool m_bJumping;			// Set on a jump event.
	float m_flJumpStartTime;	
	bool m_bFirstJumpFrame;

	// These control the prone state _achine.
	bool m_bGettingDown;
	bool m_bGettingUp;
	bool m_bWasGoingProne;
	bool m_bWasGettingUp;

	// The single Gesture layer
	bool m_bPlayingGesture;
	bool m_bAutokillGesture;
	int m_iGestureSequence;
	float m_flGestureCycle;

	int m_iGestureType;

	enum
	{
		GESTURE_NONE = -1,
		GESTURE_ATTACK1 = 0,
		GESTURE_ATTACK2,
		GESTURE_RELOAD,
		GESTURE_HAND_SIGNAL,
		GESTURE_FIDGET,
		GESTURE_PLANT,
		GESTURE_DEFUSE,
	};

	// Pose parameters.
	bool		m_bPoseParameterInit;
	float		m_flEstimateYaw;
	float		m_flEstimateVelocity;
	float		m_flLastAimPitch;
	float		m_flLastAimYaw;
	float		m_flLastBodyHeight;
	float		m_flLastAimTurnTime;
	Vector2D	m_vecLastMoveYaw;
	int			m_iMoveX;
	int			m_iMoveY;
	int			m_iAimYaw;
	int			m_iAimPitch;
	int			m_iBodyHeight;

	float m_flFireIdleTime;				// Time that we drop our gun

	bool m_bLastDeployState;		// true = last was deployed, false = last was not deployed

	DODWeaponID m_iLastWeaponID;			// remember the weapon we were last using

	// Our DOD player pointer.
	CDODPlayer *m_pOuterDOD;
};


IDODPlayerAnimState* CreatePlayerAnimState( CDODPlayer *pPlayer )
{
	CDODPlayerAnimState *pState = new CDODPlayerAnimState;
	pState->InitDOD( pPlayer );
	return pState;
}


// -------------------------------------------------------------------------------- //
// CDODPlayerAnimState implementation.
// -------------------------------------------------------------------------------- //
											
CDODPlayerAnimState::CDODPlayerAnimState()
{
	m_bGettingDown = false;
	m_bGettingUp = false;
	m_bWasGoingProne = false;
	m_bWasGettingUp = false;

	m_pOuterDOD = NULL;

	m_bPoseParameterInit = false;
	m_flEstimateYaw = 0.0f;
	m_flLastAimPitch = 0.0f;
	m_flLastAimYaw = 0.0f;
	m_flLastBodyHeight = 0.0f;
	m_flLastAimTurnTime = 0.0f;
	m_vecLastMoveYaw.Init();
	m_iMoveX = -1;
	m_iMoveY = -1;
	m_iAimYaw = -1;
	m_iAimPitch = -1;
	m_iBodyHeight = -1;
}

void CDODPlayerAnimState::InitDOD( CDODPlayer *pPlayer )
{
	m_pOuterDOD = pPlayer;
	
	CModAnimConfig config;
	config.m_flMaxBodyYawDegrees = 45;
	config.m_LegAnimType = LEGANIM_GOLDSRC;
	config.m_bUseAimSequences = false;
	
	BaseClass::Init( pPlayer, config );
}


void CDODPlayerAnimState::ClearAnimationState()
{
	m_bJumping = false;
	m_flFireIdleTime = 0;
	m_bLastDeployState = false;
	m_iLastWeaponID = WEAPON_NONE;
	CancelGestures();
	BaseClass::ClearAnimationState();
}

void CDODPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	if ( event == PLAYERANIMEVENT_FIRE_GUN )
	{
		RestartGesture( GESTURE_ATTACK1, ACT_RANGE_ATTACK1, false );

		if( GetOuterDOD()->m_Shared.IsBazookaDeployed() )
		{
			m_flFireIdleTime = gpGlobals->curtime + 0.1;	// don't hold this pose after firing
		}
		else
		{
			// hold last frame of fire pose for 2 seconds ( if we are moving )
			m_flFireIdleTime = gpGlobals->curtime + 2;	
		}
	}
	if ( event == PLAYERANIMEVENT_SECONDARY_ATTACK )
	{
		CancelGestures();
		RestartGesture( GESTURE_ATTACK2, ACT_RANGE_ATTACK2 );
	}
	else if ( event == PLAYERANIMEVENT_RELOAD )
	{
		CancelGestures();
		RestartGesture( GESTURE_RELOAD, ACT_RELOAD );
	}
	else if ( event == PLAYERANIMEVENT_THROW_GRENADE )
	{
		CancelGestures();
		RestartGesture( GESTURE_ATTACK1, ACT_RANGE_ATTACK1 );
	}
	else if ( event == PLAYERANIMEVENT_ROLL_GRENADE )
	{
		CancelGestures();
		RestartGesture( GESTURE_ATTACK2, ACT_RANGE_ATTACK2 );
	}
	else if ( event == PLAYERANIMEVENT_JUMP )
	{
		// Play the jump animation.
		m_bJumping = true;
		m_bFirstJumpFrame = true;
		RestartMainSequence();
		m_flJumpStartTime = gpGlobals->curtime;
	}
	else if ( event == PLAYERANIMEVENT_HANDSIGNAL )
	{
		CDODPlayer *pPlayer = GetOuterDOD();
		if ( pPlayer && !( pPlayer->m_Shared.IsBazookaDeployed() || pPlayer->m_Shared.IsProne() || pPlayer->m_Shared.IsProneDeployed() || pPlayer->m_Shared.IsSniperZoomed() || pPlayer->m_Shared.IsSandbagDeployed() ) )
		{
			CancelGestures();
			RestartGesture( GESTURE_HAND_SIGNAL, ACT_DOD_HS_IDLE );
		}
	}
	else if ( event == PLAYERANIMEVENT_PLANT_TNT )
	{
		CancelGestures();
		RestartGesture( GESTURE_PLANT, ACT_DOD_PLANT_TNT );
	}
	else if ( event == PLAYERANIMEVENT_DEFUSE_TNT )
	{
		CancelGestures();
		RestartGesture( GESTURE_DEFUSE, ACT_DOD_DEFUSE_TNT );
	}
}

void CDODPlayerAnimState::ShowDebugInfo( void )
{
	if ( anim_showstate.GetInt() == m_pOuter->entindex() )
	{
		DebugShowAnimStateForPlayer( m_pOuter->IsServer() );
	}
}


void CDODPlayerAnimState::RestartMainSequence()
{
	CancelGestures();

	BaseClass::RestartMainSequence();
}

bool CDODPlayerAnimState::HandleJumping( Activity *idealActivity )
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
		if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f )
		{
			if ( m_pOuter->GetFlags() & FL_ONGROUND )
			{
				m_bJumping = false;
				RestartMainSequence();
			}
		}
	}
	if ( m_bJumping )
	{
		*idealActivity = ACT_HOP;
		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle the prone up animation.
//-----------------------------------------------------------------------------
bool CDODPlayerAnimState::HandleProneDown( CDODPlayer *pPlayer, Activity *idealActivity )
{
	if ( ( pPlayer->GetCycle() > 0.99f ) || ( pPlayer->m_Shared.IsProne() ) )
	{
		*idealActivity = ACT_PRONE_IDLE;
		if ( GetOuterXYSpeed() > MOVING_MINIMUM_SPEED )
		{
			*idealActivity = ACT_PRONE_FORWARD;
		}
		RestartMainSequence();

		m_bGettingDown = false;
	}
	else
	{
		*idealActivity = ACT_GET_DOWN_STAND;
		if ( pPlayer->GetFlags() & FL_DUCKING )
		{
			*idealActivity = ACT_GET_DOWN_CROUCH;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handle the prone up animation.
//-----------------------------------------------------------------------------
bool CDODPlayerAnimState::HandleProneUp( CDODPlayer *pPlayer, Activity *idealActivity )
{
	if ( ( m_pOuter->GetCycle() > 0.99f ) || ( !pPlayer->m_Shared.IsGettingUpFromProne() ) )
	{
		m_bGettingUp = false;
		RestartMainSequence();

		return false;
	}

	*idealActivity = ACT_GET_UP_STAND;
	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		*idealActivity = ACT_GET_UP_CROUCH;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handle the prone animations.
//-----------------------------------------------------------------------------
bool CDODPlayerAnimState::HandleProne( Activity *idealActivity )
{
	// Get the player.
	CDODPlayer *pPlayer = GetOuterDOD();
	if ( !pPlayer )
		return false;

	// Find the leading edge on going prone.
	bool bChange = pPlayer->m_Shared.IsGoingProne() && !m_bWasGoingProne;
	m_bWasGoingProne = pPlayer->m_Shared.IsGoingProne();
	if ( bChange )
	{
		m_bGettingDown = true;
		RestartMainSequence();
	}

	// Find the leading edge on getting up.
	bChange = pPlayer->m_Shared.IsGettingUpFromProne() && !m_bWasGettingUp;
	m_bWasGettingUp = pPlayer->m_Shared.IsGettingUpFromProne();
	if ( bChange )
	{
		m_bGettingUp = true;
		RestartMainSequence();
	}

	// Handle the transitions.
	if ( m_bGettingDown )
	{
		return HandleProneDown( pPlayer, idealActivity );
	}
	else if ( m_bGettingUp )
	{
		return HandleProneUp( pPlayer, idealActivity );
	}

	// Handle the prone state.
	if ( pPlayer->m_Shared.IsProne() )
	{
		*idealActivity = ACT_PRONE_IDLE;
		if ( GetOuterXYSpeed() > MOVING_MINIMUM_SPEED )
		{
			*idealActivity = ACT_PRONE_FORWARD;
		}

		return true;
	}

	return false;
}

bool CDODPlayerAnimState::HandleDucked( Activity *idealActivity )
{
	if ( m_pOuter->GetFlags() & FL_DUCKING )
	{
		if ( GetOuterXYSpeed() > MOVING_MINIMUM_SPEED )
			*idealActivity = ACT_RUN_CROUCH;
		else
			*idealActivity = ACT_CROUCHIDLE;
		
		return true;
	}
	else
	{
		return false;
	}
}

Activity CDODPlayerAnimState::CalcMainActivity()
{
	Activity idealActivity = ACT_IDLE;

	float flSpeed = GetOuterXYSpeed();

	if ( HandleJumping( &idealActivity ) || 
		HandleProne( &idealActivity ) ||
		HandleDucked( &idealActivity ) )
	{
		// intentionally blank
	}
	else
	{
		if ( flSpeed > MOVING_MINIMUM_SPEED )
		{
			if( flSpeed >= DOD_SPRINT_SPEED )
			{
				idealActivity = ACT_SPRINT;
				
				// If we sprint, cancel the fire idle time
				CancelGestures();
			}
			else if( flSpeed >= DOD_WALK_SPEED )
				idealActivity = ACT_RUN;
			else
				idealActivity = ACT_WALK;
		}
	}

	// Shouldn't be here but we need to ship - bazooka deployed reload/running check.
	if ( IsPlayingGesture( GESTURE_RELOAD ) )
	{
		if ( flSpeed >= DOD_RUN_SPEED && m_pOuterDOD->m_Shared.IsBazookaOnlyDeployed() )
		{
			CancelGestures();
		}
	}

	ShowDebugInfo();

	// Client specific.
#ifdef CLIENT_DLL

	if ( anim_showmainactivity.GetBool() )
	{
		DebugShowActivity( idealActivity );
	}

#endif

	return idealActivity;
}

void CDODPlayerAnimState::CancelGestures( void )
{
	m_bPlayingGesture = false;
	m_iGestureType = GESTURE_NONE;

#ifdef CLIENT_DLL	
	m_iGestureSequence = -1;
#else
	m_pOuter->RemoveAllGestures();
#endif 
}

void CDODPlayerAnimState::RestartGesture( int iGestureType, Activity act, bool bAutoKill /* = true */ )
{
	Activity idealActivity = TranslateActivity( act );
	m_bPlayingGesture = true;
	m_iGestureType = iGestureType;

#ifdef CLIENT_DLL
	m_iGestureSequence = m_pOuter->SelectWeightedSequence( idealActivity );

	if( m_iGestureSequence == -1 )
	{
		m_bPlayingGesture = false;
	}

	m_flGestureCycle = 0.0f;
	m_bAutokillGesture = bAutoKill;
#else
	m_pOuterDOD->RestartGesture( idealActivity, true, bAutoKill );
#endif
}

Activity CDODPlayerAnimState::TranslateActivity( Activity actDesired )
{
	Activity idealActivity = actDesired;

	if ( m_pOuterDOD->m_Shared.IsSandbagDeployed() )
	{
		switch( idealActivity )
		{
		case ACT_IDLE:
			idealActivity = ACT_DOD_DEPLOYED;
			break;
		case ACT_RANGE_ATTACK1:
			idealActivity = ACT_DOD_PRIMARYATTACK_DEPLOYED;
			break;
		case ACT_RELOAD:
			idealActivity = ACT_DOD_RELOAD_DEPLOYED;
			break;
		default:
			break;
		}
	} 
	else if ( m_pOuterDOD->m_Shared.IsProneDeployed() )
	{
		switch( idealActivity )
		{
		case ACT_PRONE_IDLE:
			idealActivity = ACT_DOD_PRONE_DEPLOYED;
			break;
		case ACT_RANGE_ATTACK1:
			idealActivity = ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED;
			break;
		case ACT_RELOAD:
			idealActivity = ACT_DOD_RELOAD_PRONE_DEPLOYED;
			break;
		default:
			break;
		}
	} 
	else if ( m_pOuterDOD->m_Shared.IsSniperZoomed() || m_pOuterDOD->m_Shared.IsBazookaDeployed() )
	{
		switch( idealActivity )
		{
		case ACT_IDLE:
			idealActivity = ACT_DOD_IDLE_ZOOMED;
			break;
		case ACT_WALK:
			idealActivity = ACT_DOD_WALK_ZOOMED;
			break;
		case ACT_CROUCHIDLE:
			idealActivity = ACT_DOD_CROUCH_ZOOMED;
			break;
		case ACT_RUN_CROUCH:
			idealActivity = ACT_DOD_CROUCHWALK_ZOOMED;
			break;
		case ACT_PRONE_IDLE:
			idealActivity = ACT_DOD_PRONE_ZOOMED;
			break;
		case ACT_PRONE_FORWARD:
			idealActivity = ACT_DOD_PRONE_FORWARD_ZOOMED;
			break;
		case ACT_RANGE_ATTACK1:
			if ( m_pOuterDOD->m_Shared.IsSniperZoomed() )
			{
				if( m_pOuterDOD->m_Shared.IsProne() )
					idealActivity = ACT_DOD_PRIMARYATTACK_PRONE;
			}
			break;
		case ACT_RELOAD:
			if ( m_pOuterDOD->m_Shared.IsBazookaDeployed() )
			{
				if( m_pOuterDOD->m_Shared.IsProne() )
				{
					idealActivity = ACT_DOD_RELOAD_PRONE_DEPLOYED;
				}
				else
				{
					idealActivity = ACT_DOD_RELOAD_DEPLOYED;
				}
			}
			break;
		default:
			break;
		}
	}
	else if ( m_pOuterDOD->m_Shared.IsProne() )
	{
		// translate prone shooting, reload, handsignal

		switch( idealActivity )
		{
		case ACT_RANGE_ATTACK1:
			idealActivity = ACT_DOD_PRIMARYATTACK_PRONE;
			break;
		case ACT_RANGE_ATTACK2:
			idealActivity = ACT_DOD_SECONDARYATTACK_PRONE;
			break;
		case ACT_RELOAD:			
			idealActivity = ACT_DOD_RELOAD_PRONE;
			break;
		default:
			break;
		}
	}
	else if ( m_pOuter->GetFlags() & FL_DUCKING )
	{
		switch( idealActivity )
		{
		case ACT_RANGE_ATTACK1:
			idealActivity = ACT_DOD_PRIMARYATTACK_CROUCH;
			break;
		case ACT_RANGE_ATTACK2:
			idealActivity = ACT_DOD_SECONDARYATTACK_CROUCH;
			break;
		case ACT_DOD_HS_IDLE:
			idealActivity = ACT_DOD_HS_CROUCH;
			break;
		}
	}

	// Are our guns at fire or rest?
	if ( m_flFireIdleTime > gpGlobals->curtime )
	{
		switch( idealActivity )
		{
		case ACT_IDLE:			idealActivity = ACT_DOD_STAND_AIM; break;
		case ACT_CROUCHIDLE:	idealActivity = ACT_DOD_CROUCH_AIM; break;
		case ACT_RUN_CROUCH:	idealActivity = ACT_DOD_CROUCHWALK_AIM; break;
		case ACT_WALK:			idealActivity = ACT_DOD_WALK_AIM; break;
		case ACT_RUN:			idealActivity = ACT_DOD_RUN_AIM; break;
		default: break;
		}
	}
	else
	{
		switch( idealActivity )
		{
		case ACT_IDLE:			idealActivity = ACT_DOD_STAND_IDLE; break;
		case ACT_CROUCHIDLE:	idealActivity = ACT_DOD_CROUCH_IDLE; break;
		case ACT_RUN_CROUCH:	idealActivity = ACT_DOD_CROUCHWALK_IDLE; break;
		case ACT_WALK:			idealActivity = ACT_DOD_WALK_IDLE; break;
		case ACT_RUN:			idealActivity = ACT_DOD_RUN_IDLE; break;
		default: break;
		}
	}

	return m_pOuterDOD->TranslateActivity( idealActivity );
}

CDODPlayer* CDODPlayerAnimState::GetOuterDOD() const
{
	return m_pOuterDOD;
}

float CDODPlayerAnimState::GetCurrentMaxGroundSpeed()
{
	return PLAYER_SPEED_SPRINT; 
}

float CDODPlayerAnimState::CalcMovementPlaybackRate( bool *bIsMoving )
{
	if( ( GetCurrentMainSequenceActivity() == ACT_GET_UP_STAND ) || ( GetCurrentMainSequenceActivity() == ACT_GET_DOWN_STAND ) ||
		( GetCurrentMainSequenceActivity() == ACT_GET_UP_CROUCH ) || ( GetCurrentMainSequenceActivity() == ACT_GET_DOWN_CROUCH ) )
	{
		// We don't want to change the playback speed of these, even if we move.
		*bIsMoving = false;
		return 1.0;
	}

	// it would be a good idea to ramp up from 0.5 to 1.0 as they go from stop to moveing, it looks more natural.
	*bIsMoving = true;
	return 1.0;
}

void CDODPlayerAnimState::DebugShowAnimState( int iStartLine )
{
#ifdef CLIENT_DLL
	engine->Con_NPrintf( iStartLine++, "getting down: %s\n", m_bGettingDown ? "yes" : "no" );
	engine->Con_NPrintf( iStartLine++, "getting up: %s\n", m_bGettingUp ? "yes" : "no" );
#endif

	BaseClass::DebugShowAnimState( iStartLine );
}

void CDODPlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
	// Reset some things if we're changed weapons
	// do this before ComputeSequences
	CWeaponDODBase *pWeapon = GetOuterDOD()->m_Shared.GetActiveDODWeapon();
	if ( pWeapon )
	{	
		if( pWeapon->GetWeaponID() != m_iLastWeaponID )
		{
			CancelGestures();
			m_iLastWeaponID = pWeapon->GetWeaponID();
			m_flFireIdleTime = 0;
		}
	}

	BaseClass::ComputeSequences( pStudioHdr );

	if( !m_bGettingDown && !m_bGettingUp )
	{
		ComputeFireSequence();
	
#ifdef CLIENT_DLL

		ComputeGestureSequence( pStudioHdr );

		// get the weapon's swap criteria ( reload? attack? deployed? deployed reload? )
		// and determine whether we should use alt model or not

		CWeaponDODBase *pWeapon = GetOuterDOD()->m_Shared.GetActiveDODWeapon();
		if ( pWeapon )
		{	
			int iCurrentState = ALTWPN_CRITERIA_NONE;

			if( m_bPlayingGesture && m_iGestureType == GESTURE_ATTACK1 )
				iCurrentState |= ALTWPN_CRITERIA_FIRING;

			else if( m_bPlayingGesture && m_iGestureType == GESTURE_RELOAD )
				iCurrentState |= ALTWPN_CRITERIA_RELOADING;

			if( m_pOuterDOD->m_Shared.IsProne() )
				iCurrentState |= ALTWPN_CRITERIA_PRONE;

			// always use default model while proning or hand signal
			if( !IsPlayingGesture( GESTURE_HAND_SIGNAL ) &&
				!IsPlayingGesture( GESTURE_FIDGET ) &&
				!m_bGettingDown &&
				!m_bGettingUp )
			{
				pWeapon->CheckForAltWeapon( iCurrentState );
			}
			else
			{
				pWeapon->SetUseAltModel( false );
			}
		}
#endif
	}
}

#define GESTURE_LAYER			AIMSEQUENCE_LAYER
#define NUM_LAYERS_WANTED		(GESTURE_LAYER + 1)

void CDODPlayerAnimState::ClearAnimationLayers()
{
	if ( !m_pOuter )
		return;

	m_pOuter->SetNumAnimOverlays( NUM_LAYERS_WANTED );
	for ( int i=0; i < m_pOuter->GetNumAnimOverlays(); i++ )
	{
		m_pOuter->GetAnimOverlay( i )->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
	}
}

void CDODPlayerAnimState::ComputeFireSequence( void )
{
	// Hold the shoot pose for a time after firing, unless we stand still
	if( m_flFireIdleTime < gpGlobals->curtime &&
		IsPlayingGesture( GESTURE_ATTACK1 ) &&
		GetOuterXYSpeed() > MOVING_MINIMUM_SPEED )
	{
		CancelGestures();
	}

	if( GetOuterDOD()->m_Shared.IsInMGDeploy() != m_bLastDeployState )
	{
		CancelGestures();

		m_bLastDeployState = GetOuterDOD()->m_Shared.IsInMGDeploy();
	}
}

void CDODPlayerAnimState::ComputeGestureSequence( CStudioHdr *pStudioHdr )
{
	UpdateLayerSequenceGeneric( pStudioHdr, GESTURE_LAYER, m_bPlayingGesture, m_flGestureCycle, m_iGestureSequence, !m_bAutokillGesture );
}

void CDODPlayerAnimState::UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd, float flWeight /* = 1.0 */ )
{
	if ( !bEnabled )
		return;

	if( flCurCycle > 1.0 )
		flCurCycle = 1.0;

	// Increment the fire sequence's cycle.
	flCurCycle += m_pOuter->GetSequenceCycleRate( pStudioHdr, iSequence ) * gpGlobals->frametime;
	if ( flCurCycle > 1 )
	{
		if ( bWaitAtEnd )
		{
			flCurCycle = 1;
		}
		else
		{
			// Not firing anymore.
			bEnabled = false;
			iSequence = 0;
			return;
		}
	}

	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iLayer );

	pLayer->m_flCycle = flCurCycle;
	pLayer->m_nSequence = iSequence;

	pLayer->m_flPlaybackRate = 1.0;
	pLayer->m_flWeight = flWeight;
	pLayer->m_nOrder = iLayer;
	
}

extern ConVar mp_facefronttime;
extern ConVar mp_feetyawrate;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::Update( float eyeYaw, float eyePitch )
{
	// Profile the animation update.
	VPROF( "CDODPlayerAnimState::Update" );

	// Get the studio header for the player.
	CStudioHdr *pStudioHdr = GetOuterDOD()->GetModelPtr();
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

	// Clear animation overlays because we're about to completely reconstruct them.
	ClearAnimationLayers();

	// Compute the player sequences.
	ComputeSequences( pStudioHdr );

	if ( SetupPoseParameters( pStudioHdr ) )
	{
		// Pose parameter - what direction are the player's legs running in.
		ComputePoseParam_MoveYaw( pStudioHdr );

		// Pose parameter - Torso aiming (up/down).
		ComputePoseParam_AimPitch( pStudioHdr );

		// Pose parameter - Torso aiming (rotation).
		ComputePoseParam_AimYaw( pStudioHdr );

		// Pose parameter - Body Height (torso elevation).
		ComputePoseParam_BodyHeight( pStudioHdr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDODPlayerAnimState::SetupPoseParameters( CStudioHdr *pStudioHdr )
{
	// Check to see if this has already been done.
	if ( m_bPoseParameterInit )
		return true;

	// Save off the pose parameter indices.
	if ( !pStudioHdr )
		return false;

	// Look for the movement blenders.
	m_iMoveX = GetOuterDOD()->LookupPoseParameter( pStudioHdr, "move_x" );
	m_iMoveY = GetOuterDOD()->LookupPoseParameter( pStudioHdr, "move_y" );
	if ( ( m_iMoveX < 0 ) || ( m_iMoveY < 0 ) )
		return false;

	// Look for the aim pitch blender.
	m_iAimPitch = GetOuterDOD()->LookupPoseParameter( pStudioHdr, "body_pitch" );
	if ( m_iAimPitch < 0 )
		return false;

	// Look for aim yaw blender.
	m_iAimYaw = GetOuterDOD()->LookupPoseParameter( pStudioHdr, "body_yaw" );
	if ( m_iAimYaw < 0 )
		return false;

	// Look for the body height blender.
	m_iBodyHeight = GetOuterDOD()->LookupPoseParameter( pStudioHdr, "body_height" );
	if ( m_iBodyHeight < 0 )
		return false;

	m_bPoseParameterInit = true;

	return true;
}

#define DOD_MOVEMENT_ERROR_LIMIT  1.0

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr )
{
	// Check to see if we are deployed or prone.
	if( GetOuterDOD()->m_Shared.IsInMGDeploy() || GetOuterDOD()->m_Shared.IsProne() )
	{
		// Set the 9-way blend movement pose parameters.
		Vector2D vecCurrentMoveYaw( 0.0f, 0.0f );
		GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveX, vecCurrentMoveYaw.x );
		GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveY, vecCurrentMoveYaw.y );

		m_vecLastMoveYaw = vecCurrentMoveYaw;

#if 0
		// Rotate the entire body instantly.
		m_flGoalFeetYaw = AngleNormalize( m_flEyeYaw );
		m_flCurrentFeetYaw = m_flGoalFeetYaw;
		m_flLastTurnTime = gpGlobals->curtime;

		// Rotate entire body into position.
		m_angRender[YAW] = m_flCurrentFeetYaw;
		m_angRender[PITCH] = m_angRender[ROLL] = 0;

		SetOuterBodyYaw( m_flCurrentFeetYaw );
		g_flLastBodyYaw = m_flCurrentFeetYaw;
#endif
	}
	else
	{
		// Get the estimated movement yaw.
		EstimateYaw();

		// Get the view yaw.
		float flAngle = AngleNormalize( m_flEyeYaw );

		// rotate movement into local reference frame
		float flYaw = flAngle - m_flEstimateYaw;
		flYaw = AngleNormalize( -flYaw );

		// Get the current speed the character is running.
		Vector vecEstVelocity;
		vecEstVelocity.x = cos( DEG2RAD( flYaw ) ) * m_flEstimateVelocity;
		vecEstVelocity.y = sin( DEG2RAD( flYaw ) ) * m_flEstimateVelocity;

		Vector2D vecCurrentMoveYaw( 0.0f, 0.0f );
		// set the pose parameters to the correct direction, but not value
		if ( vecEstVelocity.x != 0.0f && fabs( vecEstVelocity.x ) > fabs( vecEstVelocity.y ) )
		{
			vecCurrentMoveYaw.x = (vecEstVelocity.x < 0.0) ? -1.0 : 1.0;
			vecCurrentMoveYaw.y = vecEstVelocity.y / fabs( vecEstVelocity.x );
		}
		else if (vecEstVelocity.y != 0.0f)
		{
			vecCurrentMoveYaw.y = (vecEstVelocity.y < 0.0) ? -1.0 : 1.0;
			vecCurrentMoveYaw.x = vecEstVelocity.x / fabs( vecEstVelocity.y );
		}

#ifndef CLIENT_DLL
		GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveX, vecCurrentMoveYaw.x );
		GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveY, vecCurrentMoveYaw.y );
#else

		// refine pose parameters to be more accurate
		int i = 0;
		float dx, dy;
		Vector vecAnimVelocity;

		/*
		if ( m_pOuter->entindex() == 2 )
		{
			GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveX, vecCurrentMoveYaw.x );
			GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveY, vecCurrentMoveYaw.y );
			GetOuterDOD()->GetBlendedLinearVelocity( &vecAnimVelocity );
			DevMsgRT("(%.2f) %.3f : (%.2f) %.3f\n", vecAnimVelocity.x, vecCurrentMoveYaw.x, vecAnimVelocity.y, vecCurrentMoveYaw.y );
		}
		*/

		bool retry = true;
		do 
		{
			// Set the 9-way blend movement pose parameters.
			GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveX, vecCurrentMoveYaw.x );
			GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveY, vecCurrentMoveYaw.y );

			GetOuterDOD()->GetBlendedLinearVelocity( &vecAnimVelocity );

			// adjust X pose parameter based on movement error
			if (fabs( vecAnimVelocity.x ) > 0.001)
			{
				vecCurrentMoveYaw.x *= vecEstVelocity.x / vecAnimVelocity.x;
			}
			else
			{
				vecCurrentMoveYaw.x = 0;
				GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveX, vecCurrentMoveYaw.x );
			}
			// adjust Y pose parameter based on movement error
			if (fabs( vecAnimVelocity.y ) > 0.001)
			{
				vecCurrentMoveYaw.y *= vecEstVelocity.y / vecAnimVelocity.y;
			}
			else
			{
				vecCurrentMoveYaw.y = 0;
				GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveY, vecCurrentMoveYaw.y );
			}

			dx =  vecEstVelocity.x - vecAnimVelocity.x;
			dy =  vecEstVelocity.y - vecAnimVelocity.y;

			retry = (vecCurrentMoveYaw.x < 1.0 && vecCurrentMoveYaw.x > -1.0) && (dx < -DOD_MOVEMENT_ERROR_LIMIT || dx > DOD_MOVEMENT_ERROR_LIMIT);
			retry = retry || ((vecCurrentMoveYaw.y < 1.0 && vecCurrentMoveYaw.y > -1.0) && (dy < -DOD_MOVEMENT_ERROR_LIMIT || dy > DOD_MOVEMENT_ERROR_LIMIT));

		} while (i++ < 5 && retry);

		/*
		if ( m_pOuter->entindex() == 2 )
		{
			DevMsgRT("%d(%.2f : %.2f) %.3f : (%.2f : %.2f) %.3f\n", 
				i,
				vecEstVelocity.x, vecAnimVelocity.x, vecCurrentMoveYaw.x, 
				vecEstVelocity.y, vecAnimVelocity.y, vecCurrentMoveYaw.y );
		}
		*/
#endif

		m_vecLastMoveYaw = vecCurrentMoveYaw;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::EstimateYaw( void )
{
	// Get the frame time.
	float flDeltaTime = gpGlobals->frametime;
	if ( flDeltaTime == 0.0f )
	{
		// FIXME: why does this short circuit?
		m_flEstimateVelocity = 0.0;
		m_flEstimateYaw = 0.0;
		return;
	}

	// Get the player's velocity and angles.
	Vector vecEstVelocity;
	GetOuterAbsVelocity( vecEstVelocity );
	QAngle angles = GetOuterDOD()->GetLocalAngles();

	// If we are not moving, sync up the feet and eyes slowly.
	if ( vecEstVelocity.x == 0.0f && vecEstVelocity.y == 0.0f )
	{
		float flYawDelta = angles[YAW] - m_flEstimateYaw;
		flYawDelta = AngleNormalize( flYawDelta );

		if ( flDeltaTime < 0.25f )
		{
			flYawDelta *= ( flDeltaTime * 4.0f );
		}
		else
		{
			flYawDelta *= flDeltaTime;
		}

		m_flEstimateVelocity = 0.0;
		m_flEstimateYaw += flYawDelta;
		AngleNormalize( m_flEstimateYaw );
	}
	else
	{
		m_flEstimateVelocity = vecEstVelocity.Length2D();
		m_flEstimateYaw = ( atan2( vecEstVelocity.y, vecEstVelocity.x ) * 180.0f / M_PI );
		m_flEstimateYaw = clamp( m_flEstimateYaw, -180.0f, 180.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::ComputePoseParam_AimPitch( CStudioHdr *pStudioHdr )
{
	// Get the view pitch.
	float flAimPitch = m_flEyePitch;

	// Lock pitch at 0 if a reload gesture is playing
#ifdef CLIENT_DLL
	if ( IsPlayingGesture( GESTURE_RELOAD ) )
		flAimPitch = 0;
#else
	Activity idealActivity = TranslateActivity( ACT_RELOAD );

	if ( m_pOuter->IsPlayingGesture( idealActivity ) )
		flAimPitch = 0;
#endif

	// Set the aim pitch pose parameter and save.
	GetOuter()->SetPoseParameter( pStudioHdr, m_iAimPitch, -flAimPitch );
	m_flLastAimPitch = flAimPitch;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::ComputePoseParam_AimYaw( CStudioHdr *pStudioHdr )
{
	// Get the movement velocity.
	Vector vecVelocity;
	GetOuterAbsVelocity( vecVelocity );

	// Check to see if we are moving.
	bool bMoving = ( vecVelocity.Length() > 1.0f ) ? true : false;

	// Check our prone and deployed state.
	bool bDeployed = GetOuterDOD()->m_Shared.IsSandbagDeployed() || GetOuterDOD()->m_Shared.IsProneDeployed();
	bool bProne = GetOuterDOD()->m_Shared.IsProne();

	// If we are moving or are prone and undeployed.
	if ( bMoving || ( bProne && !bDeployed ) )
	{
		// The feet match the eye direction when moving - the move yaw takes care of the rest.
		m_flGoalFeetYaw = m_flEyeYaw;
	}
	// Else if we are not moving.
	else
	{
		// Initialize the feet.
		if ( m_flLastAimTurnTime <= 0.0f )
		{
			m_flGoalFeetYaw	= m_flEyeYaw;
			m_flCurrentFeetYaw = m_flEyeYaw;
			m_flLastAimTurnTime = gpGlobals->curtime;
		}
		// Make sure the feet yaw isn't too far out of sync with the eye yaw.
		// TODO: Do something better here!
		else
		{
			float flYawDelta = AngleNormalize(  m_flGoalFeetYaw - m_flEyeYaw );

			if ( bDeployed )
			{
				if ( fabs( flYawDelta ) > 20.0f )
				{
					float flSide = ( flYawDelta > 0.0f ) ? -1.0f : 1.0f;
					m_flGoalFeetYaw += ( 20.0f * flSide );
				}
			}
			else
			{
				if ( fabs( flYawDelta ) > m_AnimConfig.m_flMaxBodyYawDegrees )
				{
					float flSide = ( flYawDelta > 0.0f ) ? -1.0f : 1.0f;
					m_flGoalFeetYaw += ( m_AnimConfig.m_flMaxBodyYawDegrees * flSide );
				}
			}
		}
	}

	// Fix up the feet yaw.
	m_flGoalFeetYaw = AngleNormalize( m_flGoalFeetYaw );
	if ( m_flGoalFeetYaw != m_flCurrentFeetYaw )
	{
		ConvergeYawAngles( m_flGoalFeetYaw, DOD_BODYYAW_RATE, gpGlobals->frametime, m_flCurrentFeetYaw );
		m_flLastAimTurnTime = gpGlobals->curtime;
	}

	// Rotate the body into position.
	m_angRender[YAW] = m_flCurrentFeetYaw;

	// Find the aim(torso) yaw base on the eye and feet yaws.
	float flAimYaw = m_flEyeYaw - m_flCurrentFeetYaw;
	flAimYaw = AngleNormalize( flAimYaw );

	// Set the aim yaw and save.
	GetOuter()->SetPoseParameter( pStudioHdr, m_iAimYaw, -flAimYaw );
	m_flLastAimYaw	= flAimYaw;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::ConvergeYawAngles( float flGoalYaw, float flYawRate, float flDeltaTime, float &flCurrentYaw )
{
#define FADE_TURN_DEGREES 60.0f

	// Find the yaw delta.
	float flDeltaYaw = flGoalYaw - flCurrentYaw;
	float flDeltaYawAbs = fabs( flDeltaYaw );
	flDeltaYaw = AngleNormalize( flDeltaYaw );

	// Always do at least a bit of the turn (1%).
	float flScale = 1.0f;
	flScale = flDeltaYawAbs / FADE_TURN_DEGREES;
	flScale = clamp( flScale, 0.01f, 1.0f );

	float flYaw = flYawRate * flDeltaTime * flScale;
	if ( flDeltaYawAbs < flYaw )
	{
		flCurrentYaw = flGoalYaw;
	}
	else
	{
		float flSide = ( flDeltaYaw < 0.0f ) ? -1.0f : 1.0f;
		flCurrentYaw += ( flYaw * flSide );
	}

	flCurrentYaw = AngleNormalize( flCurrentYaw );

#undef FADE_TURN_DEGREES
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::ComputePoseParam_BodyHeight( CStudioHdr *pStudioHdr )
{
	if( m_pOuterDOD->m_Shared.IsSandbagDeployed() )
	{
//		float flHeight = m_pOuterDOD->m_Shared.GetDeployedHeight() - 4.0f;
		float flHeight = m_pOuterDOD->m_Shared.GetDeployedHeight() - dod_bodyheightoffset.GetFloat();
		GetOuter()->SetPoseParameter( pStudioHdr, m_iBodyHeight, flHeight );
		m_flLastBodyHeight = flHeight;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Anim_StateLog( const char *pMsg, ... )
{
	// Format the string.
	char str[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( str, sizeof( str ), pMsg, marker );
	va_end( marker );

	// Log it?	
	if ( anim_showstatelog.GetInt() == 1 || anim_showstatelog.GetInt() == 3 )
	{
		Msg( "%s", str );
	}

	if ( anim_showstatelog.GetInt() > 1 )
	{
//		static FileHandle_t hFile = filesystem->Open( "AnimState.log", "wt" );
//		filesystem->FPrintf( hFile, "%s", str );
//		filesystem->Flush( hFile );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Anim_StatePrintf( int iLine, const char *pMsg, ... )
{
	// Format the string.
	char str[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( str, sizeof( str ), pMsg, marker );
	va_end( marker );

	// Show it with Con_NPrintf.
	engine->Con_NPrintf( iLine, "%s", str );

	// Log it.
	Anim_StateLog( "%s\n", str );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::DebugShowAnimStateForPlayer( bool bIsServer )
{
	// Get the player's velocity.
	Vector vecVelocity;
	GetOuterAbsVelocity( vecVelocity );

	// Start animation state logging.
	int iLine = 5;
	if ( bIsServer )
	{
		iLine = 12;
	}
//	Anim_StateLog( "-------------%s: frame %d -----------------\n", bIsServer ? "Server" : "Client", gpGlobals->framecount );
	Anim_StatePrintf( iLine++, "-------------%s: frame %d -----------------\n", bIsServer ? "Server" : "Client", gpGlobals->framecount );

	// Write out the main sequence and its data.
	Anim_StatePrintf( iLine++, "Main: %s, Cycle: %.2f\n", GetSequenceName( GetOuter()->GetModelPtr(), GetOuter()->GetSequence() ), GetOuter()->GetCycle() );

	// Write out the layers and their data.
	for ( int iAnim = 0; iAnim < GetOuter()->GetNumAnimOverlays(); ++iAnim )
	{
		CAnimationLayer *pLayer = GetOuter()->GetAnimOverlay( iAnim );
		if ( pLayer && ( pLayer->m_nOrder != CBaseAnimatingOverlay::MAX_OVERLAYS ) )
		{
			Anim_StatePrintf( iLine++, "Layer %s: Weight: %.2f, Cycle: %.2f", GetSequenceName( GetOuter()->GetModelPtr(), pLayer->m_nSequence ), (float)pLayer->m_flWeight, (float)pLayer->m_flCycle );
		}
	}

	// Write out the speed data.
	Anim_StatePrintf( iLine++, "Time: %.2f, Speed: %.2f, MaxSpeed: %.2f", gpGlobals->curtime, vecVelocity.Length2D(), GetCurrentMaxGroundSpeed() );

	// Write out the 9-way blend data.
	Anim_StatePrintf( iLine++, "EntityYaw: %.2f, AimYaw: %.2f, AimPitch: %.2f, MoveX: %.2f, MoveY: %.2f Body: %.2f", m_angRender[YAW], m_flLastAimYaw, m_flLastAimPitch, m_vecLastMoveYaw.x, m_vecLastMoveYaw.y, m_flLastBodyHeight );

//	Anim_StateLog( "--------------------------------------------\n\n" );
	Anim_StatePrintf( iLine++, "--------------------------------------------\n\n" );

	DebugShowEyeYaw();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::DebugShowEyeYaw( void )
{
#ifdef _NDEBUG

	float flBaseSize = 10;
	float flHeight = 80;

	Vector vecPos = GetOuter()->GetAbsOrigin() + Vector( 0.0f, 0.0f, 3.0f );
	QAngle angles( 0.0f, 0.0f, 0.0f );

	angles[YAW] = m_flEyeYaw;

	Vector vecForward, vecRight, vecUp;
	AngleVectors( angles, &vecForward, &vecRight, &vecUp );

	// Draw a red triangle on the ground for the eye yaw.
	debugoverlay->AddTriangleOverlay( ( vecPos + vecRight * flBaseSize / 2.0f ), 
		( vecPos - vecRight * flBaseSize / 2.0f ), 
		( vecPos + vecForward * flHeight, 255, 0, 0, 255, false, 0.01f );

#endif
}

// Client specific debug functions.
#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::DebugShowActivity( Activity activity )
{
#ifdef _DEBUG

	const char *pszActivity = "other";

	switch( activity )
	{
	case ACT_IDLE:
		{
			pszActivity = "idle";
			break;
		}
	case ACT_SPRINT:
		{
			pszActivity = "sprint";
			break;
		}
	case ACT_WALK:
		{
			pszActivity = "walk";
			break;
		}
	case ACT_RUN:
		{
			pszActivity = "run";
			break;
		}
	}

	Msg( "Activity: %s\n", pszActivity );

#endif
}

#endif
