//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A place where vehicles can be built
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

//-----------------------------------------------------------------------------
// Purpose: A place where vehicles can be built
//-----------------------------------------------------------------------------
class C_FuncConstructionYard : public C_BaseEntity
{
	DECLARE_CLASS( C_FuncConstructionYard, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();
//	DECLARE_MINIMAP_PANEL();

	C_FuncConstructionYard();
	const char	*GetTargetDescription( void ) const;
};


IMPLEMENT_CLIENTCLASS_DT(C_FuncConstructionYard, DT_FuncConstructionYard, CFuncConstructionYard)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_FuncConstructionYard::C_FuncConstructionYard()
{
//	CONSTRUCT_MINIMAP_PANEL( "minimap_construction_yard", MINIMAP_RESOURCE_ZONES );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_FuncConstructionYard::GetTargetDescription( void ) const
{
	return "Construction Yard";
}

