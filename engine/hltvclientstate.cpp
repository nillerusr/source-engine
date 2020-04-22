//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <proto_version.h>
#include <netmessages.h>
#include "hltvclientstate.h"
#include "hltvserver.h"
#include "quakedef.h"
#include "cl_main.h"
#include "host.h"
#include "dt_recv_eng.h"
#include "dt_common_eng.h"
#include "framesnapshot.h"
#include "clientframe.h"
#include "ents_shared.h"
#include "server.h"
#include "eiface.h"
#include "server_class.h"
#include "cdll_engine_int.h"
#include "sv_main.h"
#include "changeframelist.h"
#include "GameEventManager.h"
#include "dt_recv_decoder.h"
#include "utllinkedlist.h"
#include "cl_demo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// copy message data from in to out buffer
#define CopyDataInToOut(msg)									\
	int	 size = PAD_NUMBER( Bits2Bytes(msg->m_nLength), 4);		\
	byte *buffer = (byte*) stackalloc( size );					\
	msg->m_DataIn.ReadBits( buffer, msg->m_nLength );			\
	msg->m_DataOut.StartWriting( buffer, size, msg->m_nLength );\
	
static void HLTV_Callback_InstanceBaseline( void *object, INetworkStringTable *stringTable, int stringNumber, char const *newString, void const *newData )
{
	// relink server classes to instance baselines
	CHLTVServer *pHLTV = (CHLTVServer*)object;
	pHLTV->LinkInstanceBaselines();
}

extern CUtlLinkedList< CRecvDecoder *, unsigned short > g_RecvDecoders;

extern	ConVar tv_autorecord;
static	ConVar tv_autoretry( "tv_autoretry", "1", 0, "Relay proxies retry connection after network timeout" );
static	ConVar tv_timeout( "tv_timeout", "30", 0, "SourceTV connection timeout in seconds." );
		ConVar tv_snapshotrate("tv_snapshotrate", "16", 0, "Snapshots broadcasted per second" );


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////



CHLTVClientState::CHLTVClientState()
{
	m_pNewClientFrame = NULL;
	m_pCurrentClientFrame = NULL;
	m_bSaveMemory = false;
}

CHLTVClientState::~CHLTVClientState()
{

}

