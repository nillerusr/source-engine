#include "cbase.h"
#include "engine/IEngineSound.h"
#include "in_buttons.h"
#include "ammodef.h"
#include "IEffects.h"
#include "beam_shared.h"
#include "weapon_gauss.h"
#include "soundenvelope.h"
#include "decals.h"
#include "soundent.h"
#include "grenade_ar2.h"
#include "te_effect_dispatch.h"
#include "hl2_player.h"
#include "ndebugoverlay.h"
#include "movevars_shared.h"
#include "bone_setup.h"
#include "ai_basenpc.h"
#include "ai_hint.h"
#include "globalstate.h"
#include "asw_jeep.h"
#include "asw_dummy_vehicle.h"
#include "asw_player.h"
#include "asw_marine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	VEHICLE_HITBOX_DRIVER		1
#define LOCK_SPEED					10
#define JEEP_GUN_YAW				"vehicle_weapon_yaw"
#define JEEP_GUN_PITCH				"vehicle_weapon_pitch"
#define JEEP_GUN_SPIN				"gun_spin"
#define	JEEP_GUN_SPIN_RATE			20
#define SF_PROP_VEHICLE_ALWAYSTHINK		0x00000001

#define CANNON_MAX_UP_PITCH			20
#define CANNON_MAX_DOWN_PITCH		20
#define CANNON_MAX_LEFT_YAW			90
#define CANNON_MAX_RIGHT_YAW		90

#define OVERTURNED_EXIT_WAITTIME	2.0f

#define JEEP_AMMOCRATE_HITGROUP		5

#define JEEP_STEERING_SLOW_ANGLE	50.0f
#define JEEP_STEERING_FAST_ANGLE	15.0f

#define	JEEP_AMMO_CRATE_CLOSE_DELAY	2.0f

#define JEEP_DELTA_LENGTH_MAX	12.0f			// 1 foot
#define JEEP_FRAMETIME_MIN		1e-6

ConVar	asw_sk_jeep_gauss_damage( "asw_sk_jeep_gauss_damage", "15" );
ConVar	asw_hud_jeephint_numentries( "asw_hud_jeephint_numentries", "10", FCVAR_NONE );
ConVar	asw_g_jeepexitspeed( "asw_g_jeepexitspeed", "100", FCVAR_CHEAT );

//=============================================================================
//
// Jeep water data.
//


BEGIN_SIMPLE_DATADESC( ASWJeepWaterData_t )
	DEFINE_ARRAY( m_bWheelInWater,			FIELD_BOOLEAN,	ASW_JEEP_WHEEL_COUNT ),
	DEFINE_ARRAY( m_bWheelWasInWater,			FIELD_BOOLEAN,	ASW_JEEP_WHEEL_COUNT ),
	DEFINE_ARRAY( m_vecWheelContactPoints,	FIELD_VECTOR,	ASW_JEEP_WHEEL_COUNT ),
	DEFINE_ARRAY( m_flNextRippleTime,			FIELD_TIME,		ASW_JEEP_WHEEL_COUNT ),
	DEFINE_FIELD( m_bBodyInWater,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bBodyWasInWater,			FIELD_BOOLEAN ),
END_DATADESC()	

//-----------------------------------------------------------------------------
// Purpose: Four wheel physics vehicle server vehicle with weaponry
//-----------------------------------------------------------------------------
class CASWJeepFourWheelServerVehicle : public CFourWheelServerVehicle
{
	typedef CFourWheelServerVehicle BaseClass;
// IServerVehicle
public:
	bool		NPC_HasPrimaryWeapon( void ) { return true; }
	void		NPC_AimPrimaryWeapon( Vector vecTarget );
	int			GetExitAnimToUse( Vector &vecEyeExitEndpoint, bool &bAllPointsBlocked );
};

