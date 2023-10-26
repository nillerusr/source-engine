//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates train data for escort gametype
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_team_train_watcher.h"
#include "igameevents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT_NOBASE(C_TeamTrainWatcher, DT_TeamTrainWatcher, CTeamTrainWatcher)

	RecvPropFloat( RECVINFO( m_flTotalProgress ) ),
	RecvPropInt( RECVINFO( m_iTrainSpeedLevel ) ),
	RecvPropFloat( RECVINFO( m_flRecedeTime ) ),
	RecvPropInt( RECVINFO( m_nNumCappers ) ),

END_RECV_TABLE()

C_TeamTrainWatcher *g_pTrainWatcher = NULL;

C_TeamTrainWatcher::C_TeamTrainWatcher()
{
	g_pTrainWatcher = this;

	// force updates when we get our baseline
	m_iTrainSpeedLevel = -2;
	m_flTotalProgress = -1;
	m_flRecedeTime = -1;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TeamTrainWatcher::~C_TeamTrainWatcher()
{
	if ( g_pTrainWatcher == this )
	{
		g_pTrainWatcher = NULL;
	}
}

void C_TeamTrainWatcher::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	m_iOldTrainSpeedLevel = m_iTrainSpeedLevel;
	m_flOldProgress = m_flTotalProgress;
	m_flOldRecedeTime = m_flRecedeTime;
	m_nOldNumCappers = m_nNumCappers;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TeamTrainWatcher::OnDataChanged( DataUpdateType_t updateType )
{
	if ( m_iOldTrainSpeedLevel != m_iTrainSpeedLevel || m_nOldNumCappers != m_nNumCappers )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "escort_speed" );
		if ( event )
		{
			event->SetInt( "speed", m_iTrainSpeedLevel );
			event->SetInt( "players", m_nNumCappers );
			gameeventmanager->FireEventClientSide( event );
		}
	}

	if ( m_flOldProgress != m_flTotalProgress )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "escort_progress" );
		if ( event )
		{
			event->SetFloat( "progress", m_flTotalProgress );

			if ( m_flOldProgress <= -1 )
			{
				event->SetBool( "reset", true );
			}

			gameeventmanager->FireEventClientSide( event );
		}
	}

	if ( m_flOldRecedeTime != m_flRecedeTime )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "escort_recede" );
		if ( event )
		{
			event->SetFloat( "recedetime", m_flRecedeTime );
			gameeventmanager->FireEventClientSide( event );
		}
	}
}