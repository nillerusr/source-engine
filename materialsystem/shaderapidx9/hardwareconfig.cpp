//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "togl/rendermechanism.h"
#include "hardwareconfig.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapi_global.h"
#include "materialsystem/materialsystem_config.h"
#include "tier1/convar.h"
#include "shaderdevicebase.h"
#include "tier0/icommandline.h"


//-----------------------------------------------------------------------------
//
// Hardware Config!
//
//-----------------------------------------------------------------------------
static CHardwareConfig s_HardwareConfig;
CHardwareConfig *g_pHardwareConfig = &s_HardwareConfig;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CHardwareConfig, IMaterialSystemHardwareConfig, 
	MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, s_HardwareConfig )

template<typename T>
static void ccs_create_convar_from_hwconfig( const T& val, const char *pName )
{
	int nValue = static_cast<int>( val );

	ConVar *pConVar = g_pCVar->FindVar( pName );
	if ( pConVar )
	{
		pConVar->SetValue( nValue );
	}
	else
	{
		// Don't care if this leaks - this is only used for development
		pConVar = new ConVar( pName, "" );
	}
	if ( !nValue )
		pConVar->SetValue( "0" );
	else
		pConVar->SetValue( nValue );
}

static void ccs_create_convar_from_hwconfig( const char *pVal, const char *pName )
{
	ConVar *pConVar = g_pCVar->FindVar( pName );
	if ( pConVar )
	{
		pConVar->SetValue( pVal );
	}
	else
	{
		// Don't care if this leaks - this is only used for development
		pConVar = new ConVar( pName, "" );
	}
	pConVar->SetValue( pVal );
}

CON_COMMAND_F( ccs_create_convars_from_hwconfig, "Create convars from the current hardware config, useful for diffing purposes", FCVAR_CHEAT )
{
	if ( !g_pHardwareConfig )
		return;

	const HardwareCaps_t &caps = g_pHardwareConfig->CapsForEdit();

#define HWCFG( Name ) ccs_create_convar_from_hwconfig( caps.Name, #Name )
	HWCFG( m_NumTextureStages );
	HWCFG( m_nMaxAnisotropy );
	HWCFG( m_MaxTextureWidth );
	HWCFG( m_MaxTextureHeight );
	HWCFG( m_MaxTextureDepth );
	HWCFG( m_MaxTextureAspectRatio );
	HWCFG( m_MaxPrimitiveCount );
	HWCFG( m_NumPixelShaderConstants );
	HWCFG( m_NumBooleanPixelShaderConstants );
	HWCFG( m_NumIntegerPixelShaderConstants );
	HWCFG( m_NumVertexShaderConstants );
	HWCFG( m_NumBooleanVertexShaderConstants );
	HWCFG( m_NumIntegerVertexShaderConstants );
	HWCFG( m_TextureMemorySize );
	HWCFG( m_MaxNumLights );
	HWCFG( m_MaxBlendMatrices );
	HWCFG( m_MaxBlendMatrixIndices );
	HWCFG( m_MaxVertexShaderBlendMatrices );
	HWCFG( m_MaxUserClipPlanes );
	HWCFG( m_HDRType );
	HWCFG( m_pShaderDLL );
	HWCFG( m_ShadowDepthTextureFormat );
	HWCFG( m_NullTextureFormat );
	HWCFG( m_nVertexTextureCount );
	HWCFG( m_nMaxVertexTextureDimension );
	HWCFG( m_AlphaToCoverageState );					
	HWCFG( m_AlphaToCoverageEnableValue );				
	HWCFG( m_AlphaToCoverageDisableValue );			
	HWCFG( m_nMaxViewports );
	HWCFG( m_flMinGammaControlPoint );
	HWCFG( m_flMaxGammaControlPoint );
	HWCFG( m_nGammaControlPointCount );
	HWCFG( m_MaxVertexShader30InstructionSlots );
	HWCFG( m_MaxPixelShader30InstructionSlots );
	HWCFG( m_MaxSimultaneousRenderTargets );
	HWCFG( m_bDeviceOk );
	HWCFG( m_HasSetDeviceGammaRamp );
	HWCFG( m_SupportsVertexShaders );
	HWCFG( m_SupportsVertexShaders_2_0 );
	HWCFG( m_SupportsPixelShaders );
	HWCFG( m_SupportsPixelShaders_1_4 );
	HWCFG( m_SupportsPixelShaders_2_0 );
	HWCFG( m_SupportsPixelShaders_2_b );
	HWCFG( m_SupportsShaderModel_3_0 );
	HWCFG( m_bSupportsAnisotropicFiltering );
	HWCFG( m_bSupportsMagAnisotropicFiltering );
	HWCFG( m_bSupportsVertexTextures );
	HWCFG( m_ZBiasAndSlopeScaledDepthBiasSupported );
	HWCFG( m_SupportsMipmapping );
	HWCFG( m_SupportsOverbright );
	HWCFG( m_SupportsCubeMaps );
	HWCFG( m_SupportsHardwareLighting );
	HWCFG( m_SupportsMipmappedCubemaps );
	HWCFG( m_SupportsNonPow2Textures );
	HWCFG( m_PreferDynamicTextures );
	HWCFG( m_HasProjectedBumpEnv );
	HWCFG( m_SupportsSRGB );
	HWCFG( m_bSupportsSpheremapping );
	HWCFG( m_UseFastClipping );
	HWCFG( m_bNeedsATICentroidHack );
	HWCFG( m_bDisableShaderOptimizations );
	HWCFG( m_bColorOnSecondStream );
	HWCFG( m_bSupportsStreamOffset );
	HWCFG( m_bFogColorSpecifiedInLinearSpace );
	HWCFG( m_bFogColorAlwaysLinearSpace );
	HWCFG( m_bSupportsAlphaToCoverage );
	HWCFG( m_bSupportsShadowDepthTextures );
	HWCFG( m_bSupportsFetch4 );
	HWCFG( m_bSoftwareVertexProcessing );
	HWCFG( m_bScissorSupported );
	HWCFG( m_bSupportsFloat32RenderTargets );
	HWCFG( m_bSupportsBorderColor );
	HWCFG( m_bDX10Card );							
	HWCFG( m_bDX10Blending );						
	HWCFG( m_bSupportsStaticControlFlow );			
	HWCFG( m_FakeSRGBWrite );						
	HWCFG( m_CanDoSRGBReadFromRTs );				
	HWCFG( m_bSupportsGLMixedSizeTargets );			
	HWCFG( m_bCanStretchRectFromTextures );

	HWCFG( m_MaxHDRType );
#undef HWCFG
}

