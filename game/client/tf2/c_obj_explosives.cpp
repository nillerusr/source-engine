//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "commanderoverlay.h"
#include "tf_obj_baseupgrade_shared.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectExplosives : public C_BaseObjectUpgrade
{
	DECLARE_CLASS( C_ObjectExplosives, C_BaseObjectUpgrade );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectExplosives();

private:
	C_ObjectExplosives( const C_ObjectExplosives & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectExplosives, DT_ObjectExplosives, CObjectExplosives)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectExplosives::C_ObjectExplosives()
{
}

