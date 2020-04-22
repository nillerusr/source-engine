//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A moving vehicle that is used as a battering ram
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_vehicle_wagon.h"
#include "engine/IEngineSound.h"
#include "VGuiScreen.h"


#define WAGON_MINS			Vector(-30, -50, -10)
#define WAGON_MAXS			Vector( 30,  50, 55)
#define WAGON_MODEL			"models/objects/vehicle_wagon.mdl"
  

IMPLEMENT_SERVERCLASS_ST(CVehicleWagon, DT_VehicleWagon)
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(vehicle_wagon, CVehicleWagon);
PRECACHE_REGISTER(vehicle_wagon);

// CVars
ConVar	vehicle_wagon_health( "vehicle_wagon_health","800", FCVAR_NONE, "Wagon health" );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVehicleWagon::CVehicleWagon()
{

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleWagon::Precache()
{
	PrecacheModel( WAGON_MODEL );

	PrecacheVGuiScreen( "screen_vehicle_wagon" );
	PrecacheVGuiScreen( "screen_vulnerable_point");
	
	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleWagon::Spawn()
{
	SetModel( WAGON_MODEL );
	
	// This size is used for placement only...
	UTIL_SetSize(this, WAGON_MINS, WAGON_MAXS);
	m_takedamage = DAMAGE_YES;
	m_iHealth = vehicle_wagon_health.GetInt();

	SetType( OBJ_WAGON );
	SetMaxPassengerCount( 4 );

	BaseClass::Spawn();
}


//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CVehicleWagon::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	//pPanelName = "screen_vehicle_wagon";
	pPanelName = "screen_vulnerable_point";
}


