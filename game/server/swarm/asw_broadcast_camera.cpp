#include "cbase.h"
#include "asw_broadcast_camera.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SF_CAMERA_PLAYER_POSITION		1			// NOTE: Starting at player position not supported
#define SF_CAMERA_PLAYER_TARGET			2			// NOTE: Following player not supported
#define SF_CAMERA_PLAYER_TAKECONTROL	4
#define SF_CAMERA_PLAYER_INFINITE_WAIT	8
#define SF_CAMERA_PLAYER_SNAP_TO		16
#define SF_CAMERA_PLAYER_NOT_SOLID		32			// NOTE: not supported (all players in asw are non-solid anyway)
#define SF_CAMERA_PLAYER_INTERRUPT		64			// NOTE: not supported

LINK_ENTITY_TO_CLASS( asw_broadcast_camera, CASW_Broadcast_Camera );

BEGIN_DATADESC( CASW_Broadcast_Camera )

	//DEFINE_FIELD( m_hPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pPath, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_sPath, FIELD_STRING ),
	DEFINE_FIELD( m_flWait, FIELD_FLOAT ),
	DEFINE_FIELD( m_flReturnTime, FIELD_TIME ),
	DEFINE_FIELD( m_flStopTime, FIELD_TIME ),
	DEFINE_FIELD( m_moveDistance, FIELD_FLOAT ),
	DEFINE_FIELD( m_targetSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_initialSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_acceleration, FIELD_FLOAT ),
	DEFINE_FIELD( m_deceleration, FIELD_FLOAT ),
	DEFINE_FIELD( m_state, FIELD_INTEGER ),
	DEFINE_FIELD( m_vecMoveDir, FIELD_VECTOR ),
	DEFINE_KEYFIELD( m_iszTargetAttachment, FIELD_STRING, "targetattachment" ),
	DEFINE_FIELD( m_iAttachmentIndex, FIELD_INTEGER ),
	DEFINE_FIELD( m_bSnapToGoal, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecLastPos, FIELD_VECTOR),
	//DEFINE_FIELD( m_nPlayerButtons, FIELD_INTEGER ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

	// Function Pointers
	DEFINE_FUNCTION( FollowTarget ),
	DEFINE_OUTPUT( m_OnEndFollow, "OnEndFollow" ),

END_DATADESC()

void CASW_Broadcast_Camera::Spawn( void )
{
	BaseClass::Spawn();

	SetMoveType( MOVETYPE_NOCLIP );
	SetSolid( SOLID_NONE );								// Remove model & collisions
	SetRenderAlpha( 0 );								// The engine won't draw this model if this is set to 0 and blending is on
	m_nRenderMode = kRenderTransTexture;

	m_state = USE_OFF;
	
	m_initialSpeed = m_flSpeed;

	if ( m_acceleration == 0 )
		m_acceleration = 500;

	if ( m_deceleration == 0 )
		m_deceleration = 500;

	DispatchUpdateTransmitState();
}

int CASW_Broadcast_Camera::UpdateTransmitState()
{
	// always tranmit if currently used by a monitor
	if ( m_state == USE_ON )
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}
	else
	{
		return SetTransmitState( FL_EDICT_DONTSEND );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CASW_Broadcast_Camera::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "wait"))
	{
		m_flWait = atof(szValue);
	}
	else if (FStrEq(szKeyName, "moveto"))
	{
		m_sPath = AllocPooledString( szValue );
	}
	else if (FStrEq(szKeyName, "acceleration"))
	{
		m_acceleration = atof( szValue );
	}
	else if (FStrEq(szKeyName, "deceleration"))
	{
		m_deceleration = atof( szValue );
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}

//------------------------------------------------------------------------------
// Purpose: Input handler to turn on this trigger.
//------------------------------------------------------------------------------
void CASW_Broadcast_Camera::InputEnable( inputdata_t &inputdata )
{ 
	//m_hPlayer = inputdata.pActivator;
	Enable();
}


