//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: steam state machine that handles authenticating steam users
//
//=============================================================================//
#ifdef _WIN32
#if !defined( _X360 )
#include "winlite.h"
#include <winsock2.h> // INADDR_ANY defn
#endif
#elif POSIX
#include <netinet/in.h>
#endif

#include <utlbuffer.h>
#include "cl_steamauth.h"
#include "interface.h"
#include "filesystem_engine.h"
#include "tier0/icommandline.h"
#include "tier0/vprof.h"
#include "host.h"
#include "cmd.h"
#include "common.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif

#pragma warning( disable: 4355 ) // disables ' 'this' : used in base member initializer list'

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CSteam3Client s_Steam3Client;
CSteam3Client  &Steam3Client()
{
	return s_Steam3Client;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSteam3Client::CSteam3Client() 
#if !defined(NO_STEAM)
:
			m_CallbackClientGameServerDeny( this, &CSteam3Client::OnClientGameServerDeny ),
			m_CallbackGameServerChangeRequested( this, &CSteam3Client::OnGameServerChangeRequested ),
			m_CallbackGameOverlayActivated( this, &CSteam3Client::OnGameOverlayActivated ),
			m_CallbackPersonaStateChanged( this, &CSteam3Client::OnPersonaUpdated ),
			m_CallbackLowBattery( this, &CSteam3Client::OnLowBattery )
#endif
{
	m_bActive = false;
	m_bGSSecure = false;
	m_hAuthTicket = k_HAuthTicketInvalid;
	m_unIP = 0;
	m_usPort = 0;
	m_nTicketSize = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSteam3Client::~CSteam3Client()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Unload the steam3 engine
//-----------------------------------------------------------------------------
void CSteam3Client::Shutdown()
{	
	if ( !m_bActive )
		return;

	m_bActive = false;	
#if !defined( NO_STEAM )
	SteamAPI_Shutdown();
	Clear(); // clear our interface pointers now they are invalid
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Initialize the steam3 connection
//-----------------------------------------------------------------------------
void CSteam3Client::Activate()
{
	if ( m_bActive )
		return;

	m_bActive = true;
	m_bGSSecure = false;

#if !defined( NO_STEAM )
	SteamAPI_InitSafe(); // ignore failure, that will fall out later when they don't get a valid logon cookie
	SteamAPI_SetTryCatchCallbacks( false ); // We don't use exceptions, so tell steam not to use try/catch in callback handlers
	Init(); // Steam API context init
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Get the steam3 logon cookie to use
//-----------------------------------------------------------------------------
void CSteam3Client::GetAuthSessionTicket( void *pTicket, int cbMaxTicket, uint32 *pcbTicket, uint32 unIP, uint16 usPort, uint64 unGSSteamID,  bool bSecure )
{
#ifdef NO_STEAM
	m_bGSSecure = bSecure;
#else
	CSteamID steamIDGS( unGSSteamID );

	// Assume failure
	*pcbTicket = 0;

	// We must have interface pointers
	if ( !SteamUser() )
	{
		Warning( "No SteamUser interface.  Cannot perform steam authentication\n" );
		return;
	}

	// Make sure we have a valid Steam ID
	CSteamID steamID = SteamUser()->GetSteamID();
	if ( !steamID.IsValid() )
	{
		Warning( "Our steam ID %s is not valid.  Steam must be running and you must be logged in\n", steamID.Render() );
		return;
	}

	// Get a new ticket, if we don't already have one for this server
	if ( m_hAuthTicket == k_HAuthTicketInvalid
		|| m_unIP != unIP
		|| m_usPort != usPort
		|| m_bGSSecure != bSecure
		|| m_steamIDGS != steamIDGS
		|| m_nTicketSize <= 0 )
	{

		// Cancel any previously issues ticket
		if ( m_hAuthTicket != k_HAuthTicketInvalid )
			SteamUser()->CancelAuthTicket( m_hAuthTicket );

		// Shove the GS ID in the first bits
		*(uint64*)m_arbTicketData = SteamUser()->GetSteamID().ConvertToUint64();

		// Ask Steam for a ticket
		m_nTicketSize = 0;
		m_hAuthTicket = SteamUser()->GetAuthSessionTicket( m_arbTicketData+sizeof(uint64), sizeof(m_arbTicketData)-sizeof(uint64), &m_nTicketSize );
		if ( m_hAuthTicket == k_HAuthTicketInvalid || m_nTicketSize <= 0 )
		{
			// Failed!
			Assert( m_hAuthTicket != k_HAuthTicketInvalid );
			Assert( m_nTicketSize > 0 );
			m_hAuthTicket = k_HAuthTicketInvalid;
			m_nTicketSize = 0;
			Warning( "ISteamUser::GetAuthSessionTicket failed to return a valid ticket\n" );
		}
		else
		{
			// Got valid ticket.  Remember its properties
			m_nTicketSize += sizeof(uint64);
			m_unIP = unIP;
			m_usPort = usPort;
			m_bGSSecure = bSecure;
			m_steamIDGS = steamIDGS;
		}
	}

	// Give them back the ticket data, if we were able to get one, and it will fit
	*pcbTicket = 0;
	if ( m_nTicketSize > 0 )
	{
		if ( cbMaxTicket >= (int)m_nTicketSize )
		{
			memcpy( pTicket, m_arbTicketData, m_nTicketSize );
			*pcbTicket = m_nTicketSize;
		}
		else
		{
			Assert( cbMaxTicket >= (int)m_nTicketSize );
		}
	}

	// Tell the steam backend about the server we are playing on - so that it can broadcast this to our friends and they can join.
	// This may be something that you should NOT do if you are on a listen server - or you know that the game is not joinable
	// for some reason.
	#ifdef _DEBUG
		Msg( "Sending AdvertiseGame %s %s (%s)\n", steamIDGS.Render(), netadr_t(unIP, usPort).ToString(), m_bGSSecure ? "secure" : "insecure" );
	#endif
	SteamUser()->AdvertiseGame( steamIDGS, unIP, usPort );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Tell steam that we are leaving a server
//-----------------------------------------------------------------------------
void CSteam3Client::CancelAuthTicket()
{
	m_bGSSecure = false;
	if ( !SteamUser() )
		return;

#if !defined( NO_STEAM )
	if ( m_hAuthTicket != k_HAuthTicketInvalid )
		SteamUser()->CancelAuthTicket( m_hAuthTicket );
	#ifdef _DEBUG
		Msg( "Clearing auth ticket, sending void AdvertiseGame\n" );
	#endif
	CSteamID steamIDGS; // invalid steamID means not playing anywhere
	SteamUser()->AdvertiseGame( steamIDGS, 0, 0 );
	m_hAuthTicket = k_HAuthTicketInvalid;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Process any callbacks we may have
//-----------------------------------------------------------------------------
void CSteam3Client::RunFrame()
{
#if !defined( NO_STEAM )
	VPROF_BUDGET( "CSteam3Client::RunFrame", VPROF_BUDGETGROUP_STEAM );
	SteamAPI_RunCallbacks();
#endif
}


#if !defined(NO_STEAM)
//-----------------------------------------------------------------------------
// Purpose: Disconnect the user from their current server
//-----------------------------------------------------------------------------
void CSteam3Client::OnClientGameServerDeny( ClientGameServerDeny_t *pClientGameServerDeny )
{
	if ( pClientGameServerDeny->m_uAppID == GetSteamAppID() )
	{
		const char *pszReason = "Unknown";
		switch ( pClientGameServerDeny->m_uReason )
		{
			case ( k_EDenyInvalidVersion ) : pszReason = "Invalid version"; break;
			case ( k_EDenyGeneric ) : pszReason = "Kicked"; break;
			case ( k_EDenyNotLoggedOn ) : pszReason = "Not logged on"; break;
			case ( k_EDenyNoLicense ) : pszReason = "No license"; break;
			case ( k_EDenyCheater ) : pszReason = "VAC banned "; break;
			case ( k_EDenyLoggedInElseWhere ) : pszReason = "Dropped from server"; break;
			case ( k_EDenyUnknownText ) : pszReason = "Unknown"; break;
			case ( k_EDenyIncompatibleAnticheat ) : pszReason = "Incompatible Anti Cheat"; break;
			case ( k_EDenyMemoryCorruption ) : pszReason = "Memory corruption"; break;
			case ( k_EDenyIncompatibleSoftware ) : pszReason = "Incompatible software"; break;
			case ( k_EDenySteamConnectionLost ) : pszReason = "Steam connection lost"; break;
			case ( k_EDenySteamConnectionError ) : pszReason = "Steam connection error"; break;
			case ( k_EDenySteamResponseTimedOut ) : pszReason = "Response timed out"; break;
			case ( k_EDenySteamValidationStalled ) : pszReason = "Verification failed"; break;
		}

		Warning( "Disconnect: %s\n", pszReason );

		Host_Disconnect( true );
	}
	
}


extern ConVar	password;
//-----------------------------------------------------------------------------
// Purpose: Disconnect the user from their current server
//-----------------------------------------------------------------------------
void CSteam3Client::OnGameServerChangeRequested( GameServerChangeRequested_t *pGameServerChangeRequested )
{
	password.SetValue( pGameServerChangeRequested->m_rgchPassword );
	Msg( "Connecting to %s\n", pGameServerChangeRequested->m_rgchServer );
	Cbuf_AddText( va( "connect %s steam\n", pGameServerChangeRequested->m_rgchServer ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Client::OnGameOverlayActivated( GameOverlayActivated_t *pGameOverlayActivated )
{
#ifndef SWDS
	if ( Host_IsSinglePlayerGame() )
	{
		if ( !EngineVGui()->IsGameUIVisible() && 
			!EngineVGui()->IsConsoleVisible() )
		{
			if ( pGameOverlayActivated->m_bActive )
				Cbuf_AddText( "setpause" );
			else
				Cbuf_AddText( "unpause" );
		}
	}
#endif
}

extern void UpdateNameFromSteamID( IConVar *pConVar, CSteamID *pSteamID );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Client::OnPersonaUpdated( PersonaStateChange_t *pPersonaStateChanged )
{
	if ( pPersonaStateChanged->m_nChangeFlags & k_EPersonaChangeName )
	{
		if ( SteamUtils() && SteamFriends() && SteamUser() )
		{
			CSteamID steamID = SteamUser()->GetSteamID();
			IConVar *pConVar = g_pCVar->FindVar( "name" );
			UpdateNameFromSteamID( pConVar, &steamID );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Client::OnLowBattery( LowBatteryPower_t *pLowBat )
{
	// on the 9min, 5 min and 1 min warnings tell the engine to fire off a save
	switch(  pLowBat->m_nMinutesBatteryLeft )
	{
	case 9: 
	case 5: 
	case 1: 
		Cbuf_AddText( "save LowBattery_AutoSave" );
		break;

	default:
		break;
	}
}

#endif
