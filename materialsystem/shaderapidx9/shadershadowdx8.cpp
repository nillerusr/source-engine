//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#define DISABLE_PROTECTED_THINGS
#include "togl/rendermechanism.h"
#include "shadershadowdx8.h"
#include "locald3dtypes.h"
#include "utlvector.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapidx8_global.h"
#include "shaderapidx8.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialsystem.h"
#include "imeshdx8.h"
#include "materialsystem/materialsystem_config.h"
#include "vertexshaderdx8.h"

// NOTE: This must be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// The DX8 implementation of the shader setup interface
//-----------------------------------------------------------------------------
class CShaderShadowDX8 : public IShaderShadowDX8
{
public:
	// constructor, destructor
	CShaderShadowDX8( );
	virtual ~CShaderShadowDX8();

	// Initialize render state
	void Init( );

	// Sets the default state
	void SetDefaultState();

	// Methods related to depth buffering
	void DepthFunc( ShaderDepthFunc_t depthFunc );
	void EnableDepthWrites( bool bEnable );
	void EnableDepthTest( bool bEnable );
	void EnablePolyOffset( PolygonOffsetMode_t nOffsetMode );

	// Methods related to stencil. obsolete
	virtual void EnableStencil( bool bEnable )
	{
	}
	virtual void StencilFunc( ShaderStencilFunc_t stencilFunc )
	{
	}
	virtual void StencilPassOp( ShaderStencilOp_t stencilOp )
	{
	}
	virtual void StencilFailOp( ShaderStencilOp_t stencilOp )
	{
	}
	virtual void StencilDepthFailOp( ShaderStencilOp_t stencilOp )
	{
	}
	virtual void StencilReference( int nReference )
	{
	}
	virtual void StencilMask( int nMask )
	{
	}
	virtual void StencilWriteMask( int nMask )
	{
	}

	// Suppresses/activates color writing 
	void EnableColorWrites( bool bEnable );
	void EnableAlphaWrites( bool bEnable );

	// Methods related to alpha blending
	void EnableBlending( bool bEnable );

	void BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor );
	void BlendOp( ShaderBlendOp_t blendOp );
	void BlendOpSeparateAlpha( ShaderBlendOp_t blendOp );

	// Alpha testing
	void EnableAlphaTest( bool bEnable );
	void AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ );

	// Wireframe/filled polygons
	void PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode );

	// Back face culling
	void EnableCulling( bool bEnable );
	
	// constant color
	void EnableConstantColor( bool bEnable );

	// Indicates we're going to light the model
	void EnableLighting( bool bEnable );

	// Indicates specular lighting is going to be used
	void EnableSpecular( bool bEnable );

	// Convert from linear to gamma color space on writes to frame buffer.
	void EnableSRGBWrite( bool bEnable );

	// Convert from gamma to linear on texture fetch.
	void EnableSRGBRead( Sampler_t stage, bool bEnable );

	// Set up appropriate shadow filtering state (such as Fetch4 on ATI)
	void SetShadowDepthFiltering( Sampler_t stage );

	// Computes the vertex format
	virtual void VertexShaderVertexFormat( unsigned int nFlags, 
		int nTexCoordCount, int* pTexCoordDimensions, int nUserDataSize );

	// Pixel and vertex shader methods
	virtual void SetVertexShader( const char* pFileName, int nStaticVshIndex );
	virtual	void SetPixelShader( const char* pFileName, int nStaticPshIndex );

	// Indicates we're going to be using the ambient cube
	void EnableAmbientLightCubeOnStage0( bool bEnable );

	// Activate/deactivate skinning
	void EnableVertexBlend( bool bEnable );

	// per texture unit stuff
	void OverbrightValue( TextureStage_t stage, float value );
	void EnableTexture( Sampler_t stage, bool bEnable );
	void EnableTexGen( TextureStage_t stage, bool bEnable );
	void TexGen( TextureStage_t stage, ShaderTexGenParam_t param );
	void TextureCoordinate( TextureStage_t stage, int useCoord );

	// alternate method of specifying per-texture unit stuff, more flexible and more complicated
	// Can be used to specify different operation per channel (alpha/color)...
	void EnableCustomPixelPipe( bool bEnable );
	void CustomTextureStages( int stageCount );
	void CustomTextureOperation( TextureStage_t stage, ShaderTexChannel_t channel,
		ShaderTexOp_t op, ShaderTexArg_t arg1, ShaderTexArg_t arg2 );

	// A simpler method of dealing with alpha modulation
	void EnableAlphaPipe( bool bEnable );
	void EnableConstantAlpha( bool bEnable );
	void EnableVertexAlpha( bool bEnable );
	void EnableTextureAlpha( TextureStage_t stage, bool bEnable );

	// helper functions
	void EnableSphereMapping( TextureStage_t stage, bool bEnable );

	// Last call to be make before snapshotting
	void ComputeAggregateShadowState( );

	// Gets at the shadow state
	const ShadowState_t & GetShadowState();
	const ShadowShaderState_t & GetShadowShaderState();

	// GR - Separate alpha blending
	void EnableBlendingSeparateAlpha( bool bEnable );
	void BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor );

	void FogMode( ShaderFogMode_t fogMode );
	void DisableFogGammaCorrection( bool bDisable );

	void SetDiffuseMaterialSource( ShaderMaterialSource_t materialSource );
	virtual void SetMorphFormat( MorphFormat_t flags );

	// Alpha to coverage
	void EnableAlphaToCoverage( bool bEnable );

private:
	struct TextureStageState_t
	{
		int				m_TexCoordIndex;
		int				m_TexCoordinate;
		float			m_OverbrightVal;
		ShaderTexArg_t	m_Arg[2][2];
		ShaderTexOp_t	m_Op[2];
		unsigned char	m_TexGenEnable:1;
		unsigned char	m_TextureAlphaEnable:1;
	};

	struct SamplerState_t
	{
		bool m_TextureEnable : 1;
	};

	// Computes the blend factor
	D3DBLEND BlendFuncValue( ShaderBlendFactor_t factor ) const;

	// Computes the blend op
	D3DBLENDOP BlendOpValue( ShaderBlendOp_t blendOp ) const;

	// Configures the FVF vertex shader
	void ConfigureFVFVertexShader( unsigned int flags );
	void ConfigureCustomFVFVertexShader( unsigned int flags );

	// Configures our texture indices
	void ConfigureTextureCoordinates( unsigned int flags );

	// Returns a blend value based on overbrighting
	D3DTEXTUREOP OverbrightBlendValue( TextureStage_t stage );

	// Sets the desired color and alpha op state
	void DrawFlags( unsigned int flags );

	// Computes a vertex format for the draw flags
	VertexFormat_t FlagsToVertexFormat( int flags ) const;

	// Indicates we've got a constant color specified
	bool HasConstantColor() const;

	// Configures the alpha pipe
	void ConfigureAlphaPipe( unsigned int flags );

	// returns true if we're using texture coordinates at a given stage
	bool IsUsingTextureCoordinates( Sampler_t stage ) const;

	// Recomputes the tex coord index
	void RecomputeTexCoordIndex( TextureStage_t stage );


	// State needed to create the snapshots
	IMaterialSystemHardwareConfig* m_pHardwareConfig;
	
	// Separate alpha control?
	bool		m_AlphaPipe;

	// Constant color state
	bool		m_HasConstantColor;
	bool		m_HasConstantAlpha;

	// Vertex color state
	bool		m_HasVertexAlpha;

	// funky custom method of specifying shader state
	bool		m_CustomTextureStageState;

	// Number of stages used by the custom pipeline
	int			m_CustomTextureStages;

	// Number of bones...
	int			m_NumBlendVertices;

	// Draw flags
	int			m_DrawFlags;

	// Alpha blending...
	D3DBLEND	m_SrcBlend;
	D3DBLEND	m_DestBlend;
	D3DBLENDOP	m_BlendOp;

	// GR - Separate alpha blending...
	D3DBLEND	m_SrcBlendAlpha;
	D3DBLEND	m_DestBlendAlpha;
	D3DBLENDOP	m_BlendOpAlpha;

	// Alpha testing
	D3DCMPFUNC	m_AlphaFunc;
	int			m_AlphaRef;

	// Stencil
	D3DCMPFUNC	m_StencilFunc;
	int			m_StencilRef;
	int			m_StencilMask;
	DWORD		m_StencilFail;
	DWORD		m_StencilZFail;
	DWORD		m_StencilPass;
	int			m_StencilWriteMask;

	// The current shadow state
	ShadowState_t m_ShadowState;
	ShadowShaderState_t m_ShadowShaderState;

	// State info stores with each texture stage
	TextureStageState_t m_TextureStage[MAX_TEXTURE_STAGES];
	SamplerState_t m_SamplerState[MAX_SAMPLERS];
};


