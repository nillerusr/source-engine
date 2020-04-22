//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include <KeyValues.h>
#include "materialsystem/imaterialvar.h"
#include "C_BaseTFPlayer.h"
#include "functionproxy.h"
#include "C_PlayerResource.h"
#include "Weapon_CombatShield.h"

// for the 2/5/03 demo
#include "c_demo_entities.h"
#include "c_basefourwheelvehicle.h"

//=============================================================================
//
// TF Shield raising proxy. 
//
class CTFSunroofProxy : public CResultProxy
{
public:

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pEnt );

private:

	CFloatInput	m_Factor;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool CTFSunroofProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	// Init shield raise times.
	//if ( !m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ) )
	//	return false;

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTFSunroofProxy::OnBind( void *pArg )
{
	C_BaseEntity *pEntity = BindArgToEntity( pArg );
	C_BaseTFFourWheelVehicle* pVehicle = dynamic_cast<C_BaseTFFourWheelVehicle*>(pEntity);
	if( !pVehicle )
		return;

	if( pVehicle->GetPassengerCount() )
	{
		SetFloatResult(0.f);
	} 
	else
	{
		SetFloatResult(1.f);
	}
}

EXPOSE_INTERFACE( CTFSunroofProxy, IMaterialProxy, "TFSunroof" IMATERIAL_PROXY_INTERFACE_VERSION );
