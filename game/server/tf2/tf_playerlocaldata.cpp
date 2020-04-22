//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_playerlocaldata.h"
#include "tf_player.h"
#include "mathlib/mathlib.h"
#include "entitylist.h"

extern ConVar tf_fastbuild;
ConVar tf_maxbankresources( "tf_maxbankresources", "2000", 0, "Max resources a single player can have." );

#define BANK_RESOURCE_BITS	20
#define MAX_PLAYER_RESOURCES ( (1 <<BANK_RESOURCE_BITS) - 1 ) 

//-----------------------------------------------------------------------------
// Purpose: SendProxy that converts the UtlVector list of objects to entindexes, where it's reassembled on the client
//-----------------------------------------------------------------------------
void SendProxy_PlayerObjectList( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CTFPlayerLocalData *pLocalData = (CTFPlayerLocalData*)pStruct;
	Assert( pLocalData->m_pPlayer );

	// If this fails, then SendProxyArrayLength_PlayerObjects didn't work.
	Assert( iElement < pLocalData->m_pPlayer->GetObjectCount() );

	CBaseObject *pObject = pLocalData->m_pPlayer->GetObject(iElement);

	EHANDLE hObject;
	hObject = pObject;

	SendProxy_EHandleToInt( pProp, pStruct, &hObject, pOut, iElement, objectID );
}


int SendProxyArrayLength_PlayerObjects( const void *pStruct, int objectID )
{
	CTFPlayerLocalData *pLocalData = (CTFPlayerLocalData*)pStruct;
	Assert( pLocalData->m_pPlayer );
	int iObjects = pLocalData->m_pPlayer->GetObjectCount();
	Assert( iObjects < MAX_OBJECTS_PER_PLAYER );
	return iObjects;
}

BEGIN_SEND_TABLE_NOBASE( CTFPlayerLocalData, DT_TFLocal )
	SendPropInt( SENDINFO(m_nInTacticalView), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bKnockedDown ), 1, SPROP_UNSIGNED ),
	SendPropVector( SENDINFO( m_vecKnockDownDir ), -1, SPROP_COORD ),
	SendPropInt( SENDINFO( m_bThermalVision ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iIDEntIndex ), 10, SPROP_UNSIGNED ),
	SendPropArray(	SendPropInt( SENDINFO_ARRAY(m_iResourceAmmo), 4, SPROP_UNSIGNED ), m_iResourceAmmo ),
	SendPropInt( SENDINFO(m_iBankResources), BANK_RESOURCE_BITS, SPROP_UNSIGNED ),
	SendPropArray2( 
		SendProxyArrayLength_PlayerObjects,
		SendPropInt("player_object_array_element", 0, SIZEOF_IGNORE, NUM_NETWORKED_EHANDLE_BITS, SPROP_UNSIGNED, SendProxy_PlayerObjectList), 
		MAX_OBJECTS_PER_PLAYER, 
		0, 
		"player_object_array"
		 ),
	SendPropInt( SENDINFO( m_bAttachingSapper ), 1, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flSapperAttachmentFrac ), 7, SPROP_ROUNDDOWN, 0.0f, 1.0f ),
	SendPropInt( SENDINFO( m_bForceMapOverview ), 1, SPROP_UNSIGNED ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFPlayerLocalData::CTFPlayerLocalData()
{
	m_nInTacticalView = 0;
	m_pPlayer = NULL;

	m_bKnockedDown = false;
	m_vecKnockDownDir.Init();

	m_bThermalVision = false;
	m_iIDEntIndex = 0;

	// Resource carrying
	for ( int i = 0; i < RESOURCE_TYPES; i++ )
	{
		m_iResourceAmmo.Set( i, 0 );
	}

	if( inv_demo.GetInt() )
	{
		m_iBankResources = 5000;
	}
	else
	{
		m_iBankResources = 0;
	}
	m_aObjects.Purge();

	m_bAttachingSapper = false;
	m_flSapperAttachmentFrac = 0;
	m_bForceMapOverview = false;
}

//-----------------------------------------------------------------------------
// Add, remove, count resources
//-----------------------------------------------------------------------------
void CTFPlayerLocalData::AddResources( int iAmount )
{
	m_iBankResources += iAmount;

	if ( !tf_fastbuild.GetBool() && (m_iBankResources > tf_maxbankresources.GetFloat()) )
	{
		m_iBankResources = tf_maxbankresources.GetFloat();
	}

	// Clamp for overflow...
	if ( m_iBankResources > MAX_PLAYER_RESOURCES )
	{
		Msg("Warning: Player overflowed resource count!\n");
		m_iBankResources = MAX_PLAYER_RESOURCES;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerLocalData::RemoveResources( int iAmount )
{
	Assert( iAmount <= m_iBankResources );
	m_iBankResources = MAX(0, m_iBankResources - iAmount);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFPlayerLocalData::ResourceCount( void ) const
{
	return m_iBankResources;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerLocalData::SetResources( int iAmount )
{
	m_iBankResources = iAmount;
}