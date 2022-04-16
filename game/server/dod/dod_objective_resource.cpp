//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "dod_objective_resource.h"
#include "shareddefs.h"
#include <coordsize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Datatable
IMPLEMENT_SERVERCLASS_ST_NOBASE(CDODObjectiveResource, DT_DODObjectiveResource)

	SendPropInt( SENDINFO(m_iNumControlPoints), 4, SPROP_UNSIGNED ),

	// data variables
	SendPropArray( SendPropVector( SENDINFO_ARRAY(m_vCPPositions), -1, SPROP_COORD), m_vCPPositions ),
	SendPropArray3( SENDINFO_ARRAY3(m_bCPIsVisible), SendPropInt( SENDINFO_ARRAY(m_bCPIsVisible), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iAlliesIcons), SendPropInt( SENDINFO_ARRAY(m_iAlliesIcons), 8, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iAxisIcons), SendPropInt( SENDINFO_ARRAY(m_iAxisIcons), 8, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iNeutralIcons), SendPropInt( SENDINFO_ARRAY(m_iNeutralIcons), 8, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iTimerCapIcons), SendPropInt( SENDINFO_ARRAY(m_iTimerCapIcons), 8, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iBombedIcons), SendPropInt( SENDINFO_ARRAY(m_iBombedIcons), 8, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iAlliesReqCappers), SendPropInt( SENDINFO_ARRAY(m_iAlliesReqCappers), 4, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iAxisReqCappers), SendPropInt( SENDINFO_ARRAY(m_iAxisReqCappers), 4, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_flAlliesCapTime), SendPropTime( SENDINFO_ARRAY(m_flAlliesCapTime) ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_flAxisCapTime), SendPropTime( SENDINFO_ARRAY(m_flAxisCapTime) ) ),

	SendPropArray3( SENDINFO_ARRAY3(m_bBombPlanted), SendPropInt( SENDINFO_ARRAY(m_bBombPlanted), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iBombsRequired), SendPropInt( SENDINFO_ARRAY(m_iBombsRequired), 2, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iBombsRemaining), SendPropInt( SENDINFO_ARRAY(m_iBombsRemaining), 2, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_bBombBeingDefused), SendPropInt( SENDINFO_ARRAY(m_bBombBeingDefused), 1, SPROP_UNSIGNED ) ),

	// state variables
	SendPropArray3( SENDINFO_ARRAY3(m_iNumAllies), SendPropInt( SENDINFO_ARRAY(m_iNumAllies), 4, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iNumAxis), SendPropInt( SENDINFO_ARRAY(m_iNumAxis), 4, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iCappingTeam), SendPropInt( SENDINFO_ARRAY(m_iCappingTeam), 4, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iOwner), SendPropInt( SENDINFO_ARRAY(m_iOwner), 4, SPROP_UNSIGNED ) ),

END_SEND_TABLE()


BEGIN_DATADESC( CDODObjectiveResource )
END_DATADESC()


LINK_ENTITY_TO_CLASS( dod_objective_resource, CDODObjectiveResource );

CDODObjectiveResource *g_pObjectiveResource;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODObjectiveResource::Spawn( void )
{
	m_iNumControlPoints = 0;

	for ( int i=0; i < MAX_CONTROL_POINTS; i++ )
	{
		// data variables
		m_vCPPositions.Set( i, vec3_origin );
		m_bCPIsVisible.Set( i, true );
		m_iAlliesIcons.Set( i, 0 );
		m_iAxisIcons.Set( i, 0 );
		m_iNeutralIcons.Set( i, 0 );
		m_iTimerCapIcons.Set( i, 0 );
		m_iBombedIcons.Set( i, 0 );
		m_iAlliesReqCappers.Set( i, 0 );
		m_iAxisReqCappers.Set( i, 0 );
		m_flAlliesCapTime.Set( i, 0.0f );
		m_flAxisCapTime.Set( i, 0.0f );
		m_bBombPlanted.Set( i, 0 );
		m_iBombsRequired.Set( i, 0 );
		m_iBombsRemaining.Set( i, 0 );
		m_bBombBeingDefused.Set( i, 0 );

		// state variables
		m_iNumAllies.Set( i, 0 );
		m_iNumAxis.Set( i, 0 );
		m_iCappingTeam.Set( i, TEAM_UNASSIGNED );
		m_iOwner.Set( i, TEAM_UNASSIGNED );
	}

}

