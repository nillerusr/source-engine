//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"
#include "clientsteamcontext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CClientSteamContext g_ClientSteamContext;
CClientSteamContext  &ClientSteamContext()
{
	return g_ClientSteamContext;
}

CSteamAPIContext *steamapicontext = &g_ClientSteamContext;

//-----------------------------------------------------------------------------
CClientSteamContext::CClientSteamContext() 
#if !defined(NO_STEAM)
:
	m_CallbackSteamServersDisconnected( this, &CClientSteamContext::OnSteamServersDisconnected ),
	m_CallbackSteamServerConnectFailure( this, &CClientSteamContext::OnSteamServerConnectFailure ),
	m_CallbackSteamServersConnected( this, &CClientSteamContext::OnSteamServersConnected )
#ifdef TF_CLIENT_DLL
	, m_GameJoinRequested( this, &CClientSteamContext::OnGameJoinRequested )
#endif // TF_CLIENT_DLL
#endif
{
	m_bActive = false;
	m_bLoggedOn = false;
	m_nAppID = 0;
}


//-----------------------------------------------------------------------------
CClientSteamContext::~CClientSteamContext()
{
}


//-----------------------------------------------------------------------------
// Purpose: Unload the steam3 engine
//-----------------------------------------------------------------------------
void CClientSteamContext::Shutdown()
{	
	if ( !m_bActive )
		return;

	m_bActive = false;
	m_bLoggedOn = false;
#if !defined( NO_STEAM )
	Clear(); // Steam API context shutdown
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the steam3 connection
//-----------------------------------------------------------------------------
void CClientSteamContext::Activate()
{
	if ( m_bActive )
		return;

	m_bActive = true;

#if !defined( NO_STEAM )
	SteamAPI_InitSafe(); // ignore failure, that will fall out later when they don't get a valid logon cookie
	SteamAPI_SetTryCatchCallbacks( false ); // We don't use exceptions, so tell steam not to use try/catch in callback handlers
	Init(); // Steam API context init
	
	UpdateLoggedOnState();
	Msg( "CClientSteamContext logged on = %d\n", m_bLoggedOn );
#endif
}

void CClientSteamContext::UpdateLoggedOnState()
{
	bool bPreviousLoggedOn = m_bLoggedOn;
	m_bLoggedOn = ( SteamUser() && SteamUtils() && SteamUser()->BLoggedOn() );

	if ( !bPreviousLoggedOn && m_bLoggedOn )
	{
		// update Steam info
		m_SteamIDLocalPlayer = SteamUser()->GetSteamID();
		m_nUniverse = SteamUtils()->GetConnectedUniverse();
		m_nAppID = SteamUtils()->GetAppID();
	}

	if ( bPreviousLoggedOn != m_bLoggedOn )
	{
		// Notify any listeners of the change in logged on state
		SteamLoggedOnChange_t loggedOnChange;
		loggedOnChange.bPreviousLoggedOn = bPreviousLoggedOn;
		loggedOnChange.bLoggedOn = m_bLoggedOn;
		InvokeCallbacks( loggedOnChange );
	}
}

#if !defined(NO_STEAM)
void CClientSteamContext::OnSteamServersDisconnected( SteamServersDisconnected_t *pDisconnected )
{
	UpdateLoggedOnState();
	Msg( "CClientSteamContext OnSteamServersDisconnected logged on = %d\n", m_bLoggedOn );
}

void CClientSteamContext::OnSteamServerConnectFailure( SteamServerConnectFailure_t *pConnectFailure )
{
	UpdateLoggedOnState();
	Msg( "CClientSteamContext OnSteamServerConnectFailure logged on = %d\n", m_bLoggedOn );
}

void CClientSteamContext::OnSteamServersConnected( SteamServersConnected_t *pConnected )
{
	UpdateLoggedOnState();
	Msg( "CClientSteamContext OnSteamServersConnected logged on = %d\n", m_bLoggedOn );
}

#ifdef TF_CLIENT_DLL
void CClientSteamContext::OnGameJoinRequested( GameRichPresenceJoinRequested_t *pCallback )
{
	if ( pCallback && pCallback->m_rgchConnect && ( pCallback->m_rgchConnect[0] == '+' ) )
	{
		char const *szConCommand = pCallback->m_rgchConnect + 1;
		//
		// Work around Steam Overlay bug that it doesn't replace %20 characters
		//
		CFmtStr fmtCommand;
		if ( StringHasPrefix( szConCommand, "tf_econ_item_preview%20" ) )
		{
			fmtCommand.AppendFormat( "%s", szConCommand );
			while ( char *pszReplace = strstr( fmtCommand.Access(), "%20" ) )
			{
				*pszReplace = ' ';
				Q_memmove( pszReplace + 1, pszReplace + 3, Q_strlen( pszReplace + 3 ) + 1 );
			}
			szConCommand = fmtCommand.Access();
		}
		//
		// End of Steam Overlay bug workaround
		//
		if ( char const *szItemId = StringAfterPrefix( szConCommand, "tf_econ_item_preview " ) )
		{
			Msg( "CClientSteamContext OnGameJoinRequested tf_econ_item_preview" );

			bool bItemIdValid = ( pCallback->m_steamIDFriend.GetAccountID() == ~0u );
			while ( *szItemId )
			{
				if ( ( ( *szItemId >= '0' ) && ( *szItemId <= '9' ) ) ||
					( ( *szItemId >= 'A' ) && ( *szItemId <= 'S' ) ) )
					++szItemId;	// support new encoding for owner steamid and assetid
				else
				{
					bItemIdValid = false;
					break;
				}
			}
			if ( bItemIdValid )
			{
				engine->ClientCmd( szConCommand );
			}
		}
	}
}
#endif // TF_CLIENT_DLL

#endif // !defined(NO_STEAM)

void CClientSteamContext::InstallCallback( CUtlDelegate< void ( const SteamLoggedOnChange_t & ) > delegate )
{
	m_LoggedOnCallbacks.AddToTail( delegate );
}

void CClientSteamContext::RemoveCallback( CUtlDelegate< void ( const SteamLoggedOnChange_t & ) > delegate )
{
	m_LoggedOnCallbacks.FindAndRemove( delegate );
}

void CClientSteamContext::InvokeCallbacks( const SteamLoggedOnChange_t &loggedOnStatus )
{
	for ( int i = 0; i < m_LoggedOnCallbacks.Count(); ++i )
	{
		m_LoggedOnCallbacks[i]( loggedOnStatus );
	}
}