//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Proxy to hook into an object's powered state
//
//=============================================================================//

#include "cbase.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include <KeyValues.h>
#include "materialsystem/imaterialvar.h"
#include "C_BaseTFPlayer.h"
#include "functionproxy.h"
#include "c_baseobject.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CTFObjectPowerProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pEnt );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFObjectPowerProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFObjectPowerProxy::OnBind( void *pArg )
{
	C_BaseEntity *pEntity = BindArgToEntity( pArg );
	if (!pEntity)
		return;

	C_BaseObject *pObject = dynamic_cast<C_BaseObject*>( pEntity );
	Assert( pObject );

	float flPoweredState = (float)(!pObject->IsBuilding() && !pObject->IsPlacing() && pObject->IsPowered());

	Assert( m_pResult );
	SetFloatResult( flPoweredState );
}


EXPOSE_INTERFACE( CTFObjectPowerProxy, IMaterialProxy, "TFObjectPower" IMATERIAL_PROXY_INTERFACE_VERSION );
