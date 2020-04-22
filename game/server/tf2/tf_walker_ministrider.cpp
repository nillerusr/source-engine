//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "tf_walker_ministrider.h"
#include "in_buttons.h"
#include "studio.h"
#include "shake.h"
#include "tf_gamerules.h"
#include "npcevent.h"
#include "ndebugoverlay.h"
#include "te_effect_dispatch.h"
#include "beam_shared.h"
#include "ai_activity.h"

#define WALKER_MINI_STRIDER_MODEL			"models/objects/alien_vehicle_strider.mdl"
#define STRIDER_TORSO_VERTICAL_SLIDE_SPEED	100

float LARGE_GUN_FIRE_TIME = 3;

// Skirmisher CVars
ConVar tf_skirmisher_speed( "tf_skirmisher_speed", "300" );
ConVar tf_skirmisher_machinegun_range( "tf_skirmisher_machinegun_range", "2000" );
ConVar tf_skirmisher_machinegun_damage( "tf_skirmisher_machinegun_damage", "20" );
ConVar tf_skirmisher_machinegun_rof( "tf_skirmisher_machinegun_rof", "10" );

IMPLEMENT_SERVERCLASS_ST( CWalkerMiniStrider, DT_WalkerMiniStrider )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( walker_mini_strider, CWalkerMiniStrider );
PRECACHE_REGISTER( walker_mini_strider );



CWalkerMiniStrider::CWalkerMiniStrider()
{
	m_flWantedZ = -1;
	m_bFiringMachineGun = m_bFiringLargeGun = false;
	m_flOriginToLowestLegHeight = 133;
	m_State = STATE_NORMAL;
	m_bFiringLeftGun = true;
	m_flNextFootstepSoundTime = 0;
}


CWalkerMiniStrider::~CWalkerMiniStrider()
{
	UTIL_Remove( m_pEnergyBeam );
}


void CWalkerMiniStrider::Precache()
{
	PrecacheModel( WALKER_MINI_STRIDER_MODEL );

	PrecacheScriptSound( "Skirmisher.Footstep" );
	PrecacheScriptSound( "Skirmisher.GunSound" );
	PrecacheScriptSound( "Skirmisher.GunChargeSound" );

	BaseClass::Precache();
}


void CWalkerMiniStrider::Spawn()
{
	SpawnWalker( 
		WALKER_MINI_STRIDER_MODEL,	// Model name.
		OBJ_WALKER_MINI_STRIDER,	// Object type.
		Vector( 0, 0, 140 ) + Vector( -100, -100, -100 ), // Placement dimensions.
		Vector( 0, 0, 140 ) + Vector( 100, 100, 100 ),
		200,						// Health.
		1,							// Max passengers.
		1.0							// 1x speed multiplier
		);

	SetVelocityDecayRate( 110 );
}


bool CWalkerMiniStrider::IsPassengerVisible( int nRole )
{
	if ( nRole == VEHICLE_ROLE_DRIVER )
		return false;

	return true;
}


void CWalkerMiniStrider::StartFiringMachineGun()
{
	StopFiringLargeGun();
	m_bFiringMachineGun = true;
	m_flNextShootTime = gpGlobals->curtime;
}


void CWalkerMiniStrider::StopFiringMachineGun()
{
	m_bFiringMachineGun = false;
}


Vector CWalkerMiniStrider::GetLargeGunShootOrigin()
{
	Vector vRet;
	QAngle dummyAngle;
	if ( GetAttachment( LookupAttachment( "LargeGun" ), vRet, dummyAngle ) )
	{
		return vRet;
	}
	else
	{
		return GetAbsOrigin();
	}
}


