//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ENTITY_BURN_EFFECT_H
#define ENTITY_BURN_EFFECT_H
#ifdef _WIN32
#pragma once
#endif


#include "baseentity.h"
#include "server_class.h"


class CEntityBurnEffect : public CBaseEntity
{
public:
	
	DECLARE_CLASS( CEntityBurnEffect, CBaseEntity );
	DECLARE_SERVERCLASS();

	static CEntityBurnEffect* Create( CBaseEntity *pBurningEntity );


// Overrides.
public:

	virtual	int		UpdateTransmitState();
	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo	);


private:
	CNetworkHandle( CBaseEntity, m_hBurningEntity );
};


#endif // ENTITY_BURN_EFFECT_H
