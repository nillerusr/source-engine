//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "server_pch.h"
#include "client.h"
#include "sv_packedentities.h"
#include "bspfile.h"
#include "eiface.h"
#include "dt_send_eng.h"
#include "dt_common_eng.h"
#include "changeframelist.h"
#include "sv_main.h"
#include "hltvserver.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#endif
#include "dt_instrumentation_server.h"
#include "LocalNetworkBackdoor.h"
#include "tier0/vprof.h"
#include "host.h"
#include "networkstringtableserver.h"
#include "networkstringtable.h"
#include "utlbuffer.h"
#include "dt.h"
#include "con_nprint.h"
#include "smooth_average.h"
#include "vengineserver_impl.h"
#include "tier0/vcrmode.h"
#include "vstdlib/jobthread.h"
#include "enginethreads.h"

#ifdef SWDS
IClientEntityList *entitylist = NULL;
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sv_debugmanualmode( "sv_debugmanualmode", "0", 0, "Make sure entities correctly report whether or not their network data has changed." );

// Returns false and calls Host_Error if the edict's pvPrivateData is NULL.
static inline bool SV_EnsurePrivateData(edict_t *pEdict)
{
	if(pEdict->GetUnknown())
	{
		return true;
	}
	else
	{
		Host_Error("SV_EnsurePrivateData: pEdict->pvPrivateData==NULL (ent %d).\n", pEdict - sv.edicts);
		return false;
	}
}

// This function makes sure that this entity class has an instance baseline.
// If it doesn't have one yet, it makes a new one.
void SV_EnsureInstanceBaseline( ServerClass *pServerClass, int iEdict, const void *pData, int nBytes )
{
	edict_t *pEnt = &sv.edicts[iEdict];
	ErrorIfNot( pEnt->GetNetworkable(),
		("SV_EnsureInstanceBaseline: edict %d missing ent", iEdict)
	);

	ServerClass *pClass = pEnt->GetNetworkable()->GetServerClass();

	// See if we already have a baseline for this class.
	if ( pClass->m_InstanceBaselineIndex == INVALID_STRING_INDEX )
	{
		AUTO_LOCK( g_svInstanceBaselineMutex );

		// We need this second check in case multiple instances of the same class have grabbed the lock.
		if ( pClass->m_InstanceBaselineIndex == INVALID_STRING_INDEX )
		{
			char idString[32];
			Q_snprintf( idString, sizeof( idString ), "%d", pClass->m_ClassID );

			// Ok, make a new instance baseline so they can reference it.
			int temp = sv.GetInstanceBaselineTable()->AddString( 
				true,
				idString,	// Note we're sending a string with the ID number, not the class name.
				nBytes,
				pData );

			// Insert a compiler and/or CPU memory barrier to ensure that all side-effects have
			// been published before the index is published. Otherwise the string index may
			// be visible before its initialization has finished. This potential problem is caused
			// by the use of double-checked locking -- the problem is that the code outside of the
			// lock is looking at the variable that is protected by the lock. See this article for details:
			// http://en.wikipedia.org/wiki/Double-checked_locking
			// Write-release barrier
			ThreadMemoryBarrier();
			pClass->m_InstanceBaselineIndex = temp;
			Assert( pClass->m_InstanceBaselineIndex != INVALID_STRING_INDEX );
		}
	}
	// Read-acquire barrier
	ThreadMemoryBarrier();
}

//-----------------------------------------------------------------------------
// Pack the entity....
//-----------------------------------------------------------------------------

