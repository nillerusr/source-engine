//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmemdl.h"
#include "movieobjects/dmetransform.h"
#include "movieobjects/dmedag.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datacache/imdlcache.h"
#include "istudiorender.h"
#include "bone_setup.h"
#include "tier3/tier3.h"
#include "tier3/mdlutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMDL, CDmeMDL );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeMDL::OnConstruction()
{
	m_bDrawInEngine = false;
//	SetAttributeValueElement( "transform", CreateElement< CDmeTransform >() );
//	SetAttributeValue( "mdlfilename", "models/alyx.mdl" );
	m_Color.InitAndSet( this, "color", Color( 255, 255, 255, 255 ) );
	m_nSkin.InitAndSet( this, "skin", 0 );
	m_nBody.InitAndSet( this, "body", 0 );
	m_nSequence.InitAndSet( this, "sequence", 0 );
	m_nLOD.InitAndSet( this, "lod", 0 );
	m_flPlaybackRate.InitAndSet( this, "playbackrate", 30.0f );
	m_flTime.InitAndSet( this, "time", 0.0f );
	m_vecViewTarget.Init( this, "viewTarget" );
	m_bWorldSpaceViewTarget.Init( this, "worldSpaceViewTarget" );
}

void CDmeMDL::OnDestruction()
{
	m_MDL.SetMDL( MDLHANDLE_INVALID );
}


void CDmeMDL::SetMDL( MDLHandle_t handle )
{
	m_MDL.SetMDL( handle );

	Vector vecMins, vecMaxs;
	GetMDLBoundingBox( &vecMins, &vecMaxs, m_MDL.GetMDL(), m_nSequence );
	Vector vecLookAt( 100.0f, 0.0f, vecMaxs.z );

	m_vecViewTarget.Set( vecLookAt );
	m_bWorldSpaceViewTarget = false;
}

MDLHandle_t CDmeMDL::GetMDL( ) const
{
	return m_MDL.GetMDL();
}

	
//-----------------------------------------------------------------------------
// Loads the model matrix based on the transform
//-----------------------------------------------------------------------------
void CDmeMDL::DrawInEngine( bool bDrawInEngine )
{
	m_bDrawInEngine = bDrawInEngine;
}

bool CDmeMDL::IsDrawingInEngine() const
{
	return m_bDrawInEngine;
}
	

//-----------------------------------------------------------------------------
// Returns the bounding box for the model
//-----------------------------------------------------------------------------
void CDmeMDL::GetBoundingBox( Vector *pMins, Vector *pMaxs ) const
{
	GetMDLBoundingBox( pMins, pMaxs, m_MDL.GetMDL(), m_nSequence );

	// Rotate the root transform to make it align with DMEs
	// DMEs up vector is the y axis
	if ( !m_bDrawInEngine )
	{
		Vector vecMins, vecMaxs;
		matrix3x4_t engineToDme;
		CDmeDag::EngineToDmeMatrix( engineToDme );
		TransformAABB( engineToDme, *pMins, *pMaxs, vecMins, vecMaxs );
		*pMins = vecMins;
		*pMaxs = vecMaxs;
	}
}


//-----------------------------------------------------------------------------
// Returns the radius of the model as measured from the origin
//-----------------------------------------------------------------------------
float CDmeMDL::GetRadius() const
{
	return GetMDLRadius( m_MDL.GetMDL(), m_nSequence );
}

	
//-----------------------------------------------------------------------------
// Returns a more accurate bounding sphere
//-----------------------------------------------------------------------------
void CDmeMDL::GetBoundingSphere( Vector &vecCenter, float &flRadius )
{
	Vector vecEngineCenter;
	GetMDLBoundingSphere( &vecEngineCenter, &flRadius, m_MDL.GetMDL(), m_nSequence );

	// Rotate the root transform to make it align with DMEs
	// DMEs up vector is the y axis
	if ( !m_bDrawInEngine )
	{
		matrix3x4_t engineToDme;
		CDmeDag::EngineToDmeMatrix( engineToDme );
		VectorTransform( vecEngineCenter, engineToDme, vecCenter );
	}
	else
	{
		vecCenter = vecEngineCenter;
	}
}

	
//-----------------------------------------------------------------------------
// Updates the MDL rendering helper
//-----------------------------------------------------------------------------
void CDmeMDL::UpdateMDL()
{
	m_MDL.m_Color = m_Color;
	m_MDL.m_nSkin = m_nSkin;
	m_MDL.m_nBody = m_nBody;
	m_MDL.m_nSequence = m_nSequence;
	m_MDL.m_nLOD = m_nLOD;
	m_MDL.m_flPlaybackRate = m_flPlaybackRate;
	m_MDL.m_flTime = m_flTime;
	m_MDL.m_vecViewTarget = m_vecViewTarget;
	m_MDL.m_Color = m_Color;
	m_MDL.m_bWorldSpaceViewTarget = m_bWorldSpaceViewTarget;
}


//-----------------------------------------------------------------------------
// Draws the mesh
//-----------------------------------------------------------------------------
void CDmeMDL::Draw( const matrix3x4_t &shapeToWorld, CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	UpdateMDL();
	studiohdr_t *pStudioHdr = m_MDL.GetStudioHdr();
	if ( !pStudioHdr )
		return;

	// FIXME: Why is this necessary!?!?!?
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	if ( !m_bDrawInEngine )
	{
		pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
	}

	matrix3x4_t *pBoneToWorld = g_pStudioRender->LockBoneMatrices( pStudioHdr->numbones );
	SetUpBones( shapeToWorld, pStudioHdr->numbones, pBoneToWorld );
	g_pStudioRender->UnlockBoneMatrices();

	m_MDL.Draw( shapeToWorld, pBoneToWorld );

	// FIXME: Why is this necessary!?!?!?
	if ( !m_bDrawInEngine )
	{
		pRenderContext->CullMode( MATERIAL_CULLMODE_CW );
	}
}


void CDmeMDL::SetUpBones( const matrix3x4_t& shapeToWorld, int nMaxBoneCount, matrix3x4_t *pOutputMatrices )
{
	UpdateMDL();

	// Root transform
	matrix3x4_t rootToWorld;

	// Rotate the root transform to make it align with DMEs
	// DMEs up vector is the y axis
	if ( !m_bDrawInEngine )
	{
		matrix3x4_t engineToDme;
		CDmeDag::EngineToDmeMatrix( engineToDme );
		ConcatTransforms( engineToDme, shapeToWorld, rootToWorld );
	}
	else
	{
		MatrixCopy( shapeToWorld, rootToWorld );
	}

	m_MDL.SetUpBones( rootToWorld, nMaxBoneCount, pOutputMatrices );
}