CHardwareConfig::CHardwareConfig()
{
	memset( &m_Caps, 0, sizeof( HardwareCaps_t ) );
	memset( &m_ActualCaps, 0, sizeof( HardwareCaps_t ) );
	memset( &m_UnOverriddenCaps, 0, sizeof( HardwareCaps_t ) );

#ifdef POSIX
		GLMPRINTF((" CHardwareConfig::CHardwareConfig setting m_bHDREnabled to false on %8x", this ));
#endif

	m_bHDREnabled = false;

	// FIXME: This is kind of a hack to deal with DX8 worldcraft startup.
	// We can at least have this much texture 
	m_Caps.m_MaxTextureWidth = m_Caps.m_MaxTextureHeight = m_Caps.m_MaxTextureDepth = 256;
}


//-----------------------------------------------------------------------------

bool CHardwareConfig::GetHDREnabled( void ) const
{
//	printf("\n CHardwareConfig::GetHDREnabled returning m_bHDREnabled value of %s on %8x", m_bHDREnabled?"true":"false", this );
	return m_bHDREnabled;
}

void CHardwareConfig::SetHDREnabled( bool bEnable )
{
//	printf("\n CHardwareConfig::SetHDREnabled setting m_bHDREnabled to value of %s on %8x", bEnable?"true":"false",  this );
	m_bHDREnabled = bEnable;
}


