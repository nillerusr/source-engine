//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "tier0/icommandline.h"
#include "dt_stack.h"
#include "client.h"
#include "host.h"
#include "utllinkedlist.h"
#include "server.h"
#include "server_class.h"
#include "eiface.h"
#include "demo.h"
#include "sv_packedentities.h"

#ifndef DEDICATED
#include "renamed_recvtable_compat.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern CUtlLinkedList< CClientSendTable*, unsigned short > g_ClientSendTables;
extern CUtlLinkedList< CRecvDecoder *, unsigned short > g_RecvDecoders;

RecvTable* FindRecvTable( const char *pName );

RecvTable *DataTable_FindRenamedTable( const char *pOldTableName )
{
#ifdef DEDICATED
	return NULL;
#else
	extern IBaseClientDLL *g_ClientDLL;
	if ( !g_ClientDLL )
		return NULL;

	// Get the renamed receive table list from the client DLL and see if we can find
	// a new name (assuming it was renamed at all).
	const CRenamedRecvTableInfo *pCur = g_ClientDLL->GetRenamedRecvTableInfos();

	// This should be a very short list, so we'll do string compares until 2020 when
	// someone finds this code and the list has grown to 10,000.
	while ( pCur && pCur->m_pOldName && pCur->m_pNewName )
	{
		if ( !V_stricmp( pCur->m_pOldName, pOldTableName ) )
		{
			return FindRecvTable( pCur->m_pNewName );
		}

		pCur = pCur->m_pNext;
	}

	return NULL;
#endif
}

bool DataTable_SetupReceiveTableFromSendTable( SendTable *sendTable, bool bNeedsDecoder )
{
	CClientSendTable *pClientSendTable = new CClientSendTable;
	SendTable *pTable = &pClientSendTable->m_SendTable;
	g_ClientSendTables.AddToTail( pClientSendTable );

	// Read the name.
	pTable->m_pNetTableName = COM_StringCopy( sendTable->m_pNetTableName );

	// Create a decoder for it if necessary.
	if ( bNeedsDecoder )
	{
		// Make a decoder for it.
		CRecvDecoder *pDecoder = new CRecvDecoder;
		g_RecvDecoders.AddToTail( pDecoder );
		
		RecvTable *pRecvTable = FindRecvTable( pTable->m_pNetTableName );
		if ( !pRecvTable )
		{
			// Attempt to find a renamed version of the table.
			pRecvTable = DataTable_FindRenamedTable( pTable->m_pNetTableName );
			if ( !pRecvTable )
			{
				DataTable_Warning( "No matching RecvTable for SendTable '%s'.\n", pTable->m_pNetTableName );
				return false;
			}
		}

		pRecvTable->m_pDecoder = pDecoder;
		pDecoder->m_pTable = pRecvTable;

		pDecoder->m_pClientSendTable = pClientSendTable;
		pDecoder->m_Precalc.m_pSendTable = pClientSendTable->GetSendTable();
		pClientSendTable->GetSendTable()->m_pPrecalc = &pDecoder->m_Precalc;

		// Initialize array properties.
		SetupArrayProps_R<RecvTable, RecvTable::PropType>( pRecvTable );
	}

	// Read the property list.
	pTable->m_nProps = sendTable->m_nProps;
	pTable->m_pProps = pTable->m_nProps ? new SendProp[ pTable->m_nProps ] : 0;
	pClientSendTable->m_Props.SetSize( pTable->m_nProps );

	for ( int iProp=0; iProp < pTable->m_nProps; iProp++ )
	{
		CClientSendProp *pClientProp = &pClientSendTable->m_Props[iProp];
		SendProp *pProp = &pTable->m_pProps[iProp];
		const SendProp *pSendTableProp = &sendTable->m_pProps[ iProp ];

		pProp->m_Type = (SendPropType)pSendTableProp->m_Type;
		pProp->m_pVarName = COM_StringCopy( pSendTableProp->GetName() );
		pProp->SetFlags( pSendTableProp->GetFlags() );

		if ( CommandLine()->FindParm("-dti" ) && pSendTableProp->GetParentArrayPropName() )
		{
			pProp->m_pParentArrayPropName = COM_StringCopy( pSendTableProp->GetParentArrayPropName() );
		}

		if ( pProp->m_Type == DPT_DataTable )
		{
			const char *pDTName = pSendTableProp->m_pExcludeDTName; // HACK

			if ( pSendTableProp->GetDataTable() )
				pDTName = pSendTableProp->GetDataTable()->m_pNetTableName;

			Assert( pDTName && Q_strlen(pDTName) > 0 );

			pClientProp->SetTableName( COM_StringCopy( pDTName ) );
			
			// Normally we wouldn't care about this but we need to compare it against 
			// proxies in the server DLL in SendTable_BuildHierarchy.
			pProp->SetDataTableProxyFn( pSendTableProp->GetDataTableProxyFn() );
			pProp->SetOffset( pSendTableProp->GetOffset() );
		}
		else
		{
			if ( pProp->IsExcludeProp() )
			{
				pProp->m_pExcludeDTName = COM_StringCopy( pSendTableProp->GetExcludeDTName() );
			}
			else if ( pProp->GetType() == DPT_Array )
			{
				pProp->SetNumElements( pSendTableProp->GetNumElements() );
			}
			else
			{
				pProp->m_fLowValue = pSendTableProp->m_fLowValue;
				pProp->m_fHighValue = pSendTableProp->m_fHighValue;
				pProp->m_nBits = pSendTableProp->m_nBits;
			}
		}
	}

	return true;
}

