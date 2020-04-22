//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DT_LOCALTRANSFER_H
#define DT_LOCALTRANSFER_H
#ifdef _WIN32
#pragma once
#endif


#include "dt_send.h"
#include "dt_recv.h"


class CBaseEdict;


// This sets up the ability to copy an entity with the specified SendTable directly
// into an entity with the specified RecvTable, thus avoiding compression overhead.
void LocalTransfer_InitFastCopy( 
	const SendTable *pSendTable, 
	const CStandardSendProxies *pSendProxies,
	RecvTable *pRecvTable,
	const CStandardRecvProxies *pRecvProxies,
	int &nSlowCopyProps,		// These are incremented to tell you how many fast copy props it found.
	int &nFastCopyProps
	);

// Transfer the data from pSrcEnt to pDestEnt using the specified SendTable and RecvTable.
void LocalTransfer_TransferEntity( 
	const CBaseEdict *pEdict, 
	const SendTable *pSendTable, 
	const void *pSrcEnt, 
	RecvTable *pRecvTable, 
	void *pDestEnt,
	bool bNewlyCreated,
	bool bJustEnteredPVS,
	int objectID );

// Call this after packing all the entities in a frame.
void PrintPartialChangeEntsList();


#endif // DT_LOCALTRANSFER_H