//-----------------------------------------------------------------------------
// Gets the recommended configuration associated with a particular dx level
//-----------------------------------------------------------------------------
void CHardwareConfig::ForceCapsToDXLevel( HardwareCaps_t *pCaps, int nDxLevel, const HardwareCaps_t &actualCaps )
{
	if ( !IsPC() || nDxLevel >= 100 )
		return;

	pCaps->m_nDXSupportLevel = nDxLevel;
	switch( nDxLevel )
	{
	case 60:
		// NOTE: Prior to dx9, numsamplers = num texture stages
		pCaps->m_NumTextureStages = min( 2, actualCaps.m_NumTextureStages );
		pCaps->m_NumSamplers = pCaps->m_NumTextureStages;
		pCaps->m_SupportsVertexShaders = false;
		pCaps->m_SupportsVertexShaders_2_0 = false;
		pCaps->m_SupportsPixelShaders = false;
		pCaps->m_SupportsPixelShaders_1_4 = false;
		pCaps->m_SupportsPixelShaders_2_0 = false;
		pCaps->m_SupportsPixelShaders_2_b = false;
		pCaps->m_SupportsShaderModel_3_0 = false;
		pCaps->m_bSupportsStaticControlFlow = false;
		pCaps->m_SupportsCompressedTextures = COMPRESSED_TEXTURES_OFF;
		pCaps->m_SupportsCompressedVertices = VERTEX_COMPRESSION_NONE;
		pCaps->m_bSupportsAnisotropicFiltering = false;
		pCaps->m_bSupportsMagAnisotropicFiltering = false;
		pCaps->m_nMaxAnisotropy = 1;
		pCaps->m_MaxTextureWidth = max( 256, pCaps->m_MaxTextureWidth );
		pCaps->m_MaxTextureHeight = max( 256, pCaps->m_MaxTextureHeight );
		pCaps->m_MaxTextureDepth = max( 256, pCaps->m_MaxTextureDepth );
		//		m_MaxTextureAspectRatio;
		//		int  m_MaxPrimitiveCount;
		pCaps->m_ZBiasAndSlopeScaledDepthBiasSupported = false;
		//		pCaps->m_SupportsMipmapping = 
		//		bool m_SupportsOverbright;
		pCaps->m_SupportsCubeMaps = false;
		pCaps->m_NumPixelShaderConstants = 0;
		pCaps->m_NumBooleanPixelShaderConstants = 0;
		pCaps->m_NumIntegerPixelShaderConstants = 0;
		pCaps->m_NumVertexShaderConstants = 0;
		pCaps->m_NumBooleanVertexShaderConstants = 0;
		pCaps->m_NumIntegerVertexShaderConstants = 0;
		pCaps->m_NumBooleanPixelShaderConstants = 0;
		pCaps->m_NumIntegerPixelShaderConstants = 0;
		pCaps->m_TextureMemorySize = 32 * 1024 * 1024;
		pCaps->m_MaxNumLights = 0;
		pCaps->m_SupportsHardwareLighting = 0;
		pCaps->m_MaxBlendMatrices = 0;
		pCaps->m_MaxBlendMatrixIndices = 0;
		pCaps->m_MaxVertexShaderBlendMatrices = 0;
		pCaps->m_SupportsMipmappedCubemaps = false;
		pCaps->m_SupportsNonPow2Textures = false;
		//		pCaps->m_DXSupportLevel = 60;
		pCaps->m_PreferDynamicTextures = false;
		pCaps->m_HasProjectedBumpEnv = false;
		pCaps->m_MaxUserClipPlanes = 0;
		pCaps->m_SupportsSRGB = false;
		pCaps->m_FakeSRGBWrite = false;
		pCaps->m_CanDoSRGBReadFromRTs = true;
		pCaps->m_bSupportsGLMixedSizeTargets = false;
		pCaps->m_HDRType = HDR_TYPE_NONE;
		//		pCaps->m_bSupportsSpheremapping = true;
		pCaps->m_UseFastClipping = true;
		pCaps->m_bNeedsATICentroidHack = false;
		pCaps->m_bColorOnSecondStream = false;
		pCaps->m_bSupportsStreamOffset = false;
		pCaps->m_bFogColorSpecifiedInLinearSpace = false;
		pCaps->m_bFogColorAlwaysLinearSpace = false;
		pCaps->m_bSupportsAlphaToCoverage = false;
		pCaps->m_bSupportsShadowDepthTextures = false;
		pCaps->m_bSupportsFetch4 = false;
		pCaps->m_bSupportsBorderColor = false;
		// m_bSoftwareVertexProcessing
		pCaps->m_nVertexTextureCount = 0;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_bSupportsVertexTextures = false;
		pCaps->m_nMaxViewports = 1;
		// m_bScissorSupported
		pCaps->m_bSupportsFloat32RenderTargets = false;
		// ImageFormat m_ShadowDepthTextureFormat;
		// ImageFormat m_NullTextureFormat;
		// m_AlphaToCoverageState;
		// m_AlphaToCoverageEnableValue;
		// m_AlphaToCoverageDisableValue;
		// m_flMinGammaControlPoint
		// m_flMaxGammaControlPoint
		// m_nGammaControlPointCount
		pCaps->m_MaxVertexShader30InstructionSlots = 0;
		pCaps->m_MaxPixelShader30InstructionSlots = 0;
		pCaps->m_bCanStretchRectFromTextures = false;
		break;

	case 70:
		// NOTE: Prior to dx9, numsamplers = num texture stages
		pCaps->m_NumTextureStages = min( 2, actualCaps.m_NumTextureStages );
		pCaps->m_NumSamplers = pCaps->m_NumTextureStages;
		pCaps->m_SupportsVertexShaders = false;
		pCaps->m_SupportsVertexShaders_2_0 = false;
		pCaps->m_SupportsPixelShaders = false;
		pCaps->m_SupportsPixelShaders_1_4 = false;
		pCaps->m_SupportsPixelShaders_2_0 = false;
		pCaps->m_SupportsPixelShaders_2_b = false;
		pCaps->m_SupportsShaderModel_3_0 = false;
		pCaps->m_bSupportsStaticControlFlow = false;
		//		pCaps->m_SupportsCompressedTextures = true;
		pCaps->m_SupportsCompressedVertices = VERTEX_COMPRESSION_NONE;
		pCaps->m_bSupportsAnisotropicFiltering = false;
		pCaps->m_bSupportsMagAnisotropicFiltering = false;
		pCaps->m_nMaxAnisotropy = 1;
		//		pCaps->m_MaxTextureWidth = max( 256, pCaps->m_MaxTextureWidth );
		//		pCaps->m_MaxTextureHeight = max( 256, pCaps->m_MaxTextureHeight );
//		pCaps->m_MaxTextureDepth = max( 256, pCaps->m_MaxTextureDepth );
		//		m_MaxTextureAspectRatio;
		//		int  m_MaxPrimitiveCount;
		pCaps->m_ZBiasAndSlopeScaledDepthBiasSupported = false;
		//		pCaps->m_SupportsMipmapping = 
		//		bool m_SupportsOverbright;
		//		pCaps->m_SupportsCubeMaps = false;
		pCaps->m_NumPixelShaderConstants = 0;
		pCaps->m_NumBooleanPixelShaderConstants = 0;
		pCaps->m_NumIntegerPixelShaderConstants = 0;
		pCaps->m_NumBooleanPixelShaderConstants = 0;
		pCaps->m_NumIntegerPixelShaderConstants = 0;
		pCaps->m_NumVertexShaderConstants = 0;
		pCaps->m_NumBooleanVertexShaderConstants = 0;
		pCaps->m_NumIntegerVertexShaderConstants = 0;
		pCaps->m_TextureMemorySize = 32 * 1024 * 1024;
		pCaps->m_MaxNumLights = 2;
		pCaps->m_SupportsHardwareLighting = 1;
		pCaps->m_MaxBlendMatrices = 0;
		pCaps->m_MaxBlendMatrixIndices = 0;
		pCaps->m_MaxVertexShaderBlendMatrices = 0;
		pCaps->m_SupportsMipmappedCubemaps = false;
		pCaps->m_SupportsNonPow2Textures = false;
		pCaps->m_nDXSupportLevel = 70;
		pCaps->m_PreferDynamicTextures = false;
		pCaps->m_HasProjectedBumpEnv = false;
		pCaps->m_MaxUserClipPlanes = 0;
		pCaps->m_SupportsSRGB = false;
		pCaps->m_FakeSRGBWrite = false;
		pCaps->m_CanDoSRGBReadFromRTs = true;
		pCaps->m_bSupportsGLMixedSizeTargets = false;
		pCaps->m_HDRType = HDR_TYPE_NONE;
		//		pCaps->m_bSupportsSpheremapping = true;
		pCaps->m_UseFastClipping = true;
		pCaps->m_bNeedsATICentroidHack = false;
		//		pCaps->m_bColorOnSecondStream = false;	// dont' force this!
		pCaps->m_bSupportsStreamOffset = false;
		pCaps->m_bFogColorSpecifiedInLinearSpace = false;
		pCaps->m_bFogColorAlwaysLinearSpace = false;
		pCaps->m_bSupportsAlphaToCoverage = false;
		pCaps->m_bSupportsShadowDepthTextures = false;
		pCaps->m_bSupportsFetch4 = false;
		pCaps->m_bSupportsBorderColor = false;
		// m_bSoftwareVertexProcessing
		pCaps->m_nVertexTextureCount = 0;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_bSupportsVertexTextures = false;
		pCaps->m_nMaxViewports = 1;
		// m_bScissorSupported
		pCaps->m_bSupportsFloat32RenderTargets = false;
		pCaps->m_bDX10Card = false;
		pCaps->m_bDX10Blending = false;
		pCaps->m_bCanStretchRectFromTextures = false;
		break;

	case 80:
		// NOTE: Prior to dx9, numsamplers = num texture stages
		// We clamp num texture stages to 2, though, since we never use
		// fixed-function shaders with more than 2 texture stages
		pCaps->m_NumTextureStages = min( 2, actualCaps.m_NumTextureStages );
		pCaps->m_NumSamplers = min( 4, actualCaps.m_NumTextureStages );
		//		pCaps->m_SupportsVertexShaders = true;
		pCaps->m_SupportsVertexShaders_2_0 = false;
		//		pCaps->m_SupportsPixelShaders = false;
		pCaps->m_SupportsPixelShaders_1_4 = false;
		pCaps->m_SupportsPixelShaders_2_0 = false;
		pCaps->m_SupportsPixelShaders_2_b = false;
		pCaps->m_SupportsShaderModel_3_0 = false;
		pCaps->m_bSupportsStaticControlFlow = false;
		//		pCaps->m_SupportsCompressedTextures = true;
		pCaps->m_SupportsCompressedVertices = VERTEX_COMPRESSION_NONE;
		//		pCaps->m_bSupportsAnisotropicFiltering = false;
		//		pCaps->m_bSupportsMagAnisotropicFiltering = false;
		//		pCaps->m_nMaxAnisotropy = 1;
		//		pCaps->m_MaxTextureWidth = max( 256, pCaps->m_MaxTextureWidth );
		//		pCaps->m_MaxTextureHeight = max( 256, pCaps->m_MaxTextureHeight );
		//		pCaps->m_MaxTextureDepth = max( 256, pCaps->m_MaxTextureDepth );
		//		m_MaxTextureAspectRatio;
		//		int  m_MaxPrimitiveCount;
		//		pCaps->m_ZBiasAndSlopeScaledDepthBiasSupported = false;
		//		pCaps->m_SupportsMipmapping = 
		//		bool m_SupportsOverbright;
		//		pCaps->m_SupportsCubeMaps = false;
		pCaps->m_NumPixelShaderConstants = 8;
		pCaps->m_NumVertexShaderConstants = min( 96, pCaps->m_NumVertexShaderConstants );
		pCaps->m_NumBooleanVertexShaderConstants = 0;
		pCaps->m_NumIntegerVertexShaderConstants = 0;
		pCaps->m_NumBooleanPixelShaderConstants = 0;
		pCaps->m_NumIntegerPixelShaderConstants = 0;
		//		pCaps->m_TextureMemorySize = 32 * 1024 * 1024;
		//		pCaps->m_MaxNumLights = 0;
		//		pCaps->m_SupportsHardwareLighting = 0;
		//		pCaps->m_MaxBlendMatrices = 0;
		//		pCaps->m_MaxBlendMatrixIndices = 0;
		pCaps->m_MaxVertexShaderBlendMatrices = min( 16, pCaps->m_MaxVertexShaderBlendMatrices );
		//		pCaps->m_SupportsMipmappedCubemaps = false;
		//		pCaps->m_SupportsNonPow2Textures = false;
		pCaps->m_nDXSupportLevel = 80;
		//		pCaps->m_PreferDynamicTextures = false;
		//		pCaps->m_HasProjectedBumpEnv = false;
		//		pCaps->m_MaxUserClipPlanes = 0;
		pCaps->m_bSupportsGLMixedSizeTargets = false;
		pCaps->m_HDRType = HDR_TYPE_NONE;
		//		pCaps->m_bSupportsSpheremapping = true;
		//		pCaps->m_UseFastClipping = true;
		//		pCaps->m_bNeedsATICentroidHack = false;
		//		pCaps->m_bColorOnSecondStream = false;	
		pCaps->m_bSupportsStreamOffset = false;
		pCaps->m_bFogColorSpecifiedInLinearSpace = false;
		pCaps->m_bFogColorAlwaysLinearSpace = false;
		pCaps->m_bSupportsAlphaToCoverage = false;
		pCaps->m_bSupportsShadowDepthTextures = false;
		pCaps->m_bSupportsFetch4 = false;
		// m_bSoftwareVertexProcessing
		pCaps->m_nVertexTextureCount = 0;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_bSupportsVertexTextures = false;
		pCaps->m_MaxNumLights = 2;
		pCaps->m_nMaxViewports = 1;
		// m_bScissorSupported
		pCaps->m_SupportsSRGB = false;
		pCaps->m_FakeSRGBWrite = false;
		pCaps->m_CanDoSRGBReadFromRTs = true;
		pCaps->m_bSupportsGLMixedSizeTargets = false;
		pCaps->m_bSupportsFetch4 = false;
		pCaps->m_bSupportsBorderColor = false;
		pCaps->m_bSupportsFloat32RenderTargets = false;
		// ImageFormat m_ShadowDepthTextureFormat;
		// ImageFormat m_NullTextureFormat;
		// m_AlphaToCoverageState;
		// m_AlphaToCoverageEnableValue;
		// m_AlphaToCoverageDisableValue;
		// m_flMinGammaControlPoint
		// m_flMaxGammaControlPoint
		// m_nGammaControlPointCount
		pCaps->m_MaxVertexShader30InstructionSlots = 0;
		pCaps->m_MaxPixelShader30InstructionSlots = 0;
		pCaps->m_bDX10Card = false;
		pCaps->m_bDX10Blending = false;
		pCaps->m_bCanStretchRectFromTextures = false;
		break;

	case 81:
		// NOTE: Prior to dx9, numsamplers = num texture stages
		// We clamp num texture stages to 2, though, since we never use
		// fixed-function shaders with more than 2 texture stages
		pCaps->m_NumTextureStages = min( 2, actualCaps.m_NumTextureStages );
		pCaps->m_NumSamplers = min( 6, actualCaps.m_NumTextureStages );
		//		pCaps->m_SupportsVertexShaders = true;
		pCaps->m_SupportsVertexShaders_2_0 = false;
		//		pCaps->m_SupportsPixelShaders = false;
		pCaps->m_SupportsPixelShaders_1_4 = true;
		pCaps->m_SupportsPixelShaders_2_0 = false;
		pCaps->m_SupportsPixelShaders_2_b = false;
		pCaps->m_SupportsShaderModel_3_0 = false;
		pCaps->m_bSupportsStaticControlFlow = false;
		//		pCaps->m_SupportsCompressedTextures = true;
		pCaps->m_SupportsCompressedVertices = VERTEX_COMPRESSION_NONE;
		//		pCaps->m_bSupportsAnisotropicFiltering = false;
		//		pCaps->m_bSupportsMagAnisotropicFiltering = false;
		//		pCaps->m_nMaxAnisotropy = 1;
		//		pCaps->m_MaxTextureWidth = max( 256, pCaps->m_MaxTextureWidth );
		//		pCaps->m_MaxTextureHeight = max( 256, pCaps->m_MaxTextureHeight );
		//		pCaps->m_MaxTextureDepth = max( 256, pCaps->m_MaxTextureDepth );
		//		m_MaxTextureAspectRatio;
		//		int  m_MaxPrimitiveCount;
		//		pCaps->m_ZBiasAndSlopeScaledDepthBiasSupported = false;
		//		pCaps->m_SupportsMipmapping = 
		//		bool m_SupportsOverbright;
		//		pCaps->m_SupportsCubeMaps = false;
		pCaps->m_NumPixelShaderConstants = 8;
		pCaps->m_NumVertexShaderConstants = min( 96, pCaps->m_NumVertexShaderConstants );
		pCaps->m_NumBooleanVertexShaderConstants = 0;
		pCaps->m_NumIntegerVertexShaderConstants = 0;
		pCaps->m_NumBooleanPixelShaderConstants = 0;
		pCaps->m_NumIntegerPixelShaderConstants = 0;
		//		pCaps->m_TextureMemorySize = 32 * 1024 * 1024;
		pCaps->m_MaxNumLights = 2;
		//		pCaps->m_SupportsHardwareLighting = 0;
		//		pCaps->m_MaxBlendMatrices = 0;
		//		pCaps->m_MaxBlendMatrixIndices = 0;
		pCaps->m_MaxVertexShaderBlendMatrices = min( 16, pCaps->m_MaxVertexShaderBlendMatrices );
		//		pCaps->m_SupportsMipmappedCubemaps = false;
		//		pCaps->m_SupportsNonPow2Textures = false;
		pCaps->m_nDXSupportLevel = 81;
		//		pCaps->m_PreferDynamicTextures = false;
		//		pCaps->m_HasProjectedBumpEnv = false;
		//		pCaps->m_MaxUserClipPlanes = 0;
		pCaps->m_SupportsSRGB = false;
		pCaps->m_FakeSRGBWrite = false;
		pCaps->m_CanDoSRGBReadFromRTs = true;
		pCaps->m_bSupportsGLMixedSizeTargets = false;
		pCaps->m_HDRType = HDR_TYPE_NONE;
		//		pCaps->m_bSupportsSpheremapping = true;
		//		pCaps->m_UseFastClipping = true;
		//		pCaps->m_bNeedsATICentroidHack = false;
		//		pCaps->m_bColorOnSecondStream = false;
		pCaps->m_bSupportsStreamOffset = false;
		pCaps->m_bFogColorSpecifiedInLinearSpace = false;
		pCaps->m_bFogColorAlwaysLinearSpace = false;
		pCaps->m_bSupportsAlphaToCoverage = false;
		pCaps->m_bSupportsShadowDepthTextures = false;
		pCaps->m_bSupportsFetch4 = false;
		pCaps->m_bSupportsBorderColor = false;
		// m_bSoftwareVertexProcessing
		pCaps->m_nVertexTextureCount = 0;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_bSupportsVertexTextures = false;
		pCaps->m_nMaxViewports = 1;
		// m_bScissorSupported
		pCaps->m_bSupportsFloat32RenderTargets = false;
		// ImageFormat m_ShadowDepthTextureFormat;
		// ImageFormat m_NullTextureFormat;
		// m_AlphaToCoverageState;
		// m_AlphaToCoverageEnableValue;
		// m_AlphaToCoverageDisableValue;
		// m_flMinGammaControlPoint
		// m_flMaxGammaControlPoint
		// m_nGammaControlPointCount
		pCaps->m_MaxVertexShader30InstructionSlots = 0;
		pCaps->m_MaxPixelShader30InstructionSlots = 0;
		pCaps->m_bDX10Card = false;
		pCaps->m_bDX10Blending = false;
		pCaps->m_bCanStretchRectFromTextures = false;
		break;

	case 90:
		pCaps->m_nVertexTextureCount = 0;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_bSupportsVertexTextures = false;
		pCaps->m_bSupportsStreamOffset = true;
		pCaps->m_bSupportsGLMixedSizeTargets = true;

		if ( IsOpenGL() )
		{
			//FIXME this is way too complicated, we should just check the caps bit from GLM

			pCaps->m_bSupportsStaticControlFlow = false;

			if (1)	//(CommandLine()->FindParm("-glslmode"))
			{
				// rbarris 03Feb10: this is now hardwired because we are defaulting GLSL mode "on".
				// so this will mean that the engine will always ask for user clip planes.
				// this will misbehave under ARB mode, since ARB shaders won't respect that state.
				// it's difficult to make this fluid without teaching the engine about a cap that could change during run.

				pCaps->m_MaxUserClipPlanes = 2;
				pCaps->m_UseFastClipping = false;
			}
			else
			{
				pCaps->m_MaxUserClipPlanes = 0;
				pCaps->m_UseFastClipping = true;
			}

			pCaps->m_MaxNumLights = 2;
		}
		else
		{
			pCaps->m_bSupportsStaticControlFlow = true;
			pCaps->m_MaxNumLights = pCaps->m_SupportsPixelShaders_2_b ? 4 : 2;	// 2b gets four lights, 2.0 gets two...
		}

		pCaps->m_bSupportsBorderColor = false;
		pCaps->m_nMaxViewports = 1;
		pCaps->m_NumPixelShaderConstants = 32;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_MaxVertexShader30InstructionSlots = 0;
		pCaps->m_MaxPixelShader30InstructionSlots  = 0;
		pCaps->m_bCanStretchRectFromTextures = false;
		break;

	case 92:
		pCaps->m_nVertexTextureCount = 0;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_bSupportsVertexTextures = false;
		pCaps->m_bSupportsBorderColor = false;

		// 2b gets four lights, 2.0 gets two...
		pCaps->m_SupportsShaderModel_3_0 = false;
		if ( IsOpenGL() )
		{
			if ( IsOSX() )
			{
				pCaps->m_bSupportsStaticControlFlow = CommandLine()->CheckParm( "-glslcontrolflow" ) != NULL;
			}
			else
			{
				pCaps->m_bSupportsStaticControlFlow = !CommandLine()->CheckParm( "-noglslcontrolflow" );
			}

			pCaps->m_MaxUserClipPlanes = 2;
			pCaps->m_UseFastClipping = false;
			pCaps->m_MaxNumLights = pCaps->m_bSupportsStaticControlFlow ? MAX_NUM_LIGHTS : ( MAX_NUM_LIGHTS - 2 );
		}
		else
		{
			pCaps->m_MaxNumLights = MAX_NUM_LIGHTS;
		}

		pCaps->m_nMaxViewports = 1;
		pCaps->m_NumPixelShaderConstants = 32;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_bDX10Card = false;
		pCaps->m_bDX10Blending = false;
		pCaps->m_MaxVertexShader30InstructionSlots = 0;
		pCaps->m_MaxPixelShader30InstructionSlots  = 0;
		pCaps->m_bCanStretchRectFromTextures = false;
		break;

	case 95:
		pCaps->m_bSupportsStreamOffset = true;
		pCaps->m_bSupportsStaticControlFlow = true;
		pCaps->m_bDX10Card = false;
		pCaps->m_bDX10Blending = false;
		pCaps->m_MaxNumLights = MAX_NUM_LIGHTS;
		pCaps->m_nMaxViewports = 1;
		pCaps->m_bSupportsBorderColor = false;
		pCaps->m_bCanStretchRectFromTextures = false;
		break;

	case 100:
		break;

	default:
		Assert( 0 );
		break;
	}
}


