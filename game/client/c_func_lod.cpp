//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "cbase.h"
#include "view.h"
#include "iviewrender.h"
#include "clientalphaproperty.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_Func_LOD : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_Func_LOD, C_BaseEntity );
	DECLARE_CLIENTCLASS();

					C_Func_LOD();

// C_BaseEntity overrides.
public:

	virtual void	OnDataChanged( DataUpdateType_t type );

public:
// Replicated vars from the server.
// These are documented in the server-side entity.
public:
	int m_nDisappearMinDist;
	int m_nDisappearMaxDist;
};


// ------------------------------------------------------------------------- //
// Tables.
// ------------------------------------------------------------------------- //

// Datatable..
IMPLEMENT_CLIENTCLASS_DT(C_Func_LOD, DT_Func_LOD, CFunc_LOD)
	RecvPropInt(RECVINFO(m_nDisappearMinDist)),
	RecvPropInt(RECVINFO(m_nDisappearMaxDist)),
END_RECV_TABLE()



// ------------------------------------------------------------------------- //
// C_Func_LOD implementation.
// ------------------------------------------------------------------------- //

C_Func_LOD::C_Func_LOD()
{
	m_nDisappearMinDist = 5000;
	m_nDisappearMaxDist = 5800;
}


void C_Func_LOD::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	bool bCreate = (type == DATA_UPDATE_CREATED) ? true : false;
	VPhysicsShadowDataChanged(bCreate, this);

	// Copy in fade parameters
	AlphaProp()->SetFade( 1.0f, m_nDisappearMinDist, m_nDisappearMaxDist );
}