//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
static CShaderShadowDX8 g_ShaderShadow;
IShaderShadowDX8 *g_pShaderShadowDx8 = &g_ShaderShadow;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderShadowDX8, IShaderShadow, 
								  SHADERSHADOW_INTERFACE_VERSION, g_ShaderShadow )

//-----------------------------------------------------------------------------
// Global instance
//-----------------------------------------------------------------------------
IShaderShadowDX8* ShaderShadow()
{
	return &g_ShaderShadow;
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShaderShadowDX8::CShaderShadowDX8( ) :
	m_DrawFlags(0), m_pHardwareConfig(0), m_HasConstantColor(false)
{	
	memset( &m_ShadowState, 0, sizeof(m_ShadowState) );
	memset( &m_TextureStage, 0, sizeof(m_TextureStage) );
}

CShaderShadowDX8::~CShaderShadowDX8()
{
}


//-----------------------------------------------------------------------------
// Initialize render state
//-----------------------------------------------------------------------------
void CShaderShadowDX8::Init( )
{
	m_pHardwareConfig = HardwareConfig();
	
	// Clear out the shadow state
	memset( &m_ShadowState, 0, sizeof(m_ShadowState) );

	// No funky custom methods..
	m_CustomTextureStageState = false;

	// No constant color modulation
	m_HasConstantColor = false;
	m_HasConstantAlpha = false;
	m_HasVertexAlpha = false;

	m_ShadowShaderState.m_ModulateConstantColor = false;

	m_ShadowState.m_bDisableFogGammaCorrection = false;

	// By default we're using fixed function
	m_ShadowState.m_UsingFixedFunction = true;

	// Lighting off by default
	m_ShadowState.m_Lighting = false;

	// Pixel + vertex shaders
	m_ShadowShaderState.m_VertexShader = INVALID_SHADER;
	m_ShadowShaderState.m_PixelShader = INVALID_SHADER;
	m_ShadowShaderState.m_nStaticPshIndex = 0;
	m_ShadowShaderState.m_nStaticVshIndex = 0;
	m_ShadowShaderState.m_VertexUsage = 0;

	// Drawing nothing..
	m_DrawFlags = 0;

	// No alpha control
	m_AlphaPipe = false;

	// Vertex blending
	m_NumBlendVertices = 0;
	m_ShadowState.m_VertexBlendEnable = false;

	// NOTE: If you change these defaults, change the code in ComputeAggregateShadowState + CreateTransitionTableEntry
	int i;
	for (i = 0; i < MAX_TEXTURE_STAGES; ++i)
	{
		m_ShadowState.m_TextureStage[i].m_ColorOp	= D3DTOP_DISABLE;
		m_ShadowState.m_TextureStage[i].m_ColorArg1 = D3DTA_TEXTURE;
		m_ShadowState.m_TextureStage[i].m_ColorArg2 = (i == 0) ? D3DTA_DIFFUSE : D3DTA_CURRENT;
		m_ShadowState.m_TextureStage[i].m_AlphaOp	= D3DTOP_DISABLE;
		m_ShadowState.m_TextureStage[i].m_AlphaArg1 = D3DTA_TEXTURE;
		m_ShadowState.m_TextureStage[i].m_AlphaArg2 = (i == 0) ? D3DTA_DIFFUSE : D3DTA_CURRENT;
		m_ShadowState.m_TextureStage[i].m_TexCoordIndex = i;
	}

	for (i = 0; i < MAX_SAMPLERS; ++i)
	{
		m_ShadowState.m_SamplerState[i].m_TextureEnable = false;
		m_ShadowState.m_SamplerState[i].m_SRGBReadEnable = false;
		m_ShadowState.m_SamplerState[i].m_Fetch4Enable = false;
#ifdef DX_TO_GL_ABSTRACTION
		m_ShadowState.m_SamplerState[i].m_ShadowFilterEnable = false;
#endif
		// A *real* measure if the texture stage is being used.
		// we sometimes have to set the shadow state to not mirror this.
		m_SamplerState[i].m_TextureEnable = false;
	}
}


//-----------------------------------------------------------------------------
// Sets the default state
//-----------------------------------------------------------------------------
void CShaderShadowDX8::SetDefaultState()
{
	DepthFunc( SHADER_DEPTHFUNC_NEAREROREQUAL );
	EnableDepthWrites( true );
	EnableDepthTest( true );
	EnableColorWrites( true );
	EnableAlphaWrites( false );
	EnableAlphaTest( false );
	EnableLighting( false );
	EnableConstantColor( false );
	EnableBlending( false );
	BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ZERO );
	BlendOp( SHADER_BLEND_OP_ADD );
	// GR - separate alpha
	EnableBlendingSeparateAlpha( false );
	BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ZERO );
	BlendOpSeparateAlpha( SHADER_BLEND_OP_ADD );
	AlphaFunc( SHADER_ALPHAFUNC_GEQUAL, 0.7f );
	PolyMode( SHADER_POLYMODEFACE_FRONT_AND_BACK, SHADER_POLYMODE_FILL );
	EnableCulling( true );
	EnableAlphaToCoverage( false );
	EnablePolyOffset( SHADER_POLYOFFSET_DISABLE );
	EnableVertexBlend( false );
	EnableSpecular( false );
	EnableSRGBWrite( false );
	DrawFlags( SHADER_DRAW_POSITION );
	EnableCustomPixelPipe( false );
	CustomTextureStages( 0 );
	EnableAlphaPipe( false );
	EnableConstantAlpha( false );
	EnableVertexAlpha( false );
	SetVertexShader( NULL, 0 );
	SetPixelShader( NULL, 0 );
	FogMode( SHADER_FOGMODE_DISABLED );
	DisableFogGammaCorrection( false );
	SetDiffuseMaterialSource( SHADER_MATERIALSOURCE_MATERIAL );
	EnableStencil( false );
	StencilFunc( SHADER_STENCILFUNC_ALWAYS );
	StencilPassOp( SHADER_STENCILOP_KEEP );
	StencilFailOp( SHADER_STENCILOP_KEEP );
	StencilDepthFailOp( SHADER_STENCILOP_KEEP );
	StencilReference( 0 );
	StencilMask( 0xFFFFFFFF );
	StencilWriteMask( 0xFFFFFFFF );
	m_ShadowShaderState.m_VertexUsage = 0;

	int i;
	int nSamplerCount = HardwareConfig()->GetSamplerCount();
	for( i = 0; i < nSamplerCount; i++ )
	{
		EnableTexture( (Sampler_t)i, false );
		EnableSRGBRead( (Sampler_t)i, false );
	}

	int nTextureStageCount = HardwareConfig()->GetTextureStageCount();
	for( i = 0; i < nTextureStageCount; i++ )
	{
		EnableTexGen( (TextureStage_t)i, false );
		OverbrightValue( (TextureStage_t)i, 1.0f );
		EnableTextureAlpha( (TextureStage_t)i, false );
		CustomTextureOperation( (TextureStage_t)i, SHADER_TEXCHANNEL_COLOR, 
			SHADER_TEXOP_DISABLE, SHADER_TEXARG_TEXTURE, SHADER_TEXARG_PREVIOUSSTAGE );
		CustomTextureOperation( (TextureStage_t)i, SHADER_TEXCHANNEL_ALPHA, 
			SHADER_TEXOP_DISABLE, SHADER_TEXARG_TEXTURE, SHADER_TEXARG_PREVIOUSSTAGE );
	}
}


//-----------------------------------------------------------------------------
// Gets at the shadow state
//-----------------------------------------------------------------------------
const ShadowState_t &CShaderShadowDX8::GetShadowState()
{
	return m_ShadowState;
}

const ShadowShaderState_t &CShaderShadowDX8::GetShadowShaderState()
{
	return m_ShadowShaderState;
}


//-----------------------------------------------------------------------------
// Depth functions...
//-----------------------------------------------------------------------------
void CShaderShadowDX8::DepthFunc( ShaderDepthFunc_t depthFunc )
{
	D3DCMPFUNC zFunc;

	switch( depthFunc )
	{
	case SHADER_DEPTHFUNC_NEVER:
		zFunc = D3DCMP_NEVER;
		break;
	case SHADER_DEPTHFUNC_NEARER:
		zFunc = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? D3DCMP_GREATER : D3DCMP_LESS;
		break;
	case SHADER_DEPTHFUNC_EQUAL:
		zFunc = D3DCMP_EQUAL;
		break;
	case SHADER_DEPTHFUNC_NEAREROREQUAL:
		zFunc = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? D3DCMP_GREATEREQUAL : D3DCMP_LESSEQUAL;
		break;
	case SHADER_DEPTHFUNC_FARTHER:
		zFunc = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? D3DCMP_LESS : D3DCMP_GREATER;
		break;
	case SHADER_DEPTHFUNC_NOTEQUAL:
		zFunc = D3DCMP_NOTEQUAL;
		break;
	case SHADER_DEPTHFUNC_FARTHEROREQUAL:
		zFunc = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? D3DCMP_LESSEQUAL : D3DCMP_GREATEREQUAL;
		break;
	case SHADER_DEPTHFUNC_ALWAYS:
		zFunc = D3DCMP_ALWAYS;
		break;
	default:
		zFunc = D3DCMP_ALWAYS;
		Warning( "DepthFunc: invalid param\n" );
		break;
	}

	m_ShadowState.m_ZFunc = zFunc;
}

