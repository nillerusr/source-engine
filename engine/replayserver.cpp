//========= Copyright Valve Corporation, All rights reserved. ============//
//
//----------------------------------------------------------------------------------------

#if defined( REPLAY_ENABLED )

#include <server_class.h>
#include <inetmessage.h>
#include <tier0/vprof.h>
#include <tier0/vcrmode.h>
#include <KeyValues.h>
#include <edict.h>
#include <eiface.h>
#include <PlayerState.h>
#include <time.h>

#include "replayserver.h"
#include "sv_client.h"
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
#include "dt_common_eng.h"
#include "baseautocompletefilelist.h"
#include "sv_steamauth.h"
#include "con_nprint.h"
#include "tier0/icommandline.h"
#include "client_class.h"
#include "replay_internal.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern CNetworkStringTableContainer *networkStringTableContainerClient;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CReplayServer *replay = NULL;

CReplayDeltaEntityCache::CReplayDeltaEntityCache()
{
	Q_memset( m_Cache, 0, sizeof(m_Cache) );
	m_nTick = 0;
	m_nMaxEntities = 0;
	m_nCacheSize = 0;
}

CReplayDeltaEntityCache::~CReplayDeltaEntityCache()
{
	Flush();
}

void CReplayDeltaEntityCache::Flush()
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

void CReplayDeltaEntityCache::SetTick( int nTick, int nMaxEntities )
{
	if ( nTick == m_nTick )
		return;

	Flush();

	m_nCacheSize = 2 * 1024;

	if ( m_nCacheSize <= 0 )
		return;

	m_nMaxEntities = MIN(nMaxEntities,MAX_EDICTS);
	m_nTick = nTick;
}

unsigned char* CReplayDeltaEntityCache::FindDeltaBits( int nEntityIndex, int nDeltaTick, int &nBits )
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

void CReplayDeltaEntityCache::AddDeltaBits( int nEntityIndex, int nDeltaTick, int nBits, bf_write *pBuffer )
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

void CReplayServer::FreeClientRecvTables()
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
void CReplayServer::InitClientRecvTables()
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
			Msg("REPLAY_InitRecvTableMgr: failed to allocate client class %s.\n", pCur->m_pNetworkName );
			return;
		}
	}

	RecvTable_Init( m_pRecvTables, m_nRecvTables );
}



CReplayFrame::CReplayFrame()
{
	
}

CReplayFrame::~CReplayFrame()
{
	FreeBuffers();
}

void CReplayFrame::Reset( void )
{
	for ( int i=0; i<REPLAY_BUFFER_MAX; i++ )
	{
		m_Messages[i].Reset();
	}
}

bool CReplayFrame::HasData( void )
{
	for ( int i=0; i<REPLAY_BUFFER_MAX; i++ )
	{
		if ( m_Messages[i].GetNumBitsWritten() > 0 )
			return true;
	}

	return false;
}

void CReplayFrame::CopyReplayData( CReplayFrame &frame )
{
	// copy reliable messages
	int bits = frame.m_Messages[REPLAY_BUFFER_RELIABLE].GetNumBitsWritten();

	if ( bits > 0 )
	{
		int bytes = PAD_NUMBER( Bits2Bytes(bits), 4 );
		m_Messages[REPLAY_BUFFER_RELIABLE].StartWriting( new char[ bytes ], bytes, bits );
		Q_memcpy( m_Messages[REPLAY_BUFFER_RELIABLE].GetBasePointer(), frame.m_Messages[REPLAY_BUFFER_RELIABLE].GetBasePointer(), bytes );
	}

	// copy unreliable messages
	bits = frame.m_Messages[REPLAY_BUFFER_UNRELIABLE].GetNumBitsWritten();
	bits += frame.m_Messages[REPLAY_BUFFER_TEMPENTS].GetNumBitsWritten();
	bits += frame.m_Messages[REPLAY_BUFFER_SOUNDS].GetNumBitsWritten();
	bits += frame.m_Messages[REPLAY_BUFFER_VOICE].GetNumBitsWritten();
	
	if ( bits > 0 )
	{
		// collapse all unreliable buffers in one
		int bytes = PAD_NUMBER( Bits2Bytes(bits), 4 );
		m_Messages[REPLAY_BUFFER_UNRELIABLE].StartWriting( new char[ bytes ], bytes );
		m_Messages[REPLAY_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[REPLAY_BUFFER_UNRELIABLE].GetData(), frame.m_Messages[REPLAY_BUFFER_UNRELIABLE].GetNumBitsWritten() ); 
		m_Messages[REPLAY_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[REPLAY_BUFFER_TEMPENTS].GetData(), frame.m_Messages[REPLAY_BUFFER_TEMPENTS].GetNumBitsWritten() ); 
		m_Messages[REPLAY_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[REPLAY_BUFFER_SOUNDS].GetData(), frame.m_Messages[REPLAY_BUFFER_SOUNDS].GetNumBitsWritten() ); 
		m_Messages[REPLAY_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[REPLAY_BUFFER_VOICE].GetData(), frame.m_Messages[REPLAY_BUFFER_VOICE].GetNumBitsWritten() ); 
	}
}

