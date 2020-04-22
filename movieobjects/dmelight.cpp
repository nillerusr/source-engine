//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmelight.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "mathlib/vector.h"
#include "movieobjects/dmetransform.h"
#include "materialsystem/imaterialsystem.h"
#include "movieobjects_interfaces.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeLight, CDmeLight );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeLight::OnConstruction()
{
	m_Color.InitAndSet( this, "color", Color( 255, 255, 255, 255 ), FATTRIB_HAS_CALLBACK );
	m_flIntensity.InitAndSet( this, "intensity", 1.0f, FATTRIB_HAS_CALLBACK );
}

void CDmeLight::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets the color and intensity
// NOTE: Color is specified 0-255 floating point.
//-----------------------------------------------------------------------------
void CDmeLight::SetColor( const Color &color )
{
	m_Color.Set( color );
}

void CDmeLight::SetIntensity( float flIntensity )
{
	m_flIntensity = flIntensity;
}


//-----------------------------------------------------------------------------
// Sets up render state in the material system for rendering
//-----------------------------------------------------------------------------
void CDmeLight::SetupRenderStateInternal( LightDesc_t &desc, float flAtten0, float flAtten1, float flAtten2 )
{
	desc.m_Color[0] = m_Color.Get().r();
	desc.m_Color[1] = m_Color.Get().g();
	desc.m_Color[2] = m_Color.Get().b();
	desc.m_Color *= m_flIntensity / 255.0f;

	desc.m_Attenuation0 = flAtten0;
	desc.m_Attenuation1 = flAtten1;
	desc.m_Attenuation2 = flAtten2;

	desc.m_Flags = 0;
	if ( desc.m_Attenuation0 != 0.0f )
	{
		desc.m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0;
	}
	if ( desc.m_Attenuation1 != 0.0f )
	{
		desc.m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1;
	}
	if ( desc.m_Attenuation2 != 0.0f )
	{
		desc.m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2;
	}
}


//-----------------------------------------------------------------------------
//   Sets lighting state
//-----------------------------------------------------------------------------
void CDmeLight::SetupRenderState( int nLightIndex )
{
	LightDesc_t desc;
	if ( GetLightDesc( &desc ) )
	{
		// FIXME: Should we pass the light color in?
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->SetLight( nLightIndex, desc );
	}
}



//-----------------------------------------------------------------------------
//
// A directional light
//
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeDirectionalLight, CDmeDirectionalLight );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeDirectionalLight::OnConstruction()
{
	m_Direction.InitAndSet( this, "direction", Vector( 0.0f, 0.0f, -1.0f ), FATTRIB_HAS_CALLBACK );
}

void CDmeDirectionalLight::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets the light direction
//-----------------------------------------------------------------------------
void CDmeDirectionalLight::SetDirection( const Vector &direction )
{
	m_Direction.Set( direction );
}


//-----------------------------------------------------------------------------
// Gets a light desc for the light
//-----------------------------------------------------------------------------
bool CDmeDirectionalLight::GetLightDesc( LightDesc_t *pDesc )
{
	memset( pDesc, 0, sizeof(LightDesc_t) );

	pDesc->m_Type = MATERIAL_LIGHT_DIRECTIONAL;
	SetupRenderStateInternal( *pDesc, 1.0f, 0.0f, 0.0f );

	matrix3x4_t m;
	GetTransform()->GetTransform( m );
	VectorRotate( m_Direction.Get(), m, pDesc->m_Direction );
	VectorNormalize( pDesc->m_Direction );

	pDesc->m_Theta = 0.0f;
	pDesc->m_Phi = 0.0f;
	pDesc->m_Falloff = 1.0f;

	return true;
}


//-----------------------------------------------------------------------------
//
// A point light
//
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePointLight, CDmePointLight );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmePointLight::OnConstruction()
{
	m_Position.InitAndSet( this, "position", Vector( 0, 0, 0 ), FATTRIB_HAS_CALLBACK );
	m_flAttenuation0.InitAndSet( this, "constantAttenuation", 1.0f, FATTRIB_HAS_CALLBACK );
	m_flAttenuation1.InitAndSet( this, "linearAttenuation", 0.0f, FATTRIB_HAS_CALLBACK );
	m_flAttenuation2.InitAndSet( this, "quadraticAttenuation", 0.0f, FATTRIB_HAS_CALLBACK );
	m_flMaxDistance.InitAndSet( this, "maxDistance", 0.0f, FATTRIB_HAS_CALLBACK );
}

