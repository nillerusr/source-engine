//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include <KeyValues.h>
#include "materialsystem/imaterialvar.h"
#include "C_BaseTFPlayer.h"
#include "functionproxy.h"
#include "C_PlayerResource.h"
#include "Weapon_CombatShield.h"

// for the 2/5/03 demo
#include "c_demo_entities.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Shield proxy base class
//-----------------------------------------------------------------------------
class CTFShieldProxy : public CResultProxy
{
protected:
	C_WeaponCombatShield *BindArgToShield( void *pArg );
};


//-----------------------------------------------------------------------------
// Gets the shield
//-----------------------------------------------------------------------------
C_WeaponCombatShield *CTFShieldProxy::BindArgToShield( void *pArg )
{
	C_BaseEntity *pEntity = BindArgToEntity( pArg );
	if (!pEntity)
		return NULL;

	if (dynamic_cast<C_BaseViewModel*>(pEntity))
	{
		// If this is the view model, snack the state from the local player...
		C_BaseTFPlayer* pPlayer = C_BaseTFPlayer::GetLocalPlayer();
		return pPlayer->GetShield();
	}
	
	if ( dynamic_cast<C_BaseTFPlayer*>(pEntity) )
	{
		C_BaseTFPlayer* pPlayer = dynamic_cast<C_BaseTFPlayer*>( pEntity );
		return pPlayer->GetShield();
	}

	return dynamic_cast<C_WeaponCombatShield*>( pEntity );
}


//=============================================================================
//
// TF Shield raising proxy. 
//
class CTFShieldRaiseProxy : public CTFShieldProxy
{
public:

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pEnt );

private:
	CFloatInput	m_Factor;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CTFShieldRaiseProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	// Init shield raise times.
	if ( !m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTFShieldRaiseProxy::OnBind( void *pArg )
{
	float flTimeSinceRaised = 0.0f;
	C_WeaponCombatShield *pShield = BindArgToShield( pArg );
	if (pShield)
	{
		flTimeSinceRaised = pShield->GetRaisingTime();
	}
	
	// Return the time in seconds from when we started raising.
	SetFloatResult( flTimeSinceRaised * m_Factor.GetFloat() );
}

EXPOSE_INTERFACE( CTFShieldRaiseProxy, IMaterialProxy, "TFShieldRaise" IMATERIAL_PROXY_INTERFACE_VERSION );

//=============================================================================
//
// TF Shield lowering proxy. 
//
class CTFShieldLowerProxy : public CTFShieldProxy
{
public:

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pEnt );

private:
	CFloatInput	m_Factor;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CTFShieldLowerProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	// Init shield raise times.
	if ( !m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTFShieldLowerProxy::OnBind( void *pArg )
{
	float flTimeSinceLowered = 0.0f;
	C_WeaponCombatShield *pShield = BindArgToShield( pArg );
	if ( pShield )
	{
		flTimeSinceLowered = pShield->GetLoweringTime();
	}
	else
	{
		// NOTE: This is for the Feb 5 03 demo only
		C_BaseEntity *pEntity = BindArgToEntity( pArg );
		C_Cycler_TF2Commando *pDemoCommando = dynamic_cast<C_Cycler_TF2Commando*>( pEntity );
		if (pDemoCommando)
		{
			flTimeSinceLowered = pDemoCommando->GetShieldLowerTime();
		}
	}

	// Return the time in seconds from when we started lowering.
	Assert( m_pResult );
	SetFloatResult( flTimeSinceLowered * m_Factor.GetFloat() );

//	Msg( "Lowering = %f\n", flTimeSinceLowered );
}

EXPOSE_INTERFACE( CTFShieldLowerProxy, IMaterialProxy, "TFShieldLower" IMATERIAL_PROXY_INTERFACE_VERSION );

//=============================================================================
//
// TF Shield raising proxy. 
//
class CTFShieldVisibilityProxy : public CTFShieldProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pEnt );

private:
	CFloatInput	m_RaiseFactor;
	CFloatInput	m_LowerFactor;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CTFShieldVisibilityProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	// Init shield raise times.
	if ( !m_RaiseFactor.Init( pMaterial, pKeyValues, "raisescale", 1 ) )
		return false;

	if ( !m_LowerFactor.Init( pMaterial, pKeyValues, "lowerscale", 1 ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTFShieldVisibilityProxy::OnBind( void *pArg )
{
	float flAlpha = 0.0f;
	C_WeaponCombatShield *pShield = BindArgToShield( pArg );
	if ( pShield )
	{
		if (pShield->GetRaisingTime() != 0.0f)
		{
			flAlpha = pShield->GetRaisingTime() * m_RaiseFactor.GetFloat();
			flAlpha = clamp( flAlpha, 0, 1 );
		}
		else
		{
			flAlpha = 1.0f - pShield->GetLoweringTime() * m_LowerFactor.GetFloat();
			flAlpha = clamp( flAlpha, 0, 1 );
		}
	}

	// Return the time in seconds from when we started lowering.
	Assert( m_pResult );
	SetFloatResult( flAlpha );
}

EXPOSE_INTERFACE( CTFShieldVisibilityProxy, IMaterialProxy, "TFShieldVisibility" IMATERIAL_PROXY_INTERFACE_VERSION );


//=============================================================================
//
// TF Shield active proxy. 
//
class CTFShieldActiveProxy : public CTFShieldProxy
{
public:

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pEnt );
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CTFShieldActiveProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTFShieldActiveProxy::OnBind( void *pArg )
{
	bool bIsShieldUp = false;
	C_WeaponCombatShield *pShield = BindArgToShield( pArg );
	if ( pShield )
	{
		bIsShieldUp = ( pShield->IsUp() == 1.0f );
	}
	else
	{
		// NOTE: This is for the Feb 5 03 demo only
		C_BaseEntity *pEntity = BindArgToEntity( pArg );
		C_Cycler_TF2Commando *pDemoCommando = dynamic_cast<C_Cycler_TF2Commando*>( pEntity );
		if (pDemoCommando)
		{
			bIsShieldUp = pDemoCommando->IsShieldActive();
		}
	}

	Assert( m_pResult );
	SetFloatResult( ( float )bIsShieldUp );

//	Msg( "Active = %d\n", bIsShieldUp );
}

EXPOSE_INTERFACE( CTFShieldActiveProxy, IMaterialProxy, "TFShieldActive" IMATERIAL_PROXY_INTERFACE_VERSION );

//=============================================================================
//
// TF Shield health proxy. 
//
class CTFShieldHealthProxy : public CTFShieldProxy
{
public:

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pEnt );
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CTFShieldHealthProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTFShieldHealthProxy::OnBind( void *pArg )
{
	float flShieldHealth = 1;
	C_WeaponCombatShield *pShield = BindArgToShield( pArg );
	if ( pShield )
	{
		flShieldHealth = pShield->GetShieldHealth();
	}

	Assert( m_pResult );
	SetFloatResult( flShieldHealth );
}

EXPOSE_INTERFACE( CTFShieldHealthProxy, IMaterialProxy, "TFShieldHealth" IMATERIAL_PROXY_INTERFACE_VERSION );
