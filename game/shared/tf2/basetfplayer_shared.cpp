//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF2's player object, code shared between client & server.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_combatshield.h"
#include "weapon_objectselection.h"
#include "weapon_twohandedcontainer.h"
#ifdef CLIENT_DLL
#include "c_weapon_builder.h"
#else
#include "weapon_builder.h"
#include "basegrenade_shared.h"
#include "grenade_objectsapper.h"
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsClass( TFClass iClass )
{
	if ( !GetPlayerClass()  )
	{
		// Special case for undecided players
		if ( iClass == TFCLASS_UNDECIDED )
			return true;
		return false;
	}

	return ( PlayerClass() == iClass );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponCombatShield *CBaseTFPlayer::GetCombatShield( void )
{
	if ( !m_hWeaponCombatShield )
	{
		if ( GetTeamNumber() == TEAM_ALIENS )
		{
			m_hWeaponCombatShield = static_cast< CWeaponCombatShield * >( Weapon_OwnsThisType( "weapon_combat_shield_alien" ) );
#ifndef CLIENT_DLL
			if ( !m_hWeaponCombatShield )
			{
				m_hWeaponCombatShield = static_cast< CWeaponCombatShield * >( GiveNamedItem( "weapon_combat_shield_alien" ) );
			}
#endif
		}
		else
		{
			m_hWeaponCombatShield = static_cast< CWeaponCombatShield * >( Weapon_OwnsThisType( "weapon_combat_shield" ) );
#ifndef CLIENT_DLL
			if ( !m_hWeaponCombatShield )
			{
				m_hWeaponCombatShield = static_cast< CWeaponCombatShield * >( GiveNamedItem( "weapon_combat_shield" ) );
			}
#endif
		}
	}

	return m_hWeaponCombatShield;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if the shot is blocked by the player's handheld shield
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsHittingShield( const Vector &vecVelocity, float *flDamage )
{
	if (!IsParrying() && !IsBlocking())
		return false;

	Vector2D vecDelta = vecVelocity.AsVector2D();
	Vector2DNormalize( vecDelta );

	Vector forward;
	AngleVectors( GetLocalAngles(), &forward );

	Vector2DNormalize( forward.AsVector2D() );

	float flDot = DotProduct2D( vecDelta, forward.AsVector2D() );

	// This gives us a little more than a 90 degree protection angle
	if (flDot < -0.67f)
	{
		// We've hit the players handheld shield, see if the shield can do anything about it
		if ( flDamage && GetCombatShield() )
		{
			// Return true if the shield blocked it all
			*flDamage = GetCombatShield()->AttemptToBlock( *flDamage );
			return ( !(*flDamage) );
		}
		
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Play a sound to show we've been hurt
//-----------------------------------------------------------------------------
void CBaseTFPlayer::PainSound( void )
{
	char *sSoundName = NULL;

	if ( GetTeamNumber() == TEAM_HUMANS )
	{
		sSoundName = "Humans.Pain";
	}
	else if ( GetTeamNumber() == TEAM_ALIENS )
	{
		switch( PlayerClass() )
		{
		case TFCLASS_COMMANDO:
			sSoundName = "AlienCommando.Pain";
			break;

		case TFCLASS_MEDIC:
			sSoundName = "AlienMedic.Pain";
			break;

		case TFCLASS_DEFENDER:
			sSoundName = "AlienDefender.Pain";
			break;

		case TFCLASS_ESCORT:
			sSoundName = "AlienEscort.Pain";
			break;

		default:
			break;
		}
	}

	if ( !sSoundName )
		return;

	CPASAttenuationFilter filter( this, sSoundName );
	EmitSound( filter, entindex(), sSoundName );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we should record our last weapon when switching between the two specified weapons
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::Weapon_ShouldSetLast( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon )
{
	// Don't record last weapons when switching to an object
	if ( dynamic_cast< CWeaponObjectSelection* >( pNewWeapon ) )
	{
		// Store this weapon off so we can switch back to it
		// Don't store it if it's also an object
		CBaseCombatWeapon *pLast = pOldWeapon->GetLastWeapon();
#ifdef CLIENT_DLL
		if ( !dynamic_cast< C_WeaponBuilder* >( pLast ) )
#else
		if ( !dynamic_cast< CWeaponBuilder* >( pLast ) )
#endif
		{
			m_hLastWeaponBeforeObject = pLast;
		}
		return false;
	}

	// Don't record last weapons when switching from the builder
	// If the old weapon is a twohanded container, check the left weapon
	CWeaponTwoHandedContainer *pContainer = dynamic_cast< CWeaponTwoHandedContainer * >( pOldWeapon );
	if ( pContainer )
	{
		pOldWeapon = dynamic_cast< CBaseTFCombatWeapon  * >( pContainer->GetLeftWeapon() );
	}
#ifdef CLIENT_DLL
	if ( dynamic_cast< C_WeaponBuilder* >( pOldWeapon ) )
		return false;
#else
	if ( dynamic_cast< CWeaponBuilder* >( pOldWeapon ) )
		return false;
#endif

	return BaseClass::Weapon_ShouldSetLast( pOldWeapon, pNewWeapon );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we should allow selection of the specified item
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::Weapon_ShouldSelectItem( CBaseCombatWeapon *pWeapon )
{
	CBaseCombatWeapon *pActiveWeapon = GetActiveWeapon();
	// If the old weapon is a twohanded container, check the left weapon
	CWeaponTwoHandedContainer *pContainer = dynamic_cast< CWeaponTwoHandedContainer * >( pActiveWeapon );
	if ( pContainer )
	{
		pActiveWeapon = pContainer->GetLeftWeapon();
	}

	return ( pWeapon != pActiveWeapon );
}

#ifndef CLIENT_DLL
// Sapper handling is all here because it'll soon be shared Client / Server

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsAttachingSapper( void )
{
	return ( m_TFLocal.m_bAttachingSapper );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CBaseTFPlayer::GetSapperAttachmentTime( void )
{
	return (gpGlobals->curtime - m_flSapperAttachmentStartTime);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::StartAttachingSapper( CBaseObject *pObject, CGrenadeObjectSapper *pSapper )
{
	Assert( pSapper );

	m_TFLocal.m_bAttachingSapper = true;
	m_TFLocal.m_flSapperAttachmentFrac = 0.0f;

	m_hSappedObject = pObject;
	m_flSapperAttachmentStartTime = gpGlobals->curtime;
	m_flSapperAttachmentFinishTime = gpGlobals->curtime + m_hSappedObject->GetSapperAttachTime();
	m_hSapper = pSapper;
	m_hSapper->SetArmed( false );

	CPASAttenuationFilter filter( m_hSapper, "WeaponObjectSapper.Attach" );
	EmitSound( filter, m_hSapper->entindex(), "WeaponObjectSapper.Attach" );

	// Drop the player's weapon
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->Holster();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::CheckSapperAttaching( void )
{
	// Did we stop attaching?
	if ( !m_TFLocal.m_bAttachingSapper )
	{
		if ( m_TFLocal.m_flSapperAttachmentFrac )
		{
			StopAttaching();
		}
		return;
	}

	// Object gone?
	if ( m_hSappedObject == NULL )
	{
		StopAttaching();
		return;
	}

	// Sapper gone?
	if ( m_hSapper == NULL )
	{
		StopAttaching();
		return;
	}

	// Make sure I'm still looking at the target
	trace_t tr;
	Vector vecAiming;
	Vector vecSrc = EyePosition();
	EyeVectors( &vecAiming );
	UTIL_TraceLine( vecSrc, vecSrc + (vecAiming * 128), MASK_SOLID, this, TFCOLLISION_GROUP_WEAPON, &tr );
	if ( tr.fraction == 1.0 || tr.m_pEnt != m_hSappedObject )
	{
		StopAttaching();
		return;
	}

	// Finished?
	if ( m_flSapperAttachmentFinishTime >= gpGlobals->curtime )
	{
		float dt = m_flSapperAttachmentFinishTime - m_flSapperAttachmentStartTime;
		if ( dt > 0.0f )
		{
			m_TFLocal.m_flSapperAttachmentFrac = ( gpGlobals->curtime - m_flSapperAttachmentStartTime ) / dt;
			m_TFLocal.m_flSapperAttachmentFrac = clamp( m_TFLocal.m_flSapperAttachmentFrac, 0.0f, 1.0f );
		}
		else
		{
			m_TFLocal.m_flSapperAttachmentFrac = 0.0f;
		}
		return;
	}

	FinishAttaching();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::CleanupAfterAttaching( void )
{
	Assert( m_TFLocal.m_bAttachingSapper );
	m_TFLocal.m_bAttachingSapper = false;

	m_flSapperAttachmentFinishTime = -1;
	m_flSapperAttachmentStartTime = -1;
	m_TFLocal.m_flSapperAttachmentFrac = 0.0f;

	// Restore the player's weapon
	m_flNextAttack = gpGlobals->curtime;
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->Deploy();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::StopAttaching( void )
{
	CleanupAfterAttaching();	

	if ( m_hSapper != NULL )
	{
		CPASAttenuationFilter filter( m_hSapper, "WeaponObjectSapper.AttachFail" );
		EmitSound( filter, m_hSapper->entindex(), "WeaponObjectSapper.AttachFail" );

		m_hSapper->SetTargetObject( NULL );
		m_hSapper->Remove( );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::FinishAttaching( void )
{
	CleanupAfterAttaching();

	if ( m_hSapper != NULL )
	{
		m_hSapper->SetTargetObject( m_hSappedObject );
		m_hSapper->SetArmed( true );
	}
}
#endif

// Below this many degrees, slow down turning rate linearly
#define FADE_TURN_DEGREES	45.0f
// After this, need to start turning feet
#define MAX_TORSO_ANGLE		90.0f
// Below this amount, don't play a turning animation/perform IK
#define MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION		15.0f

static ConVar tf2_feetyawrunscale( "tf2_feetyawrunscale", "2", FCVAR_REPLICATED, "Multiplier on tf2_feetyawrate to allow turning faster when running." );
extern ConVar sv_backspeed;
extern ConVar mp_feetyawrate;
extern ConVar mp_facefronttime;
extern ConVar mp_ik;

CPlayerAnimState::CPlayerAnimState( CBaseTFPlayer *outer )
	: m_pOuter( outer )
{
	m_flGaitYaw = 0.0f;
	m_flGoalFeetYaw = 0.0f;
	m_flCurrentFeetYaw = 0.0f;
	m_flCurrentTorsoYaw = 0.0f;
	m_flLastYaw = 0.0f;
	m_flLastTurnTime = 0.0f;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::Update()
{
	m_angRender = GetOuter()->GetLocalAngles();

	ComputePoseParam_BodyYaw();
	ComputePoseParam_BodyPitch( GetOuter()->GetModelPtr() );
	ComputePoseParam_BodyLookYaw();

	ComputePlaybackRate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePlaybackRate()
{
	// Determine ideal playback rate
	Vector vel;
	GetOuterAbsVelocity( vel );

	float speed = vel.Length2D();

	bool isMoving = ( speed > 0.5f ) ? true : false;

	Activity currentActivity = 	GetOuter()->GetSequenceActivity( GetOuter()->GetSequence() );

	switch ( currentActivity )
	{
	case ACT_WALK:
	case ACT_RUN:
	case ACT_IDLE:
		{
			float maxspeed = GetOuter()->MaxSpeed();
			if ( isMoving && ( maxspeed > 0.0f ) )
			{
				float flFactor = 1.0f;

				// HACK HACK:: Defender backward animation is animated at 0.6 times speed, so scale up animation for this class
				//  if he's running backward.

				// Not sure if we're really going to do all classes this way.
				if ( GetOuter()->IsClass( TFCLASS_DEFENDER ) ||
					 GetOuter()->IsClass( TFCLASS_MEDIC ) )
				{
					Vector facing;
					Vector moving;

					moving = vel;
					AngleVectors( GetOuter()->GetLocalAngles(), &facing );
					VectorNormalize( moving );

					float dot = moving.Dot( facing );
					if ( dot < 0.0f )
					{
						float backspeed = sv_backspeed.GetFloat();
						flFactor = 1.0f - fabs( dot ) * (1.0f - backspeed);

						if ( flFactor > 0.0f )
						{
							flFactor = 1.0f / flFactor;
						}
					}
				}

				// Note this gets set back to 1.0 if sequence changes due to ResetSequenceInfo below
				GetOuter()->SetPlaybackRate( ( speed * flFactor ) / maxspeed );

				// BUG BUG:
				// This stuff really should be m_flPlaybackRate = speed / m_flGroundSpeed
			}
			else
			{
				GetOuter()->SetPlaybackRate( 1.0f );
			}
		}
		break;
	default:
		{
			GetOuter()->SetPlaybackRate( 1.0f );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBasePlayer
//-----------------------------------------------------------------------------
CBaseTFPlayer *CPlayerAnimState::GetOuter()
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void CPlayerAnimState::EstimateYaw( void )
{
	float dt = gpGlobals->frametime;

	if ( !dt )
	{
		return;
	}

	Vector est_velocity;
	QAngle	angles;

	GetOuterAbsVelocity( est_velocity );

	angles = GetOuter()->GetLocalAngles();

	if ( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		float flYawDiff = angles[YAW] - m_flGaitYaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if (flYawDiff > 180)
			flYawDiff -= 360;
		if (flYawDiff < -180)
			flYawDiff += 360;

		if (dt < 0.25)
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		m_flGaitYaw += flYawDiff;
		m_flGaitYaw = m_flGaitYaw - (int)(m_flGaitYaw / 360) * 360;
	}
	else
	{
		m_flGaitYaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);

		if (m_flGaitYaw > 180)
			m_flGaitYaw = 180;
		else if (m_flGaitYaw < -180)
			m_flGaitYaw = -180;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override for backpeddling
// Input  : dt - 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePoseParam_BodyYaw( void )
{
	int iYaw = GetOuter()->LookupPoseParameter( "move_yaw" );
	if ( iYaw < 0 )
		return;

	// view direction relative to movement
	float flYaw;	 

	EstimateYaw();

	QAngle	angles = GetOuter()->GetLocalAngles();
	float ang = angles[ YAW ];
	if ( ang > 180.0f )
	{
		ang -= 360.0f;
	}
	else if ( ang < -180.0f )
	{
		ang += 360.0f;
	}

	// calc side to side turning
	flYaw = ang - m_flGaitYaw;
	// Invert for mapping into 8way blend
	flYaw = -flYaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;

	if (flYaw < -180)
	{
		flYaw = flYaw + 360;
	}
	else if (flYaw > 180)
	{
		flYaw = flYaw - 360;
	}
	
	GetOuter()->SetPoseParameter( iYaw, flYaw );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr )
{
	// Get pitch from v_angle
	float flPitch = GetOuter()->GetLocalAngles()[ PITCH ];
	if ( flPitch > 180.0f )
	{
		flPitch -= 360.0f;
	}
	flPitch = clamp( flPitch, -90, 90 );

	QAngle absangles = GetOuter()->GetAbsAngles();
	absangles.x = 0.0f;
	m_angRender = absangles;

	// See if we have a blender for pitch
	int pitch = GetOuter()->LookupPoseParameter( pStudioHdr, "body_pitch" );
	if ( pitch < 0 )
		return;

	GetOuter()->SetPoseParameter( pStudioHdr, pitch, flPitch );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : goal - 
//			maxrate - 
//			dt - 
//			current - 
// Output : int
//-----------------------------------------------------------------------------
int CPlayerAnimState::ConvergeAngles( float goal,float maxrate, float dt, float& current )
{
	int direction = TURN_NONE;

	float anglediff = goal - current;
	float anglediffabs = fabs( anglediff );

	anglediff = AngleNormalize( anglediff );

	float scale = 1.0f;
	if ( anglediffabs <= FADE_TURN_DEGREES )
	{
		scale = anglediffabs / FADE_TURN_DEGREES;
		// Always do at least a bit of the turn ( 1% )
		scale = clamp( scale, 0.01f, 1.0f );
	}

	float maxmove = maxrate * dt * scale;

	if ( fabs( anglediff ) < maxmove )
	{
		current = goal;
	}
	else
	{
		if ( anglediff > 0 )
		{
			current += maxmove;
			direction = TURN_LEFT;
		}
		else
		{
			current -= maxmove;
			direction = TURN_RIGHT;
		}
	}

	current = AngleNormalize( current );

	return direction;
}

void CPlayerAnimState::ComputePoseParam_BodyLookYaw( void )
{
	QAngle absangles = GetOuter()->GetAbsAngles();
	absangles.y = AngleNormalize( absangles.y );
	m_angRender = absangles;

	// See if we even have a blender for pitch
	int upper_body_yaw = GetOuter()->LookupPoseParameter( "body_yaw" );
	if ( upper_body_yaw < 0 )
	{
		return;
	}

	// Assume upper and lower bodies are aligned and that we're not turning
	float flGoalTorsoYaw = 0.0f;
	int turning = TURN_NONE;
	float turnrate = mp_feetyawrate.GetFloat();

	Vector vel;
	
	GetOuterAbsVelocity( vel );

	bool isMoving = ( vel.Length() > 0.0f ) ? true : false;

	if ( !isMoving )
	{
		// Just stopped moving, try and clamp feet
		if ( m_flLastTurnTime <= 0.0f )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
			m_flLastYaw			= GetOuter()->GetAbsAngles().y;
			// Snap feet to be perfectly aligned with torso/eyes
			m_flGoalFeetYaw		= GetOuter()->GetAbsAngles().y;
			m_flCurrentFeetYaw	= m_flGoalFeetYaw;
			m_nTurningInPlace	= TURN_NONE;
		}

		// If rotating in place, update stasis timer
		if ( m_flLastYaw != GetOuter()->GetAbsAngles().y )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
			m_flLastYaw			= GetOuter()->GetAbsAngles().y;
		}

		if ( m_flGoalFeetYaw != m_flCurrentFeetYaw )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
		}

		turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, gpGlobals->frametime, m_flCurrentFeetYaw );

		// See how far off current feetyaw is from true yaw
		float yawdelta = GetOuter()->GetAbsAngles().y - m_flCurrentFeetYaw;
		yawdelta = AngleNormalize( yawdelta );

		bool rotated_too_far = false;

		float yawmagnitude = fabs( yawdelta );
		// If too far, then need to turn in place
		if ( yawmagnitude > MAX_TORSO_ANGLE )
		{
			rotated_too_far = true;
		}

		// Standing still for a while, rotate feet around to face forward
		// Or rotated too far
		// FIXME:  Play an in place turning animation
		if ( rotated_too_far || 
			( gpGlobals->curtime > m_flLastTurnTime + mp_facefronttime.GetFloat() ) )
		{
			m_flGoalFeetYaw		= GetOuter()->GetAbsAngles().y;
			m_flLastTurnTime	= gpGlobals->curtime;

			float yd = m_flCurrentFeetYaw - m_flGoalFeetYaw;
			if ( yd > 0 )
			{
				m_nTurningInPlace = TURN_RIGHT;
			}
			else if ( yd < 0 )
			{
				m_nTurningInPlace = TURN_LEFT;
			}
			else
			{
				m_nTurningInPlace = TURN_NONE;
			}

			turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, gpGlobals->frametime, m_flCurrentFeetYaw );
			yawdelta = GetOuter()->GetAbsAngles().y - m_flCurrentFeetYaw;
		}

		// Snap upper body into position since the delta is already smoothed for the feet
		flGoalTorsoYaw = yawdelta;
		m_flCurrentTorsoYaw = flGoalTorsoYaw;
	}
	else
	{
		m_flLastTurnTime = 0.0f;
		m_nTurningInPlace = TURN_NONE;
		m_flGoalFeetYaw = GetOuter()->GetAbsAngles().y;
		flGoalTorsoYaw = 0.0f;
		turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, gpGlobals->frametime, m_flCurrentFeetYaw );
		m_flCurrentTorsoYaw = GetOuter()->GetAbsAngles().y - m_flCurrentFeetYaw;
	}


	if ( turning == TURN_NONE )
	{
		m_nTurningInPlace = turning;
	}

	if ( m_nTurningInPlace != TURN_NONE )
	{
		// If we're close to finishing the turn, then turn off the turning animation
		if ( fabs( m_flCurrentFeetYaw - m_flGoalFeetYaw ) < MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION )
		{
			m_nTurningInPlace = TURN_NONE;
		}
	}

	// Counter rotate upper body as needed
	ConvergeAngles( flGoalTorsoYaw, turnrate, gpGlobals->frametime, m_flCurrentTorsoYaw );

	// Rotate entire body into position
	absangles = GetOuter()->GetAbsAngles();
	absangles.y = m_flCurrentFeetYaw;
	m_angRender = absangles;

	GetOuter()->SetPoseParameter( upper_body_yaw, clamp( m_flCurrentTorsoYaw, -90.0f, 90.0f ) );
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CPlayerAnimState::BodyYawTranslateActivity( Activity activity )
{
	// Not even standing still, sigh
	if ( activity != ACT_IDLE )
		return activity;

	// Not turning
	switch ( m_nTurningInPlace )
	{
	default:
	case TURN_NONE:
		return activity;
	/*
	case TURN_RIGHT:
		return ACT_TURNRIGHT45;
	case TURN_LEFT:
		return ACT_TURNLEFT45;
	*/
	case TURN_RIGHT:
	case TURN_LEFT:
		return mp_ik.GetBool() ? ACT_TURN : activity;
	}

	Assert( 0 );
	return activity;
}

const QAngle& CPlayerAnimState::GetRenderAngles()
{
	return m_angRender;
}


void CPlayerAnimState::GetOuterAbsVelocity( Vector& vel )
{
#if defined( CLIENT_DLL )
	GetOuter()->EstimateAbsVelocity( vel );
#else
	vel = GetOuter()->GetAbsVelocity();
#endif
}
