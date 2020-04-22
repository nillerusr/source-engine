//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// hltvserver.cpp: implementation of the CHLTVServer class.
//
//////////////////////////////////////////////////////////////////////

#include <server_class.h>
#include <inetmessage.h>
#include <tier0/vprof.h>
#include <tier0/vcrmode.h>
#include <KeyValues.h>
#include <edict.h>
#include <eiface.h>
#include <PlayerState.h>
#include <ihltvdirector.h>
#include <time.h>

#include "hltvserver.h"
#include "sv_client.h"
#include "hltvclient.h"
#include "server.h"
#include "sv_main.h"
#include "framesnapshot.h"
#include "networkstringtable.h"
#include "cmodel_engine.h"
#include "dt_recv_eng.h"
#include "cdll_engine_int.h"
#include "GameEventManager.h"
#include "host.h"
#include "proto_version.h"
#include "proto_oob.h"
#include "dt_common_eng.h"
#include "baseautocompletefilelist.h"
#include "sv_steamauth.h"
#include "tier0/icommandline.h"
#include "sys_dll.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define S2A_EXTRA_DATA_HAS_GAME_PORT				0x80		// Next 2 bytes include the game port.
#define S2A_EXTRA_DATA_HAS_SPECTATOR_DATA			0x40		// Next 2 bytes include the spectator port, then the spectator server name.
#define S2A_EXTRA_DATA_HAS_GAMETAG_DATA				0x20		// Next bytes are the game tag string
#define S2A_EXTRA_DATA_HAS_STEAMID					0x10		// Next 8 bytes are the steamID
#define S2A_EXTRA_DATA_GAMEID						0x01		// Next 8 bytes are the gameID of the server

#define A2S_KEY_STRING_STEAM		"Source Engine Query" // required postfix to a A2S_INFO query

extern CNetworkStringTableContainer *networkStringTableContainerClient;
extern ConVar sv_tags;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CHLTVServer *hltv = NULL;

static void tv_title_changed_f( IConVar *var, const char *pOldString, float flOldValue )
{
	if ( hltv && hltv->IsActive() )
	{
		hltv->BroadcastLocalTitle();
	}
}

static void tv_name_changed_f( IConVar *var, const char *pOldValue, float flOldValue )
{
	Steam3Server().NotifyOfServerNameChange();
}

static ConVar tv_maxclients( "tv_maxclients", "128", 0, "Maximum client number on SourceTV server.",
							  true, 0, true, 255 );

ConVar tv_autorecord( "tv_autorecord", "0", 0, "Automatically records all games as SourceTV demos." );
ConVar tv_name( "tv_name", "SourceTV", 0, "SourceTV host name", tv_name_changed_f );
static ConVar tv_password( "tv_password", "", FCVAR_NOTIFY | FCVAR_PROTECTED | FCVAR_DONTRECORD, "SourceTV password for all clients" );

static ConVar tv_overridemaster( "tv_overridemaster", "0", 0, "Overrides the SourceTV master root address." );
static ConVar tv_dispatchmode( "tv_dispatchmode", "1", 0, "Dispatch clients to relay proxies: 0=never, 1=if appropriate, 2=always" );
ConVar tv_transmitall( "tv_transmitall", "0", FCVAR_REPLICATED, "Transmit all entities (not only director view)" );
ConVar tv_debug( "tv_debug", "0", 0, "SourceTV debug info." );
ConVar tv_title( "tv_title", "SourceTV", 0, "Set title for SourceTV spectator UI", tv_title_changed_f );
static ConVar tv_deltacache( "tv_deltacache", "2", 0, "Enable delta entity bit stream cache" );
static ConVar tv_relayvoice( "tv_relayvoice", "1", 0, "Relay voice data: 0=off, 1=on" );

CDeltaEntityCache::CDeltaEntityCache()
{
	Q_memset( m_Cache, 0, sizeof(m_Cache) );
	m_nTick = 0;
	m_nMaxEntities = 0;
	m_nCacheSize = 0;
}

CDeltaEntityCache::~CDeltaEntityCache()
{
	Flush();
}

void CDeltaEntityCache::Flush()
{
	if ( m_nMaxEntities != 0 )
	{
		// at least one entity was set
		for ( int i=0; i<m_nMaxEntities; i++ )
		{
			if ( m_Cache[i] != NULL )
			{
				free( m_Cache[i] );
				m_Cache[i] = NULL;
			}
		}

		m_nMaxEntities = 0;
	}

	m_nCacheSize = 0;
}

void CDeltaEntityCache::SetTick( int nTick, int nMaxEntities )
{
	if ( nTick == m_nTick )
		return;

	Flush();

	m_nCacheSize = tv_deltacache.GetInt() * 1024;

	if ( m_nCacheSize <= 0 )
		return;

	m_nMaxEntities = min(nMaxEntities,MAX_EDICTS);
	m_nTick = nTick;
}

unsigned char* CDeltaEntityCache::FindDeltaBits( int nEntityIndex, int nDeltaTick, int &nBits )
{
	nBits = -1;
	
	if ( nEntityIndex < 0 || nEntityIndex >= m_nMaxEntities )
		return NULL;

	DeltaEntityEntry_s *pEntry = m_Cache[nEntityIndex];

	while  ( pEntry )
	{
		if ( pEntry->nDeltaTick == nDeltaTick )
		{
			nBits = pEntry->nBits;
			return (unsigned char*)(pEntry) + sizeof(DeltaEntityEntry_s);		
		}
		else
		{
			// keep searching entry list
			pEntry = pEntry->pNext;
		}
	}
	
	return NULL;
}

void CDeltaEntityCache::AddDeltaBits( int nEntityIndex, int nDeltaTick, int nBits, bf_write *pBuffer )
{
	if ( nEntityIndex < 0 || nEntityIndex >= m_nMaxEntities || m_nCacheSize <= 0 )
		return;

	int	nBufferSize = PAD_NUMBER( Bits2Bytes(nBits), 4);

	DeltaEntityEntry_s *pEntry = m_Cache[nEntityIndex];

	if ( pEntry == NULL )
	{
		if ( (int)(nBufferSize+sizeof(DeltaEntityEntry_s)) > m_nCacheSize )
			return;  // way too big, don't even create an entry

		pEntry = m_Cache[nEntityIndex] = (DeltaEntityEntry_s *) malloc( m_nCacheSize );
	}
	else
	{
		char *pEnd = (char*)(pEntry) + m_nCacheSize;	// end marker

		while( pEntry->pNext )
		{
			pEntry = pEntry->pNext;
		}

		int entrySize = sizeof(DeltaEntityEntry_s) + PAD_NUMBER( Bits2Bytes(pEntry->nBits), 4);

		DeltaEntityEntry_s *pNew = (DeltaEntityEntry_s*)((char*)(pEntry) + entrySize);

		if ( ((char*)(pNew) + sizeof(DeltaEntityEntry_s) + nBufferSize) > pEnd )
			return;	// data wouldn't fit into cache anymore, don't add new entries

		pEntry = pNew;
		pEntry->pNext = pEntry;
	}

	pEntry->pNext = NULL; // link to next
	pEntry->nDeltaTick = nDeltaTick;
	pEntry->nBits = nBits;
	
	if ( nBits > 0 )
	{
		bf_read  inBuffer; 
		inBuffer.StartReading( pBuffer->GetData(), pBuffer->m_nDataBytes, pBuffer->GetNumBitsWritten() );
		bf_write outBuffer( (char*)(pEntry) + sizeof(DeltaEntityEntry_s), nBufferSize );
		outBuffer.WriteBitsFromBuffer( &inBuffer, nBits );
	}
}

						  
static RecvTable* FindRecvTable( const char *pName, RecvTable **pRecvTables, int nRecvTables )
{
	for ( int i=0; i< nRecvTables; i++ )
	{
		if ( !Q_strcmp( pName, pRecvTables[i]->GetName() ) )
			return pRecvTables[i];
	}

	return NULL;
}

static RecvTable* AddRecvTableR( SendTable *sendt, RecvTable **pRecvTables, int &nRecvTables )
{
	RecvTable *recvt = FindRecvTable( sendt->m_pNetTableName, pRecvTables, nRecvTables );

	if ( recvt )
		return recvt;	// already in list

	if ( sendt->m_nProps > 0 )
	{
		RecvProp *receiveProps = new RecvProp[sendt->m_nProps];

		for ( int i=0; i < sendt->m_nProps; i++ )
		{
			// copy property data

			SendProp * sp = sendt->GetProp( i );
			RecvProp * rp = &receiveProps[i];

			rp->m_pVarName	= sp->m_pVarName;
			rp->m_RecvType	= sp->m_Type;
			
			if ( sp->IsExcludeProp() )
			{
				// if prop is excluded, give different name
				rp->m_pVarName = "IsExcludedProp";
			}

			if ( sp->IsInsideArray() )
			{
				rp->SetInsideArray();
				rp->m_pVarName = "InsideArrayProp"; // give different name
			}
			
			if ( sp->GetType() == DPT_Array )
			{
				Assert ( sp->GetArrayProp() == sendt->GetProp( i-1 ) );
				Assert( receiveProps[i-1].IsInsideArray() );
				
				rp->SetArrayProp( &receiveProps[i-1] );
				rp->InitArray( sp->m_nElements, sp->m_ElementStride );
			}

			if ( sp->GetType() == DPT_DataTable )
			{
				// recursive create
				Assert ( sp->GetDataTable() );
				RecvTable *subTable = AddRecvTableR( sp->GetDataTable(), pRecvTables, nRecvTables );
				rp->SetDataTable( subTable );
			}
		}

		recvt = new RecvTable( receiveProps, sendt->m_nProps, sendt->m_pNetTableName );
	}
	else
	{
		// table with no properties
		recvt = new RecvTable( NULL, 0, sendt->m_pNetTableName );
	}

	pRecvTables[nRecvTables] = recvt;
	nRecvTables++;

	return recvt;
}

