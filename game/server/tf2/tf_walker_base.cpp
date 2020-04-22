//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "tf_walker_base.h"


#include "in_buttons.h"
#include "shake.h"


static float MAX_WALKER_VEL = 100;


IMPLEMENT_SERVERCLASS_ST( CWalkerBase, DT_WalkerBase )
END_SEND_TABLE()


CWalkerBase::CWalkerBase()
{
	m_vSteerVelocity.Init();
	m_iMovePoseParamX = -1;
	m_iMovePoseParamY = -1;
	m_bWalkMode = false;
	m_flDontMakeSoundsUntil = 0;
	m_flPlaybackSpeedBoost = 1;
	m_flVelocityDecayRate = 80;
	m_LastButtons = 0;
	m_vLastCmdViewAngles.Init();
}


void CWalkerBase::SpawnWalker(
	const char *pModelName,
	int objectType,
	const Vector &vPlacementMins,
	const Vector &vPlacementMaxs,
	int iHealth,
	int nMaxPassengers,
	float flPlaybackSpeedBoost
	)
{
	SetModel( pModelName );
	SetType( objectType );

	UTIL_SetSize( this, vPlacementMins, vPlacementMaxs );
	m_iHealth = iHealth;
	m_flPlaybackSpeedBoost = flPlaybackSpeedBoost;

	m_takedamage = DAMAGE_YES;
	SetMaxPassengerCount( nMaxPassengers );
	
	

	// The model should be set before the derived class calls our Spawn().
	Assert( GetModel() );

	// By default, all walkers use the walk_box animation as they move.
	m_iMovePoseParamX = LookupPoseParameter( "move_x" );
	m_iMovePoseParamY = LookupPoseParameter( "move_y" );
	EnableWalkMode( true );

	// The base class spawn sets a default collision group, so this needs to
	// be called post.
	SetCollisionGroup( COLLISION_GROUP_VEHICLE );
	
	BaseClass::Spawn();


	// HACKHACK: this is just so CBaseObject doesn't call StudioFrameAdvance for us. We should probably have 
	// a specific flag for this behavior.
	m_fObjectFlags |= OF_DOESNT_HAVE_A_MODEL;
	m_fObjectFlags &= ~OF_MUST_BE_BUILT_ON_ATTACHMENT;


	// We animate, so let's not use manual mode for now.
	SetMoveType( MOVETYPE_STEP );
	AddSolidFlags( FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST );
	
	EnableServerIK();

	SetContextThink( WalkerThink, gpGlobals->curtime, "WalkerThink" );
}

void CWalkerBase::EnableWalkMode( bool bEnable )
{
	m_bWalkMode = bEnable;

	// Stop any movement..
	m_vSteerVelocity.Init();

	if ( bEnable )
	{
		ResetSequence( LookupSequence( "walk_box" ) );

		// HACK: there should be a better way to this.. like CBaseAnimating::ResetAnimation,
		// or ResetSequence should do it.
		SetCycle( 0 );
	}
}


void CWalkerBase::AdjustInitialBuildAngles()
{
	QAngle vNewAngles = GetAbsAngles();
	vNewAngles[YAW] += 90;
	SetAbsAngles( vNewAngles );
}