void CHLTVClientState::CopyNewEntity( 
	CEntityReadInfo &u,
	int iClass,
	int iSerialNum
	)
{
	ServerClass *pServerClass = SV_FindServerClass( iClass );
	Assert( pServerClass );
	
	ClientClass *pClientClass = GetClientClass( iClass );
	Assert( pClientClass );

	const int ent = u.m_nNewEntity;

	// copy class & serial
	CFrameSnapshot *pSnapshot = u.m_pTo->GetSnapshot();
	pSnapshot->m_pEntities[ent].m_nSerialNumber = iSerialNum;
	pSnapshot->m_pEntities[ent].m_pClass = pServerClass;

	// Get either the static or instance baseline.
	const void *pFromData = NULL;
	int nFromBits = 0;
	int nFromTick = 0;	// MOTODO get tick when baseline last changed

	PackedEntity *baseline = u.m_bAsDelta ? GetEntityBaseline( u.m_nBaseline, ent ) : NULL;

	if ( baseline && baseline->m_pClientClass == pClientClass )
	{
		Assert( !baseline->IsCompressed() );
		pFromData = baseline->GetData();
		nFromBits = baseline->GetNumBits();
	}
	else
	{
		// Every entity must have a static or an instance baseline when we get here.
		ErrorIfNot(
			GetClassBaseline( iClass, &pFromData, &nFromBits ),
			("HLTV_CopyNewEntity: GetDynamicBaseline(%d) failed.", iClass)
		);
		nFromBits *= 8; // convert to bits
	}

	// create new ChangeFrameList containing all properties set as changed
	int nFlatProps = SendTable_GetNumFlatProps( pServerClass->m_pTable );
	IChangeFrameList *pChangeFrame = NULL;
	
	if ( !m_bSaveMemory )
	{
		pChangeFrame = AllocChangeFrameList( nFlatProps, nFromTick );
	}

	// Now make a PackedEntity and store the new packed data in there.
	PackedEntity *pPackedEntity = framesnapshotmanager->CreatePackedEntity( pSnapshot, ent );
	pPackedEntity->SetChangeFrameList( pChangeFrame );
	pPackedEntity->SetServerAndClientClass( pServerClass, pClientClass );

	// Make space for the baseline data.
	ALIGN4 char packedData[MAX_PACKEDENTITY_DATA] ALIGN4_POST;
	bf_read fromBuf( "HLTV_ReadEnterPVS1", pFromData, Bits2Bytes( nFromBits ), nFromBits );
	bf_write writeBuf( "HLTV_ReadEnterPVS2", packedData, sizeof( packedData ) );

	int changedProps[MAX_DATATABLE_PROPS];
	
	// decode basline, is compressed against zero values 
	int nChangedProps = RecvTable_MergeDeltas( pClientClass->m_pRecvTable, &fromBuf, 
		u.m_pBuf, &writeBuf, -1, changedProps );

	// update change tick in ChangeFrameList
	if ( pChangeFrame )
	{
		pChangeFrame->SetChangeTick( changedProps, nChangedProps, pSnapshot->m_nTickCount );
	}

	if ( u.m_bUpdateBaselines )
	{
		SetEntityBaseline( (u.m_nBaseline==0)?1:0, pClientClass, u.m_nNewEntity, packedData, writeBuf.GetNumBytesWritten() );
	}

	pPackedEntity->AllocAndCopyPadded( packedData, writeBuf.GetNumBytesWritten() );

	// If ent doesn't think it's in PVS, signal that it is
	Assert( u.m_pTo->last_entity <= ent );
	u.m_pTo->last_entity = ent;
	u.m_pTo->transmit_entity.Set( ent );
}

static inline void HLTV_CopyExitingEnt( CEntityReadInfo &u )
{
	if ( !u.m_bAsDelta )  // Should never happen on a full update.
	{
		Assert(0); // cl.validsequence = 0;
		ConMsg( "WARNING: CopyExitingEnt on full update.\n" );
		u.m_UpdateType = Failed;	// break out
		return;
	}

	CFrameSnapshot *pFromSnapshot =	u.m_pFrom->GetSnapshot();	// get from snapshot
	
	const int ent = u.m_nOldEntity;
	
	CFrameSnapshot *pSnapshot = u.m_pTo->GetSnapshot(); // get to snapshot
	
	// copy ent handle, serial numbers & class info
	Assert( ent < pFromSnapshot->m_nNumEntities );
	pSnapshot->m_pEntities[ent] = pFromSnapshot->m_pEntities[ent];
	
	Assert( pSnapshot->m_pEntities[ent].m_pPackedData != INVALID_PACKED_ENTITY_HANDLE );

	// increase PackedEntity reference counter
	PackedEntity *pEntity =	framesnapshotmanager->GetPackedEntity( pSnapshot, ent );
	Assert( pEntity );
	pEntity->m_ReferenceCount++;


	Assert( u.m_pTo->last_entity <= ent );
	
	// mark flags as received
	u.m_pTo->last_entity = ent;
	u.m_pTo->transmit_entity.Set( ent );
}