void CHLTVServer::FreeClientRecvTables()
{
	for ( int i=0; i< m_nRecvTables; i++ )
	{
		RecvTable *rt = m_pRecvTables[i];

		// delete recv table props
		if ( rt->m_pProps )
		{
			Assert( rt->m_nProps > 0 );
			delete [] rt->m_pProps;
		}

		// delete the table itself
		delete rt;
		
	}

	Q_memset( m_pRecvTables, 0, sizeof( m_pRecvTables ) );
	m_nRecvTables = 0;
}

// creates client receive tables from server send tables
void CHLTVServer::InitClientRecvTables()
{
	ServerClass* pCur = NULL;
	
	if ( ClientDLL_GetAllClasses() != NULL )
		return; //already initialized

	// first create all SendTables
	for ( pCur = serverGameDLL->GetAllServerClasses(); pCur; pCur=pCur->m_pNext )
	{
		// create receive table from send table.
		AddRecvTableR( pCur->m_pTable, m_pRecvTables, m_nRecvTables );

		ErrorIfNot( 
			m_nRecvTables < ARRAYSIZE( m_pRecvTables ), 
			("AddRecvTableR: overflowed MAX_DATATABLES")
			);
	}

	// now register client classes 
	for ( pCur = serverGameDLL->GetAllServerClasses(); pCur; pCur=pCur->m_pNext )
	{
		ErrorIfNot( 
			m_nRecvTables < ARRAYSIZE( m_pRecvTables ), 
			("ClientDLL_InitRecvTableMgr: overflowed MAX_DATATABLES")
			);

		// find top receive table for class
		RecvTable * recvt = FindRecvTable( pCur->m_pTable->GetName(), m_pRecvTables, m_nRecvTables );

		Assert ( recvt );
		
		// register class, constructor addes clientClass to g_pClientClassHead list
		ClientClass * clientclass = new ClientClass( pCur->m_pNetworkName, NULL, NULL, recvt );

		if ( !clientclass	)
		{
			Msg("HLTV_InitRecvTableMgr: failed to allocate client class %s.\n", pCur->m_pNetworkName );
			return;
		}
	}

	RecvTable_Init( m_pRecvTables, m_nRecvTables );
}



CHLTVFrame::CHLTVFrame()
{
	
}

CHLTVFrame::~CHLTVFrame()
{
	FreeBuffers();
}

void CHLTVFrame::Reset( void )
{
	for ( int i=0; i<HLTV_BUFFER_MAX; i++ )
	{
		m_Messages[i].Reset();
	}
}

bool CHLTVFrame::HasData( void )
{
	for ( int i=0; i<HLTV_BUFFER_MAX; i++ )
	{
		if ( m_Messages[i].GetNumBitsWritten() > 0 )
			return true;
	}

	return false;
}

void CHLTVFrame::CopyHLTVData( CHLTVFrame &frame )
{
	// copy reliable messages
	int bits = frame.m_Messages[HLTV_BUFFER_RELIABLE].GetNumBitsWritten();

	if ( bits > 0 )
	{
		int bytes = PAD_NUMBER( Bits2Bytes(bits), 4 );
		m_Messages[HLTV_BUFFER_RELIABLE].StartWriting( new char[ bytes ], bytes, bits );
		Q_memcpy( m_Messages[HLTV_BUFFER_RELIABLE].GetBasePointer(), frame.m_Messages[HLTV_BUFFER_RELIABLE].GetBasePointer(), bytes );
	}

	// copy unreliable messages
	bits = frame.m_Messages[HLTV_BUFFER_UNRELIABLE].GetNumBitsWritten();
	bits += frame.m_Messages[HLTV_BUFFER_TEMPENTS].GetNumBitsWritten();
	bits += frame.m_Messages[HLTV_BUFFER_SOUNDS].GetNumBitsWritten();

	if ( tv_relayvoice.GetBool() )
		bits += frame.m_Messages[HLTV_BUFFER_VOICE].GetNumBitsWritten();
	
	if ( bits > 0 )
	{
		// collapse all unreliable buffers in one
		int bytes = PAD_NUMBER( Bits2Bytes(bits), 4 );
		m_Messages[HLTV_BUFFER_UNRELIABLE].StartWriting( new char[ bytes ], bytes );
		m_Messages[HLTV_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[HLTV_BUFFER_UNRELIABLE].GetData(), frame.m_Messages[HLTV_BUFFER_UNRELIABLE].GetNumBitsWritten() ); 
		m_Messages[HLTV_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[HLTV_BUFFER_TEMPENTS].GetData(), frame.m_Messages[HLTV_BUFFER_TEMPENTS].GetNumBitsWritten() ); 
		m_Messages[HLTV_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[HLTV_BUFFER_SOUNDS].GetData(), frame.m_Messages[HLTV_BUFFER_SOUNDS].GetNumBitsWritten() ); 

		if ( tv_relayvoice.GetBool() )
			m_Messages[HLTV_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[HLTV_BUFFER_VOICE].GetData(), frame.m_Messages[HLTV_BUFFER_VOICE].GetNumBitsWritten() ); 
	}
}

void CHLTVFrame::AllocBuffers( void )
{
	// allocate buffers for input frame
	for ( int i=0; i < HLTV_BUFFER_MAX; i++ )
	{
		Assert( m_Messages[i].GetBasePointer() == NULL );
		m_Messages[i].StartWriting( new char[NET_MAX_PAYLOAD], NET_MAX_PAYLOAD);
	}
}

void CHLTVFrame::FreeBuffers( void )
{
	for ( int i=0; i<HLTV_BUFFER_MAX; i++ )
	{
		bf_write &msg = m_Messages[i];

		if ( msg.GetBasePointer() )
		{
			delete[] msg.GetBasePointer();
			msg.StartWriting( NULL, 0 );
		}
	}
}

CHLTVServer::CHLTVServer()
{
	m_flTickInterval = 0.03;
	m_MasterClient = NULL;
	m_Server = NULL;
	m_Director = NULL;
	m_nFirstTick = -1;
	m_nLastTick = 0;
	m_CurrentFrame = NULL;
	m_nViewEntity = 0;
	m_nPlayerSlot = 0;
	m_bSignonState = false;
	m_flStartTime = 0;
	m_flFPS = 0;
	m_nGameServerMaxClients = 0;
	m_fNextSendUpdateTime = 0;
	Q_memset( m_pRecvTables, 0, sizeof( m_pRecvTables ) );
	m_nRecvTables = 0;
	m_vPVSOrigin.Init();
	m_nStartTick = 0;
	m_bPlayingBack = false;
	m_bPlaybackPaused = false;
	m_flPlaybackRateModifier = 0;
	m_nSkipToTick = 0;
	m_bMasterOnlyMode = false;
	m_ClientState.m_pHLTV = this;
	m_nGlobalSlots = 0;
	m_nGlobalClients = 0;
	m_nGlobalProxies = 0;
}

CHLTVServer::~CHLTVServer()
{
	if ( m_nRecvTables > 0 )
	{
		RecvTable_Term();
		FreeClientRecvTables();
	}

	// make sure everything was destroyed
	Assert( m_CurrentFrame == NULL );
	Assert( CountClientFrames() == 0 );
}

void CHLTVServer::SetMaxClients( int number )
{
	// allow max clients 0 in HLTV
	m_nMaxclients = clamp( number, 0, ABSOLUTE_PLAYER_LIMIT );
}

