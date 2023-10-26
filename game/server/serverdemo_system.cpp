//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "serverdemo_system.h"
#include "serverdemo_types.h"
#include "vstdlib/IKeyValuesSystem.h"
#include "tier1/circularbuffer.h"
#include "toolutils/enginetools_int.h"
#include "toolframework/itoolframework.h"
#include "toolframework/iserverenginetools.h"
#include "serverdemo.h"

//------------------------------------------------------------------------------------------------------------------------

bool g_bAllowServerDemoWrite = true;

//------------------------------------------------------------------------------------------------------------------------

ConVar sv_demo_entity_record_rate( "sv_demo_entity_record_rate", "30", FCVAR_GAMEDLL | FCVAR_SPONLY | FCVAR_CHEAT, "Set the server demo record rate for entities." );

//------------------------------------------------------------------------------------------------------------------------

class CServerDemoSystem : public IServerDemoSystem		// Implementation
{
public:
	CServerDemoSystem();

	bool Init();
	void Shutdown();

	virtual void WriteDemoToDiskForClient( int iClient, char const* pFilename );

	virtual void PostRecordingMessage( KeyValues* pMsg );

	virtual void Think();

	virtual void OnInitLevel( char const* pMapName );
	virtual void OnShutdownLevel();

	bool CreateDemo( char const* pMapName );
	void FreeDemo();

	CServerDemo*		m_pDemo;
	float				m_flLastEntRecordTime;
	int					m_nLastRecordSecond;
	int					m_nFrameCount;
};

//------------------------------------------------------------------------------------------------------------------------

CServerDemoSystem::CServerDemoSystem()
{
	Init();
}

bool CServerDemoSystem::Init()
{
	m_pDemo = NULL;
	m_flLastEntRecordTime = 0.0f;
	m_nFrameCount = 0;
	m_nLastRecordSecond = 0;
	return true;
}

void CServerDemoSystem::Shutdown()
{
	if ( !m_pDemo )
		return;

	FreeDemo();
}

void CServerDemoSystem::FreeDemo()
{
	delete m_pDemo;
	m_pDemo = NULL;
}

bool CServerDemoSystem::CreateDemo( char const* pMapName )
{
	m_pDemo = new CServerDemo();
	if ( !m_pDemo )
		return false;

	return m_pDemo->Init( pMapName, gpGlobals->curtime );
}

void CServerDemoSystem::PostRecordingMessage( KeyValues* pMsg )
{
	if ( !m_pDemo )
		return;

	m_pDemo->PostRecordingMessage( pMsg, gpGlobals->curtime );
}