void CWalkerBase::WalkerThink()
{
	float dt = GetTimeDelta();
	
	// Decay our velocity.
	if ( m_bWalkMode )
	{
		m_flPlaybackRate = m_flPlaybackSpeedBoost;


		float flDecayRate = m_flVelocityDecayRate;

		float flLen = m_vSteerVelocity.Length();
		Vector2DNormalize( m_vSteerVelocity );

		float flDecayAmt = flDecayRate * dt;
		flLen = MAX( 0, flLen - flDecayAmt );
		m_vSteerVelocity *= flLen;


		// Setup our pose parameters.
		SetPoseParameter( m_iMovePoseParamX, RemapVal( m_vSteerVelocity.x, -MAX_WALKER_VEL, MAX_WALKER_VEL, -1, 1 ) );
		SetPoseParameter( m_iMovePoseParamY, RemapVal( m_vSteerVelocity.y, -MAX_WALKER_VEL, MAX_WALKER_VEL, -1, 1 ) );

		// Use an idle animation if they're not moving.
		int iWantedSequence = LookupSequence( "walk_box" );	
		if ( m_vSteerVelocity.x == 0 && m_vSteerVelocity.y == 0 )
		{
			iWantedSequence = LookupSequence( "idle" );

			// HACK: HL2 Strider has no idle
			if ( iWantedSequence == -1 )
				iWantedSequence = LookupSequence( "ragdoll" );
		}

		if ( iWantedSequence != -1 && GetSequence() != iWantedSequence )
			ResetSequence( iWantedSequence );
	}


	// Now ask the model how far it thought it moved based on the animation.
	// Turns out the animation thinks it's moving just a tiny bit, even when we're centered on the idle animation,
	// so we just force it not to move here if we know we're not supposed to move.
	if ( m_vSteerVelocity.Length() > 0 ) 
	{
		Vector vNewPos = GetWalkerLocalMovement();
				
		SetLocalOrigin( vNewPos );
	}


	// Hard-coded for now. These should come from the vehicle's script eventually.
	// Now slowly rotate towards the player's eye angles.
	CBasePlayer *pPlayer = GetPassenger( VEHICLE_ROLE_DRIVER );
	if ( pPlayer )
	{
		static float flAccelRate = 180;
		static float flRotateRate = 60;


		// Figure out a force to apply to our current velocity.
		Vector2D vAccel( 0, 0 );

		if ( m_LastButtons & IN_FORWARD )
			vAccel.x += flAccelRate;

		if ( m_LastButtons & IN_BACK )
			vAccel.x -= flAccelRate;

		if ( m_LastButtons & IN_MOVELEFT )
			vAccel.y -= flAccelRate;

		if ( m_LastButtons & IN_MOVERIGHT )
			vAccel.y += flAccelRate;

		m_vSteerVelocity += vAccel * dt;

		
		m_vSteerVelocity.x = clamp( m_vSteerVelocity.x, -MAX_WALKER_VEL, MAX_WALKER_VEL );
		m_vSteerVelocity.y = clamp( m_vSteerVelocity.y, -MAX_WALKER_VEL, MAX_WALKER_VEL );


		float wantedYaw = m_vLastCmdViewAngles[YAW];
		QAngle curAngles = GetAbsAngles();
		curAngles[YAW] = ApproachAngle( wantedYaw, curAngles[YAW], flRotateRate * dt );
		SetAbsAngles( curAngles );	
	}


	DispatchAnimEvents( this );

	
	// Get another think.
	SetContextThink( WalkerThink, gpGlobals->curtime + dt, "WalkerThink" );
}


Vector CWalkerBase::GetWalkerLocalMovement()
{
	bool bIgnored;
	Vector vNewPos;
	QAngle vNewAngles;
	GetIntervalMovement( GetAnimTimeInterval(), bIgnored, vNewPos, vNewAngles );
	return vNewPos;
}


const Vector2D& CWalkerBase::GetSteerVelocity() const
{
	return m_vSteerVelocity;
}



void CWalkerBase::Spawn()
{
	// Derived classes should call SpawnWalker instead of chaining down to CWalkerBase::Spawn().
	Assert( false );
}


void CWalkerBase::Activate()
{
	WalkerActivate();
	BaseClass::Activate();
}

void CWalkerBase::WalkerActivate( void )
{
	// Until we're finished building, turn off vphysics-based motion
	SetSolid( SOLID_VPHYSICS );
	VPhysicsInitStatic();

	SetPoseParameter( m_iMovePoseParamX, 0 );
	SetPoseParameter( m_iMovePoseParamY, 0 );
}


void CWalkerBase::SetVelocityDecayRate( float flDecayRate )
{
	m_flVelocityDecayRate = flDecayRate;
}


float CWalkerBase::GetTimeDelta() const
{
	return 0.1;
}


void CWalkerBase::SetupMove( CBasePlayer *pPlayer, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	// This calls StudioFrameAdvance for us.
	//BaseClass::SetupMove( pPlayer, ucmd, pHelper, move );

	// Lose control when the player dies
	if ( pPlayer->IsAlive() == false )
	{
		m_LastButtons = 0;
		return;
	}

	// Only the driver gets to drive.
	int nRole = GetPassengerRole( pPlayer );
	if ( nRole != VEHICLE_ROLE_DRIVER )
		return;

	m_LastButtons = ucmd->buttons;
	m_vLastCmdViewAngles = ucmd->viewangles;
}


bool CWalkerBase::IsPassengerVisible( int nRole )
{
	return true;
}


bool CWalkerBase::StartBuilding( CBaseEntity *pBuilder )
{
	if ( !BaseClass::StartBuilding( pBuilder ) )
		return false;

	WalkerActivate();
	return true;
}