void CHLTVServer::StartMaster(CGameClient *client)
{
	Clear();  // clear old settings & buffers

	if ( !client )
	{
		ConMsg("SourceTV client not found.\n");
		return;
	}

	m_Director = serverGameDirector;	

	if ( !m_Director )
	{
		ConMsg("Mod doesn't support SourceTV. No director module found.\n");
		return;
	}

	m_MasterClient = client;
	m_MasterClient->m_bIsHLTV = true;
#if defined( REPLAY_ENABLED )
	m_MasterClient->m_bIsReplay = false;
#endif

	// let game.dll know that we are the HLTV client
	Assert( serverGameClients );

	CPlayerState *player = serverGameClients->GetPlayerState( m_MasterClient->edict );
	player->hltv = true;

	m_Server = (CGameServer*)m_MasterClient->GetServer();

	// set default user settings
	m_MasterClient->m_ConVars->SetString( "name", tv_name.GetString() );
	m_MasterClient->m_ConVars->SetString( "cl_team", "1" );
	m_MasterClient->m_ConVars->SetString( "rate", "30000" );
	m_MasterClient->m_ConVars->SetString( "cl_updaterate", "22" );
	m_MasterClient->m_ConVars->SetString( "cl_interp_ratio", "1.0" );
	m_MasterClient->m_ConVars->SetString( "cl_predict", "0" );

	m_nViewEntity = m_MasterClient->GetPlayerSlot() + 1;
	m_nPlayerSlot = m_MasterClient->GetPlayerSlot();

	// copy server settings from m_Server

	m_nGameServerMaxClients = m_Server->GetMaxClients(); // maxclients is different on proxy (128)
	serverclasses	= m_Server->serverclasses;
	serverclassbits	= m_Server->serverclassbits;
	V_memcpy( worldmapMD5.bits, m_Server->worldmapMD5.bits, MD5_DIGEST_LENGTH );
	m_flTickInterval= m_Server->GetTickInterval();

	// allocate buffers for input frame
	m_HLTVFrame.AllocBuffers();
			
	InstallStringTables();

	// activate director in game.dll
	m_Director->SetHLTVServer( this );

	// register as listener for mod specific events
	const char **modevents = m_Director->GetModEvents();

	int j = 0;
	while ( modevents[j] != NULL )
	{
		const char *eventname = modevents[j];

		CGameEventDescriptor *descriptor = g_GameEventManager.GetEventDescriptor( eventname );

		if ( descriptor )
		{
			g_GameEventManager.AddListener( this, descriptor, CGameEventManager::CLIENTSTUB );
		}
		else
		{
			DevMsg("CHLTVServer::StartMaster: game event %s not found.\n", eventname );
		}

		j++;
	}
	
	// copy signon buffers
	m_Signon.StartWriting( m_Server->m_Signon.GetBasePointer(), m_Server->m_Signon.m_nDataBytes, 
		m_Server->m_Signon.GetNumBitsWritten() );

	Q_strncpy( m_szMapname, m_Server->m_szMapname, sizeof(m_szMapname) );
	Q_strncpy( m_szSkyname, m_Server->m_szSkyname, sizeof(m_szSkyname) );

	NET_ListenSocket( m_Socket, true );	// activated HLTV TCP socket

	m_MasterClient->ExecuteStringCommand( "spectate" ); // become a spectator

	m_MasterClient->UpdateUserSettings(); // make sure UserInfo is correct

	// hack reduce signontick by one to catch changes made in the current tick
	m_MasterClient->m_nSignonTick--;	

	if ( m_bMasterOnlyMode )
	{
		// we allow only one client in master only mode
		tv_maxclients.SetValue( min(1,tv_maxclients.GetInt()) );
	}

	SetMaxClients( tv_maxclients.GetInt() );

	m_bSignonState = false; //master proxy is instantly connected

	m_nSpawnCount++;

	m_flStartTime = net_time;

	m_State = ss_active;

	// stop any previous recordings
	m_DemoRecorder.StopRecording();

	// start new recording if autorecord is enabled
	if ( tv_autorecord.GetBool() )
	{
		m_DemoRecorder.StartAutoRecording();
	}

	ReconnectClients();
}

void CHLTVServer::StartDemo(const char *filename)
{

}

bool CHLTVServer::DispatchToRelay( CHLTVClient *pClient )
{
	if ( tv_dispatchmode.GetInt() <= DISPATCH_MODE_OFF )
		return false; // don't redirect
	
	CBaseClient	*pBestProxy = NULL;
	float fBestRatio = 1.0f;

	// find best relay proxy
	for (int i=0; i < GetClientCount(); i++ )
	{
		CBaseClient *pProxy = m_Clients[ i ];

		// check all known proxies
		if ( !pProxy->IsConnected() || !pProxy->IsHLTV() || (pClient == pProxy) )
			continue;

		int slots = Q_atoi( pProxy->GetUserSetting( "hltv_slots" ) );
		int clients = Q_atoi( pProxy->GetUserSetting( "hltv_clients" ) );

		// skip overloaded proxies or proxies with no slots at all
		if ( (clients > slots) || slots <= 0 )
			continue;

		// calc clients/slots ratio for this proxy
		float ratio = ((float)(clients))/((float)slots);

		if ( ratio < fBestRatio )
		{
			fBestRatio = ratio;
			pBestProxy = pProxy;
		}
	}

	if ( pBestProxy == NULL )
	{
		if ( tv_dispatchmode.GetInt() == DISPATCH_MODE_ALWAYS )
		{
			// we are in always forward mode, drop client if we can't forward it
			pClient->Disconnect("No SourceTV relay available");
			return true;
		}
		else
		{
			// just let client connect to this proxy
			return false;
		}
	}

	// check if client should stay on this relay server
	if ( (tv_dispatchmode.GetInt() == DISPATCH_MODE_AUTO) && (GetMaxClients() > 0) )
	{
		// ratio = clients/slots. give relay proxies 25% bonus
		float myRatio = ((float)GetNumClients()/(float)GetMaxClients()) * 1.25f;

		myRatio = min( myRatio, 1.0f ); // clamp to 1

		// if we have a better local ratio then other proxies, keep this client here
		if ( myRatio < fBestRatio )
			return false;	// don't redirect
	}
	
	const char *pszRelayAddr = pBestProxy->GetUserSetting("hltv_addr");

	if ( !pszRelayAddr )
		return false;
	

	ConMsg( "Redirecting spectator %s to SourceTV relay %s\n", 
		pClient->GetNetChannel()->GetRemoteAddress().ToString(), 
		pszRelayAddr );

	// first tell that client that we are a SourceTV server,
	// otherwise it's might ignore the command
	SVC_ServerInfo serverInfo;
	FillServerInfo( serverInfo );
	pClient->SendNetMsg( serverInfo, true );

	// tell the client to connect to this new address
	NET_StringCmd cmdMsg( va("redirect %s\n", pszRelayAddr ) ) ;
	pClient->SendNetMsg( cmdMsg, true );

 	// increase this proxies client number in advance so this proxy isn't used again next time
	int clients = Q_atoi( pBestProxy->GetUserSetting( "hltv_clients" ) );
	pBestProxy->SetUserCVar( "hltv_clients", va("%d", clients+1 ) );

	return true;
}

void CHLTVServer::ConnectRelay(const char *address)
{
	if ( m_ClientState.IsConnected() )
	{
		// do not try to reconnect to old connection
		m_ClientState.m_szRetryAddress[0] = 0;

		// disconnect first
		m_ClientState.Disconnect( "HLTV server connecting to relay", true );

		Changelevel(); // inactivate clients
	}

	// connect to new server
	m_ClientState.Connect( address, "tvrelay" );
}

void CHLTVServer::StartRelay()
{
	if ( !m_ClientState.IsConnected() && !IsPlayingBack() )
	{
		DevMsg("StartRelay: not connected.\n");
		Shutdown();
		return;
	}

	Clear();  // clear old settings & buffers

	if ( m_nRecvTables == 0 ) 
	{
		// must be done only once since Mod never changes
		InitClientRecvTables();
	}

	m_HLTVFrame.AllocBuffers();

	m_StringTables = &m_NetworkStringTables;
	
	SetMaxClients( tv_maxclients.GetInt() );

	m_bSignonState = true;

	m_flStartTime = net_time;

	m_State = ss_loading;

	m_nSpawnCount++;
}

int	CHLTVServer::GetHLTVSlot( void )
{
	return m_nPlayerSlot;
}

float CHLTVServer::GetOnlineTime( void )
{
	return max(0., net_time - m_flStartTime);
}

void CHLTVServer::GetLocalStats( int &proxies, int &slots, int &clients )
{
	proxies = GetNumProxies();
	clients = GetNumClients();
	slots = GetMaxClients();
}

void CHLTVServer::GetRelayStats( int &proxies, int &slots, int &clients )
{
	proxies = slots = clients = 0;

	for (int i=0 ; i < GetClientCount() ; i++ )
	{
		CBaseClient *client = m_Clients[ i ];

		if ( !client->IsConnected() || !client->IsHLTV() )
			continue;

		proxies += Q_atoi( client->GetUserSetting( "hltv_proxies" ) );
		slots += Q_atoi( client->GetUserSetting( "hltv_slots" ) );
		clients += Q_atoi( client->GetUserSetting( "hltv_clients" ) );
	}
}

void CHLTVServer::GetGlobalStats( int &proxies, int &slots, int &clients )
{
	// the master proxy is the only one that really has all data to generate
	// global stats
	if ( IsMasterProxy() )
	{
		GetRelayStats( m_nGlobalProxies, m_nGlobalSlots, m_nGlobalClients );

		m_nGlobalSlots += GetMaxClients();
		m_nGlobalClients += GetNumClients();
	}

	// if this is a relay proxies, global data comes via the 
	// wire from the master proxy
	proxies = m_nGlobalProxies;
	slots = m_nGlobalSlots;
	clients = m_nGlobalClients;
}

const netadr_t *CHLTVServer::GetRelayAddress( void )
{
	if ( IsMasterProxy() )
	{
		return &net_local_adr; // TODO wrong port
	}
	else if ( m_ClientState.m_NetChannel )
	{
		return &m_ClientState.m_NetChannel->GetRemoteAddress();
	}
	else
	{
		return NULL;
	}
}