//-----------------------------------------------------------------------------
// Sets up the hardware caps given the specified DX level
//-----------------------------------------------------------------------------
void CHardwareConfig::SetupHardwareCaps( int nDXLevel, const HardwareCaps_t &actualCaps )
{
	Assert( nDXLevel != 0 );

	memcpy( &m_Caps, &actualCaps, sizeof(HardwareCaps_t) );
	memcpy( &m_UnOverriddenCaps, &actualCaps, sizeof(HardwareCaps_t) );

	// Don't bother with fallbacks for DX10 or consoles
#ifdef DX_TO_GL_ABSTRACTION
	if ( nDXLevel >= 100 )
#else
	if ( !( IsPC() || IsPosix() ) || ( nDXLevel >= 100 ) )
#endif
		return;

	// Slam the support level to what we were requested
	m_Caps.m_nDXSupportLevel = nDXLevel;
	if ( m_Caps.m_nDXSupportLevel != m_Caps.m_nMaxDXSupportLevel || CommandLine()->ParmValue( "-maxdxlevel", 0 ) > 0 )
	{
		// We're falling back to some other dx level
		ForceCapsToDXLevel( &m_Caps, m_Caps.m_nDXSupportLevel, m_ActualCaps );
	}

	// Clamp num texture stages to 2, since it's only used for fixed function
	m_Caps.m_NumTextureStages = min( 2, m_Caps.m_NumTextureStages );
	
	// Read dxsupport.cfg which has config overrides for particular cards.
	g_pShaderDeviceMgr->ReadHardwareCaps( m_Caps, m_Caps.m_nDXSupportLevel );

	// This is the spot to validate read in caps versus actual caps.
	if ( m_Caps.m_MaxUserClipPlanes > m_ActualCaps.m_MaxUserClipPlanes )
	{
		m_Caps.m_MaxUserClipPlanes = m_ActualCaps.m_MaxUserClipPlanes;
	}
	if ( m_Caps.m_MaxUserClipPlanes == 0 )
	{
		m_Caps.m_UseFastClipping = true;
	}

	if ( IsOpenGL() )
	{
		m_Caps.m_MaxNumLights = MIN( m_Caps.m_MaxNumLights, ( m_Caps.m_bSupportsStaticControlFlow && m_Caps.m_SupportsPixelShaders_2_b ) ? MAX_NUM_LIGHTS : ( MAX_NUM_LIGHTS - 2 ) );
		m_Caps.m_bSupportsShadowDepthTextures = true;
	}
	else // not POSIX or emulated
	{
		// 2b supports more lights than just 2.0
		if ( m_Caps.m_SupportsPixelShaders_2_b )
		{
			m_Caps.m_MaxNumLights = MIN( m_Caps.m_MaxNumLights, MAX_NUM_LIGHTS );
		}
		else
		{
			m_Caps.m_MaxNumLights = MIN( m_Caps.m_MaxNumLights, MAX_NUM_LIGHTS - 2 );
		}
	}

	m_Caps.m_MaxNumLights = min( m_Caps.m_MaxNumLights, (int)MAX_NUM_LIGHTS );

	memcpy( &m_UnOverriddenCaps, &m_Caps, sizeof(HardwareCaps_t) );
}


