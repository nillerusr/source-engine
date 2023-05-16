//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "utlvector.h"
#include "materialsystem/imaterialsystem.h"
#include "shadersystem.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/imesh.h"
#include "tier0/dbg.h"
#include "materialsystem/idebugtextureinfo.h"
#include "materialsystem/deformations.h"
#include "meshgl.h"
#include "shaderapidevicegl.h"
#include "shaderapigl.h"
#include "shadershadowgl.h"

//-----------------------------------------------------------------------------
//
// Shader API GL
//
//-----------------------------------------------------------------------------

CShaderAPIGL::CShaderAPIGL()  : m_Mesh( false )
{
}

CShaderAPIGL::~CShaderAPIGL()
{
}


bool CShaderAPIGL::DoRenderTargetsNeedSeparateDepthBuffer() const
{
	return false;
}

// Can we download textures?
bool CShaderAPIGL::CanDownloadTextures() const
{
	return false;
}

// Used to clear the transition table when we know it's become invalid.
void CShaderAPIGL::ClearSnapshots()
{
}

// Members of IMaterialSystemHardwareConfig
bool CShaderAPIGL::HasDestAlphaBuffer() const
{
	return false;
}

bool CShaderAPIGL::HasStencilBuffer() const
{
	return false;
}

int CShaderAPIGL::MaxViewports() const
{
	return 1;
}

int CShaderAPIGL::GetShadowFilterMode() const
{
	return 0;
}

int CShaderAPIGL::StencilBufferBits() const
{
	return 0;
}

int	 CShaderAPIGL::GetFrameBufferColorDepth() const
{
	return 0;
}

int  CShaderAPIGL::GetSamplerCount() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 60))
		return 1;
	if (( ShaderUtil()->GetConfig().dxSupportLevel >= 60 ) && ( ShaderUtil()->GetConfig().dxSupportLevel < 80 ))
		return 2;
	return 4;
}

bool CShaderAPIGL::HasSetDeviceGammaRamp() const
{
	return false;
}

bool CShaderAPIGL::SupportsCompressedTextures() const
{
	return false;
}

VertexCompressionType_t CShaderAPIGL::SupportsCompressedVertices() const
{
	return VERTEX_COMPRESSION_NONE;
}

bool CShaderAPIGL::SupportsVertexAndPixelShaders() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 80))
		return false;

	return true;
}

bool CShaderAPIGL::SupportsPixelShaders_1_4() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 81))
		return false;

	return true;
}

bool CShaderAPIGL::SupportsPixelShaders_2_0() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 90))
		return false;

	return true;
}

bool CShaderAPIGL::SupportsPixelShaders_2_b() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 90))
		return false;

	return true;
}

bool CShaderAPIGL::ActuallySupportsPixelShaders_2_b() const
{
	return true;
}

bool CShaderAPIGL::SupportsShaderModel_3_0() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
		(ShaderUtil()->GetConfig().dxSupportLevel < 95))
		return false;

	return true;
}

bool CShaderAPIGL::SupportsStaticControlFlow() const
{
	if ( IsOpenGL() )
		return false;

	return SupportsVertexShaders_2_0();
}

bool CShaderAPIGL::SupportsVertexShaders_2_0() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 90))
		return false;

	return true;
}

int  CShaderAPIGL::MaximumAnisotropicLevel() const
{
	return 0;
}

void CShaderAPIGL::SetAnisotropicLevel( int nAnisotropyLevel )
{
}

int  CShaderAPIGL::MaxTextureWidth() const
{
	// Should be big enough to cover all cases
	return 16384;
}

int  CShaderAPIGL::MaxTextureHeight() const
{
	// Should be big enough to cover all cases
	return 16384;
}

int  CShaderAPIGL::MaxTextureAspectRatio() const
{
	// Should be big enough to cover all cases
	return 16384;
}


int	 CShaderAPIGL::TextureMemorySize() const
{
	// fake it
	return 64 * 1024 * 1024;
}

int  CShaderAPIGL::GetDXSupportLevel() const 
{ 
	return 90; 
}

