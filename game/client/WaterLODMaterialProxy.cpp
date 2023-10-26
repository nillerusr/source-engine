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
#include "iviewrender.h"

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// no inputs, assumes that the results go into $CHEAPWATERSTARTDISTANCE and $CHEAPWATERENDDISTANCE
class CWaterLODMaterialProxy : public IMaterialProxy
{
public:
	CWaterLODMaterialProxy();
	virtual ~CWaterLODMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar *m_pCheapWaterStartDistanceVar;
	IMaterialVar *m_pCheapWaterEndDistanceVar;
};

CWaterLODMaterialProxy::CWaterLODMaterialProxy()
{
	m_pCheapWaterStartDistanceVar = NULL;
	m_pCheapWaterEndDistanceVar = NULL;
}

CWaterLODMaterialProxy::~CWaterLODMaterialProxy()
{
}


bool CWaterLODMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_pCheapWaterStartDistanceVar = pMaterial->FindVar( "$CHEAPWATERSTARTDISTANCE", &foundVar, false );
	if( !foundVar )
		return false;

	m_pCheapWaterEndDistanceVar = pMaterial->FindVar( "$CHEAPWATERENDDISTANCE", &foundVar, false );
	if( !foundVar )
		return false;

	return true;
}

void CWaterLODMaterialProxy::OnBind( void *pC_BaseEntity )
{
	if( !m_pCheapWaterStartDistanceVar || !m_pCheapWaterEndDistanceVar )
	{
		return;
	}
	float start, end;
	view->GetWaterLODParams( start, end );
	m_pCheapWaterStartDistanceVar->SetFloatValue( start );
	m_pCheapWaterEndDistanceVar->SetFloatValue( end );
}

IMaterial *CWaterLODMaterialProxy::GetMaterial()
{
	return m_pCheapWaterStartDistanceVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CWaterLODMaterialProxy, WaterLOD );
