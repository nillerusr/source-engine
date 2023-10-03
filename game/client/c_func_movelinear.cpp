//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_basedoor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ------------------------------------------------------------------------
class C_FuncMoveLinear: public C_BaseToggle
{
public:
	DECLARE_CLASS( C_FuncMoveLinear, C_BaseToggle );
	DECLARE_CLIENTCLASS();

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_fFlags );

	C_FuncMoveLinear();
	virtual void OnDataChanged( DataUpdateType_t type );
};


IMPLEMENT_CLIENTCLASS_DT( C_FuncMoveLinear, DT_FuncMoveLinear, CFuncMoveLinear )
	RecvPropVector( RECVINFO(m_vecVelocity), 0, RecvProxy_LocalVelocity ),
	RecvPropInt( RECVINFO( m_fFlags ) ),
END_RECV_TABLE()


C_FuncMoveLinear::C_FuncMoveLinear()
{
}

void C_FuncMoveLinear::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		SetSolid(SOLID_VPHYSICS);
		VPhysicsInitShadow( false, false );
	}
}
