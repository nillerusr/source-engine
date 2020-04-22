//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmematerial.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects_interfaces.h"

#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMaterial, CDmeMaterial );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeMaterial::OnConstruction()
{
	m_pMTL = NULL;
	m_mtlName.Init( this, "mtlName" );
}

void CDmeMaterial::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// resolve
//-----------------------------------------------------------------------------
void CDmeMaterial::Resolve()
{
	BaseClass::Resolve();
	if ( m_mtlName.IsDirty() )
	{
		m_pMTL = NULL; // no cleanup necessary
	}
}


//-----------------------------------------------------------------------------
// Sets the material
//-----------------------------------------------------------------------------
void CDmeMaterial::SetMaterial( const char *pMaterialName )
{
	m_mtlName = pMaterialName;
}


//-----------------------------------------------------------------------------
// Returns the material name
//-----------------------------------------------------------------------------
const char *CDmeMaterial::GetMaterialName() const
{
	return m_mtlName;
}


//-----------------------------------------------------------------------------
// accessor for cached IMaterial
//-----------------------------------------------------------------------------
IMaterial *CDmeMaterial::GetCachedMTL()
{
	if ( m_pMTL == NULL )
	{
		const char *mtlName = m_mtlName.Get();
		if ( mtlName == NULL )
			return NULL;
		m_pMTL = g_pMaterialSystem->FindMaterial( mtlName, NULL, false );
	}
	return m_pMTL;
}