void CDmePointLight::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets the attenuation factors
//-----------------------------------------------------------------------------
void CDmePointLight::SetAttenuation( float flConstant, float flLinear, float flQuadratic )
{
	m_flAttenuation0 = flConstant;
	m_flAttenuation1 = flLinear;
	m_flAttenuation2 = flQuadratic;
}

	
//-----------------------------------------------------------------------------
// Sets the maximum range
//-----------------------------------------------------------------------------
void CDmePointLight::SetMaxDistance( float flMaxDistance )
{
	m_flMaxDistance = flMaxDistance;
}


//-----------------------------------------------------------------------------
// Sets up render state in the material system for rendering
//-----------------------------------------------------------------------------
bool CDmePointLight::GetLightDesc( LightDesc_t *pDesc )
{
	memset( pDesc, 0, sizeof(LightDesc_t) );

	pDesc->m_Type = MATERIAL_LIGHT_POINT;
	SetupRenderStateInternal( *pDesc, m_flAttenuation0, m_flAttenuation1, m_flAttenuation2 );

	matrix3x4_t m;
	GetTransform()->GetTransform( m );
	VectorTransform( m_Position, m, pDesc->m_Position );
	pDesc->m_Direction.Init( 0, 0, 1 );
	pDesc->m_Range = m_flMaxDistance;

	pDesc->m_Theta = 0.0f;
	pDesc->m_Phi = 0.0f;
	pDesc->m_Falloff = 1.0f;

	return true;
}


//-----------------------------------------------------------------------------
//
// A spot light
//
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSpotLight, CDmeSpotLight );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeSpotLight::OnConstruction()
{
	m_Direction.InitAndSet( this, "direction", Vector( 0.0f, 0.0f, -1.0f ) );
	m_flSpotInnerAngle.InitAndSet( this, "spotInnerAngle", 60.0f );
	m_flSpotOuterAngle.InitAndSet( this, "spotOuterAngle", 90.0f );
	m_flSpotAngularFalloff.InitAndSet( this, "spotAngularFalloff", 1.0f );
}

void CDmeSpotLight::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets the light direction
//-----------------------------------------------------------------------------
void CDmeSpotLight::SetDirection( const Vector &direction )
{
	m_Direction = direction;
}


//-----------------------------------------------------------------------------
// Sets the spotlight angle factors
// Angles are specified in degrees, as full angles (as opposed to half-angles)
//-----------------------------------------------------------------------------
void CDmeSpotLight::SetAngles( float flInnerAngle, float flOuterAngle, float flAngularFalloff )
{
	m_flSpotInnerAngle = flInnerAngle;
	m_flSpotOuterAngle = flOuterAngle;
	m_flSpotAngularFalloff = flAngularFalloff;
}


//-----------------------------------------------------------------------------
// Sets up render state in the material system for rendering
//-----------------------------------------------------------------------------
bool CDmeSpotLight::GetLightDesc( LightDesc_t *pDesc )
{
	memset( pDesc, 0, sizeof(LightDesc_t) );

	pDesc->m_Type = MATERIAL_LIGHT_SPOT;
	SetupRenderStateInternal( *pDesc, m_flAttenuation0, m_flAttenuation1, m_flAttenuation2 );

	matrix3x4_t m;
	GetTransform()->GetTransform( m );
	VectorTransform( m_Position, m, pDesc->m_Position );
	VectorRotate( m_Direction.Get(), m, pDesc->m_Direction );
	VectorNormalize( pDesc->m_Direction );
	pDesc->m_Range = m_flMaxDistance;

	// Convert to radians
	pDesc->m_Theta = m_flSpotInnerAngle * M_PI / 180.0f;
	pDesc->m_Phi = m_flSpotOuterAngle * M_PI / 180.0f;
	pDesc->m_Falloff = m_flSpotAngularFalloff;

	return true;
}


//-----------------------------------------------------------------------------
//
// An ambient light
//
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAmbientLight, CDmeAmbientLight );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeAmbientLight::OnConstruction()
{
}

void CDmeAmbientLight::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets up render state in the material system for rendering
//-----------------------------------------------------------------------------
void CDmeAmbientLight::SetupRenderState( int nLightIndex )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	Vector4D cube[6];

	Vector4D vec4color( m_Color.Get().r(), m_Color.Get().g(), m_Color.Get().b(), m_Color.Get().a() );
	Vector4DMultiply( vec4color, m_flIntensity / 255.0f, cube[0] );
	cube[1] = cube[0];
	cube[2] = cube[0];
	cube[3] = cube[0];
	cube[4] = cube[0];
	cube[5] = cube[0];

	pRenderContext->SetAmbientLightCube( cube );
}

