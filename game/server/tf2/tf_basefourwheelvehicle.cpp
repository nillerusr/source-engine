//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A moving vehicle that is used as a battering ram
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_basefourwheelvehicle.h"
#include "engine/IEngineSound.h"
#include "soundenvelope.h"
#include "vcollide_parse.h"
#include "in_buttons.h"
#include "tf_movedata.h"

#define BASEFOURWHEELEDVEHICLE_THINK_CONTEXT		"BaseFourWheeledThink"
#define BASEFOURWHEELEDVEHICLE_DEPLOYTHINK_CONTEXT	"BaseFourWheeledDeployThink"
// HACK HAC
#define BASEFOURWHEELEDVEHICLE_STOPTHERODEO_CONTEXT	"BaseFourWheeledVehicleStopTheRodeoMadnessThink"

ConVar road_feel( "road_feel", "0.1", FCVAR_NOTIFY | FCVAR_REPLICATED );
extern ConVar tf_fastbuild;

BEGIN_DATADESC( CBaseTFFourWheelVehicle )

	DEFINE_EMBEDDED( m_VehiclePhysics ),

	DEFINE_INPUTFUNC( FIELD_FLOAT, "Throttle", InputThrottle ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "Steer", InputSteering ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "Action", InputAction ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOn",	InputTurnOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOff", InputTurnOff ),

END_DATADESC()


// Used for debugging to make vehicle deploying go really fast.
ConVar tf_fastdeploy( "tf_fastdeploy", "0", FCVAR_CHEAT );


IMPLEMENT_SERVERCLASS_ST(CBaseTFFourWheelVehicle, DT_BaseTFFourWheelVehicle)
	SendPropFloat( SENDINFO( m_flDeployFinishTime ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_eDeployMode ), NUM_VEHICLE_DEPLOYMODE_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bBoostUpgrade ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nBoostTimeLeft ), 8, SPROP_UNSIGNED ),
END_SEND_TABLE()

ConVar	fourwheelvehicle_hit_damage( "fourwheelvehicle_hit_damage","40", FCVAR_NONE, "Four-wheel vehicle hit damage" );
ConVar  fourwheelvehicle_hit_damage_boostmod( "fourwheelvehicle_hit_damage_boostmod", "1.5", FCVAR_NONE, "Four-wheel vehicle boosted hit damage modifier" );
ConVar	fourwheelvehicle_hit_mindamagevel( "fourwheelvehicle_hit_mindamagevel","100", FCVAR_NONE, "Four-wheel vehciel hit velocity for min damage" );
ConVar	fourwheelvehicle_hit_maxdamagevel( "fourwheelvehicle_hit_maxdamagevel","230", FCVAR_NONE, "Four-wheel vehicle hit velocity for max damage" );
ConVar  fourwheelvehicle_impact_time( "fourwheelvehicle_impact_time", "1.0", FCVAR_NONE, "Four-wheel vehicle impact wait time." );
ConVar	fourwheelvehicle_hit_damage_player( "fourwheelvehicle_hit_damage_player", "30.0f", FCVAR_NONE, "Four-wheel vehicle hit player damage." );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
#pragma warning( disable: 4355 )
CBaseTFFourWheelVehicle::CBaseTFFourWheelVehicle() : m_VehiclePhysics(this)
{
	m_flDeployFinishTime = -1;
}
#pragma warning( default: 4355 )

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CBaseTFFourWheelVehicle::~CBaseTFFourWheelVehicle ()
{
}

