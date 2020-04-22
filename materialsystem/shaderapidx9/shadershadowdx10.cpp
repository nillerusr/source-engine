//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "shadershadowdx10.h"
#include "utlvector.h"
#include "materialsystem/imaterialsystem.h"
#include "IHardwareConfigInternal.h"
#include "shadersystem.h"
#include "shaderapi/ishaderutil.h"
#include "materialsystem/imesh.h"
#include "tier0/dbg.h"
#include "materialsystem/idebugtextureinfo.h"


//-----------------------------------------------------------------------------
// Class Factory
//-----------------------------------------------------------------------------
static CShaderShadowDx10 s_ShaderShadow;
CShaderShadowDx10 *g_pShaderShadowDx10 = &s_ShaderShadow;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderShadowDx10, IShaderShadow, 
								  SHADERSHADOW_INTERFACE_VERSION, s_ShaderShadow )

//-----------------------------------------------------------------------------
// The shader shadow interface
//-----------------------------------------------------------------------------
CShaderShadowDx10::CShaderShadowDx10()
{
	m_IsTranslucent = false;
	m_IsAlphaTested = false;
	m_bIsDepthWriteEnabled = true;
	m_bUsesVertexAndPixelShaders = false;
}

CShaderShadowDx10::~CShaderShadowDx10()
{
}

// Sets the default *shadow* state
void CShaderShadowDx10::SetDefaultState()
{
	m_IsTranslucent = false;
	m_IsAlphaTested = false;
	m_bIsDepthWriteEnabled = true;
	m_bUsesVertexAndPixelShaders = false;
}

// Methods related to depth buffering
void CShaderShadowDx10::DepthFunc( ShaderDepthFunc_t depthFunc )
{
}

void CShaderShadowDx10::EnableDepthWrites( bool bEnable )
{
	m_bIsDepthWriteEnabled = bEnable;
}

void CShaderShadowDx10::EnableDepthTest( bool bEnable )
{
}

void CShaderShadowDx10::EnablePolyOffset( PolygonOffsetMode_t nOffsetMode )
{
}

// Suppresses/activates color writing 
void CShaderShadowDx10::EnableColorWrites( bool bEnable )
{
}

// Suppresses/activates alpha writing 
void CShaderShadowDx10::EnableAlphaWrites( bool bEnable )
{
}

// Methods related to alpha blending
void CShaderShadowDx10::EnableBlending( bool bEnable )
{
	m_IsTranslucent = bEnable;
}

void CShaderShadowDx10::BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
}

void CShaderShadowDx10::BlendOp( ShaderBlendOp_t blendOp )
{
}

void CShaderShadowDx10::BlendOpSeparateAlpha( ShaderBlendOp_t blendOp )
{
}

// A simpler method of dealing with alpha modulation
void CShaderShadowDx10::EnableAlphaPipe( bool bEnable )
{
}

void CShaderShadowDx10::EnableConstantAlpha( bool bEnable )
{
}

void CShaderShadowDx10::EnableVertexAlpha( bool bEnable )
{
}

void CShaderShadowDx10::EnableTextureAlpha( TextureStage_t stage, bool bEnable )
{
}

void CShaderShadowDx10::SetShadowDepthFiltering( Sampler_t stage )
{
}

// Alpha testing
void CShaderShadowDx10::EnableAlphaTest( bool bEnable )
{
	m_IsAlphaTested = bEnable;
}

void CShaderShadowDx10::AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ )
{
}

// Wireframe/filled polygons
void CShaderShadowDx10::PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode )
{
}


// Back face culling
void CShaderShadowDx10::EnableCulling( bool bEnable )
{
}

// Alpha to coverage
void CShaderShadowDx10::EnableAlphaToCoverage( bool bEnable )
{
}

// constant color + transparency
void CShaderShadowDx10::EnableConstantColor( bool bEnable )
{
}


// Indicates the vertex format for use with a vertex shader
// The flags to pass in here come from the VertexFormatFlags_t enum
// If pTexCoordDimensions is *not* specified, we assume all coordinates
// are 2-dimensional
void CShaderShadowDx10::VertexShaderVertexFormat( unsigned int flags, 
												  int numTexCoords, int* pTexCoordDimensions,
												  int userDataSize )
{
}

// Indicates we're going to light the model
void CShaderShadowDx10::EnableLighting( bool bEnable )
{
}

void CShaderShadowDx10::EnableSpecular( bool bEnable )
{
}

// Activate/deactivate skinning
void CShaderShadowDx10::EnableVertexBlend( bool bEnable )
{
}

// per texture unit stuff
void CShaderShadowDx10::OverbrightValue( TextureStage_t stage, float value )
{
}

void CShaderShadowDx10::EnableTexture( Sampler_t stage, bool bEnable )
{
}

void CShaderShadowDx10::EnableCustomPixelPipe( bool bEnable )
{
}

void CShaderShadowDx10::CustomTextureStages( int stageCount )
{
}

void CShaderShadowDx10::CustomTextureOperation( TextureStage_t stage, ShaderTexChannel_t channel, 
												ShaderTexOp_t op, ShaderTexArg_t arg1, ShaderTexArg_t arg2 )
{
}

void CShaderShadowDx10::EnableTexGen( TextureStage_t stage, bool bEnable )
{
}

void CShaderShadowDx10::TexGen( TextureStage_t stage, ShaderTexGenParam_t param )
{
}

// Sets the vertex and pixel shaders
void CShaderShadowDx10::SetVertexShader( const char *pShaderName, int vshIndex )
{
	m_bUsesVertexAndPixelShaders = ( pShaderName != NULL );
}

void CShaderShadowDx10::EnableBlendingSeparateAlpha( bool bEnable )
{
}
void CShaderShadowDx10::SetPixelShader( const char *pShaderName, int pshIndex )
{
	m_bUsesVertexAndPixelShaders = ( pShaderName != NULL );
}

void CShaderShadowDx10::BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
}
// indicates what per-vertex data we're providing
void CShaderShadowDx10::DrawFlags( unsigned int drawFlags )
{
}

