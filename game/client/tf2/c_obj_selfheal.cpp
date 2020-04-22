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
class C_ObjectSelfHeal : public C_BaseObjectUpgrade
{
	DECLARE_CLASS( C_ObjectSelfHeal, C_BaseObjectUpgrade );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectSelfHeal();

private:
	C_ObjectSelfHeal( const C_ObjectSelfHeal & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectSelfHeal, DT_ObjectSelfHeal, CObjectSelfHeal)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectSelfHeal::C_ObjectSelfHeal()
{
}

