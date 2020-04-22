//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates objective data
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_dod_objective_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// owner recv proxy
void RecvProxy_Owner( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// hacks? Not sure how else to get the index of the integer that is 
	// being transmitted.
	int index = pData->m_pRecvProp->GetOffset() / sizeof(int);

	g_pObjectiveResource->SetOwningTeam( index, pData->m_Value.m_Int );
}

// capper recv proxy
void RecvProxy_CappingTeam( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	int index = pData->m_pRecvProp->GetOffset() / sizeof(int);

	g_pObjectiveResource->SetCappingTeam( index, pData->m_Value.m_Int );
}

void RecvProxy_BombPlanted( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	int index = pData->m_pRecvProp->GetOffset() / sizeof(bool);

	g_pObjectiveResource->SetBombPlanted( index, pData->m_Value.m_Int );
}

IMPLEMENT_CLIENTCLASS_DT_NOBASE(C_DODObjectiveResource, DT_DODObjectiveResource, CDODObjectiveResource)
	
	RecvPropInt( RECVINFO(m_iNumControlPoints) ),

	// data variables
	RecvPropArray( RecvPropVector(RECVINFO(m_vCPPositions[0])), m_vCPPositions),
	RecvPropArray3( RECVINFO_ARRAY(m_bCPIsVisible),		RecvPropInt( RECVINFO(m_bCPIsVisible[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iAlliesIcons),		RecvPropInt( RECVINFO(m_iAlliesIcons[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iAxisIcons),		RecvPropInt( RECVINFO(m_iAxisIcons[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iNeutralIcons),	RecvPropInt( RECVINFO(m_iNeutralIcons[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iTimerCapIcons),	RecvPropInt( RECVINFO(m_iTimerCapIcons[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iBombedIcons),		RecvPropInt( RECVINFO(m_iBombedIcons[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iAlliesReqCappers),RecvPropInt( RECVINFO(m_iAlliesReqCappers[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iAxisReqCappers),	RecvPropInt( RECVINFO(m_iAxisReqCappers[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_flAlliesCapTime),	RecvPropTime( RECVINFO(m_flAlliesCapTime[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_flAxisCapTime),	RecvPropTime( RECVINFO(m_flAxisCapTime[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_bBombPlanted),		RecvPropInt( RECVINFO(m_bBombPlanted[0]), 0, RecvProxy_BombPlanted ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iBombsRequired),	RecvPropInt( RECVINFO(m_iBombsRequired[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iBombsRemaining),	RecvPropInt( RECVINFO(m_iBombsRemaining[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_bBombBeingDefused),RecvPropInt( RECVINFO(m_bBombBeingDefused[0]) ) ),

	// state variables
	RecvPropArray3( RECVINFO_ARRAY(m_iNumAllies),		RecvPropInt( RECVINFO(m_iNumAllies[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iNumAxis),			RecvPropInt( RECVINFO(m_iNumAxis[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iCappingTeam),		RecvPropInt( RECVINFO(m_iCappingTeam[0]), 0, RecvProxy_CappingTeam ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iOwner),			RecvPropInt( RECVINFO(m_iOwner[0]), 0, RecvProxy_Owner ) ),

END_RECV_TABLE()

C_DODObjectiveResource *g_pObjectiveResource;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_DODObjectiveResource::C_DODObjectiveResource()
{
	m_iNumControlPoints = 0;

	for ( int i=0; i < MAX_CONTROL_POINTS; i++ )
	{
		m_flCapStartTimes[i] = 0;
		m_flCapEndTimes[i] = 0;
		m_iAlliesIcons[i] = 0;
		m_iAxisIcons[i] = 0;
		m_iNeutralIcons[i] = 0;
		m_iTimerCapIcons[i] = 0;
		m_iBombedIcons[i] = 0;
		m_iNumAllies[i] = 0;
		m_iNumAxis[i] = 0;
	}

	g_pObjectiveResource = this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_DODObjectiveResource::~C_DODObjectiveResource()
{
	g_pObjectiveResource = NULL;
}

float C_DODObjectiveResource::GetCPCapPercentage( int index )
{
	Assert( 0 <= index && index <= m_iNumControlPoints );

	float flCapLength = m_flCapEndTimes[index] - m_flCapStartTimes[index];

	if( flCapLength <= 0 )
		return 0.0f;

	float flElapsedTime = gpGlobals->curtime - m_flCapStartTimes[index];

	if( flElapsedTime > flCapLength )
		return 1.0f;

	return ( flElapsedTime / flCapLength );
}

void C_DODObjectiveResource::SetOwningTeam( int index, int team )
{
	int cappingTeam = GetCappingTeam(index);

	if ( team == cappingTeam )
	{
		// successful cap, reset things
		m_iCappingTeam[index] = TEAM_UNASSIGNED;
		m_flCapStartTimes[index] = 0.0f;
		m_flCapEndTimes[index] = 0.0f;
	}

	m_iOwner[index] = team;
}

void C_DODObjectiveResource::SetCappingTeam( int index, int team )
{
	if ( team != GetOwningTeam( index ) && ( team == TEAM_ALLIES || team == TEAM_AXIS ) )
	{
		int caplength = 0;

		switch ( team )
		{
		case TEAM_ALLIES:
			caplength = m_flAlliesCapTime[index];
			break;
		case TEAM_AXIS:
			caplength = m_flAxisCapTime[index];
			break;
		}

		// start the cap
		m_flCapStartTimes[index] = gpGlobals->curtime;
		m_flCapEndTimes[index] = gpGlobals->curtime + caplength;
	}
	else
	{
		m_flCapStartTimes[index] = 0.0;
		m_flCapEndTimes[index] = 0.0;
	}

	m_iCappingTeam[index] = team;
}

void C_DODObjectiveResource::SetBombPlanted( int index, int iBombPlanted )
{
	//		Assert( index < m_iNumControlPoints );

	m_bBombPlanted[index] = ( iBombPlanted > 0 );

	if ( m_bBombPlanted[index] )
	{
		// start the countdown, make timer visible
		m_flBombEndTimes[index] = gpGlobals->curtime + DOD_BOMB_TIMER_LENGTH;
	}
	else
	{
		// hide timer, reset countdown
		m_flBombEndTimes[index] = 0;
	}
}