static inline void SV_PackEntity( 
	int edictIdx, 
	edict_t* edict, 
	ServerClass* pServerClass,
	CFrameSnapshot *pSnapshot )
{
	Assert( edictIdx < pSnapshot->m_nNumEntities );
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "PackEntities_Normal%s", __FUNCTION__ );

	int iSerialNum = pSnapshot->m_pEntities[ edictIdx ].m_nSerialNumber;

	// Check to see if this entity specifies its changes.
	// If so, then try to early out making the fullpack
	bool bUsedPrev = false;

	if ( !edict->HasStateChanged() )
	{
		// Now this may not work if we didn't previously send a packet;
		// if not, then we gotta compute it
		bUsedPrev = framesnapshotmanager->UsePreviouslySentPacket( pSnapshot, edictIdx, iSerialNum );
	}
		
	if ( bUsedPrev && !sv_debugmanualmode.GetInt() )
	{
		edict->ClearStateChanged();
		return;
	}
	
	// First encode the entity's data.
	ALIGN4 char packedData[MAX_PACKEDENTITY_DATA] ALIGN4_POST;
	bf_write writeBuf( "SV_PackEntity->writeBuf", packedData, sizeof( packedData ) );

	SendTable *pSendTable = pServerClass->m_pTable;
	
	// (avoid constructor overhead).
	unsigned char tempData[ sizeof( CSendProxyRecipients ) * MAX_DATATABLE_PROXIES ];
	CUtlMemory< CSendProxyRecipients > recip( (CSendProxyRecipients*)tempData, pSendTable->m_pPrecalc->GetNumDataTableProxies() );

	if( !SendTable_Encode( pSendTable, edict->GetUnknown(), &writeBuf, edictIdx, &recip, false ) )
	{							 
		Host_Error( "SV_PackEntity: SendTable_Encode returned false (ent %d).\n", edictIdx );
	}

#ifndef NO_VCR
	// VCR mode stuff..
	if ( vcr_verbose.GetInt() && writeBuf.GetNumBytesWritten() > 0 )
		VCRGenericValueVerify( "writebuf", writeBuf.GetBasePointer(), writeBuf.GetNumBytesWritten()-1 );
#endif

	SV_EnsureInstanceBaseline( pServerClass, edictIdx, packedData, writeBuf.GetNumBytesWritten() );
		
	int nFlatProps = SendTable_GetNumFlatProps( pSendTable );
	IChangeFrameList *pChangeFrame = NULL;

	// If this entity was previously in there, then it should have a valid IChangeFrameList 
	// which we can delta against to figure out which properties have changed.
	//
	// If not, then we want to setup a new IChangeFrameList.

	PackedEntity *pPrevFrame = framesnapshotmanager->GetPreviouslySentPacket( edictIdx, pSnapshot->m_pEntities[ edictIdx ].m_nSerialNumber );
	if ( pPrevFrame )
	{
		// Calculate a delta.
		Assert( !pPrevFrame->IsCompressed() );
		
		int deltaProps[MAX_DATATABLE_PROPS];

		int nChanges = SendTable_CalcDelta(
			pSendTable, 
			pPrevFrame->GetData(), pPrevFrame->GetNumBits(),
			packedData,	writeBuf.GetNumBitsWritten(),
			
			deltaProps,
			ARRAYSIZE( deltaProps ),

			edictIdx
			);

#ifndef NO_VCR
		if ( vcr_verbose.GetInt() )
			VCRGenericValueVerify( "nChanges", &nChanges, sizeof( nChanges ) );
#endif

		// If it's non-manual-mode, but we detect that there are no changes here, then just
		// use the previous pSnapshot if it's available (as though the entity were manual mode).
		// It would be interesting to hook here and see how many non-manual-mode entities 
		// are winding up with no changes.
		if ( nChanges == 0 )
		{
			if ( pPrevFrame->CompareRecipients( recip ) )
			{
				if ( framesnapshotmanager->UsePreviouslySentPacket( pSnapshot, edictIdx, iSerialNum ) )
				{
					edict->ClearStateChanged();
					return;
				}
			}
		}
		else
		{
			if ( !edict->HasStateChanged() )
			{
				for ( int iDeltaProp=0; iDeltaProp < nChanges; iDeltaProp++ )
				{
					Assert( pSendTable->m_pPrecalc );
					Assert( deltaProps[iDeltaProp] < pSendTable->m_pPrecalc->GetNumProps() );

					const SendProp *pProp = pSendTable->m_pPrecalc->GetProp( deltaProps[iDeltaProp] );
					// If a field changed, but it changed because it encoded against tickcount, 
					//   then it's just like the entity changed the underlying field, not an error, that is.
					if ( pProp->GetFlags() & SPROP_ENCODED_AGAINST_TICKCOUNT )
						continue;

					Msg( "Entity %d (class '%s') reported ENTITY_CHANGE_NONE but '%s' changed.\n", 
						edictIdx,
						edict->GetClassName(),
						pProp->GetName() );

				}
			}
		}

#ifndef _XBOX	
#if defined( REPLAY_ENABLED )
		if ( (hltv && hltv->IsActive()) || (replay && replay->IsActive()) )
#else
		if ( hltv && hltv->IsActive() )
#endif
		{
			// in HLTV or Replay mode every PackedEntity keeps it's own ChangeFrameList
			// we just copy the ChangeFrameList from prev frame and update it
			pChangeFrame = pPrevFrame->GetChangeFrameList();
			pChangeFrame = pChangeFrame->Copy(); // allocs and copies ChangeFrameList
		}
		else
#endif
		{
			// Ok, now snag the changeframe from the previous frame and update the 'last frame changed'
			// for the properties in the delta.
			pChangeFrame = pPrevFrame->SnagChangeFrameList();
		}
		
		ErrorIfNot( pChangeFrame,
			("SV_PackEntity: SnagChangeFrameList returned null") );
		ErrorIfNot( pChangeFrame->GetNumProps() == nFlatProps,
			("SV_PackEntity: SnagChangeFrameList mismatched number of props[%d vs %d]", nFlatProps, pChangeFrame->GetNumProps() ) );

		pChangeFrame->SetChangeTick( deltaProps, nChanges, pSnapshot->m_nTickCount );
	}
	else
	{
		// Ok, init the change frames for the first time.
		pChangeFrame = AllocChangeFrameList( nFlatProps, pSnapshot->m_nTickCount );
	}

	// Now make a PackedEntity and store the new packed data in there.
	{
		PackedEntity *pPackedEntity = framesnapshotmanager->CreatePackedEntity( pSnapshot, edictIdx );
		pPackedEntity->SetChangeFrameList( pChangeFrame );
		pPackedEntity->SetServerAndClientClass( pServerClass, NULL );
		pPackedEntity->AllocAndCopyPadded( packedData, writeBuf.GetNumBytesWritten() );
		pPackedEntity->SetRecipients( recip );
	}

	edict->ClearStateChanged();
}