//-----------------------------------------------------------------------------
// Purpose: A svc_signonnum has been received, perform a client side setup
// Output : void CL_SignonReply
//-----------------------------------------------------------------------------
bool CHLTVClientState::SetSignonState ( int state, int count )
{
	//	ConDMsg ("CL_SignonReply: %i\n", cl.signon);

	if ( !CBaseClientState::SetSignonState( state, count ) )
		return false;
	
	Assert ( m_nSignonState == state );

	switch ( m_nSignonState )
	{
		case SIGNONSTATE_CHALLENGE	:	break;
		case SIGNONSTATE_CONNECTED	:	{
											// allow longer timeout
											m_NetChannel->SetTimeout( SIGNON_TIME_OUT );

											m_NetChannel->Clear();
											// set user settings (rate etc)
											NET_SetConVar convars;
											Host_BuildConVarUpdateMessage( &convars, FCVAR_USERINFO, false );

											// let server know that we are a proxy server:
											NET_SetConVar::cvar_t acvar;
											V_strcpy_safe( acvar.name, "tv_relay" );
											V_strcpy_safe( acvar.value, "1" );
											convars.m_ConVars.AddToTail( acvar );

											m_NetChannel->SendNetMsg( convars );
										}
										break;

		case SIGNONSTATE_NEW		:	SendClientInfo();
										break;

		case SIGNONSTATE_PRESPAWN	:	break;
		
		case SIGNONSTATE_SPAWN		:	m_pHLTV->SignonComplete();
										break;

		case SIGNONSTATE_FULL		:	m_NetChannel->SetTimeout( tv_timeout.GetFloat() );
										// start new recording if autorecord is enabled
										if ( tv_autorecord.GetBool() )
										{
											hltv->m_DemoRecorder.StartAutoRecording();
											m_NetChannel->SetDemoRecorder( &hltv->m_DemoRecorder );
										}
										break;

		case SIGNONSTATE_CHANGELEVEL:	m_pHLTV->Changelevel();
										m_NetChannel->SetTimeout( SIGNON_TIME_OUT );  // allow 5 minutes timeout
										break;
	}

	if ( m_nSignonState >= SIGNONSTATE_CONNECTED )
	{
		// tell server that we entered now that state
		NET_SignonState signonState(  m_nSignonState, count );
		m_NetChannel->SendNetMsg( signonState );
	}

	return true;
}

void CHLTVClientState::SendClientInfo( void )
{
	CLC_ClientInfo info;
	
	info.m_nSendTableCRC = SendTable_GetCRC();
	info.m_nServerCount = m_nServerCount;
	info.m_bIsHLTV = true;
#if defined( REPLAY_ENABLED )
	info.m_bIsReplay = false;
#endif
	info.m_nFriendsID = 0;
	info.m_FriendsName[0] = 0;

	// CheckOwnCustomFiles(); // load & verfiy custom player files

	for ( int i=0; i< MAX_CUSTOM_FILES; i++ )
		info.m_nCustomFiles[i] = 0;
	
	m_NetChannel->SendNetMsg( info );
}


void CHLTVClientState::SendPacket()
{
	if ( !IsConnected() )
		return;

	if ( ( net_time < m_flNextCmdTime ) || !m_NetChannel->CanPacket() )  
		return;
	
	if ( IsActive() )
	{
		NET_Tick tick( m_nDeltaTick, host_frametime_unbounded, host_frametime_stddeviation );
		m_NetChannel->SendNetMsg( tick );
	}

	m_NetChannel->SendDatagram( NULL );

	if ( IsActive() )
	{
		// use full update rate when active
		float commandInterval = (2.0f/3.0f) / tv_snapshotrate.GetInt();
		float maxDelta = min ( host_state.interval_per_tick, commandInterval );
		float delta = clamp( (float)(net_time - m_flNextCmdTime), 0.0f, maxDelta );
		m_flNextCmdTime = net_time + commandInterval - delta;
	}
	else
	{
		// during signon process send only 5 packets/second
		m_flNextCmdTime = net_time + ( 1.0f / 5.0f );
	}
}

