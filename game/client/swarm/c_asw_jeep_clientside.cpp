#include "cbase.h"
#include "c_asw_jeep_clientside.h"
#include "props_shared.h"
#include "c_asw_marine.h"
#include "c_asw_player.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SMOOTHING_FACTOR 0.9

C_ASW_PropJeep_Clientside *C_ASW_PropJeep_Clientside::CreateNew( bool bForce )
{
	C_ASW_PropJeep_Clientside* pVehicle;
	pVehicle = new C_ASW_PropJeep_Clientside();
	//pVehicle->Initialize();

	return pVehicle;
}

C_ASW_PropJeep_Clientside::C_ASW_PropJeep_Clientside() : m_VehiclePhysics( this )
{
	m_bInitialisedPhysics = false;	

	//m_fDeathTime = -1;
	//m_impactEnergyScale = 1.0f;
	m_iHealth = 0;
	bDestroyVehicle = false;
	//m_iPhysicsMode = PHYSICS_MULTIPLAYER_AUTODETECT;

	//s_PhysPropList.AddToTail( this );
	Msg("C_ASW_PropJeep_Clientside created\n");
}

C_ASW_PropJeep_Clientside::~C_ASW_PropJeep_Clientside()
{
	//PhysCleanupFrictionSounds( this );
	//VPhysicsDestroyObject();
	//s_PhysPropList.FindAndRemove( this );	
}

#define VEHICLE_MODEL "models/buggy.mdl"
//#define VEHICLE_MODEL "models/combine_APC.mdl"

bool C_ASW_PropJeep_Clientside::Initialize()
{
	SetModelName( VEHICLE_MODEL );
	PrecacheModel(VEHICLE_MODEL);
	SetModel(VEHICLE_MODEL);
	if ( InitializeAsClientEntity( STRING(GetModelName()), false ) == false )
	{
		return false;
	}

	const model_t *mod = GetModel();
	if ( mod )
	{
		Vector mins, maxs;
		modelinfo->GetModelBounds( mod, mins, maxs );
		SetCollisionBounds( mins, maxs );
	}

	solid_t tmpSolid;

	// Create the object in the physics system

	if ( !PhysModelParseSolid( tmpSolid, this, GetModelIndex() ) )
	{
		DevMsg("C_ASW_PropJeep_Clientside::Initialize: PhysModelParseSolid failed for entity %i.\n", GetModelIndex() );
		return false;
	}
	else
	{
		m_pPhysicsObject = VPhysicsInitNormal( SOLID_VPHYSICS, 0, false, &tmpSolid );
	
		if ( !m_pPhysicsObject )
		{
			// failed to create a physics object
		DevMsg(" C_ASW_PropJeep_Clientside::Initialize: VPhysicsInitNormal() failed for %s.\n", STRING(GetModelName()) );
			return false;
		}
	}
		
	Spawn();
		
	if ( engine->IsInEditMode() )
	{
		// don't spawn in map edit mode
		return false;
	}

	// player can push it away
	SetCollisionGroup( COLLISION_GROUP_VEHICLE );

	UpdatePartitionListEntry();

	CollisionProp()->UpdatePartition();

	//SetBlocksLOS( false ); // this should be a small object

	// Set up shadows; do it here so that objects can change shadowcasting state
	CreateShadow();

	UpdateVisibility();

	SetNextClientThink( gpGlobals->curtime + 0.4f  );

	return true;
}

