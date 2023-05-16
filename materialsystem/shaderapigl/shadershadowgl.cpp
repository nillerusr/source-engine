#include "utlvector.h"
#include "materialsystem/imaterialsystem.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/imesh.h"
#include "materialsystem/idebugtextureinfo.h"
#include "materialsystem/deformations.h"
#include "meshgl.h"
#include "shaderapigl.h"
#include "shaderapidevicegl.h"
#include "shadershadowgl.h"


//-----------------------------------------------------------------------------
// The shader shadow interface
//-----------------------------------------------------------------------------
CShaderShadowGL::CShaderShadowGL()
{
	m_IsTranslucent = false;
	m_IsAlphaTested = false;
	m_bIsDepthWriteEnabled = true;
	m_bUsesVertexAndPixelShaders = false;
}

CShaderShadowGL::~CShaderShadowGL()
{
}

// Sets the default *shadow* state
void CShaderShadowGL::SetDefaultState()
{
	m_IsTranslucent = false;
	m_IsAlphaTested = false;
	m_bIsDepthWriteEnabled = true;
	m_bUsesVertexAndPixelShaders = false;
}

// Methods related to depth buffering
void CShaderShadowGL::DepthFunc( ShaderDepthFunc_t depthFunc )
{
}

void CShaderShadowGL::EnableDepthWrites( bool bEnable )
{
	m_bIsDepthWriteEnabled = bEnable;
}

void CShaderShadowGL::EnableDepthTest( bool bEnable )
{
}

void CShaderShadowGL::EnablePolyOffset( PolygonOffsetMode_t nOffsetMode )
{
}

// Suppresses/activates color writing 
void CShaderShadowGL::EnableColorWrites( bool bEnable )
{
}

// Suppresses/activates alpha writing 
void CShaderShadowGL::EnableAlphaWrites( bool bEnable )
{
}

// Methods related to alpha blending
void CShaderShadowGL::EnableBlending( bool bEnable )
{
	m_IsTranslucent = bEnable;
}

void CShaderShadowGL::BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
}

// A simpler method of dealing with alpha modulation
void CShaderShadowGL::EnableAlphaPipe( bool bEnable )
{
}

void CShaderShadowGL::EnableConstantAlpha( bool bEnable )
{
}

void CShaderShadowGL::EnableVertexAlpha( bool bEnable )
{
}

void CShaderShadowGL::EnableTextureAlpha( TextureStage_t stage, bool bEnable )
{
}


// Alpha testing
void CShaderShadowGL::EnableAlphaTest( bool bEnable )
{
	m_IsAlphaTested = bEnable;
}

void CShaderShadowGL::AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ )
{
}


// Wireframe/filled polygons
void CShaderShadowGL::PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode )
{
}


// Back face culling
void CShaderShadowGL::EnableCulling( bool bEnable )
{
}


// Alpha to coverage
void CShaderShadowGL::EnableAlphaToCoverage( bool bEnable )
{
}


// constant color + transparency
void CShaderShadowGL::EnableConstantColor( bool bEnable )
{
}

// Indicates the vertex format for use with a vertex shader
// The flags to pass in here come from the VertexFormatFlags_t enum
// If pTexCoordDimensions is *not* specified, we assume all coordinates
// are 2-dimensional
void CShaderShadowGL::VertexShaderVertexFormat( unsigned int nFlags, 
												   int nTexCoordCount,
												   int* pTexCoordDimensions,
												   int nUserDataSize )
{
}

// Indicates we're going to light the model
void CShaderShadowGL::EnableLighting( bool bEnable )
{
}

void CShaderShadowGL::EnableSpecular( bool bEnable )
{
}

// Activate/deactivate skinning
void CShaderShadowGL::EnableVertexBlend( bool bEnable )
{
}

// per texture unit stuff
void CShaderShadowGL::OverbrightValue( TextureStage_t stage, float value )
{
}

void CShaderShadowGL::EnableTexture( Sampler_t stage, bool bEnable )
{
}

void CShaderShadowGL::EnableCustomPixelPipe( bool bEnable )
{
}

void CShaderShadowGL::CustomTextureStages( int stageCount )
{
}

void CShaderShadowGL::CustomTextureOperation( TextureStage_t stage, ShaderTexChannel_t channel, 
	ShaderTexOp_t op, ShaderTexArg_t arg1, ShaderTexArg_t arg2 )
{
}

void CShaderShadowGL::EnableTexGen( TextureStage_t stage, bool bEnable )
{
}

void CShaderShadowGL::TexGen( TextureStage_t stage, ShaderTexGenParam_t param )
{
}

// Sets the vertex and pixel shaders
void CShaderShadowGL::SetVertexShader( const char *pShaderName, int vshIndex )
{
	m_bUsesVertexAndPixelShaders = ( pShaderName != NULL );
}

void CShaderShadowGL::EnableBlendingSeparateAlpha( bool bEnable )
{
}
void CShaderShadowGL::SetPixelShader( const char *pShaderName, int pshIndex )
{
	m_bUsesVertexAndPixelShaders = ( pShaderName != NULL );
}

void CShaderShadowGL::BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
}
// indicates what per-vertex data we're providing
void CShaderShadowGL::DrawFlags( unsigned int drawFlags )
{
}


