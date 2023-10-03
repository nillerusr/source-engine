//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: implements various common send proxies
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "recvproxy.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void RecvProxy_IntSubOne( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	int *pInt = (int *)pOut;
	
	*pInt = pData->m_Value.m_Int - 1;
}

void RecvProxy_ShortSubOne( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	short *pInt = (short *)pOut;
	
	*pInt = pData->m_Value.m_Int - 1;
}

RecvProp RecvPropIntWithMinusOneFlag( char *pVarName, int offset, int sizeofVar, RecvVarProxyFn proxyFn )
{
	return RecvPropInt( pVarName, offset, sizeofVar, 0, proxyFn );
}

//-----------------------------------------------------------------------------
// Purpose: Okay, so we have to queue up the actual ehandle to entity lookup for the following reason:
//  If a player has an EHandle/CHandle to an object such as a weapon, since the player is in slot 1-31, then
//  if the weapon is created and given to the player in the same frame, then the weapon won't have been
//  created at the time we parse this EHandle index, since the player is ahead of every other entity in the
//  packet (except for the world).
// So instead, we remember which ehandles need to be set and we set them after all network data has
//  been received.  Sigh.
// Input  : *pData - 
//			*pStruct - 
//			*pOut - 
//-----------------------------------------------------------------------------

void RecvProxy_IntToEHandle( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseHandle *pEHandle = (CBaseHandle*)pOut;
	
	if ( pData->m_Value.m_Int == INVALID_NETWORKED_EHANDLE_VALUE )
	{
		*pEHandle = INVALID_EHANDLE_INDEX;
	}
	else
	{
		int iEntity = pData->m_Value.m_Int & ((1 << MAX_EDICT_BITS) - 1);
		int iSerialNum = pData->m_Value.m_Int >> MAX_EDICT_BITS;
		
		pEHandle->Init( iEntity, iSerialNum );
	}
}

RecvProp RecvPropEHandle(
	char *pVarName, 
	int offset, 
	int sizeofVar,
	RecvVarProxyFn proxyFn )
{
	return RecvPropInt( pVarName, offset, sizeofVar, 0, proxyFn );
}


RecvProp RecvPropBool(
	char *pVarName, 
	int offset, 
	int sizeofVar )
{
	Assert( sizeofVar == sizeof( bool ) );
	return RecvPropInt( pVarName, offset, sizeofVar );
}


//-----------------------------------------------------------------------------
// Moveparent receive proxies
//-----------------------------------------------------------------------------
void RecvProxy_IntToMoveParent( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CHandle<C_BaseEntity> *pHandle = (CHandle<C_BaseEntity>*)pOut;
	RecvProxy_IntToEHandle( pData, pStruct, (CBaseHandle*)pHandle );
}


void RecvProxy_InterpolationAmountChanged( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// m_bSimulatedEveryTick & m_bAnimatedEveryTick are boolean
	if ( *((bool*)pOut) != (pData->m_Value.m_Int != 0) )
	{
		// Have the regular proxy store the data.
		RecvProxy_Int32ToInt8( pData, pStruct, pOut );

		C_BaseEntity *pEntity = (C_BaseEntity *) pStruct;
		pEntity->Interp_UpdateInterpolationAmounts( pEntity->GetVarMapping() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Decodes a time value
// Input  : *pStruct - ( C_BaseEntity * ) used to flag animtime is changine
//			*pVarData - 
//			*pIn - 
//			objectID - 
//-----------------------------------------------------------------------------
static void RecvProxy_Time( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	float	t;
	float	clock_base;
	float	offset;

	// Get msec offset
	offset	= ( float )pData->m_Value.m_Int / 1000.0f;

	// Get base
	clock_base = floor( engine->GetLastTimeStamp() );

	// Add together and clamp to msec precision
	t = ClampToMsec( clock_base + offset );

	// Store decoded value
	*( float * )pOut = t;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVarName - 
//			sizeofVar - 
// Output : RecvProp
//-----------------------------------------------------------------------------
RecvProp RecvPropTime(
	char *pVarName, 
	int offset, 
	int sizeofVar/*=SIZEOF_IGNORE*/ )
{
//	return RecvPropInt( pVarName, offset, sizeofVar, 0, RecvProxy_Time );
	return RecvPropFloat( pVarName, offset, sizeofVar );
};

//-----------------------------------------------------------------------------
// Purpose: Okay, so we have to queue up the actual ehandle to entity lookup for the following reason:
//  If a player has an EHandle/CHandle to an object such as a weapon, since the player is in slot 1-31, then
//  if the weapon is created and given to the player in the same frame, then the weapon won't have been
//  created at the time we parse this EHandle index, since the player is ahead of every other entity in the
//  packet (except for the world).
// So instead, we remember which ehandles need to be set and we set them after all network data has
//  been received.  Sigh.
// Input  : *pData - 
//			*pStruct - 
//			*pOut - 
//-----------------------------------------------------------------------------
#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
static void RecvProxy_IntToPredictableId( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CPredictableId *pId = (CPredictableId*)pOut;
	Assert( pId );
	pId->SetRaw( pData->m_Value.m_Int );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVarName - 
//			sizeofVar - 
// Output : RecvProp
//-----------------------------------------------------------------------------
RecvProp RecvPropPredictableId(
	char *pVarName, 
	int offset, 
	int sizeofVar/*=SIZEOF_IGNORE*/ )
{
	return RecvPropInt( pVarName, offset, sizeofVar, 0, RecvProxy_IntToPredictableId );
}
#endif
