//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_obj_base_manned_gun.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectMannedPlasmagun : public C_ObjectBaseMannedGun
{
	DECLARE_CLASS( C_ObjectMannedPlasmagun, C_ObjectBaseMannedGun );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectMannedPlasmagun() {}

private:
	C_ObjectMannedPlasmagun( const C_ObjectMannedPlasmagun & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectMannedPlasmagun, DT_ObjectMannedPlasmagun, CObjectMannedPlasmagun)
END_RECV_TABLE()