//-----------------------------------------------------------------------------
// Purpose: Put a vehicle in a deploy state. Turn off the engine and run a
//          deploy animation.
//-----------------------------------------------------------------------------
bool CBaseTFFourWheelVehicle::Deploy( void )
{
	// Make sure we're allowed to deploy here
	if ( !IsReadyToDrive() )
		return false;

	// Check to see if we are already in a deploy mode.
	if ( m_eDeployMode != VEHICLE_MODE_NORMAL )
		return false;

	// Disable the vehicle's motion.
	DisableMotion();

	// Save pre-deploy activity.
	m_PreDeployActivity = GetActivity();

	// Set the deploying activity - ACT_DEPLOY
	SetActivity( ACT_DEPLOY );

	// Get the deployment time.
	float flDeployTime = SequenceDuration();
	if ( tf_fastdeploy.GetBool() )
		flDeployTime = 1;

	m_flDeployFinishTime = gpGlobals->curtime + flDeployTime;

	SetContextThink( BaseFourWheeledVehicleDeployThink, gpGlobals->curtime + flDeployTime, 
		             BASEFOURWHEELEDVEHICLE_DEPLOYTHINK_CONTEXT );

	// Set the deploy mode.
	m_eDeployMode = VEHICLE_MODE_DEPLOYING;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Release a vehicle from a deployed state.  Run a de-coupling 
//          animation and turn on the vehicle.
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::UnDeploy( void )
{
	// Make sure we are deployed.
	if ( !IsDeployed() )
		return;

	// Enable motion and turn on the vehicle.
	EnableMotion();

	// Set the undeploying activity - ACT_UNDEPLOY
	SetActivity( ACT_UNDEPLOY );

	// Get the undeployment time.
	float flUnDeployTime = SequenceDuration();
	if ( tf_fastdeploy.GetBool() )
		flUnDeployTime = 1;

	m_flDeployFinishTime = gpGlobals->curtime + flUnDeployTime;

	SetContextThink( BaseFourWheeledVehicleDeployThink, gpGlobals->curtime + flUnDeployTime, 
		             BASEFOURWHEELEDVEHICLE_DEPLOYTHINK_CONTEXT );

	// Set the deploy mode.
	m_eDeployMode = VEHICLE_MODE_UNDEPLOYING;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::CancelDeploy( void )
{
	// Check for the deploying state.
	if ( !IsDeploying() )
		return;

	// Re-enable the motion.
	EnableMotion();

	// Reset the activity to the previous activity.
	SetActivity( m_PreDeployActivity );
	
	// Reset the deploy mode.
	m_eDeployMode = VEHICLE_MODE_NORMAL;

	m_flDeployFinishTime = -1;

	// Turn off the context think.
	SetContextThink( NULL, 0, BASEFOURWHEELEDVEHICLE_DEPLOYTHINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::BaseFourWheeledVehicleDeployThink( void )
{
	// Called from deploy.
	if ( IsDeploying() )
	{
		OnFinishedDeploy();
		m_flDeployFinishTime = -1;
	}
	// Called from undeploy.
	else if ( IsUndeploying() )
	{
		OnFinishedUnDeploy();
		m_flDeployFinishTime = -1;
	}

	// Turn off the context think.
	SetContextThink( NULL, 0, BASEFOURWHEELEDVEHICLE_DEPLOYTHINK_CONTEXT );
}

void CBaseTFFourWheelVehicle::BaseFourWheeledVehicleStopTheRodeoMadnessThink( void )
{
	// HACK HACK:  See note above at FinishBuilding call
	// This resets the handbrake, so the newly placed object doesn't roll down any hills.
	m_VehiclePhysics.ResetControls();

	// Turn off the context think.
	SetContextThink( NULL, 0, BASEFOURWHEELEDVEHICLE_STOPTHERODEO_CONTEXT );

	// Start our base think
	SetContextThink( BaseFourWheeledVehicleThink, gpGlobals->curtime + 0.1, BASEFOURWHEELEDVEHICLE_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::OnFinishedDeploy( void )
{
	SetActivity( ACT_DEPLOY_IDLE );
	m_eDeployMode = VEHICLE_MODE_DEPLOYED;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::OnFinishedUnDeploy( void )
{
	m_eDeployMode = VEHICLE_MODE_NORMAL;
}

//-----------------------------------------------------------------------------
// Purpose: Allow the vehicle to move.
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::EnableMotion( void )
{
	// Enable vehicle chasis motion.
	IPhysicsObject *pVehicleObject = VPhysicsGetObject();
	if ( pVehicleObject )
	{
		pVehicleObject->EnableMotion( true );
	}

	// Enable motion on the tires.
	m_VehiclePhysics.EnableMotion();
}

//-----------------------------------------------------------------------------
// Purpose: Dis-allow the vehicle to move.
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::DisableMotion( void )
{
	// Disable vehicle chasis motion.
	IPhysicsObject *pVehicleObject = VPhysicsGetObject();
	if ( pVehicleObject )
	{
		pVehicleObject->EnableMotion( false );
	}

	// Disable motion on the tires.
	m_VehiclePhysics.DisableMotion();
}

//-----------------------------------------------------------------------------
// Precache
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "BaseTFFourWheelVehicle.EMP" );
	PrecacheScriptSound( "BaseTFFourWheelVehicle.RamSound" );
}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::Spawn( )
{
	SetModel( STRING( GetModelName() ) );
//	CFourWheelServerVehicle *pServerVehicle = dynamic_cast<CFourWheelServerVehicle*>(GetServerVehicle());
//	m_VehiclePhysics.SetOuter( this, pServerVehicle );
	m_VehiclePhysics.Spawn();
	BaseClass::Spawn();
	// The base class spawn sets a default collision group, so this needs to
	// be called post.
	SetCollisionGroup( COLLISION_GROUP_VEHICLE );

	m_eDeployMode = VEHICLE_MODE_NORMAL;

	SetBoostUpgrade( false );

	m_flNextHitTime = 0.0f;
}	

//-----------------------------------------------------------------------------
// Teleport
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::Teleport( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity )
{
	// We basically just have to make sure the wheels are in the right place
	// after teleportation occurs
	matrix3x4_t startMatrixInv;
	MatrixInvert( EntityToWorldTransform(), startMatrixInv );

	BaseClass::Teleport( newPosition, newAngles, newVelocity );

	// Teleport the vehicle physics from the starting position to the ending one
	matrix3x4_t relativeTransform;
	ConcatTransforms( EntityToWorldTransform(), startMatrixInv, relativeTransform );
	m_VehiclePhysics.Teleport( relativeTransform );
}


//-----------------------------------------------------------------------------
// Debugging methods
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::DrawDebugGeometryOverlays()
{
	if (m_debugOverlays & OVERLAY_BBOX_BIT) 
	{	
		m_VehiclePhysics.DrawDebugGeometryOverlays();
	}
	BaseClass::DrawDebugGeometryOverlays();
}

int CBaseTFFourWheelVehicle::DrawDebugTextOverlays()
{
	int nOffset = BaseClass::DrawDebugTextOverlays();
	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		nOffset = m_VehiclePhysics.DrawDebugTextOverlays( nOffset );
	}
	return nOffset;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFFourWheelVehicle::StartBuilding( CBaseEntity *pPlayer )
{
	if (!BaseClass::StartBuilding(pPlayer))
		return false;

	// Until we're finished building, turn off vphysics-based motion
	SetSolid( SOLID_VPHYSICS );
	VPhysicsInitStatic();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	char pScriptName[128];
	Q_snprintf( pScriptName, sizeof( pScriptName ), "scripts/vehicles/%s.txt", GetClassname() );
	m_VehiclePhysics.Initialize( pScriptName, true );

	// HACK HACK:  This is a hack to avoid physics spazzing out on a newly created vehicle with the handbrake
	//  set.  We create and activate it, but then release the handbrake for a single Think function call and 
	//  then zero out the controls right then.  This seems to stabilize something in the physics simulator.
	m_VehiclePhysics.ReleaseHandbrake();
	SetContextThink( BaseFourWheeledVehicleStopTheRodeoMadnessThink, gpGlobals->curtime, BASEFOURWHEELEDVEHICLE_STOPTHERODEO_CONTEXT );
	
	ResetDeteriorationTime();
}

//-----------------------------------------------------------------------------
// Input methods
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::InputThrottle( inputdata_t &inputdata )
{
	m_VehiclePhysics.SetThrottle( inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::InputSteering( inputdata_t &inputdata )
{
	m_VehiclePhysics.SetSteering( inputdata.value.Float(), 2*gpGlobals->frametime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::InputAction( inputdata_t &inputdata )
{
	m_VehiclePhysics.SetAction( inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::InputTurnOn( inputdata_t &inputdata )
{
	if (!m_VehiclePhysics.IsOn())
	{
		SetContextThink( BaseFourWheeledVehicleThink, gpGlobals->curtime + 0.1, BASEFOURWHEELEDVEHICLE_THINK_CONTEXT );
		m_VehiclePhysics.TurnOn( );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::InputTurnOff( inputdata_t &inputdata )
{
	if ( m_VehiclePhysics.IsOn() )
	{
		m_VehiclePhysics.TurnOff( );
	}
}

//-----------------------------------------------------------------------------
// Input methods
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::BaseFourWheeledVehicleThink()
{
	if (m_VehiclePhysics.Think())
	{
		SetNextThink( gpGlobals->curtime, BASEFOURWHEELEDVEHICLE_THINK_CONTEXT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	// must be a wheel
	if (!m_VehiclePhysics.VPhysicsUpdate(pPhysics))
		return;

	BaseClass::VPhysicsUpdate( pPhysics );
}


//-----------------------------------------------------------------------------
// Methods related to getting in and out of the vehicle 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::SetPassenger( int nRole, CBasePlayer *pEnt )
{
	if ( nRole == VEHICLE_ROLE_DRIVER )
	{
		if (pEnt)
		{
			PlayerControlInit( ToBasePlayer(pEnt) );
		}
		else
		{
			PlayerControlShutdown( );
		}
	}
	BaseClass::SetPassenger( nRole, pEnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::PlayerControlInit( CBasePlayer *pPlayer )
{
	// Blat out the view offset
	m_savedViewOffset = pPlayer->GetViewOffset();
	pPlayer->SetViewOffset( vec3_origin );

	m_playerOn.FireOutput( pPlayer, this, 0 );
	InputTurnOn( inputdata_t() );

	// Release the handbrake.
	if ( !IsDeployed() )
	{
		m_VehiclePhysics.ReleaseHandbrake();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::ResetUseKey( CBasePlayer *pPlayer )
{
	pPlayer->m_afButtonPressed &= ~IN_USE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::PlayerControlShutdown()
{
	CBasePlayer *pPlayer = GetDriverPlayer();
	if ( !pPlayer )
		return;

	ResetUseKey( pPlayer );
	pPlayer->SetViewOffset( m_savedViewOffset );

	m_playerOff.FireOutput( pPlayer, this, 0 );
	// clear out the fire buttons
	m_attackaxis.Set( 0, pPlayer, this );
	m_attack2axis.Set( 0, pPlayer, this );

	InputTurnOff( inputdata_t() );
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier )
{
	switch( iPowerup )
	{
	case POWERUP_EMP:
		m_VehiclePhysics.SetMaxThrottle( 0.1 );
		break;

	default:
		break;
	}

	BaseClass::PowerupStart( iPowerup, flAmount, pAttacker, pDamageModifier );
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::PowerupEnd( int iPowerup )
{
	switch ( iPowerup )
	{
	case POWERUP_EMP:
		m_VehiclePhysics.SetMaxThrottle( 1.0 );
		break;

	default:
		break;
	}

	BaseClass::PowerupEnd( iPowerup );
}

//-----------------------------------------------------------------------------
// Methods related to actually driving the vehicle
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::DriveVehicle( CBasePlayer *pPlayer, CUserCmd *ucmd )
{
	// Lose control when the player dies
	if ( pPlayer->IsAlive() == false )
		return;

	// Only the driver gets to drive.
	int nRole = GetPassengerRole( pPlayer );
	if ( nRole != VEHICLE_ROLE_DRIVER )
		return;

	// No driving in the mothership, kids
	if ( !tf_fastbuild.GetInt() && !IsReadyToDrive() )
	{
		m_VehiclePhysics.SetHandbrake( true );
		m_VehiclePhysics.SetThrottle( 0 );
		m_VehiclePhysics.SetSteering( 0, 0 );
		m_attackaxis.Set( 0, pPlayer, this );
		m_attack2axis.Set( 0, pPlayer, this );
		return;
	}

	// Update the boost time.
	m_nBoostTimeLeft = m_VehiclePhysics.BoostTimeLeft();

	// If the vehicle's emped, it can't drive
	if ( HasPowerup( POWERUP_EMP ) )
	{
		// Play sounds if they're trying to drive
		if ( ucmd->buttons & (IN_MOVELEFT | IN_MOVERIGHT | IN_FORWARD | IN_BACK) )
		{
			if ( m_flNextEmpSound < gpGlobals->curtime )
			{
				EmitSound( "BaseTFFourWheelVehicle.EMP" );
				m_flNextEmpSound = gpGlobals->curtime + 2.0;
			}
		}
	}

	ResetDeteriorationTime();

	m_VehiclePhysics.UpdateDriverControls( ucmd, TICK_INTERVAL );

	float attack = 0, attack2 = 0;

	if ( pPlayer->m_afButtonPressed & IN_ATTACK )
	{
		m_pressedAttack.FireOutput( pPlayer, this, 0 );
	}
	if ( pPlayer->m_afButtonPressed & IN_ATTACK2 )
	{
		m_pressedAttack2.FireOutput( pPlayer, this, 0 );
	}

	if ( ucmd->buttons & IN_ATTACK )
	{
		attack = 1;
	}
	if ( ucmd->buttons & IN_ATTACK2 )
	{
		attack2 = 1;
	}

	m_attackaxis.Set( attack, pPlayer, this );
	m_attack2axis.Set( attack2, pPlayer, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::SetupMove( CBasePlayer *pPlayer, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	BaseClass::SetupMove( pPlayer, ucmd, pHelper, move );

	if ( IsDeployed() )
		return;

	DriveVehicle( pPlayer, ucmd );
	m_nMovementRole = GetPassengerRole( pPlayer );
	Assert( m_nMovementRole >= 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::SetBoostUpgrade( bool bBoostUpgrade ) 
{ 
	m_bBoostUpgrade = bBoostUpgrade;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFFourWheelVehicle::IsBoostable( void )
{
	return m_bBoostUpgrade;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::StartBoost( void )
{
	if ( IsBoostable() )
	{
		m_VehiclePhysics.SetBoost( 1.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFFourWheelVehicle::IsBoosting( void )
{
	return m_VehiclePhysics.IsBoosting();
}

//-----------------------------------------------------------------------------
// Purpose: Vehicle damage!
//-----------------------------------------------------------------------------
void CBaseTFFourWheelVehicle::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	BaseClass::VPhysicsCollision( index, pEvent );

	int otherIndex = !index;
	CBaseEntity *pEntity = pEvent->pEntities[otherIndex];

    // We only damage objects...
	// And only if we're travelling fast enough...
	Assert( pEntity );
	if ( !pEntity->IsSolid() )
		return;

	// Ignore shields..
	if ( pEntity->GetCollisionGroup() == TFCOLLISION_GROUP_SHIELD )
		return;

	// Ignore anything that's not an object
	if ( pEntity->Classify() != CLASS_MILITARY && !pEntity->IsPlayer() )
		return;

	// Ignore teammates
	if ( InSameTeam( pEntity ))
		return;

	// Ignore invulnerable stuff
	if ( pEntity->m_takedamage == DAMAGE_NO )
		return;

	// See if we can damage again? (Time-based)
	if ( m_flNextHitTime > gpGlobals->curtime )
		return;

	// Do damage based on velocity.
	Vector vecVelocity = pEvent->preVelocity[index];

	CTakeDamageInfo info;
	info.SetInflictor( this );
	info.SetAttacker( GetDriverPlayer() );
	info.SetDamageType( DMG_CLUB );

	float flMaxDamage = fourwheelvehicle_hit_damage.GetFloat();
	float flMaxDamageVel = fourwheelvehicle_hit_maxdamagevel.GetFloat();
	float flMinDamageVel = fourwheelvehicle_hit_mindamagevel.GetFloat();

	float flVel = vecVelocity.Length();
	if ( flVel < flMinDamageVel )
		return;

	EmitSound( "BaseTFFourWheelVehicle.RamSound" );

	float flDamageFactor = flMaxDamage;
	// Special damage for players.
	if ( pEntity->IsPlayer() )
	{
		flDamageFactor = fourwheelvehicle_hit_damage_player.GetFloat();

		if ( IsBoosting() )
		{
			flDamageFactor *= 4.0f;
		}

		// Knock the player up into the air
		float flForceScale = (flVel*0.5) * 75 * 4;
		Vector vecForce = vecVelocity;
		VectorNormalize( vecForce );
		vecForce.z += 0.7;
		vecForce *= flForceScale;
		info.SetDamageForce( vecForce );
	}
	// Damage to objects.
	else
	{
		if ( IsBoosting() )
		{
			flDamageFactor *= fourwheelvehicle_hit_damage_boostmod.GetFloat();
		}
		
		if ( ( flMaxDamageVel > flMinDamageVel ) && ( flVel < flMaxDamageVel ) )
		{
			// Use less damage when we're not moving fast enough
			float flVelocityFactor = ( flVel - flMinDamageVel ) / ( flMaxDamageVel - flMinDamageVel );
			flVelocityFactor *= flVelocityFactor;
			flDamageFactor *= flVelocityFactor;
		}
	}

	info.SetDamage( flDamageFactor );
	Vector damagePos;
	pEvent->pInternalData->GetContactPoint( damagePos );
	Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass();
	info.SetDamageForce( damageForce );
	info.SetDamagePosition( damagePos );
	PhysCallbackDamage( pEntity, info, *pEvent, index );

	// Set next time hit time
	m_flNextHitTime = gpGlobals->curtime + fourwheelvehicle_impact_time.GetFloat();
}
