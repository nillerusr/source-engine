//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Upgrade that boosts vehicle speeds for short periods of time.
//
//=============================================================================//

#include "cbase.h"
#include "tf_player.h"
#include "tf_obj.h"
#include "tf_obj_vehicleboost.h"
#include "tf_basefourwheelvehicle.h"

#define VEHICLE_BOOST_MINS				Vector( -10, -10, 0 )
#define VEHICLE_BOOST_MAXS				Vector(  10,  10, 10 )
#define VEHICLE_BOOST_MODEL				"models/objects/obj_vehicle_boost.mdl"

BEGIN_DATADESC( CObjectVehicleBoost )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CObjectVehicleBoost, DT_ObjectVehicleBoost )
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( obj_vehicle_boost, CObjectVehicleBoost );
PRECACHE_REGISTER( obj_vehicle_boost );

ConVar	obj_vehicle_boost_health( "obj_vehicle_boost_health","100", FCVAR_NONE, "Vehicle Boost Health" );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CObjectVehicleBoost::CObjectVehicleBoost()
{
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectVehicleBoost::Spawn( void )
{
	Precache();
	SetModel( VEHICLE_BOOST_MODEL );
	SetCollisionGroup( TFCOLLISION_GROUP_COMBATOBJECT );

	UTIL_SetSize(this, VEHICLE_BOOST_MINS, VEHICLE_BOOST_MAXS );
	m_takedamage = DAMAGE_YES;
	m_iHealth = obj_vehicle_boost_health.GetInt();

	SetType( OBJ_VEHICLE_BOOST );
	m_fObjectFlags |= OF_SUPPRESS_NOTIFY_UNDER_ATTACK | OF_SUPPRESS_TECH_ANALYZER | 
		              OF_DONT_AUTO_REPAIR | OF_MUST_BE_BUILT_ON_ATTACHMENT;

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectVehicleBoost::Precache( void )
{
	PrecacheModel( VEHICLE_BOOST_MODEL );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectVehicleBoost::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	CBaseTFFourWheelVehicle *pVehicle = dynamic_cast<CBaseTFFourWheelVehicle*>( GetParent() );
	if ( pVehicle )
	{
		pVehicle->SetBoostUpgrade( true );
	}
}