void CShaderShadowDX8::EnableDepthWrites( bool bEnable )
{
	m_ShadowState.m_ZWriteEnable = bEnable;
}

void CShaderShadowDX8::EnableDepthTest( bool bEnable )
{
	m_ShadowState.m_ZEnable = bEnable ? D3DZB_TRUE : D3DZB_FALSE;
}

void CShaderShadowDX8::EnablePolyOffset( PolygonOffsetMode_t nOffsetMode )
{
	m_ShadowState.m_ZBias = nOffsetMode;
}

//-----------------------------------------------------------------------------
// Color write state
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableColorWrites( bool bEnable )
{
	if (bEnable)
	{
		m_ShadowState.m_ColorWriteEnable |= D3DCOLORWRITEENABLE_BLUE |
							D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED;
	}
	else
	{
		m_ShadowState.m_ColorWriteEnable &= ~( D3DCOLORWRITEENABLE_BLUE |
							D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED );
	}
}

void CShaderShadowDX8::EnableAlphaWrites( bool bEnable )
{
	if (bEnable)
	{
		m_ShadowState.m_ColorWriteEnable |= D3DCOLORWRITEENABLE_ALPHA;
	}
	else
	{
		m_ShadowState.m_ColorWriteEnable &= ~D3DCOLORWRITEENABLE_ALPHA;
	}
}


//-----------------------------------------------------------------------------
// Alpha blending states
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableBlending( bool bEnable )
{
	m_ShadowState.m_AlphaBlendEnable = bEnable;
}

// GR - separate alpha
void CShaderShadowDX8::EnableBlendingSeparateAlpha( bool bEnable )
{
	m_ShadowState.m_SeparateAlphaBlendEnable = bEnable;
}

void CShaderShadowDX8::EnableAlphaTest( bool bEnable )
{
	m_ShadowState.m_AlphaTestEnable = bEnable;
}

void CShaderShadowDX8::AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ )
{
	D3DCMPFUNC d3dCmpFunc;

	switch( alphaFunc )
	{
	case SHADER_ALPHAFUNC_NEVER:
		d3dCmpFunc = D3DCMP_NEVER;
		break;
	case SHADER_ALPHAFUNC_LESS:
		d3dCmpFunc = D3DCMP_LESS;
		break;
	case SHADER_ALPHAFUNC_EQUAL:
		d3dCmpFunc = D3DCMP_EQUAL;
		break;
	case SHADER_ALPHAFUNC_LEQUAL:
		d3dCmpFunc = D3DCMP_LESSEQUAL;
		break;
	case SHADER_ALPHAFUNC_GREATER:
		d3dCmpFunc = D3DCMP_GREATER;
		break;
	case SHADER_ALPHAFUNC_NOTEQUAL:
		d3dCmpFunc = D3DCMP_NOTEQUAL;
		break;
	case SHADER_ALPHAFUNC_GEQUAL:
		d3dCmpFunc = D3DCMP_GREATEREQUAL;
		break;
	case SHADER_ALPHAFUNC_ALWAYS:
		d3dCmpFunc = D3DCMP_ALWAYS;
		break;
	default:
		Warning( "AlphaFunc: invalid param\n" );
		return;
	}

	m_AlphaFunc = d3dCmpFunc;
	m_AlphaRef = (int)(alphaRef * 255);
}

D3DBLEND CShaderShadowDX8::BlendFuncValue( ShaderBlendFactor_t factor ) const
{
	switch( factor )
	{
	case SHADER_BLEND_ZERO:
		return D3DBLEND_ZERO;

	case SHADER_BLEND_ONE:
		return D3DBLEND_ONE;

	case SHADER_BLEND_DST_COLOR:
		return D3DBLEND_DESTCOLOR;

	case SHADER_BLEND_ONE_MINUS_DST_COLOR:
		return D3DBLEND_INVDESTCOLOR;

	case SHADER_BLEND_SRC_ALPHA:
		return D3DBLEND_SRCALPHA;

	case SHADER_BLEND_ONE_MINUS_SRC_ALPHA:
		return D3DBLEND_INVSRCALPHA;

	case SHADER_BLEND_DST_ALPHA:
		return D3DBLEND_DESTALPHA;

	case SHADER_BLEND_ONE_MINUS_DST_ALPHA:
		return D3DBLEND_INVDESTALPHA;

	case SHADER_BLEND_SRC_ALPHA_SATURATE:
		return D3DBLEND_SRCALPHASAT;

	case SHADER_BLEND_SRC_COLOR:
		return D3DBLEND_SRCCOLOR;

	case SHADER_BLEND_ONE_MINUS_SRC_COLOR:
		return D3DBLEND_INVSRCCOLOR;
	}

	Warning( "BlendFunc: invalid factor\n" );
	return D3DBLEND_ONE;
}

D3DBLENDOP CShaderShadowDX8::BlendOpValue( ShaderBlendOp_t blendOp ) const
{
	switch( blendOp )
	{
	case SHADER_BLEND_OP_ADD:
		return D3DBLENDOP_ADD;

	case SHADER_BLEND_OP_SUBTRACT:
		return D3DBLENDOP_SUBTRACT;

	case SHADER_BLEND_OP_REVSUBTRACT:
		return D3DBLENDOP_REVSUBTRACT;

	case SHADER_BLEND_OP_MIN:
		return D3DBLENDOP_MIN;

	case SHADER_BLEND_OP_MAX:
		return D3DBLENDOP_MAX;
	}

	Warning( "BlendOp: invalid op\n" );
	return D3DBLENDOP_ADD;
}

void CShaderShadowDX8::BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
	D3DBLEND d3dSrcFactor = BlendFuncValue( srcFactor );
	D3DBLEND d3dDstFactor = BlendFuncValue( dstFactor );
	m_SrcBlend = d3dSrcFactor;
	m_DestBlend = d3dDstFactor;
}

// GR - separate alpha blend
void CShaderShadowDX8::BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
	D3DBLEND d3dSrcFactor = BlendFuncValue( srcFactor );
	D3DBLEND d3dDstFactor = BlendFuncValue( dstFactor );
	m_SrcBlendAlpha = d3dSrcFactor;
	m_DestBlendAlpha = d3dDstFactor;
}

void CShaderShadowDX8::BlendOp( ShaderBlendOp_t blendOp )
{
	m_BlendOp = BlendOpValue( blendOp );
}

void CShaderShadowDX8::BlendOpSeparateAlpha( ShaderBlendOp_t blendOp )
{
	m_BlendOpAlpha = BlendOpValue( blendOp );
}

//-----------------------------------------------------------------------------
// Polygon fill mode states
//-----------------------------------------------------------------------------
void CShaderShadowDX8::PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode )
{
	// DX8 can't handle different modes on front and back faces
// FIXME:	Assert( face == SHADER_POLYMODEFACE_FRONT_AND_BACK );
	if (face == SHADER_POLYMODEFACE_BACK)
		return;
	
	D3DFILLMODE fillMode;
	switch( polyMode )
	{
	case SHADER_POLYMODE_POINT:
		fillMode = D3DFILL_POINT;
		break;
	case SHADER_POLYMODE_LINE:
		fillMode =  D3DFILL_WIREFRAME;
		break;
	case SHADER_POLYMODE_FILL:
		fillMode =  D3DFILL_SOLID;
		break;
	default:
		Warning( "PolyMode: invalid poly mode\n" );
		return;
	}

	m_ShadowState.m_FillMode = fillMode;
}


//-----------------------------------------------------------------------------
// Backface cull states
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableCulling( bool bEnable )
{
	m_ShadowState.m_CullEnable = bEnable;
}


//-----------------------------------------------------------------------------
// Alpha to coverage
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableAlphaToCoverage( bool bEnable )
{
	m_ShadowState.m_EnableAlphaToCoverage = bEnable;
}


//-----------------------------------------------------------------------------
// Indicates we've got a constant color specified
//-----------------------------------------------------------------------------
bool CShaderShadowDX8::HasConstantColor() const
{
	return m_HasConstantColor;
}

void CShaderShadowDX8::EnableConstantColor( bool bEnable ) 
{
	m_HasConstantColor = bEnable;
}


//-----------------------------------------------------------------------------
// A simpler method of dealing with alpha modulation
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableAlphaPipe( bool bEnable )
{
	m_AlphaPipe = bEnable;
}