void CReplayFrame::AllocBuffers( void )
{
	// allocate buffers for input frame
	for ( int i=0; i < REPLAY_BUFFER_MAX; i++ )
	{
		Assert( m_Messages[i].GetBasePointer() == NULL );
		m_Messages[i].StartWriting( new char[NET_MAX_PAYLOAD], NET_MAX_PAYLOAD);
	}
}

void CReplayFrame::FreeBuffers( void )
{
	for ( int i=0; i<REPLAY_BUFFER_MAX; i++ )
	{
		bf_write &msg = m_Messages[i];

		if ( msg.GetBasePointer() )
		{
			delete[] msg.GetBasePointer();
			msg.StartWriting( NULL, 0 );
		}
	}
}

CReplayServer::CReplayServer()
:	m_DemoRecorder( this )
{
	m_flTickInterval = 0.03;
	m_MasterClient = NULL;
	m_Server = NULL;
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
	m_bMasterOnlyMode = false;
	m_nGlobalSlots = 0;
	m_nGlobalClients = 0;
	m_nGlobalProxies = 0;
	m_flStartRecordTime = 0.0f;
	m_flStopRecordTime = 0.0f;
}

CReplayServer::~CReplayServer()
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

void CReplayServer::SetMaxClients( int number )
{
	// allow max clients 0 in Replay
	m_nMaxclients = clamp( number, 0, ABSOLUTE_PLAYER_LIMIT );
}

void CReplayServer::StartMaster(CGameClient *client)
{
	Clear();  // clear old settings & buffers

	if ( !client )
	{
		ConMsg("Replay client not found.\n");
		return;
	}

	m_MasterClient = client;
	m_MasterClient->m_bIsHLTV = false;
	m_MasterClient->m_bIsReplay = true;

	// let game.dll know that we are the Replay client
	Assert( serverGameClients );

	CPlayerState *player = serverGameClients->GetPlayerState( m_MasterClient->edict );
	player->replay = true;

	m_Server = (CGameServer*)m_MasterClient->GetServer();

	// set default user settings
	ConVarRef replay_name( "replay_name" );
	m_MasterClient->m_ConVars->SetString( "name", replay_name.GetString() );
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
	worldmapMD5		= m_Server->worldmapMD5;
	m_flTickInterval= m_Server->GetTickInterval();

	// allocate buffers for input frame
	m_ReplayFrame.AllocBuffers();
			
	InstallStringTables();

	// copy signon buffers
	m_Signon.StartWriting( m_Server->m_Signon.GetBasePointer(), m_Server->m_Signon.m_nDataBytes, 
		m_Server->m_Signon.GetNumBitsWritten() );

	Q_strncpy( m_szMapname, m_Server->m_szMapname, sizeof(m_szMapname) );
	Q_strncpy( m_szSkyname, m_Server->m_szSkyname, sizeof(m_szSkyname) );

	m_MasterClient->ExecuteStringCommand( "spectate" ); // become a spectator

	m_MasterClient->UpdateUserSettings(); // make sure UserInfo is correct

	// hack reduce signontick by one to catch changes made in the current tick
	m_MasterClient->m_nSignonTick--;	

	SetMaxClients( 0 );

	m_bSignonState = false; //master proxy is instantly connected

	m_nSpawnCount++;

	m_flStartTime = net_time;

	m_State = ss_active;

	// stop any previous recordings
	StopRecording();

	ReconnectClients();
}

int	CReplayServer::GetReplaySlot( void )
{
	return m_nPlayerSlot;
}

float CReplayServer::GetOnlineTime( void )
{
	return MAX(0, net_time - m_flStartTime);
}

void CReplayServer::FireGameEvent(IGameEvent *event)
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
		DevMsg("CReplayServer::FireGameEvent: failed to serialize event '%s'.\n", event->GetName() );
	}
}

int CReplayServer::GetEventDebugID()
{
	return m_nDebugID;
}