bool CShaderAPIGL::SupportsOverbright() const
{
	return false;
}

bool CShaderAPIGL::SupportsCubeMaps() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 70))
		return false;

	return true;
}

bool CShaderAPIGL::SupportsNonPow2Textures() const
{
	return true;
}

bool CShaderAPIGL::SupportsMipmappedCubemaps() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 70))
		return false;

	return true;
}

int  CShaderAPIGL::GetTextureStageCount() const
{
	return 4;
}

int	 CShaderAPIGL::NumVertexShaderConstants() const
{
	return 128;
}

int	 CShaderAPIGL::NumBooleanVertexShaderConstants() const
{
	return 0;
}

int	 CShaderAPIGL::NumIntegerVertexShaderConstants() const
{
	return 0;
}

int	 CShaderAPIGL::NumPixelShaderConstants() const
{
	return 8;
}

int	 CShaderAPIGL::MaxNumLights() const
{
	return 4;
}

bool CShaderAPIGL::SupportsSpheremapping() const
{
	return false;
}


// This is the max dx support level supported by the card
int	CShaderAPIGL::GetMaxDXSupportLevel() const
{
	return 90;
}

bool CShaderAPIGL::SupportsHardwareLighting() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 70))
		return false;

	return true;
}

int	 CShaderAPIGL::MaxBlendMatrices() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 70))
	{
		return 1;
	}

	return 0;
}

int	 CShaderAPIGL::MaxBlendMatrixIndices() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 70))
	{
		return 1;
	}

	return 0;
}

int	 CShaderAPIGL::MaxVertexShaderBlendMatrices() const
{
	return 0;
}

int	CShaderAPIGL::MaxUserClipPlanes() const
{
	return 0;
}

bool CShaderAPIGL::SpecifiesFogColorInLinearSpace() const
{
	return false;
}

bool CShaderAPIGL::SupportsSRGB() const
{
	return false;
}

bool CShaderAPIGL::FakeSRGBWrite() const
{
	return false;
}

bool CShaderAPIGL::CanDoSRGBReadFromRTs() const
{
	return true;
}

bool CShaderAPIGL::SupportsGLMixedSizeTargets() const
{
	return false;
}

const char *CShaderAPIGL::GetHWSpecificShaderDLLName() const
{
	return 0;
}

// Sets the default *dynamic* state
void CShaderAPIGL::SetDefaultState()
{
}


// Returns the snapshot id for the shader state
StateSnapshot_t	 CShaderAPIGL::TakeSnapshot( )
{
	StateSnapshot_t id = 0;
	if (g_ShaderShadow.m_IsTranslucent)
		id |= TRANSLUCENT;
	if (g_ShaderShadow.m_IsAlphaTested)
		id |= ALPHATESTED;
	if (g_ShaderShadow.m_bUsesVertexAndPixelShaders)
		id |= VERTEX_AND_PIXEL_SHADERS;
	if (g_ShaderShadow.m_bIsDepthWriteEnabled)
		id |= DEPTHWRITE;
	return id;
}

// Returns true if the state snapshot is transparent
bool CShaderAPIGL::IsTranslucent( StateSnapshot_t id ) const
{
	return (id & TRANSLUCENT) != 0; 
}

bool CShaderAPIGL::IsAlphaTested( StateSnapshot_t id ) const
{
	return (id & ALPHATESTED) != 0; 
}

bool CShaderAPIGL::IsDepthWriteEnabled( StateSnapshot_t id ) const
{
	return (id & DEPTHWRITE) != 0; 
}

bool CShaderAPIGL::UsesVertexAndPixelShaders( StateSnapshot_t id ) const
{
	return (id & VERTEX_AND_PIXEL_SHADERS) != 0; 
}

// Gets the vertex format for a set of snapshot ids
VertexFormat_t CShaderAPIGL::ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds ) const
{
	return 0;
}

// Gets the vertex format for a set of snapshot ids
VertexFormat_t CShaderAPIGL::ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds ) const
{
	return 0;
}

// Uses a state snapshot
void CShaderAPIGL::UseSnapshot( StateSnapshot_t snapshot )
{
}

