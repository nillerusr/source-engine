//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Teleport Station vehicle
//
//=============================================================================//

#include "cbase.h"
#include "tf_vehicle_teleport_station.h"
#include "engine/IEngineSound.h"
#include "VGuiScreen.h"
#include "ammodef.h"
#include "in_buttons.h"
#include "ndebugoverlay.h"
#include "IEffects.h"
#include "info_act.h"
#include "tf_obj_mcv_selection_panel.h"
#include "info_vehicle_bay.h"

#define TELEPORT_STATION_MINS				Vector(-30, -50, -10)
#define TELEPORT_STATION_MAXS				Vector( 30,  50, 55)
#define TELEPORT_STATION_MODEL				"models/objects/vehicle_teleport_station.mdl"
#define TELEPORT_STATION_ZONE_HEIGHT		200
#define TELEPORT_STATION_THINK_CONTEXT		"TeleportThink"

BEGIN_DATADESC( CVehicleTeleportStation )

	DEFINE_THINKFUNC( PostTeleportThink ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CVehicleTeleportStation, DT_VehicleTeleportStation )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( vehicle_teleport_station, CVehicleTeleportStation );
PRECACHE_REGISTER( vehicle_teleport_station );

// CVars
ConVar	vehicle_teleport_station_health( "vehicle_teleport_station_health","600", FCVAR_NONE, "Siege tower health" );


CUtlVector<CVehicleTeleportStation*> g_AllTeleportStations;
CUtlVector<CVehicleTeleportStation*> CVehicleTeleportStation::s_DeployedTeleportStations;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVehicleTeleportStation::CVehicleTeleportStation()
{
	g_AllTeleportStations.AddToTail( this );
	
	//FIXME: we should be able to use clientside animation on this vehicle, but prediction is messing it
	//up right now.
	//UseClientSideAnimation();
}