void CWalkerMiniStrider::StartFiringLargeGun()
{
	CBasePlayer *pPlayer = GetPassenger( VEHICLE_ROLE_DRIVER );
	Assert( pPlayer );
	if ( !pPlayer )
		return;

	// Figure out what we're shooting at.
	Vector vSrc = GetLargeGunShootOrigin();
	
	Vector vEyePos = pPlayer->EyePosition();
	Vector vEyeForward;
	AngleVectors( pPlayer->LocalEyeAngles(), &vEyeForward );

	trace_t trace;
	UTIL_TraceLine( vEyePos, vEyePos + vEyeForward * 2000, MASK_SOLID, this, COLLISION_GROUP_NONE, &trace );
	if ( trace.fraction < 1 )
	{
		m_vLargeGunForward = trace.endpos - vSrc;
		VectorNormalize( m_vLargeGunForward );

		trace_t trace;
		UTIL_TraceLine( vSrc, vSrc + m_vLargeGunForward * 2000, MASK_SOLID, this, COLLISION_GROUP_NONE, &trace );
		if ( trace.fraction < 1 )
		{
			EnableWalkMode( false );

			m_vLargeGunTargetPos = trace.endpos;
			m_flLargeGunCountdown = LARGE_GUN_FIRE_TIME;
			m_bFiringLargeGun = true;
			
			// Show an energy beam until we actually shoot.
			m_pEnergyBeam = CBeam::BeamCreate( "sprites/physbeam.vmt", 25 );
			m_pEnergyBeam->SetColor( 255, 0, 0 ); 
			m_pEnergyBeam->SetBrightness( 100 );
			m_pEnergyBeam->SetNoise( 4 );
			m_pEnergyBeam->PointsInit( vSrc, m_vLargeGunTargetPos );
			m_pEnergyBeam->LiveForTime( LARGE_GUN_FIRE_TIME );

			// Play a charge-up sound.
			CPASAttenuationFilter filter( this, "Skirmisher.GunChargeSound" );
			EmitSound( filter, 0, "Skirmisher.GunChargeSound", &vSrc );
		}
	}
}


void CWalkerMiniStrider::StopFiringLargeGun()
{
	if ( m_bFiringLargeGun )
	{
		m_bFiringLargeGun = false;

		UTIL_Remove( m_pEnergyBeam );
		m_pEnergyBeam = NULL;

		EnableWalkMode( true );
	}
}


void CWalkerMiniStrider::UpdateLargeGun()
{
	float dt = GetTimeDelta();

	if ( !m_bFiringLargeGun )
		return;

	m_flLargeGunCountdown -= dt;
	if ( m_flLargeGunCountdown <= 0 )
	{
		// Fire!
		Vector vSrc = GetLargeGunShootOrigin();
		trace_t trace;
		UTIL_TraceLine( vSrc, vSrc + m_vLargeGunForward * 2000, MASK_SOLID, this, COLLISION_GROUP_NONE, &trace );
		if ( trace.fraction < 1 )
		{
			CBasePlayer *pDriver = GetPassenger( VEHICLE_ROLE_DRIVER );
			if ( pDriver )
			{
				UTIL_ImpactTrace( &trace, DMG_ENERGYBEAM, "Strider" );

				Vector vHitPos = trace.endpos;
				float flDamageRadius = 100;
				float flDamage = 100;

				CPASFilter filter( vHitPos );
				te->Explosion( filter, 0.0,
					&vHitPos, 
					g_sModelIndexFireball,
					2.0, 
					15,
					TE_EXPLFLAG_NONE,
					flDamageRadius,
					flDamage );

				UTIL_ScreenShake( vHitPos, 10.0, 150.0, 1.0, 100, SHAKE_START );
				RadiusDamage( CTakeDamageInfo( this, pDriver, flDamage, DMG_BLAST ), vHitPos, flDamageRadius, CLASS_NONE, NULL );
			}
		}

		StopFiringLargeGun();
	}
}


void CWalkerMiniStrider::Crouch()
{
	if ( m_State == STATE_CROUCHING || m_State == STATE_CROUCHED )
		return;

	// Disable the base class's walking functionality while we're crouched.
	EnableWalkMode( false );

	SetActivity( ACT_CROUCH );
	m_flCrouchTimer = SequenceDuration();
	m_State = STATE_CROUCHING;
}