// Sets the color to modulate by
void CShaderAPIGL::Color3f( float r, float g, float b )
{
}

void CShaderAPIGL::Color3fv( float const* pColor )
{
}

void CShaderAPIGL::Color4f( float r, float g, float b, float a )
{
}

void CShaderAPIGL::Color4fv( float const* pColor )
{
}

// Faster versions of color
void CShaderAPIGL::Color3ub( unsigned char r, unsigned char g, unsigned char b )
{
}

void CShaderAPIGL::Color3ubv( unsigned char const* rgb )
{
}

void CShaderAPIGL::Color4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
}

void CShaderAPIGL::Color4ubv( unsigned char const* rgba )
{
}

// The shade mode
void CShaderAPIGL::ShadeMode( ShaderShadeMode_t mode )
{
}

// Binds a particular material to render with
void CShaderAPIGL::Bind( IMaterial* pMaterial )
{
}

// Cull mode
void CShaderAPIGL::CullMode( MaterialCullMode_t cullMode )
{
}

void CShaderAPIGL::ForceDepthFuncEquals( bool bEnable )
{
}

// Forces Z buffering on or off
void CShaderAPIGL::OverrideDepthEnable( bool bEnable, bool bDepthEnable )
{
}

void CShaderAPIGL::OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
{
}

void CShaderAPIGL::OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
{
}

//legacy fast clipping linkage
void CShaderAPIGL::SetHeightClipZ( float z )
{
}

void CShaderAPIGL::SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode )
{
}

// Sets the lights
void CShaderAPIGL::SetLight( int lightNum, const LightDesc_t& desc )
{
}

// Sets lighting origin for the current model
void CShaderAPIGL::SetLightingOrigin( Vector vLightingOrigin )
{
}

void CShaderAPIGL::SetAmbientLight( float r, float g, float b )
{
}

void CShaderAPIGL::SetAmbientLightCube( Vector4D cube[6] )
{
}

// Get lights
int CShaderAPIGL::GetMaxLights( void ) const
{
	return 0;
}

const LightDesc_t& CShaderAPIGL::GetLight( int lightNum ) const
{
	static LightDesc_t blah;
	return blah;
}

// Render state for the ambient light cube (vertex shaders)
void CShaderAPIGL::SetVertexShaderStateAmbientLightCube()
{
}

void CShaderAPIGL::SetSkinningMatrices()
{
}

// Lightmap texture binding
void CShaderAPIGL::BindLightmap( TextureStage_t stage )
{
}

void CShaderAPIGL::BindBumpLightmap( TextureStage_t stage )
{
}

void CShaderAPIGL::BindFullbrightLightmap( TextureStage_t stage )
{
}

void CShaderAPIGL::BindWhite( TextureStage_t stage )
{
}

void CShaderAPIGL::BindBlack( TextureStage_t stage )
{
}

void CShaderAPIGL::BindGrey( TextureStage_t stage )
{
}

// Gets the lightmap dimensions
void CShaderAPIGL::GetLightmapDimensions( int *w, int *h )
{
	g_pShaderUtil->GetLightmapDimensions( w, h );
}

// Special system flat normal map binding.
void CShaderAPIGL::BindFlatNormalMap( TextureStage_t stage )
{
}

void CShaderAPIGL::BindNormalizationCubeMap( TextureStage_t stage )
{
}

void CShaderAPIGL::BindSignedNormalizationCubeMap( TextureStage_t stage )
{
}

void CShaderAPIGL::BindFBTexture( TextureStage_t stage, int textureIndex )
{
}

// Flushes any primitives that are buffered
void CShaderAPIGL::FlushBufferedPrimitives()
{
}

// Gets the dynamic mesh; note that you've got to render the mesh
// before calling this function a second time. Clients should *not*
// call DestroyStaticMesh on the mesh returned by this call.
IMesh* CShaderAPIGL::GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	return &m_Mesh;
}

IMesh* CShaderAPIGL::GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t fmt, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	return &m_Mesh;
}

