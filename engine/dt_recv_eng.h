//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATATABLE_RECV_ENG_H
#define DATATABLE_RECV_ENG_H
#ifdef _WIN32
#pragma once
#endif


#include "dt_recv.h"
#include "bitbuf.h"
#include "dt.h"

class CStandardSendProxies;

// ------------------------------------------------------------------------------------------ //
// RecvTable functions.
// ------------------------------------------------------------------------------------------ //

// These are the module startup and shutdown routines.
bool		RecvTable_Init( RecvTable **pTables, int nTables );
void		RecvTable_Term( bool clearall = true );

// After calling RecvTable_Init to provide the list of RecvTables, call this
// as the server sends its SendTables over. When bNeedsDecoder is specified,
// it will precalculate the necessary data to actually decode this type of
// SendTable from the server. nDemoProtocol = 0 means current version.
bool		RecvTable_RecvClassInfos( bf_read *pBuf, bool bNeedsDecoder, int nDemoProtocol = 0);


// After ALL the SendTables have been received, call this and it will create CRecvDecoders
// for all the SendTable->RecvTable matches it finds.
// Returns false if there is an unrecoverable error.
//
// bAllowMismatches is true when playing demos back so we can change datatables without breaking demos.
// If pAnyMisMatches is non-null, it will be set to true if the client's recv tables mismatched the server's ones.
bool		RecvTable_CreateDecoders( const CStandardSendProxies *pSendProxies, bool bAllowMismatches, bool *pAnyMismatches=NULL );

// objectID gets passed into proxies and can be used to track data on particular objects.
// NOTE: this function can ONLY decode a buffer outputted from RecvTable_MergeDeltas
//       or RecvTable_CopyEncoding because if the way it follows the exclude prop bits.
bool		RecvTable_Decode( 
	RecvTable *pTable, 
	void *pStruct, 
	bf_read *pIn, 
	int objectID,
	bool updateDTI = true
	);

// This acts like a RecvTable_Decode() call where all properties are written and all their values are zero.
void RecvTable_DecodeZeros( RecvTable *pTable, void *pStruct, int objectID );

// This writes all pNewState properties into pOut.
//
// If pOldState is non-null and contains any properties that aren't in pNewState, then
// those properties are written to the output as well. Returns # of changed props
int RecvTable_MergeDeltas(
	RecvTable *pTable,

	bf_read *pOldState,		// this can be null
	bf_read *pNewState,

	bf_write *pOut,

	int objectID = - 1,
	
	int	*pChangedProps = NULL,

	bool updateDTI = false // enclude merge from newState in the DTI reports
	);


// Just copies the bits from the bf_read into the bf_write (this function is used
// when you don't know the length of the encoded data).
void RecvTable_CopyEncoding( 
	RecvTable *pRecvTable, 
	bf_read *pIn, 
	bf_write *pOut,
	int objectID
	);



// ------------------------------------------------------------------------------------------ //
// Globals
// ------------------------------------------------------------------------------------------ //

// This is incremented each time a property is decoded.
extern int g_nPropsDecoded;


#endif // DATATABLE_RECV_ENG_H