void CServerDemoSystem::Think()
{
	if ( !g_bAllowServerDemoWrite )
		return;

	if ( !m_pDemo || !m_pDemo->m_pBuffer )
		return;

	// Write how much of buffer has been used
	engine->Con_NPrintf( 0, "%% circular buffer used: %2f", (float)(m_pDemo->m_pBuffer->GetSize() - m_pDemo->m_pBuffer->GetWriteAvailable()) / m_pDemo->m_pBuffer->GetSize() );

	// Write entities?
	float flRecordRate = MAX( 20.0f, MIN( 60.0f, sv_demo_entity_record_rate.GetFloat() ) );
	if ( gpGlobals->curtime - m_flLastEntRecordTime < 1.0f/flRecordRate )
		return;

	int const iCurSecond = (int)gpGlobals->curtime;
	if ( iCurSecond != m_nLastRecordSecond )
	{
//		DevMsg( "%d: frames record: %d\n", m_lastRecordSecond, m_frameCount );

		m_nLastRecordSecond = iCurSecond;
		m_nFrameCount = 0;
	}
	else
	{
		++m_nFrameCount;
	}
	
	for ( CBaseEntity *pEntity = gEntList.FirstEnt(); pEntity != NULL; pEntity = gEntList.NextEnt(pEntity) )
	{
		if ( !pEntity )
			continue;

		KeyValues* pMsg = new KeyValues( "entity" );

		// Store server demo ptr
		pMsg->SetPtr( "serverdemo", m_pDemo );

		// Fill msg with state data - only post message if state changed
		if ( pEntity->GetDemoRecordingState( pMsg ) )
		{
			ServerDemoPacket_BaseEntity* pBaseEntPacket = (ServerDemoPacket_BaseEntity*)pMsg->GetPtr( "baseentity" );
			if ( pBaseEntPacket )
			{
				matrix3x4_t m;
				AngleMatrix( pEntity->GetAbsAngles(), pEntity->GetAbsOrigin(), m );

				ServerDemoPacket_BaseAnimating* pBaseAnimatingPacket = (ServerDemoPacket_BaseAnimating*)pMsg->GetPtr( "baseanimating" );
				ServerDemoPacket_BaseAnimatingOverlay* pBaseAnimatingPacketOverlay = (ServerDemoPacket_BaseAnimatingOverlay*)pMsg->GetPtr( "baseanimatingoverlay" );

				if ( ( pBaseEntPacket->m_fModified != 0 ) ||
					 ( pBaseAnimatingPacket && pBaseAnimatingPacket->m_fModified ) ||
					 ( pBaseAnimatingPacketOverlay && pBaseAnimatingPacketOverlay->m_fModified ) )
				{
					debugoverlay->AddCoordFrameOverlay( m, 25 );
					pBaseEntPacket->AddTextOverlaysForModifiedFields( pEntity->GetAbsOrigin() );

					// Add BaseAnimatin text
					if ( pBaseAnimatingPacket )
					{
						pBaseAnimatingPacket->AddTextOverlaysForModifiedFields( pEntity->GetAbsOrigin() );
					}

					// Add BaseAnimatinOverlay text
					if ( pBaseAnimatingPacketOverlay )
					{
						pBaseAnimatingPacketOverlay->AddTextOverlaysForModifiedFields( pEntity->GetAbsOrigin() );
					}
				}
			}

			// Post a message to the demo system
			g_pServerDemoSystem->PostRecordingMessage( pMsg );

			// No longer first frame
			pEntity->m_bFirstRecordingFrame = false;
		}

		pMsg->deleteThis();
	}

	// Stamp record time
	m_flLastEntRecordTime = gpGlobals->curtime;
}

void CServerDemoSystem::WriteDemoToDiskForClient( int iClient, char const* pFilename )
{
	// TODO: Send the circular buffer to SFM for save to file
	if ( !serverenginetools->SFM_WriteServerDemoFile( pFilename, m_pDemo ) )
	{
		Warning( "Failed to write server demo file, %s\n", pFilename );
	}
}

void CServerDemoSystem::OnInitLevel( char const* pMapName )
{
	g_bAllowServerDemoWrite = true;

	FreeDemo();

	Init();

	if ( !CreateDemo( pMapName ) )
	{
		Warning( "Failed to create server demo\n" );
		FreeDemo();
	}
}

void CServerDemoSystem::OnShutdownLevel()
{
	g_bAllowServerDemoWrite = false;

	FreeDemo();
}

//------------------------------------------------------------------------------------------------------------------------

IServerDemoSystem* g_pServerDemoSystem = NULL;
static CServerDemoSystem g_serverDemoSystem;

//------------------------------------------------------------------------------------------------------------------------

bool ServerDemoSystem_Init()
{
	Assert( !g_pServerDemoSystem );

	// Setup the interface
	g_pServerDemoSystem = &g_serverDemoSystem;

	// Init system
	return g_serverDemoSystem.Init();		// TODO: Should be passed in or accessed from command line, etc.
}

void ServerDemoSystem_Shutdown()
{
	if ( g_pServerDemoSystem )
	{
		g_serverDemoSystem.Shutdown();
	}
}

//------------------------------------------------------------------------------------------------------------------------

CON_COMMAND( dump_server_demo, "dump_sever_demo <filename>" )
{
	if ( !g_bAllowServerDemoWrite )
	{
		DevMsg( "Server demo not allowed now.\n" );
		return;
	}

	if ( args.ArgC() != 2 )
	{
		DevMsg( "Please specify an output filename.\n" );
		return;
	}

	if ( !g_pServerDemoSystem )
	{
		DevMsg( "Server demo system not initialized!\n" );
		return;
	}

	g_bAllowServerDemoWrite = false;

		// Use dummy client id for now.
		g_pServerDemoSystem->WriteDemoToDiskForClient( -1, args[1] );

	g_bAllowServerDemoWrite = true;
}

//------------------------------------------------------------------------------------------------------------------------
