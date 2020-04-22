//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Medic's portable power generator
//
//=============================================================================//

#include "cbase.h"
#include "tf_obj_buff_station.h"
#include "tf_player.h"
#include "rope.h"
#include "rope_shared.h"
#include "entitylist.h"
#include "VGuiScreen.h"
#include "engine/IEngineSound.h"
#include "tf_team.h"

//=============================================================================
//
// Console Variables
//
static ConVar obj_buff_station_damage_modifier( "obj_buff_station_damage_modifier", "1.5", 0, "Scales the damage a player does while connected to the buff station." );
static ConVar obj_buff_station_heal_rate( "obj_buff_station_heal_rate", "10" );
static ConVar obj_buff_station_range( "obj_buff_station_range", "300" );
static ConVar obj_buff_station_obj_range( "obj_buff_station_obj_range", "800" );
static ConVar obj_buff_station_health( "obj_buff_station_health","100", FCVAR_NONE, "Buff Station health" );

//-----------------------------------------------------------------------------
// Buff Station defines
//-----------------------------------------------------------------------------
#define BUFF_STATION_MINS							Vector( -30, -30, 0  )
#define BUFF_STATION_MAXS							Vector(  30,  30, 50 )

#define BUFF_STATION_HUMAN_MODEL					"models/objects/human_obj_buffstation.mdl"
#define BUFF_STATION_HUMAN_ASSEMBLING_MODEL			"models/objects/human_obj_buffstation_build.mdl"
#define BUFF_STATION_ALIEN_MODEL					"models/objects/alien_obj_buffstation.mdl"
#define BUFF_STATION_ALIEN_ASSEMBLING_MODEL			"models/objects/alien_obj_buffstation_build.mdl"

#define BUFF_STATION_VGUI_SCREEN					"screen_obj_buffstation"

#define BUFF_STATION_BOOST_PLAYER_THINK_CONTEXT		"BoostPlayerThink"
#define BUFF_STATION_BOOST_OBJECT_THINK_CONTEXT		"BoostObjectThink"
#define BUFF_STATION_BOOST_PLAYER_THINK_INTERVAL	0.1f
#define BUFF_STATION_BOOST_OBJECT_THINK_INTERVAL	2.0f

#define BUFF_STATION_BUFF_RANGE						( 600 * 600 )

//=============================================================================
//
// Data Description
//
BEGIN_DATADESC( CObjectBuffStation )
	DEFINE_INPUTFUNC( FIELD_VOID, "PlayerSpawned", InputPlayerSpawned ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PlayerAttachedToGenerator", InputPlayerAttachedToGenerator ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PlayerEnteredVehicle", InputPlayerSpawned ),	// NJS: Detach player from buff pack.
END_DATADESC()

//=============================================================================
//
// Server Class
//
IMPLEMENT_SERVERCLASS_ST( CObjectBuffStation, DT_ObjectBuffStation )
	SendPropInt( SENDINFO( m_nPlayerCount ), BUFF_STATION_MAX_PLAYER_BITS, SPROP_UNSIGNED ),
	SendPropArray( SendPropEHandle( SENDINFO_ARRAY( m_hPlayers ) ), m_hPlayers ),
	SendPropInt( SENDINFO( m_nObjectCount ), BUFF_STATION_MAX_OBJECT_BITS, SPROP_UNSIGNED ),
	SendPropArray( SendPropEHandle( SENDINFO_ARRAY( m_hObjects ) ), m_hObjects ),
END_SEND_TABLE()

//=============================================================================
//
// Linking and Precache
//
LINK_ENTITY_TO_CLASS( obj_buff_station, CObjectBuffStation );
PRECACHE_REGISTER( obj_buff_station );