bool CHLTVServer::IsMasterProxy( void )
{
	return ( m_MasterClient != NULL );
}

bool CHLTVServer::IsTVRelay()
{
	return !IsMasterProxy();
}

bool CHLTVServer::IsDemoPlayback( void )
{
	return false;
}

void CHLTVServer::BroadcastLocalTitle( CHLTVClient *client )
{
	IGameEvent *event = g_GameEventManager.CreateEvent( "hltv_title", true );

	if ( !event )
		return;

	event->SetString( "text", tv_title.GetString() );

	char	 buffer_data[MAX_EVENT_BYTES];

	SVC_GameEvent eventMsg;

	eventMsg.SetReliable( true );

	eventMsg.m_DataOut.StartWriting( buffer_data, sizeof(buffer_data) );

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg.m_DataOut ) )
	{
		DevMsg("CHLTVServer: failed to serialize title '%s'.\n", event->GetName() );
		g_GameEventManager.FreeEvent( event );
		return;
	}

	if ( client )
	{
		client->SendNetMsg( eventMsg );
	}
	else
	{
		for ( int i = 0; i < m_Clients.Count(); i++ )
		{
			client = Client(i);

			if ( !client->IsActive() || client->IsHLTV() )
				continue;

			client->SendNetMsg( eventMsg );
		}
	}

	g_GameEventManager.FreeEvent( event );
}

void CHLTVServer::BroadcastLocalChat( const char *pszChat, const char *pszGroup )
{
	IGameEvent *event = g_GameEventManager.CreateEvent( "hltv_chat", true );

	if ( !event )
		return;
	
	event->SetString( "text", pszChat );

	char	 buffer_data[MAX_EVENT_BYTES];

	SVC_GameEvent eventMsg;

	eventMsg.SetReliable( false );

	eventMsg.m_DataOut.StartWriting( buffer_data, sizeof(buffer_data) );

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg.m_DataOut ) )
	{
		DevMsg("CHLTVServer: failed to serialize chat '%s'.\n", event->GetName() );
		g_GameEventManager.FreeEvent( event );
		return;
	}

	for ( int i = 0; i < m_Clients.Count(); i++ )
	{
		CHLTVClient *cl = Client(i);

		if ( !cl->IsActive() || !cl->IsSpawned() || cl->IsHLTV() )
			continue;

		// if this is a spectator chat message and client disabled it, don't show it
		if ( Q_strcmp( cl->m_szChatGroup, pszGroup) || cl->m_bNoChat )
			continue;

		cl->SendNetMsg( eventMsg );
	}

	g_GameEventManager.FreeEvent( event );
}

void CHLTVServer::BroadcastEventLocal( IGameEvent *event, bool bReliable )
{
	char	 buffer_data[MAX_EVENT_BYTES];

	SVC_GameEvent eventMsg;

	eventMsg.SetReliable( bReliable );

	eventMsg.m_DataOut.StartWriting( buffer_data, sizeof(buffer_data) );

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg.m_DataOut ) )
	{
		DevMsg("CHLTVServer: failed to serialize local event '%s'.\n", event->GetName() );
		return;
	}

	for ( int i = 0; i < m_Clients.Count(); i++ )
	{
		CHLTVClient *cl = Client(i);

		if ( !cl->IsActive() || !cl->IsSpawned() || cl->IsHLTV() )
			continue;

		if ( !cl->SendNetMsg( eventMsg ) )
		{
			if ( eventMsg.IsReliable() )
			{
				DevMsg( "BroadcastMessage: Reliable broadcast message overflow for client %s", cl->GetClientName() );
			}
		}
	}

	if ( tv_debug.GetBool() )
		Msg("SourceTV broadcast local event: %s\n", event->GetName() );
}

void CHLTVServer::BroadcastEvent(IGameEvent *event)
{
	char	 buffer_data[MAX_EVENT_BYTES];

	SVC_GameEvent eventMsg;

	eventMsg.m_DataOut.StartWriting( buffer_data, sizeof(buffer_data) );

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg.m_DataOut ) )
	{
		DevMsg("CHLTVServer: failed to serialize event '%s'.\n", event->GetName() );
		return;
	}
	
	BroadcastMessage( eventMsg, true, true );
	
	if ( tv_debug.GetBool() )
		Msg("SourceTV broadcast event: %s\n", event->GetName() );
}
 
void CHLTVServer::FireGameEvent(IGameEvent *event)
{
	if ( !IsActive() )
		return;

	char buffer_data[MAX_EVENT_BYTES];

	SVC_GameEvent eventMsg;

	eventMsg.m_DataOut.StartWriting( buffer_data, sizeof(buffer_data) );

	// create bit stream from KeyValues
	if ( g_GameEventManager.SerializeEvent( event, &eventMsg.m_DataOut ) )
	{
		SendNetMsg( eventMsg );
	}
	else
	{
		DevMsg("CHLTVServer::FireGameEvent: failed to serialize event '%s'.\n", event->GetName() );
	}
}

bool CHLTVServer::ShouldUpdateMasterServer()
{

	// If the main game server is active, then we let it update Steam with the server info.
	return !sv.IsActive();
}

CBaseClient *CHLTVServer::CreateNewClient(int slot )
{
	return new CHLTVClient( slot, this );
}

void CHLTVServer::InstallStringTables( void )
{
#ifndef SHARED_NET_STRING_TABLES

	int numTables = m_Server->m_StringTables->GetNumTables();

	m_StringTables = &m_NetworkStringTables;

	Assert( m_StringTables->GetNumTables() == 0); // must be empty

	m_StringTables->AllowCreation( true );
	
	// master hltv needs to keep a list of changes for all table items
	m_StringTables->EnableRollback( true );

	for ( int i =0; i<numTables; i++)
	{
		// iterate through server tables
		CNetworkStringTable *serverTable = 
			(CNetworkStringTable*)m_Server->m_StringTables->GetTable( i );

		if ( !serverTable )
			continue;

		// get matching client table
		CNetworkStringTable *hltvTable = 
			(CNetworkStringTable*)m_StringTables->CreateStringTableEx(
				serverTable->GetTableName(),
				serverTable->GetMaxStrings(),
				serverTable->GetUserDataSize(),
				serverTable->GetUserDataSizeBits(),
				serverTable->HasFileNameStrings() 
				);

		if ( !hltvTable )
		{
			DevMsg("SV_InstallHLTVStringTableMirrors! Missing client table \"%s\".\n ", serverTable->GetTableName() );
			continue;
		}

		// make hltv table an exact copy of server table
		hltvTable->CopyStringTable( serverTable ); 

		// link hltv table to server table
		serverTable->SetMirrorTable( hltvTable );
	}

	m_StringTables->AllowCreation( false );

#endif
}

void CHLTVServer::RestoreTick( int tick )
{
#ifndef SHARED_NET_STRING_TABLES

	// only master proxy delays time
	if ( !IsMasterProxy() )
		return;

	int numTables = m_StringTables->GetNumTables();

	for ( int i =0; i<numTables; i++)
	{
			// iterate through server tables
		CNetworkStringTable *pTable = (CNetworkStringTable*) m_StringTables->GetTable( i );
		pTable->RestoreTick( tick );
	}

#endif
}

void CHLTVServer::UserInfoChanged( int nClientIndex )
{
	// don't change UserInfo table, it keeps the infos of the original players
}

void CHLTVServer::LinkInstanceBaselines( void )
{	
	// Forces to update m_pInstanceBaselineTable.
	AUTO_LOCK( g_svInstanceBaselineMutex );
	GetInstanceBaselineTable(); 

	Assert( m_pInstanceBaselineTable );
		
	// update all found server classes 
	for ( ServerClass *pClass = serverGameDLL->GetAllServerClasses(); pClass; pClass=pClass->m_pNext )
	{
		char idString[32];
		Q_snprintf( idString, sizeof( idString ), "%d", pClass->m_ClassID );

		// Ok, make a new instance baseline so they can reference it.
		int index  = m_pInstanceBaselineTable->FindStringIndex( idString );
			
		if ( index != -1 )
		{
			pClass->m_InstanceBaselineIndex = index;
		}
		else
		{
			pClass->m_InstanceBaselineIndex = INVALID_STRING_INDEX;
		}
	}
}

/* CHLTVServer::GetOriginFromPackedEntity is such a bad, bad hack.

extern float DecodeFloat(SendProp const *pProp, bf_read *pIn);

Vector CHLTVServer::GetOriginFromPackedEntity(PackedEntity* pe)
{
	Vector origin; origin.Init();

	SendTable *pSendTable = pe->m_pSendTable;

	// recursively go down until BaseEntity sendtable
	while ( Q_strcmp( pSendTable->GetName(), "DT_BaseEntity") )
	{
		SendProp *pProp = pSendTable->GetProp( 0 ); // 0 = baseclass
		pSendTable = pProp->GetDataTable();
	}

	for ( int i=0; i < pSendTable->GetNumProps(); i++ )
	{
		SendProp *pProp = pSendTable->GetProp( i );

		if ( Q_strcmp( pProp->GetName(), "m_vecOrigin" ) == 0 )
		{
			Assert( pProp->GetType() == DPT_Vector );
		
			bf_read buf( pe->LockData(), Bits2Bytes(pe->GetNumBits()), pProp->GetOffset() );

			origin[0] = DecodeFloat(pProp, &buf);
			origin[1] = DecodeFloat(pProp, &buf);
			origin[2] = DecodeFloat(pProp, &buf);

			break;
		}
	}

	return origin;
} */