void C_ASW_PropJeep_Clientside::ClientThink()
{	
	if (m_bInitialisedPhysics)
	{
		// if we have no driver, then destroy the clientside vehicle
		if (bDestroyVehicle)
		{
			SetNextClientThink( CLIENT_THINK_NEVER );			
			// todo: reveal the dummy?
			Release();
			return;
		}
		else
		{
			//ThinkTick();
		}
	}
	else
	{	
		InitPhysics();	
	}

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void C_ASW_PropJeep_Clientside::Spawn()
{
	m_VehiclePhysics.SetOuter( this );

	BaseClass::Spawn();

	//SetModel("models/buggy.mdl");
	
	m_vecSmoothedVelocity.Init();

	BaseClass::Spawn();

	m_takedamage = DAMAGE_NO;
	AddSolidFlags( FSOLID_NOT_STANDABLE );	
}

void C_ASW_PropJeep_Clientside::OnDataChanged( DataUpdateType_t updateType )
{

}

int C_ASW_PropJeep_Clientside::VPhysicsGetObjectList( IPhysicsObject **pList, int listMax )
{
	return m_VehiclePhysics.VPhysicsGetObjectList( pList, listMax );
}

// pass passenger type questions on to the dummy
int C_ASW_PropJeep_Clientside::ASWGetNumPassengers()
{
	if (GetDummy())
		return GetDummy()->ASWGetNumPassengers();

	return 0;
}

C_ASW_Marine* C_ASW_PropJeep_Clientside::ASWGetDriver()
{
	if (GetDummy())
		return GetDummy()->ASWGetDriver();

	return NULL;
}

C_ASW_Marine* C_ASW_PropJeep_Clientside::ASWGetPassenger(int i)
{
	if (GetDummy())
		return GetDummy()->ASWGetPassenger(i);

	return NULL;
}

IASW_Client_Vehicle* C_ASW_PropJeep_Clientside::GetDummy()
{
	C_ASW_Marine* pMarine = ASWGetDriver();
	if (!pMarine)
		return NULL;

	return pMarine->GetASWVehicle();
}

void C_ASW_PropJeep_Clientside::SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	if (!m_bInitialisedPhysics)
		return;
	//Msg("SetupMove cnum=%d [C] forward=%f side=%f", ucmd->command_number, ucmd->forwardmove, ucmd->sidemove);
	DriveVehicle( player, ucmd );
}

void C_ASW_PropJeep_Clientside::ProcessMovement( C_BasePlayer *pPlayer, CMoveData *pMoveData )
{
	if (!m_bInitialisedPhysics)
		return;
	//Msg("[C] PreProcess \tx=%f\t\ty=%f\t\tz=%f\t\tp=%\t\ty=%\t\tr=%\t\tvx=%f\t\tvy=%f\t\tvz=%f\n",
		//GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z,
		//GetAbsAngles().x, GetAbsAngles().y, GetAbsAngles().z,
		//GetAbsVelocity().x, GetAbsVelocity().y, GetAbsVelocity().z);
	// Update the steering angles based on speed.
	UpdateSteeringAngle();

	//ThinkTick();
	//Msg("[C] PostProcess \tx=%f\t\ty=%f\t\tz=%f\t\tp=%\t\ty=%\t\tr=%\t\tvx=%f\t\tvy=%f\t\tvz=%f\n",
		//GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z,
		//GetAbsAngles().x, GetAbsAngles().y, GetAbsAngles().z,
		//GetAbsVelocity().x, GetAbsVelocity().y, GetAbsVelocity().z);
}

#define JEEP_STEERING_SLOW_ANGLE	50.0f
#define JEEP_STEERING_FAST_ANGLE	15.0f

void C_ASW_PropJeep_Clientside::UpdateSteeringAngle( void )
{
	float flMaxSpeed = m_VehiclePhysics.GetMaxSpeed();
	float flSpeed = m_VehiclePhysics.GetSpeed();

	float flRatio = 1.0f - ( flSpeed / flMaxSpeed );
	float flSteeringDegrees = JEEP_STEERING_FAST_ANGLE + ( ( JEEP_STEERING_SLOW_ANGLE - JEEP_STEERING_FAST_ANGLE ) * flRatio );
	flSteeringDegrees = clamp( flSteeringDegrees, JEEP_STEERING_FAST_ANGLE, JEEP_STEERING_SLOW_ANGLE );
	m_VehiclePhysics.SetSteeringDegrees( flSteeringDegrees );
}

void C_ASW_PropJeep_Clientside::ThinkTick()
{
	m_VehiclePhysics.Think( gpGlobals->frametime );

	//SetSimulationTime( gpGlobals->curtime );
		
	//SetAnimatedEveryTick( true );
    
	StudioFrameAdvance();
}