void CShaderShadowDX8::EnableConstantAlpha( bool bEnable )
{
	m_HasConstantAlpha = bEnable;
}

void CShaderShadowDX8::EnableVertexAlpha( bool bEnable )
{
	m_HasVertexAlpha = bEnable;
}

void CShaderShadowDX8::EnableTextureAlpha( TextureStage_t stage, bool bEnable )
{
	if ( stage < m_pHardwareConfig->GetSamplerCount() )
	{
		m_TextureStage[stage].m_TextureAlphaEnable = bEnable;
	}
}


//-----------------------------------------------------------------------------
// Indicates we're going to light the model
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableLighting( bool bEnable )
{
	m_ShadowState.m_Lighting = bEnable;
}


//-----------------------------------------------------------------------------
// Enables specular lighting (lighting has also got to be enabled)
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableSpecular( bool bEnable )
{
	m_ShadowState.m_SpecularEnable = bEnable;
}

//-----------------------------------------------------------------------------
// Enables auto-conversion from linear to gamma space on write to framebuffer.
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableSRGBWrite( bool bEnable )
{
	if ( m_pHardwareConfig->SupportsSRGB() )
	{
		m_ShadowState.m_SRGBWriteEnable = bEnable;
	}
	else
	{
		m_ShadowState.m_SRGBWriteEnable = false;
	}
}

//-----------------------------------------------------------------------------
// Activate/deactivate skinning
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableVertexBlend( bool bEnable )
{
	// Activate/deactivate skinning. Indexed blending is automatically
	// enabled if it's available for this hardware. When blending is enabled,
	// we allocate enough room for 3 weights (max allowed)
	if ((m_pHardwareConfig->MaxBlendMatrices() > 0) || (!bEnable))
	{
		m_ShadowState.m_VertexBlendEnable = bEnable;
	}
}

				 
//-----------------------------------------------------------------------------
// Texturemapping state
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableTexture( Sampler_t sampler, bool bEnable )
{
	if ( sampler < m_pHardwareConfig->GetSamplerCount() )
	{
		m_SamplerState[sampler].m_TextureEnable = bEnable;
	}
	else
	{
		Warning( "Attempting to bind a texture to an invalid sampler (%d)!\n", sampler );
	}
}

void CShaderShadowDX8::EnableSRGBRead( Sampler_t sampler, bool bEnable )
{
	if ( !m_pHardwareConfig->SupportsSRGB() )
	{
		m_ShadowState.m_SamplerState[sampler].m_SRGBReadEnable = false;
		return;
	}

	if ( sampler < m_pHardwareConfig->GetSamplerCount() )
	{
		m_ShadowState.m_SamplerState[sampler].m_SRGBReadEnable = bEnable;
	}
	else
	{
		Warning( "Attempting set SRGBRead state on an invalid sampler (%d)!\n", sampler );
	}
}

void CShaderShadowDX8::SetShadowDepthFiltering( Sampler_t stage )
{
#ifdef DX_TO_GL_ABSTRACTION
	if ( stage < m_pHardwareConfig->GetSamplerCount() )
	{
		m_ShadowState.m_SamplerState[stage].m_ShadowFilterEnable = true;
		return;
	}
#else
	if ( !m_pHardwareConfig->SupportsFetch4() )
	{
		m_ShadowState.m_SamplerState[stage].m_Fetch4Enable = false;
		return;
	}

	if ( stage < m_pHardwareConfig->GetSamplerCount() )
	{
		m_ShadowState.m_SamplerState[stage].m_Fetch4Enable = true;
		return;
	}
#endif
	
	Warning( "Attempting set shadow filtering state on an invalid sampler (%d)!\n", stage );
}


//-----------------------------------------------------------------------------
// Binds texture coordinates to a particular stage...
//-----------------------------------------------------------------------------
void CShaderShadowDX8::TextureCoordinate( TextureStage_t stage, int useTexCoord )
{
	if ( stage < m_pHardwareConfig->GetTextureStageCount() )
	{
		m_TextureStage[stage].m_TexCoordinate = useTexCoord;

		// Need to recompute the texCoordIndex, since that's affected by this
		RecomputeTexCoordIndex(stage);
	}
}


//-----------------------------------------------------------------------------
// Automatic texture coordinate generation
//-----------------------------------------------------------------------------
void CShaderShadowDX8::RecomputeTexCoordIndex( TextureStage_t stage )
{
	int texCoordIndex = m_TextureStage[stage].m_TexCoordinate;
	if (m_TextureStage[stage].m_TexGenEnable)
		texCoordIndex |= m_TextureStage[stage].m_TexCoordIndex;
	m_ShadowState.m_TextureStage[stage].m_TexCoordIndex = texCoordIndex;
}


//-----------------------------------------------------------------------------
// Automatic texture coordinate generation
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableTexGen( TextureStage_t stage, bool bEnable )
{
	if ( stage >= m_pHardwareConfig->GetTextureStageCount() )
	{
		Assert( 0 );
		return;
	}

	m_TextureStage[stage].m_TexGenEnable = bEnable;
	RecomputeTexCoordIndex(stage);
}

//-----------------------------------------------------------------------------
// Automatic texture coordinate generation
//-----------------------------------------------------------------------------
void CShaderShadowDX8::TexGen( TextureStage_t stage, ShaderTexGenParam_t param )
{
#ifdef FIXED_FUNCTION_PIPELINE
	if ( stage >= m_pHardwareConfig->GetTextureStageCount() )
		return;

	switch( param )
	{
	case SHADER_TEXGENPARAM_OBJECT_LINEAR:
		m_TextureStage[stage].m_TexCoordIndex = 0;	
		break;	
	case SHADER_TEXGENPARAM_EYE_LINEAR:
		m_TextureStage[stage].m_TexCoordIndex = D3DTSS_TCI_CAMERASPACEPOSITION;
		break;
	case SHADER_TEXGENPARAM_SPHERE_MAP:
		if ( m_pHardwareConfig->SupportsSpheremapping() )
		{
			m_TextureStage[stage].m_TexCoordIndex = D3DTSS_TCI_SPHEREMAP;
		}
		else
		{
			m_TextureStage[stage].m_TexCoordIndex = D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR;
		}
		break;
	case SHADER_TEXGENPARAM_CAMERASPACEREFLECTIONVECTOR:
		m_TextureStage[stage].m_TexCoordIndex = D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR;
		break;
	case SHADER_TEXGENPARAM_CAMERASPACENORMAL:
		m_TextureStage[stage].m_TexCoordIndex = D3DTSS_TCI_CAMERASPACENORMAL;
		break;
	}

	// Set the board state...
	RecomputeTexCoordIndex(stage);
#endif
}


//-----------------------------------------------------------------------------
// Overbrighting
//-----------------------------------------------------------------------------
void CShaderShadowDX8::OverbrightValue( TextureStage_t stage, float value )
{
	if ( m_pHardwareConfig->SupportsOverbright() && 
		( stage < m_pHardwareConfig->GetTextureStageCount() ) )
	{
		m_TextureStage[stage].m_OverbrightVal = value;
	}
}




//-----------------------------------------------------------------------------
// alternate method of specifying per-texture unit stuff, more flexible and more complicated
// Can be used to specify different operation per channel (alpha/color)...
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableCustomPixelPipe( bool bEnable )
{
	m_CustomTextureStageState = bEnable;
}

void CShaderShadowDX8::CustomTextureStages( int stageCount )
{
	m_CustomTextureStages = stageCount;
	Assert( stageCount <= m_pHardwareConfig->GetTextureStageCount() );
	if ( stageCount > m_pHardwareConfig->GetTextureStageCount() )
		stageCount = m_pHardwareConfig->GetTextureStageCount();
}

void CShaderShadowDX8::CustomTextureOperation( TextureStage_t stage, 
	ShaderTexChannel_t channel, ShaderTexOp_t op, ShaderTexArg_t arg1, ShaderTexArg_t arg2 )
{
	m_TextureStage[stage].m_Op[channel]= op;
	m_TextureStage[stage].m_Arg[channel][0] = arg1;
	m_TextureStage[stage].m_Arg[channel][1] = arg2;
}


