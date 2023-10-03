//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "materialsystem/IMaterialProxy.h"
#include "materialsystem/IMaterial.h"

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CDummyMaterialProxy : public IMaterialProxy
{
public:
	CDummyMaterialProxy();
	virtual ~CDummyMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial() { return NULL; }
};

CDummyMaterialProxy::CDummyMaterialProxy()
{
	DevMsg( 1, "CDummyMaterialProxy::CDummyMaterialProxy()\n" );
}

CDummyMaterialProxy::~CDummyMaterialProxy()
{
	DevMsg( 1, "CDummyMaterialProxy::~CDummyMaterialProxy()\n" );
}


bool CDummyMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	DevMsg( 1, "CDummyMaterialProxy::Init( material = \"%s\" )\n", pMaterial->GetName() );
	return true;
}

void CDummyMaterialProxy::OnBind( void *pC_BaseEntity )
{
	DevMsg( 1, "CDummyMaterialProxy::OnBind( %p )\n", pC_BaseEntity );
}

EXPOSE_MATERIAL_PROXY( CDummyMaterialProxy, Dummy );
