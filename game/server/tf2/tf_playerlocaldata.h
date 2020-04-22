//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_PLAYERLOCALDATA_H
#define TF_PLAYERLOCALDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "techtree.h"
#include "predictable_entity.h"
#include "tf_obj.h"

//-----------------------------------------------------------------------------
// Purpose: Player specific data for TF2 ( sent only to local player, too )
//-----------------------------------------------------------------------------
class CTFPlayerLocalData
{
public:
	DECLARE_PREDICTABLE();
	DECLARE_CLASS_NOBASE( CTFPlayerLocalData );
	DECLARE_EMBEDDED_NETWORKVAR();

	CTFPlayerLocalData();
	
	CBaseTFPlayer	*m_pPlayer;

	CNetworkVar( bool, m_nInTacticalView );

	// Player has been knocked down
	CNetworkVar( bool, m_bKnockedDown );
	CNetworkQAngle( m_vecKnockDownDir );

	// Player is using thermal vision
	CNetworkVar( bool, m_bThermalVision );

	// ID
	CNetworkVar( int, m_iIDEntIndex );

	// Resource chunk carrying counts
	CNetworkArray( int, m_iResourceAmmo, RESOURCE_TYPES );	// 0 = Normal resources, 1 = Processed resources

	// Resource manipulation
	void		AddResources( int iAmount );
	void		RemoveResources( int iAmount );
	int			ResourceCount( void ) const;
	void		SetResources( int iAmount );

	// Resource bank
	CNetworkVar( int, m_iBankResources );					// Current amounts of resources in my bank

	// Objects
	CUtlVector< CHandle<CBaseObject> > m_aObjects;

	// Object sapper placement handling
	CNetworkVar( bool,	m_bAttachingSapper );
	CNetworkVar( float,	m_flSapperAttachmentFrac );

	CNetworkVar( bool, m_bForceMapOverview );
};

EXTERN_SEND_TABLE(DT_TFLocal);

#endif // TF_PLAYERLOCALDATA_H