//-----------------------------------------------------------------------------
// Compute the vertex format from vertex descriptor flags
//-----------------------------------------------------------------------------
void CShaderShadowDX8::VertexShaderVertexFormat( unsigned int nFlags, 
	int nTexCoordCount, int* pTexCoordDimensions, int nUserDataSize )
{
	// Code that creates a Mesh should specify whether it contains bone weights+indices, *not* the shader.
	Assert( ( nFlags & VERTEX_BONE_INDEX ) == 0 );
	nFlags &= ~VERTEX_BONE_INDEX;

	// This indicates we're using a vertex shader
	nFlags |= VERTEX_FORMAT_VERTEX_SHADER;
	m_ShadowShaderState.m_VertexUsage = MeshMgr()->ComputeVertexFormat( nFlags, nTexCoordCount, 
		pTexCoordDimensions, 0, nUserDataSize );
	m_ShadowState.m_UsingFixedFunction = false;

	// Avoid an error if vertex stream 0 is too narrow
	if ( CVertexBufferBase::VertexFormatSize( m_ShadowShaderState.m_VertexUsage ) <= 16 )
	{
		// FIXME: this is only necessary because we
		//          (a) put the flex normal/position stream in ALL vertex decls
		//          (b) bind stream 0's VB to stream 2 if there is no actual flex data
		//        ...it would be far more sensible to not add stream 2 to all vertex decls.
		static bool bComplained = false;
		if( !bComplained )
		{
			Warning( "ERROR: shader asking for a too-narrow vertex format - you will see errors if running with debug D3D DLLs!\n\tPadding the vertex format with extra texcoords\n\tWill not warn again.\n" );
			bComplained = true;
		}
		// All vertex formats should contain position...
		Assert( nFlags & VERTEX_POSITION );
		nFlags |= VERTEX_POSITION;
		// This error should occur only if we have zero texcoords, or if we have a single, 1-D texcoord
		Assert( ( nTexCoordCount == 0 ) ||
			    ( ( nTexCoordCount == 1 ) && pTexCoordDimensions && ( pTexCoordDimensions[0] == 1 ) ) );
		nTexCoordCount = 1;
		m_ShadowShaderState.m_VertexUsage = MeshMgr()->ComputeVertexFormat( nFlags, nTexCoordCount, NULL, 0, nUserDataSize );
	}
}


//-----------------------------------------------------------------------------
// Compute the vertex format from vertex descriptor flags
//-----------------------------------------------------------------------------
void CShaderShadowDX8::SetMorphFormat( MorphFormat_t flags )
{
	m_ShadowShaderState.m_MorphUsage = flags;
}

//-----------------------------------------------------------------------------
// Pixel and vertex shader methods
//-----------------------------------------------------------------------------
void CShaderShadowDX8::SetVertexShader( const char* pFileName, int nStaticVshIndex )
{
	char debugLabel[500] = "";
#ifdef DX_TO_GL_ABSTRACTION
	Q_snprintf( debugLabel, sizeof(debugLabel), "vs-file %s vs-index %d", pFileName, nStaticVshIndex ); 
#endif

	m_ShadowShaderState.m_VertexShader = ShaderManager()->CreateVertexShader( pFileName, nStaticVshIndex, debugLabel );
	m_ShadowShaderState.m_nStaticVshIndex = nStaticVshIndex;
}

void CShaderShadowDX8::SetPixelShader( const char* pFileName, int nStaticPshIndex )
{
	char debugLabel[500] = "";
#ifdef DX_TO_GL_ABSTRACTION
	Q_snprintf( debugLabel, sizeof(debugLabel), "ps-file %s ps-index %d", pFileName, nStaticPshIndex ); 
#endif

	m_ShadowShaderState.m_PixelShader = ShaderManager()->CreatePixelShader( pFileName, nStaticPshIndex, debugLabel );
	m_ShadowShaderState.m_nStaticPshIndex = nStaticPshIndex;
}


//-----------------------------------------------------------------------------
// NOTE: See Version 5 of this file for NVidia 8-stage shader stuff
//-----------------------------------------------------------------------------
inline bool CShaderShadowDX8::IsUsingTextureCoordinates( Sampler_t sampler ) const
{
	return m_SamplerState[sampler].m_TextureEnable;
}

inline D3DTEXTUREOP CShaderShadowDX8::OverbrightBlendValue( TextureStage_t stage )
{
	D3DTEXTUREOP colorop;
	if (m_TextureStage[stage].m_OverbrightVal < 2.0F)
		colorop = D3DTOP_MODULATE;
	else if (m_TextureStage[stage].m_OverbrightVal < 4.0F)
		colorop = D3DTOP_MODULATE2X;
	else
		colorop = D3DTOP_MODULATE4X;
	return colorop;
}

static inline int ComputeArg( ShaderTexArg_t arg )
{
	switch(arg)
	{
	case SHADER_TEXARG_TEXTURE:
		return D3DTA_TEXTURE;

	case SHADER_TEXARG_ZERO:
		return D3DTA_SPECULAR | D3DTA_COMPLEMENT;

	case SHADER_TEXARG_ONE:
		return D3DTA_SPECULAR;

	case SHADER_TEXARG_TEXTUREALPHA:
		return D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE;

	case SHADER_TEXARG_INVTEXTUREALPHA:
		return D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE | D3DTA_COMPLEMENT;

	case SHADER_TEXARG_NONE:
	case SHADER_TEXARG_VERTEXCOLOR:
		return D3DTA_DIFFUSE;

	case SHADER_TEXARG_SPECULARCOLOR:
		return D3DTA_SPECULAR;

	case SHADER_TEXARG_CONSTANTCOLOR:
		return D3DTA_TFACTOR;

	case SHADER_TEXARG_PREVIOUSSTAGE:
		return D3DTA_CURRENT;
	}

	Assert(0);
	return D3DTA_TEXTURE;
}

static inline D3DTEXTUREOP ComputeOp( ShaderTexOp_t op )
{
	switch(op)
	{
	case SHADER_TEXOP_MODULATE:
		return D3DTOP_MODULATE;

	case SHADER_TEXOP_MODULATE2X:
		return D3DTOP_MODULATE2X;

	case SHADER_TEXOP_MODULATE4X:
		return D3DTOP_MODULATE4X;

	case SHADER_TEXOP_SELECTARG1:
		return D3DTOP_SELECTARG1;

	case SHADER_TEXOP_SELECTARG2:
		return D3DTOP_SELECTARG2;

	case SHADER_TEXOP_ADD:
		return D3DTOP_ADD;

	case SHADER_TEXOP_SUBTRACT:
		return D3DTOP_SUBTRACT;

	case SHADER_TEXOP_ADDSIGNED2X:
		return D3DTOP_ADDSIGNED2X;

	case SHADER_TEXOP_BLEND_CONSTANTALPHA:
		return D3DTOP_BLENDFACTORALPHA;

	case SHADER_TEXOP_BLEND_PREVIOUSSTAGEALPHA:
		return D3DTOP_BLENDCURRENTALPHA;

	case SHADER_TEXOP_BLEND_TEXTUREALPHA:
		return D3DTOP_BLENDTEXTUREALPHA;
	
	case SHADER_TEXOP_MODULATECOLOR_ADDALPHA:
		return D3DTOP_MODULATECOLOR_ADDALPHA;

	case SHADER_TEXOP_MODULATEINVCOLOR_ADDALPHA:
		return D3DTOP_MODULATEINVCOLOR_ADDALPHA;

	case SHADER_TEXOP_DOTPRODUCT3:
		return D3DTOP_DOTPRODUCT3;

	case SHADER_TEXOP_DISABLE:
		return D3DTOP_DISABLE;
	}

	Assert(0);
	return D3DTOP_MODULATE;
}

void CShaderShadowDX8::ConfigureCustomFVFVertexShader( unsigned int flags )
{
	int i;
	for ( i = 0; i < m_CustomTextureStages; ++i)
	{
		m_ShadowState.m_TextureStage[i].m_ColorArg1 = ComputeArg( m_TextureStage[i].m_Arg[0][0] );
		m_ShadowState.m_TextureStage[i].m_ColorArg2 = ComputeArg( m_TextureStage[i].m_Arg[0][1] );								
		m_ShadowState.m_TextureStage[i].m_AlphaArg1 = ComputeArg( m_TextureStage[i].m_Arg[1][0] );
		m_ShadowState.m_TextureStage[i].m_AlphaArg2 = ComputeArg( m_TextureStage[i].m_Arg[1][1] );
 		m_ShadowState.m_TextureStage[i].m_ColorOp = ComputeOp( m_TextureStage[i].m_Op[0] );
		m_ShadowState.m_TextureStage[i].m_AlphaOp = ComputeOp( m_TextureStage[i].m_Op[1] );
	}

	// Deal with texture stage 1 -> n
	for ( i = m_CustomTextureStages; i < m_pHardwareConfig->GetTextureStageCount(); ++i )
	{
		m_ShadowState.m_TextureStage[i].m_ColorArg1 = D3DTA_TEXTURE;
		m_ShadowState.m_TextureStage[i].m_ColorArg2 = D3DTA_CURRENT;
		m_ShadowState.m_TextureStage[i].m_AlphaArg1 = D3DTA_TEXTURE;
		m_ShadowState.m_TextureStage[i].m_AlphaArg2 = D3DTA_CURRENT;
 		m_ShadowState.m_TextureStage[i].m_ColorOp = D3DTOP_DISABLE;
		m_ShadowState.m_TextureStage[i].m_AlphaOp = D3DTOP_DISABLE;
	}
}


