//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "materialsystem/IMaterialProxy.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialVar.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


// $monitorTextureVar
class CMonitorMaterialProxy : public IMaterialProxy
{
public:
	CMonitorMaterialProxy();
	virtual ~CMonitorMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
private:
	IMaterialVar *m_pMonitorTextureVar;
};

CMonitorMaterialProxy::CMonitorMaterialProxy()
{
	m_pMonitorTextureVar = NULL;
}

CMonitorMaterialProxy::~CMonitorMaterialProxy()
{
	m_pMonitorTextureVar = NULL;
}


bool CMonitorMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pMonitorTextureVarName = pKeyValues->getString( "$monitorTextureVar" );
	if( !pMonitorTextureVarName )
		return false;

	bool foundVar;
	m_pMonitorTextureVar = pMaterial->FindVar( pMonitorTextureVarName, &foundVar, false );
	if( !foundVar )
	{
		m_pMonitorTextureVar = NULL;
		return false;
	}
	return true;
}

void CMonitorMaterialProxy::OnBind( void *pC_BaseEntity )
{
	if( !m_pMonitorTextureVar )
	{
		return;
	}
}

EXPOSE_MATERIAL_PROXY( CMonitorMaterialProxy, Monitor );