bool CReplayServer::ShouldUpdateMasterServer()
{
	// The replay server should never do this work
	return false;
}

void CReplayServer::InstallStringTables( void )
{
#ifndef SHARED_NET_STRING_TABLES

	int numTables = m_Server->m_StringTables->GetNumTables();

	m_StringTables = &m_NetworkStringTables;

	Assert( m_StringTables->GetNumTables() == 0); // must be empty

	m_StringTables->AllowCreation( true );
	
	// master replay needs to keep a list of changes for all table items
	m_StringTables->EnableRollback( true );

	for ( int i =0; i<numTables; i++)
	{
		// iterate through server tables
		CNetworkStringTable *serverTable = 
			(CNetworkStringTable*)m_Server->m_StringTables->GetTable( i );

		if ( !serverTable )
			continue;

		// get matching client table
		CNetworkStringTable *replayTable = 
			(CNetworkStringTable*)m_StringTables->CreateStringTableEx(
				serverTable->GetTableName(),
				serverTable->GetMaxStrings(),
				serverTable->GetUserDataSize(),
				serverTable->GetUserDataSizeBits(),
				serverTable->HasFileNameStrings() 
				);

		if ( !replayTable )
		{
			DevMsg("SV_InstallReplayStringTableMirrors! Missing client table \"%s\".\n ", serverTable->GetTableName() );
			continue;
		}

		// make replay table an exact copy of server table
		replayTable->CopyStringTable( serverTable ); 

		// link replay table to server table
		serverTable->SetMirrorTable( replayTable );
	}

	m_StringTables->AllowCreation( false );

#endif
}

void CReplayServer::RestoreTick( int tick )
{
#ifndef SHARED_NET_STRING_TABLES

	int numTables = m_StringTables->GetNumTables();

	for ( int i =0; i<numTables; i++)
	{
			// iterate through server tables
		CNetworkStringTable *pTable = (CNetworkStringTable*) m_StringTables->GetTable( i );
		pTable->RestoreTick( tick );
	}

#endif
}

void CReplayServer::UserInfoChanged( int nClientIndex )
{
	// don't change UserInfo table, it keeps the infos of the original players
}

void CReplayServer::LinkInstanceBaselines( void )
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

