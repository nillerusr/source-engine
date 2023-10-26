#include "cbase.h"
#include "asw_simple_drone.h"
#include "asw_trace_filter_melee.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_simple_drone, CASW_Simple_Drone );

BEGIN_DATADESC( CASW_Simple_Drone )

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CASW_Simple_Drone, DT_ASW_Simple_Drone)
	
END_SEND_TABLE()

// =========================================
// Creation
// =========================================

CASW_Simple_Drone::CASW_Simple_Drone()
{
}

CASW_Simple_Drone::~CASW_Simple_Drone()
{
}

void CASW_Simple_Drone::Spawn(void)
{
	BaseClass::Spawn();
}

void CASW_Simple_Drone::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "ASW_Drone.Land" );
	PrecacheScriptSound( "ASW_Drone.Pain" );
	PrecacheScriptSound( "ASW_Drone.Alert" );
	PrecacheScriptSound( "ASW_Drone.Death" );
	PrecacheScriptSound( "ASW_Drone.Attack" );
	PrecacheScriptSound( "ASW_Drone.Swipe" );

	PrecacheModel( "models/swarm/DroneGibs/dronepart01.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart20.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart29.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart31.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart32.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart44.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart45.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart47.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart49.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart50.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart53.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart54.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart56.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart57.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart58.mdl" );
	PrecacheModel( "models/swarm/DroneGibs/dronepart59.mdl" );
}

// =========================================
// Anims
// =========================================

void CASW_Simple_Drone::PlayRunningAnimation()
{
	ResetSequence(LookupSequence("run_idle"));
	m_flPlaybackRate = 1.25f;
}

void CASW_Simple_Drone::PlayAttackingAnimation()
{
	ResetSequence(LookupSequence("lunge_attack"));
	m_flPlaybackRate = 1.25f;
}

// =========================================
// Sounds
// =========================================

void CASW_Simple_Drone::PainSound( const CTakeDamageInfo &info )
{
	if (gpGlobals->curtime > m_fNextPainSound)
	{
		EmitSound("ASW_Drone.Pain");
		m_fNextPainSound = gpGlobals->curtime + 0.5f;
	}
}

void CASW_Simple_Drone::AlertSound()
{
	EmitSound("ASW_Drone.Alert");
}

void CASW_Simple_Drone::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound( "ASW_Drone.Death" );
}

void CASW_Simple_Drone::AttackSound()
{
	EmitSound( "ASW_Drone.Attack" );
}