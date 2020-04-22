//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================


#include "datamodel/dmelementfactoryhelper.h"
#include "tier1/KeyValues.h"
#include "tier1/utlrbtree.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmedrawsettings.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeDrawSettings, CDmeDrawSettings );


//-----------------------------------------------------------------------------
// Statics
//-----------------------------------------------------------------------------
bool CDmeDrawSettings::s_bWireframeMaterialInitialized( false );
CMaterialReference CDmeDrawSettings::s_WireframeMaterial;
bool CDmeDrawSettings::s_bWireframeOnShadedMaterialInitialized( false );
CMaterialReference CDmeDrawSettings::s_WireframeOnShadedMaterial;
bool CDmeDrawSettings::s_bFlatGrayMaterial( false );
CMaterialReference CDmeDrawSettings::s_FlatGrayMaterial;
bool CDmeDrawSettings::s_bUnlitGrayMaterial( false );
CMaterialReference CDmeDrawSettings::s_UnlitGrayMaterial;

CUtlRBTree< CUtlSymbol > CDmeDrawSettings::s_KnownDrawableTypes;


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDrawSettings::OnConstruction()
{
	if ( s_KnownDrawableTypes.Count() == 0 )
	{
		BuildKnownDrawableTypes();
	}

	SetDefLessFunc< CUtlRBTree< CUtlSymbol > >( m_NotDrawable );
	m_NotDrawable.RemoveAll();

	m_DrawType.InitAndSet( this, "drawType", static_cast< int >( DRAW_SMOOTH ) );
	m_bBackfaceCulling.InitAndSet( this, "backfaceCulling", true );
	m_bWireframeOnShaded.InitAndSet( this, "wireframeOnShaded", false );
	m_bXRay.InitAndSet( this, "xray", false );
	m_bGrayShade.InitAndSet( this, "grayShade", false );
	m_bNormals.InitAndSet( this, "normals", false );
	m_NormalLength.InitAndSet( this, "normalLength", 1.0 );
	m_Color.InitAndSet( this, "color", Color( 0, 0, 0, 1 ) );
	m_bDeltaHighlight.InitAndSet( this, "highlightDeltas", false );
	m_flHighlightSize.InitAndSet( this, "highlightSize", 1.5f );
	m_cHighlightColor.InitAndSet( this, "highlightColor", Color( 0xff, 0x14, 0x93, 0xff ) );	// Deep Pink

	m_IsAMaterialBound = false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDrawSettings::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDrawSettings::Resolve()
{
}


//-----------------------------------------------------------------------------
// Wireframe
//-----------------------------------------------------------------------------
void CDmeDrawSettings::BindWireframe()
{
	if ( !s_bWireframeMaterialInitialized )
	{
		s_bWireframeMaterialInitialized = true;

		KeyValues *pKeyValues = new KeyValues( "wireframe" );
		pKeyValues->SetInt( "$decal", 0 );
		pKeyValues->SetInt( "$vertexcolor", 1 );
		pKeyValues->SetInt( "$ignorez", 1 );
		s_WireframeMaterial.Init( "__DmeWireframe", pKeyValues );
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( s_WireframeMaterial );
	m_IsAMaterialBound = true;
}


//-----------------------------------------------------------------------------
// Wireframe as a decal
//-----------------------------------------------------------------------------
void CDmeDrawSettings::BindWireframeOnShaded()
{
	if ( !s_bWireframeOnShadedMaterialInitialized )
	{
		s_bWireframeOnShadedMaterialInitialized = true;

		KeyValues *pKeyValues = new KeyValues( "wireframe" );
		pKeyValues->SetInt( "$decal", 0 );
		pKeyValues->SetInt( "$vertexcolor", 1 );
		pKeyValues->SetInt( "$ignorez", 0 );
		s_WireframeOnShadedMaterial.Init( "__DmeWireframeOnShaded", pKeyValues );
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( s_WireframeOnShadedMaterial );
	m_IsAMaterialBound = true;
}


//-----------------------------------------------------------------------------
// Flat Gray Shaded
//-----------------------------------------------------------------------------
void CDmeDrawSettings::BindGray()
{
	if ( !s_bFlatGrayMaterial )
	{
		s_bFlatGrayMaterial = true;

		KeyValues *pKeyValues = new KeyValues( "VertexLitGeneric" );
		pKeyValues->SetInt( "$model", 1 );
		s_FlatGrayMaterial.Init( "__DmeFlatGray", pKeyValues );
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( s_FlatGrayMaterial );
	m_IsAMaterialBound = true;
}


//-----------------------------------------------------------------------------
// Flat Gray Shaded
//-----------------------------------------------------------------------------
void CDmeDrawSettings::BindUnlitGray()
{
	if ( !s_bUnlitGrayMaterial )
	{
		s_bUnlitGrayMaterial = true;

		KeyValues *pKeyValues = new KeyValues( "UnlitGeneric" );
		pKeyValues->SetInt( "$model", 1 );
		pKeyValues->SetInt( "$vertexcolor", 1 );
		s_UnlitGrayMaterial.Init( "__DmeUnlitGray", pKeyValues );
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( s_UnlitGrayMaterial );
	m_IsAMaterialBound = true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeDrawSettings::Drawable( CDmElement *pElement )
{
	if ( m_NotDrawable.IsValidIndex( m_NotDrawable.Find( pElement->GetType() ) ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDrawSettings::BuildKnownDrawableTypes()
{
	SetDefLessFunc< CUtlRBTree< CUtlSymbol > >( s_KnownDrawableTypes );

	s_KnownDrawableTypes.RemoveAll();

	s_KnownDrawableTypes.InsertIfNotFound( g_pDataModel->GetSymbol( "DmeMesh" ) );
	s_KnownDrawableTypes.InsertIfNotFound( g_pDataModel->GetSymbol( "DmeJoint" ) );
	s_KnownDrawableTypes.InsertIfNotFound( g_pDataModel->GetSymbol( "DmeModel" ) );
	s_KnownDrawableTypes.InsertIfNotFound( g_pDataModel->GetSymbol( "DmeAttachment" ) );

	m_NotDrawable.RemoveAll();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDrawSettings::DrawDag( CDmeDag *pDag )
{
	if ( !pDag )
		return;

	m_vHighlightPoints.RemoveAll();

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	m_IsAMaterialBound = false;

	if ( GetDeltaHighlight() )
	{
		BindUnlitGray();
	}
	else
	{
		if ( !Shaded() )
		{
			BindWireframe();
		}
		else if ( GetGrayShade() )
		{
			BindGray();
		}
	}

	pDag->Draw( this );

	m_IsAMaterialBound = false;

	if ( GetDeltaHighlight() || ( GetWireframeOnShaded() && Shaded() ) )
	{
		VMatrix m;

		pRenderContext->GetMatrix( MATERIAL_PROJECTION, &m );

		/*	Extract the near and far clipping plane values from projection matrix
		float c = m[ 2 ][ 2 ];
		float d = m[ 2 ][ 3 ];

		const float near = d / ( c - 1.0f );
		const float far = d / ( c + 1.0f );
		*/

		const float zBias = 0.00025;
		m[ 2 ][ 2 ] += zBias;
		m[ 2 ][ 3 ] += zBias;

		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PushMatrix();
		pRenderContext->LoadMatrix( m );

		BindWireframeOnShaded();
		PushDrawType();
		SetDrawType( CDmeDrawSettings::DRAW_WIREFRAME );
		pDag->Draw( this );
		PopDrawType();

		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PopMatrix();
	}
}