//-----------------------------------------------------------------------------
// Sets up the alpha texture stage state
//-----------------------------------------------------------------------------
void CShaderShadowDX8::ConfigureAlphaPipe( unsigned int flags )
{
	// Are we using color?
	bool isUsingVertexAlpha = m_HasVertexAlpha && ((flags & SHADER_DRAW_COLOR) != 0);
	bool isUsingConstantAlpha = m_HasConstantAlpha;

	int lastTextureStage = m_pHardwareConfig->GetTextureStageCount() - 1;
	while ( lastTextureStage >= 0 )
	{
		if ( m_TextureStage[lastTextureStage].m_TextureAlphaEnable )
			break;
		--lastTextureStage;
	}

	for ( int i = 0; i < m_pHardwareConfig->GetTextureStageCount(); ++i )
	{
		m_ShadowState.m_TextureStage[i].m_AlphaOp = D3DTOP_MODULATE;
		if ( m_TextureStage[i].m_TextureAlphaEnable )
		{
			if (i == 0)
			{
				m_ShadowState.m_TextureStage[i].m_AlphaArg1 = D3DTA_TEXTURE;
				m_ShadowState.m_TextureStage[i].m_AlphaArg2 = 
					isUsingConstantAlpha ? D3DTA_TFACTOR : D3DTA_DIFFUSE;
				if (!isUsingConstantAlpha && !isUsingVertexAlpha)
					m_ShadowState.m_TextureStage[i].m_AlphaOp = D3DTOP_SELECTARG1;
				if (isUsingConstantAlpha)
					isUsingConstantAlpha = false;
				else if (isUsingVertexAlpha)
					isUsingVertexAlpha = false;
			}
			else
			{
				// Deal with texture stage 0
				m_ShadowState.m_TextureStage[i].m_AlphaArg1 = D3DTA_TEXTURE;
				m_ShadowState.m_TextureStage[i].m_AlphaArg2 = D3DTA_CURRENT;
			}
		}
		else
		{
			// Blat out unused stages
			if ((i > lastTextureStage) && !isUsingVertexAlpha && !isUsingConstantAlpha)
			{
				m_ShadowState.m_TextureStage[i].m_AlphaArg1 = D3DTA_TEXTURE;
				m_ShadowState.m_TextureStage[i].m_AlphaArg2 = D3DTA_CURRENT;
				m_ShadowState.m_TextureStage[i].m_AlphaOp = D3DTOP_DISABLE;
				continue;
			}

			// No texture coordinates; try to fold in vertex or constant alpha
			if (i == 0)
			{
				m_ShadowState.m_TextureStage[i].m_AlphaArg1 = D3DTA_TFACTOR;
				m_ShadowState.m_TextureStage[i].m_AlphaArg2 = D3DTA_DIFFUSE;
				if (isUsingVertexAlpha)
				{
					m_ShadowState.m_TextureStage[i].m_AlphaOp = 
						isUsingConstantAlpha ? D3DTOP_MODULATE : D3DTOP_SELECTARG2;
				}
				else
				{
					m_ShadowState.m_TextureStage[i].m_AlphaOp = D3DTOP_SELECTARG1;
				}
				isUsingVertexAlpha = false;
				isUsingConstantAlpha = false; 
			}
			else
			{
				m_ShadowState.m_TextureStage[i].m_AlphaArg1 = D3DTA_CURRENT;
				if (isUsingConstantAlpha)
				{
					m_ShadowState.m_TextureStage[i].m_AlphaArg2 = D3DTA_TFACTOR;
					isUsingConstantAlpha = false;
				}
				else if (isUsingVertexAlpha)
				{
					m_ShadowState.m_TextureStage[i].m_AlphaArg2 = D3DTA_DIFFUSE;
					isUsingVertexAlpha = false;
				}
				else
				{
					m_ShadowState.m_TextureStage[i].m_AlphaArg2 = D3DTA_DIFFUSE;
					m_ShadowState.m_TextureStage[i].m_AlphaOp = D3DTOP_SELECTARG1;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Sets up the texture stage state
//-----------------------------------------------------------------------------
void CShaderShadowDX8::ConfigureFVFVertexShader( unsigned int flags )
{
	// For non-modulation, we can't really use the path below...
	if (m_CustomTextureStageState)
	{
		ConfigureCustomFVFVertexShader( flags );
		return;
	}

	// Deal with texture stage 0
	m_ShadowState.m_TextureStage[0].m_ColorArg1 = D3DTA_TEXTURE;
	m_ShadowState.m_TextureStage[0].m_ColorArg2 = D3DTA_DIFFUSE;
	m_ShadowState.m_TextureStage[0].m_AlphaArg1 = D3DTA_TEXTURE;
	m_ShadowState.m_TextureStage[0].m_AlphaArg2 = D3DTA_DIFFUSE;

	// Are we using color?
	bool isUsingVertexColor = (flags & SHADER_DRAW_COLOR) != 0;
	bool isUsingConstantColor = (flags & SHADER_HAS_CONSTANT_COLOR) != 0;

	// Are we using texture coordinates?
	if ( IsUsingTextureCoordinates( SHADER_SAMPLER0 ) )
	{
		if (isUsingVertexColor)
		{
			m_ShadowState.m_TextureStage[0].m_ColorOp = OverbrightBlendValue(SHADER_TEXTURE_STAGE0);
			m_ShadowState.m_TextureStage[0].m_AlphaOp = D3DTOP_MODULATE;
		}
		else
		{
			// Just blend in the constant color here, and don't blend it in below
			m_ShadowState.m_TextureStage[0].m_ColorArg2 = D3DTA_TFACTOR;
			m_ShadowState.m_TextureStage[0].m_AlphaArg2 = D3DTA_TFACTOR;
			isUsingConstantColor = false;

			m_ShadowState.m_TextureStage[0].m_ColorOp = OverbrightBlendValue(SHADER_TEXTURE_STAGE0);
			m_ShadowState.m_TextureStage[0].m_AlphaOp = D3DTOP_MODULATE;
		}
	}
	else
	{
		// Are we using color?
		if (isUsingVertexColor)
		{
			// Color, but no texture
			if ( m_TextureStage[0].m_OverbrightVal < 2.0f )
			{
				// Use diffuse * constant color, if we have a constant color
				if (isUsingConstantColor)
				{
					m_ShadowState.m_TextureStage[0].m_ColorArg1 = D3DTA_TFACTOR;
					m_ShadowState.m_TextureStage[0].m_AlphaArg1 = D3DTA_TFACTOR;
					m_ShadowState.m_TextureStage[0].m_ColorOp = OverbrightBlendValue((TextureStage_t)0);
					m_ShadowState.m_TextureStage[0].m_AlphaOp = D3DTOP_MODULATE;

					// This'll make sure we don't apply the constant color again below
					isUsingConstantColor = false;
				}
				else
				{
					m_ShadowState.m_TextureStage[0].m_ColorOp = D3DTOP_SELECTARG2;
					m_ShadowState.m_TextureStage[0].m_AlphaOp = D3DTOP_SELECTARG2;
				}
			}
			else if (m_TextureStage[0].m_OverbrightVal < 4.0f)
			{
				// Produce diffuse + diffuse
				m_ShadowState.m_TextureStage[0].m_ColorArg1 = D3DTA_DIFFUSE;
				m_ShadowState.m_TextureStage[0].m_ColorOp = D3DTOP_ADD;
				m_ShadowState.m_TextureStage[0].m_AlphaOp = D3DTOP_SELECTARG2;
			}
			else
			{
				// no 4x overbright yet!
				Assert(0);
			}
		}
		else
		{
			// No texture, no color
			if (isUsingConstantColor)
			{
				m_ShadowState.m_TextureStage[0].m_ColorArg1 = D3DTA_TFACTOR;
				m_ShadowState.m_TextureStage[0].m_AlphaArg1 = D3DTA_TFACTOR;
				m_ShadowState.m_TextureStage[0].m_ColorOp = D3DTOP_SELECTARG1;
				m_ShadowState.m_TextureStage[0].m_AlphaOp = D3DTOP_SELECTARG1;

				// This'll make sure we don't apply the constant color again below
				isUsingConstantColor = false;
			}
			else
			{
				// Deal with texture stage 0
				m_ShadowState.m_TextureStage[0].m_ColorArg1 = D3DTA_TFACTOR;
				m_ShadowState.m_TextureStage[0].m_AlphaArg1 = D3DTA_TFACTOR;
				m_ShadowState.m_TextureStage[0].m_ColorOp = D3DTOP_SELECTARG1;
				m_ShadowState.m_TextureStage[0].m_AlphaOp = D3DTOP_SELECTARG1;
			}
		}
	}

	// Deal with texture stage 1 -> n
	int lastUsedTextureStage = 0;
	for ( int i = 1; i < m_pHardwareConfig->GetTextureStageCount(); ++i )
	{
		m_ShadowState.m_TextureStage[i].m_ColorArg1 = D3DTA_TEXTURE;
		m_ShadowState.m_TextureStage[i].m_ColorArg2 = D3DTA_CURRENT;
		m_ShadowState.m_TextureStage[i].m_AlphaArg1 = D3DTA_TEXTURE;
		m_ShadowState.m_TextureStage[i].m_AlphaArg2 = D3DTA_CURRENT;

		// Not doing anything? Disable the stage
		if ( !IsUsingTextureCoordinates( (Sampler_t)i ) )
		{
			if (m_TextureStage[i].m_OverbrightVal < 2.0f)
			{
				m_ShadowState.m_TextureStage[i].m_ColorOp = D3DTOP_DISABLE;
				m_ShadowState.m_TextureStage[i].m_AlphaOp = D3DTOP_DISABLE;
			}
			else
			{
				// Here, we're modulating. Add in the constant color if we need to...
				m_ShadowState.m_TextureStage[i].m_ColorArg1 = D3DTA_TFACTOR;
				m_ShadowState.m_TextureStage[i].m_AlphaArg1 = D3DTA_TFACTOR;

				m_ShadowState.m_TextureStage[i].m_ColorOp = OverbrightBlendValue((TextureStage_t)i);
				m_ShadowState.m_TextureStage[i].m_AlphaOp = D3DTOP_MODULATE;

				isUsingConstantColor = false;
				lastUsedTextureStage = i;
			}
		}
		else							  
		{
			// Here, we're modulating. Keep track of the last modulation stage,
			// cause the constant color modulation comes in the stage after that
			lastUsedTextureStage = i;
			m_ShadowState.m_TextureStage[i].m_ColorOp = OverbrightBlendValue((TextureStage_t)i);
			m_ShadowState.m_TextureStage[i].m_AlphaOp = D3DTOP_MODULATE;
		}
	}

	// massive amounts of suck: gotta overbright here if we really
	// wanted to overbright stage0 but couldn't because of the add.
	// This isn't totally correct, but there's no way around putting it here
	// because we can't texture out of stage2 on low or medium end hardware
	m_ShadowShaderState.m_ModulateConstantColor = false;
	if (isUsingConstantColor)
	{
		++lastUsedTextureStage;

		if (isUsingConstantColor &&
			(lastUsedTextureStage >= m_pHardwareConfig->GetTextureStageCount()))
		{
			// This is the case where we'd want to modulate in a particular texture
			// stage, but we can't because there aren't enough. In this case, we're gonna
			// need to do the modulation in the per-vertex color.
			m_ShadowShaderState.m_ModulateConstantColor = true;
		}
		else
		{
			AssertOnce (lastUsedTextureStage < 2);
				
			// Here, we've got enough texture stages to do the modulation
			m_ShadowState.m_TextureStage[lastUsedTextureStage].m_ColorArg1 = D3DTA_TFACTOR;
			m_ShadowState.m_TextureStage[lastUsedTextureStage].m_ColorArg2 = D3DTA_CURRENT;
			m_ShadowState.m_TextureStage[lastUsedTextureStage].m_AlphaArg1 = D3DTA_TFACTOR;
			m_ShadowState.m_TextureStage[lastUsedTextureStage].m_AlphaArg2 = D3DTA_CURRENT;
			m_ShadowState.m_TextureStage[lastUsedTextureStage].m_ColorOp = D3DTOP_MODULATE;
			m_ShadowState.m_TextureStage[lastUsedTextureStage].m_AlphaOp = D3DTOP_MODULATE;
		}
	}

	// Overwrite the alpha stuff if we asked to independently control it
	if (m_AlphaPipe)
	{
		ConfigureAlphaPipe( flags );
	}
}


//-----------------------------------------------------------------------------
// Makes sure we report if we're getting garbage.
//-----------------------------------------------------------------------------
void CShaderShadowDX8::DrawFlags( unsigned int flags )
{
	m_DrawFlags = flags;
	m_ShadowState.m_UsingFixedFunction = true;
}


//-----------------------------------------------------------------------------
// Compute texture coordinates
//-----------------------------------------------------------------------------
void CShaderShadowDX8::ConfigureTextureCoordinates( unsigned int flags )
{
	// default...
	for (int i = 0; i < m_pHardwareConfig->GetTextureStageCount(); ++i)
	{
		TextureCoordinate( (TextureStage_t)i, i );
	}

	if (flags & SHADER_DRAW_TEXCOORD0)
	{
		Assert( (flags & SHADER_DRAW_LIGHTMAP_TEXCOORD0) == 0 );
		TextureCoordinate( SHADER_TEXTURE_STAGE0, 0 );
	}
	else if (flags & SHADER_DRAW_LIGHTMAP_TEXCOORD0)
	{
		TextureCoordinate( SHADER_TEXTURE_STAGE0, 1 );
	}
	else if (flags & SHADER_DRAW_SECONDARY_TEXCOORD0 ) 
	{
		TextureCoordinate( SHADER_TEXTURE_STAGE0, 2 );
	}

	if (flags & SHADER_DRAW_TEXCOORD1)
	{
		Assert( (flags & SHADER_DRAW_LIGHTMAP_TEXCOORD1) == 0 );
		TextureCoordinate( SHADER_TEXTURE_STAGE1, 0 );
	}
	else if (flags & SHADER_DRAW_LIGHTMAP_TEXCOORD1)
	{
		TextureCoordinate( SHADER_TEXTURE_STAGE1, 1 );
	}
	else if (flags & SHADER_DRAW_SECONDARY_TEXCOORD1 ) 
	{
		TextureCoordinate( SHADER_TEXTURE_STAGE1, 2 );
	}
}


//-----------------------------------------------------------------------------
// Converts draw flags into vertex format
//-----------------------------------------------------------------------------
VertexFormat_t CShaderShadowDX8::FlagsToVertexFormat( int flags ) const
{
	// Flags -1 occurs when there's an error condition;
	// we'll just give em the max space and let them fill it in.
	int formatFlags = 0;
	int texCoordSize[VERTEX_MAX_TEXTURE_COORDINATES] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	int userDataSize = 0;
	int numBones = 0;

	// Flags -1 occurs when there's an error condition;
	// we'll just give em the max space and let them fill it in.
	if (flags == -1)
	{
		formatFlags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_COLOR |
			VERTEX_TANGENT_S | VERTEX_TANGENT_T;
		texCoordSize[0] = texCoordSize[1] = texCoordSize[2] = 2;
	}
	else
	{
		if (flags & SHADER_DRAW_POSITION)
			formatFlags |= VERTEX_POSITION;

		if (flags & SHADER_DRAW_NORMAL)
			formatFlags |= VERTEX_NORMAL;

		if (flags & SHADER_DRAW_COLOR)
			formatFlags |= VERTEX_COLOR;
		
		if( flags & SHADER_DRAW_SPECULAR )
			formatFlags |= VERTEX_SPECULAR;

		if (flags & SHADER_TEXCOORD_MASK)
		{
			// normal texture coords into texture 0
			texCoordSize[0] = 2;
		}

		if (flags & SHADER_LIGHTMAP_TEXCOORD_MASK)
		{
			// lightmaps go into texcoord 1
			texCoordSize[1] = 2;
		}

		if (flags & SHADER_SECONDARY_TEXCOORD_MASK)
		{
			// any texgen, or secondary texture coordinate is put into texcoord 2
			texCoordSize[2] = 2;
		}
	}

	// Hardware skinning...	always store space for up to 3 bones
	// and always assume index blend enabled if available
	if (m_ShadowState.m_VertexBlendEnable)
	{
		if (HardwareConfig()->MaxBlendMatrixIndices() > 0)
			formatFlags |= VERTEX_BONE_INDEX;

		if (HardwareConfig()->MaxBlendMatrices() > 2)
			numBones = 2;	// the third bone weight is implied
		else
			numBones = HardwareConfig()->MaxBlendMatrices() - 1;
	}

	return MeshMgr()->ComputeVertexFormat( formatFlags, VERTEX_MAX_TEXTURE_COORDINATES, 
		texCoordSize, numBones, userDataSize );
}


//-----------------------------------------------------------------------------
// Computes shadow state based on bunches of other parameters
//-----------------------------------------------------------------------------
void CShaderShadowDX8::ComputeAggregateShadowState( )
{
	unsigned int flags = 0;

	// Initialize the texture stage usage; this may get changed later
	for (int i = 0; i < m_pHardwareConfig->GetSamplerCount(); ++i)
	{
		m_ShadowState.m_SamplerState[i].m_TextureEnable = 
			IsUsingTextureCoordinates( (Sampler_t)i );

		// Deal with the alpha pipe
		if ( m_ShadowState.m_UsingFixedFunction && m_AlphaPipe )
		{
			if ( m_TextureStage[i].m_TextureAlphaEnable )
			{
				m_ShadowState.m_SamplerState[i].m_TextureEnable = true;
			}
		}
	}

	// Always use the same alpha src + dest if it's disabled
	// NOTE: This is essential for stateblocks to work
	if ( m_ShadowState.m_AlphaBlendEnable )
	{
		m_ShadowState.m_SrcBlend = m_SrcBlend;
		m_ShadowState.m_DestBlend = m_DestBlend;
		m_ShadowState.m_BlendOp = m_BlendOp;
	}
	else
	{
		m_ShadowState.m_SrcBlend = D3DBLEND_ONE;
		m_ShadowState.m_DestBlend = D3DBLEND_ZERO;
		m_ShadowState.m_BlendOp = D3DBLENDOP_ADD;
	}

	// GR
	if (m_ShadowState.m_SeparateAlphaBlendEnable)
	{
		m_ShadowState.m_SrcBlendAlpha = m_SrcBlendAlpha;
		m_ShadowState.m_DestBlendAlpha = m_DestBlendAlpha;
		m_ShadowState.m_BlendOpAlpha = m_BlendOpAlpha;
	}
	else
	{
		m_ShadowState.m_SrcBlendAlpha = D3DBLEND_ONE;
		m_ShadowState.m_DestBlendAlpha = D3DBLEND_ZERO;
		m_ShadowState.m_BlendOpAlpha = D3DBLENDOP_ADD;
	}

	// Use the same func if it's disabled
	if (m_ShadowState.m_AlphaTestEnable)
	{
		// If alpha test is enabled, just use the values set
		m_ShadowState.m_AlphaFunc = m_AlphaFunc;
		m_ShadowState.m_AlphaRef = m_AlphaRef;
	}
	else
	{
		// A default value
		m_ShadowState.m_AlphaFunc = D3DCMP_GREATEREQUAL;
		m_ShadowState.m_AlphaRef = 0;

		// If not alpha testing and doing a standard alpha blend, force on alpha testing
		if ( m_ShadowState.m_AlphaBlendEnable )
		{
			if ( ( m_ShadowState.m_SrcBlend == D3DBLEND_SRCALPHA ) && ( m_ShadowState.m_DestBlend == D3DBLEND_INVSRCALPHA ) )
			{
				m_ShadowState.m_AlphaFunc = D3DCMP_GREATEREQUAL;
				m_ShadowState.m_AlphaRef = 1;
			}
		}
	}
	if ( m_ShadowState.m_UsingFixedFunction )
	{
		flags = m_DrawFlags;

		// We need to take this bad boy into account
		if (HasConstantColor())
			flags |= SHADER_HAS_CONSTANT_COLOR;

		// We need to take lighting into account..
		if ( m_ShadowState.m_Lighting )
			flags |= SHADER_DRAW_NORMAL;

		if (m_ShadowState.m_Lighting)
			flags |= SHADER_DRAW_COLOR;

		// Look for inconsistency in the shadow state (can't have texgen &
		// SHADER_DRAW_TEXCOORD or SHADER_DRAW_SECONDARY_TEXCOORD0 on the same stage)
		if (flags & (SHADER_DRAW_TEXCOORD0 | SHADER_DRAW_SECONDARY_TEXCOORD0))
		{
			Assert( (m_ShadowState.m_TextureStage[0].m_TexCoordIndex & 0xFFFF0000) == 0 );
		}
		if (flags & (SHADER_DRAW_TEXCOORD1 | SHADER_DRAW_SECONDARY_TEXCOORD1))
		{
			Assert( (m_ShadowState.m_TextureStage[1].m_TexCoordIndex & 0xFFFF0000) == 0 );
		}			
		if (flags & (SHADER_DRAW_TEXCOORD2 | SHADER_DRAW_SECONDARY_TEXCOORD2))
		{
			Assert( (m_ShadowState.m_TextureStage[2].m_TexCoordIndex & 0xFFFF0000) == 0 );
		}
		if (flags & (SHADER_DRAW_TEXCOORD3 | SHADER_DRAW_SECONDARY_TEXCOORD3))
		{
			Assert( (m_ShadowState.m_TextureStage[3].m_TexCoordIndex & 0xFFFF0000) == 0 );
		}

		// Vertex usage has already been set for pixel + vertex shaders
		m_ShadowShaderState.m_VertexUsage = FlagsToVertexFormat( flags );

		// Configure the texture stages
		ConfigureFVFVertexShader(flags);

#if 0
//#ifdef _DEBUG
		// NOTE: This must be true for stateblocks to work
		for ( i = 0; i < m_pHardwareConfig->GetTextureStageCount(); ++i )
		{
			if ( m_ShadowState.m_TextureStage[i].m_ColorOp == D3DTOP_DISABLE )
			{
				Assert( m_ShadowState.m_TextureStage[i].m_ColorArg1 == D3DTA_TEXTURE );
 				Assert( m_ShadowState.m_TextureStage[i].m_ColorArg2 == ((i == 0) ? D3DTA_DIFFUSE : D3DTA_CURRENT) );
			}

			if ( m_ShadowState.m_TextureStage[i].m_AlphaOp == D3DTOP_DISABLE )
			{
				Assert( m_ShadowState.m_TextureStage[i].m_AlphaArg1 == D3DTA_TEXTURE );
 				Assert( m_ShadowState.m_TextureStage[i].m_AlphaArg2 == ((i == 0) ? D3DTA_DIFFUSE : D3DTA_CURRENT) );
			}
		}
#endif
	}
	else
	{
		// Pixel shaders, disable everything so as to prevent unnecessary state changes....
		for ( int i = 0; i < m_pHardwareConfig->GetTextureStageCount(); ++i )
		{
			m_ShadowState.m_TextureStage[i].m_ColorArg1 = D3DTA_TEXTURE;
			m_ShadowState.m_TextureStage[i].m_ColorArg2 = (i == 0) ? D3DTA_DIFFUSE : D3DTA_CURRENT;
			m_ShadowState.m_TextureStage[i].m_AlphaArg1 = D3DTA_TEXTURE;
			m_ShadowState.m_TextureStage[i].m_AlphaArg2 = (i == 0) ? D3DTA_DIFFUSE : D3DTA_CURRENT;
 			m_ShadowState.m_TextureStage[i].m_ColorOp = D3DTOP_DISABLE;
			m_ShadowState.m_TextureStage[i].m_AlphaOp = D3DTOP_DISABLE;
			m_ShadowState.m_TextureStage[i].m_TexCoordIndex = i;
		}
		m_ShadowState.m_Lighting = false;
		m_ShadowState.m_SpecularEnable = false;
		m_ShadowState.m_VertexBlendEnable = false;
		m_ShadowShaderState.m_ModulateConstantColor = false;
	}

	// Compute texture coordinates
	ConfigureTextureCoordinates(flags);

	// Alpha to coverage
	if ( m_ShadowState.m_EnableAlphaToCoverage )
	{
		// Only allow this to be enabled if blending is disabled and testing is enabled
		if ( ( m_ShadowState.m_AlphaBlendEnable == true ) || ( m_ShadowState.m_AlphaTestEnable == false ) )
		{
			m_ShadowState.m_EnableAlphaToCoverage = false;
		}
	}
}

void CShaderShadowDX8::FogMode( ShaderFogMode_t fogMode )
{
	Assert( fogMode >= 0 && fogMode < SHADER_FOGMODE_NUMFOGMODES );
	m_ShadowState.m_FogMode = fogMode;
}

void CShaderShadowDX8::DisableFogGammaCorrection( bool bDisable )
{
	m_ShadowState.m_bDisableFogGammaCorrection = bDisable;
}

void CShaderShadowDX8::SetDiffuseMaterialSource( ShaderMaterialSource_t materialSource )
{
	COMPILE_TIME_ASSERT( ( int )D3DMCS_MATERIAL == ( int )SHADER_MATERIALSOURCE_MATERIAL );
	COMPILE_TIME_ASSERT( ( int )D3DMCS_COLOR1 == ( int )SHADER_MATERIALSOURCE_COLOR1 );
	COMPILE_TIME_ASSERT( ( int )D3DMCS_COLOR2 == ( int )SHADER_MATERIALSOURCE_COLOR2 );
	Assert( materialSource == SHADER_MATERIALSOURCE_MATERIAL ||
			materialSource == SHADER_MATERIALSOURCE_COLOR1 ||
			materialSource == SHADER_MATERIALSOURCE_COLOR2 );
	m_ShadowState.m_DiffuseMaterialSource = ( D3DMATERIALCOLORSOURCE )materialSource;
}
