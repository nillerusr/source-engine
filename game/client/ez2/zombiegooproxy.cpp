//================= Copyright LOLOLOLOL, All rights reserved. ==================//
//
// Purpose: Makes zombie goo decals fade away based on given convars.
// 
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "functionproxy.h"

ConVar ez2_goo_puddle_time_linger("ez2_goo_puddle_time_linger", "30", FCVAR_REPLICATED);
ConVar ez2_goo_puddle_time_fade("ez2_goo_puddle_time_fade", "10", FCVAR_REPLICATED);

class CZombieGooMaterialProxy : public CResultProxy
{
public:
	CZombieGooMaterialProxy();
	virtual ~CZombieGooMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	IMaterialVar *m_pMaterialParamTime;
};

CZombieGooMaterialProxy::CZombieGooMaterialProxy()
{
	m_pMaterialParamTime = NULL;
}

CZombieGooMaterialProxy::~CZombieGooMaterialProxy()
{
	// Do nothing
}

bool CZombieGooMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	bool bFoundVar = false;

	m_pMaterialParamTime = pMaterial->FindVar( "$time", &bFoundVar, false );
	if ( bFoundVar == false)
		return false;

	return true;
}

void CZombieGooMaterialProxy::OnBind( void *pC_BaseEntity )
{
	if (m_pMaterialParamTime->GetFloatValue() == -1.0f)
		m_pMaterialParamTime->SetFloatValue( gpGlobals->curtime );

	if (!pC_BaseEntity)
		DevMsg("OnBind is not a pC_BaseEntity.\n");
	else
		DevMsg("IT IS A pC_BaseEntity!!!\n");

	// Figure out how to do this decal-by-decal
	float flTime = gpGlobals->curtime - m_pMaterialParamTime->GetFloatValue();
	if (flTime >= ez2_goo_puddle_time_linger.GetFloat())
	{
		flTime = 1.0f - (flTime - ez2_goo_puddle_time_linger.GetFloat()) / ez2_goo_puddle_time_fade.GetFloat();
		if (flTime < 0.0f)
			flTime = 0.0f;

		SetFloatResult(flTime);
	}

	DevMsg("Zombie goo fade: %f\n", flTime);
}

EXPOSE_INTERFACE( CZombieGooMaterialProxy, IMaterialProxy, "ZombieGooPuddle" IMATERIAL_PROXY_INTERFACE_VERSION );
