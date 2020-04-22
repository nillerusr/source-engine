//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERSHADOWDX8_H
#define SHADERSHADOWDX8_H

#ifdef _WIN32
#pragma once
#endif

#include "togl/rendermechanism.h"
#include "locald3dtypes.h"
#include "shaderapi/ishadershadow.h"

class IShaderAPIDX8;


//-----------------------------------------------------------------------------
// Important enumerations
//-----------------------------------------------------------------------------
enum
{
	MAX_SAMPLERS = 16,
	MAX_TEXTURE_STAGES = 16,
};


//-----------------------------------------------------------------------------
// A structure maintaining the shadowed board state
//-----------------------------------------------------------------------------
struct TextureStageShadowState_t
{
	// State shadowing affects these
	D3DTEXTUREOP	m_ColorOp;
	int				m_ColorArg1;
	int				m_ColorArg2;
	D3DTEXTUREOP	m_AlphaOp;
	int				m_AlphaArg1;
	int				m_AlphaArg2;
	int				m_TexCoordIndex;
};

struct SamplerShadowState_t
{
	bool	m_TextureEnable : 1;
	bool	m_SRGBReadEnable : 1;
	bool	m_Fetch4Enable : 1;
	bool	m_ShadowFilterEnable : 1;
};

struct ShadowState_t
{
	// Depth buffering state
	D3DCMPFUNC		m_ZFunc;
	D3DZBUFFERTYPE	m_ZEnable;

	// Write enable
	DWORD			m_ColorWriteEnable;

	// Fill mode
	D3DFILLMODE		m_FillMode;

	// Alpha state
	D3DBLEND		m_SrcBlend;
	D3DBLEND		m_DestBlend;
	D3DBLENDOP		m_BlendOp;

	// Separate alpha blend state
	D3DBLEND		m_SrcBlendAlpha;
	D3DBLEND		m_DestBlendAlpha;
	D3DBLENDOP		m_BlendOpAlpha;

	D3DCMPFUNC		m_AlphaFunc;
	int				m_AlphaRef;

	// Texture stage state
	TextureStageShadowState_t m_TextureStage[MAX_TEXTURE_STAGES];

	// Sampler state
	SamplerShadowState_t	m_SamplerState[MAX_SAMPLERS];

	ShaderFogMode_t			m_FogMode;

	D3DMATERIALCOLORSOURCE	m_DiffuseMaterialSource;

	unsigned char			m_ZWriteEnable:1;
	unsigned char			m_ZBias:2;
	// Cull State?
	unsigned char			m_CullEnable:1;
	// Lighting in hardware?
	unsigned char			m_Lighting:1;
	unsigned char			m_SpecularEnable:1;
	unsigned char			m_AlphaBlendEnable:1;
	unsigned char			m_AlphaTestEnable:1;

	// Fixed function?
	unsigned char			m_UsingFixedFunction:1;
	// Vertex blending?
	unsigned char			m_VertexBlendEnable:1;
	// Auto-convert from linear to gamma upon writing to the frame buffer?
	unsigned char			m_SRGBWriteEnable:1;
	// Seperate Alpha Blend?
	unsigned char			m_SeparateAlphaBlendEnable:1;
	// Stencil?
	unsigned char			m_StencilEnable:1;

	unsigned char			m_bDisableFogGammaCorrection:1;

	unsigned char			m_EnableAlphaToCoverage:1;

	unsigned char			m_Reserved : 1;
	unsigned short			m_nReserved2;
};


//-----------------------------------------------------------------------------
// These are part of the "shadow" since they describe the shading algorithm
// but aren't actually captured in the state transition table 
// because it would produce too many transitions
//-----------------------------------------------------------------------------
struct ShadowShaderState_t
{
	// The vertex + pixel shader group to use...
	VertexShader_t	m_VertexShader;
	PixelShader_t	m_PixelShader;

	// The static vertex + pixel shader indices
	int				m_nStaticVshIndex;
	int				m_nStaticPshIndex;

	// Vertex data used by this snapshot
	// Note that the vertex format actually used will be the
	// aggregate of the vertex formats used by all snapshots in a material
	VertexFormat_t	m_VertexUsage;

	// Morph data used by this snapshot
	// Note that the morph format actually used will be the
	// aggregate of the morph formats used by all snapshots in a material
	MorphFormat_t	m_MorphUsage;

	// Modulate constant color into the vertex color
	bool			m_ModulateConstantColor;

	bool			m_nReserved[3];
};


//-----------------------------------------------------------------------------
// The shader setup API
//-----------------------------------------------------------------------------
abstract_class IShaderShadowDX8 : public IShaderShadow
{
public:
	// Initializes it
	virtual void Init() = 0;

	// Gets at the shadow state
	virtual ShadowState_t const& GetShadowState() = 0;
	virtual ShadowShaderState_t const& GetShadowShaderState() = 0;

	// This must be called right before taking a snapshot
	virtual void ComputeAggregateShadowState( ) = 0;

	// Class factory methods
	static IShaderShadowDX8* Create( IShaderAPIDX8* pShaderAPIDX8 );
	static void Destroy( IShaderShadowDX8* pShaderShadow );
};

extern IShaderShadowDX8 *g_pShaderShadowDx8;

#endif // SHADERSHADOWDX8_H
