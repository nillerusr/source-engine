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
class C_ObjectVehicleBoost : public C_BaseObjectUpgrade
{

	DECLARE_CLASS( C_ObjectVehicleBoost, C_BaseObjectUpgrade );

public:

	DECLARE_CLIENTCLASS();

	C_ObjectVehicleBoost();

private:

	C_ObjectVehicleBoost( const C_ObjectVehicleBoost& ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT( C_ObjectVehicleBoost, DT_ObjectVehicleBoost, CObjectVehicleBoost )
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectVehicleBoost::C_ObjectVehicleBoost()
{
}

