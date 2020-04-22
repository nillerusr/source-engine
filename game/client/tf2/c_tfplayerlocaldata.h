//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the player specific data that is sent only to the player
//			to whom it belongs.
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TFPLAYERLOCALDATA_H
#define C_TFPLAYERLOCALDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "techtree.h"
#include "c_baseobject.h"

//-----------------------------------------------------------------------------
// Purpose: Player specific data ( sent only to local player, too )
//-----------------------------------------------------------------------------
class CTFPlayerLocalData
{
public:
	DECLARE_PREDICTABLE();

	int			m_nInTacticalView;

	bool		m_bKnockedDown;
	QAngle		m_vecKnockDownDir;

	bool		m_bThermalVision;

	int			m_iIDEntIndex;

	// Resource chunk carrying counts
	int			m_iResourceAmmo[ RESOURCE_TYPES ];	// 0 = Normal resources, 1 = Processed resources

	// Resource bank
	int			m_iBankResources;		// Current amounts of resource in my bank

	// Objects
	CUtlVector< CHandle<C_BaseObject> > m_aObjects;

	// Object sapper placement handling
	bool		m_bAttachingSapper;
	float		m_flSapperAttachmentFrac;
	bool		m_bForceMapOverview;
};

#endif // C_TFPLAYERLOCALDATA_H