void CWalkerMiniStrider::UnCrouch()
{
	if ( m_State == STATE_CROUCHING || m_State == STATE_UNCROUCHING || m_State == STATE_NORMAL )
		return;

	SetActivity( ACT_STAND );

	m_flCrouchTimer = SequenceDuration();

	m_State = STATE_UNCROUCHING;
}


void CWalkerMiniStrider::UpdateCrouch()
{
	float dt = GetTimeDelta();

	m_flCrouchTimer -= dt;
	if ( m_flCrouchTimer <= 0 )
	{
		if ( m_State == STATE_CROUCHING )
		{
			m_State = STATE_CROUCHED;
			SetActivity( ACT_CROUCHIDLE );
		}
		else if ( m_State == STATE_UNCROUCHING )
		{
			EnableWalkMode( true );
			m_State = STATE_NORMAL;
			SetActivity( ACT_IDLE );
		}
	}
}


void CWalkerMiniStrider::FireMachineGun()
{
	CBaseEntity *pDriver = GetPassenger( VEHICLE_ROLE_DRIVER );
	if ( pDriver )
	{
		// Alternate the gun we're firing
		char *attachmentNames[2] = { "MachineGun_Left", "MachineGun_Right" };
		int iAttachment = LookupAttachment( attachmentNames[!m_bFiringLeftGun] );
		m_bFiringLeftGun = !m_bFiringLeftGun;

		Vector vAttachmentPos;
		QAngle vAttachmentAngles;
		GetAttachment( iAttachment, vAttachmentPos, vAttachmentAngles );

		Vector vEyePos = pDriver->EyePosition();
		Vector vEyeForward;
		QAngle vecAngles = pDriver->LocalEyeAngles();
		// Use the skirmisher's yaw
		vecAngles[YAW] = GetAbsAngles()[YAW];
		AngleVectors( vecAngles, &vEyeForward );

		// Trace ahead to find out where the crosshair is aiming 
		trace_t trace;
		float flMaxRange = tf_skirmisher_machinegun_range.GetFloat();
		UTIL_TraceLine( 
			vEyePos, 
			vEyePos + vEyeForward * flMaxRange,
			MASK_SOLID, 
			this, 
			COLLISION_GROUP_NONE, 
			&trace );

		Vector vecDir;
		if ( trace.fraction < 1.0 )
		{
			vecDir = (trace.endpos - vAttachmentPos );
			VectorNormalize( vecDir );
		}
		else
		{
			vecDir = vEyeForward;
		}

		// Shoot!
		TFGameRules()->FireBullets( CTakeDamageInfo( this, pDriver, tf_skirmisher_machinegun_damage.GetFloat(), DMG_BULLET ), 
			1,						// Num shots
			vAttachmentPos, 
			vecDir, 
			VECTOR_CONE_3DEGREES,	// Spread
			flMaxRange,				// Range
			DMG_BULLET,
			1,						// Tracer freq
			entindex(), 
			iAttachment,			// Attachment ID
			"MinigunTracer" );

		m_flNextShootTime += (1.0f / tf_skirmisher_machinegun_rof.GetFloat());

		// Play the fire sound.
		Vector vCenter = WorldSpaceCenter();
		CPASAttenuationFilter filter( this, "Skirmisher.GunSound" );
		EmitSound( filter, 0, "Skirmisher.GunSound", &vCenter );
	}
}