// in HLTV mode we ALWAYS have to store position and PVS info, even if entity didnt change
void SV_FillHLTVData( CFrameSnapshot *pSnapshot, edict_t *edict, int iValidEdict )
{
#if !defined( _XBOX )
	if ( pSnapshot->m_pHLTVEntityData && edict )
	{
		CHLTVEntityData *pHLTVData = &pSnapshot->m_pHLTVEntityData[iValidEdict];

		PVSInfo_t *pvsInfo = edict->GetNetworkable()->GetPVSInfo();

		if ( pvsInfo->m_nClusterCount == 1 )
		{
			// store cluster, if entity spawns only over one cluster
			pHLTVData->m_nNodeCluster = pvsInfo->m_pClusters[0];
		}
		else
		{
			// otherwise save PVS head node for larger entities
			pHLTVData->m_nNodeCluster = pvsInfo->m_nHeadNode | (1<<31);
		}

		// remember origin
		pHLTVData->origin[0] = pvsInfo->m_vCenter[0];
		pHLTVData->origin[1] = pvsInfo->m_vCenter[1];
		pHLTVData->origin[2] = pvsInfo->m_vCenter[2];
	}
#endif
}

// in Replay mode we ALWAYS have to store position and PVS info, even if entity didnt change
void SV_FillReplayData( CFrameSnapshot *pSnapshot, edict_t *edict, int iValidEdict )
{
#if !defined( _XBOX )
	if ( pSnapshot->m_pReplayEntityData && edict )
	{
		CReplayEntityData *pReplayData = &pSnapshot->m_pReplayEntityData[iValidEdict];

		PVSInfo_t *pvsInfo = edict->GetNetworkable()->GetPVSInfo();

		if ( pvsInfo->m_nClusterCount == 1 )
		{
			// store cluster, if entity spawns only over one cluster
			pReplayData->m_nNodeCluster = pvsInfo->m_pClusters[0];
		}
		else
		{
			// otherwise save PVS head node for larger entities
			pReplayData->m_nNodeCluster = pvsInfo->m_nHeadNode | (1<<31);
		}

		// remember origin
		pReplayData->origin[0] = pvsInfo->m_vCenter[0];
		pReplayData->origin[1] = pvsInfo->m_vCenter[1];
		pReplayData->origin[2] = pvsInfo->m_vCenter[2];
	}
#endif
}

