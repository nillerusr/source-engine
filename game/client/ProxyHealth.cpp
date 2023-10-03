//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "FunctionProxy.h"

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Returns the player health (from 0 to 1)
//-----------------------------------------------------------------------------
class CProxyHealth : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Factor;
};

bool CProxyHealth::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CProxyHealth::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	Assert( m_pResult );
	SetFloatResult( pEntity->HealthFraction() * m_Factor.GetFloat() );
}

EXPOSE_MATERIAL_PROXY( CProxyHealth, Health );