// Backwards compatability...
LINK_ENTITY_TO_CLASS( obj_portable_power_generator, CObjectBuffStation );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CObjectBuffStation::CObjectBuffStation()
{
	// Verify networking data.
	COMPILE_TIME_ASSERT( BUFF_STATION_MAX_PLAYERS < ( 1 << BUFF_STATION_MAX_PLAYER_BITS ) );
	COMPILE_TIME_ASSERT( BUFF_STATION_MAX_PLAYERS >= ( 1 << ( BUFF_STATION_MAX_PLAYER_BITS - 1 ) ) );

	COMPILE_TIME_ASSERT( BUFF_STATION_MAX_OBJECTS < ( 1 << BUFF_STATION_MAX_OBJECT_BITS ) );
	COMPILE_TIME_ASSERT( BUFF_STATION_MAX_OBJECTS >= ( 1 << ( BUFF_STATION_MAX_OBJECT_BITS - 1 ) ) );

	// Uses the client-side animation system.
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: Spawn
//-----------------------------------------------------------------------------
void CObjectBuffStation::Spawn()
{
	// This must be set before calling the base class spawn.
	m_iHealth = obj_buff_station_health.GetInt();

	BaseClass::Spawn();

	SetModel( BUFF_STATION_HUMAN_MODEL );
	SetSolid( SOLID_BBOX );

	SetType( OBJ_BUFF_STATION );
	UTIL_SetSize( this, BUFF_STATION_MINS, BUFF_STATION_MAXS );

	m_takedamage = DAMAGE_YES;

	// Initialize buff station attachment data.
	InitAttachmentData();

	// Thinking
	SetContextThink( BoostPlayerThink, 1.0f, BUFF_STATION_BOOST_PLAYER_THINK_CONTEXT );
	SetContextThink( BoostObjectThink, 2.0f, BUFF_STATION_BOOST_OBJECT_THINK_CONTEXT );

	m_bBuilding = false;
}

//-----------------------------------------------------------------------------
// Purpose: Precache model, vgui elements, and sound.
//-----------------------------------------------------------------------------
void CObjectBuffStation::Precache()
{
	// Models
	PrecacheModel( BUFF_STATION_HUMAN_MODEL );
	PrecacheModel( BUFF_STATION_ALIEN_MODEL );
	
	// Build models
	PrecacheModel( BUFF_STATION_HUMAN_ASSEMBLING_MODEL );
	PrecacheModel( BUFF_STATION_ALIEN_ASSEMBLING_MODEL );
	
	// VGUI Screen
	PrecacheVGuiScreen( BUFF_STATION_VGUI_SCREEN );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBuffStation::SetupTeamModel( void )
{
	if ( GetTeamNumber() == TEAM_HUMANS )
	{
		if ( m_bBuilding )
		{
			SetModel( BUFF_STATION_HUMAN_ASSEMBLING_MODEL );
		}
		else
		{
			SetModel( BUFF_STATION_HUMAN_MODEL );
		}
	}
	else
	{
		if ( m_bBuilding )
		{
			SetModel( BUFF_STATION_ALIEN_ASSEMBLING_MODEL );
		}
		else
		{
			SetModel( BUFF_STATION_ALIEN_MODEL );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CObjectBuffStation::GetControlPanelInfo( int nControlPanelIndex, const char *&pPanelName )
{
	pPanelName = BUFF_STATION_VGUI_SCREEN;
}

//-----------------------------------------------------------------------------
// Purpose: Remove this object from it's team and mark it for deletion.
//-----------------------------------------------------------------------------
void CObjectBuffStation::DestroyObject( void )
{
	// Detach all players.
	for ( int iPlayer = 0; iPlayer < BUFF_STATION_MAX_PLAYERS; iPlayer++ )
	{
		DetachPlayerByIndex( iPlayer );
	}

	// Detach all objects.
	for( int iObject = m_nObjectCount - 1; iObject >= 0; --iObject )
	{
		DetachObjectByIndex( iObject );
	}

	// Inform all other buff stations on this team to attempt to power object (cover for this one).
	if ( GetTFTeam() )
	{
		GetTFTeam()->UpdateBuffStations( this, NULL, false );
	}

	// We shouldn't get any more messages
	g_pNotify->ClearEntity( this );
	BaseClass::DestroyObject();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBuffStation::OnGoInactive( void )
{
	BaseClass::OnGoInactive();

	// Detach all players.
	for ( int iPlayer = 0; iPlayer < BUFF_STATION_MAX_PLAYERS; iPlayer++ )
	{
		CBaseTFPlayer *pPlayer = m_hPlayers[iPlayer].Get();
		if ( pPlayer )
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "Lost power to Buff Station!" );
		}

		DetachPlayerByIndex( iPlayer );
	}

	// Detach all objects.
	for ( int iObject = m_nObjectCount - 1; iObject >= 0; --iObject )
	{
		DetachObjectByIndex( iObject );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Attach to players who touch me
//-----------------------------------------------------------------------------
void CObjectBuffStation::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType == USE_ON )
	{
		// See if the activator is a player
		if ( !pActivator->IsPlayer() || !InSameTeam( pActivator ) || !pActivator->CanBePoweredUp() )
			return;

		CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>(pActivator);
		if ( pPlayer )
		{
			UpdatePlayerAttachment( pPlayer );
		}
	}

	BaseClass::Use( pActivator, pCaller, useType, value );
}

//-----------------------------------------------------------------------------
// Purpose: Handle commands sent from vgui panels on the client 
//-----------------------------------------------------------------------------
bool CObjectBuffStation::ClientCommand( CBaseTFPlayer *pPlayer, const char *pCmd, ICommandArguments *pArg )
{
	if ( FStrEq( pCmd, "toggle_connect" ) )
	{
		UpdatePlayerAttachment( pPlayer );
		return true;
	}

	return BaseClass::ClientCommand( pPlayer, pCmd, pArg );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBuffStation::InitAttachmentData( void )
{
	// Initialize the attachment data.
	char szAttachName[13];

	m_nPlayerCount = 0;
	Q_strncpy( szAttachName, "playercable1", 13 );
	for ( int iPlayer = 0; iPlayer < BUFF_STATION_MAX_PLAYERS; ++iPlayer )
	{
		m_hPlayers.Set( iPlayer, NULL );

		szAttachName[11] = '1' + iPlayer;
		m_aPlayerAttachInfo[iPlayer].m_iAttachPoint = LookupAttachment( szAttachName );
	}

	m_nObjectCount = 0;
	Q_strncpy( szAttachName, "objectcable1", 13 );	
	for ( int iObject = 0; iObject < BUFF_STATION_MAX_OBJECTS; ++iObject )
	{
		m_hObjects.Set( iObject, NULL );

		szAttachName[11] = '1' + iObject;
		m_aObjectAttachInfo[iObject].m_iAttachPoint = LookupAttachment( szAttachName );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Create "Buff Station" cable (rope).
//-----------------------------------------------------------------------------
CRopeKeyframe *CObjectBuffStation::CreateRope( CBaseTFPlayer *pPlayer, int iAttachPoint )
{
	CRopeKeyframe *pRope = CRopeKeyframe::Create( this, pPlayer, iAttachPoint, 0 );
	if ( pRope )
	{
		pRope->m_RopeLength = obj_buff_station_range.GetFloat();
		pRope->m_Slack = 0.0f;
		pRope->m_Width = 2;
		pRope->m_nSegments = ROPE_MAX_SEGMENTS;
		pRope->m_RopeFlags |= ROPE_COLLIDE;
		pRope->EnablePlayerWeaponAttach( true );
		pRope->ActivateStartDirectionConstraints( true );
		if ( GetTeamNumber() == TEAM_HUMANS )
		{
			pRope->SetMaterial( "cable/human_buffcable.vmt" );
		}
		else
		{
			pRope->SetMaterial( "cable/alien_buffcable.vmt" );
		}
	}

	return pRope;
}

//-----------------------------------------------------------------------------
// Purpose: Create "Buff Station" cable (rope).
//-----------------------------------------------------------------------------
CRopeKeyframe *CObjectBuffStation::CreateRope( CBaseObject *pObject, int iAttachPoint, int iObjectAttachPoint )
{
	CRopeKeyframe *pRope = CRopeKeyframe::Create( this, pObject, iAttachPoint, iObjectAttachPoint );
	if ( pRope )
	{
		pRope->m_RopeLength = obj_buff_station_obj_range.GetFloat();
		pRope->m_Slack = 0.0f;
		pRope->m_Width = 2;
		pRope->m_nSegments = ROPE_MAX_SEGMENTS;
		pRope->m_RopeFlags |= ROPE_COLLIDE;
		pRope->EnablePlayerWeaponAttach( true );
		pRope->ActivateStartDirectionConstraints( true );
		if ( GetTeamNumber() == TEAM_HUMANS )
		{
			pRope->SetMaterial( "cable/human_buffcable.vmt" );
		}
		else
		{
			pRope->SetMaterial( "cable/alien_buffcable.vmt" );
		}
	}

	return pRope;
}

//-----------------------------------------------------------------------------
// Purpose: Is a particular player attached?
//-----------------------------------------------------------------------------
bool CObjectBuffStation::IsPlayerAttached( CBaseTFPlayer *pPlayer )
{
	for ( int iPlayer = 0; iPlayer < BUFF_STATION_MAX_PLAYERS; iPlayer++ )
	{
		if ( m_hPlayers[iPlayer].Get() == pPlayer )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Is a particular object attached?
//-----------------------------------------------------------------------------
bool CObjectBuffStation::IsObjectAttached( CBaseObject *pObject )
{
	for ( int iObject = 0; iObject < m_nObjectCount; ++iObject )
	{
		if ( m_hObjects[iObject].Get() == pObject )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Attach the player to the "Buff Station."
//-----------------------------------------------------------------------------
void CObjectBuffStation::AttachPlayer( CBaseTFPlayer *pPlayer )
{
	// Player shouldn't already be attached.
	Assert( !IsPlayerAttached( pPlayer ) );

	// Check to see if the player is alive and on the correct team.
	if ( !pPlayer->IsAlive() || !pPlayer->InSameTeam( this ) )
		return;

	// Check attachment availability.
	if ( m_nPlayerCount == BUFF_STATION_MAX_PLAYERS )
	{
		// Unless the player is the owner he cannot connect.
		if ( pPlayer != GetOwner() )
			return;

		// Kick a non-owning player off.
		DetachPlayerByIndex( BUFF_STATION_MAX_PLAYERS - 1 );
	}

	// This will disconnect the player from other Buff Stations, and keep track of important player events.
	g_pNotify->ReportNamedEvent( pPlayer, "PlayerAttachedToGenerator" );
	g_pNotify->AddEntity( this, pPlayer );

	// Connect player.
	// Find the nearest empty slot
	int iNearest = BUFF_STATION_MAX_PLAYERS;
	float flNearestDist = 9999*9999;
	for ( int iPlayer = 0; iPlayer < BUFF_STATION_MAX_PLAYERS; iPlayer++ )
	{
		if ( !m_hPlayers[iPlayer] )
		{
			Vector vecPoint;
			QAngle angPoint;
			GetAttachment( m_aPlayerAttachInfo[iPlayer].m_iAttachPoint, vecPoint, angPoint );
			float flDistance = ( vecPoint - pPlayer->GetAbsOrigin() ).LengthSqr();
			if ( flDistance < flNearestDist )
			{
				flNearestDist = flDistance;
				iNearest = iPlayer;
			}
		}
	}
	Assert( iNearest != BUFF_STATION_MAX_PLAYERS );

	m_hPlayers.Set( iNearest, pPlayer );
	m_aPlayerAttachInfo[iNearest].m_DamageModifier.SetModifier( obj_buff_station_damage_modifier.GetFloat() );
	m_aPlayerAttachInfo[iNearest].m_hRope = CreateRope( pPlayer, m_aPlayerAttachInfo[iNearest].m_iAttachPoint );
	m_nPlayerCount++;

	// Tell the player to constrain his movement.
	pPlayer->ActivateMovementConstraint( this, GetAbsOrigin(), obj_buff_station_range.GetFloat(), 75.0f, 0.15f );

	// Update think.
	if ( GetNextThink(BUFF_STATION_BOOST_PLAYER_THINK_CONTEXT) > gpGlobals->curtime + BUFF_STATION_BOOST_PLAYER_THINK_INTERVAL )
	{
		SetNextThink( gpGlobals->curtime + BUFF_STATION_BOOST_PLAYER_THINK_INTERVAL, BUFF_STATION_BOOST_PLAYER_THINK_CONTEXT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Detach the player from the "Buff Station."
//-----------------------------------------------------------------------------
void CObjectBuffStation::DetachPlayer( CBaseTFPlayer *pPlayer )
{
	for ( int iPlayer = 0; iPlayer < BUFF_STATION_MAX_PLAYERS; iPlayer++ )
	{
		if ( m_hPlayers[iPlayer].Get() == pPlayer )
		{
			DetachPlayerByIndex( iPlayer );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Detach the player from the "Buff Station."
//-----------------------------------------------------------------------------
void CObjectBuffStation::DetachPlayerByIndex( int nIndex )
{
	// Valid index?
	Assert( nIndex < BUFF_STATION_MAX_PLAYERS );

	// Get the player.
	CBaseTFPlayer *pPlayer = m_hPlayers[nIndex].Get();
	if ( !pPlayer )
	{
		m_hPlayers.Set( nIndex, NULL );
		return;
	}

	// Remove the damage modifier.
	m_aPlayerAttachInfo[nIndex].m_DamageModifier.RemoveModifier();

	// Remove the rope (cable).
	if ( m_aPlayerAttachInfo[nIndex].m_hRope.Get() )
	{
		m_aPlayerAttachInfo[nIndex].m_hRope->DetachPoint( 1 );
		m_aPlayerAttachInfo[nIndex].m_hRope->DieAtNextRest();
	}

	// Unconstrain the player movement.
	pPlayer->DeactivateMovementConstraint();

	// Keep track of player events.
	g_pNotify->RemoveEntity( this, pPlayer );

	// Reduce player count.
	m_nPlayerCount--;
	m_hPlayers.Set( nIndex, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Attach the object to the "Buff Station."
//-----------------------------------------------------------------------------
void CObjectBuffStation::AttachObject( CBaseObject *pObject, bool bPlacing )
{
	// Check to see if the object is already attached.
	if ( IsObjectAttached( pObject ) )
		return;

	// Check to see if the object is on the correct team.
	if ( !pObject->InSameTeam( this ) )
		return;

	// Check to see if the object is already being buffed by another station.
	if ( pObject->IsHookedAndBuffed() )
		return;

	// Check attachment availability.
	if ( m_nObjectCount == BUFF_STATION_MAX_OBJECTS )
		return;

	// Attach cable to object - get the attachment point.
	int nObjectAttachPoint = pObject->LookupAttachment( "boostpoint" );
	if ( nObjectAttachPoint <= 0 )
		nObjectAttachPoint = 1;

	// Connect object.
	m_hObjects.Set( m_nObjectCount, pObject );
	m_aObjectAttachInfo[m_nObjectCount].m_DamageModifier.SetModifier( obj_buff_station_damage_modifier.GetFloat() );
	m_aObjectAttachInfo[m_nObjectCount].m_hRope = CreateRope( pObject, m_aObjectAttachInfo[m_nObjectCount].m_iAttachPoint, nObjectAttachPoint );
	m_nObjectCount += 1;

	// If we're placing, we're pretending to buff objects, but not really powering them
	pObject->SetBuffStation( this, bPlacing );

	// Update think.
	if ( GetNextThink(BUFF_STATION_BOOST_OBJECT_THINK_CONTEXT) > gpGlobals->curtime + BUFF_STATION_BOOST_OBJECT_THINK_INTERVAL )
	{
		SetNextThink( gpGlobals->curtime + BUFF_STATION_BOOST_OBJECT_THINK_INTERVAL, BUFF_STATION_BOOST_OBJECT_THINK_CONTEXT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Detach the object from the "Buff Station."
//-----------------------------------------------------------------------------
void CObjectBuffStation::DetachObject( CBaseObject *pObject )
{
	for ( int iObject = 0; iObject < m_nObjectCount; ++iObject )
	{
		if ( m_hObjects[iObject].Get() == pObject )
		{
			DetachObjectByIndex( iObject );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Detach the object from the "Buff Station."
//-----------------------------------------------------------------------------
void CObjectBuffStation::DetachObjectByIndex( int nIndex )
{
	// Valid index?
	Assert( nIndex >= 0 );
	Assert( nIndex < m_nObjectCount );

	// Get the object.
	CBaseObject *pObject = m_hObjects[nIndex].Get();
	if ( !pObject )
		return;

	// Remove the damage modifier.
	m_aObjectAttachInfo[nIndex].m_DamageModifier.RemoveModifier();

	// Remove the rope (cable).
	if ( m_aObjectAttachInfo[nIndex].m_hRope.Get() )
	{
		m_aObjectAttachInfo[nIndex].m_hRope->DetachPoint( 1 );
		m_aObjectAttachInfo[nIndex].m_hRope->DieAtNextRest();
	}

	// Reduce object count.
	m_nObjectCount -= 1;

	// Set the object as unbuffed.
	pObject->SetBuffStation( NULL, false );

	// If the detached object wasn't the last object in the list, swap placement.
	if ( nIndex != m_nObjectCount )
	{
		SwapObjectAttachment( nIndex );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBuffStation::UpdatePlayerAttachment( CBaseTFPlayer *pPlayer )
{
	// Valid player?
	if ( !pPlayer )
		return;

	// Attach/Detach (toggle).
	if ( IsPlayerAttached( pPlayer ) )
	{
		DetachPlayer( pPlayer );
	}
	else
	{
		// Check for power, do not attach to unpowered generator.
		if ( !IsPowered() )
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "No power source for the Buff Station!" );
		}
		else
		{
			AttachPlayer( pPlayer );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectBuffStation::SwapObjectAttachment( int nIndex )
{
	bool bModifierActive = m_aObjectAttachInfo[m_nObjectCount].m_DamageModifier.GetCharacter() != NULL;
	m_aObjectAttachInfo[m_nObjectCount].m_DamageModifier.RemoveModifier();

	m_hObjects.Set( nIndex, m_hObjects[m_nObjectCount] );
	m_aObjectAttachInfo[nIndex] = m_aObjectAttachInfo[m_nObjectCount];

	if ( bModifierActive )
	{
		m_aObjectAttachInfo[nIndex].m_DamageModifier.AddModifierToEntity( m_hObjects[nIndex].Get() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler
//-----------------------------------------------------------------------------
void CObjectBuffStation::InputPlayerSpawned( inputdata_t &inputdata )
{
	if ( inputdata.pActivator->IsPlayer() )
	{
		CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>( inputdata.pActivator );
		if ( IsPlayerAttached( pPlayer ) )
		{
			DetachPlayer( pPlayer );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler
//-----------------------------------------------------------------------------
void CObjectBuffStation::InputPlayerAttachedToGenerator( inputdata_t &inputdata )
{
	if ( inputdata.pActivator->IsPlayer() )
	{
		CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>( inputdata.pActivator );
		if ( IsPlayerAttached( pPlayer ) )
		{
			DetachPlayer( pPlayer );
		}
	}
}

//-----------------------------------------------------------------------------
// Boost those attached to me as long as I'm not EMPed
//-----------------------------------------------------------------------------
void CObjectBuffStation::BoostPlayerThink( void )
{
	// Are we emped?
	bool bIsEmped = HasPowerup( POWERUP_EMP );

	// Get range (squared = faster test).
	float flMaxRangeSq = obj_buff_station_range.GetFloat();
	flMaxRangeSq *= flMaxRangeSq;

	// Boost all attached players and objects.
	for ( int iPlayer = 0; iPlayer < BUFF_STATION_MAX_PLAYERS; iPlayer++ )
	{
		// Clean up dangling pointers + dead players, subversion, disconnection
		CBaseTFPlayer *pPlayer = m_hPlayers[iPlayer].Get();
		if ( !pPlayer || !pPlayer->IsAlive() || !InSameTeam( pPlayer ) || !pPlayer->PlayerClass() )
		{
			DetachPlayerByIndex( iPlayer );
			continue;
		}

		// Check for out of range.
		float flDistSq = GetAbsOrigin().DistToSqr( pPlayer->GetAbsOrigin() ); 
		if ( flDistSq > flMaxRangeSq )
		{
			DetachPlayerByIndex( iPlayer );
			continue;
		}

		bool bBoosted = false;
		if ( !bIsEmped )
		{
			float flHealAmount = obj_buff_station_heal_rate.GetFloat() * BUFF_STATION_BOOST_PLAYER_THINK_INTERVAL;
			bBoosted = pPlayer->AttemptToPowerup( POWERUP_BOOST, 0, flHealAmount, this, &m_aPlayerAttachInfo[iPlayer].m_DamageModifier );
		}

		if ( !bBoosted )
		{
			m_aPlayerAttachInfo[iPlayer].m_DamageModifier.RemoveModifier();
		}
	}

	// Set next think time.
	if ( m_nPlayerCount > 0 )
	{
		SetNextThink( gpGlobals->curtime + BUFF_STATION_BOOST_PLAYER_THINK_INTERVAL, 
			          BUFF_STATION_BOOST_PLAYER_THINK_CONTEXT );
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 1.0f, BUFF_STATION_BOOST_PLAYER_THINK_CONTEXT );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectBuffStation::BoostObjectThink( void )
{
	// Set next boost object think time.
	SetNextThink( gpGlobals->curtime + BUFF_STATION_BOOST_OBJECT_THINK_INTERVAL, 
		          BUFF_STATION_BOOST_OBJECT_THINK_CONTEXT );

	// If we're emped, placing, or building, we're not ready to powerup
	if ( IsPlacing() || IsBuilding() || HasPowerup( POWERUP_EMP ) )
		return;

	float flMaxRangeSq = obj_buff_station_obj_range.GetFloat();
	flMaxRangeSq *= flMaxRangeSq;

	// Boost objects.
	for ( int iObject = m_nObjectCount; --iObject >= 0; )
	{
		CBaseObject *pObject = m_hObjects[iObject].Get();
		if ( !pObject || !InSameTeam( pObject ) )
		{
			DetachObjectByIndex( iObject );
			continue;
		}

		// Check for out of range.
		float flDistSq = GetAbsOrigin().DistToSqr( pObject->GetAbsOrigin() ); 
		if ( flDistSq > flMaxRangeSq )
		{
			DetachObjectByIndex( iObject );
			continue;
		}

		// Don't powerup it until it's finished building
		if ( pObject->IsPlacing() || pObject->IsBuilding() )
			continue;

		// Boost it
		if ( !pObject->AttemptToPowerup( POWERUP_BOOST, BUFF_STATION_BOOST_OBJECT_THINK_INTERVAL, 0, 
			                                  this, &m_aObjectAttachInfo[iObject].m_DamageModifier ) )
		{
			m_aObjectAttachInfo[iObject].m_DamageModifier.RemoveModifier();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectBuffStation::DeBuffObject( CBaseObject *pObject )
{
	DetachObject( pObject );
}

//-----------------------------------------------------------------------------
// Purpose: Find nearby objects and buff them.
//-----------------------------------------------------------------------------
void CObjectBuffStation::BuffNearbyObjects( CBaseObject *pObjectToTarget, bool bPlacing )
{
	// ROBIN: Disabled object buffing for now
	return;

	// Check for a team.
	if ( !GetTFTeam() )
		return;

	// Am I ready to power anything?
	if ( IsBuilding() || ( !bPlacing && IsPlacing() ) )
		return;

	// Am I already full?
	if ( m_nObjectCount >= BUFF_STATION_MAX_OBJECTS )
		return;

	// Do we have a specific target?
	if ( pObjectToTarget )
	{
		if( !pObjectToTarget->CanBeHookedToBuffStation() || pObjectToTarget->GetBuffStation() )
			return;

		if ( IsWithinBuffRange( pObjectToTarget ) )
		{
			AttachObject( pObjectToTarget, bPlacing );
		}
	}
	else
	{
		// Find nearby objects 
		for ( int iObject = 0; iObject < GetTFTeam()->GetNumObjects(); iObject++ )
		{
			CBaseObject *pObject = GetTFTeam()->GetObject( iObject );
			assert(pObject);

			if ( pObject == this || !pObject->CanBeHookedToBuffStation() || pObject->GetBuffStation() )
				continue;

			// Make sure it's within range
			if ( IsWithinBuffRange( pObject ) )
			{
				AttachObject( pObject, bPlacing );

				// Am I now full?
				if ( m_nObjectCount >= BUFF_STATION_MAX_OBJECTS )
					break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update buff connections on the fly while placing
//-----------------------------------------------------------------------------
bool CObjectBuffStation::CalculatePlacement( CBaseTFPlayer *pPlayer )
{
	bool bReturn = BaseClass::CalculatePlacement( pPlayer );

	// First, disconnect any connections that should break (too far away).
	for ( int iObject = m_nObjectCount - 1; iObject >= 0; --iObject )
	{
		if ( GetBuffedObject( iObject ) )
		{
			CheckBuffConnection( GetBuffedObject( iObject ) );
		}
	}

	// If we have any spare connections, look for nearby objects to buff
	if ( m_nObjectCount < BUFF_STATION_MAX_OBJECTS )
	{
		BuffNearbyObjects( NULL, true );
	}

	return bReturn;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBuffStation::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	for( int iObject = 0; iObject < m_nObjectCount; ++iObject )
	{
		if ( GetBuffedObject( iObject ) )
		{
			GetBuffedObject( iObject )->SetBuffStation( this, false );
		}
	}

	BuffNearbyObjects( NULL, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBuffStation::CheckBuffConnection( CBaseObject *pObject )
{
	if ( !pObject->CanBeHookedToBuffStation() )
		return;

	// Check to see if the object is within the buff range.
	if ( IsWithinBuffRange( pObject ) )
		return;

	// It's obscured, or out of range. Remove it.
	DetachObject( pObject );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this object is powerable
//-----------------------------------------------------------------------------
bool CObjectBuffStation::IsWithinBuffRange( CBaseObject *pObject )
{
	if ( ( pObject->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr() < BUFF_STATION_BUFF_RANGE )
	{
		// Can I see it?
		// Ignore things we're attached to
		trace_t tr;
		CTraceFilterWorldAndPropsOnly buffFilter;
		UTIL_TraceLine( WorldSpaceCenter(), pObject->WorldSpaceCenter(), MASK_SOLID_BRUSHONLY, &buffFilter, &tr );
		CBaseEntity *pEntity = tr.m_pEnt;
		if ( ( tr.fraction == 1.0 ) || ( pEntity == pObject ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : act - 
//-----------------------------------------------------------------------------
void CObjectBuffStation::OnActivityChanged( Activity act )
{
	BaseClass::OnActivityChanged( act );

	switch ( act )
	{
	case ACT_OBJ_ASSEMBLING:
		m_bBuilding = true;
		break;
	default:
		m_bBuilding = false;
		break;
	}

	SetupTeamModel();
}