// Returns the SendTable that should be used with the specified edict.
SendTable* GetEntSendTable(edict_t *pEdict)
{
	if ( pEdict->GetNetworkable() )
	{
		ServerClass *pClass = pEdict->GetNetworkable()->GetServerClass();
		if ( pClass )
		{
			return pClass->m_pTable;
		}
	}

	return NULL;
}


void PackEntities_NetworkBackDoor( 
	int clientCount, 
	CGameClient **clients,
	CFrameSnapshot *snapshot )
{
	Assert( clientCount == 1 );

	VPROF_BUDGET( "PackEntities_NetworkBackDoor", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	CGameClient *client = clients[0];	// update variables cl, pInfo, frame for current client
	CCheckTransmitInfo *pInfo =  &client->m_PackInfo;

	for ( int iValidEdict=0; iValidEdict < snapshot->m_nValidEntities; iValidEdict++ )
	{
		int index = snapshot->m_pValidEntities[iValidEdict];
		edict_t* edict = &sv.edicts[ index ];
		
		// this is a bit of a hack to ensure that we get a "preview" of the
		//  packet timstamp that the server will send so that things that
		//  are encoded relative to packet time will be correct
		Assert( edict->m_NetworkSerialNumber != -1 );

		bool bShouldTransmit = pInfo->m_pTransmitEdict->Get( index ) ? true : false;

		//CServerDTITimer timer( pSendTable, SERVERDTI_ENCODE );
		// If we're using the fast path for a single-player game, just pass the entity
		// directly over to the client.
		Assert( index < snapshot->m_nNumEntities );
		ServerClass *pSVClass = snapshot->m_pEntities[ index ].m_pClass;
		g_pLocalNetworkBackdoor->EntState( index, edict->m_NetworkSerialNumber, 
			pSVClass->m_ClassID, pSVClass->m_pTable, edict->GetUnknown(), edict->HasStateChanged(), bShouldTransmit );
		edict->ClearStateChanged();
	}
	
	// Tell the client about any entities that are now dormant.
	g_pLocalNetworkBackdoor->ProcessDormantEntities();
	InvalidateSharedEdictChangeInfos();
}

static ConVar sv_parallel_packentities( "sv_parallel_packentities", "1" );

struct PackWork_t
{
	int				nIdx;
	edict_t			*pEdict;
	CFrameSnapshot	*pSnapshot;

	static void Process( PackWork_t &item )
	{
		SV_PackEntity( item.nIdx, item.pEdict, item.pSnapshot->m_pEntities[ item.nIdx ].m_pClass, item.pSnapshot );
	}
};

void PackEntities_Normal( 
	int clientCount, 
	CGameClient **clients,
	CFrameSnapshot *snapshot )
{
	Assert( snapshot->m_nValidEntities >= 0 && snapshot->m_nValidEntities <= MAX_EDICTS );
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s %d", __FUNCTION__, snapshot->m_nValidEntities );

	CUtlVectorFixed< PackWork_t, MAX_EDICTS > workItems;

	// check for all active entities, if they are seen by at least on client, if
	// so, bit pack them 
	for ( int iValidEdict=0; iValidEdict < snapshot->m_nValidEntities; ++iValidEdict )
	{
		int index = snapshot->m_pValidEntities[ iValidEdict ];
		
		Assert( index < snapshot->m_nNumEntities );

		edict_t* edict = &sv.edicts[ index ];

		// if HLTV is running save PVS info for each entity
		SV_FillHLTVData( snapshot, edict, iValidEdict );
		
		// if Replay is running save PVS info for each entity
		SV_FillReplayData( snapshot, edict, iValidEdict );

		// Check to see if the entity changed this frame...
		//ServerDTI_RegisterNetworkStateChange( pSendTable, ent->m_bStateChanged );

		for ( int iClient = 0; iClient < clientCount; ++iClient )
		{
			// entities is seen by at least this client, pack it and exit loop
			CGameClient *client = clients[iClient];	// update variables cl, pInfo, frame for current client
			CClientFrame *frame = client->m_pCurrentFrame;

			if( frame->transmit_entity.Get( index ) )
			{	
				PackWork_t w;
				w.nIdx = index;
				w.pEdict = edict;
				w.pSnapshot = snapshot;

				workItems.AddToTail( w );
				break;
			}
		}
	}

	// Process work
	if ( sv_parallel_packentities.GetBool() )
	{
		ParallelProcess( "PackWork_t::Process", workItems.Base(), workItems.Count(), &PackWork_t::Process );
	}
	else
	{
		int c = workItems.Count();
		for ( int i = 0; i < c; ++i )
		{
			PackWork_t &w = workItems[ i ];
			SV_PackEntity( w.nIdx, w.pEdict, w.pSnapshot->m_pEntities[ w.nIdx ].m_pClass, w.pSnapshot );
		}
	}

	InvalidateSharedEdictChangeInfos();
}


//-----------------------------------------------------------------------------
// Writes the compressed packet of entities to all clients
//-----------------------------------------------------------------------------

void SV_ComputeClientPacks( 
	int clientCount, 
	CGameClient **clients,
	CFrameSnapshot *snapshot )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);
	// Do some setup for each client
	{
		VPROF_BUDGET_FLAGS( "SV_ComputeClientPacks", "CheckTransmit", BUDGETFLAG_SERVER );

		for (int iClient = 0; iClient < clientCount; ++iClient)
		{
			CCheckTransmitInfo *pInfo = &clients[iClient]->m_PackInfo;
			clients[iClient]->SetupPackInfo( snapshot );
			serverGameEnts->CheckTransmit( pInfo, snapshot->m_pValidEntities, snapshot->m_nValidEntities );
			clients[iClient]->SetupPrevPackInfo();
		}
	}

	VPROF_BUDGET_FLAGS( "SV_ComputeClientPacks", "ComputeClientPacks", BUDGETFLAG_SERVER );

	if ( g_pLocalNetworkBackdoor )
	{
		// This will force network string table updates for local client to go through the backdoor if it's active
#ifdef SHARED_NET_STRING_TABLES
		sv.m_StringTables->TriggerCallbacks( clients[0]->m_nDeltaTick );
#else
		sv.m_StringTables->DirectUpdate( clients[0]->GetMaxAckTickCount() );
#endif
		
		g_pLocalNetworkBackdoor->StartEntityStateUpdate();

#ifndef SWDS
		int saveClientTicks = cl.GetClientTickCount();
		int saveServerTicks = cl.GetServerTickCount();
		bool bSaveSimulation = cl.insimulation;
		float flSaveLastServerTickTime = cl.m_flLastServerTickTime;

		cl.insimulation = true;
		cl.SetClientTickCount( sv.m_nTickCount );
		cl.SetServerTickCount( sv.m_nTickCount );

		cl.m_flLastServerTickTime = sv.m_nTickCount * host_state.interval_per_tick;
		g_ClientGlobalVariables.tickcount = cl.GetClientTickCount();
		g_ClientGlobalVariables.curtime = cl.GetTime();
#endif

		PackEntities_NetworkBackDoor( clientCount, clients, snapshot );

		g_pLocalNetworkBackdoor->EndEntityStateUpdate();

#ifndef SWDS
		cl.SetClientTickCount( saveClientTicks );
		cl.SetServerTickCount( saveServerTicks );
		cl.insimulation = bSaveSimulation;
		cl.m_flLastServerTickTime = flSaveLastServerTickTime;

		g_ClientGlobalVariables.tickcount = cl.GetClientTickCount();
		g_ClientGlobalVariables.curtime = cl.GetTime();
#endif

		PrintPartialChangeEntsList();
	}
	else
	{
		PackEntities_Normal( clientCount, clients, snapshot );
	}
}