//-----------------------------------------------------------------------------
// Sets up the hardware caps given the specified DX level
//-----------------------------------------------------------------------------
void CHardwareConfig::SetupHardwareCaps( const ShaderDeviceInfo_t& mode, const HardwareCaps_t &actualCaps )
{
	memcpy( &m_ActualCaps, &actualCaps, sizeof(HardwareCaps_t) );
	SetupHardwareCaps( mode.m_nDXLevel, actualCaps );
}


void CHardwareConfig::OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport )
{
	if ( bOverrideEnabled )
	{
		m_Caps.m_bSupportsStreamOffset = bEnableSupport;
		if ( !m_ActualCaps.m_bSupportsStreamOffset )
		{
			m_Caps.m_bSupportsStreamOffset = false;
		}
	}
	else
	{
		// Go back to default
		m_Caps.m_bSupportsStreamOffset = m_UnOverriddenCaps.m_bSupportsStreamOffset;
	}
}


//-----------------------------------------------------------------------------
// Implementation of IMaterialSystemHardwareConfig
//-----------------------------------------------------------------------------
bool CHardwareConfig::HasDestAlphaBuffer() const
{
	if ( !g_pShaderDevice )
		return false;
	return (g_pShaderDevice->GetBackBufferFormat() == IMAGE_FORMAT_BGRA8888);
}