bool CHLTVClientState::ProcessStringCmd(NET_StringCmd *msg)
{
	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessSetConVar(NET_SetConVar *msg)
{
	if ( !CBaseClientState::ProcessSetConVar( msg ) )
		return false;

	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

void CHLTVClientState::Clear()
{
	CBaseClientState::Clear();

	m_pNewClientFrame = NULL;
	m_pCurrentClientFrame = NULL;
}

bool CHLTVClientState::ProcessServerInfo(SVC_ServerInfo *msg )
{
	// Reset client state
	Clear();

	// is server a HLTV proxy or demo file ?
	if ( !m_pHLTV->IsPlayingBack() )
	{
		if ( !msg->m_bIsHLTV )
		{
			ConMsg ( "Server (%s) is not a SourceTV proxy.\n", m_NetChannel->GetAddress() );
			Disconnect( "Server is not a SourceTV proxy", true );
			return false; 
		}	
	}

	// tell HLTV relay to clear everything
	m_pHLTV->StartRelay();

	// Process the message
	if ( !CBaseClientState::ProcessServerInfo( msg ) )
	{
		Disconnect( "CBaseClientState::ProcessServerInfo failed", true );
		return false;
	}

	m_StringTableContainer = m_pHLTV->m_StringTables;

	Assert( m_StringTableContainer->GetNumTables() == 0); // must be empty

#ifndef SHARED_NET_STRING_TABLES
	// relay uses normal string tables without a change history
	m_StringTableContainer->EnableRollback( false );
#endif

	// copy setting from HLTV client to HLTV server 
	m_pHLTV->m_nGameServerMaxClients = m_nMaxClients;
	m_pHLTV->serverclasses		= m_nServerClasses;
	m_pHLTV->serverclassbits	= m_nServerClassBits;
	m_pHLTV->m_nPlayerSlot		= m_nPlayerSlot;
	
	// copy other settings to HLTV server
	V_memcpy( m_pHLTV->worldmapMD5.bits, msg->m_nMapMD5.bits, MD5_DIGEST_LENGTH );
	m_pHLTV->m_flTickInterval	= msg->m_fTickInterval;

	host_state.interval_per_tick = msg->m_fTickInterval;
		
	Q_strncpy( m_pHLTV->m_szMapname, msg->m_szMapName, sizeof(m_pHLTV->m_szMapname) );
	Q_strncpy( m_pHLTV->m_szSkyname, msg->m_szSkyName, sizeof(m_pHLTV->m_szSkyname) );

	return true;
}

bool CHLTVClientState::ProcessClassInfo( SVC_ClassInfo *msg )
{
	if ( !msg->m_bCreateOnClient )
	{
		ConMsg("HLTV SendTable CRC differs from server.\n");
		Disconnect( "HLTV SendTable CRC differs from server.", true );
		return false;
	}

#ifdef _HLTVTEST
	RecvTable_Term( false );
#endif

	// Create all of the send tables locally
	DataTable_CreateClientTablesFromServerTables();

	// Now create all of the server classes locally, too
	DataTable_CreateClientClassInfosFromServerClasses( this );

	LinkClasses();	// link server and client classes

#ifdef DEDICATED
	bool bAllowMismatches = false;
#else
	bool bAllowMismatches = ( demoplayer && demoplayer->IsPlayingBack() );
#endif // DEDICATED

	if ( !RecvTable_CreateDecoders( serverGameDLL->GetStandardSendProxies(), bAllowMismatches ) ) // create receive table decoders
	{
		Host_EndGame( true, "CL_ParseClassInfo_EndClasses: CreateDecoders failed.\n" );
		return false;
	}

	return true;
}

void CHLTVClientState::PacketEnd( void )
{
	// did we get a snapshot with this packet ?
	if ( m_pNewClientFrame )
	{
		// if so, add a new frame to HLTV
		m_pCurrentClientFrame = m_pHLTV->AddNewFrame( m_pNewClientFrame );
		delete m_pNewClientFrame; // release own refernces
		m_pNewClientFrame = NULL;
	}
}

bool CHLTVClientState::HookClientStringTable( char const *tableName )
{
	INetworkStringTable *table = GetStringTable( tableName );
	if ( !table )
		return false;

	// Hook instance baseline table
	if ( !Q_strcasecmp( tableName, INSTANCE_BASELINE_TABLENAME ) )
	{
		table->SetStringChangedCallback( m_pHLTV,  HLTV_Callback_InstanceBaseline );
		return true;
	}

	return false;
}

void CHLTVClientState::InstallStringTableCallback( char const *tableName )
{
	INetworkStringTable *table = GetStringTable( tableName );

	if ( !table )
		return;

	// Hook instance baseline table
	if ( !Q_strcasecmp( tableName, INSTANCE_BASELINE_TABLENAME ) )
	{
		table->SetStringChangedCallback( m_pHLTV,  HLTV_Callback_InstanceBaseline );
		return;
	}
}

bool CHLTVClientState::ProcessSetView( SVC_SetView *msg )
{
	if ( !CBaseClientState::ProcessSetView( msg ) )
		return false;

	return m_pHLTV->SendNetMsg( *msg );
}

bool CHLTVClientState::ProcessVoiceInit( SVC_VoiceInit *msg )
{
	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessVoiceData( SVC_VoiceData *msg )
{
	int	 size = PAD_NUMBER( Bits2Bytes(msg->m_nLength), 4);
	byte *buffer = (byte*) stackalloc( size );
	msg->m_DataIn.ReadBits( buffer, msg->m_nLength );
	msg->m_DataOut = buffer;

	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessSounds(SVC_Sounds *msg)
{
	CopyDataInToOut( msg );

	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessPrefetch( SVC_Prefetch *msg )
{
	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessFixAngle( SVC_FixAngle *msg )
{
	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessCrosshairAngle( SVC_CrosshairAngle *msg )
{
	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessBSPDecal( SVC_BSPDecal *msg )
{
	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessGameEvent( SVC_GameEvent *msg )
{
	bf_read tmpBuf = msg->m_DataIn;

	IGameEvent *event = g_GameEventManager.UnserializeEvent( &tmpBuf );

	if ( event )
	{
		const char *pszName = event->GetName();

		bool bDontForward = false;

		if ( Q_strcmp( pszName, "hltv_status" ) == 0 )
		{
			m_pHLTV->m_nGlobalSlots = event->GetInt("slots");;
			m_pHLTV->m_nGlobalProxies = event->GetInt("proxies");
			m_pHLTV->m_nGlobalClients = event->GetInt("clients");
			m_pHLTV->m_RootServer.SetFromString( event->GetString("master") );
			bDontForward = true;
		}
		else if ( Q_strcmp( pszName, "hltv_title" ) == 0 )
		{
			// ignore title messages
			bDontForward = true;
		}

		// free event resources
		g_GameEventManager.FreeEvent( event );

		if ( bDontForward )
			return true;
	}

	// forward event
	CopyDataInToOut( msg );

	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessGameEventList( SVC_GameEventList *msg )
{
	// copy message before processing
	SVC_GameEventList tmpMsg = *msg;
	CBaseClientState::ProcessGameEventList( &tmpMsg );

	CopyDataInToOut( msg );

	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessUserMessage( SVC_UserMessage *msg )
{
	CopyDataInToOut( msg );

	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessEntityMessage( SVC_EntityMessage *msg )
{
	CopyDataInToOut( msg );

	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessMenu( SVC_Menu *msg )
{
	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

bool CHLTVClientState::ProcessPacketEntities( SVC_PacketEntities *entmsg )
{
	CClientFrame *oldFrame = NULL;

#ifdef _HLTVTEST
	if ( g_RecvDecoders.Count() == 0 )
		return false;
#endif

	if ( entmsg->m_bIsDelta )
	{
		if ( GetServerTickCount() == entmsg->m_nDeltaFrom )
		{
			Host_Error( "Update self-referencing, connection dropped.\n" );
			return false;
		}

		// Otherwise, mark where we are valid to and point to the packet entities we'll be updating from.
		oldFrame = m_pHLTV->GetClientFrame( entmsg->m_nDeltaFrom );
	}

	// create new empty snapshot
	CFrameSnapshot* pSnapshot = framesnapshotmanager->CreateEmptySnapshot( GetServerTickCount(), entmsg->m_nMaxEntries );

	Assert( m_pNewClientFrame == NULL );
	
	m_pNewClientFrame = new CClientFrame( pSnapshot );

	Assert( entmsg->m_nBaseline >= 0 && entmsg->m_nBaseline < 2 );

	if ( entmsg->m_bUpdateBaseline )
	{
		// server requested to use this snapshot as baseline update
		int nUpdateBaseline = (entmsg->m_nBaseline == 0) ? 1 : 0;
		CopyEntityBaseline( entmsg->m_nBaseline, nUpdateBaseline );

		// send new baseline acknowledgement(as reliable)
		CLC_BaselineAck baseline( GetServerTickCount(), entmsg->m_nBaseline );
		m_NetChannel->SendNetMsg( baseline, true );
	}

	// copy classes and serial numbers from current frame
	if ( m_pCurrentClientFrame )
	{
		CFrameSnapshot* pLastSnapshot = m_pCurrentClientFrame->GetSnapshot();
		CFrameSnapshotEntry *pEntry = pSnapshot->m_pEntities;
		CFrameSnapshotEntry *pLastEntry = pLastSnapshot->m_pEntities;

		Assert( pLastSnapshot->m_nNumEntities <= pSnapshot->m_nNumEntities );

		for ( int i = 0; i<pLastSnapshot->m_nNumEntities; i++ )
		{
			pEntry->m_nSerialNumber = pLastEntry->m_nSerialNumber; 
			pEntry->m_pClass = pLastEntry->m_pClass;

			pEntry++;
			pLastEntry++;
		}
	}

	CEntityReadInfo u;
	u.m_pBuf = &entmsg->m_DataIn;
	u.m_pFrom = oldFrame;
	u.m_pTo = m_pNewClientFrame;
	u.m_bAsDelta = entmsg->m_bIsDelta;
	u.m_nHeaderCount = entmsg->m_nUpdatedEntries;
	u.m_nBaseline = entmsg->m_nBaseline;
	u.m_bUpdateBaselines = entmsg->m_bUpdateBaseline;

	ReadPacketEntities( u );

	// adjust reference count to be 1
	pSnapshot->ReleaseReference();

	return CBaseClientState::ProcessPacketEntities( entmsg );
}

bool CHLTVClientState::ProcessTempEntities( SVC_TempEntities *msg )
{
	CopyDataInToOut( msg );

	return m_pHLTV->SendNetMsg( *msg ); // relay to server
}

void CHLTVClientState::ReadEnterPVS( CEntityReadInfo &u )
{
	int iClass = u.m_pBuf->ReadUBitLong( m_nServerClassBits );

	int iSerialNum = u.m_pBuf->ReadUBitLong( NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS );

	CopyNewEntity( u, iClass, iSerialNum );

	if ( u.m_nNewEntity == u.m_nOldEntity ) // that was a recreate
		u.NextOldEntity();
}

void CHLTVClientState::ReadLeavePVS( CEntityReadInfo &u )
{
	// do nothing, this entity was removed
	Assert( !u.m_pTo->transmit_entity.Get(u.m_nOldEntity) );

	if ( u.m_UpdateFlags & FHDR_DELETE )
	{
		CFrameSnapshot *pSnapshot = u.m_pTo->GetSnapshot();
		CFrameSnapshotEntry *pEntry = &pSnapshot->m_pEntities[u.m_nOldEntity];

		// clear entity references
		pEntry->m_nSerialNumber = -1;
		pEntry->m_pClass = NULL;
		Assert( pEntry->m_pPackedData == INVALID_PACKED_ENTITY_HANDLE );
	}

	u.NextOldEntity();

}

void CHLTVClientState::ReadDeltaEnt( CEntityReadInfo &u )
{
	const int i = u.m_nNewEntity;
	CFrameSnapshot *pFromSnapshot =	u.m_pFrom->GetSnapshot();

	CFrameSnapshot *pSnapshot = u.m_pTo->GetSnapshot();

	Assert( i < pFromSnapshot->m_nNumEntities );
	pSnapshot->m_pEntities[i] = pFromSnapshot->m_pEntities[i];
	
	PackedEntity *pToPackedEntity = framesnapshotmanager->CreatePackedEntity( pSnapshot, i );

	// WARNING! get pFromPackedEntity after new pPackedEntity has been created, otherwise pointer may be wrong
	PackedEntity *pFromPackedEntity = framesnapshotmanager->GetPackedEntity( pFromSnapshot, i );

	pToPackedEntity->SetServerAndClientClass( pFromPackedEntity->m_pServerClass, pFromPackedEntity->m_pClientClass );

	// create a copy of the pFromSnapshot ChangeFrameList
	IChangeFrameList* pChangeFrame = NULL;
 
	if ( !m_bSaveMemory )
	{
		pChangeFrame = pFromPackedEntity->GetChangeFrameList()->Copy();
		pToPackedEntity->SetChangeFrameList( pChangeFrame );
	}

	// Make space for the baseline data.
	ALIGN4 char packedData[MAX_PACKEDENTITY_DATA] ALIGN4_POST;
	const void *pFromData;
	int nFromBits;

	if ( pFromPackedEntity->IsCompressed() )
	{
		pFromData = m_pHLTV->UncompressPackedEntity( pFromPackedEntity, nFromBits );
	}
	else
	{
		pFromData = pFromPackedEntity->GetData();
		nFromBits = pFromPackedEntity->GetNumBits();
	}

	bf_read fromBuf( "HLTV_ReadEnterPVS1", pFromData, Bits2Bytes( nFromBits ), nFromBits );
	bf_write writeBuf( "HLTV_ReadEnterPVS2", packedData, sizeof( packedData ) );

	int changedProps[MAX_DATATABLE_PROPS];
	
	// decode baseline, is compressed against zero values 
	int nChangedProps = RecvTable_MergeDeltas( pToPackedEntity->m_pClientClass->m_pRecvTable,
		&fromBuf, u.m_pBuf, &writeBuf, -1, changedProps, false );

	// update change tick in ChangeFrameList
	if ( pChangeFrame )
	{
		pChangeFrame->SetChangeTick( changedProps, nChangedProps, pSnapshot->m_nTickCount );
	}

	if ( m_bSaveMemory )
	{
		int bits = writeBuf.GetNumBitsWritten();

		const char *compressedData = m_pHLTV->CompressPackedEntity( 
			pToPackedEntity->m_pServerClass,
			(char*)writeBuf.GetData(),
			bits );

		// store as compressed data and don't use mem pools
		pToPackedEntity->AllocAndCopyPadded( compressedData, Bits2Bytes(bits) );
		pToPackedEntity->SetCompressed();
	}
	else
	{
		// store as normal
		pToPackedEntity->AllocAndCopyPadded( packedData, writeBuf.GetNumBytesWritten() );
	}

	u.m_pTo->last_entity = u.m_nNewEntity;
	u.m_pTo->transmit_entity.Set( u.m_nNewEntity );

	u.NextOldEntity();
}

void CHLTVClientState::ReadPreserveEnt( CEntityReadInfo &u )
{
	// copy one of the old entities over to the new packet unchanged

	// XXX(JohnS): This was historically checking for NewEntity overflow, though this path does not care (and new entity
	//             may be -1).  The old entity bounds check here seems like what was intended, but since nNewEntity
	//             should not be overflowed either, I've left that check in case it was guarding against a case I am
	//             overlooking.
	if ( u.m_nOldEntity >= MAX_EDICTS || u.m_nOldEntity < 0 || u.m_nNewEntity >= MAX_EDICTS )
	{
		Host_Error( "CL_ReadPreserveEnt: Entity out of bounds. Old: %i, New: %i",
		            u.m_nOldEntity, u.m_nNewEntity );
	}

	HLTV_CopyExitingEnt( u );

	u.NextOldEntity();
}

void CHLTVClientState::ReadDeletions( CEntityReadInfo &u )
{
	while ( u.m_pBuf->ReadOneBit()!=0 )
	{
		int idx = u.m_pBuf->ReadUBitLong( MAX_EDICT_BITS );
		
		Assert( !u.m_pTo->transmit_entity.Get( idx ) );
		
		CFrameSnapshot *pSnapshot = u.m_pTo->GetSnapshot();
		CFrameSnapshotEntry *pEntry = &pSnapshot->m_pEntities[idx];

		// clear entity references
		pEntry->m_nSerialNumber = -1;
		pEntry->m_pClass = NULL;
		Assert( pEntry->m_pPackedData == INVALID_PACKED_ENTITY_HANDLE );
	}
}

int CHLTVClientState::GetConnectionRetryNumber() const
{
	if ( tv_autoretry.GetBool() )
	{
		// in autoretry mode try extra long
		return CL_CONNECTION_RETRIES * 4;
	}
	else
	{
		return CL_CONNECTION_RETRIES;
	}
}

void CHLTVClientState::ConnectionCrashed(const char *reason)
{
	CBaseClientState::ConnectionCrashed( reason );

	if ( tv_autoretry.GetBool() && m_szRetryAddress[0] )
	{
		Cbuf_AddText( va( "tv_relay %s\n", m_szRetryAddress ) );
	}
}

void CHLTVClientState::ConnectionClosing( const char *reason )
{
	CBaseClientState::ConnectionClosing( reason );

	if ( tv_autoretry.GetBool() && m_szRetryAddress[0] )
	{
		Cbuf_AddText( va( "tv_relay %s\n", m_szRetryAddress ) );
	}
}

void CHLTVClientState::RunFrame()
{
	CBaseClientState::RunFrame();

	if ( m_NetChannel && m_NetChannel->IsTimedOut() && IsConnected() )
	{
		ConMsg ("\nSourceTV connection timed out.\n");
		Disconnect( "nSourceTV connection timed out", true );
		return;
	}

	UpdateStats();
}

void CHLTVClientState::UpdateStats()
{
	if ( m_nSignonState < SIGNONSTATE_FULL )
	{
		m_fNextSendUpdateTime = 0.0f;
		return;
	}

	if ( m_fNextSendUpdateTime > net_time )
		return;

	m_fNextSendUpdateTime = net_time + 8.0f;

	int proxies, slots, clients;

	m_pHLTV->GetRelayStats( proxies, slots, clients );

	proxies += 1; // add self to number of proxies
	slots += m_pHLTV->GetMaxClients();	// add own slots
	clients += m_pHLTV->GetNumClients(); // add own clients

	NET_SetConVar conVars;
	NET_SetConVar::cvar_t acvar;

	Q_strncpy( acvar.name, "hltv_proxies", sizeof(acvar.name) );
	Q_snprintf( acvar.value, sizeof(acvar.value), "%d", proxies  );
	conVars.m_ConVars.AddToTail( acvar );

	Q_strncpy( acvar.name, "hltv_clients", sizeof(acvar.name) );
	Q_snprintf( acvar.value, sizeof(acvar.value), "%d", clients );
	conVars.m_ConVars.AddToTail( acvar );

	Q_strncpy( acvar.name, "hltv_slots", sizeof(acvar.name) );
	Q_snprintf( acvar.value, sizeof(acvar.value), "%d", slots );
	conVars.m_ConVars.AddToTail( acvar );

	Q_strncpy( acvar.name, "hltv_addr", sizeof(acvar.name) );
	Q_snprintf( acvar.value, sizeof(acvar.value), "%s:%u", net_local_adr.ToString(true), m_pHLTV->GetUDPPort()  );
	conVars.m_ConVars.AddToTail( acvar );

	m_NetChannel->SendNetMsg( conVars );
}
	