// If the table's ID is -1, writes its info into the buffer and increments curID.
void SV_MaybeWriteSendTable( SendTable *pTable, bf_write &pBuf, bool bNeedDecoder )
{
	// Already sent?
	if ( pTable->GetWriteFlag() )
		return;

	pTable->SetWriteFlag( true );

	SVC_SendTable sndTbl;

	byte	tmpbuf[4096];
	sndTbl.m_DataOut.StartWriting( tmpbuf, sizeof(tmpbuf) );
	
	// write send table properties into message buffer
	SendTable_WriteInfos(pTable, &sndTbl.m_DataOut );

	sndTbl.m_bNeedsDecoder = bNeedDecoder;

	// write message to stream
	sndTbl.WriteToBuffer( pBuf );
}

// Calls SV_MaybeWriteSendTable recursively.
void SV_MaybeWriteSendTable_R( SendTable *pTable, bf_write &pBuf )
{
	SV_MaybeWriteSendTable( pTable, pBuf, false );

	// Make sure we send child send tables..
	for(int i=0; i < pTable->m_nProps; i++)
	{
		SendProp *pProp = &pTable->m_pProps[i];

		if( pProp->m_Type == DPT_DataTable )
			SV_MaybeWriteSendTable_R( pProp->GetDataTable(), pBuf );
	}
}