CHLTVEntityData *FindHLTVDataInSnapshot( CFrameSnapshot * pSnapshot, int iEntIndex )
{
	int a = 0;
	int z = pSnapshot->m_nValidEntities-1;

	if ( iEntIndex < pSnapshot->m_pValidEntities[a] ||
		 iEntIndex > pSnapshot->m_pValidEntities[z] )
		 return NULL;
	
	while ( a < z )
	{
		int m = (a+z)/2;

		int index = pSnapshot->m_pValidEntities[m];

		if ( index == iEntIndex )
			return &pSnapshot->m_pHLTVEntityData[m];
		
		if ( iEntIndex > index )
		{
			if ( pSnapshot->m_pValidEntities[z] == iEntIndex )
				return &pSnapshot->m_pHLTVEntityData[z];

			if ( a == m )
				return NULL;

			a = m;
		}
		else
		{
			if ( pSnapshot->m_pValidEntities[a] == iEntIndex )
				return &pSnapshot->m_pHLTVEntityData[a];

			if ( z == m )
				return NULL;

			z = m;
		}
	}

	return NULL;
}

void CHLTVServer::EntityPVSCheck( CClientFrame *pFrame )
{
	byte PVS[PAD_NUMBER( MAX_MAP_CLUSTERS,8 ) / 8];
	int nPVSSize = (GetCollisionBSPData()->numclusters + 7) / 8;

	// setup engine PVS
	SV_ResetPVS( PVS, nPVSSize );

	CFrameSnapshot * pSnapshot = pFrame->GetSnapshot();	

	Assert ( pSnapshot->m_pHLTVEntityData != NULL );

	int nDirectorEntity = m_Director->GetPVSEntity();
    	
	if ( pSnapshot && nDirectorEntity > 0 )
	{
		CHLTVEntityData *pHLTVData = FindHLTVDataInSnapshot( pSnapshot, nDirectorEntity );

		if ( pHLTVData )
		{
			m_vPVSOrigin.x = pHLTVData->origin[0];
			m_vPVSOrigin.y = pHLTVData->origin[1];
			m_vPVSOrigin.z = pHLTVData->origin[2];
		}
	}
	else
	{
		m_vPVSOrigin = m_Director->GetPVSOrigin();
	}


	SV_AddOriginToPVS( m_vPVSOrigin );

	// know remove all entities that aren't in PVS
	int entindex = -1;

	while ( true )
	{
		entindex = pFrame->transmit_entity.FindNextSetBit( entindex+1 );

		if ( entindex < 0 )
			break;
			
		// is transmit_always is set ->  no PVS check
		if ( pFrame->transmit_always->Get(entindex) )
		{
			pFrame->last_entity = entindex;
			continue;
		}

		CHLTVEntityData *pHLTVData = FindHLTVDataInSnapshot( pSnapshot, entindex );

		if ( !pHLTVData )
			continue;

		unsigned int nNodeCluster = pHLTVData->m_nNodeCluster;

		// check if node or cluster is in PVS

		if ( nNodeCluster & (1<<31) )
		{
			// it's a node SLOW
			nNodeCluster &= ~(1<<31);
			if ( CM_HeadnodeVisible( nNodeCluster, PVS, nPVSSize ) )
			{
				pFrame->last_entity = entindex;
				continue;
			}
		}
		else
		{
			// it's a cluster QUICK
			if ( PVS[nNodeCluster >> 3] & (1 << (nNodeCluster & 7)) )
			{
				pFrame->last_entity = entindex;
				continue;
			}
		}

 		// entity is not in PVS, remove from transmit_entity list
		pFrame->transmit_entity.Clear( entindex );
	}
}

void CHLTVServer::SignonComplete()
{
	Assert ( !IsMasterProxy() );
	
	m_bSignonState = false;

	LinkInstanceBaselines();

	if ( tv_debug.GetBool() )
		Msg("SourceTV signon complete.\n" );
}

CClientFrame *CHLTVServer::AddNewFrame( CClientFrame *clientFrame )
{
	VPROF_BUDGET( "CHLTVServer::AddNewFrame", "HLTV" );

	Assert ( clientFrame );
	Assert( clientFrame->tick_count > m_nLastTick );

	m_nLastTick = clientFrame->tick_count;

	m_HLTVFrame.SetSnapshot( clientFrame->GetSnapshot() );
	m_HLTVFrame.tick_count = clientFrame->tick_count;	
	m_HLTVFrame.last_entity = clientFrame->last_entity;
	m_HLTVFrame.transmit_entity = clientFrame->transmit_entity;

	// remember tick of first valid frame
	if ( m_nFirstTick < 0 )
	{
		m_nFirstTick = clientFrame->tick_count;
		m_nTickCount = m_nFirstTick;

		if ( !IsMasterProxy() )
		{
			Assert ( m_State == ss_loading );
			m_State = ss_active; // we are now ready to go

			ReconnectClients();

			ConMsg("SourceTV relay active.\n" );

			Steam3Server().Activate( CSteam3Server::eServerTypeTVRelay );
			Steam3Server().SendUpdatedServerDetails();
		}
		else
		{
			ConMsg("SourceTV broadcast active.\n" );
		}
	}

	CHLTVFrame *hltvFrame = new CHLTVFrame;

	// copy tickcount & entities from client frame
	hltvFrame->CopyFrame( *clientFrame );

	//copy rest (messages, tempents) from current HLTV frame
	hltvFrame->CopyHLTVData( m_HLTVFrame );

	// add frame to HLTV server
	AddClientFrame( hltvFrame );

	if ( IsMasterProxy() && m_DemoRecorder.IsRecording() )
	{
		m_DemoRecorder.WriteFrame( &m_HLTVFrame );
	}

	// reset HLTV frame for recording next messages etc.
	m_HLTVFrame.Reset();
	m_HLTVFrame.SetSnapshot( NULL );

	return hltvFrame;
}

void CHLTVServer::SendClientMessages ( bool bSendSnapshots )
{
	// build individual updates
	for ( int i=0; i< m_Clients.Count(); i++ )
	{
		CHLTVClient* client = Client(i);
		
		// Update Host client send state...
		if ( !client->ShouldSendMessages() )
		{
			continue;
		}

		// Append the unreliable data (player updates and packet entities)
		if ( m_CurrentFrame && client->IsActive() )
		{
			// don't send same snapshot twice
			client->SendSnapshot( m_CurrentFrame );
		}
		else
		{
			// Connected, but inactive, just send reliable, sequenced info.
			client->m_NetChannel->Transmit();
		}

		client->UpdateSendState();
		client->m_fLastSendTime = net_time;
	}
}

void CHLTVServer::UpdateStats( void )
{
	if ( m_fNextSendUpdateTime > net_time )
		return;

	m_fNextSendUpdateTime = net_time + 8.0f;
	
	// fire game event for everyone
	IGameEvent *event = NULL; 

	if ( !IsMasterProxy() && !m_ClientState.IsConnected() )
	{
		// we are disconnected from SourceTV server
		event = g_GameEventManager.CreateEvent( "hltv_message", true );

		if ( !event )
			return;

		event->SetString( "text", "SourceTV reconnecting ..." );
	}
	else 
	{
		int proxies, slots, clients;
		GetGlobalStats( proxies, slots, clients );

		event = g_GameEventManager.CreateEvent( "hltv_status", true );

		if ( !event )
			return;

		char address[32];

		if ( IsMasterProxy() || tv_overridemaster.GetBool() )
		{
			// broadcast own address
			Q_snprintf( address, sizeof(address), "%s:%u", net_local_adr.ToString(true), GetUDPPort() );
		}
		else
		{
			// forward address
			Q_snprintf( address, sizeof(address), "%s", m_RootServer.ToString() );
		}

		event->SetString( "master", address );
		event->SetInt( "clients", clients );
		event->SetInt( "slots", slots);
		event->SetInt( "proxies", proxies );
	}

	if ( IsMasterProxy() )
	{
		// as a master fire event for every one
		g_GameEventManager.FireEvent( event );
	}
	else
	{
		// as a relay proxy just broadcast event
		BroadcastEvent( event );
	}

}

bool CHLTVServer::SendNetMsg( INetMessage &msg, bool bForceReliable )
{
	if ( m_bSignonState	)
	{
		return msg.WriteToBuffer( m_Signon );
	}

	int buffer = HLTV_BUFFER_UNRELIABLE;	// default destination

	if ( msg.IsReliable() )
	{
		buffer = HLTV_BUFFER_RELIABLE;
	}
	else if ( msg.GetType() == svc_Sounds )
	{
		buffer = HLTV_BUFFER_SOUNDS;
	}
	else if ( msg.GetType() == svc_VoiceData )
	{
		buffer = HLTV_BUFFER_VOICE;
	}
	else if ( msg.GetType() == svc_TempEntities )
	{
		buffer = HLTV_BUFFER_TEMPENTS;
	}

	// anything else goes to the unreliable bin
	return msg.WriteToBuffer( m_HLTVFrame.m_Messages[buffer] );
}