IMesh* CShaderAPIGL::GetFlexMesh()
{
	return &m_Mesh;
}

// Begins a rendering pass that uses a state snapshot
void CShaderAPIGL::BeginPass( StateSnapshot_t snapshot  )
{
}

// Renders a single pass of a material
void CShaderAPIGL::RenderPass( int nPass, int nPassCount )
{
}

// stuff related to matrix stacks
void CShaderAPIGL::MatrixMode( MaterialMatrixMode_t matrixMode )
{
}

void CShaderAPIGL::PushMatrix()
{
}

void CShaderAPIGL::PopMatrix()
{
}

void CShaderAPIGL::LoadMatrix( float *m )
{
}

void CShaderAPIGL::MultMatrix( float *m )
{
}

void CShaderAPIGL::MultMatrixLocal( float *m )
{
}

void CShaderAPIGL::GetMatrix( MaterialMatrixMode_t matrixMode, float *dst )
{
}

void CShaderAPIGL::LoadIdentity( void )
{
}

void CShaderAPIGL::LoadCameraToWorld( void )
{
}

void CShaderAPIGL::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
}

void CShaderAPIGL::PerspectiveX( double fovx, double aspect, double zNear, double zFar )
{
}

void CShaderAPIGL::PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right )
{
}

void CShaderAPIGL::PickMatrix( int x, int y, int width, int height )
{
}

void CShaderAPIGL::Rotate( float angle, float x, float y, float z )
{
}

void CShaderAPIGL::Translate( float x, float y, float z )
{
}

void CShaderAPIGL::Scale( float x, float y, float z )
{
}

void CShaderAPIGL::ScaleXY( float x, float y )
{
}

// Fog methods...
void CShaderAPIGL::FogMode( MaterialFogMode_t fogMode )
{
}

void CShaderAPIGL::FogStart( float fStart )
{
}

void CShaderAPIGL::FogEnd( float fEnd )
{
}

void CShaderAPIGL::SetFogZ( float fogZ )
{
}
	
void CShaderAPIGL::FogMaxDensity( float flMaxDensity )
{
}

void CShaderAPIGL::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{
}


void CShaderAPIGL::SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
}


void CShaderAPIGL::SceneFogMode( MaterialFogMode_t fogMode )
{
}

void CShaderAPIGL::GetSceneFogColor( unsigned char *rgb )
{
}

MaterialFogMode_t CShaderAPIGL::GetSceneFogMode( )
{
	return MATERIAL_FOG_NONE;
}

int CShaderAPIGL::GetPixelFogCombo( )
{
	return 0;
}

void CShaderAPIGL::FogColor3f( float r, float g, float b )
{
}

void CShaderAPIGL::FogColor3fv( float const* rgb )
{
}

void CShaderAPIGL::FogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
}

void CShaderAPIGL::FogColor3ubv( unsigned char const* rgb )
{
}

void CShaderAPIGL::SetViewports( int nCount, const ShaderViewport_t* pViewports )
{
}

int CShaderAPIGL::GetViewports( ShaderViewport_t* pViewports, int nMax ) const
{
	return 1;
}

// Sets the vertex and pixel shaders
void CShaderAPIGL::SetVertexShaderIndex( int vshIndex )
{
}

void CShaderAPIGL::SetPixelShaderIndex( int pshIndex )
{
}

// Sets the constant registers for vertex and pixel shaders
void CShaderAPIGL::SetVertexShaderConstant( int var, float const* pVec, int numConst, bool bForce )
{
}

void CShaderAPIGL::SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numConst, bool bForce )
{
}

void CShaderAPIGL::SetIntegerVertexShaderConstant( int var, int const* pVec, int numConst, bool bForce )
{
}

void CShaderAPIGL::SetPixelShaderConstant( int var, float const* pVec, int numConst, bool bForce )
{
}

void CShaderAPIGL::SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools, bool bForce )
{
}

void CShaderAPIGL::SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs, bool bForce )
{
}

void CShaderAPIGL::InvalidateDelayedShaderConstants( void )
{
}

float CShaderAPIGL::GammaToLinear_HardwareSpecific( float fGamma ) const
{
	return 0.0f;
}

