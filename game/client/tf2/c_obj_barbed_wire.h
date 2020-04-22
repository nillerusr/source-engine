//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_OBJ_BARBED_WIRE_H
#define C_OBJ_BARBED_WIRE_H
#ifdef _WIN32
#pragma once
#endif


#include "c_baseobject.h"
#include "c_rope.h"


class C_ObjectBarbedWire : public C_BaseObject
{
public:
	DECLARE_CLASS( C_ObjectBarbedWire, C_BaseObject );
	DECLARE_CLIENTCLASS();

	C_ObjectBarbedWire();
	~C_ObjectBarbedWire();

	virtual void OnDataChanged( DataUpdateType_t type );

	virtual void Spawn();
	virtual void ClientThink();


private:
	C_ObjectBarbedWire( C_ObjectBarbedWire& ) {}

	CHandle<C_ObjectBarbedWire> m_hConnectedTo;
	CHandle<C_ObjectBarbedWire> m_hLastConnectedTo;
	CHandle<C_RopeKeyframe> m_hRope;

	// Used to determine when to recalculate the hang distance.
	Vector m_vLastOrigin;
	Vector m_vLastConnectedOrigin;
};


#endif // C_OBJ_BARBED_WIRE_H