//------------------------------------------------------------------------------
// Purpose: Input handler to turn off this trigger.
//------------------------------------------------------------------------------
void CASW_Broadcast_Camera::InputDisable( inputdata_t &inputdata )
{ 
	Disable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Broadcast_Camera::Enable( void )
{
	m_state = USE_ON;

	//if ( !m_hPlayer || !m_hPlayer->IsPlayer() )
	//{
		//m_hPlayer = UTIL_GetLocalPlayer();
	//}

	//if ( !m_hPlayer )
	//{
		//DispatchUpdateTransmitState();
		//return;
	//}

	//if ( m_hPlayer->IsPlayer() )
	//{
		 //m_nPlayerButtons = ((CBasePlayer*)m_hPlayer.Get())->m_nButtons;
	//}
	
	//if ( HasSpawnFlags( SF_CAMERA_PLAYER_NOT_SOLID ) )
	//{
		//m_hPlayer->AddSolidFlags( FSOLID_NOT_SOLID );
	//}
	
	m_flReturnTime = gpGlobals->curtime + m_flWait;
	m_flSpeed = m_initialSpeed;
	m_targetSpeed = m_initialSpeed;

	if ( HasSpawnFlags( SF_CAMERA_PLAYER_SNAP_TO ) )
	{
		m_bSnapToGoal = true;
	}

	//if ( HasSpawnFlags(SF_CAMERA_PLAYER_TARGET ) )
	//{
		//m_hTarget = m_hPlayer;
	//}
	//else
	//{
		m_hTarget = GetNextTarget();
	//}

	// If we don't have a target, ignore the attachment / etc
	if ( m_hTarget )
	{
		m_iAttachmentIndex = 0;
		if ( m_iszTargetAttachment != NULL_STRING )
		{
			if ( !m_hTarget->GetBaseAnimating() )
			{
				Warning("%s tried to target an attachment (%s) on target %s, which has no model.\n", GetClassname(), STRING(m_iszTargetAttachment), STRING(m_hTarget->GetEntityName()) );
			}
			else
			{
				m_iAttachmentIndex = m_hTarget->GetBaseAnimating()->LookupAttachment( STRING(m_iszTargetAttachment) );
				if ( !m_iAttachmentIndex )
				{
					Warning("%s could not find attachment %s on target %s.\n", GetClassname(), STRING(m_iszTargetAttachment), STRING(m_hTarget->GetEntityName()) );
				}
			}
		}
	}

	//if (HasSpawnFlags(SF_CAMERA_PLAYER_TAKECONTROL ) )
	//{
		//((CBasePlayer*)m_hPlayer.Get())->EnableControl(FALSE);
	//}

	if ( m_sPath != NULL_STRING )
	{
		m_pPath = gEntList.FindEntityByName( NULL, m_sPath, NULL); //, m_hPlayer );
	}
	else
	{
		m_pPath = NULL;
	}

	m_flStopTime = gpGlobals->curtime;
	if ( m_pPath )
	{
		if ( m_pPath->m_flSpeed != 0 )
			m_targetSpeed = m_pPath->m_flSpeed;
		
		m_flStopTime += m_pPath->GetDelay();
		m_vecMoveDir = m_pPath->GetLocalOrigin() - GetLocalOrigin();
		m_moveDistance = VectorNormalize( m_vecMoveDir );
		m_flStopTime = gpGlobals->curtime + m_pPath->GetDelay();
	}
	else
	{
		m_moveDistance = 0;
	}
	/*
	if ( m_pPath )
	{
		if ( m_pPath->m_flSpeed != 0 )
			m_targetSpeed = m_pPath->m_flSpeed;
		
		Msg("target speed is %f\n", m_targetSpeed);
		m_flStopTime += m_pPath->GetDelay();
	}
	*/

	// copy over player information
	//if (HasSpawnFlags(SF_CAMERA_PLAYER_POSITION ) )
	//{
		//UTIL_SetOrigin( this, m_hPlayer->EyePosition() );
		//SetLocalAngles( QAngle( m_hPlayer->GetLocalAngles().x, m_hPlayer->GetLocalAngles().y, 0 ) );
		//SetAbsVelocity( m_hPlayer->GetAbsVelocity() );
	//}
	//else
	//{
		SetAbsVelocity( vec3_origin );
	//}

	UpdateAllPlayers();
	//((CBasePlayer*)m_hPlayer.Get())->SetViewEntity( this );

	// Hide the player's viewmodel
	//if ( ((CBasePlayer*)m_hPlayer.Get())->GetActiveWeapon() )
	//{
		//((CBasePlayer*)m_hPlayer.Get())->GetActiveWeapon()->AddEffects( EF_NODRAW );
	//}

	// Only track if we have a target
	if ( m_hTarget )
	{
		// follow the player down
		SetThink( &CASW_Broadcast_Camera::FollowTarget );
		SetNextThink( gpGlobals->curtime );
	}

	//m_moveDistance = 0;
	m_vecLastPos = GetAbsOrigin();
	Move();

	DispatchUpdateTransmitState();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Broadcast_Camera::Disable( void )
{
	RestoreAllPlayerViews();
	//if ( m_hPlayer && m_hPlayer->IsAlive() )
	//{
		//if ( HasSpawnFlags( SF_CAMERA_PLAYER_NOT_SOLID ) )
		//{
			//m_hPlayer->RemoveSolidFlags( FSOLID_NOT_SOLID );
		//}

		//((CBasePlayer*)m_hPlayer.Get())->SetViewEntity( m_hPlayer );
		//((CBasePlayer*)m_hPlayer.Get())->EnableControl(TRUE);

		// Restore the player's viewmodel
		//if ( ((CBasePlayer*)m_hPlayer.Get())->GetActiveWeapon() )
		//{
			//((CBasePlayer*)m_hPlayer.Get())->GetActiveWeapon()->RemoveEffects( EF_NODRAW );
		//}
	//}

	m_state = USE_OFF;
	m_flReturnTime = gpGlobals->curtime;
	SetThink( NULL );

	m_OnEndFollow.FireOutput(this, this); // dvsents2: what is the best name for this output?
	SetLocalAngularVelocity( vec3_angle );

	DispatchUpdateTransmitState();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Broadcast_Camera::FollowTarget( )
{
	//if (m_hPlayer == NULL)
		//return;

	if ( m_hTarget == NULL )
	{
		Disable();
		return;
	}

	if ( !HasSpawnFlags(SF_CAMERA_PLAYER_INFINITE_WAIT) && (!m_hTarget || m_flReturnTime < gpGlobals->curtime) )
	{
		Disable();
		return;
	}

	UpdateAllPlayers();

	QAngle vecGoal;
	if ( m_iAttachmentIndex )
	{
		Vector vecOrigin;
		m_hTarget->GetBaseAnimating()->GetAttachment( m_iAttachmentIndex, vecOrigin );
		VectorAngles( vecOrigin - GetAbsOrigin(), vecGoal );
	}
	else
	{
		if ( m_hTarget )
		{
			VectorAngles( m_hTarget->GetAbsOrigin() - GetAbsOrigin(), vecGoal );
		}
		else
		{
			// Use the viewcontroller's angles
			vecGoal = GetAbsAngles();
		}
	}

	// Should we just snap to the goal angles?
	if ( m_bSnapToGoal ) 
	{
		SetAbsAngles( vecGoal );
		m_bSnapToGoal = false;
	}
	else
	{
		// UNDONE: Can't we just use UTIL_AngleDiff here?
		QAngle angles = GetLocalAngles();

		if (angles.y > 360)
			angles.y -= 360;

		if (angles.y < 0)
			angles.y += 360;

		SetLocalAngles( angles );

		float dx = vecGoal.x - GetLocalAngles().x;
		float dy = vecGoal.y - GetLocalAngles().y;

		if (dx < -180) 
			dx += 360;
		if (dx > 180) 
			dx = dx - 360;
		
		if (dy < -180) 
			dy += 360;
		if (dy > 180) 
			dy = dy - 360;

		QAngle vecAngVel;
		vecAngVel.Init( dx * 40 * gpGlobals->frametime, dy * 40 * gpGlobals->frametime, GetLocalAngularVelocity().z );
		SetLocalAngularVelocity(vecAngVel);
	}

	if (!HasSpawnFlags(SF_CAMERA_PLAYER_TAKECONTROL))	
	{
		SetAbsVelocity( GetAbsVelocity() * 0.8 );
		if (GetAbsVelocity().Length( ) < 10.0)
		{
			SetAbsVelocity( vec3_origin );
		}
	}

	SetNextThink( gpGlobals->curtime );

	Move();
}

void CASW_Broadcast_Camera::Move()
{
	/*
	if ( HasSpawnFlags( SF_CAMERA_PLAYER_INTERRUPT ) )
	{
		if ( m_hPlayer )
		{
			CBasePlayer *pPlayer = ToBasePlayer( m_hPlayer );

			if ( pPlayer  )
			{
				int buttonsChanged = m_nPlayerButtons ^ pPlayer->m_nButtons;

				if ( buttonsChanged && pPlayer->m_nButtons )
				{
					 Disable();
					 return;
				}

				m_nPlayerButtons = pPlayer->m_nButtons;
			}
		}
	}
	*/
	
	// Not moving on a path, return
	if (!m_pPath)
		return;

	// Subtract movement from the previous frame
	//m_moveDistance -= m_flSpeed * gpGlobals->frametime;
	m_moveDistance -= VectorNormalize(GetAbsOrigin() - m_vecLastPos);

	// Have we moved enough to reach the target?
	if ( m_moveDistance <= 0 )
	{
		variant_t emptyVariant;
		m_pPath->AcceptInput( "InPass", this, this, emptyVariant, 0 );
		// Time to go to the next target
		m_pPath = m_pPath->GetNextTarget();

		// Set up next corner
		if ( !m_pPath )
		{
			SetAbsVelocity( vec3_origin );
		}
		else 
		{
			if ( m_pPath->m_flSpeed != 0 )
				m_targetSpeed = m_pPath->m_flSpeed;

			m_vecMoveDir = m_pPath->GetLocalOrigin() - GetLocalOrigin();
			m_moveDistance = VectorNormalize( m_vecMoveDir );
			m_flStopTime = gpGlobals->curtime + m_pPath->GetDelay();
		}
	}

	if ( m_flStopTime > gpGlobals->curtime )
		m_flSpeed = UTIL_Approach( 0, m_flSpeed, m_deceleration * gpGlobals->frametime );
	else
		m_flSpeed = UTIL_Approach( m_targetSpeed, m_flSpeed, m_acceleration * gpGlobals->frametime );

	float fraction = 2 * gpGlobals->frametime;

	SetAbsVelocity( ((m_vecMoveDir * m_flSpeed) * fraction) + (GetAbsVelocity() * (1-fraction)) );

	m_vecLastPos = GetAbsOrigin();
}

// makes sure all players are viewing this and frozen if needed
void CASW_Broadcast_Camera::UpdateAllPlayers()
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer )
		{
			if (HasSpawnFlags(SF_CAMERA_PLAYER_TAKECONTROL ) )
				pPlayer->EnableControl(false);
			pPlayer->SetViewEntity( this );
		}
	}
}

// puts player views back to normal
void CASW_Broadcast_Camera::RestoreAllPlayerViews()
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer )
		{
			pPlayer->SetViewEntity( pPlayer );
			pPlayer->EnableControl(true);
		}
	}
}