// If the table's ID is -1, writes its info into the buffer and increments curID.
void DataTable_MaybeCreateReceiveTable( CUtlVector< SendTable * >& visited, SendTable *pTable, bool bNeedDecoder )
{
	// Already sent?
	if ( visited.Find( pTable ) != visited.InvalidIndex() )
		return;

	visited.AddToTail( pTable );

	DataTable_SetupReceiveTableFromSendTable( pTable, bNeedDecoder );
}


void DataTable_MaybeCreateReceiveTable_R( CUtlVector< SendTable * >& visited, SendTable *pTable )
{
	DataTable_MaybeCreateReceiveTable( visited, pTable, false );

	// Make sure we send child send tables..
	for(int i=0; i < pTable->m_nProps; i++)
	{
		SendProp *pProp = &pTable->m_pProps[i];

		if( pProp->m_Type == DPT_DataTable )
		{
			DataTable_MaybeCreateReceiveTable_R( visited, pProp->GetDataTable() );
		}
	}
}

void DataTable_CreateClientTablesFromServerTables()
{
	if ( !serverGameDLL )
	{
		Sys_Error( "DataTable_CreateClientTablesFromServerTables:  No serverGameDLL loaded!" );
	}

	ServerClass *pClasses = serverGameDLL->GetAllServerClasses();
	ServerClass *pCur;

	CUtlVector< SendTable * > visited;

	// First, we send all the leaf classes. These are the ones that will need decoders
	// on the client.
	for ( pCur=pClasses; pCur; pCur=pCur->m_pNext )
	{
		DataTable_MaybeCreateReceiveTable( visited, pCur->m_pTable, true );
	}

	// Now, we send their base classes. These don't need decoders on the client
	// because we will never send these SendTables by themselves.
	for ( pCur=pClasses; pCur; pCur=pCur->m_pNext )
	{
		DataTable_MaybeCreateReceiveTable_R( visited, pCur->m_pTable );
	}
}

void DataTable_CreateClientClassInfosFromServerClasses( CBaseClientState *pState )
{
	if ( !serverGameDLL )
	{
		Sys_Error( "DataTable_CreateClientClassInfosFromServerClasses:  No serverGameDLL loaded!" );
	}

	ServerClass *pClasses = serverGameDLL->GetAllServerClasses();

	// Count the number of classes.
	int nClasses = 0;
	for ( ServerClass *pCount=pClasses; pCount; pCount=pCount->m_pNext )
	{
		++nClasses;
	}

	// Remove old
	if ( pState->m_pServerClasses )
	{
		delete [] pState->m_pServerClasses;
	}

	Assert( nClasses > 0 );

	pState->m_nServerClasses = nClasses;
	pState->m_pServerClasses = new C_ServerClassInfo[ pState->m_nServerClasses ];
	if ( !pState->m_pServerClasses )
	{
		Host_EndGame(true, "CL_ParseClassInfo: can't allocate %d C_ServerClassInfos.\n", pState->m_nServerClasses);
		return;
	}

	// Now fill in the entries
	int curID = 0;
	for ( ServerClass *pClass=pClasses; pClass; pClass=pClass->m_pNext )
	{
		Assert( pClass->m_ClassID >= 0 && pClass->m_ClassID < nClasses );

		pClass->m_ClassID = curID++;

		pState->m_pServerClasses[ pClass->m_ClassID ].m_ClassName = COM_StringCopy( pClass->m_pNetworkName );
		pState->m_pServerClasses[ pClass->m_ClassID ].m_DatatableName = COM_StringCopy( pClass->m_pTable->GetName() );
	}
}

// If the table's ID is -1, writes its info into the buffer and increments curID.
void DataTable_MaybeWriteSendTableBuffer( SendTable *pTable, bf_write *pBuf, bool bNeedDecoder )
{
	// Already sent?
	if ( pTable->GetWriteFlag() )
		return;

	pTable->SetWriteFlag( true );

	pBuf->WriteOneBit( 1 ); // next SendTable follows
	pBuf->WriteOneBit( bNeedDecoder?1:0 );

	SendTable_WriteInfos( pTable, pBuf );
}

