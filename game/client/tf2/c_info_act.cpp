//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "tf_shareddefs.h"
#include "c_info_act.h"
#include "hud_timer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// We may get act begin messages for acts we haven't yet received entities for (usually during connection)
// We store the current act in this, and if an act arrives matching it, we start that act.
static int		g_iCurrentActNumber = -1;
static float	g_flActStartTime;

CHandle<C_InfoAct>	g_hCurrentAct;

IMPLEMENT_CLIENTCLASS_DT(C_InfoAct, DT_InfoAct, CInfoAct)
	RecvPropInt( RECVINFO(m_iActNumber) ),
	RecvPropInt( RECVINFO(m_spawnflags) ),
	RecvPropFloat( RECVINFO(m_flActTimeLimit) ),
	RecvPropInt(RECVINFO(m_nRespawn1Team1Time) ),
	RecvPropInt(RECVINFO(m_nRespawn1Team2Time) ),
	RecvPropInt(RECVINFO(m_nRespawn2Team1Time) ),
	RecvPropInt(RECVINFO(m_nRespawn2Team2Time) ),
	RecvPropInt(RECVINFO(m_nRespawnTeam1Delay) ),
	RecvPropInt(RECVINFO(m_nRespawnTeam2Delay) ),
END_RECV_TABLE()

typedef CHandle<C_InfoAct>	ActHandle_t;
CUtlVector< ActHandle_t >	g_hActs;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_InfoAct::C_InfoAct()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_InfoAct::~C_InfoAct()
{
	ActHandle_t hAct;
	hAct = this;
	g_hActs.FindAndRemove( hAct );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_InfoAct::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );
	m_flPreviousTimeLimit = m_flActTimeLimit;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_InfoAct::OnDataChanged( DataUpdateType_t updateType )
{
	ActHandle_t hAct;
	hAct = this;
	if ( g_hActs.Find( hAct ) == g_hActs.InvalidIndex() )
	{
		g_hActs.AddToTail( hAct );

		// Is this act the one that's supposed to be going?
		if ( GetActNumber() == g_iCurrentActNumber )
		{
			StartAct( g_flActStartTime );
			return;
		}
	}

	// Timer changed?
	if ( g_hCurrentAct == this )
	{
		if ( m_flPreviousTimeLimit != m_flActTimeLimit )
		{
			CHudTimer *timer = GET_HUDELEMENT( CHudTimer );
			if ( timer )
			{
				timer->SetFixedTimer( m_flStartTime, m_flActTimeLimit );
			}
		}
	}

	BaseClass::OnDataChanged( updateType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_InfoAct::StartAct( float flStartTime )
{
	g_hCurrentAct = this;
	m_flStartTime = flStartTime;

	CHudTimer *timer = GET_HUDELEMENT( CHudTimer );
	if ( timer )
	{
		timer->SetFixedTimer( m_flStartTime, m_flActTimeLimit );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_InfoAct::IsAWaitingAct( void )
{ 
	return (m_spawnflags & SF_ACT_WAITINGFORGAMESTART) != 0;
}


//-----------------------------------------------------------------------------
// PReturns the respawn time remaining
//-----------------------------------------------------------------------------
float C_InfoAct::RespawnTimeRemaining( int nTeam, int nTimer ) const
{
	if ((g_hCurrentAct != this) || (nTeam == 0))
		return 0;

	int nTimerTime;
	float flTimeDelta = gpGlobals->curtime - m_flStartTime;
	if (flTimeDelta <= 0)
		return 0;

	if (nTeam == 1)
	{
		nTimerTime = (nTimer == 1) ? m_nRespawn1Team1Time : m_nRespawn2Team1Time;
		flTimeDelta -= m_nRespawnTeam1Delay;
	}
	else
	{
		nTimerTime = (nTimer == 1) ? m_nRespawn1Team2Time : m_nRespawn2Team2Time;
		flTimeDelta -= m_nRespawnTeam2Delay;
	}

	if (nTimerTime <= 0)
		return 0.0f;

	// This case takes care of the initial spawn delay time...
	if (flTimeDelta < 0)
	{
		return nTimerTime - flTimeDelta;
	}

	int nFactor = flTimeDelta / nTimerTime;
	return nTimerTime - (flTimeDelta - nFactor * nTimerTime);
}


//-----------------------------------------------------------------------------
// Purpose: Server's told us to start an act
//-----------------------------------------------------------------------------
void StartAct( int iActNumber, float flStartTime )
{
	g_iCurrentActNumber = iActNumber;
	g_flActStartTime = flStartTime;

	// Find the act
	for ( int i = 0; i < g_hActs.Size(); i++ )
	{
		if ( g_hActs[i] && g_hActs[i]->GetActNumber() == iActNumber )
		{
			g_hActs[i]->StartAct( flStartTime );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int GetCurrentActNumber( void )
{
	if ( g_hCurrentAct )
		return g_hCurrentAct->GetActNumber();
	return ACT_NONE_SPECIFIED;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the current act (if any) is a waiting act.
//-----------------------------------------------------------------------------
bool CurrentActIsAWaitingAct( void )
{
	if ( g_hCurrentAct )
		return g_hCurrentAct->IsAWaitingAct();

	return false;
}