bool CHardwareConfig::HasStencilBuffer() const
{
	return StencilBufferBits() > 0;
}

int	 CHardwareConfig::GetFrameBufferColorDepth() const
{
	if ( !g_pShaderDevice )
		return 0;
	return ShaderUtil()->ImageFormatInfo( g_pShaderDevice->GetBackBufferFormat() ).m_NumBytes;
}

int CHardwareConfig::GetSamplerCount() const
{
	return m_Caps.m_NumSamplers;
}

bool CHardwareConfig::HasSetDeviceGammaRamp() const
{
	return m_Caps.m_HasSetDeviceGammaRamp;
}

bool CHardwareConfig::SupportsCompressedTextures() const
{
	Assert( m_Caps.m_SupportsCompressedTextures != COMPRESSED_TEXTURES_NOT_INITIALIZED );
	return m_Caps.m_SupportsCompressedTextures == COMPRESSED_TEXTURES_ON;
}

VertexCompressionType_t CHardwareConfig::SupportsCompressedVertices() const
{
	return m_Caps.m_SupportsCompressedVertices;
}

bool CHardwareConfig::SupportsBorderColor() const
{
	return m_Caps.m_bSupportsBorderColor;
}

bool CHardwareConfig::SupportsFetch4() const
{
	return m_Caps.m_bSupportsFetch4;
}

bool CHardwareConfig::CanStretchRectFromTextures() const
{
	return m_Caps.m_bCanStretchRectFromTextures;
}

bool CHardwareConfig::SupportsVertexAndPixelShaders() const
{
	if ( (ShaderUtil()->GetConfig().dxSupportLevel != 0) && (GetDXSupportLevel() < 80) )
		return false;

	return m_Caps.m_SupportsPixelShaders;
}

bool CHardwareConfig::SupportsPixelShaders_1_4() const
{
	if ( (ShaderUtil()->GetConfig().dxSupportLevel != 0) && (GetDXSupportLevel() < 81) )
		return false;

	return m_Caps.m_SupportsPixelShaders_1_4;
}

bool CHardwareConfig::SupportsPixelShaders_2_0() const
{
	if ( (ShaderUtil()->GetConfig().dxSupportLevel != 0) && (GetDXSupportLevel() < 90) )
		return false;

	return m_Caps.m_SupportsPixelShaders_2_0;
}

bool CHardwareConfig::SupportsPixelShaders_2_b() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel != 0) &&
		(GetDXSupportLevel() < 90))
		return false;

	return m_Caps.m_SupportsPixelShaders_2_b;
}

bool CHardwareConfig::SupportsVertexShaders_2_0() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel != 0) &&
		(GetDXSupportLevel() < 90))
		return false;

	return m_Caps.m_SupportsVertexShaders_2_0;
}

bool CHardwareConfig::SupportsStaticControlFlow() const
{
	return m_Caps.m_bSupportsStaticControlFlow;
}


bool CHardwareConfig::SupportsShaderModel_3_0() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel != 0) &&
		(GetDXSupportLevel() < 95))
		return false;

	return m_Caps.m_SupportsShaderModel_3_0;
}

// If you change these, make the corresponding change in common_ps_fxc.h
#define NVIDIA_PCF_POISSON	0
#define ATI_NOPCF			1
#define ATI_NO_PCF_FETCH4	2

