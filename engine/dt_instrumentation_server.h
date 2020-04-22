//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATATABLE_INSTRUMENTATION_SERVER_H
#define DATATABLE_INSTRUMENTATION_SERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/fasttimer.h"
#include "iservernetworkable.h"


class CDTISendTable;
class SendTable;


// Is instrumentation enabled?
extern bool g_bServerDTIEnabled;


// Types of things it times.
typedef enum
{
	SERVERDTI_CALCDELTA=0,
	SERVERDTI_ENCODE,
	SERVERDTI_SHOULDTRANSMIT,
	SERVERDTI_WRITE_DELTA_PROPS
} ServerDTITimerType;



// ------------------------------------------------------------------------------------------ // 
// Instrumentation functions.
// ------------------------------------------------------------------------------------------ // 

// This is called at startup to enable or disable instrumentation.
// If pFilename is null, no instrumentation is performed.
void ServerDTI_Init( char const *pFilename );

// This calls ServerDTI_Flush and cleans up.
void ServerDTI_Term();

// This writes out the instrumentation file.
void ServerDTI_Flush();

// Setup instrumentation on a CRecvDecoder.
CDTISendTable* ServerDTI_HookTable( SendTable *pTable );

void ServerDTI_AddEntityEncodeEvent( SendTable *pTable, float distToPlayer );

// Used to tell if the entity is using manual or auto mode.
void ServerDTI_RegisterNetworkStateChange( SendTable *pTable, bool bStateChanged );


// ------------------------------------------------------------------------------------------ // 
// Helper class to place timers easily.
// ------------------------------------------------------------------------------------------ // 

class CServerDTITimer
{
public:
				CServerDTITimer( const SendTable *pTable, ServerDTITimerType type );
				~CServerDTITimer();

private:

	const SendTable		*m_pTable;
	ServerDTITimerType	m_Type;
	CFastTimer			m_Timer;
};

inline CServerDTITimer::CServerDTITimer( const SendTable *pTable, ServerDTITimerType type )
{
	if ( g_bServerDTIEnabled )
	{
		m_pTable = pTable;
		m_Type = type;
		m_Timer.Start();
	}
}

inline CServerDTITimer::~CServerDTITimer()
{
	if ( g_bServerDTIEnabled && m_pTable )
	{
		m_Timer.End();
		extern void _ServerDTI_HookTimer( const SendTable *pTable, ServerDTITimerType timerType, CCycleCount const &count );
		_ServerDTI_HookTimer( m_pTable, m_Type, m_Timer.GetDuration() );
	}
}

inline void ServerDTI_RegisterNetworkStateChange( SendTable *pTable, bool bStateChanged )
{
	if ( g_bServerDTIEnabled )
	{
		extern void _ServerDTI_RegisterNetworkStateChange( SendTable *pTable, bool bStateChanged );
		_ServerDTI_RegisterNetworkStateChange( pTable, bStateChanged );
	}
}

#endif // DATATABLE_INSTRUMENTATION_SERVER_H