// Calls DataTable_MaybeWriteSendTable recursively.
void DataTable_MaybeWriteSendTableBuffer_R( SendTable *pTable, bf_write *pBuf )
{
	DataTable_MaybeWriteSendTableBuffer( pTable, pBuf, false );

	// Make sure we send child send tables..
	for(int i=0; i < pTable->m_nProps; i++)
	{
		SendProp *pProp = &pTable->m_pProps[i];

		if( pProp->m_Type == DPT_DataTable )
		{
			DataTable_MaybeWriteSendTableBuffer_R( pProp->GetDataTable(), pBuf );
		}
	}
}

void DataTable_ClearWriteFlags_R( SendTable *pTable )
{
	pTable->SetWriteFlag( false );

	for(int i=0; i < pTable->m_nProps; i++)
	{
		SendProp *pProp = &pTable->m_pProps[i];

		if( pProp->m_Type == DPT_DataTable )
		{
			DataTable_ClearWriteFlags_R( pProp->GetDataTable() );
		}
	}
}

void DataTable_ClearWriteFlags( ServerClass *pClasses )
{
	for ( ServerClass *pCur=pClasses; pCur; pCur=pCur->m_pNext )
	{
		DataTable_ClearWriteFlags_R( pCur->m_pTable );
	}
}

void DataTable_WriteSendTablesBuffer( ServerClass *pClasses, bf_write *pBuf )
{
	ServerClass *pCur;

	DataTable_ClearWriteFlags( pClasses );

	// First, we send all the leaf classes. These are the ones that will need decoders
	// on the client.
	for ( pCur=pClasses; pCur; pCur=pCur->m_pNext )
	{
		DataTable_MaybeWriteSendTableBuffer( pCur->m_pTable, pBuf, true );
	}

	// Now, we send their base classes. These don't need decoders on the client
	// because we will never send these SendTables by themselves.
	for ( pCur=pClasses; pCur; pCur=pCur->m_pNext )
	{
		DataTable_MaybeWriteSendTableBuffer_R( pCur->m_pTable, pBuf );
	}

	// Signal no more send tables
	pBuf->WriteOneBit( 0 );
}

void DataTable_WriteClassInfosBuffer(ServerClass *pClasses, bf_write *pBuf )
{
	int count = 0;

	ServerClass *pClass = pClasses;
	
	// first count total number of classes in list
	while ( pClass != NULL )
	{
		pClass=pClass->m_pNext;
		count++;
	}

	// write number of classes
	pBuf->WriteShort( count );	

	pClass = pClasses; // go back to first class

	// write each class info
	while ( pClass != NULL )
	{
		pBuf->WriteShort( pClass->m_ClassID );
		pBuf->WriteString( pClass->m_pNetworkName );
		pBuf->WriteString( pClass->m_pTable->GetName() );
		pClass=pClass->m_pNext;
	}
}

bool DataTable_ParseClassInfosFromBuffer( CClientState *pState, bf_read *pBuf )
{
	if(pState->m_pServerClasses)
	{
		delete [] pState->m_pServerClasses;
	}

	pState->m_nServerClasses = pBuf->ReadShort();

	Assert( pState->m_nServerClasses );
	pState->m_pServerClasses = new C_ServerClassInfo[pState->m_nServerClasses];

	if ( !pState->m_pServerClasses )
	{
		Host_EndGame(true, "CL_ParseClassInfo: can't allocate %d C_ServerClassInfos.\n", pState->m_nServerClasses);
		return false;
	}
	
	for ( int i = 0; i < pState->m_nServerClasses; i++ )
	{
		int classID = pBuf->ReadShort();

		if( classID >= pState->m_nServerClasses )
		{
			Host_EndGame(true, "DataTable_ParseClassInfosFromBuffer: invalid class index (%d).\n", classID);
			return false;
		}

		pState->m_pServerClasses[classID].m_ClassName = pBuf->ReadAndAllocateString();
		pState->m_pServerClasses[classID].m_DatatableName = pBuf->ReadAndAllocateString();
	}

	return true;
}

bool DataTable_LoadDataTablesFromBuffer( bf_read *pBuf, int nDemoProtocol )
{
	// Okay, read them out of the buffer since they weren't recorded into the main network stream during recording

	// Create all of the send tables locally
	// was DataTable_ParseClientTablesFromBuffer()
	while ( pBuf->ReadOneBit() != 0 )
	{
		bool bNeedsDecoder = pBuf->ReadOneBit() != 0;

		if ( !RecvTable_RecvClassInfos( pBuf, bNeedsDecoder, nDemoProtocol ) )
		{
			Host_Error( "DataTable_ParseClientTablesFromBuffer failed.\n" );
			return false;
		}
	}


	// Now create all of the server classes locally, too
	return DataTable_ParseClassInfosFromBuffer( &cl, pBuf );
}