CVehicleTeleportStation::~CVehicleTeleportStation()
{
	g_AllTeleportStations.FindAndRemove( this );
	s_DeployedTeleportStations.FindAndRemove( this );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleTeleportStation::Spawn()
{
	SetModel( TELEPORT_STATION_MODEL );
	
	// This size is used for placement only...
	UTIL_SetSize( this, TELEPORT_STATION_MINS, TELEPORT_STATION_MAXS );
	m_takedamage = DAMAGE_YES;
	m_iHealth = vehicle_teleport_station_health.GetInt();

	SetType( OBJ_VEHICLE_TELEPORT_STATION );
	SetMaxPassengerCount( 4 );
	SetBodygroup( 1, true );

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleTeleportStation::Precache()
{
	PrecacheModel( TELEPORT_STATION_MODEL );

	PrecacheVGuiScreen( "screen_vehicle_bay" );

	PrecacheModel( "sprites/redglow1.vmt" );
	
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleTeleportStation::UpdateOnRemove( void )
{
	RemoveCornerSprites();

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CVehicleTeleportStation::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_vehicle_bay";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleTeleportStation::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "vgui_screen_vehicle_bay";
}

//-----------------------------------------------------------------------------
// Purpose: Screens aren't active until deployed
//-----------------------------------------------------------------------------
void CVehicleTeleportStation::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	SetControlPanelsActive( false );
}

//-----------------------------------------------------------------------------
// Here's where we deal with weapons, ladders, etc.
//-----------------------------------------------------------------------------
void CVehicleTeleportStation::OnItemPostFrame( CBaseTFPlayer *pDriver )
{
	// I can't do anything if I'm not active
	if ( !ShouldBeActive() )
		return;

	if ( GetPassengerRole( pDriver ) != VEHICLE_ROLE_DRIVER )
		return;

	if ( !IsDeployed() && ( pDriver->m_afButtonPressed & IN_ATTACK ) )
	{
		if ( ValidDeployPosition() )
		{
			Deploy();
		}
	}
	else if ( IsDeployed() && ( pDriver->m_afButtonPressed & IN_ATTACK ) )
	{
		UnDeploy();

		SetControlPanelsActive( false );
		SetBodygroup( 1, true );
		RemoveCornerSprites();
		SetContextThink( NULL, 0, TELEPORT_STATION_THINK_CONTEXT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finished our deploy
//-----------------------------------------------------------------------------
void CVehicleTeleportStation::OnFinishedDeploy( void )
{
	BaseClass::OnFinishedDeploy();

	ValidDeployPosition();
	SetBodygroup( 1, false );

	// Find the teleport in the mothership
	CFuncMassTeleport *pTeleporter = NULL;
	while ( (pTeleporter = (CFuncMassTeleport*)gEntList.FindEntityByClassname( pTeleporter, "func_mass_teleport" )) != NULL )
	{
		if ( pTeleporter->IsMCVTeleport() )
		{
			m_hTeleportExit = pTeleporter;
			m_hTeleportExit->AddMCV( this );
			break;
		}
	}

	// Put some flares down to mark the teleport zone
	Vector vecOrigin;
	QAngle vecAngles;
	for ( int i = 0; i < 4; i++ )
	{
		char buf[64];
		Q_snprintf( buf, sizeof( buf ), "teleport_corner%d", i+1 );
		if ( GetAttachment( buf, vecOrigin, vecAngles ) )
		{
			CheckBuildPoint( vecOrigin + Vector(0,0,64), Vector(0,0,128), &vecOrigin );
			m_hTeleportCornerSprites[i] = CSprite::SpriteCreate( "sprites/redglow1.vmt", vecOrigin, false );
			m_hTeleportCornerSprites[i]->SetTransparency( kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation );
			m_hTeleportCornerSprites[i]->SetScale( 0.1 );
		}
	}
	// Get the ray point
	if ( GetAttachment( "muzzle", vecOrigin, vecAngles ) )
	{
		m_hTeleportCornerSprites[4] = CSprite::SpriteCreate( "sprites/redglow1.vmt", vecOrigin, false );
		m_hTeleportCornerSprites[4]->SetTransparency( kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation );
		m_hTeleportCornerSprites[4]->SetScale( 0.1 );
	}

	SetContextThink( PostTeleportThink, gpGlobals->curtime + 0.1, TELEPORT_STATION_THINK_CONTEXT );

	// Add ourselves to the list of deployed MCVs.
	s_DeployedTeleportStations.AddToTail( this );

	// Set our vehicle bay screen's buildpoint
	for ( i = m_hScreens.Count(); --i >= 0; )
	{
		if (m_hScreens[i].Get())
		{
			CVGuiScreenVehicleBay *pScreen = dynamic_cast<CVGuiScreenVehicleBay*>( m_hScreens[0].Get() );
			if ( pScreen )
			{
				Vector vecOrigin;
				QAngle vecAngles;
				if ( GetAttachment( "vehiclebay", vecOrigin, vecAngles ) )
				{
					pScreen->SetBuildPoint( vecOrigin, vecAngles );
				}
			}
		}
	}

	SetControlPanelsActive( true );

	SignalChangeInMCVSelectionPanels();
}


void CVehicleTeleportStation::OnFinishedUnDeploy( void )
{
	BaseClass::OnFinishedUnDeploy();

	s_DeployedTeleportStations.FindAndRemove( this );
	SignalChangeInMCVSelectionPanels();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleTeleportStation::RemoveCornerSprites( void )
{
	// Remove all the corner sprites
	for ( int i = 0; i < 5; i++ )
	{
		if ( m_hTeleportCornerSprites[i] )
		{
			UTIL_Remove( m_hTeleportCornerSprites[i] );
		}
	}

	// Tell the exit we're done
	if ( m_hTeleportExit )
	{
		m_hTeleportExit->RemoveMCV( this );
		m_hTeleportExit = NULL;
	}
}


int CVehicleTeleportStation::GetNumDeployedTeleportStations()
{
	return s_DeployedTeleportStations.Count();
}


CVehicleTeleportStation* CVehicleTeleportStation::GetDeployedTeleportStation( int i )
{
	return s_DeployedTeleportStations[i];
}


//-----------------------------------------------------------------------------
// Purpose: Return true if the truck's in a valid position
//-----------------------------------------------------------------------------
bool CVehicleTeleportStation::ValidDeployPosition( void )
{
	// Setup for teleporting
	QAngle vecAngles;
	if ( !GetAttachment( "teleport_corner1", m_vecTeleporterMins, vecAngles ) || !GetAttachment( "teleport_corner4", m_vecTeleporterMaxs, vecAngles ) )
		return false;

	m_vecTeleporterMaxs.z += TELEPORT_STATION_ZONE_HEIGHT;

	// Make sure we've got the right mins & maxs
	for ( int i = 0; i < 3; i++ )
	{
		if ( m_vecTeleporterMaxs[i] < m_vecTeleporterMins[i] )
		{
			float flVal = m_vecTeleporterMaxs[i];
			m_vecTeleporterMaxs[i] = m_vecTeleporterMins[i];
			m_vecTeleporterMins[i] = flVal;
		}
	}

	Vector vecDelta = (m_vecTeleporterMaxs - m_vecTeleporterMins) * 0.5;
	Vector vecEnd = (m_vecTeleporterMaxs + m_vecTeleporterMins) * 0.5; 
	Vector vecSrc = vecEnd + Vector(0,0,TELEPORT_STATION_ZONE_HEIGHT * 0.5);

	// Make sure it's clear
	// Take the hull, start it high, and try and trace down
	trace_t tr;
	UTIL_TraceHull( vecSrc, vecEnd, -vecDelta, vecDelta, MASK_SOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );

	//NDebugOverlay::Box( m_vecTeleporterMins, Vector(-2,-2,-2), Vector(2,2,2), 0,255,0,8, 0.1 );
	//NDebugOverlay::Box( m_vecTeleporterMaxs, Vector(-2,-2,-2), Vector(2,2,2), 0,255,0,8, 0.1 );
	//NDebugOverlay::Box( vecSrc, -vecDelta, vecDelta, 255,0,0,8, 0.1 );
	//NDebugOverlay::Box( vecEnd, -vecDelta, vecDelta, 0,0,255,8, 0.1 );
	//NDebugOverlay::Line( m_vecTeleporterMins, m_vecTeleporterMaxs, 255,255,255, 1, 0.1 );

	// Make sure it's clear
	if ( tr.startsolid )
		return false;

	// Get a list of nearby entities
	CBaseEntity *pListOfNearbyEntities[100];
	int iNumberOfNearbyEntities = UTIL_EntitiesInBox( pListOfNearbyEntities, 100, m_vecTeleporterMins, m_vecTeleporterMaxs, MASK_ALL );
	for ( i = 0; i < iNumberOfNearbyEntities; i++ )
	{
		CBaseEntity *pEntity = pListOfNearbyEntities[i];
		if ( pEntity->IsSolid( ) )
		{
			if ( pEntity == this )
				continue;
			if ( pEntity->GetCollisionGroup() == TFCOLLISION_GROUP_SHIELD )
				continue;
			if ( pEntity->IsPlayer() )
				continue;

			//NDebugOverlay::EntityBounds( pEntity, 0,255,0,8, 0.1 );
			return false;
		}
	}

	m_vecTeleporterCenter = vecEnd;
	m_vecTeleporterCenter.z = m_vecTeleporterMins.z;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleTeleportStation::DoTeleport( void )
{
	ResetDeteriorationTime();

	if ( m_hTeleportExit )
	{
		// Teleport all players attached to me that are within my bbox.
		EHANDLE hMCV;
		hMCV = this;
		Vector vecAbsMins, vecAbsMaxs;
		m_hTeleportExit->CollisionProp()->WorldSpaceAABB( &vecAbsMins, &vecAbsMaxs );
		m_hTeleportExit->DoTeleport( GetTFTeam(), vecAbsMins, vecAbsMaxs, m_vecTeleporterCenter, true, true, hMCV );
	}

	// Shrink the corner sprites
	for ( int i = 0; i < 5; i++ )
	{
		if ( m_hTeleportCornerSprites[i] )
		{
			m_hTeleportCornerSprites[i]->SetScale( 0.1 );
		}
	}

	SetContextThink( PostTeleportThink, gpGlobals->curtime + 0.1, TELEPORT_STATION_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleTeleportStation::PostTeleportThink( void )
{
	float flTime = 10.0;
	if ( g_hCurrentAct )
	{
		flTime = g_hCurrentAct->GetMCVTimer();
	}
	
	// Start the corner sprites up
	for ( int i = 0; i < 4; i++ )
	{
		if ( m_hTeleportCornerSprites[i] )
		{
			m_hTeleportCornerSprites[i]->SetScale( 0.75, flTime );
		}
	}
	m_hTeleportCornerSprites[4]->SetScale( 2, flTime );
}


// Returns INVALID_MCV_ID if there are no deployed MCVs.
CVehicleTeleportStation* CVehicleTeleportStation::GetFirstDeployedMCV( int iTeam )
{
	for ( int i=0; i < s_DeployedTeleportStations.Count(); i++ )
	{
		CVehicleTeleportStation *pStation = s_DeployedTeleportStations[i];
		
		if ( pStation->GetTeamNumber() == iTeam )
			return pStation;
	}
	return NULL;
}

