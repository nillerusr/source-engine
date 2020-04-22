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
#include "c_baseobject.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CObjectBuildAlphaProxy : public CEntityMaterialProxy
{
public:
	CObjectBuildAlphaProxy();
	virtual ~CObjectBuildAlphaProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues* pKeyValues );
	virtual void OnBind( C_BaseEntity *pC_BaseEntity );

private:
	IMaterialVar* m_pAlphaVar;

	float		buildstart;
	float		buildend;
};


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------

CObjectBuildAlphaProxy::CObjectBuildAlphaProxy()
{
	m_pAlphaVar = 0;
}

CObjectBuildAlphaProxy::~CObjectBuildAlphaProxy()
{
}

//-----------------------------------------------------------------------------
// Init baby...
//-----------------------------------------------------------------------------
bool CObjectBuildAlphaProxy::Init( IMaterial *pMaterial, KeyValues* pKeyValues )
{

	bool foundVar;
	m_pAlphaVar = pMaterial->FindVar( "$alpha", &foundVar, false );
	if( !foundVar )
	{
		m_pAlphaVar = 0;
	}

	buildstart = pKeyValues->GetFloat( "buildstart", 1.0f );
	buildend = pKeyValues->GetFloat( "buildfinish", 1.0f );

	return true;
}


//-----------------------------------------------------------------------------
// Set the appropriate texture...
//-----------------------------------------------------------------------------
void CObjectBuildAlphaProxy::OnBind( C_BaseEntity *pEntity )
{
	if( !m_pAlphaVar )
		return;
	
	// It needs to be a TF2 C_BaseObject to have this proxy applied
	C_BaseObject *pObject = dynamic_cast< C_BaseObject * >( pEntity );
	if ( !pObject )
		return;

	float build_amount = pObject->GetCycle(); //pObject->GetPercentageConstructed();
	float frac;

	if ( build_amount <= buildstart )
	{
		frac = 0.0f;
	}
	else if ( build_amount >= buildend )
	{
		frac = 1.0f;
	}
	else
	{
		// Avoid div by zero
		if ( buildend == buildstart )
		{
			frac = 1.0f;
		}
		else
		{
			frac = ( build_amount - buildstart ) / ( buildend - buildstart );
			frac = clamp( frac, 0.0f, 1.0f );
		}
	}

	if ( !pObject->IsBuilding() )
	{
		frac = 1.0f;
	}

	m_pAlphaVar->SetFloatValue( frac );
}

EXPOSE_INTERFACE( CObjectBuildAlphaProxy, IMaterialProxy, "TFObjectBuildAlpha" IMATERIAL_PROXY_INTERFACE_VERSION );