bf_write *CHLTVServer::GetBuffer( int nBuffer )
{
	if ( nBuffer < 0 || nBuffer >= HLTV_BUFFER_MAX )
		return NULL;

	return &m_HLTVFrame.m_Messages[nBuffer];
}

IServer *CHLTVServer::GetBaseServer()
{
	return (IServer*)this;
}

IHLTVDirector *CHLTVServer::GetDirector()
{
	return m_Director;
}

CClientFrame *CHLTVServer::GetDeltaFrame( int nTick )
{
	if ( !tv_deltacache.GetBool() )
		return GetClientFrame( nTick ); //expensive

	// TODO make that a utlmap
	FOR_EACH_VEC( m_FrameCache, iFrame )
	{
		if ( m_FrameCache[iFrame].nTick == nTick )
			return m_FrameCache[iFrame].pFrame;
	}

	int i = m_FrameCache.AddToTail();

	CFrameCacheEntry_s &entry = m_FrameCache[i];

	entry.nTick = nTick;
	entry.pFrame = GetClientFrame( nTick ); //expensive

	return entry.pFrame;
}

void CHLTVServer::RunFrame()
{
	VPROF_BUDGET( "CHLTVServer::RunFrame", "HLTV" );

	// update network time etc
	NET_RunFrame( Plat_FloatTime() );

	if ( m_ClientState.m_nSignonState > SIGNONSTATE_NONE )
	{
		// process data from net socket
		NET_ProcessSocket( m_ClientState.m_Socket, &m_ClientState );

		m_ClientState.RunFrame();
		
		m_ClientState.SendPacket();
	}

	// check if HLTV server if active
	if ( !IsActive() )
		return;

	if ( host_frametime > 0 )
	{
		m_flFPS = m_flFPS * 0.99f + 0.01f/host_frametime;
	}

	if ( IsPlayingBack() )
		return;

	// get current tick time for director module and restore 
	// world (stringtables, framebuffers) as they were at this time
	UpdateTick();

	// Run any commands from client and play client Think functions if it is time.
	CBaseServer::RunFrame();

	UpdateStats();

	SendClientMessages( true );

	// Update the Steam server if we're running a relay.
	if ( !sv.IsActive() )
		Steam3Server().RunFrame();
	
	UpdateMasterServer();
}

void CHLTVServer::UpdateTick( void )
{
	VPROF_BUDGET( "CHLTVServer::UpdateTick", "HLTV" );

	if ( m_nFirstTick < 0 )
	{
		m_nTickCount = 0;
		m_CurrentFrame = NULL;
		return;
	}

	// set tick time to last frame added
	int nNewTick = m_nLastTick;

	if ( IsMasterProxy() )
	{
		// get tick from director, he decides delay etc
		nNewTick = max( m_nFirstTick, m_Director->GetDirectorTick() );
	}

	// the the closest available frame
	CHLTVFrame *newFrame = (CHLTVFrame*) GetClientFrame( nNewTick, false );

	if ( newFrame == NULL )
		return; // we dont have a new frame
	
	if ( m_CurrentFrame == newFrame )
		return;	// current frame didn't change

	m_CurrentFrame = newFrame;
	m_nTickCount = m_CurrentFrame->tick_count;
	
	if ( IsMasterProxy() )
	{
		// now do master proxy stuff

		// restore string tables for this time
		RestoreTick( m_nTickCount );

		// remove entities out of current PVS
		if ( tv_transmitall.GetBool() == false )
		{
			EntityPVSCheck( m_CurrentFrame );	
		}
	}
	else
	{
		// delta entity cache works only for relay proxies
		m_DeltaCache.SetTick( m_CurrentFrame->tick_count, m_CurrentFrame->last_entity+1 );
	}

	int removeTick = m_nTickCount - 16.0f/m_flTickInterval; // keep 16 seconds buffer

	if ( removeTick > 0 )
	{
		DeleteClientFrames( removeTick );
	}

	m_FrameCache.RemoveAll();
}

const char *CHLTVServer::GetName( void ) const
{
	return tv_name.GetString();
}

void CHLTVServer::FillServerInfo(SVC_ServerInfo &serverinfo)
{
	CBaseServer::FillServerInfo( serverinfo );
	
	serverinfo.m_nPlayerSlot = m_nPlayerSlot; // all spectators think they're the HLTV client
	serverinfo.m_nMaxClients = m_nGameServerMaxClients;
}

void CHLTVServer::Clear( void )
{
	CBaseServer::Clear();

	m_Director = NULL;
	m_MasterClient = NULL;
	m_ClientState.Clear();
	m_Server = NULL;
	m_nFirstTick = -1;
	m_nLastTick = 0;
	m_nTickCount = 0;
	m_CurrentFrame = NULL;
	m_nPlayerSlot = 0;
	m_flStartTime = 0.0f;
	m_nViewEntity = 1;
	m_nGameServerMaxClients = 0;
	m_fNextSendUpdateTime = 0.0f;
	m_HLTVFrame.FreeBuffers();
	m_vPVSOrigin.Init();
		
	DeleteClientFrames( -1 );

	m_DeltaCache.Flush();
	m_FrameCache.RemoveAll();
}

bool CHLTVServer::ProcessConnectionlessPacket( netpacket_t * packet )
{
	bf_read msg = packet->message; // We're copying the message, so we don't need to seek back when passing packet to the base class.
//	int bits = msg.GetNumBitsRead();

	char c = msg.ReadChar();

	if ( c == 0 )
	{
		return false;
	}

	switch ( c )
	{
#ifndef NO_STEAM
	case A2S_INFO:
		char rgchInfoPostfix[64];
		msg.ReadString( rgchInfoPostfix, sizeof( rgchInfoPostfix ) );
		if ( !Q_stricmp( rgchInfoPostfix, A2S_KEY_STRING_STEAM ) )
		{
			ReplyInfo( packet->from );
			return true;
		}

		break;
	//case A2S_PLAYER:
	//	return true;
#endif // #ifndef NO_STEAM
	}

	return CBaseServer::ProcessConnectionlessPacket( packet );
}

void CHLTVServer::Init(bool bIsDedicated)
{
	CBaseServer::Init( bIsDedicated );

	m_Socket = NS_HLTV;
	
	// check if only master proxy is allowed, no broadcasting
	if ( CommandLine()->FindParm("-tvmasteronly") )
	{
		m_bMasterOnlyMode = true;
	}
}

void CHLTVServer::Changelevel()
{
	m_DemoRecorder.StopRecording();

	InactivateClients();

	DeleteClientFrames(-1);

	m_CurrentFrame = NULL;
}

void CHLTVServer::GetNetStats( float &avgIn, float &avgOut )
{
	CBaseServer::GetNetStats( avgIn, avgOut	);

	if ( m_ClientState.IsActive() )
	{
		avgIn += m_ClientState.m_NetChannel->GetAvgData(FLOW_INCOMING);
		avgOut += m_ClientState.m_NetChannel->GetAvgData(FLOW_OUTGOING);
	}
}

void CHLTVServer::Shutdown( void )
{
	m_DemoRecorder.StopRecording(); // if recording, stop now

	if ( IsMasterProxy() )
	{
		if ( m_MasterClient )
			m_MasterClient->Disconnect( "SourceTV stop." );

		if ( m_Director )
			m_Director->SetHLTVServer( NULL );
	}
	else
	{
		// do not try to reconnect to old connection
		m_ClientState.m_szRetryAddress[0] = 0;

		m_ClientState.Disconnect( "HLTV server shutting down", true );
	}

	g_GameEventManager.RemoveListener( this );

	CBaseServer::Shutdown();
}

