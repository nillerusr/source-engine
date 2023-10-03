//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: implements various common send proxies
//
// $NoKeywords: $
//=============================================================================//

#ifndef RECVPROXY_H
#define RECVPROXY_H


#include "dt_recv.h"

class CRecvProxyData;


// This converts the int stored in pData to an EHANDLE in pOut.
void RecvProxy_IntToEHandle( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_IntToMoveParent( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_IntToColor32( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_IntSubOne( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_ShortSubOne( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_InterpolationAmountChanged( const CRecvProxyData *pData, void *pStruct, void *pOut );

RecvProp RecvPropTime(
	char *pVarName, 
	int offset, 
	int sizeofVar=SIZEOF_IGNORE );

#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
RecvProp RecvPropPredictableId(
	char *pVarName, 
	int offset, 
	int sizeofVar=SIZEOF_IGNORE );
#endif

RecvProp RecvPropEHandle(
	char *pVarName, 
	int offset, 
	int sizeofVar=SIZEOF_IGNORE,
	RecvVarProxyFn proxyFn=RecvProxy_IntToEHandle );

RecvProp RecvPropBool(
	char *pVarName, 
	int offset, 
	int sizeofVar );

RecvProp RecvPropIntWithMinusOneFlag(
	char *pVarName, 
	int offset, 
	int sizeofVar=SIZEOF_IGNORE,
	RecvVarProxyFn proxyFn=RecvProxy_IntSubOne );

#endif // RECVPROXY_H

