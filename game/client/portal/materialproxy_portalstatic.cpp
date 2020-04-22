//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterial.h"
#include "portalrenderable_flatbasic.h"
#include "c_prop_portal.h"
#include "toolframework_client.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPortalStaticProxy : public IMaterialProxy
{
protected:
	IMaterialVar *m_StaticOutput;
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pBind );
	virtual void Release( void ) { delete this; }

	virtual IMaterial *	GetMaterial() { return ( m_StaticOutput ) ? m_StaticOutput->GetOwningMaterial() : NULL; }

	float ComputeStaticAmount( CPortalRenderable_FlatBasic *pPortal );
};

bool CPortalStaticProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pszResultVar = pKeyValues->GetString( "resultVar" );
	if( !pszResultVar )
		return false;

	bool foundVar;
	m_StaticOutput = pMaterial->FindVar( pszResultVar, &foundVar, false );
	if( !foundVar )
		return false;

	return true;
}

float CPortalStaticProxy::ComputeStaticAmount( CPortalRenderable_FlatBasic *pFlatBasic )
{
	float flStaticAmount = pFlatBasic->m_fStaticAmount;

	if ( !pFlatBasic->GetLinkedPortal() )
	{
		flStaticAmount = 1.0f;
	}
	if ( pFlatBasic->WillUseDepthDoublerThisDraw() )
	{
		flStaticAmount = 0.0f;
	}
	else if ( g_pPortalRender->GetRemainingPortalViewDepth() == 0 ) //end of the line, no more views
	{
		flStaticAmount = 1.0f;
	}
	else if ( (g_pPortalRender->GetRemainingPortalViewDepth() == 1) && (pFlatBasic->m_fSecondaryStaticAmount > flStaticAmount) ) //fading in from no views to another view (player just walked through it)
	{
		flStaticAmount = pFlatBasic->m_fSecondaryStaticAmount;
	}
	return flStaticAmount;
}

void CPortalStaticProxy::OnBind( void *pBind )
{
	if ( pBind == NULL )
		return;
	
	float flStaticAmount;
	IClientRenderable *pRenderable = (IClientRenderable*)pBind;
	CPortalRenderable *pRecordedPortal = g_pPortalRender->FindRecordedPortal( pRenderable );

	if ( pRecordedPortal )
	{
		CPortalRenderable_FlatBasic *pRecordedFlatBasic = dynamic_cast<CPortalRenderable_FlatBasic *>(pRecordedPortal);
		if ( !pRecordedFlatBasic )
			return;

		flStaticAmount = ComputeStaticAmount( pRecordedFlatBasic );
	}
	else
	{
		CPortalRenderable_FlatBasic *pFlatBasic = dynamic_cast<CPortalRenderable_FlatBasic*>( pRenderable );
		if ( !pFlatBasic )
			return;

		flStaticAmount = ComputeStaticAmount( pFlatBasic );
	}

	m_StaticOutput->SetFloatValue( flStaticAmount );

	if ( ToolsEnabled() )
	{
		ToolFramework_RecordMaterialParams( GetMaterial() );
	}
}

EXPOSE_INTERFACE( CPortalStaticProxy, IMaterialProxy, "PortalStaticModel" IMATERIAL_PROXY_INTERFACE_VERSION );


class CPortalStaticPortalProxy : public CPortalStaticProxy
{
public:
	virtual void OnBind( void *pBind );
};

void CPortalStaticPortalProxy::OnBind( void *pBind )
{
	if ( pBind == NULL )
		return;

	IClientRenderable *pRenderable = (IClientRenderable*)( pBind );
	C_Prop_Portal *pPortal = (C_Prop_Portal *)pRenderable;

	float flStaticAmount = ComputeStaticAmount( pPortal );
	m_StaticOutput->SetFloatValue( flStaticAmount );
}

EXPOSE_INTERFACE( CPortalStaticPortalProxy, IMaterialProxy, "PortalStatic" IMATERIAL_PROXY_INTERFACE_VERSION );


class CPortalOpenAmountProxy : public CPortalStaticProxy
{
public:
	virtual void OnBind( void *pBind );
};

void CPortalOpenAmountProxy::OnBind( void *pBind )
{
	if ( pBind == NULL )
		return;

	IClientRenderable *pRenderable = (IClientRenderable*)( pBind );
	C_Prop_Portal *pPortal = (C_Prop_Portal *)pRenderable;

	m_StaticOutput->SetFloatValue( pPortal->m_fOpenAmount );
}

EXPOSE_INTERFACE( CPortalOpenAmountProxy, IMaterialProxy, "PortalOpenAmount" IMATERIAL_PROXY_INTERFACE_VERSION );


