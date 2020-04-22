//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"

// $sineVar : name of variable that controls the alpha level (float)
class CzTestMaterialProxy : public IMaterialProxy
{
public:
	CzTestMaterialProxy();
	virtual ~CzTestMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
private:
	C_BaseEntity *BindArgToEntity( void *pArg );

	IMaterialVar *m_AlphaVar;
};

CzTestMaterialProxy::CzTestMaterialProxy()
{
	m_AlphaVar = NULL;
}

CzTestMaterialProxy::~CzTestMaterialProxy()
{
}

C_BaseEntity *CzTestMaterialProxy::BindArgToEntity( void *pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	return pRend->GetIClientUnknown()->GetBaseEntity();
}

bool CzTestMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_AlphaVar = pMaterial->FindVar( "$alpha", &foundVar, false );
	return foundVar;
}

void CzTestMaterialProxy::OnBind( void *pC_BaseEntity )
{
	C_BaseEntity *pEnt = BindArgToEntity( pC_BaseEntity );
	if( !pEnt )
	{
		return;
	}

	if (m_AlphaVar)
	{
		m_AlphaVar->SetFloatValue( pEnt->m_clrRender.a );
	}

}

EXPOSE_INTERFACE( CzTestMaterialProxy, IMaterialProxy, "zTest" IMATERIAL_PROXY_INTERFACE_VERSION );