// Sets up SendTable IDs and sends an svc_sendtable for each table.
void SV_WriteSendTables( ServerClass *pClasses, bf_write &pBuf )
{
	ServerClass *pCur;

	DataTable_ClearWriteFlags( pClasses );

	// First, we send all the leaf classes. These are the ones that will need decoders
	// on the client.
	for ( pCur=pClasses; pCur; pCur=pCur->m_pNext )
	{
		SV_MaybeWriteSendTable( pCur->m_pTable, pBuf, true );
	}

	// Now, we send their base classes. These don't need decoders on the client
	// because we will never send these SendTables by themselves.
	for ( pCur=pClasses; pCur; pCur=pCur->m_pNext )
	{
		SV_MaybeWriteSendTable_R( pCur->m_pTable, pBuf );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : crc - 
//-----------------------------------------------------------------------------
void SV_ComputeClassInfosCRC( CRC32_t* crc )
{
	ServerClass *pClasses = serverGameDLL->GetAllServerClasses();

	for ( ServerClass *pClass=pClasses; pClass; pClass=pClass->m_pNext )
	{
		CRC32_ProcessBuffer( crc, (void *)pClass->m_pNetworkName, Q_strlen( pClass->m_pNetworkName ) );
		CRC32_ProcessBuffer( crc, (void *)pClass->m_pTable->GetName(), Q_strlen(pClass->m_pTable->GetName() ) );
	}
}

void CGameServer::AssignClassIds()
{
	ServerClass *pClasses = serverGameDLL->GetAllServerClasses();

	// Count the number of classes.
	int nClasses = 0;
	for ( ServerClass *pCount=pClasses; pCount; pCount=pCount->m_pNext )
	{
		++nClasses;
	}
	
	// These should be the same! If they're not, then it should detect an explicit create message.
	ErrorIfNot( nClasses <= MAX_SERVER_CLASSES,
		("CGameServer::AssignClassIds: too many server classes (%i, MAX = %i).\n", nClasses, MAX_SERVER_CLASSES );
	);

	serverclasses = nClasses;
	serverclassbits = Q_log2( serverclasses ) + 1;

	bool bSpew = CommandLine()->FindParm( "-netspike" ) != 0;

	int curID = 0;
	for ( ServerClass *pClass=pClasses; pClass; pClass=pClass->m_pNext )
	{
		pClass->m_ClassID = curID++;

		if ( bSpew )
		{
			Msg( "%d == '%s'\n", pClass->m_ClassID, pClass->GetName() );
		}
	}
}

// Assign each class and ID and write an svc_classinfo for each one.
void SV_WriteClassInfos(ServerClass *pClasses, bf_write &pBuf)
{
	// Assert( sv.serverclasses < MAX_SERVER_CLASSES );
	
	SVC_ClassInfo classinfomsg;

	classinfomsg.m_bCreateOnClient = false;
	
	for ( ServerClass *pClass=pClasses; pClass; pClass=pClass->m_pNext )
	{
		SVC_ClassInfo::class_t svclass;

		svclass.classID = pClass->m_ClassID;
		Q_strncpy( svclass.datatablename, pClass->m_pTable->GetName(), sizeof(svclass.datatablename) );
		Q_strncpy( svclass.classname, pClass->m_pNetworkName, sizeof(svclass.classname) );
				
		classinfomsg.m_Classes.AddToTail( svclass );  // add all known classes to message
	}

	classinfomsg.WriteToBuffer( pBuf );
}

// This is implemented for the datatable code so its warnings can include an object's classname.
const char* GetObjectClassName( int objectID )
{
	if ( objectID >= 0 && objectID < sv.num_edicts )
	{
		return sv.edicts[objectID].GetClassName();
	}
	else
	{
		return "[unknown]";
	}
}