int CHardwareConfig::GetShadowFilterMode() const
{

#ifdef DX_TO_GL_ABSTRACTION
	if ( !m_Caps.m_bSupportsShadowDepthTextures )
		return 0;
#else	
	if ( !m_Caps.m_bSupportsShadowDepthTextures || !ShaderUtil()->GetConfig().ShadowDepthTexture() )
		return 0;
#endif

	switch ( m_Caps.m_ShadowDepthTextureFormat )
	{
		case IMAGE_FORMAT_NV_DST16:
		case IMAGE_FORMAT_NV_DST24:

			return NVIDIA_PCF_POISSON;							// NVIDIA hardware bilinear PCF

		case IMAGE_FORMAT_ATI_DST16:
		case IMAGE_FORMAT_ATI_DST24:

			if ( m_Caps.m_bSupportsFetch4 )
				return ATI_NO_PCF_FETCH4;						// ATI fetch4 depth texture sampling

			return ATI_NOPCF;									// ATI vanilla depth texture sampling

#if defined( _X360 )
		case IMAGE_FORMAT_X360_DST16:
		case IMAGE_FORMAT_X360_DST24:
		case IMAGE_FORMAT_X360_DST24F:
			return 0;
#endif

		default:
			return 0;
	}

	return 0;
}

static ConVar r_shader_srgb( "r_shader_srgb", "0", FCVAR_ALLOWED_IN_COMPETITIVE, "-1 = use hardware caps. 0 = use hardware srgb. 1 = use shader srgb(software lookup)" );		// -1=use caps 0=off 1=on

int CHardwareConfig::NeedsShaderSRGBConversion() const
{
	if ( IsX360() )
	{
		// 360 always now uses a permanent hw solution
		return false;
	}

	int cValue = r_shader_srgb.GetInt();
	switch( cValue )
	{
		case 0:
			return false;

		case 1:
			return true;

		default:
			return m_Caps.m_bDX10Blending;						// !!! change to return false after portal deport built!!!!!
	}
}

bool CHardwareConfig::UsesSRGBCorrectBlending() const
{
	int cValue = r_shader_srgb.GetInt();
	return ( cValue == 0 ) && ( ( m_Caps.m_bDX10Blending ) || IsX360() );
}

static ConVar mat_disablehwmorph( "mat_disablehwmorph", "0", FCVAR_ALLOWED_IN_COMPETITIVE, "Disables HW morphing for particular mods" );
bool CHardwareConfig::HasFastVertexTextures() const
{
	static int bEnableFastVertexTextures = -1;
	static bool bDisableHWMorph = false;
	if ( bEnableFastVertexTextures < 0 )
	{
		bEnableFastVertexTextures = 1;
		if ( CommandLine()->FindParm( "-disallowhwmorph" ) )
		{
			bEnableFastVertexTextures = 0;
		}
		bDisableHWMorph = ( mat_disablehwmorph.GetInt() != 0 );
	}

	// JasonM - turned this off for Orange Box release...
	return false;

//	return m_Caps.m_bDX10Card && ( GetDXSupportLevel() >= 95 ) && ( bEnableFastVertexTextures != 0 ) && ( !bDisableHWMorph );
}

int CHardwareConfig::MaxHWMorphBatchCount() const
{
	return ShaderUtil()->MaxHWMorphBatchCount();
}

int CHardwareConfig::MaximumAnisotropicLevel() const
{
	return m_Caps.m_nMaxAnisotropy;
}

int CHardwareConfig::MaxTextureWidth() const
{
	return m_Caps.m_MaxTextureWidth;
}

int CHardwareConfig::MaxTextureHeight() const
{
	return m_Caps.m_MaxTextureHeight;
}

int	CHardwareConfig::TextureMemorySize() const
{
	return m_Caps.m_TextureMemorySize;
}

bool CHardwareConfig::SupportsOverbright() const
{
	return m_Caps.m_SupportsOverbright;
}

bool CHardwareConfig::SupportsCubeMaps() const
{
	if ( (ShaderUtil()->GetConfig().dxSupportLevel > 0) && (GetDXSupportLevel() < 70) )
		return false;

	return m_Caps.m_SupportsCubeMaps;
}

bool CHardwareConfig::SupportsMipmappedCubemaps() const
{
	if ( (ShaderUtil()->GetConfig().dxSupportLevel > 0) && (GetDXSupportLevel() < 70) )
		return false;

	return m_Caps.m_SupportsMipmappedCubemaps;
}

bool CHardwareConfig::SupportsNonPow2Textures() const
{
	return m_Caps.m_SupportsNonPow2Textures;
}

int  CHardwareConfig::GetTextureStageCount() const
{
	return m_Caps.m_NumTextureStages;
}

int	 CHardwareConfig::NumVertexShaderConstants() const
{
	return m_Caps.m_NumVertexShaderConstants;
}

int	 CHardwareConfig::NumBooleanVertexShaderConstants() const
{
	return m_Caps.m_NumBooleanVertexShaderConstants;
}

int	 CHardwareConfig::NumIntegerVertexShaderConstants() const
{
	return m_Caps.m_NumIntegerVertexShaderConstants;
}

int	 CHardwareConfig::NumPixelShaderConstants() const
{
	return m_Caps.m_NumPixelShaderConstants;
}

int	 CHardwareConfig::NumBooleanPixelShaderConstants() const
{
	return m_Caps.m_NumBooleanPixelShaderConstants;
}

int	 CHardwareConfig::NumIntegerPixelShaderConstants() const
{
	return m_Caps.m_NumIntegerPixelShaderConstants;
}

int	 CHardwareConfig::MaxNumLights() const
{
	return m_Caps.m_MaxNumLights;
}

bool CHardwareConfig::SupportsHardwareLighting() const
{
	if ( (ShaderUtil()->GetConfig().dxSupportLevel > 0) && (GetDXSupportLevel() < 70) )
		return false;

	return m_Caps.m_SupportsHardwareLighting;
}

int	 CHardwareConfig::MaxBlendMatrices() const
{
	if ( (ShaderUtil()->GetConfig().dxSupportLevel > 0) && (GetDXSupportLevel() < 70) )
		return 1;

	return m_Caps.m_MaxBlendMatrices;
}

int	 CHardwareConfig::MaxBlendMatrixIndices() const
{
	if ( (ShaderUtil()->GetConfig().dxSupportLevel > 0) && (GetDXSupportLevel() < 70) )
		return 1;

	return m_Caps.m_MaxBlendMatrixIndices;
}

int CHardwareConfig::MaxTextureAspectRatio() const
{
	return m_Caps.m_MaxTextureAspectRatio;
}

int	 CHardwareConfig::MaxVertexShaderBlendMatrices() const
{
	if ( (ShaderUtil()->GetConfig().dxSupportLevel > 0) && (GetDXSupportLevel() < 70) )
		return 1;

	return m_Caps.m_MaxVertexShaderBlendMatrices;
}

// Useful for testing fastclip on Windows
extern ConVar mat_fastclip;

int CHardwareConfig::MaxUserClipPlanes() const
{
	if ( mat_fastclip.GetBool() )
		return 0;

	return m_Caps.m_MaxUserClipPlanes;
}

