//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "materialsystem/IMaterialProxy.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialVar.h"
#include "c_world.h"

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

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
		C_World *pWorld = GetClientWorldEntity();
		if ( pWorld )
		{
			m_pMinsVar->SetVecValue( (const float*)&pWorld->m_WorldMins, 3 );
			m_pMaxsVar->SetVecValue( (const float*)&pWorld->m_WorldMaxs, 3 );
		}
	}
}

IMaterial *CWorldDimsProxy::GetMaterial()
{
	return m_pMinsVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CWorldDimsProxy, WorldDims );


