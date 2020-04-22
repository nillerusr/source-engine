//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles host migration for a session (not for the game server)
//
//=============================================================================//

#include "matchmaking.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Start a Matchmaking session as the host 
//-----------------------------------------------------------------------------
CClientInfo *CMatchmaking::SelectNewHost()
{
	// For now, just grab the first guy in the list
	CClientInfo *pClient = &m_Local;
	for ( int i = 0; i < m_Remote.Count(); ++i )
	{
		if ( m_Remote[i]->m_id > pClient->m_id )
		{
			pClient = m_Remote[i];
		}
	}
	return pClient;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMatchmaking::StartHostMigration()
{
	SwitchToState( MMSTATE_HOSTMIGRATE_STARTINGMIGRATION );

	m_pNewHost = SelectNewHost();
	if ( m_pNewHost == &m_Local )
	{
		// We're the new host, so start hosting
		Msg( "Starting new host" );
		BeginHosting();
	}
	else
	{
		Msg( "Waiting for a new host" );
		SwitchToNewHost();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMatchmaking::BeginHosting()
{
	m_Session.SetIsHost( true );
	m_Host = m_Local;

	// Move into private slots
	if ( !m_Local.m_bInvited )
	{
		RemovePlayersFromSession( &m_Local );
		m_Local.m_bInvited = true;
		AddPlayersToSession( &m_Local );
	}

	if ( !m_Session.MigrateHost() )
	{
		Warning( "Session migrate failed!\n" );

		SessionNotification( SESSION_NOTIFY_FAIL_MIGRATE );
		return;
	}

	SwitchToState( MMSTATE_HOSTMIGRATE_MIGRATING );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMatchmaking::TellClientsToMigrate()
{
	Msg( "Sending migrate request\n" );

	XSESSION_INFO info;
	m_Session.GetNewSessionInfo( &info );

	MM_Migrate msg;
	msg.m_MsgType = MM_Migrate::MESSAGE_HOSTING;
	msg.m_Id = m_Local.m_id;
	msg.m_sessionId = info.sessionID; 
	msg.m_xnaddr = info.hostAddress;  
	msg.m_key = info.keyExchangeKey;

	for ( int i = 0; i < m_Remote.Count(); ++i )
	{
		if ( m_Remote[i]->m_bMigrated )
		{
			continue;
		}

		SendMessage( &msg, &m_Remote[i]->m_adr );
	}

	m_fSendTimer = GetTime();
	++m_nSendCount;
}

//-----------------------------------------------------------------------------
// Purpose: Handle a migration message from our new host
//-----------------------------------------------------------------------------
bool CMatchmaking::ProcessMigrate( MM_Migrate *pMsg )
{
	MM_Migrate reply;
	int type = pMsg->m_MsgType;

	if ( m_CurrentState == MMSTATE_HOSTMIGRATE_WAITINGFORHOST )
	{
		if ( type == MM_Migrate::MESSAGE_HOSTING )
		{
			// Make sure this is the host we were expecting
			if ( !Q_memcmp( &pMsg->m_xnaddr, &m_Host.m_xnaddr, sizeof( m_Host.m_xnaddr ) ) )
			{
				// Reply to the host
				reply.m_MsgType = MM_Migrate::MESSAGE_MIGRATED;
				reply.m_xnaddr = m_Local.m_xnaddr;
				SendMessage( &reply, &m_Host.m_adr );

				XSESSION_INFO info;
				info.sessionID = pMsg->m_sessionId;
				info.hostAddress = pMsg->m_xnaddr;
				info.keyExchangeKey = pMsg->m_key;

				m_Session.SetNewSessionInfo( &info );
				m_Session.SetOwnerId( XUSER_INDEX_NONE );

				if ( !m_Session.MigrateHost() )
				{
					Warning( "Session migrate failed!\n" );

					SessionNotification( SESSION_NOTIFY_FAIL_MIGRATE );
					return true;
				}

				SwitchToState( MMSTATE_HOSTMIGRATE_MIGRATING );
			}
			else
			{
				// Someone else is trying to host
				reply.m_MsgType = MM_Migrate::MESSAGE_STANDBY;
				SendMessage( &reply, &m_Host.m_adr );
			}
		}
	}
	else if ( m_CurrentState == MMSTATE_HOSTMIGRATE_WAITINGFORCLIENTS )
	{
		if ( type == MM_Migrate::MESSAGE_MIGRATED )
		{
			// Flag the client as having migrated
			bool bClientsOutstanding = false;

			for ( int i = 0; i < m_Remote.Count(); ++i )
			{
				if ( m_Remote[i]->m_id == pMsg->m_Id )
				{
					m_Remote[i]->m_bMigrated = true;
				}

				bClientsOutstanding = bClientsOutstanding && m_Remote[i]->m_bMigrated;
			}

			if ( !bClientsOutstanding )
			{
				// Everyone's migrated!
				EndMigration();
			}
		}

		if ( type == MM_Migrate::MESSAGE_STANDBY )
		{
			// Someone requested a standby
			--m_nSendCount;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMatchmaking::SwitchToNewHost()
{
	// Set a timer to wait for the host to contact us
	m_fWaitTimer = GetTime();

	// Get rid of the current host net channel
	MarkChannelForRemoval( &m_Host.m_adr );

	AddRemoteChannel( &m_pNewHost->m_adr );

	SwitchToState( MMSTATE_HOSTMIGRATE_WAITINGFORHOST );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMatchmaking::EndMigration()
{
	Msg( "Migration complete\n" );

	if ( m_Session.IsHost() )
	{
		// Drop any clients that failed to migrate
		for ( int i = m_Remote.Count()-1; i >= 0; --i )
		{
			ClientDropped( m_Remote[i] );
		}

		// Update the lobby to show the new host
		SendPlayerInfoToLobby( &m_Local, 0 );

		// X360TBD: Figure out what state we should be in
		int newState = m_PreMigrateState;
		switch( m_PreMigrateState )
		{
		case MMSTATE_SESSION_CONNECTING:
			newState = MMSTATE_ACCEPTING_CONNECTIONS;
			break;

		default:
			Warning( "Unhandled post-migrate state transition" );
		}

		// Don't use SwitchToState() to set our new state because when changing 
		// from a client to a host the state transition is usually invalid.
		m_CurrentState = newState;
	}
	else
	{
		// Still a client, just restore our previous state
		m_CurrentState =  m_PreMigrateState;
	}
}

void CMatchmaking::TestStats()
{

}
