//=============================================================================//
//
// Purpose: Pulse pistol clientside effects.
// 
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_basehlcombatweapon.h"
#include "proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "baseviewmodel_shared.h"

class C_WeaponPulsePistol : public C_BaseHLCombatWeapon
{
public:
	DECLARE_CLASS( C_WeaponPulsePistol, C_BaseHLCombatWeapon );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_WeaponPulsePistol();
	~C_WeaponPulsePistol();

	// Don't show the reserve vlaue in the HUD
	//bool ShouldDisplaySecondaryValue( void ) const { return false; }
};

LINK_ENTITY_TO_CLASS( weapon_pulsepistol, C_WeaponPulsePistol );

IMPLEMENT_CLIENTCLASS_DT( C_WeaponPulsePistol, DT_WeaponPulsePistol, CWeaponPulsePistol )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_WeaponPulsePistol )
END_PREDICTION_DATA()

C_WeaponPulsePistol::C_WeaponPulsePistol()
{
}

C_WeaponPulsePistol::~C_WeaponPulsePistol()
{
}

//-----------------------------------------------------------------------------
// Apply effects when the pulse pistol is charging
//-----------------------------------------------------------------------------
class CPulsePistolMaterialProxy : public CEntityMaterialProxy
{
public:
	CPulsePistolMaterialProxy() { m_ChargeVar = NULL; }
	virtual ~CPulsePistolMaterialProxy() {}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( C_BaseEntity *pEntity );
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar *m_ChargeVar;
	//IMaterialVar *m_FullyChargedVar;
};

bool CPulsePistolMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundAnyVar = false;
	bool foundVar;

	const char *pszChargeVar = pKeyValues->GetString( "outputCharge" );
	m_ChargeVar = pMaterial->FindVar( pszChargeVar, &foundVar, true );
	if (foundVar)
		foundAnyVar = true;

	//const char *pszFullyChargedVar = pKeyValues->GetString( "outputFullyCharged" );
	//m_FullyChargedVar = pMaterial->FindVar( pszFullyChargedVar, &foundVar, true );
	//if (foundVar)
	//	foundAnyVar = true;

	return foundAnyVar;
}

void CPulsePistolMaterialProxy::OnBind( C_BaseEntity *pEnt )
{
	if (!m_ChargeVar)
		return;

	C_WeaponPulsePistol *pPistol = NULL;
	if ( pEnt->IsBaseCombatWeapon() )
		pPistol = assert_cast<C_WeaponPulsePistol*>( pEnt );
	else
	{
		C_BaseViewModel *pVM = dynamic_cast<C_BaseViewModel*>( pEnt );
		if (pVM)
			pPistol = assert_cast<C_WeaponPulsePistol*>( pVM->GetOwningWeapon() );
	}

	if ( pPistol )
	{
		// Keep synced with the max in weapon_pistol.cpp
		const float flMaxCharge = 40.0f;
		m_ChargeVar->SetFloatValue( ((float)pPistol->m_iClip2) / flMaxCharge );

		//m_FullyChargedVar->SetFloatValue( pPistol->m_iClip2 == flMaxCharge ? 1.0f : 0.0f );
	}
	else
	{
		m_ChargeVar->SetFloatValue( 0.0f );
	}
}

IMaterial *CPulsePistolMaterialProxy::GetMaterial()
{
	if ( !m_ChargeVar )
		return NULL;

	return m_ChargeVar->GetOwningMaterial();
}

EXPOSE_INTERFACE( CPulsePistolMaterialProxy, IMaterialProxy, "PulsePistolCharge" IMATERIAL_PROXY_INTERFACE_VERSION );