void C_ASW_PropJeep_Clientside::DriveVehicle( C_BasePlayer *pPlayer, CUserCmd *ucmd )
{
	//Lose control when the player dies
	if ( pPlayer->IsAlive() == false )
		return;

	DriveVehicle( TICK_INTERVAL, ucmd, pPlayer->m_afButtonPressed, pPlayer->m_afButtonReleased );
}

void C_ASW_PropJeep_Clientside::DriveVehicle( float flFrameTime, CUserCmd *ucmd, int iButtonsDown, int iButtonsReleased )
{
	int iButtons = ucmd->buttons;
		
	// Only handle the cannon if the vehicle has one
	/*
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
	}*/

	m_VehiclePhysics.UpdateDriverControls( ucmd, flFrameTime );

	m_nSpeed = m_VehiclePhysics.GetSpeed();	//send speed to client
	m_nRPM = clamp( m_VehiclePhysics.GetRPM(), 0, 4095 );
	m_nBoostTimeLeft = m_VehiclePhysics.BoostTimeLeft();
	m_nHasBoost = m_VehiclePhysics.HasBoost();
	m_flThrottle = m_VehiclePhysics.GetThrottle();

	m_nScannerDisabledWeapons = false;		// off for now, change once we have scanners
	m_nScannerDisabledVehicle = false;		// off for now, change once we have scanners

	//
	// Fire the appropriate outputs based on button pressed events.
	//
	// BUGBUG: m_afButtonPressed is broken - check the player.cpp code!!!
	float attack = 0, attack2 = 0;

	/*
	if ( iButtonsDown & IN_ATTACK )
	{
		m_pressedAttack.FireOutput( this, this, 0 );
	}
	if ( iButtonsDown & IN_ATTACK2 )
	{
		m_pressedAttack2.FireOutput( this, this, 0 );
	}*/

	if ( iButtons & IN_ATTACK )
	{
		attack = 1;
	}
	if ( iButtons & IN_ATTACK2 )
	{
		attack2 = 1;
	}

	//m_attackaxis.Set( attack, this, this );
	//m_attack2axis.Set( attack2, this, this );
}

/*
void CPropVehicle::DrawDebugGeometryOverlays()
{
	if (m_debugOverlays & OVERLAY_BBOX_BIT) 
	{	
		m_VehiclePhysics.DrawDebugGeometryOverlays();
	}
	BaseClass::DrawDebugGeometryOverlays();
}
*/


void C_ASW_PropJeep_Clientside::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	if ( IsMarkedForDeletion() )
		return;

	Vector	velocity;
	VPhysicsGetObject()->GetVelocity( &velocity, NULL );

	//Update our smoothed velocity
	m_vecSmoothedVelocity = m_vecSmoothedVelocity * SMOOTHING_FACTOR + velocity * ( 1 - SMOOTHING_FACTOR );

	// must be a wheel
	if (!m_VehiclePhysics.VPhysicsUpdate( pPhysics ))
		return;
	
	BaseClass::VPhysicsUpdate( pPhysics );

	if (!m_bInitialisedPhysics)
		InitPhysics();

	//if (!ASWGetDriver())
		//SetNextClientThink(gpGlobals->curtime);
	ThinkTick();
}

void C_ASW_PropJeep_Clientside::InitPhysics()
{
	if (m_bInitialisedPhysics)
		return;

	m_VehiclePhysics.Spawn();
	if (!m_VehiclePhysics.Initialize( "scripts/vehicles/jeep_test.txt", VEHICLE_TYPE_CAR_WHEELS ))	
			return;

	ASWStartEngine();
	m_bInitialisedPhysics = true;
}

void C_ASW_PropJeep_Clientside::ASWStartEngine( void )
{
	//if ( m_bEngineLocked )
	//{
		//m_VehiclePhysics.SetHandbrake( true );
		//return;
	//}

	m_VehiclePhysics.TurnOn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ASW_PropJeep_Clientside::ASWStopEngine( void )
{
	m_VehiclePhysics.TurnOff();

	bDestroyVehicle = true;
}