bool CHardwareConfig::UseFastClipping() const
{
	// rbarris broke this up for easier view of outcome in debugger
	bool fastclip = mat_fastclip.GetBool();
	
	bool result = m_Caps.m_UseFastClipping || fastclip;
	
	return result;
}

int CHardwareConfig::MaxTextureDepth() const
{
	return m_Caps.m_MaxTextureDepth;
}

int CHardwareConfig::GetDXSupportLevel() const
{
	if ( ShaderUtil()->GetConfig().dxSupportLevel != 0 )
	{
		return min( ShaderUtil()->GetConfig().dxSupportLevel, m_Caps.m_nDXSupportLevel );
	}

	return m_Caps.m_nDXSupportLevel;
}

const char *CHardwareConfig::GetShaderDLLName() const
{
	return ( m_Caps.m_pShaderDLL && m_Caps.m_pShaderDLL[0] ) ? m_Caps.m_pShaderDLL : "DEFAULT";
}

bool CHardwareConfig::ReadPixelsFromFrontBuffer() const
{
	if ( IsX360() )
	{
		// future proof safety, not allowing the front read path
		return false;
	}

	// GR - in DX 9.0a can blit from MSAA back buffer
	return false;
}

bool CHardwareConfig::PreferDynamicTextures() const
{
	if ( IsX360() )
	{
		// future proof safety, not allowing these
		return false;
	}

	return m_Caps.m_PreferDynamicTextures;
}

bool CHardwareConfig::SupportsHDR() const
{
	// This is a deprecated function. . use GetHDRType instead.  For shipping HL2, this always being false is correct.
	Assert( 0 );
	return false;
}

bool CHardwareConfig::SupportsHDRMode( HDRType_t nHDRType ) const
{
	switch( nHDRType )
	{
		case HDR_TYPE_NONE:
			return true;

		case HDR_TYPE_INTEGER:
			return ( m_Caps.m_MaxHDRType == HDR_TYPE_INTEGER ) || ( m_Caps.m_MaxHDRType == HDR_TYPE_FLOAT );

		case HDR_TYPE_FLOAT:
			return ( m_Caps.m_MaxHDRType == HDR_TYPE_FLOAT );
			
	}
	return false;

}

bool CHardwareConfig::HasProjectedBumpEnv() const
{
	return m_Caps.m_HasProjectedBumpEnv;
}

bool CHardwareConfig::SupportsSpheremapping() const
{
	return m_Caps.m_bSupportsSpheremapping;
}

bool CHardwareConfig::NeedsAAClamp() const
{
	return false;
}

bool CHardwareConfig::NeedsATICentroidHack() const
{
	return m_Caps.m_bNeedsATICentroidHack;
}

bool CHardwareConfig::SupportsColorOnSecondStream() const
{
	return m_Caps.m_bColorOnSecondStream;
}

bool CHardwareConfig::SupportsStaticPlusDynamicLighting() const
{
	return GetDXSupportLevel() >= 80;
}

bool CHardwareConfig::PreferReducedFillrate() const
{
	return ShaderUtil()->GetConfig().ReduceFillrate();
}

// This is the max dx support level supported by the card
int CHardwareConfig::GetMaxDXSupportLevel() const
{
	return m_ActualCaps.m_nMaxDXSupportLevel;
}

bool CHardwareConfig::SpecifiesFogColorInLinearSpace() const
{
	return m_Caps.m_bFogColorSpecifiedInLinearSpace;
}

bool CHardwareConfig::SupportsSRGB() const
{
	return m_Caps.m_SupportsSRGB;
}

bool CHardwareConfig::FakeSRGBWrite() const
{
	return m_Caps.m_FakeSRGBWrite;
}

bool CHardwareConfig::CanDoSRGBReadFromRTs() const
{
	return m_Caps.m_CanDoSRGBReadFromRTs;
}

bool CHardwareConfig::SupportsGLMixedSizeTargets() const
{
	return m_Caps.m_bSupportsGLMixedSizeTargets;
}

bool CHardwareConfig::IsAAEnabled() const
{
	return g_pShaderDevice ? g_pShaderDevice->IsAAEnabled() : false;
//	bool bAntialiasing = ( m_PresentParameters.MultiSampleType != D3DMULTISAMPLE_NONE );
//	return bAntialiasing;
}

int CHardwareConfig::GetVertexTextureCount() const
{
	return m_Caps.m_nVertexTextureCount;
}

int CHardwareConfig::GetMaxVertexTextureDimension() const
{
	return m_Caps.m_nMaxVertexTextureDimension;
}

HDRType_t CHardwareConfig::GetHDRType() const
{
	bool enabled = m_bHDREnabled;
	int dxlev = GetDXSupportLevel();
	int dxsupp = dxlev >= 90;
	HDRType_t caps_hdr = m_Caps.m_HDRType;
	HDRType_t result = HDR_TYPE_NONE;
	
	//printf("\nCHardwareConfig::GetHDRType...");
	if (enabled)
	{
		//printf("-> enabled...");
		if (dxsupp)
		{
			//printf("-> supported...");
			result = caps_hdr;
		}
	}
	
	//printf("-> result is %d.\n", result);
	return result;

/*
	if ( m_bHDREnabled && ( GetDXSupportLevel() >= 90 ) )
		return m_Caps.m_HDRType;
	return HDR_TYPE_NONE;
*/
}

HDRType_t CHardwareConfig::GetHardwareHDRType() const
{
	return m_Caps.m_HDRType;
}

bool CHardwareConfig::SupportsStreamOffset() const
{
	return ( (GetDXSupportLevel() >= 90) && m_Caps.m_bSupportsStreamOffset );
}

int CHardwareConfig::StencilBufferBits() const
{
	return g_pShaderDevice ? g_pShaderDevice->StencilBufferBits() : 0;
}

int CHardwareConfig:: MaxViewports() const
{
	return m_Caps.m_nMaxViewports;
}

int CHardwareConfig::GetActualSamplerCount() const
{
	return m_ActualCaps.m_NumSamplers;
}

int CHardwareConfig::GetActualTextureStageCount() const
{
	return m_ActualCaps.m_NumTextureStages;
}

const char *CHardwareConfig::GetHWSpecificShaderDLLName()	const
{
	return m_Caps.m_pShaderDLL && m_Caps.m_pShaderDLL[0] ? m_Caps.m_pShaderDLL : NULL;
}

bool CHardwareConfig::SupportsMipmapping() const
{
	return m_Caps.m_SupportsMipmapping;
}

bool CHardwareConfig::ActuallySupportsPixelShaders_2_b() const
{
	return m_ActualCaps.m_SupportsPixelShaders_2_b;
}