CDemoFile *CHLTVServer::GetDemoFile()
{
	return &m_DemoFile;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHLTVServer::IsPlayingBack( void )
{
	return m_bPlayingBack;
}

bool CHLTVServer::IsPlaybackPaused()
{
	return m_bPlaybackPaused;	
}

float CHLTVServer::GetPlaybackTimeScale()
{
	return m_flPlaybackRateModifier;
}

void CHLTVServer::SetPlaybackTimeScale(float timescale)
{
	m_flPlaybackRateModifier = timescale;
}

void CHLTVServer::ResyncDemoClock()
{
	m_nStartTick = host_tickcount;
}

int CHLTVServer::GetPlaybackStartTick( void )
{
	return m_nStartTick;
}

int	CHLTVServer::GetPlaybackTick( void )
{
	return host_tickcount - m_nStartTick;
}

int CHLTVServer::GetTotalTicks(void)
{
	return m_DemoFile.m_DemoHeader.playback_ticks;	
}

bool CHLTVServer::StartPlayback( const char *filename, bool bAsTimeDemo )
{
	Clear();

	if ( !m_DemoFile.Open( filename, true )  )
	{
		return false;
	}

	// Read in the m_DemoHeader
	demoheader_t *dh = m_DemoFile.ReadDemoHeader();

	if ( !dh )
	{
		ConMsg( "Failed to read demo header.\n" );
		m_DemoFile.Close();
		return false;
	}
	
	// create a fake channel with a NULL address
	m_ClientState.m_NetChannel = NET_CreateNetChannel( NS_CLIENT, NULL, "DEMO", &m_ClientState );

	if ( !m_ClientState.m_NetChannel )
	{
		ConMsg( "CDemo::Play: failed to create demo net channel\n" );
		m_DemoFile.Close();
		return false;
	}
	
	m_ClientState.m_NetChannel->SetTimeout( -1.0f );	// never timeout


	// Now read in the directory structure.

	m_bPlayingBack = true;

	ConMsg( "Reading complete demo file at once...\n");

	double start = Plat_FloatTime();

	ReadCompleteDemoFile();

	double diff = Plat_FloatTime() - start;

	ConMsg( "Reading time :%.4f\n", diff );

	NET_RemoveNetChannel( m_ClientState.m_NetChannel, true );
	m_ClientState.m_NetChannel = NULL;

	return true;
}

void CHLTVServer::ReadCompleteDemoFile()
{
	int			tick = 0;
	byte		cmd = dem_signon;
	char		buffer[NET_MAX_PAYLOAD];
	netpacket_t	demoPacket;

	// setup demo packet data buffer
	Q_memset( &demoPacket, 0, sizeof(demoPacket) );
	demoPacket.from.SetType( NA_LOOPBACK);
	
	while ( true )
	{
		m_DemoFile.ReadCmdHeader( cmd, tick );

		// COMMAND HANDLERS
		switch ( cmd )
		{
		case dem_synctick:
			ResyncDemoClock();
			break;
		case dem_stop:
			// MOTODO we finished reading the file
			return ;
		case dem_consolecmd:
			{
				NET_StringCmd cmdmsg( m_DemoFile.ReadConsoleCommand() );
				m_ClientState.ProcessStringCmd( &cmdmsg );
			}
			break;
		case dem_datatables:
			{
				ALIGN4 char data[64*1024] ALIGN4_POST;
				bf_read buf( "dem_datatables", data, sizeof(data) );
				
				m_DemoFile.ReadNetworkDataTables( &buf );
				buf.Seek( 0 );

				// support for older engine demos
				if ( !DataTable_LoadDataTablesFromBuffer( &buf, m_DemoFile.m_DemoHeader.demoprotocol ) )
				{
					Host_Error( "Error parsing network data tables during demo playback." );
				}
			}
			break;
		case dem_stringtables:
			{
				void *data = NULL;
				int dataLen = 512 * 1024;
				while ( dataLen <= DEMO_FILE_MAX_STRINGTABLE_SIZE )
				{
					data = realloc( data, dataLen );
					bf_read buf( "dem_stringtables", data, dataLen );
					// did we successfully read
					if ( m_DemoFile.ReadStringTables( &buf ) > 0 )
					{
						buf.Seek( 0 );
						if ( !networkStringTableContainerClient->ReadStringTables( buf ) )
						{
							Host_Error( "Error parsing string tables during demo playback." );
						}
						break;
					}

					// Didn't fit.  Try doubling the size of the buffer
					dataLen *= 2;
				}

				if ( dataLen > DEMO_FILE_MAX_STRINGTABLE_SIZE )
				{
					Warning( "ReadCompleteDemoFile failed to read string tables. Trying to read string tables that's bigger than max string table size\n" );
				}

				free( data );
			}
			break;
		case dem_usercmd:
			{
				char bufferIn[256];
				int  length = sizeof( bufferIn );
				m_DemoFile.ReadUserCmd( bufferIn, length );
				// MOTODO HLTV must store user commands too
			}
			break;
		case dem_signon:
		case dem_packet:
			{
				int inseq, outseqack = 0;

				m_DemoFile.ReadCmdInfo( m_LastCmdInfo );	// MOTODO must be stored somewhere
				m_DemoFile.ReadSequenceInfo( inseq, outseqack );

				int length = m_DemoFile.ReadRawData( buffer, sizeof(buffer) );

				if ( length > 0 )
				{
					// succsessfully read new demopacket
					demoPacket.received = realtime;
					demoPacket.size = length;
					demoPacket.message.StartReading( buffer,  length );

					m_ClientState.m_NetChannel->ProcessPacket( &demoPacket, false );
				}
			}
	
			break;
		}
	}
}

int	CHLTVServer::GetChallengeType ( netadr_t &adr )
{
	return PROTOCOL_HASHEDCDKEY; // HLTV doesn't need Steam authentication
}

const char *CHLTVServer::GetPassword() const
{
	const char *password = tv_password.GetString();

	// if password is empty or "none", return NULL
	if ( !password[0] || !Q_stricmp(password, "none" ) )
	{
		return NULL;
	}

	return password;
}

IClient *CHLTVServer::ConnectClient ( netadr_t &adr, int protocol, int challenge, int clientChallenge, int authProtocol, 
									 const char *name, const char *password, const char *hashedCDkey, int cdKeyLen )
{
	IClient	*client = (CHLTVClient*)CBaseServer::ConnectClient( 
		adr, protocol, challenge, clientChallenge, authProtocol, name, password, hashedCDkey, cdKeyLen );

	if ( client )
	{
		// remember password
		CHLTVClient *pHltvClient = (CHLTVClient*)client;
		Q_strncpy( pHltvClient->m_szPassword, password, sizeof(pHltvClient->m_szPassword) );
	}

	return client;
}

int CHLTVServer::GetProtocolVersion()
{
	if ( GetDemoFile() )
		return GetDemoFile()->GetProtocolVersion();
	return PROTOCOL_VERSION;
}

#ifndef NO_STEAM
void CHLTVServer::ReplyInfo( const netadr_t &adr )
{
	static char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	CUtlBuffer buf;
	buf.EnsureCapacity( 2048 );

	buf.PutUnsignedInt( LittleDWord( CONNECTIONLESS_HEADER ) );
	buf.PutUnsignedChar( S2A_INFO_SRC );

	buf.PutUnsignedChar( GetProtocolVersion() ); // Hardcoded protocol version number
	buf.PutString( GetName() );
	buf.PutString( GetMapName() );
	buf.PutString( gamedir );
	buf.PutString( serverGameDLL->GetGameDescription() );

	// The next field is a 16-bit version of the AppID.  If our AppID < 65536,
	// then let's go ahead and put in in there, to maximize compatibility
	// with old clients who might be only using this field but not the new one.
	// However, if our AppID won't fit, there's no way we can be compatible,
	// anyway, so just put in a zero, which is better than a bogus AppID.
	uint16 usAppIdShort = (uint16)GetSteamAppID();
	if ( (AppId_t)usAppIdShort != GetSteamAppID() )
	{
		usAppIdShort = 0;
	}
	buf.PutShort( LittleWord( usAppIdShort ) );

	// player info
	buf.PutUnsignedChar( GetNumClients() );
	buf.PutUnsignedChar( GetMaxClients() );
	buf.PutUnsignedChar( 0 );

	// NOTE: This key's meaning is changed in the new version. Since we send gameport and specport,
	// it knows whether we're running SourceTV or not. Then it only needs to know if we're a dedicated or listen server.
	if ( IsDedicated() )
		buf.PutUnsignedChar( 'd' );	// d = dedicated server
	else
		buf.PutUnsignedChar( 'l' );	// l = listen server

#if defined(_WIN32)
	buf.PutUnsignedChar( 'w' );
#elif defined(OSX)
	buf.PutUnsignedChar( 'm' );
#else // LINUX?
	buf.PutUnsignedChar( 'l' );
#endif

	// Password?
	buf.PutUnsignedChar( GetPassword() != NULL ? 1 : 0 );
	buf.PutUnsignedChar( Steam3Server().BSecure() ? 1 : 0 );
	buf.PutString( GetSteamInfIDVersionInfo().szVersionString );

	//
	// NEW DATA.
	//

	// Write a byte with some flags that describe what is to follow.
	const char *pchTags = sv_tags.GetString();
	byte nNewFlags = 0;
	//if ( GetGamePort() != 0 )
	//	nNewFlags |= S2A_EXTRA_DATA_HAS_GAME_PORT;

	if ( Steam3Server().GetGSSteamID().IsValid() )
		nNewFlags |= S2A_EXTRA_DATA_HAS_STEAMID;

	if ( GetUDPPort() != 0 )
		nNewFlags |= S2A_EXTRA_DATA_HAS_SPECTATOR_DATA;

	if ( pchTags && pchTags[0] != '\0' )
		nNewFlags |= S2A_EXTRA_DATA_HAS_GAMETAG_DATA;

	nNewFlags |= S2A_EXTRA_DATA_GAMEID;

	buf.PutUnsignedChar( nNewFlags );

	// Write the rest of the data.
	//if ( nNewFlags & S2A_EXTRA_DATA_HAS_GAME_PORT )
	//{
	//	buf.PutShort( LittleWord( GetGamePort() ) );
	//}

	if ( nNewFlags & S2A_EXTRA_DATA_HAS_STEAMID )
	{
		buf.PutUint64( LittleQWord( Steam3Server().GetGSSteamID().ConvertToUint64() ) );
	}

	if ( nNewFlags & S2A_EXTRA_DATA_HAS_SPECTATOR_DATA )
	{
		buf.PutShort( LittleWord( GetUDPPort() ) );
		buf.PutString( GetName() );
	}

	if ( nNewFlags & S2A_EXTRA_DATA_HAS_GAMETAG_DATA )
	{
		buf.PutString( pchTags );
	}

	if ( nNewFlags & S2A_EXTRA_DATA_GAMEID )
	{
		// !FIXME! Is there a reason we aren't using the other half
		// of this field?  Shouldn't we put the game mod ID in there, too?
		// We have the game dir.
		buf.PutUint64( LittleQWord( CGameID( GetSteamAppID() ).ToUint64() ) );
	}

	NET_SendPacket( NULL, m_Socket, adr, (unsigned char *)buf.Base(), buf.TellPut() );
}
#endif // #ifndef NO_STEAM

CON_COMMAND( tv_status, "Show SourceTV server status." ) 
{
	int		slots, proxies,	clients;
	float	in, out;
	char	gd[MAX_OSPATH];

	Q_FileBase( com_gamedir, gd, sizeof( gd ) );

	if ( !hltv || !hltv->IsActive() )
	{
		ConMsg("SourceTV not active.\n" );
		return;
	}

	hltv->GetNetStats( in, out );

	in /= 1024; // as KB
	out /= 1024;

	ConMsg("--- SourceTV Status ---\n");
	ConMsg("Online %s, FPS %.1f, Version %i (%s)\n", 
		COM_FormatSeconds( hltv->GetOnlineTime() ), hltv->m_flFPS, build_number(),

#ifdef _WIN32
		"Win32" );
#else
		"Linux" );
#endif

	if ( hltv->IsDemoPlayback() )
	{
		ConMsg("Playing Demo File \"%s\"\n", "TODO demo file name" );
	}
	else if ( hltv->IsMasterProxy() )
	{
		ConMsg("Master \"%s\", delay %.0f\n", hltv->GetName(), hltv->GetDirector()->GetDelay() );
	}
	else // if ( m_Server->IsRelayProxy() )
	{
		if ( hltv->GetRelayAddress() )
		{
			ConMsg("Relay \"%s\", connect to %s\n", hltv->GetName(), hltv->GetRelayAddress()->ToString() );
		}
		else
		{
			ConMsg("Relay \"%s\", not connect.\n", hltv->GetName() );
		}
	}

	ConMsg("Game Time %s, Mod \"%s\", Map \"%s\", Players %i\n", COM_FormatSeconds( hltv->GetTime() ),
		gd, hltv->GetMapName(), hltv->GetNumPlayers() );

	ConMsg("Local IP %s:%i, KB/sec In %.1f, Out %.1f\n",
		net_local_adr.ToString( true ), hltv->GetUDPPort(), in ,out );

	hltv->GetLocalStats( proxies, slots, clients );
	
	ConMsg("Local Slots %i, Spectators %i, Proxies %i\n", 
		slots, clients-proxies, proxies );

	hltv->GetGlobalStats( proxies, slots, clients);

	ConMsg("Total Slots %i, Spectators %i, Proxies %i\n", 
		slots, clients-proxies, proxies);

	if ( hltv->m_DemoRecorder.IsRecording() )
	{
		ConMsg("Recording to \"%s\", length %s.\n", hltv->m_DemoRecorder.GetDemoFile()->m_szFileName, 
			COM_FormatSeconds( host_state.interval_per_tick * hltv->m_DemoRecorder.GetRecordingTick() ) );
	}		
}