float CShaderAPIGL::LinearToGamma_HardwareSpecific( float fLinear ) const
{
	return 0.0f;
}

void CShaderAPIGL::SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture )
{
}


// Returns the nearest supported format
ImageFormat CShaderAPIGL::GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired /* = true */ ) const
{
	return fmt;
}

ImageFormat CShaderAPIGL::GetNearestRenderTargetFormat( ImageFormat fmt ) const
{
	return fmt;
}

// Sets the texture state
void CShaderAPIGL::BindTexture( Sampler_t stage, ShaderAPITextureHandle_t textureHandle )
{
}

void CShaderAPIGL::ClearColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
}

void CShaderAPIGL::ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
}

// Indicates we're going to be modifying this texture
// TexImage2D, TexSubImage2D, TexWrap, TexMinFilter, and TexMagFilter
// all use the texture specified by this function.
void CShaderAPIGL::ModifyTexture( ShaderAPITextureHandle_t textureHandle )
{
}

// Texture management methods
void CShaderAPIGL::TexImage2D( int level, int cubeFace, ImageFormat dstFormat, int zOffset, int width, int height, 
						 ImageFormat srcFormat, bool bSrcIsTiled, void *imageData )
{
}

void CShaderAPIGL::TexSubImage2D( int level, int cubeFace, int xOffset, int yOffset, int zOffset, int width, int height,
						 ImageFormat srcFormat, int srcStride, bool bSrcIsTiled, void *imageData )
{
}

void CShaderAPIGL::TexImageFromVTF( IVTFTexture *pVTF, int iVTFFrame )
{
}

bool CShaderAPIGL::TexLock( int level, int cubeFaceID, int xOffset, int yOffset, 
								int width, int height, CPixelWriter& writer )
{
	return false;
}

void CShaderAPIGL::TexUnlock( )
{
}


// These are bound to the texture, not the texture environment
void CShaderAPIGL::TexMinFilter( ShaderTexFilterMode_t texFilterMode )
{
}

void CShaderAPIGL::TexMagFilter( ShaderTexFilterMode_t texFilterMode )
{
}

void CShaderAPIGL::TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode )
{
}

void CShaderAPIGL::TexSetPriority( int priority )
{
}

ShaderAPITextureHandle_t CShaderAPIGL::CreateTexture( 
	int width, 
	int height,
	int depth,
	ImageFormat dstImageFormat, 
	int numMipLevels, 
	int numCopies, 
	int flags, 
	const char *pDebugName,
	const char *pTextureGroupName )
{
	return 0;
}

// Create a multi-frame texture (equivalent to calling "CreateTexture" multiple times, but more efficient)
void CShaderAPIGL::CreateTextures( 
							ShaderAPITextureHandle_t *pHandles,
							int count,
							int width, 
							int height,
							int depth,
							ImageFormat dstImageFormat, 
							int numMipLevels, 
							int numCopies, 
							int flags, 
							const char *pDebugName,
							const char *pTextureGroupName )
{
	for ( int k = 0; k < count; ++ k )
		pHandles[ k ] = 0;
}


ShaderAPITextureHandle_t CShaderAPIGL::CreateDepthTexture( ImageFormat renderFormat, int width, int height, const char *pDebugName, bool bTexture )
{
	return 0;
}

void CShaderAPIGL::DeleteTexture( ShaderAPITextureHandle_t textureHandle )
{
}

bool CShaderAPIGL::IsTexture( ShaderAPITextureHandle_t textureHandle )
{
	return true;
}

bool CShaderAPIGL::IsTextureResident( ShaderAPITextureHandle_t textureHandle )
{
	return false;
}

// stuff that isn't to be used from within a shader
void CShaderAPIGL::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight )
{
}

void CShaderAPIGL::ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth )
{
}

void CShaderAPIGL::ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
{
}

void CShaderAPIGL::PerformFullScreenStencilOperation( void )
{
}

void CShaderAPIGL::SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor )
{
}

void CShaderAPIGL::ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat )
{
}

