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
class C_ObjectMannedShield : public C_ObjectBaseMannedGun
{
	DECLARE_CLASS( C_ObjectMannedShield, C_ObjectBaseMannedGun );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectMannedShield() {}

private:
	C_ObjectMannedShield( const C_ObjectMannedShield & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT( C_ObjectMannedShield, DT_ObjectMannedShield, CObjectMannedShield )
END_RECV_TABLE()