//-----------------------------------------------------------------------------
// Purpose: The objective resource is always transmitted to clients
//-----------------------------------------------------------------------------
int CDODObjectiveResource::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Round is starting, reset state
//-----------------------------------------------------------------------------
void CDODObjectiveResource::ResetControlPoints( void )
{
	for ( int i=0; i < MAX_CONTROL_POINTS; i++ )
	{
		m_iNumAllies.Set( i, 0 );
		m_iNumAxis.Set( i, 0 );
		m_iCappingTeam.Set( i, TEAM_UNASSIGNED );

		m_bBombPlanted.Set( i, 0 );
		m_bBombBeingDefused.Set( i, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Data setting functions
//-----------------------------------------------------------------------------
void CDODObjectiveResource::SetNumControlPoints( int num )
{
	Assert( num <= MAX_CONTROL_POINTS );
	m_iNumControlPoints = num;
}

void CDODObjectiveResource::SetCPIcons( int index, int iAlliesIcon, int iAxisIcon, int iNeutralIcon, int iTimerCapIcon, int iBombedIcon )
{
	AssertValidIndex(index);
	m_iAlliesIcons.Set( index, iAlliesIcon);
	m_iAxisIcons.Set( index, iAxisIcon );
	m_iNeutralIcons.Set( index, iNeutralIcon );
	m_iTimerCapIcons.Set( index, iTimerCapIcon );
	m_iBombedIcons.Set( index, iBombedIcon );
}

void CDODObjectiveResource::SetCPPosition( int index, const Vector& vPosition )
{
	AssertValidIndex(index);
	m_vCPPositions.Set( index, vPosition );
}

void CDODObjectiveResource::SetCPVisible( int index, bool bVisible )
{
	AssertValidIndex(index);
	m_bCPIsVisible.Set( index, bVisible );
}

void CDODObjectiveResource::SetCPRequiredCappers( int index, int iReqAllies, int iReqAxis )
{
	AssertValidIndex(index);
	m_iAlliesReqCappers.Set( index, iReqAllies );
	m_iAxisReqCappers.Set( index, iReqAxis );
}

void CDODObjectiveResource::SetCPCapTime( int index, float flAlliesCapTime, float flAxisCapTime )
{
	AssertValidIndex(index);
	m_flAlliesCapTime.Set( index, flAlliesCapTime );
	m_flAxisCapTime.Set( index, flAxisCapTime );
}

//-----------------------------------------------------------------------------
// Purpose: Data setting functions
//-----------------------------------------------------------------------------
void CDODObjectiveResource::SetNumPlayers( int index, int team, int iNumPlayers )
{
	AssertValidIndex(index);

	switch( team )
	{
	case TEAM_ALLIES:
		m_iNumAllies.Set( index, iNumPlayers );
		break;

	case TEAM_AXIS:
		m_iNumAxis.Set( index, iNumPlayers );
		break;

	default:
		Assert( 0 );
		break;
	}
}

void CDODObjectiveResource::StartCap( int index, int team )
{
	AssertValidIndex(index);
	m_iCappingTeam.Set( index, team );
}

void CDODObjectiveResource::SetOwningTeam( int index, int team )
{
	AssertValidIndex(index);
	m_iOwner.Set( index, team );

	// clear the capper
	m_iCappingTeam.Set( index, TEAM_UNASSIGNED );
}

void CDODObjectiveResource::SetCappingTeam( int index, int team )
{
	AssertValidIndex(index);
	m_iCappingTeam.Set( index, team );
}

void CDODObjectiveResource::SetBombPlanted( int index, bool bPlanted )
{
	AssertValidIndex(index);
	m_bBombPlanted.Set( index, bPlanted );
}

void CDODObjectiveResource::SetBombBeingDefused( int index, bool bBeingDefused )
{
	AssertValidIndex(index);
	m_bBombBeingDefused.Set( index, bBeingDefused );
}

void CDODObjectiveResource::SetBombsRequired( int index, int iBombsRequired )
{
	AssertValidIndex(index);
	m_iBombsRequired.Set( index, iBombsRequired );
}

void CDODObjectiveResource::SetBombsRemaining( int index, int iBombsRemaining )
{
	AssertValidIndex(index);
	m_iBombsRemaining.Set( index, iBombsRemaining );
}
