//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Decorator class to make a DME renderable as a MDL
//													 
//===========================================================================//

#ifndef DMEMDLRENDERABLE_H
#define DMEMDLRENDERABLE_H
#ifdef _WIN32
#pragma once
#endif

#include "toolutils/dmerenderable.h"
#include "movieobjects/dmemdl.h"
#include "movieobjects/dmetransform.h"
#include "datacache/imdlcache.h"
#include "mathlib/mathlib.h"
#include "datamodel/dmehandle.h"
#include "toolutils/enginetools_int.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier3/tier3.h"


//-----------------------------------------------------------------------------
// Deals with the base implementation for turning a Dme into a renderable
//-----------------------------------------------------------------------------
template < class T >
class CDmeMdlRenderable : public CDmeRenderable< T >
{
	DEFINE_UNINSTANCEABLE_ELEMENT( CDmeMdlRenderable, CDmeRenderable< T > );

// IClientUnknown implementation.
public:
	virtual int					GetBody();
	virtual int					GetSkin();
	virtual int					DrawModel( int flags );
	virtual void				GetRenderBounds( Vector& mins, Vector& maxs );

	void SetModelName( const char *pMDLName );

protected:
	CDmeMDL *GetMDL() { return m_hMDL; }

private:
	void SetUpLighting( const Vector &vecCenter );

	CDmeHandle<CDmeMDL> m_hMDL;
	CDmeHandle<CDmeTransform> m_hTransform;
};


//-----------------------------------------------------------------------------
// Construction, destruction
//-----------------------------------------------------------------------------
template < class T >
void CDmeMdlRenderable<T>::OnConstruction() 
{
	m_hMDL = CreateElement<CDmeMDL>( "MDLRenderable", GetFileId() );
	m_hTransform = CreateElement<CDmeTransform>( "MDLTransform", GetFileId() );
}

template < class T >
void CDmeMdlRenderable<T>::OnDestruction() 
{
	g_pDataModel->DestroyElement( m_hMDL );
	g_pDataModel->DestroyElement( m_hTransform );
}

template < class T >
int	CDmeMdlRenderable<T>::GetBody()
{
	return m_hMDL->m_nBody;
}

template < class T >
int	CDmeMdlRenderable<T>::GetSkin()
{
	return m_hMDL->m_nSkin;
}

template < class T >
int	CDmeMdlRenderable<T>::DrawModel( int flags )
{
	matrix3x4_t mat;
	AngleMatrix( GetRenderAngles(), GetRenderOrigin(), mat );
	m_hTransform->SetTransform( mat );
	m_hMDL->m_flTime = Plat_FloatTime();
	SetUpLighting( GetRenderOrigin() );
	bool bIsDrawingInEngine = m_hMDL->IsDrawingInEngine();
	m_hMDL->DrawInEngine( true );
	m_hMDL->Draw( mat );
	m_hMDL->DrawInEngine( bIsDrawingInEngine );
	return 1;
}

template < class T >
void CDmeMdlRenderable<T>::GetRenderBounds( Vector& mins, Vector& maxs )
{
	m_hMDL->GetBoundingBox( &mins, &maxs );
}

template < class T >
void CDmeMdlRenderable<T>::SetModelName( const char *pMDLRelativePath )
{
	if ( pMDLRelativePath )
	{
		MDLHandle_t hMdl = g_pMDLCache->FindMDL( pMDLRelativePath );
		m_hMDL->SetMDL( hMdl );
	}
	else
	{
		m_hMDL->SetMDL( MDLHANDLE_INVALID );
	}
}

//-----------------------------------------------------------------------------
// Set up lighting conditions
//-----------------------------------------------------------------------------
template < class T >
void CDmeMdlRenderable<T>::SetUpLighting( const Vector &vecCenter )
{
	// Set up lighting conditions
	Vector vecAmbient[6];
	Vector4D vecAmbient4D[6];
	LightDesc_t desc[2];
	int nLightCount = enginetools->GetLightingConditions( vecCenter, vecAmbient, 2, desc );
	int nMaxLights = g_pMaterialSystemHardwareConfig->MaxNumLights();
	if( nLightCount > nMaxLights )
	{
		nLightCount = nMaxLights;
	}

	int i;
	for( i = 0; i < 6; i++ )
	{
		VectorCopy( vecAmbient[i], vecAmbient4D[i].AsVector3D() );
		vecAmbient4D[i][3] = 1.0f;
	}
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetAmbientLightCube( vecAmbient4D );

	for( i = 0; i < nLightCount; i++ )
	{
		LightDesc_t *pLight = &desc[i];
		pLight->m_Flags = 0;
		if( pLight->m_Attenuation0 != 0.0f )
		{
			pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0;
		}
		if( pLight->m_Attenuation1 != 0.0f )
		{
			pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1;
		}
		if( pLight->m_Attenuation2 != 0.0f )
		{
			pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2;
		}

		pRenderContext->SetLight( i, desc[i] );
	}

	for( ; i < nMaxLights; i++ )
	{
		LightDesc_t disableDesc;
		disableDesc.m_Type = MATERIAL_LIGHT_DISABLE;
		pRenderContext->SetLight( i, disableDesc );
	}
}


#endif // DMEMDLRENDERABLE_H