/* CReplayServer::GetOriginFromPackedEntity is such a bad, bad hack.

extern float DecodeFloat(SendProp const *pProp, bf_read *pIn);

Vector CReplayServer::GetOriginFromPackedEntity(PackedEntity* pe)
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

CReplayEntityData *FindReplayDataInSnapshot( CFrameSnapshot * pSnapshot, int iEntIndex )
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
			return &pSnapshot->m_pReplayEntityData[m];
		
		if ( iEntIndex > index )
		{
			if ( pSnapshot->m_pValidEntities[z] == iEntIndex )
				return &pSnapshot->m_pReplayEntityData[z];

			if ( a == m )
				return NULL;

			a = m;
		}
		else
		{
			if ( pSnapshot->m_pValidEntities[a] == iEntIndex )
				return &pSnapshot->m_pReplayEntityData[a];

			if ( z == m )
				return NULL;

			z = m;
		}
	}

	return NULL;
}

void CReplayServer::EntityPVSCheck( CClientFrame *pFrame )
{
	byte PVS[PAD_NUMBER( MAX_MAP_CLUSTERS,8 ) / 8];
	int nPVSSize = (GetCollisionBSPData()->numclusters + 7) / 8;

	// setup engine PVS
	SV_ResetPVS( PVS, nPVSSize );

	CFrameSnapshot * pSnapshot = pFrame->GetSnapshot();	

	Assert ( pSnapshot->m_pReplayEntityData != NULL );

	m_vPVSOrigin.Init();

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

		CReplayEntityData *pReplayData = FindReplayDataInSnapshot( pSnapshot, entindex );

		if ( !pReplayData )
			continue;

		unsigned int nNodeCluster = pReplayData->m_nNodeCluster;

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

CClientFrame *CReplayServer::AddNewFrame( CClientFrame *clientFrame )
{
	VPROF_BUDGET( "CReplayServer::AddNewFrame", "Replay" );

	Assert ( clientFrame );
	Assert( clientFrame->tick_count > m_nLastTick );

	m_nLastTick = clientFrame->tick_count;

	m_ReplayFrame.SetSnapshot( clientFrame->GetSnapshot() );
	m_ReplayFrame.tick_count = clientFrame->tick_count;	
	m_ReplayFrame.last_entity = clientFrame->last_entity;
	m_ReplayFrame.transmit_entity = clientFrame->transmit_entity;

	// remember tick of first valid frame
	if ( m_nFirstTick < 0 )
	{
		m_nFirstTick = clientFrame->tick_count;
		m_nTickCount = m_nFirstTick;
	}
	
	CReplayFrame *replayFrame = new CReplayFrame;

	// copy tickcount & entities from client frame
	replayFrame->CopyFrame( *clientFrame );

	//copy rest (messages, tempents) from current Replay frame
	replayFrame->CopyReplayData( m_ReplayFrame );
	
	// add frame to Replay server
	AddClientFrame( replayFrame );

	if ( m_DemoRecorder.IsRecording() )
	{
		m_DemoRecorder.WriteFrame( &m_ReplayFrame );
	}

	// reset Replay frame for recording next messages etc.
	m_ReplayFrame.Reset();
	m_ReplayFrame.SetSnapshot( NULL );

	return replayFrame;
}

bool CReplayServer::SendNetMsg( INetMessage &msg, bool bForceReliable )
{
	if ( m_bSignonState	)
	{
		return msg.WriteToBuffer( m_Signon );
	}

	int buffer = REPLAY_BUFFER_UNRELIABLE;	// default destination

	if ( msg.IsReliable() )
	{
		buffer = REPLAY_BUFFER_RELIABLE;
	}
	else if ( msg.GetType() == svc_Sounds )
	{
		buffer = REPLAY_BUFFER_SOUNDS;
	}
	else if ( msg.GetType() == svc_VoiceData )
	{
		buffer = REPLAY_BUFFER_VOICE;
	}
	else if ( msg.GetType() == svc_TempEntities )
	{
		buffer = REPLAY_BUFFER_TEMPENTS;
	}

	// anything else goes to the unreliable bin
	return msg.WriteToBuffer( m_ReplayFrame.m_Messages[buffer] );
}

bf_write *CReplayServer::GetBuffer( int nBuffer )
{
	if ( nBuffer < 0 || nBuffer >= REPLAY_BUFFER_MAX )
		return NULL;

	return &m_ReplayFrame.m_Messages[nBuffer];
}

IServer *CReplayServer::GetBaseServer()
{
	return (IServer*)this;
}

CClientFrame *CReplayServer::GetDeltaFrame( int nTick )
{
	// TODO make that a utlmap
	FOR_EACH_VEC( m_FrameCache, iFrame )
	{
		if ( m_FrameCache[iFrame].nTick == nTick )
			return m_FrameCache[iFrame].pFrame;
	}

	int i = m_FrameCache.AddToTail();

	CReplayFrameCacheEntry_s &entry = m_FrameCache[i];

	entry.nTick = nTick;
	entry.pFrame = GetClientFrame( nTick ); //expensive

	return entry.pFrame;
}

void CReplayServer::RunFrame()
{
	VPROF_BUDGET( "CReplayServer::RunFrame", "Replay" );

	// update network time etc
	NET_RunFrame( Plat_FloatTime() );

	// check if Replay server if active
	if ( !IsActive() )
		return;

	if ( host_frametime > 0 )
	{
		m_flFPS = m_flFPS * 0.99f + 0.01f/host_frametime;
	}

	// get current tick time for director module and restore 
	// world (stringtables, framebuffers) as they were at this time
	UpdateTick();

	// Update the Steam server if we're running a relay.
	if ( !sv.IsActive() )
		Steam3Server().RunFrame();
	
	UpdateMasterServer();

	SendPendingEvents();
}

void CReplayServer::UpdateTick( void )
{
	VPROF_BUDGET( "CReplayServer::UpdateTick", "Replay" );

	if ( m_nFirstTick < 0 )
	{
		m_nTickCount = 0;
		m_CurrentFrame = NULL;
		return;
	}

	// set tick time to last frame added
	int nNewTick = MAX( m_nLastTick, 0 );

	// the the closest available frame
	CReplayFrame *newFrame = (CReplayFrame*) GetClientFrame( nNewTick, false );

	if ( newFrame == NULL )
		return; // we dont have a new frame
	
	if ( m_CurrentFrame == newFrame )
		return;	// current frame didn't change

	m_CurrentFrame = newFrame;
	m_nTickCount = m_CurrentFrame->tick_count;
	
	// restore string tables for this time
	RestoreTick( m_nTickCount );

	int removeTick = m_nTickCount - 16.0f/m_flTickInterval; // keep 16 second buffer

	if ( removeTick > 0 )
	{
		DeleteClientFrames( removeTick );
	}

	m_FrameCache.RemoveAll();
}

const char *CReplayServer::GetName( void ) const
{
	ConVarRef replay_name( "replay_name" );
	return replay_name.GetString();
}

void CReplayServer::FillServerInfo(SVC_ServerInfo &serverinfo)
{
	CBaseServer::FillServerInfo( serverinfo );
	
	serverinfo.m_nPlayerSlot = m_nPlayerSlot; // all spectators think they're the Replay client
	serverinfo.m_nMaxClients = m_nGameServerMaxClients;
}

void CReplayServer::Clear( void )
{
	CBaseServer::Clear();

	m_MasterClient = NULL;
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
	m_ReplayFrame.FreeBuffers();
	m_vPVSOrigin.Init();
		
	DeleteClientFrames( -1 );

	m_DeltaCache.Flush();
	m_FrameCache.RemoveAll();
}

void CReplayServer::Init(bool bIsDedicated)
{
	CBaseServer::Init( bIsDedicated );

	// No broadcasting
	m_bMasterOnlyMode = true;
}

void CReplayServer::Changelevel()
{
	if ( g_pReplay->IsRecording() )
	{
		g_pReplay->SV_EndRecordingSession();
	}

	InactivateClients();
	DeleteClientFrames(-1);
	m_CurrentFrame = NULL;
}

void CReplayServer::GetNetStats( float &avgIn, float &avgOut )
{
	CBaseServer::GetNetStats( avgIn, avgOut	);
}

void CReplayServer::Shutdown()
{
	StopRecording(); // if recording, stop now

	if ( m_MasterClient )
		m_MasterClient->Disconnect( "Replay stop." );

	g_GameEventManager.RemoveListener( this );

	// Delete the temp replay if it exists
	if ( g_pFullFileSystem->FileExists( TMP_REPLAY_FILENAME ) )
	{
		g_pFullFileSystem->RemoveFile( TMP_REPLAY_FILENAME );
	}

	CBaseServer::Shutdown();
}

int	CReplayServer::GetChallengeType ( netadr_t &adr )
{
	return PROTOCOL_HASHEDCDKEY; // Replay doesn't need Steam authentication
}

const char *CReplayServer::GetPassword() const
{
	return NULL;
}

IClient *CReplayServer::ConnectClient ( netadr_t &adr, int protocol, int challenge, int clientChallenge, int authProtocol, 
									 const char *name, const char *password, const char *hashedCDkey, int cdKeyLen )
{
	// Don't let anyone connect to the replay server
	return NULL;
}

void CReplayServer::ReplyChallenge(netadr_t &adr, int clientChallenge )
{
	// No reply for replay.
	return;
}

void CReplayServer::ReplyServerChallenge(netadr_t &adr)
{
	return;
}

void CReplayServer::RejectConnection( const netadr_t &adr, int clientChallenge, const char *s )
{
	return;
}

CBaseClient *CReplayServer::CreateFakeClient(const char *name)
{
	return NULL;
}

void CReplayServer::StartRecording()
{
	if ( m_DemoRecorder.IsRecording() )
		return;

	extern ConVar replay_debug;
	if ( replay_debug.GetBool() )	Msg( "CReplayServer::StartRecording() now, %f\n", host_time );

	m_DemoRecorder.StartRecording();
	m_flStartRecordTime = host_time;
}

void CReplayServer::StopRecording()
{
	if ( !m_DemoRecorder.IsRecording() )
		return;

	m_DemoRecorder.StopRecording();
	m_flStopRecordTime = host_time;
}

void CReplayServer::SendPendingEvents()
{
	// Did we recently stop recording?
	if ( m_flStopRecordTime != 0.0f )
	{
		// Let clients know the server has stopped recording replays
		g_pReplay->SV_SendReplayEvent( "replay_endrecord", -1 );

		// Reset stop record time
		m_flStopRecordTime = 0.0f;
	}

	// Did we recently begin recording?
	if ( m_flStartRecordTime != 0.0f )
	{
		// Send start record event to everyone
		g_pReplay->SV_SendReplayEvent( "replay_startrecord", -1 );

		// Send recording session info to everyone
		IGameEvent *pSessionInfoEvent = g_pServerReplayContext->CreateReplaySessionInfoEvent();
		if ( pSessionInfoEvent )
		{
			// Let clients know the server is ready to capture replays
			g_pReplay->SV_SendReplayEvent( pSessionInfoEvent, -1 );
		}
		else
		{
			AssertMsg( 0, "Failed to create replay_sessioninfo event!" );
		}

		// Reset the start record timer
		m_flStartRecordTime = 0.0f;
	}
}

#endif