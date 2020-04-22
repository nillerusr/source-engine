//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "EntityOutput.h"
#include "EntityList.h"
#include "info_vehicle_bay.h"
#include "tf_obj.h"
#include "tf_player.h"
#include "info_act.h"

extern ConVar tf_fastbuild;

BEGIN_DATADESC( CInfoVehicleBay )
END_DATADESC()

LINK_ENTITY_TO_CLASS( info_vehicle_bay, CInfoVehicleBay );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoVehicleBay::Spawn( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Vehicle bays have 1 build point
//-----------------------------------------------------------------------------
int CInfoVehicleBay::GetNumBuildPoints( void ) const
{
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified object type can be built on this point
//-----------------------------------------------------------------------------
bool CInfoVehicleBay::CanBuildObjectOnBuildPoint( int iPoint, int iObjectType )
{
	ASSERT( iPoint <= GetNumBuildPoints() );

	// Don't allow building if there's another vehicle in the way


	// Only vehicles can be built here
	return ( iObjectType >= OBJ_BATTERING_RAM );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CInfoVehicleBay::GetBuildPoint( int iPoint, Vector &vecOrigin, QAngle &vecAngles )
{
	ASSERT( iPoint <= GetNumBuildPoints() );

	vecOrigin = GetAbsOrigin();
	vecAngles = GetAbsAngles();
	return true;
}

int CInfoVehicleBay::GetBuildPointAttachmentIndex( int iPoint ) const
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoVehicleBay::SetObjectOnBuildPoint( int iPoint, CBaseObject *pObject )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CInfoVehicleBay::GetNumObjectsOnMe( void )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity	*CInfoVehicleBay::GetFirstObjectOnMe( void )
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObject *CInfoVehicleBay::GetObjectOfTypeOnMe( int iObjectType )
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CInfoVehicleBay::FindObjectOnBuildPoint( CBaseObject *pObject )
{
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoVehicleBay::GetExitPoint( CBaseEntity *pPlayer, int iPoint, Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	Assert(0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoVehicleBay::RemoveAllObjects( void )
{
}

//===================================================================================================================
// Vehicle Bay VGui Screen
//===================================================================================================================
BEGIN_DATADESC( CVGuiScreenVehicleBay )
	// outputs
	DEFINE_OUTPUT( m_OnStartedBuild, "OnStartedBuild" ),
	DEFINE_OUTPUT( m_OnFinishedBuild, "OnFinishedBuild" ),
	DEFINE_OUTPUT( m_OnReadyToBuildAgain, "OnReadyToBuildAgain" ),

	// functions
	DEFINE_FUNCTION( BayThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( vgui_screen_vehicle_bay, CVGuiScreenVehicleBay );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGuiScreenVehicleBay::Activate( void )
{
	BaseClass::Activate();

	// Make sure we have a buildpoint specified
	CBaseEntity *pBuildPoint = gEntList.FindEntityByName( NULL, m_target );
	if ( !pBuildPoint )
	{
		Msg("ERROR: vgui_screen_vehicle_bay with no buildpoint as its target.\n" );
		UTIL_Remove( this );
		return;
	}

	Vector vecOrigin = pBuildPoint->GetAbsOrigin();
	QAngle vecAngles = pBuildPoint->GetAbsAngles();
	SetBuildPoint( vecOrigin, vecAngles );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGuiScreenVehicleBay::SetBuildPoint( Vector &vecOrigin, QAngle &vecAngles )
{
	m_vecBuildPointOrigin = vecOrigin;
	m_vecBuildPointAngles = vecAngles;
	m_bBayIsClear = false;
	
	// Start checking to see when I'm clear again
	SetThink( BayThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGuiScreenVehicleBay::BuildVehicle( CBaseTFPlayer *pPlayer, int iObjectType )
{
	if ( !IsObjectAVehicle(iObjectType) )
		return;
	Assert( m_vecBuildPointOrigin != vec3_origin );

	// Can't build if the game hasn't started
	if ( !tf_fastbuild.GetInt() && CurrentActIsAWaitingAct() )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "Can't build until the game's started.\n" );
		return;
	}

	// Try and spawn the object
	CBaseEntity *pEntity = CreateEntityByName( GetObjectInfo(iObjectType)->m_pClassName );
	if ( !pEntity )
		return;

	if ( !m_bBayIsClear )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "Vehicle bay isn't clear.\n" );
		return;
	}

	pEntity->SetAbsOrigin( m_vecBuildPointOrigin );
	pEntity->SetAbsAngles( m_vecBuildPointAngles );

	CBaseObject *pObject = dynamic_cast<CBaseObject*>(pEntity);
	if ( pObject )
		pObject->AdjustInitialBuildAngles();

	pEntity->Spawn();
	
	// If it's an object, finish setting it up
	if ( !pObject )
		return;

	pObject->StartPlacement( pPlayer );
	pObject->SetVehicleBay( this );

	// StartBuilding will return false if the player couldn't afford the vehicle
	if ( !pObject->StartBuilding( pPlayer ) )
		return;

	// Fire our started-building output
	m_OnStartedBuild.FireOutput( pPlayer, this );

	m_bBayIsClear = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGuiScreenVehicleBay::FinishedBuildVehicle( CBaseObject *pObject )
{
	m_OnFinishedBuild.FireOutput( pObject->GetBuilder(), this );

	// Start checking to see when I'm clear again
	SetThink( BayThink );
	SetNextThink( gpGlobals->curtime + 0.3 );
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if we're clear enough to allow another vehicle to be built here
//-----------------------------------------------------------------------------
void CVGuiScreenVehicleBay::BayThink( void )
{
	// Get a list of entities around our buildpoint
	CBaseEntity *pListOfNearbyEntities[100];
	int iNumberOfNearbyEntities = UTIL_EntitiesInSphere( pListOfNearbyEntities, 100, m_vecBuildPointOrigin, 128, 0 );
	for ( int i = 0; i < iNumberOfNearbyEntities; i++ )
	{
		CBaseEntity *pEntity = pListOfNearbyEntities[i];
		if ( pEntity->IsSolid( ) )
		{
			// Ignore shields..
			if ( pEntity->GetCollisionGroup() == TFCOLLISION_GROUP_SHIELD )
				continue;
			// Ignore func brushes
			if ( pEntity->GetMoveType() == MOVETYPE_PUSH )
				continue;
		
			//NDebugOverlay::EntityBounds( pEntity, 0,255,0,8, 0.1 );

			// Check again soon
			SetNextThink( gpGlobals->curtime + 1 );
			return;
		}
	}

	// We're clear
 	m_bBayIsClear = true;
	SetThink( NULL );

	m_OnReadyToBuildAgain.FireOutput( this, this );
}