void CWalkerMiniStrider::WalkerThink()
{
	float dt = GetTimeDelta();

	BaseClass::WalkerThink();

	// Shoot the machine gun?
	if ( !m_bFiringLargeGun )
	{
		if ( m_LastButtons & IN_ATTACK )
		{
			if ( !m_bFiringMachineGun )
				StartFiringMachineGun();
		}
		else if ( m_bFiringMachineGun )
		{
			StopFiringMachineGun();
		}
	}

	// Fire the large gun?
	if ( !m_bFiringMachineGun )
	{
		if ( m_LastButtons & IN_ATTACK2 )
		{
			if ( !m_bFiringLargeGun )
				StartFiringLargeGun();
		}
	}


	UpdateCrouch();

	// Make sure it's crouched when there is no driver.
	if ( GetPassenger( VEHICLE_ROLE_DRIVER ) )
	{
		if ( m_LastButtons & IN_DUCK )
		{
			Crouch();
		}
		else
		{
			UnCrouch();
		}
	}
	else
	{
		Crouch();
	}

	if ( m_bFiringMachineGun )
	{
		while ( gpGlobals->curtime > m_flNextShootTime )
		{
			FireMachineGun();
		}
	}
	
	UpdateLargeGun();

	// Move our torso within range of our feet.
	if ( m_flOriginToLowestLegHeight != -1 )
	{
		Vector vCenter = WorldSpaceCenter();

		//NDebugOverlay::EntityBounds( this, 255, 100, 0, 0 ,0 );
		//NDebugOverlay::Line( vCenter, vCenter-Vector(0,0,2000), 255,0,0, true, 0 );
		
		trace_t trace;
		UTIL_TraceLine( 
			vCenter, 
			vCenter - Vector( 0, 0, 2000 ),
			MASK_SOLID_BRUSHONLY, 
			this, 
			COLLISION_GROUP_NONE, 
			&trace );

		if ( trace.fraction < 1 )
		{
			m_flWantedZ = trace.endpos.z + m_flOriginToLowestLegHeight;
		}
		
		// Move our Z towards the wanted Z.
		if ( m_flWantedZ != -1 )
		{
			Vector vCur = vCenter;
			vCur.z = Approach( m_flWantedZ, vCur.z, STRIDER_TORSO_VERTICAL_SLIDE_SPEED * dt );
			SetAbsOrigin( GetAbsOrigin() + Vector( 0, 0, vCur.z - vCenter.z ) );
		}		
	}
}


void CWalkerMiniStrider::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Sapper removal
	if ( RemoveEnemyAttachments( pActivator ) )
		return;

	CBaseTFPlayer *pPlayer = dynamic_cast<CBaseTFPlayer*>(pActivator);
	if ( !pPlayer || !InSameTeam( pPlayer ) )
		return;
	
	// Ok, put them in the driver role.
	AttemptToBoardVehicle( pPlayer );
}


void CWalkerMiniStrider::SetupMove( CBasePlayer *pPlayer, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	BaseClass::SetupMove( pPlayer, ucmd, pHelper, move );
}


bool CWalkerMiniStrider::StartBuilding( CBaseEntity *pBuilder )
{
	return BaseClass::StartBuilding( pBuilder );
}


Vector CWalkerMiniStrider::GetWalkerLocalMovement()
{
	float dt = GetTimeDelta();

	Vector vForward, vRight;
	AngleVectors( GetLocalAngles(), &vForward, &vRight, NULL );

	float flSpeed = (tf_skirmisher_speed.GetFloat() / 100) * dt;
	Vector vMovement = 
		vForward * (GetSteerVelocity().x * flSpeed) +
		vRight * (GetSteerVelocity().y * flSpeed);

	return GetLocalOrigin() + vMovement;
}


void CWalkerMiniStrider::SetPassenger( int nRole, CBasePlayer *pPassenger )
{
	BaseClass::SetPassenger( nRole, pPassenger );

	if ( nRole == VEHICLE_ROLE_DRIVER && pPassenger )
		UnCrouch();
}


void CWalkerMiniStrider::FootHit( const char *pFootName )
{
	if ( gpGlobals->curtime >= m_flNextFootstepSoundTime )
	{
		Vector footPosition;
		QAngle angles;

		((BaseClass*)this)->GetAttachment( pFootName, footPosition, angles );
		CPASAttenuationFilter filter( this, "Skirmisher.Footstep" );
		EmitSound( filter, entindex(), "Skirmisher.Footstep", &footPosition );

		m_flNextFootstepSoundTime = gpGlobals->curtime + 0.2;
	}
}


void CWalkerMiniStrider::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
		case 1:
		case 2:
		case 3:
		{
			FootHit( "back foot" );
		}
		break;
	}
}
