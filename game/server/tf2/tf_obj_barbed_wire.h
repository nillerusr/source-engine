//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TF_OBJ_BARBED_WIRE_H
#define TF_OBJ_BARBED_WIRE_H
#ifdef _WIN32
#pragma once
#endif


#include "tf_obj.h"


class CObjectBarbedWire : public CBaseObject
{
public:
	DECLARE_CLASS( CObjectBarbedWire, CBaseObject );
	DECLARE_SERVERCLASS();


	CObjectBarbedWire();

	
	void BarbedWireThink();

	virtual void Precache();
	virtual void Spawn();

	virtual void StartPlacement( CBaseTFPlayer *pPlayer );

	virtual bool PreStartBuilding();
	virtual void FinishedBuilding();

private:
	CNetworkHandle( CObjectBarbedWire, m_hConnectedTo );
		
};


#endif // TF_OBJ_BARBED_WIRE_H
