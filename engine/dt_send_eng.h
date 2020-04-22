//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATATABLE_SEND_ENG_H
#define DATATABLE_SEND_ENG_H
#ifdef _WIN32
#pragma once
#endif


#include "dt_send.h"
#include "bitbuf.h"
#include "utlmemory.h"

typedef unsigned int CRC32_t;

class CStandardSendProxies;


#define MAX_DELTABITS_SIZE 2048

// ------------------------------------------------------------------------ //
// SendTable functions.
// ------------------------------------------------------------------------ //

// Precalculate data that enables the SendTable to be used to encode data.
bool		SendTable_Init( SendTable **pTables, int nTables );
void		SendTable_Term();
CRC32_t		SendTable_GetCRC();
int			SendTable_GetNum();
SendTable	*SendTabe_GetTable(int index);


// Return the number of unique properties in the table.
int	SendTable_GetNumFlatProps( SendTable *pTable );

// compares properties and writes delta properties
int SendTable_WriteAllDeltaProps(
	const SendTable *pTable,					
	const void *pFromData,
	const int	nFromDataBits,
	const void *pToData,
	const int nToDataBits,
	const int nObjectID,
	bf_write *pBufOut );

// Write the properties listed in pCheckProps and nCheckProps.
void SendTable_WritePropList(
	const SendTable *pTable,
	const void *pState,
	const int nBits,
	bf_write *pOut,
	const int objectID,
	const int *pCheckProps,
	const int nCheckProps
	);

//
// Writes the property indices that must be written to move from pFromState to pToState into pDeltaProps.
// Returns the number of indices written to pDeltaProps.
//
int	SendTable_CalcDelta(
	const SendTable *pTable,
	
	const void *pFromState,
	const int nFromBits,

	const void *pToState,
	const int nToBits,

	int *pDeltaProps,
	int nMaxDeltaProps,

	const int objectID );


// This function takes the list of property indices in startProps and the values from
// SendProxies in pProxyResults, and fills in a new array in outProps with the properties
// that the proxies want to allow for iClient's client.
//
// If pOldStateProxies is non-null, this function adds new properties into the output list
// if a proxy has turned on from the previous state.
int SendTable_CullPropsFromProxies( 
	const SendTable *pTable,
	
	const int *pStartProps,
	int nStartProps,

	const int iClient,
	
	const CSendProxyRecipients *pOldStateProxies,
	const int nOldStateProxies, 
	
	const CSendProxyRecipients *pNewStateProxies,
	const int nNewStateProxies,
	
	int *pOutProps,
	int nMaxOutProps
	);


// Encode the properties that are referenced in the delta bits.
// If pDeltaBits is NULL, then all the properties are encoded.
bool SendTable_Encode(
	const SendTable *pTable,
	const void *pStruct, 
	bf_write *pOut, 
	int objectID = -1,
	CUtlMemory<CSendProxyRecipients> *pRecipients = NULL,	// If non-null, this is an array of CSendProxyRecipients.
															// The array must have room for pTable->GetNumDataTableProxies().
	bool bNonZeroOnly = false								// If this is true, then it will write all properties that have
															// nonzero values.
	);


// In order to receive a table, you must send it from the server and receive its info
// on the client so the client knows how to unpack it.
bool SendTable_WriteInfos( SendTable *pTable, bf_write *pBuf );

// do all kinds of checks on a packed entity bitbuffer
bool SendTable_CheckIntegrity( SendTable *pTable, const void *pData, const int nDataBits );


#endif // DATATABLE_SEND_ENG_H
