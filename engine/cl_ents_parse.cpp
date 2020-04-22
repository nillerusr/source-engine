//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Parsing of entity network packets.
//
// $NoKeywords: $
//=============================================================================//


#include "client_pch.h"
#include "con_nprint.h"
#include "iprediction.h"
#include "cl_entityreport.h"
#include "dt_recv_eng.h"
#include "net_synctags.h"
#include "ispatialpartitioninternal.h"
#include "LocalNetworkBackdoor.h"
#include "basehandle.h"
#include "dt_localtransfer.h"
#include "iprediction.h"
#include "netmessages.h"
#include "ents_shared.h"
#include "cl_ents_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar cl_flushentitypacket("cl_flushentitypacket", "0", FCVAR_CHEAT, "For debugging. Force the engine to flush an entity packet.");

// Prints important entity creation/deletion events to console
#if defined( _DEBUG )
static ConVar cl_deltatrace( "cl_deltatrace", "0", 0, "For debugging, print entity creation/deletion info to console." );
#define TRACE_DELTA( text ) if ( cl_deltatrace.GetInt() ) { ConMsg( "%s", text ); };
#else
#define TRACE_DELTA( funcs )
#endif



//-----------------------------------------------------------------------------
// Debug networking stuff.
//-----------------------------------------------------------------------------


// #define DEBUG_NETWORKING 1

#if defined( DEBUG_NETWORKING )
void SpewToFile( char const* pFmt, ... );
static ConVar cl_packettrace( "cl_packettrace", "1", 0, "For debugging, massive spew to file." );
#define TRACE_PACKET( text ) if ( cl_packettrace.GetInt() ) { SpewToFile text ; };
#else
#define TRACE_PACKET( text )
#endif


#if defined( DEBUG_NETWORKING )

//-----------------------------------------------------------------------------
// Opens the recording file
//-----------------------------------------------------------------------------

static FileHandle_t OpenRecordingFile()
{
	FileHandle_t fp = 0;
	static bool s_CantOpenFile = false;
	static bool s_NeverOpened = true;
	if (!s_CantOpenFile)
	{
		fp = g_pFileSystem->Open( "cltrace.txt", s_NeverOpened ? "wt" : "at" );
		if (!fp)
		{
			s_CantOpenFile = true;			
		}
		s_NeverOpened = false;
	}
	return fp;
}

//-----------------------------------------------------------------------------
// Records an argument for a command, flushes when the command is done
//-----------------------------------------------------------------------------

void SpewToFile( char const* pFmt, ... )
{
	static CUtlVector<unsigned char> s_RecordingBuffer;

	char temp[2048];
	va_list args;

	va_start( args, pFmt );
	int len = Q_vsnprintf( temp, sizeof( temp ), pFmt, args );
	va_end( args );
	assert( len < 2048 );

	int idx = s_RecordingBuffer.AddMultipleToTail( len );
	memcpy( &s_RecordingBuffer[idx], temp, len );
	if ( 1 ) //s_RecordingBuffer.Size() > 8192)
	{
		FileHandle_t fp = OpenRecordingFile();
		g_pFileSystem->Write( s_RecordingBuffer.Base(), s_RecordingBuffer.Size(), fp );
		g_pFileSystem->Close( fp );

		s_RecordingBuffer.RemoveAll();
	}
}