void CShaderAPIGL::ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride )
{
}

void CShaderAPIGL::FlushHardware()
{
}

void CShaderAPIGL::ResetRenderState( bool bFullReset )
{
}

// Set the number of bone weights
void CShaderAPIGL::SetNumBoneWeights( int numBones )
{
}

void CShaderAPIGL::EnableHWMorphing( bool bEnable )
{
}

// Selection mode methods
int CShaderAPIGL::SelectionMode( bool selectionMode )
{
	return 0;
}

void CShaderAPIGL::SelectionBuffer( unsigned int* pBuffer, int size )
{
}

void CShaderAPIGL::ClearSelectionNames( )
{
}

void CShaderAPIGL::LoadSelectionName( int name )
{
}

void CShaderAPIGL::PushSelectionName( int name )
{
}

void CShaderAPIGL::PopSelectionName()
{
}


// Use this to get the mesh builder that allows us to modify vertex data
CMeshBuilder* CShaderAPIGL::GetVertexModifyBuilder()
{
	return 0;
}

// Board-independent calls, here to unify how shaders set state
// Implementations should chain back to IShaderUtil->BindTexture(), etc.

// Use this to begin and end the frame
void CShaderAPIGL::BeginFrame()
{
}

void CShaderAPIGL::EndFrame()
{
}

// returns the current time in seconds....
double CShaderAPIGL::CurrentTime() const
{
	return Sys_FloatTime();
}

// Get the current camera position in world space.
void CShaderAPIGL::GetWorldSpaceCameraPosition( float * pPos ) const
{
}

void CShaderAPIGL::ForceHardwareSync( void )
{
}

void CShaderAPIGL::SetClipPlane( int index, const float *pPlane )
{
}

void CShaderAPIGL::EnableClipPlane( int index, bool bEnable )
{
}

void CShaderAPIGL::SetFastClipPlane( const float *pPlane )
{
}

void CShaderAPIGL::EnableFastClip( bool bEnable )
{
}

int CShaderAPIGL::GetCurrentNumBones( void ) const
{
	return 0;
}

bool CShaderAPIGL::IsHWMorphingEnabled( void ) const
{
	return false;
}

int CShaderAPIGL::GetCurrentLightCombo( void ) const
{
	return 0;
}

void CShaderAPIGL::GetDX9LightState( LightState_t *state ) const
{
	state->m_nNumLights = 0;
	state->m_bAmbientLight = false;
	state->m_bStaticLightVertex = false;
	state->m_bStaticLightTexel = false;
}

MaterialFogMode_t CShaderAPIGL::GetCurrentFogType( void ) const
{
	return MATERIAL_FOG_NONE;
}

void CShaderAPIGL::RecordString( const char *pStr )
{
}

bool CShaderAPIGL::ReadPixelsFromFrontBuffer() const
{
	return true;
}

bool CShaderAPIGL::PreferDynamicTextures() const
{
	return false;
}

bool CShaderAPIGL::PreferReducedFillrate() const
{ 
	return false; 
}

bool CShaderAPIGL::HasProjectedBumpEnv() const
{
	return true;
}

int  CShaderAPIGL::GetCurrentDynamicVBSize( void )
{
	return 0;
}

void CShaderAPIGL::DestroyVertexBuffers( bool bExitingLevel )
{
}

void CShaderAPIGL::EvictManagedResources()
{
}

void CShaderAPIGL::SetTextureTransformDimension( TextureStage_t textureStage, int dimension, bool projected )
{
}

void CShaderAPIGL::SetBumpEnvMatrix( TextureStage_t textureStage, float m00, float m01, float m10, float m11 )
{
}

void CShaderAPIGL::SyncToken( const char *pToken )
{
}

void CShaderAPIGL::GetBackBufferDimensions( int& width, int& height ) const
{
	s_ShaderDeviceGL.GetBackBufferDimensions( width, height );
}

void CShaderAPIGL::GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
{
	pInfo->m_bIsEnabled = false;
	pInfo->m_nLookupCount = 0;
	pInfo->m_flDefaultWeight = 0.0f;
}