BEGIN_DATADESC( CASW_PropJeep )
	DEFINE_FIELD( m_bGunHasBeenCutOff, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flDangerSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_nBulletType, FIELD_INTEGER ),
	DEFINE_FIELD( m_bCannonCharging, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flCannonTime, FIELD_TIME ),
	DEFINE_FIELD( m_flCannonChargeStartTime, FIELD_TIME ),
	DEFINE_FIELD( m_vecGunOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_SOUNDPATCH( m_sndCannonCharge ),
	DEFINE_FIELD( m_nSpinPos, FIELD_INTEGER ),
	DEFINE_FIELD( m_aimYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_aimPitch, FIELD_FLOAT ),
	DEFINE_FIELD( m_throttleDisableTime, FIELD_TIME ),
	DEFINE_FIELD( m_flHandbrakeTime, FIELD_TIME ),
	DEFINE_FIELD( m_bInitialHandbrake, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flOverturnedTime, FIELD_TIME ),
	DEFINE_FIELD( m_flAmmoCrateCloseTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecLastEyePos, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecLastEyeTarget, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecEyeSpeed, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecTargetSpeed, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_bHeadlightIsOn, FIELD_BOOLEAN ),
	DEFINE_EMBEDDED( m_WaterData ),

	DEFINE_FIELD( m_iNumberOfEntries, FIELD_INTEGER ),
	DEFINE_FIELD( m_nAmmoType, FIELD_INTEGER ),

	DEFINE_FIELD( m_flPlayerExitedTime, FIELD_TIME ),
	DEFINE_FIELD( m_flLastSawPlayerAt, FIELD_TIME ),
	DEFINE_FIELD( m_hLastPlayerInVehicle, FIELD_EHANDLE ),		
	DEFINE_FIELD( m_hDriver , FIELD_EHANDLE ),

	DEFINE_INPUTFUNC( FIELD_VOID, "StartRemoveTauCannon", InputStartRemoveTauCannon ),
	DEFINE_INPUTFUNC( FIELD_VOID, "FinishRemoveTauCannon", InputFinishRemoveTauCannon ),
	
	DEFINE_THINKFUNC( DestroyAndReplace ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CASW_PropJeep, DT_ASW_PropJeep )
	//SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	//SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	//SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	//SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	//SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
	SendPropEHandle( SENDINFO( m_hDriver ) ),
	//SendPropExclude( "DT_BaseEntity", "m_angRotation" ),

	////SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	
	// asw_playeranimstate and clientside animation takes care of these on the client
	//SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	//SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),
	
	SendPropBool( SENDINFO( m_bHeadlightIsOn ) ),
END_SEND_TABLE();

// This is overriden for the episodic jeep
#ifndef HL2_EPISODIC
LINK_ENTITY_TO_CLASS( asw_vehicle_jeep, CASW_PropJeep );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_PropJeep::CASW_PropJeep( void )
{
	m_bHasGun = true;
	m_bGunHasBeenCutOff = false;
	m_bCannonCharging = false;
	m_flCannonChargeStartTime = 0;
	m_flCannonTime = 0;
	m_nBulletType = -1;
	m_flOverturnedTime = 0.0f;
	m_iNumberOfEntries = 0;

	m_vecEyeSpeed.Init();

	InitWaterData();

	m_bUnableToFire = true;
	m_flAmmoCrateCloseTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::CreateServerVehicle( void )
{
	// Create our armed server vehicle
	m_pServerVehicle = new CASWJeepFourWheelServerVehicle();
	m_pServerVehicle->SetVehicle( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::Precache( void )
{	

	PrecacheScriptSound( "PropJeep.AmmoClose" );
	PrecacheScriptSound( "PropJeep.FireCannon" );
	PrecacheScriptSound( "PropJeep.FireChargedCannon" );
	PrecacheScriptSound( "PropJeep.AmmoOpen" );

	PrecacheScriptSound( "Jeep.GaussCharge" );

	PrecacheModel( GAUSS_BEAM_SPRITE );

	BaseClass::Precache();
}

//------------------------------------------------
// Spawn
//------------------------------------------------
void CASW_PropJeep::Spawn( void )
{
	// Setup vehicle as a real-wheels car.
	SetVehicleType( VEHICLE_TYPE_CAR_WHEELS );

	BaseClass::Spawn();
	m_flHandbrakeTime = gpGlobals->curtime + 0.1;
	m_bInitialHandbrake = false;

	m_flMinimumSpeedToEnterExit = LOCK_SPEED;

	m_nBulletType = GetAmmoDef()->Index("GaussEnergy");

	if ( m_bHasGun )
	{
		SetBodygroup( 1, true );
	}
	else
	{
		SetBodygroup( 1, false );
	}

	// Initialize pose parameters
	SetPoseParameter( JEEP_GUN_YAW, 0 );
	SetPoseParameter( JEEP_GUN_PITCH, 0 );
	m_nSpinPos = 0;
	SetPoseParameter( JEEP_GUN_SPIN, m_nSpinPos );
	m_aimYaw = 0;
	m_aimPitch = 0;

	AddSolidFlags( FSOLID_NOT_STANDABLE );

	CAmmoDef *pAmmoDef = GetAmmoDef();
	m_nAmmoType = pAmmoDef->Index("GaussEnergy");

	// normal HL2 vehicles don't feel nice in a network game
	//  so destroy ourselves and spawn the serverside component of our custom ASW client authorative vehicle system
}

void CASW_PropJeep::DestroyAndReplace()
{
	Msg("Replacing normal jeep with a dummy jeep\n");
	Vector vecVehicle = GetAbsOrigin();
	QAngle angVehicle = GetAbsAngles();
	UTIL_Remove(this);
	CASW_Dummy_Vehicle *pDummy = dynamic_cast<CASW_Dummy_Vehicle*>(CreateEntityByName("asw_dummy_vehicle"));
	pDummy->SetAbsOrigin(vecVehicle);
	pDummy->SetAbsAngles(angVehicle);
	pDummy->Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &tr - 
//			nDamageType - 
//-----------------------------------------------------------------------------
void CASW_PropJeep::DoImpactEffect( trace_t &tr, int nDamageType )
{
	//Draw our beam
	DrawBeam( tr.startpos, tr.endpos, 2.4 );

	if ( (tr.surface.flags & SURF_SKY) == false )
	{
		CPVSFilter filter( tr.endpos );
		te->GaussExplosion( filter, 0.0f, tr.endpos, tr.plane.normal, 0 );

		UTIL_ImpactTrace( &tr, m_nBulletType );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr )
{
	CTakeDamageInfo info = inputInfo;
	if ( ptr->hitbox != VEHICLE_HITBOX_DRIVER )
	{
		if ( inputInfo.GetDamageType() & DMG_BULLET )
		{
			info.ScaleDamage( 0.0001 );
		}
	}

	BaseClass::TraceAttack( info, vecDir, ptr );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CASW_PropJeep::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	//Do scaled up physics damage to the car
	CTakeDamageInfo info = inputInfo;
	info.ScaleDamage( 25 );
	
	// HACKHACK: Scale up grenades until we get a better explosion/pressure damage system
	if ( inputInfo.GetDamageType() & DMG_BLAST )
	{
		info.SetDamageForce( inputInfo.GetDamageForce() * 10 );
	}
	VPhysicsTakeDamage( info );

	// reset the damage
	info.SetDamage( inputInfo.GetDamage() );

	// small amounts of shock damage disrupt the car, but aren't transferred to the player
	if ( info.GetDamageType() == DMG_SHOCK )
	{
		if ( info.GetDamage() <= 10 )
		{
			// take 10% damage and make the engine stall
			info.ScaleDamage( 0.1 );
			m_throttleDisableTime = gpGlobals->curtime + 2;
		}
	}

	//Check to do damage to driver
	if ( GetDriver() )
	{
		//Take no damage from physics damages
		if ( info.GetDamageType() & DMG_CRUSH )
			return 0;

		// Take the damage (strip out the DMG_BLAST)
		info.SetDamageType( info.GetDamageType() & (~DMG_BLAST) );
		GetDriver()->TakeDamage( info );
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CASW_PropJeep::BodyTarget( const Vector &posSrc, bool bNoisy )
{
	Vector	shotPos;
	matrix3x4_t	matrix;

	int eyeAttachmentIndex = LookupAttachment("vehicle_driver_eyes");
	GetAttachment( eyeAttachmentIndex, matrix );
	MatrixGetColumn( matrix, 3, shotPos );

	if ( bNoisy )
	{
		shotPos[0] += random->RandomFloat( -8.0f, 8.0f );
		shotPos[1] += random->RandomFloat( -8.0f, 8.0f );
		shotPos[2] += random->RandomFloat( -8.0f, 8.0f );
	}

	return shotPos;
}

//-----------------------------------------------------------------------------
// Purpose: Aim Gun at a target
//-----------------------------------------------------------------------------
void CASW_PropJeep::AimGunAt( Vector *endPos, float flInterval )
{
	Vector	aimPos = *endPos;

	// See if the gun should be allowed to aim
	if ( IsOverturned() || m_bEngineLocked || m_bHasGun == false )
	{
		SetPoseParameter( JEEP_GUN_YAW, 0 );
		SetPoseParameter( JEEP_GUN_PITCH, 0 );
		SetPoseParameter( JEEP_GUN_SPIN, 0 );
		return;

		// Make the gun go limp and look "down"
		Vector	v_forward, v_up;
		AngleVectors( GetLocalAngles(), NULL, &v_forward, &v_up );
		aimPos = WorldSpaceCenter() + ( v_forward * -32.0f ) - Vector( 0, 0, 128.0f );
	}

	matrix3x4_t gunMatrix;
	GetAttachment( LookupAttachment("gun_ref"), gunMatrix );

	// transform the enemy into gun space
	Vector localEnemyPosition;
	VectorITransform( aimPos, gunMatrix, localEnemyPosition );

	// do a look at in gun space (essentially a delta-lookat)
	QAngle localEnemyAngles;
	VectorAngles( localEnemyPosition, localEnemyAngles );
	
	// convert to +/- 180 degrees
	localEnemyAngles.x = UTIL_AngleDiff( localEnemyAngles.x, 0 );	
	localEnemyAngles.y = UTIL_AngleDiff( localEnemyAngles.y, 0 );

	float targetYaw = m_aimYaw + localEnemyAngles.y;
	float targetPitch = m_aimPitch + localEnemyAngles.x;
	
	// Constrain our angles
	float newTargetYaw	= clamp( targetYaw, -CANNON_MAX_LEFT_YAW, CANNON_MAX_RIGHT_YAW );
	float newTargetPitch = clamp( targetPitch, -CANNON_MAX_DOWN_PITCH, CANNON_MAX_UP_PITCH );

	// If the angles have been clamped, we're looking outside of our valid range
	if ( fabs(newTargetYaw-targetYaw) > 1e-4 || fabs(newTargetPitch-targetPitch) > 1e-4 )
	{
		m_bUnableToFire = true;
	}

	targetYaw = newTargetYaw;
	targetPitch = newTargetPitch;

	// Exponentially approach the target
	float yawSpeed = 8;
	float pitchSpeed = 8;

	m_aimYaw = UTIL_Approach( targetYaw, m_aimYaw, yawSpeed );
	m_aimPitch = UTIL_Approach( targetPitch, m_aimPitch, pitchSpeed );

	SetPoseParameter( JEEP_GUN_YAW, -m_aimYaw);
	SetPoseParameter( JEEP_GUN_PITCH, -m_aimPitch );

	InvalidateBoneCache();

	// read back to avoid drift when hitting limits
	// as long as the velocity is less than the delta between the limit and 180, this is fine.
	m_aimPitch = -GetPoseParameter( JEEP_GUN_PITCH );
	m_aimYaw = -GetPoseParameter( JEEP_GUN_YAW );

	// Now draw crosshair for actual aiming point
	Vector	vecMuzzle, vecMuzzleDir;
	QAngle	vecMuzzleAng;

	GetAttachment( "Muzzle", vecMuzzle, vecMuzzleAng );
	AngleVectors( vecMuzzleAng, &vecMuzzleDir );

	trace_t	tr;
	UTIL_TraceLine( vecMuzzle, vecMuzzle + (vecMuzzleDir * MAX_TRACE_LENGTH), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

		// see if we hit something, if so, adjust endPos to hit location
	if ( tr.fraction < 1.0 )
	{
		m_vecGunCrosshair = vecMuzzle + ( vecMuzzleDir * MAX_TRACE_LENGTH * tr.fraction );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::InitWaterData( void )
{
	m_WaterData.m_bBodyInWater = false;
	m_WaterData.m_bBodyWasInWater = false;

	for ( int iWheel = 0; iWheel < ASW_JEEP_WHEEL_COUNT; ++iWheel )
	{
		m_WaterData.m_bWheelInWater[iWheel] = false;
		m_WaterData.m_bWheelWasInWater[iWheel] = false;
		m_WaterData.m_vecWheelContactPoints[iWheel].Init();
		m_WaterData.m_flNextRippleTime[iWheel] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::HandleWater( void )
{
	// Only check the wheels and engine in water if we have a driver (player).
	if ( !GetDriver() )
		return;

	// Check to see if we are in water.
	if ( CheckWater() )
	{
		for ( int iWheel = 0; iWheel < ASW_JEEP_WHEEL_COUNT; ++iWheel )
		{
			// Create an entry/exit splash!
			if ( m_WaterData.m_bWheelInWater[iWheel] != m_WaterData.m_bWheelWasInWater[iWheel] )
			{
				CreateSplash( m_WaterData.m_vecWheelContactPoints[iWheel] );
				CreateRipple( m_WaterData.m_vecWheelContactPoints[iWheel] );
			}
			
			// Create ripples.
			if ( m_WaterData.m_bWheelInWater[iWheel] && m_WaterData.m_bWheelWasInWater[iWheel] )
			{
				if ( m_WaterData.m_flNextRippleTime[iWheel] < gpGlobals->curtime )
				{
					// Stagger ripple times
					m_WaterData.m_flNextRippleTime[iWheel] = gpGlobals->curtime + RandomFloat( 0.1, 0.3 );
					CreateRipple( m_WaterData.m_vecWheelContactPoints[iWheel] );
				}
			}
		}
	}

	// Save of data from last think.
	for ( int iWheel = 0; iWheel < ASW_JEEP_WHEEL_COUNT; ++iWheel )
	{
		m_WaterData.m_bWheelWasInWater[iWheel] = m_WaterData.m_bWheelInWater[iWheel];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CASW_PropJeep::CheckWater( void )
{
	bool bInWater = false;

	// Check all four wheels.
	for ( int iWheel = 0; iWheel < ASW_JEEP_WHEEL_COUNT; ++iWheel )
	{
		// Get the current wheel and get its contact point.
		IPhysicsObject *pWheel = m_VehiclePhysics.GetWheel( iWheel );
		if ( !pWheel )
			continue;

		// Check to see if we hit water.
		if ( pWheel->GetContactPoint( &m_WaterData.m_vecWheelContactPoints[iWheel], NULL ) )
		{
			m_WaterData.m_bWheelInWater[iWheel] = ( UTIL_PointContents( m_WaterData.m_vecWheelContactPoints[iWheel], MASK_WATER ) & MASK_WATER ) ? true : false;
			if ( m_WaterData.m_bWheelInWater[iWheel] )
			{
				bInWater = true;
			}
		}
	}

	// Check the body and the BONNET.
	int iEngine = LookupAttachment( "vehicle_engine" );
	Vector vecEnginePoint;
	QAngle vecEngineAngles;
	GetAttachment( iEngine, vecEnginePoint, vecEngineAngles );

	m_WaterData.m_bBodyInWater = ( UTIL_PointContents( vecEnginePoint, MASK_WATER ) & MASK_WATER ) ? true : false;

	if ( m_WaterData.m_bBodyInWater )
	{		
		if ( !m_VehiclePhysics.IsEngineDisabled() )
		{
			m_VehiclePhysics.SetDisableEngine( true );
		}
	}
	else
	{
		if ( m_VehiclePhysics.IsEngineDisabled() )
		{
			m_VehiclePhysics.SetDisableEngine( false );
		}
	}

	if ( bInWater )
	{
		// Check the player's water level.
		CheckWaterLevel();
	}

	return bInWater;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::CheckWaterLevel( void )
{
	CBaseEntity *pEntity = GetDriver();
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast<CBasePlayer*>( pEntity );
		
		Vector vecAttachPoint;
		QAngle vecAttachAngles;
		
		// Check eyes. (vehicle_driver_eyes point)
		int iAttachment = LookupAttachment( "vehicle_driver_eyes" );
		GetAttachment( iAttachment, vecAttachPoint, vecAttachAngles );

		// Add the jeep's Z view offset
		Vector vecUp;
		AngleVectors( vecAttachAngles, NULL, NULL, &vecUp );
		vecUp.z = clamp( vecUp.z, 0.0f, vecUp.z );
		vecAttachPoint.z += r_JeepViewZHeight.GetFloat() * vecUp.z;

		bool bEyes = ( UTIL_PointContents( vecAttachPoint, MASK_WATER ) & MASK_WATER ) ? true : false;
		if ( bEyes )
		{
			pPlayer->SetWaterLevel( WL_Eyes );
			return;
		}

		// Check waist.  (vehicle_engine point -- see parent function).
		if ( m_WaterData.m_bBodyInWater )
		{
			pPlayer->SetWaterLevel( WL_Waist );
			return;
		}

		// Check feet. (vehicle_feet_passenger0 point)
		iAttachment = LookupAttachment( "vehicle_feet_passenger0" );
		GetAttachment( iAttachment, vecAttachPoint, vecAttachAngles );
		bool bFeet = ( UTIL_PointContents( vecAttachPoint, MASK_WATER ) & MASK_WATER ) ? true : false;
		if ( bFeet )
		{
			pPlayer->SetWaterLevel( WL_Feet );
			return;
		}

		// Not in water.
		pPlayer->SetWaterLevel( WL_NotInWater );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::CreateSplash( const Vector &vecPosition )
{
	// Splash data.
	CEffectData	data;
	data.m_fFlags = 0;
	data.m_vOrigin = vecPosition;
	data.m_vNormal.Init( 0.0f, 0.0f, 1.0f );
	VectorAngles( data.m_vNormal, data.m_vAngles );
	data.m_flScale = 10.0f + random->RandomFloat( 0, 2 );

	// Create the splash..
	DispatchEffect( "watersplash", data );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::CreateRipple( const Vector &vecPosition )
{
	// Ripple data.
	CEffectData	data;
	data.m_fFlags = 0;
	data.m_vOrigin = vecPosition;
	data.m_vNormal.Init( 0.0f, 0.0f, 1.0f );
	VectorAngles( data.m_vNormal, data.m_vAngles );
	data.m_flScale = 10.0f + random->RandomFloat( 0, 2 );
	if ( GetWaterType() & CONTENTS_SLIME )
	{
		data.m_fFlags |= FX_WATER_IN_SLIME;
	}

	// Create the ripple.
	DispatchEffect( "waterripple", data );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::Think(void)
{
	if (gpGlobals->maxClients > 1)
	{
		DestroyAndReplace();
	}
	else
	{
		ThinkTick();
	}
}

void CASW_PropJeep::ThinkTick()
{
	m_VehiclePhysics.Think();

	// Derived classes of CPropVehicle have their own code to determine how frequently to think.
	// But the prop_vehicle entity native to this class will only think one time, so this flag
	// was added to allow prop_vehicle to always think without affecting the derived classes.
	if( HasSpawnFlags(SF_PROP_VEHICLE_ALWAYSTHINK) )
	{
		SetNextThink(gpGlobals->curtime);
	}

	if ( ShouldThink() )
	{
		SetNextThink( gpGlobals->curtime );
	}

	// If we have an NPC Driver, tell him to drive
	if ( m_hNPCDriver )
	{
		GetServerVehicle()->NPC_DriveVehicle();
	}

	// Keep thinking while we're waiting to turn off the keep upright
	if ( m_flTurnOffKeepUpright )
	{
		SetNextThink( gpGlobals->curtime );

		// Time up?
		if ( m_hKeepUpright != NULL && m_flTurnOffKeepUpright < gpGlobals->curtime )
		{
			variant_t emptyVariant;
			m_hKeepUpright->AcceptInput( "TurnOff", this, this, emptyVariant, USE_TOGGLE );
			m_flTurnOffKeepUpright = 0;

			UTIL_Remove( m_hKeepUpright );
		}
	}
	/*
	CBasePlayer	*pPlayer = UTIL_GetLocalPlayer();

	if ( m_bEngineLocked )
	{
		m_bUnableToFire = true;
		
		if ( pPlayer != NULL )
		{
			pPlayer->m_Local.m_iHideHUD |= HIDEHUD_VEHICLE_CROSSHAIR;
		}
	}
	else if ( m_bHasGun )
	{
		// Start this as false and update it again each frame
		m_bUnableToFire = false;

		if ( pPlayer != NULL )
		{
			pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_VEHICLE_CROSSHAIR;
		}
	}*/

	// Water!?
	HandleWater();

	SetSimulationTime( gpGlobals->curtime );
	
	SetNextThink( gpGlobals->curtime );
	SetAnimatedEveryTick( true );

    if ( !m_bInitialHandbrake )	// after initial timer expires, set the handbrake
	{
		m_bInitialHandbrake = true;
		m_VehiclePhysics.SetHandbrake( true );
		m_VehiclePhysics.Think();
	}

	// Check overturned status.
	if ( !IsOverturned() )
	{
		m_flOverturnedTime = 0.0f;
	}
	else
	{
		m_flOverturnedTime += gpGlobals->frametime;
	}

	// spin gun if charging cannon
	//FIXME: Don't bother for E3
	if ( m_bCannonCharging )
	{
		m_nSpinPos += JEEP_GUN_SPIN_RATE;
		SetPoseParameter( JEEP_GUN_SPIN, m_nSpinPos );
	}

	// Aim gun based on the player view direction.
	if ( m_hPlayer && !m_bExitAnimOn && !m_bEnterAnimOn )
	{
		Vector vecEyeDir, vecEyePos;
		m_hPlayer->EyePositionAndVectors( &vecEyePos, &vecEyeDir, NULL, NULL );

		// Trace out from the player's eye point.
		Vector	vecEndPos = vecEyePos + ( vecEyeDir * MAX_TRACE_LENGTH );
		trace_t	trace;
		UTIL_TraceLine( vecEyePos, vecEndPos, MASK_SHOT, this, COLLISION_GROUP_NONE, &trace );

		// See if we hit something, if so, adjust end position to hit location.
		if ( trace.fraction < 1.0 )
		{
   			vecEndPos = vecEyePos + ( vecEyeDir * MAX_TRACE_LENGTH * trace.fraction );
		}

		//m_vecLookCrosshair = vecEndPos;
		AimGunAt( &vecEndPos, 0.1f );
	}

	StudioFrameAdvance();

	// If the enter or exit animation has finished, tell the server vehicle
	if ( IsSequenceFinished() && (m_bExitAnimOn || m_bEnterAnimOn) )
	{
		if ( m_bEnterAnimOn )
		{
			m_VehiclePhysics.ReleaseHandbrake();
			StartEngine();

			// HACKHACK: This forces the jeep to play a sound when it gets entered underwater
			if ( m_VehiclePhysics.IsEngineDisabled() )
			{
				CBaseServerVehicle *pServerVehicle = dynamic_cast<CBaseServerVehicle *>(GetServerVehicle());
				if ( pServerVehicle )
				{
					pServerVehicle->SoundStartDisabled();
				}
			}

			// The first few time we get into the jeep, print the jeep help
			if ( m_iNumberOfEntries < asw_hud_jeephint_numentries.GetInt() )
			{
				UTIL_HudHintText( m_hPlayer, "#Valve_Hint_JeepKeys" );
				m_iNumberOfEntries++;
			}
		}
		
		// If we're exiting and have had the tau cannon removed, we don't want to reset the animation
		GetServerVehicle()->HandleEntryExitFinish( m_bExitAnimOn, !(m_bExitAnimOn && TauCannonHasBeenCutOff()) );
	}

	// See if the ammo crate needs to close
	if ( ( m_flAmmoCrateCloseTime < gpGlobals->curtime ) && ( GetSequence() == LookupSequence( "ammo_open" ) ) )
	{
		m_flAnimTime = gpGlobals->curtime;
		m_flPlaybackRate = 0.0;
		SetCycle( 0 );
		ResetSequence( LookupSequence( "ammo_close" ) );
	}
	else if ( ( GetSequence() == LookupSequence( "ammo_close" ) ) && IsSequenceFinished() )
	{
		m_flAnimTime = gpGlobals->curtime;
		m_flPlaybackRate = 0.0;
		SetCycle( 0 );
		ResetSequence( LookupSequence( "idle" ) );

		CPASAttenuationFilter sndFilter( this, "PropJeep.AmmoClose" );
		EmitSound( sndFilter, entindex(), "PropJeep.AmmoClose" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &startPos - 
//			&endPos - 
//			width - 
//			useMuzzle - 
//-----------------------------------------------------------------------------
void CASW_PropJeep::DrawBeam( const Vector &startPos, const Vector &endPos, float width )
{
	//Tracer down the middle
	UTIL_Tracer( startPos, endPos, 0, TRACER_DONT_USE_ATTACHMENT, 6500, false, "GaussTracer" );

	//Draw the main beam shaft
	CBeam *pBeam = CBeam::BeamCreate( GAUSS_BEAM_SPRITE, 0.5 );
	
	pBeam->SetStartPos( startPos );
	pBeam->PointEntInit( endPos, this );
	pBeam->SetEndAttachment( LookupAttachment("Muzzle") );
	pBeam->SetWidth( width );
	pBeam->SetEndWidth( 0.05f );
	pBeam->SetBrightness( 255 );
	pBeam->SetColor( 255, 185+random->RandomInt( -16, 16 ), 40 );
	pBeam->RelinkBeam();
	pBeam->LiveForTime( 0.1f );

	//Draw electric bolts along shaft
	pBeam = CBeam::BeamCreate( GAUSS_BEAM_SPRITE, 3.0f );
	
	pBeam->SetStartPos( startPos );
	pBeam->PointEntInit( endPos, this );
	pBeam->SetEndAttachment( LookupAttachment("Muzzle") );

	pBeam->SetBrightness( random->RandomInt( 64, 255 ) );
	pBeam->SetColor( 255, 255, 150+random->RandomInt( 0, 64 ) );
	pBeam->RelinkBeam();
	pBeam->LiveForTime( 0.1f );
	pBeam->SetNoise( 1.6f );
	pBeam->SetEndWidth( 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::FireCannon( void )
{
	//Don't fire again if it's been too soon
	if ( m_flCannonTime > gpGlobals->curtime )
		return;

	if ( m_bUnableToFire )
		return;

	m_flCannonTime = gpGlobals->curtime + 0.2f;
	m_bCannonCharging = false;

	//Find the direction the gun is pointing in
	Vector aimDir;
	GetCannonAim( &aimDir );

	FireBulletsInfo_t	info( 1, m_vecGunOrigin, aimDir, VECTOR_CONE_1DEGREES, MAX_TRACE_LENGTH, m_nAmmoType );

	info.m_nFlags = FIRE_BULLETS_ALLOW_WATER_SURFACE_IMPACTS;
	info.m_pAttacker = m_hPlayer;

	FireBullets( info );

	// Register a muzzleflash for the AI
	if ( m_hPlayer )
	{
		m_hPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );
	}

	CPASAttenuationFilter sndFilter( this, "PropJeep.FireCannon" );
	EmitSound( sndFilter, entindex(), "PropJeep.FireCannon" );
	
	// make cylinders of gun spin a bit
	m_nSpinPos += JEEP_GUN_SPIN_RATE;
	//SetPoseParameter( JEEP_GUN_SPIN, m_nSpinPos );	//FIXME: Don't bother with this for E3, won't look right
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::FireChargedCannon( void )
{
	bool penetrated = false;

	m_bCannonCharging	= false;
	m_flCannonTime		= gpGlobals->curtime + 0.5f;

	StopChargeSound();

	CPASAttenuationFilter sndFilter( this, "PropJeep.FireChargedCannon" );
	EmitSound( sndFilter, entindex(), "PropJeep.FireChargedCannon" );

	//Find the direction the gun is pointing in
	Vector aimDir;
	GetCannonAim( &aimDir );

	Vector endPos = m_vecGunOrigin + ( aimDir * MAX_TRACE_LENGTH );
	
	//Shoot a shot straight out
	trace_t	tr;
	UTIL_TraceLine( m_vecGunOrigin, endPos, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
	
	ClearMultiDamage();

	//Find how much damage to do
	float flChargeAmount = ( gpGlobals->curtime - m_flCannonChargeStartTime ) / MAX_GAUSS_CHARGE_TIME;

	//Clamp this
	if ( flChargeAmount > 1.0f )
	{
		flChargeAmount = 1.0f;
	}

	//Determine the damage amount
	//FIXME: Use ConVars!
	float flDamage = 15 + ( ( 250 - 15 ) * flChargeAmount );

	CBaseEntity *pHit = tr.m_pEnt;
	
	//Look for wall penetration
	if ( tr.DidHitWorld() && !(tr.surface.flags & SURF_SKY) )
	{
		//Try wall penetration
		UTIL_ImpactTrace( &tr, m_nBulletType, "ImpactJeep" );
		UTIL_DecalTrace( &tr, "RedGlowFade" );

		CPVSFilter filter( tr.endpos );
		te->GaussExplosion( filter, 0.0f, tr.endpos, tr.plane.normal, 0 );
		
		Vector	testPos = tr.endpos + ( aimDir * 48.0f );

		UTIL_TraceLine( testPos, tr.endpos, MASK_SHOT, GetDriver(), COLLISION_GROUP_NONE, &tr );
			
		if ( tr.allsolid == false )
		{
			UTIL_DecalTrace( &tr, "RedGlowFade" );

			penetrated = true;
		}
	}
	else if ( pHit != NULL )
	{
		CTakeDamageInfo dmgInfo( this, GetDriver(), flDamage, DMG_SHOCK );
		CalculateBulletDamageForce( &dmgInfo, GetAmmoDef()->Index("GaussEnergy"), aimDir, tr.endpos, 1.0f + flChargeAmount * 4.0f );

		//Do direct damage to anything in our path
		pHit->DispatchTraceAttack( dmgInfo, aimDir, &tr );
	}

	ApplyMultiDamage();

	//Kick up an effect
	if ( !(tr.surface.flags & SURF_SKY) )
	{
  		UTIL_ImpactTrace( &tr, m_nBulletType, "ImpactJeep" );

		//Do a gauss explosion
		CPVSFilter filter( tr.endpos );
		te->GaussExplosion( filter, 0.0f, tr.endpos, tr.plane.normal, 0 );
	}

	//Show the effect
	DrawBeam( m_vecGunOrigin, tr.endpos, 9.6 );

	// Register a muzzleflash for the AI
	if ( m_hPlayer )
	{
		m_hPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5f );
	}

	//Rock the car
	IPhysicsObject *pObj = VPhysicsGetObject();

	if ( pObj != NULL )
	{
		Vector	shoveDir = aimDir * -( flDamage * 500.0f );

		pObj->ApplyForceOffset( shoveDir, m_vecGunOrigin );
	}

	//Do radius damage if we didn't penetrate the wall
	if ( penetrated == true )
	{
		RadiusDamage( CTakeDamageInfo( this, this, flDamage, DMG_SHOCK ), tr.endpos, 200.0f, CLASS_NONE, NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::ChargeCannon( void )
{
	//Don't fire again if it's been too soon
	if ( m_flCannonTime > gpGlobals->curtime )
		return;

	//See if we're starting a charge
	if ( m_bCannonCharging == false )
	{
		m_flCannonChargeStartTime = gpGlobals->curtime;
		m_bCannonCharging = true;

		//Start charging sound
		CPASAttenuationFilter filter( this );
		m_sndCannonCharge = (CSoundEnvelopeController::GetController()).SoundCreate( filter, entindex(), CHAN_STATIC, "Jeep.GaussCharge", ATTN_NORM );

		assert(m_sndCannonCharge!=NULL);
		if ( m_sndCannonCharge != NULL )
		{
			(CSoundEnvelopeController::GetController()).Play( m_sndCannonCharge, 1.0f, 50 );
			(CSoundEnvelopeController::GetController()).SoundChangePitch( m_sndCannonCharge, 250, 3.0f );
		}

		return;
	}

	//TODO: Add muzzle effect?

	//TODO: Check for overcharge and have the weapon simply fire or instead "decharge"?
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::StopChargeSound( void )
{
	if ( m_sndCannonCharge != NULL )
	{
		(CSoundEnvelopeController::GetController()).SoundFadeOut( m_sndCannonCharge, 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds the true aiming position of the gun (looks at what player 
//			is looking at and adjusts)
// Input  : &resultDir - direction to be calculated
//-----------------------------------------------------------------------------
void CASW_PropJeep::GetCannonAim( Vector *resultDir )
{
	Vector	muzzleOrigin;
	QAngle	muzzleAngles;

	GetAttachment( LookupAttachment("gun_ref"), muzzleOrigin, muzzleAngles );

	AngleVectors( muzzleAngles, resultDir );
}

//-----------------------------------------------------------------------------
// Purpose: If the player uses the jeep while at the back, he gets ammo from the crate instead
//-----------------------------------------------------------------------------
void CASW_PropJeep::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	
	if ( pPlayer == NULL)
		return;

	// Find out if the player's looking at our ammocrate hitbox 
	Vector vecForward;
	pPlayer->EyeVectors( &vecForward, NULL, NULL );

	trace_t tr;
	Vector vecStart = pPlayer->EyePosition();
	UTIL_TraceLine( vecStart, vecStart + vecForward * 1024, MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_HITBOX, pPlayer, COLLISION_GROUP_NONE, &tr );
	
	if ( tr.m_pEnt == this && tr.hitgroup == JEEP_AMMOCRATE_HITGROUP )
	{
		// Player's using the crate.
		// Fill up his SMG ammo.
		pPlayer->GiveAmmo( 300, "SMG1");
		
		if ( ( GetSequence() != LookupSequence( "ammo_open" ) ) && ( GetSequence() != LookupSequence( "ammo_close" ) ) )
		{
			// Open the crate
			m_flAnimTime = gpGlobals->curtime;
			m_flPlaybackRate = 0.0;
			SetCycle( 0 );
			ResetSequence( LookupSequence( "ammo_open" ) );
			
			CPASAttenuationFilter sndFilter( this, "PropJeep.AmmoOpen" );
			EmitSound( sndFilter, entindex(), "PropJeep.AmmoOpen" );
		}

		m_flAmmoCrateCloseTime = gpGlobals->curtime + JEEP_AMMO_CRATE_CLOSE_DELAY;
		return;
	}

	// Fall back and get in the vehicle instead
	BaseClass::Use( pActivator, pCaller, useType, value );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CASW_PropJeep::CanExitVehicle( CBaseEntity *pEntity )
{
	return ( !m_bEnterAnimOn && !m_bExitAnimOn && !m_bLocked && (m_nSpeed <= asw_g_jeepexitspeed.GetFloat() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::DampenEyePosition( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles )
{
	// Get the frametime. (Check to see if enough time has passed to warrent dampening).
	float flFrameTime = gpGlobals->frametime;
	if ( flFrameTime < JEEP_FRAMETIME_MIN )
	{
		vecVehicleEyePos = m_vecLastEyePos;
		DampenUpMotion( vecVehicleEyePos, vecVehicleEyeAngles, 0.0f );
		return;
	}

	// Keep static the sideways motion.

	// Dampen forward/backward motion.
	DampenForwardMotion( vecVehicleEyePos, vecVehicleEyeAngles, flFrameTime );

	// Blend up/down motion.
	DampenUpMotion( vecVehicleEyePos, vecVehicleEyeAngles, flFrameTime );
}

//-----------------------------------------------------------------------------
// Use the controller as follows:
// speed += ( pCoefficientsOut[0] * ( targetPos - currentPos ) + pCoefficientsOut[1] * ( targetSpeed - currentSpeed ) ) * flDeltaTime;
//-----------------------------------------------------------------------------
void CASW_PropJeep::ComputePDControllerCoefficients( float *pCoefficientsOut,
												  float flFrequency, float flDampening,
												  float flDeltaTime )
{
	float flKs = 9.0f * flFrequency * flFrequency;
	float flKd = 4.5f * flFrequency * flDampening;

	float flScale = 1.0f / ( 1.0f + flKd * flDeltaTime + flKs * flDeltaTime * flDeltaTime );

	pCoefficientsOut[0] = flKs * flScale;
	pCoefficientsOut[1] = ( flKd + flKs * flDeltaTime ) * flScale;
}
 
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_PropJeep::DampenForwardMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime )
{
	// Get forward vector.
	Vector vecForward;
	AngleVectors( vecVehicleEyeAngles, &vecForward);

	// Simulate the eye position forward based on the data from last frame
	// (assumes no acceleration - it will get that from the "spring").
	Vector vecCurrentEyePos = m_vecLastEyePos + m_vecEyeSpeed * flFrameTime;

	// Calculate target speed based on the current vehicle eye position and the last vehicle eye position and frametime.
	Vector vecVehicleEyeSpeed = ( vecVehicleEyePos - m_vecLastEyeTarget ) / flFrameTime;
	m_vecLastEyeTarget = vecVehicleEyePos;	

	// Calculate the speed and position deltas.
	Vector vecDeltaSpeed = vecVehicleEyeSpeed - m_vecEyeSpeed;
	Vector vecDeltaPos = vecVehicleEyePos - vecCurrentEyePos;

	// Clamp.
	if ( vecDeltaPos.Length() > JEEP_DELTA_LENGTH_MAX )
	{
		float flSign = vecForward.Dot( vecVehicleEyeSpeed ) >= 0.0f ? -1.0f : 1.0f;
		vecVehicleEyePos += flSign * ( vecForward * JEEP_DELTA_LENGTH_MAX );
		m_vecLastEyePos = vecVehicleEyePos;
		m_vecEyeSpeed = vecVehicleEyeSpeed;
		return;
	}

	// Generate an updated (dampening) speed for use in next frames position extrapolation.
	float flCoefficients[2];
	ComputePDControllerCoefficients( flCoefficients, r_JeepViewDampenFreq.GetFloat(), r_JeepViewDampenDamp.GetFloat(), flFrameTime );
	m_vecEyeSpeed += ( ( flCoefficients[0] * vecDeltaPos + flCoefficients[1] * vecDeltaSpeed ) * flFrameTime );

	// Save off data for next frame.
	m_vecLastEyePos = vecCurrentEyePos;

	// Move eye forward/backward.
	Vector vecForwardOffset = vecForward * ( vecForward.Dot( vecDeltaPos ) );
	vecVehicleEyePos -= vecForwardOffset;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_PropJeep::DampenUpMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime )
{
	// Get up vector.
	Vector vecUp;
	AngleVectors( vecVehicleEyeAngles, NULL, NULL, &vecUp );
	vecUp.z = clamp( vecUp.z, 0.0f, vecUp.z );
	vecVehicleEyePos.z += r_JeepViewZHeight.GetFloat() * vecUp.z;

	// NOTE: Should probably use some damped equation here.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	//Msg("SetupMove cnum=%d [S] forward=%f side=%f", ucmd->command_number, ucmd->forwardmove, ucmd->sidemove);
	// asw comment
	/*
	// If we are overturned and hit any key - leave the vehicle (IN_USE is already handled!).
	if ( m_flOverturnedTime > OVERTURNED_EXIT_WAITTIME )
	{
		if ( (ucmd->buttons & (IN_FORWARD|IN_BACK|IN_MOVELEFT|IN_MOVERIGHT|IN_SPEED|IN_JUMP|IN_ATTACK|IN_ATTACK2) ) && !m_bExitAnimOn )
		{
			// Can't exit yet? We're probably still moving. Swallow the keys.
			if ( !CanExitVehicle(player) )
				return;

			if ( !GetServerVehicle()->HandlePassengerExit( m_hPlayer ) && ( m_hPlayer != NULL ) )
			{
				m_hPlayer->PlayUseDenySound();
			}
			return;
		}
	}
	*/

	// If the throttle is disabled or we're upside-down, don't allow throttling (including turbo)
	CUserCmd tmp;
	if ( ( m_throttleDisableTime > gpGlobals->curtime ) || ( IsOverturned() ) )
	{
		m_bUnableToFire = true;
		
		tmp = (*ucmd);
		tmp.buttons &= ~(IN_FORWARD|IN_BACK|IN_SPEED);
		// asw comment, no tampering with buttons at this time
		//ucmd = &tmp;
	}
	
	BaseClass::SetupMove( player, ucmd, pHelper, move );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::DriveVehicle( float flFrameTime, CUserCmd *ucmd, int iButtonsDown, int iButtonsReleased )
{
	int iButtons = ucmd->buttons;

	//Adrian: No headlights on Superfly.
/*	if ( ucmd->impulse == 100 )
	{
		if (HeadlightIsOn())
		{
			HeadlightTurnOff();
		}
        else 
		{
			HeadlightTurnOn();
		}
	}*/
		
	// Only handle the cannon if the vehicle has one
	if ( m_bHasGun )
	{
		// If we're holding down an attack button, update our state
		if ( IsOverturned() == false )
		{
			if ( iButtons & IN_ATTACK )
			{
				if ( m_bCannonCharging )
				{
					FireChargedCannon();
				}
				else
				{
					FireCannon();
				}
			}
			else if ( iButtons & IN_ATTACK2 )
			{
				ChargeCannon();
			}
		}

		// If we've released our secondary button, fire off our cannon
		if ( ( iButtonsReleased & IN_ATTACK2 ) && ( m_bCannonCharging ) )
		{
			FireChargedCannon();
		}
	}

	BaseClass::DriveVehicle( flFrameTime, ucmd, iButtonsDown, iButtonsReleased );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			*pMoveData - 
//-----------------------------------------------------------------------------
void CASW_PropJeep::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData )
{
	//Msg("[S]   PreProcess x=%f\t\ty=%f\t\tz=%f\t\tp=%\t\ty=%\t\tr=%\t\tvx=%f\t\tvy=%f\t\tvz=%f\n",
		//GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z,
		//GetAbsAngles().x, GetAbsAngles().y, GetAbsAngles().z,
		//GetAbsVelocity().x, GetAbsVelocity().y, GetAbsVelocity().z);
	BaseClass::ProcessMovement( pPlayer, pMoveData );

	// Update the steering angles based on speed.
	UpdateSteeringAngle();

	// Create dangers sounds in front of the vehicle.
	CreateDangerSounds();

	//ThinkTick();
	//Msg("[S]   PostProcess x=%f\t\ty=%f\t\tz=%f\t\tp=%\t\ty=%\t\tr=%\t\tvx=%f\t\tvy=%f\t\tvz=%f\n",
		//GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z,
		//GetAbsAngles().x, GetAbsAngles().y, GetAbsAngles().z,
		//GetAbsVelocity().x, GetAbsVelocity().y, GetAbsVelocity().z);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::UpdateSteeringAngle( void )
{
	float flMaxSpeed = m_VehiclePhysics.GetMaxSpeed();
	float flSpeed = m_VehiclePhysics.GetSpeed();

	float flRatio = 1.0f - ( flSpeed / flMaxSpeed );
	float flSteeringDegrees = JEEP_STEERING_FAST_ANGLE + ( ( JEEP_STEERING_SLOW_ANGLE - JEEP_STEERING_FAST_ANGLE ) * flRatio );
	flSteeringDegrees = clamp( flSteeringDegrees, JEEP_STEERING_FAST_ANGLE, JEEP_STEERING_SLOW_ANGLE );
	m_VehiclePhysics.SetSteeringDegrees( flSteeringDegrees );
}

//-----------------------------------------------------------------------------
// Purpose: Create danger sounds in front of the vehicle.
//-----------------------------------------------------------------------------
void CASW_PropJeep::CreateDangerSounds( void )
{
	QAngle dummy;
	GetAttachment( "Muzzle", m_vecGunOrigin, dummy );

	if ( m_flDangerSoundTime > gpGlobals->curtime )
		return;

	QAngle vehicleAngles = GetLocalAngles();
	Vector vecStart = GetAbsOrigin();
	Vector vecDir, vecRight;

	GetVectors( &vecDir, &vecRight, NULL );

	const float soundDuration = 0.25;
	float speed = m_VehiclePhysics.GetHLSpeed();
	// Make danger sounds ahead of the jeep
	if ( fabs(speed) > 120 )
	{
		Vector	vecSpot;

		float steering = m_VehiclePhysics.GetSteering();
		if ( steering != 0 )
		{
			if ( speed > 0 )
			{
				vecDir += vecRight * steering * 0.5;
			}
			else
			{
				vecDir -= vecRight * steering * 0.5;
			}
			VectorNormalize(vecDir);
		}
		const float radius = speed * 0.4;
		// 0.3 seconds ahead of the jeep
		vecSpot = vecStart + vecDir * (speed * 0.3f);
		CSoundEnt::InsertSound( SOUND_DANGER, vecSpot, radius, soundDuration, this, 0 );
		CSoundEnt::InsertSound( SOUND_PHYSICS_DANGER, vecSpot, radius, soundDuration, this, 1 );
		//NDebugOverlay::Box(vecSpot, Vector(-radius,-radius,-radius),Vector(radius,radius,radius), 255, 0, 255, 0, soundDuration);

#if 0
		trace_t	tr;
		// put sounds a bit to left and right but slightly closer to Jeep to make a "cone" of sound 
		// in front of it
		vecSpot = vecStart + vecDir * (speed * 0.5f) - vecRight * speed * 0.5;
		UTIL_TraceLine( vecStart, vecSpot, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		CSoundEnt::InsertSound( SOUND_DANGER, vecSpot, 400, soundDuration, this, 1 );

		vecSpot = vecStart + vecDir * (speed * 0.5f) + vecRight * speed * 0.5;
		UTIL_TraceLine( vecStart, vecSpot, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		CSoundEnt::InsertSound( SOUND_DANGER, vecSpot, 400, soundDuration, this, 2);
#endif
	}

	m_flDangerSoundTime = gpGlobals->curtime + 0.1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::EnterVehicle( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	CheckWater();
	BaseClass::EnterVehicle( pPlayer );
	
	m_hLastPlayerInVehicle = m_hPlayer;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::ExitVehicle( int iRole )
{
	HeadlightTurnOff();

	BaseClass::ExitVehicle(iRole);

	//If the player has exited, stop charging
	StopChargeSound();
	m_bCannonCharging = false;

	// Remember when we last saw the player
	m_flPlayerExitedTime = gpGlobals->curtime;
	m_flLastSawPlayerAt = gpGlobals->curtime;	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::InputStartRemoveTauCannon( inputdata_t &inputdata )
{
	// Start the gun removal animation
	m_flAnimTime = gpGlobals->curtime;
	m_flPlaybackRate = 0.0;
	SetCycle( 0 );
	ResetSequence( LookupSequence( "tau_levitate" ) );

	m_bGunHasBeenCutOff = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_PropJeep::InputFinishRemoveTauCannon( inputdata_t &inputdata )
{
	// Remove & hide the gun
	SetBodygroup( 1, false );
	m_bHasGun = false;
}

//========================================================================================================================================
// JEEP FOUR WHEEL PHYSICS VEHICLE SERVER VEHICLE
//========================================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASWJeepFourWheelServerVehicle::NPC_AimPrimaryWeapon( Vector vecTarget )
{
	((CASW_PropJeep*)m_pVehicle)->AimGunAt( &vecTarget, 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecEyeExitEndpoint - 
// Output : int
//-----------------------------------------------------------------------------
int CASWJeepFourWheelServerVehicle::GetExitAnimToUse( Vector &vecEyeExitEndpoint, bool &bAllPointsBlocked )
{
	bAllPointsBlocked = false;

	if ( !m_bParsedAnimations )
	{
		// Load the entry/exit animations from the vehicle
		ParseEntryExitAnims();
		m_bParsedAnimations = true;
	}

	CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating *>(m_pVehicle);
	// If we don't have the gun anymore, we want to get out using the "gun-less" animation
	if ( pAnimating && ((CASW_PropJeep*)m_pVehicle)->TauCannonHasBeenCutOff() )
	{
		// HACK: We know the tau-cannon removed exit anim uses the first upright anim's exit details
		trace_t tr;

		// Convert our offset points to worldspace ones
		Vector vehicleExitOrigin = m_ExitAnimations[0].vecExitPointLocal;
		QAngle vehicleExitAngles = m_ExitAnimations[0].vecExitAnglesLocal;
		UTIL_ParentToWorldSpace( pAnimating, vehicleExitOrigin, vehicleExitAngles );

		// Ensure the endpoint is clear by dropping a point down from above
		vehicleExitOrigin -= VEC_VIEW;
		Vector vecMove = Vector(0,0,64);
		Vector vecStart = vehicleExitOrigin + vecMove;
		Vector vecEnd = vehicleExitOrigin - vecMove;
		UTIL_TraceHull( vecStart, vecEnd, VEC_HULL_MIN, VEC_HULL_MAX, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );

		Assert( !tr.startsolid && tr.fraction < 1.0 );
		m_vecCurrentExitEndPoint = vecStart + ((vecEnd - vecStart) * tr.fraction);
		vecEyeExitEndpoint = m_vecCurrentExitEndPoint + VEC_VIEW;
		m_iCurrentExitAnim = 0;
		return pAnimating->LookupSequence( "exit_tauremoved" );
	}

	return BaseClass::GetExitAnimToUse( vecEyeExitEndpoint, bAllPointsBlocked );
}


// implement driver interface
CASW_Marine* CASW_PropJeep::ASWGetDriver()
{
	return dynamic_cast<CASW_Marine*>(m_hDriver.Get());
}

void CASW_PropJeep::ActivateUseIcon( CASW_Marine* pMarine, int nHoldType )
{
	if ( nHoldType == ASW_USE_HOLD_START )
		return;

	if ( pMarine )
	{
		if ( pMarine->IsInVehicle() )
			pMarine->StopDriving(this);
		else
			pMarine->StartDriving(this);
	}
}

bool CASW_PropJeep::IsUsable(CBaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}