#endif // DEBUG_NETWORKING

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Frees the client DLL's binding to the object.
// Input  : iEnt - 
//-----------------------------------------------------------------------------
void CL_DeleteDLLEntity( int iEnt, const char *reason, bool bOnRecreatingAllEntities )
{
	IClientNetworkable *pNet = entitylist->GetClientNetworkable( iEnt );

	if ( pNet )
	{
		ClientClass *pClientClass = pNet->GetClientClass();
		TRACE_DELTA( va( "Trace %i (%s): delete (%s)\n", iEnt, pClientClass ? pClientClass->m_pNetworkName : "unknown", reason ) );
#ifndef _XBOX
		CL_RecordDeleteEntity( iEnt, pClientClass );
#endif
		if ( bOnRecreatingAllEntities )
		{
			pNet->SetDestroyedOnRecreateEntities();
		}

		pNet->Release();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Has the client DLL allocate its data for the object.
// Input  : iEnt - 
//			iClass - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
IClientNetworkable* CL_CreateDLLEntity( int iEnt, int iClass, int iSerialNum )
{
#if defined( _DEBUG )
	IClientNetworkable *pOldNetworkable = entitylist->GetClientNetworkable( iEnt );
	Assert( !pOldNetworkable );
#endif

	ClientClass *pClientClass;
	if ( ( pClientClass = cl.m_pServerClasses[iClass].m_pClientClass ) != NULL )
	{
		TRACE_DELTA( va( "Trace %i (%s): create\n", iEnt, pClientClass->m_pNetworkName ) );
#ifndef _XBOX
		CL_RecordAddEntity( iEnt );
#endif		

		if ( !cl.IsActive() )
		{
			COM_TimestampedLog( "cl:  create '%s'", pClientClass->m_pNetworkName );
		}

		// Create the entity.
		return pClientClass->m_pCreateFn( iEnt, iSerialNum );
	}

	Assert(false);
	return NULL;
}

void	SpewBitStream( unsigned char* pMem, int bit, int lastbit )
{
	int val = 0;
	char buf[1024];
	char* pTemp = buf;
	int bitcount = 0;
	int charIdx = 1;
	while( bit < lastbit )
	{
		int byte = bit >> 3;
		int bytebit = bit & 0x7;

		val |= ((pMem[byte] & bytebit) != 0) << bitcount;

		++bit;
		++bitcount;

		if (bitcount == 4)
		{
			if ((val >= 0) && (val <= 9))
				pTemp[charIdx] = '0' + val;
			else
				pTemp[charIdx] = 'A' + val - 0xA;
			if (charIdx == 1)
				charIdx = 0;
			else
			{
				charIdx = 1;
				pTemp += 2;
			}
			bitcount = 0;
			val = 0;
		}
	}
	if ((bitcount != 0) || (charIdx != 0))
	{
		if (bitcount > 0)
		{
			if ((val >= 0) && (val <= 9))
				pTemp[charIdx] = '0' + val;
			else
				pTemp[charIdx] = 'A' + val - 0xA;
		}
		if (charIdx == 1)
		{
			pTemp[0] = '0';
		}
		pTemp += 2;
	}
	pTemp[0] = '\0';

	TRACE_PACKET(( "    CL Bitstream %s\n", buf ));
}


inline static void CL_AddPostDataUpdateCall( CEntityReadInfo &u, int iEnt, DataUpdateType_t updateType )
{
	ErrorIfNot( u.m_nPostDataUpdateCalls < MAX_EDICTS,
		("CL_AddPostDataUpdateCall: overflowed u.m_PostDataUpdateCalls") );

	u.m_PostDataUpdateCalls[u.m_nPostDataUpdateCalls].m_iEnt = iEnt;
	u.m_PostDataUpdateCalls[u.m_nPostDataUpdateCalls].m_UpdateType = updateType;
	++u.m_nPostDataUpdateCalls;
}




//-----------------------------------------------------------------------------
// Purpose: Get the receive table for the specified entity
// Input  : *pEnt - 
// Output : RecvTable*
//-----------------------------------------------------------------------------
static inline RecvTable* GetEntRecvTable( int entnum )
{
	IClientNetworkable *pNet = entitylist->GetClientNetworkable( entnum );
	if ( pNet )
		return pNet->GetClientClass()->m_pRecvTable;
	else
		return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the entity index corresponds to a player slot 
// Input  : index - 
// Output : bool
//-----------------------------------------------------------------------------
static inline bool CL_IsPlayerIndex( int index )
{
	return ( index >= 1 && index <= cl.m_nMaxClients );
}


//-----------------------------------------------------------------------------
// Purpose: Bad data was received, just flushes incoming delta data.
//-----------------------------------------------------------------------------
void CL_FlushEntityPacket( CClientFrame *packet, char const *errorString, ... )
{
	con_nprint_t np;
	char str[2048];
	va_list marker;

	// Spit out an error.
	va_start(marker, errorString);
	Q_vsnprintf(str, sizeof(str), errorString, marker);
	va_end(marker);
	
	ConMsg("%s", str);

	np.fixed_width_font = false;
	np.time_to_live = 1.0;
	np.index = 0;
	np.color[ 0 ] = 1.0;
	np.color[ 1 ] = 0.2;
	np.color[ 2 ] = 0.0;
	Con_NXPrintf( &np, "WARNING:  CL_FlushEntityPacket, %s", str );

	// Free packet memory.
	delete packet;
}


// ----------------------------------------------------------------------------- //
// Regular handles for ReadPacketEntities.
// ----------------------------------------------------------------------------- //

void CL_CopyNewEntity( 
	CEntityReadInfo &u,
	int iClass,
	int iSerialNum
	)
{
	if ( u.m_nNewEntity < 0 || u.m_nNewEntity >= MAX_EDICTS )
	{
		Host_Error ("CL_CopyNewEntity: u.m_nNewEntity < 0 || m_nNewEntity >= MAX_EDICTS");
		return;
	}

	// If it's new, make sure we have a slot for it.
	IClientNetworkable *ent = entitylist->GetClientNetworkable( u.m_nNewEntity );

	if( iClass >= cl.m_nServerClasses )
	{
		Host_Error("CL_CopyNewEntity: invalid class index (%d).\n", iClass);
		return;
	}

	// Delete the entity.
	ClientClass *pClass = cl.m_pServerClasses[iClass].m_pClientClass;
	bool bNew = false;
	if ( ent )
	{
		// if serial number is different, destory old entity
		if ( ent->GetIClientUnknown()->GetRefEHandle().GetSerialNumber() != iSerialNum )
		{
			CL_DeleteDLLEntity( u.m_nNewEntity, "CopyNewEntity" );
			ent = NULL; // force a recreate
		}
	}

	if ( !ent )
	{	
		// Ok, it doesn't exist yet, therefore this is not an "entered PVS" message.
		ent = CL_CreateDLLEntity( u.m_nNewEntity, iClass, iSerialNum );
		if( !ent )
		{
			const char *pNetworkName = cl.m_pServerClasses[iClass].m_pClientClass ? cl.m_pServerClasses[iClass].m_pClientClass->m_pNetworkName : "";
			Host_Error( "CL_ParsePacketEntities:  Error creating entity %s(%i)\n", pNetworkName, u.m_nNewEntity );
			return;
		}

		bNew = true;
	}

	int start_bit = u.m_pBuf->GetNumBitsRead();

	DataUpdateType_t updateType = bNew ? DATA_UPDATE_CREATED : DATA_UPDATE_DATATABLE_CHANGED;
	ent->PreDataUpdate( updateType );

	// Get either the static or instance baseline.
	const void *pFromData;
	int nFromBits;

	PackedEntity *baseline = u.m_bAsDelta ? cl.GetEntityBaseline( u.m_nBaseline, u.m_nNewEntity ) : NULL;
	if ( baseline && baseline->m_pClientClass == pClass )
	{
		Assert( !baseline->IsCompressed() );
		pFromData = baseline->GetData();
		nFromBits = baseline->GetNumBits();
	}
	else
	{
		// Every entity must have a static or an instance baseline when we get here.
		ErrorIfNot(
			cl.GetClassBaseline( iClass, &pFromData, &nFromBits ),
			("CL_CopyNewEntity: GetClassBaseline(%d) failed.", iClass)
		);
		
		nFromBits *= 8; // convert to bits
	}

	// Delta from baseline and merge to new baseline
	bf_read fromBuf( "CL_CopyNewEntity->fromBuf", pFromData, Bits2Bytes(nFromBits), nFromBits );

	RecvTable *pRecvTable = GetEntRecvTable( u.m_nNewEntity );

	if( !pRecvTable )
		Host_Error( "CL_ParseDelta: invalid recv table for ent %d.\n", u.m_nNewEntity );

	if ( u.m_bUpdateBaselines )
	{
		// store this baseline in u.m_pUpdateBaselines
		ALIGN4 char packedData[MAX_PACKEDENTITY_DATA] ALIGN4_POST;
		bf_write writeBuf( "CL_CopyNewEntity->newBuf", packedData, sizeof(packedData) );

		RecvTable_MergeDeltas( pRecvTable, &fromBuf, u.m_pBuf, &writeBuf, -1, NULL, true );

		// set the other baseline
		cl.SetEntityBaseline( (u.m_nBaseline==0)?1:0, pClass, u.m_nNewEntity, packedData, writeBuf.GetNumBytesWritten() );

		fromBuf.StartReading( packedData, writeBuf.GetNumBytesWritten() );

		RecvTable_Decode( pRecvTable, ent->GetDataTableBasePtr(), &fromBuf, u.m_nNewEntity, false );

	}
	else
	{
		// write data from baseline into entity
		RecvTable_Decode( pRecvTable, ent->GetDataTableBasePtr(), &fromBuf, u.m_nNewEntity, false );

		// Now parse in the contents of the network stream.
		RecvTable_Decode( pRecvTable, ent->GetDataTableBasePtr(), u.m_pBuf, u.m_nNewEntity, true );
	}

	CL_AddPostDataUpdateCall( u, u.m_nNewEntity, updateType );

	// If ent doesn't think it's in PVS, signal that it is
	Assert( u.m_pTo->last_entity <= u.m_nNewEntity );
	u.m_pTo->last_entity = u.m_nNewEntity;
	Assert( !u.m_pTo->transmit_entity.Get(u.m_nNewEntity) );
	u.m_pTo->transmit_entity.Set( u.m_nNewEntity );

	//
	// Net stats..
	//
	int bit_count = u.m_pBuf->GetNumBitsRead() - start_bit;
#ifndef _XBOX
	if ( cl_entityreport.GetBool() )
		CL_RecordEntityBits( u.m_nNewEntity, bit_count );
#endif
	if ( CL_IsPlayerIndex( u.m_nNewEntity ) )
	{
		if ( u.m_nNewEntity == cl.m_nPlayerSlot + 1 )
		{
			u.m_nLocalPlayerBits += bit_count;
		}
		else
		{
			u.m_nOtherPlayerBits += bit_count;
		}
	}
}

void CL_PreserveExistingEntity( int nOldEntity )
{
	IClientNetworkable *pEnt = entitylist->GetClientNetworkable( nOldEntity );
	if ( !pEnt )
	{
		// If you hit this, this is because there's a networked client entity that got released
		// by some method other than a server update.  This can happen if client code calls
		// release on a networked entity.

#if defined( STAGING_ONLY )
		// Try to use the cl_removeentity_backtrace_capture code in cliententitylist.cpp...
		Msg( "%s: missing client entity %d.\n", __FUNCTION__, nOldEntity );
		Cbuf_AddText( CFmtStr( "cl_removeentity_backtrace_dump %d\n", nOldEntity ) );
		Cbuf_Execute();
#endif // STAGING_ONLY

		Host_Error( "CL_PreserveExistingEntity: missing client entity %d.\n", nOldEntity );
		return;
	}

	pEnt->OnDataUnchangedInPVS();
}

void CL_CopyExistingEntity( CEntityReadInfo &u )
{
	int start_bit = u.m_pBuf->GetNumBitsRead();

	IClientNetworkable *pEnt = entitylist->GetClientNetworkable( u.m_nNewEntity );
	if ( !pEnt )
	{
		Host_Error( "CL_CopyExistingEntity: missing client entity %d.\n", u.m_nNewEntity );
		return;
	}

	Assert( u.m_pFrom->transmit_entity.Get(u.m_nNewEntity) );

	// Read raw data from the network stream
	pEnt->PreDataUpdate( DATA_UPDATE_DATATABLE_CHANGED );

	RecvTable *pRecvTable = GetEntRecvTable( u.m_nNewEntity );

	if( !pRecvTable )
	{
		Host_Error( "CL_ParseDelta: invalid recv table for ent %d.\n", u.m_nNewEntity );
		return;
	}

	RecvTable_Decode( pRecvTable, pEnt->GetDataTableBasePtr(), u.m_pBuf, u.m_nNewEntity );

	CL_AddPostDataUpdateCall( u, u.m_nNewEntity, DATA_UPDATE_DATATABLE_CHANGED );

	u.m_pTo->last_entity = u.m_nNewEntity;
	Assert( !u.m_pTo->transmit_entity.Get(u.m_nNewEntity) );
	u.m_pTo->transmit_entity.Set( u.m_nNewEntity );

	int bit_count = u.m_pBuf->GetNumBitsRead() - start_bit;
#ifndef _XBOX
	if ( cl_entityreport.GetBool() )
		CL_RecordEntityBits( u.m_nNewEntity,  bit_count );
#endif
	if ( CL_IsPlayerIndex( u.m_nNewEntity ) )
	{
		if ( u.m_nNewEntity == cl.m_nPlayerSlot + 1 )
		{
			u.m_nLocalPlayerBits += bit_count;
		}
		else
		{
			u.m_nOtherPlayerBits += bit_count;
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CL_MarkEntitiesOutOfPVS( CBitVec<MAX_EDICTS> *pvs_flags )
{
	int highest_index = entitylist->GetHighestEntityIndex();
	// Note that we go up to and including the highest_index
	for ( int i = 0; i <= highest_index; i++ )
	{
		IClientNetworkable *ent = entitylist->GetClientNetworkable( i );
		if ( !ent )
			continue;

		// FIXME: We can remove IClientEntity here if we keep track of the
		// last frame's entity_in_pvs
		bool curstate = !ent->IsDormant();
		bool newstate = pvs_flags->Get( i )  ? true : false;

		if ( !curstate && newstate )
		{
			// Inform the client entity list that the entity entered the PVS
			ent->NotifyShouldTransmit( SHOULDTRANSMIT_START );
		}
		else if ( curstate && !newstate )
		{
			// Inform the client entity list that the entity left the PVS
			ent->NotifyShouldTransmit( SHOULDTRANSMIT_END );
#ifndef _XBOX
			CL_RecordLeavePVS( i );
#endif
		}
	}
}

static void CL_CallPostDataUpdates( CEntityReadInfo &u )
{
	for ( int i=0; i < u.m_nPostDataUpdateCalls; i++ )
	{
		MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);
		CPostDataUpdateCall *pCall = &u.m_PostDataUpdateCalls[i];
	
		IClientNetworkable *pEnt = entitylist->GetClientNetworkable( pCall->m_iEnt );
		ErrorIfNot( pEnt, 
			("CL_CallPostDataUpdates: missing ent %d", pCall->m_iEnt) );

		pEnt->PostDataUpdate( pCall->m_UpdateType );
	}
}

static float g_flLastPerfRequest = 0.0f;

static ConVar cl_debug_player_perf( "cl_debug_player_perf", "0", 0 );

//-----------------------------------------------------------------------------
// Purpose: An svc_packetentities has just been parsed, deal with the
//  rest of the data stream.  This can be a delta from the baseline or from a previous
//  client frame for this client.
// Input  : delta - 
//			*playerbits - 
// Output : void CL_ParsePacketEntities
//-----------------------------------------------------------------------------
bool CL_ProcessPacketEntities ( SVC_PacketEntities *entmsg )
{
	VPROF( "_CL_ParsePacketEntities" );

	// Packed entities for that frame
	// Allocate space for new packet info.
	CClientFrame *newFrame = cl.AllocateFrame();
	newFrame->Init( cl.GetServerTickCount() );
	CClientFrame *oldFrame = NULL;

	// if cl_flushentitypacket is set to N, the next N entity updates will be flushed
	if ( cl_flushentitypacket.GetInt() )
	{	
		// we can't use this, it is too old
		CL_FlushEntityPacket( newFrame, "Forced by cvar\n" );
		cl_flushentitypacket.SetValue( cl_flushentitypacket.GetInt() - 1 );	// Reduce the cvar.
		return false;
	}

	if ( entmsg->m_bIsDelta )
	{
		int nDeltaTicks = cl.GetServerTickCount() - entmsg->m_nDeltaFrom;
		float flDeltaSeconds = TICKS_TO_TIME( nDeltaTicks );

		// If we have cl_debug_player_perf set and we see a huge delta between what we've ack'd to the server and where it's at
		//  ask it for an instantaneous perf snapshot
		if ( cl_debug_player_perf.GetBool() &&
			( flDeltaSeconds > 0.5f ) &&							// delta is pretty out of date
			( ( realtime - g_flLastPerfRequest ) > 5.0f ) )		// haven't requested in a while
		{
			g_flLastPerfRequest = realtime;
			Warning( "Gap in server data, requesting connection perf data\n" );
			cl.SendStringCmd( "playerperf\n" );
		}

		if ( cl.GetServerTickCount() == entmsg->m_nDeltaFrom )
		{
			Host_Error( "Update self-referencing, connection dropped.\n" );
			return false;
		}

		// Otherwise, mark where we are valid to and point to the packet entities we'll be updating from.
		oldFrame = cl.GetClientFrame( entmsg->m_nDeltaFrom );

		if ( !oldFrame )
		{
			CL_FlushEntityPacket( newFrame, "Update delta not found.\n" );
			return false;
		}
	}
	else
	{
		if ( cl_debug_player_perf.GetBool() )
		{
			Warning( "Received uncompressed server update\n" );
		}

		// Clear out the client's entity states..
		for ( int i=0; i <= entitylist->GetHighestEntityIndex(); i++ )
		{
			CL_DeleteDLLEntity( i, "ProcessPacketEntities", true );
		}
	}

	// signal client DLL that we have started updating entities
	ClientDLL_FrameStageNotify( FRAME_NET_UPDATE_START );

	g_nPropsDecoded = 0;

	Assert( entmsg->m_nBaseline >= 0 && entmsg->m_nBaseline < 2 );

	if ( entmsg->m_bUpdateBaseline )
	{
		// server requested to use this snapshot as baseline update
		int nUpdateBaseline = (entmsg->m_nBaseline == 0) ? 1 : 0;
		cl.CopyEntityBaseline( entmsg->m_nBaseline, nUpdateBaseline );

		// send new baseline acknowledgement(as reliable)
		cl.m_NetChannel->SendNetMsg( CLC_BaselineAck( cl.GetServerTickCount(), entmsg->m_nBaseline ), true );
		
	}

	CEntityReadInfo u;
	u.m_pBuf = &entmsg->m_DataIn;
	u.m_pFrom = oldFrame;
	u.m_pTo = newFrame;
	u.m_bAsDelta = entmsg->m_bIsDelta;
	u.m_nHeaderCount = entmsg->m_nUpdatedEntries;
	u.m_nBaseline = entmsg->m_nBaseline;
	u.m_bUpdateBaselines = entmsg->m_bUpdateBaseline;
	
	// update the entities
	cl.ReadPacketEntities( u );

	ClientDLL_FrameStageNotify( FRAME_NET_UPDATE_POSTDATAUPDATE_START );

	// call PostDataUpdate() for each entity
	CL_CallPostDataUpdates( u );

	ClientDLL_FrameStageNotify( FRAME_NET_UPDATE_POSTDATAUPDATE_END );

	// call NotifyShouldTransmit() for entities that entered or left the PVS
	CL_MarkEntitiesOutOfPVS( &newFrame->transmit_entity );

	// adjust net channel stats

	cl.m_NetChannel->UpdateMessageStats( INetChannelInfo::LOCALPLAYER, u.m_nLocalPlayerBits );
	cl.m_NetChannel->UpdateMessageStats( INetChannelInfo::OTHERPLAYERS, u.m_nOtherPlayerBits );
	cl.m_NetChannel->UpdateMessageStats( INetChannelInfo::ENTITIES, -(u.m_nLocalPlayerBits+u.m_nOtherPlayerBits) );

 	cl.DeleteClientFrames( entmsg->m_nDeltaFrom );

	// If the client has more than 64 frames, the host will start to eat too much memory.
	// TODO: We should enforce this somehow.
	if ( MAX_CLIENT_FRAMES < cl.AddClientFrame( newFrame ) )
	{
		DevMsg( 1, "CL_ProcessPacketEntities: frame window too big (>%i)\n", MAX_CLIENT_FRAMES );	
	}

	// all update activities are finished
	ClientDLL_FrameStageNotify( FRAME_NET_UPDATE_END );

	return true;
}

/*
==================
CL_PreprocessEntities

Server information pertaining to this client only
==================
*/
namespace CDebugOverlay
{
	extern void PurgeServerOverlays( void );
}

void CL_PreprocessEntities( void )
{
	// Zero latency!!! (single player or listen server?)
	bool bIsUsingMultiplayerNetworking = NET_IsMultiplayer();
	bool bLastOutgoingCommandEqualsLastAcknowledgedCommand = cl.lastoutgoingcommand == cl.command_ack;

	// We always want to re-run prediction when using the multiplayer networking, or if we're the listen server and we get a packet
	//  before any frames have run
	if ( bIsUsingMultiplayerNetworking ||
		bLastOutgoingCommandEqualsLastAcknowledgedCommand )
	{
		//Msg( "%i/%i CL_ParseClientdata:  no latency server ack %i\n", 
		//	host_framecount, cl.tickcount,
		//	command_ack );
		CL_RunPrediction( PREDICTION_SIMULATION_RESULTS_ARRIVING_ON_SEND_FRAME );
	}

	// Copy some results from prediction back into right spot
	// Anything not sent over the network from server to client must be specified here.
	//if ( cl.last_command_ack  )
	{
		int number_of_commands_executed = ( cl.command_ack - cl.last_command_ack );

#if 0
		COM_Log( "cl.log", "Receiving frame acknowledging %i commands\n",
			number_of_commands_executed );

		COM_Log( "cl.log", "  last command number executed %i\n",
			cl.command_ack );

		COM_Log( "cl.log", "  previous last command number executed %i\n",
			cl.last_command_ack );

		COM_Log( "cl.log", "  current world frame %i\n",
			cl.m_nCurrentSequence );
#endif

		// Copy last set of changes right into current frame.
		g_pClientSidePrediction->PreEntityPacketReceived( number_of_commands_executed, cl.m_nCurrentSequence );
	}

	CDebugOverlay::PurgeServerOverlays();
}





