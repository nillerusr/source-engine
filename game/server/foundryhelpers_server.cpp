//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "foundryhelpers_server.h"
#include "basetempentity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: This just marshalls certain FoundryHelpers_ calls to the client.
//-----------------------------------------------------------------------------
class CTEFoundryHelpers : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEFoundryHelpers, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTEFoundryHelpers( const char *pName ) :
		CBaseTempEntity( pName )
	{
	}

public:
	CNetworkVar( int, m_iEntity );	// -1 means turn the effect off for all entities.
};

IMPLEMENT_SERVERCLASS_ST( CTEFoundryHelpers, DT_TEFoundryHelpers )
	SendPropInt( SENDINFO(m_iEntity), 32, 0 ),
END_SEND_TABLE()

// Singleton to fire TEMuzzleFlash objects
static CTEFoundryHelpers g_TEFoundryHelpers( "FoundryHelpers" );


void FoundryHelpers_ClearEntityHighlightEffects()
{
	g_TEFoundryHelpers.m_iEntity = -1;
	
	CBroadcastRecipientFilter filter;
	g_TEFoundryHelpers.Create( filter, 0 );
}

void FoundryHelpers_AddEntityHighlightEffect( CBaseEntity *pEnt )
{
	g_TEFoundryHelpers.m_iEntity = pEnt->entindex();
	
	CBroadcastRecipientFilter filter;
	g_TEFoundryHelpers.Create( filter, 0 );
}


CBasePlayer* CheckInFoundryMode()
{
	if ( !serverfoundry )
	{
		Warning( "Not in Foundry mode.\n" );
		return NULL;
	}

	return UTIL_GetListenServerHost();
}


void GetCrosshairOrNamedEntities( const CCommand &args, CUtlVector<CBaseEntity*> &entities )
{
	if ( args.ArgC() < 2 )
	{
		CBasePlayer *pPlayer = UTIL_GetCommandClient();
		trace_t tr;
		Vector forward;
		pPlayer->EyeVectors( &forward );
		UTIL_TraceLine(pPlayer->EyePosition(), pPlayer->EyePosition() + forward * MAX_COORD_RANGE,
			MASK_SHOT_HULL|CONTENTS_GRATE|CONTENTS_DEBRIS, pPlayer, COLLISION_GROUP_NONE, &tr );

		if ( tr.DidHit() && !tr.DidHitWorld() )
		{
			entities.AddToTail( tr.m_pEnt );
		}
	}
	else
	{
		CBaseEntity *pEnt = NULL;
		while ((pEnt = gEntList.FindEntityGeneric( pEnt, args[1] ) ) != NULL)
		{
			entities.AddToTail( pEnt );
		}
	}
}


CON_COMMAND( foundry_update_entity, "Updates the entity's position/angles when in edit mode" )
{
	if ( !CheckInFoundryMode() )
		return;

	CUtlVector<CBaseEntity*> entities;
	GetCrosshairOrNamedEntities( args, entities );

	for ( int i=0; i < entities.Count(); i++ )
	{
		CBaseEntity *pEnt = entities[i];
		serverfoundry->MoveEntityTo( pEnt->GetHammerID(), pEnt->GetAbsOrigin(), pEnt->GetAbsAngles() );
	}
}

CON_COMMAND( foundry_sync_hammer_view, "Move Hammer's 3D view to the same position as the engine's 3D view." )
{
	CBasePlayer *pPlayer = CheckInFoundryMode();
	if ( !pPlayer )
		return;

	Vector vPos = pPlayer->EyePosition();
	QAngle vAngles = pPlayer->pl.v_angle;
	serverfoundry->MoveHammerViewTo( vPos, vAngles );
}

CON_COMMAND( foundry_engine_get_mouse_control, "Give the engine control of the mouse." )
{
	if ( !CheckInFoundryMode() )
		return;

	serverfoundry->EngineGetMouseControl();
}


CON_COMMAND( foundry_engine_release_mouse_control, "Give the control of the mouse back to Hammer." )
{
	if ( !CheckInFoundryMode() )
		return;

	serverfoundry->EngineReleaseMouseControl();
}

CON_COMMAND( foundry_select_entity, "Select the entity under the crosshair or select entities with the specified name." )
{
	CUtlVector<CBaseEntity*> entities;
	GetCrosshairOrNamedEntities( args, entities );

	CUtlVector<int> hammerIDs;
	for ( int i=0; i < entities.Count(); i++ )
	{
		CBaseEntity *pEnt = entities[i];
		hammerIDs.AddToTail( pEnt->GetHammerID() );
	}

	if ( hammerIDs.Count() == 0 )
	{
		CBasePlayer *pPlayer = CheckInFoundryMode();
		if ( !pPlayer )
			return;

		Vector vPos = pPlayer->EyePosition();
		QAngle vAngles = pPlayer->pl.v_angle;
		serverfoundry->SelectionClickInCenterOfView( vPos, vAngles );
	}
	else
	{
		serverfoundry->SelectEntities( hammerIDs.Base(), hammerIDs.Count() );
	}
}

