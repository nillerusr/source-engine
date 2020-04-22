//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialsystem.h"
#include "c_basetfplayer.h"
#include "c_tf_basecombatweapon.h"

class CInfiltratorCamoMaterialProxy : public CEntityMaterialProxy
{
public:
	CInfiltratorCamoMaterialProxy();
	virtual ~CInfiltratorCamoMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues* pKeyValues );
	virtual void OnBind( C_BaseEntity *pC_BaseEntity );

private:
	IMaterialVar *m_CamoVar;
};

CInfiltratorCamoMaterialProxy::CInfiltratorCamoMaterialProxy()
{
	m_CamoVar = NULL;
}

CInfiltratorCamoMaterialProxy::~CInfiltratorCamoMaterialProxy()
{
}


bool CInfiltratorCamoMaterialProxy::Init( IMaterial *pMaterial, KeyValues* pKeyValues  )
{
	bool foundVar;

	m_CamoVar = pMaterial->FindVar( "$alpha", &foundVar, false );
	if( !foundVar )
	{
		m_CamoVar = NULL;
		return false;
	}
	return true;
}

void CInfiltratorCamoMaterialProxy::OnBind( C_BaseEntity *pEnt )
{
	if( !m_CamoVar )
		return;

	C_BaseTFPlayer *player = dynamic_cast< C_BaseTFPlayer * >( pEnt );
	if ( player )
	{
		float amount = 1 - player->ComputeCamoEffectAmount();
		m_CamoVar->SetFloatValue( amount );
	}
	else
	{
		// Weapon model?
		C_BaseTFCombatWeapon *pWeapon = dynamic_cast< C_BaseTFCombatWeapon * >( pEnt );
		if ( pWeapon )
		{
			float amount = 1 - ((C_BaseTFPlayer *)pWeapon->GetOwner())->ComputeCamoEffectAmount();
			m_CamoVar->SetFloatValue( amount );
		}
		else
		{
			C_BaseViewModel *pViewmodel = dynamic_cast< C_BaseViewModel * >( pEnt );
			if ( pViewmodel )
			{
				// Get the local player's values
				player = C_BaseTFPlayer::GetLocalPlayer();
				float amount = 1 - player->ComputeCamoEffectAmount();
				m_CamoVar->SetFloatValue( amount );
			}
		}
	}
}

EXPOSE_INTERFACE( CInfiltratorCamoMaterialProxy, IMaterialProxy, "InfiltratorCamo" IMATERIAL_PROXY_INTERFACE_VERSION );
