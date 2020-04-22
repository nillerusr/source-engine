//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: steam state machine that handles authenticating steam users
//
//=============================================================================//

#ifndef CL_STEAMUAUTH_H
#define CL_STEAMUAUTH_H
#ifdef _WIN32
#pragma once
#endif

#include "baseclient.h"
#include "utlvector.h"
#include "netadr.h"

#include "steam/steam_api.h"

class CSteam3Client : public CSteamAPIContext
{
public:
	CSteam3Client();
	~CSteam3Client();

	void Activate();
	void Shutdown();

	void GetAuthSessionTicket( void *pTicket, int cbMaxTicket, uint32 *pcbTicket, uint32 unIP, uint16 usPort, uint64 unGSSteamID, bool bSecure );
	void CancelAuthTicket();

	bool BGSSecure() { return m_bGSSecure; }
	void RunFrame();
#if !defined(NO_STEAM)
	STEAM_CALLBACK( CSteam3Client, OnClientGameServerDeny, ClientGameServerDeny_t, m_CallbackClientGameServerDeny );
	STEAM_CALLBACK( CSteam3Client, OnGameServerChangeRequested, GameServerChangeRequested_t, m_CallbackGameServerChangeRequested );
	STEAM_CALLBACK( CSteam3Client, OnGameOverlayActivated, GameOverlayActivated_t, m_CallbackGameOverlayActivated );
	STEAM_CALLBACK( CSteam3Client, OnPersonaUpdated, PersonaStateChange_t, m_CallbackPersonaStateChanged );
	STEAM_CALLBACK( CSteam3Client, OnLowBattery, LowBatteryPower_t, m_CallbackLowBattery );
#endif

private:

	//
	// Cached data for active ticket, if any
	//
	HAuthTicket m_hAuthTicket;
	uint32 m_unIP;
	uint16 m_usPort;
	bool m_bActive;
	bool m_bGSSecure;
	CSteamID m_steamIDGS;
	uint32 m_nTicketSize;
	unsigned char m_arbTicketData[ 1024 ];
};

#ifndef SWDS
// singleton accessor
CSteam3Client &Steam3Client();
#endif // SWDS

#endif // CL_STEAMUAUTH_H
