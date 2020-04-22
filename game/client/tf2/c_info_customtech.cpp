//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud_technologytreedoc.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_InfoCustomTechnology : public C_BaseEntity
{
	DECLARE_CLASS( C_InfoCustomTechnology, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	C_InfoCustomTechnology( void );
	virtual void SetDormant( bool bDormant );

public:
	// Sent via datatable
	char		m_szTechTreeFile[128];
};

IMPLEMENT_CLIENTCLASS_DT(C_InfoCustomTechnology, DT_InfoCustomTechnology, CInfoCustomTechnology)
	RecvPropString(RECVINFO(m_szTechTreeFile)),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_InfoCustomTechnology::C_InfoCustomTechnology( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Whenever we enter the PVS, add ourselves to the tech tree. This will
//			only happen when the player joins a new team.
//-----------------------------------------------------------------------------
void C_InfoCustomTechnology::SetDormant( bool bDormant )
{
	if ( IsDormant() && !bDormant )
	{
		// Tell the techtree to add the file to it's list of technologies
		GetTechnologyTreeDoc().AddTechnologyFile( m_szTechTreeFile );
	}

	BaseClass::SetDormant( bDormant );
}