CON_COMMAND( tv_relay, "Connect to SourceTV server and relay broadcast." )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "Usage:  tv_relay <ip:port>\n" );
		return;
	}

	const char *address = args.ArgS();

	// If it's not a single player connection to "localhost", initialize networking & stop listenserver
	if ( !Q_strncmp( address, "localhost", 9 ) )
	{
		ConMsg( "SourceTV can't connect to localhost.\n" );
		return;
	}

	if ( !hltv )
	{
		hltv = new CHLTVServer;
		hltv->Init( NET_IsDedicated() );
	}

	if ( hltv->m_bMasterOnlyMode )
	{
		ConMsg("SourceTV in Master-Only mode.\n" );
		return;
	}

	// shutdown anything else
	Host_Disconnect( false );

	// start networking
	NET_SetMutiplayer( true );	

	hltv->ConnectRelay( address );
}

CON_COMMAND( tv_stop, "Stops the SourceTV broadcast." )
{
	if ( !hltv || !hltv->IsActive() )
	{
		ConMsg("SourceTV not active.\n" );
		return;
	}

	int nClients = hltv->GetNumClients();

	hltv->Shutdown();

	ConMsg("SourceTV stopped, %i clients disconnected.\n", nClients );
}

CON_COMMAND( tv_retry, "Reconnects the SourceTV relay proxy." )
{
	if ( !hltv )
	{
		ConMsg("SourceTV not active.\n" );
		return;
	}

	if ( hltv->m_bMasterOnlyMode )
	{
		ConMsg("SourceTV in Master-Only mode.\n" );
		return;
	}

	if ( !hltv->m_ClientState.m_szRetryAddress[ 0 ] )
	{
		ConMsg( "Can't retry, no previous SourceTV connection\n" );
		return;
	}

	ConMsg( "Commencing SourceTV connection retry to %s\n", hltv->m_ClientState.m_szRetryAddress );
	Cbuf_AddText( va( "tv_relay %s\n", hltv->m_ClientState.m_szRetryAddress ) );
}

CON_COMMAND( tv_record, "Starts SourceTV demo recording." )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "Usage:  tv_record  <filename>\n" );
		return;
	}

	if ( !hltv || !hltv->IsActive() )
	{
		ConMsg("SourceTV not active.\n" );
		return;
	}

	if ( !hltv->IsMasterProxy() )
	{
		ConMsg("Only SourceTV Master can record demos instantly.\n" );
		return;
	}

	if ( hltv->m_DemoRecorder.IsRecording() )
	{
		ConMsg("SourceTV already recording to %s.\n", hltv->m_DemoRecorder.GetDemoFile()->m_szFileName );
		return;
	}

	// check path first
	if ( !COM_IsValidPath( args[1] ) )
	{
		ConMsg( "record %s: invalid path.\n", args[1] );
		return;
	}
 
	char name[ MAX_OSPATH ];

	Q_strncpy( name, args[1], sizeof( name ) );

	// add .dem if not already set by user
	Q_DefaultExtension( name, ".dem", sizeof( name ) );

	hltv->m_DemoRecorder.StartRecording( name, false );
}

CON_COMMAND( tv_stoprecord, "Stops SourceTV demo recording." )
{
	if ( !hltv || !hltv->IsActive() )
	{
		ConMsg("SourceTV not active.\n" );
		return;
	}
	
	hltv->m_DemoRecorder.StopRecording();
}

CON_COMMAND( tv_clients, "Shows list of connected SourceTV clients." )
{
	if ( !hltv || !hltv->IsActive() )
	{
		ConMsg("SourceTV not active.\n" );
		return;
	}

	int nCount = 0;

	for ( int i=0; i<hltv->GetClientCount(); i++)
	{
		CHLTVClient *client = hltv->Client( i );
		INetChannel *netchan = client->GetNetChannel();

		if ( !netchan )
			continue;
		
		ConMsg("ID: %i, \"%s\" %s, Time %s, %s, In %.1f, Out %.1f.\n",
            client->GetUserID(),
			client->GetClientName(),
			client->IsHLTV() ? "(Relay)" : "",
			COM_FormatSeconds( netchan->GetTimeConnected() ),
			netchan->GetAddress(),
			netchan->GetAvgData( FLOW_INCOMING ) / 1024,
			netchan->GetAvgData( FLOW_OUTGOING ) / 1024 );

		nCount++;
	}

	ConMsg("--- Total %i connected clients ---\n", nCount );
}

CON_COMMAND( tv_msg, "Send a screen message to all clients." )
{
	if ( !hltv || !hltv->IsActive() )
	{
		ConMsg("SourceTV not active.\n" );
		return;
	}

	IGameEvent *msg = g_GameEventManager.CreateEvent( "hltv_message", true );

	if ( msg )
	{
		msg->SetString( "text", args.ArgS() );
		hltv->BroadcastEventLocal( msg, false );
		g_GameEventManager.FreeEvent( msg );
	}
}

#ifndef SWDS
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void EditDemo_f( const CCommand &args )
{
	if ( cmd_source != src_command )
		return;

	if ( args.ArgC() < 2 )
	{
		Msg ("editdemo <demoname> : edits a demo\n");
		return;
	}

	// set current demo player to client demo player
	demoplayer = hltv;

	//
	// open the demo file
	//
	char name[ MAX_OSPATH ];

	Q_strncpy( name, args[1], sizeof( name ) );

	Q_DefaultExtension( name, ".dem", sizeof( name ) );

	hltv->m_ClientState.m_bSaveMemory = true;

	demoplayer->StartPlayback( name, false );
}

CON_COMMAND_AUTOCOMPLETEFILE( editdemo, EditDemo_f, "Edit a recorded demo file (.dem ).", NULL, dem );
#endif
