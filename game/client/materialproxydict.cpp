//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "imaterialproxydict.h"
#include "materialsystem/imaterialproxy.h"
#include "tier1/utlstringmap.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CMaterialProxyDict : public IMaterialProxyDict
{
public:
	IMaterialProxy *	CreateProxy( const char *proxyName );
	void				Add( const char *pMaterialProxyName, MaterialProxyFactory_t *pMaterialProxyFactory );
private:
	CUtlStringMap<MaterialProxyFactory_t *> m_StringToProxyFactoryMap;
};

void CMaterialProxyDict::Add( const char *pMaterialProxyName, MaterialProxyFactory_t *pMaterialProxyFactory )
{
	Assert( pMaterialProxyName );
	m_StringToProxyFactoryMap[pMaterialProxyName] = pMaterialProxyFactory;
}

IMaterialProxyDict &GetMaterialProxyDict()
{
	static CMaterialProxyDict g_MaterialProxyDict;
	return g_MaterialProxyDict;
}

IMaterialProxy *CMaterialProxyDict::CreateProxy( const char *pMaterialProxyName )
{
	UtlSymId_t sym = m_StringToProxyFactoryMap.Find( pMaterialProxyName );
	if ( sym == m_StringToProxyFactoryMap.InvalidIndex() )
	{
		return NULL;
	}
	MaterialProxyFactory_t *pMaterialProxyFactory = m_StringToProxyFactoryMap[sym];
	Assert( pMaterialProxyFactory );
	return (*pMaterialProxyFactory)();
}
