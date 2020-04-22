//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "tier1/strtools.h"
#include "materialsystem/IMaterialProxy.h"
#include "materialsystem/IMaterialProxyFactory.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterial.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


// ------------------------------------------------------------------------------------- //
// The material proxy factory for WC.
// ------------------------------------------------------------------------------------- //

class CMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	IMaterialProxy *CreateProxy( const char *proxyName );
	void DeleteProxy( IMaterialProxy *pProxy );
};
CMaterialProxyFactory g_MaterialProxyFactory;


IMaterialProxy *CMaterialProxyFactory::CreateProxy( const char *proxyName )
{
	// assumes that the client.dll is already LoadLibraried
	CreateInterfaceFn clientFactory = Sys_GetFactoryThis();

	// allocate exactly enough memory for the versioned name on the stack.
	char proxyVersionedName[512];
	Q_snprintf( proxyVersionedName, sizeof( proxyVersionedName ), "%s%s", proxyName, IMATERIAL_PROXY_INTERFACE_VERSION );
	return ( IMaterialProxy * )clientFactory( proxyVersionedName, NULL );
}

void CMaterialProxyFactory::DeleteProxy( IMaterialProxy *pProxy )
{
	if ( pProxy )
	{
		pProxy->Release();
	}
}


IMaterialProxyFactory* GetHammerMaterialProxyFactory()
{
	return &g_MaterialProxyFactory;
}



// ------------------------------------------------------------------------------------- //
// Specific material proxies.
// ------------------------------------------------------------------------------------- //

class CWorldDimsProxy : public IMaterialProxy
{
public:
	CWorldDimsProxy();
	virtual ~CWorldDimsProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

public:
	IMaterialVar *m_pMinsVar;
	IMaterialVar *m_pMaxsVar;
};


CWorldDimsProxy::CWorldDimsProxy()
{
	m_pMinsVar = m_pMaxsVar = NULL;
}

CWorldDimsProxy::~CWorldDimsProxy()
{
}

bool CWorldDimsProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	m_pMinsVar = pMaterial->FindVar( "$world_mins", NULL, false );
	m_pMaxsVar = pMaterial->FindVar( "$world_maxs", NULL, false );
	return true;
}

void CWorldDimsProxy::OnBind( void *pC_BaseEntity )
{
	if ( m_pMinsVar && m_pMaxsVar )
	{
		float mins[3] = {-500,-500,-500};
		float maxs[3] = {+500,+500,+500};
		m_pMinsVar->SetVecValue( (const float*)mins, 3 );
		m_pMaxsVar->SetVecValue( (const float*)maxs, 3 );
	}
}

IMaterial *CWorldDimsProxy::GetMaterial()
{
	if ( m_pMinsVar && m_pMaxsVar )
		return m_pMinsVar->GetOwningMaterial();
	return NULL;
}

EXPOSE_INTERFACE( CWorldDimsProxy, IMaterialProxy, "WorldDims" IMATERIAL_PROXY_INTERFACE_VERSION );

