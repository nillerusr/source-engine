//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_obj_base_manned_gun.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectMannedMissileLauncher : public C_ObjectBaseMannedGun
{
	DECLARE_CLASS( C_ObjectMannedMissileLauncher, C_ObjectBaseMannedGun );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectMannedMissileLauncher() {}

private:
	C_ObjectMannedMissileLauncher( const C_ObjectMannedMissileLauncher & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectMannedMissileLauncher, DT_ObjectMannedMissileLauncher, CObjectMannedMissileLauncher)
END_RECV_TABLE()

