//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "materialsystem/imesh.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imorph.h"
#include "materialsystem/imaterialsystemstub.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialvar.h"
#include "bitmap/imageformat.h"
#include "mathlib/vmatrix.h"
#include "utlvector.h"
// GR
#include "imaterialinternal.h"
#include "materialsystem/materialsystem_config.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ---------------------------------------------------------------------------------------- //
// IMaterialSystem and IMesh stub classes.
// ---------------------------------------------------------------------------------------- //
static unsigned short g_DummyIndices[6];

class CDummyMesh : public IMesh
{
public:
	// Locks/ unlocks the mesh, providing space for nVertexCount and nIndexCount.
	// nIndexCount of -1 means don't lock the index buffer...
	virtual void LockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc )
	{
		Lock( nVertexCount, false, *static_cast< VertexDesc_t* >( &desc ) );
		Lock( nIndexCount, false, *static_cast< IndexDesc_t* >( &desc ) );
	}

	virtual void UnlockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc )
	{
	}

	virtual void ModifyBeginEx( bool bReadOnly, int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc )
	{
		ModifyBegin( bReadOnly, nFirstIndex, nIndexCount, *static_cast< IndexDesc_t* >( &desc ) );
	}

	virtual void ModifyBegin( int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc )
	{
		ModifyBegin( false, nFirstIndex, nIndexCount, *static_cast< IndexDesc_t* >( &desc ) );
	}

	virtual void ModifyEnd( MeshDesc_t &desc )
	{
	}

	// FIXME: Make this work! Unsupported methods of IIndexBuffer
	virtual bool Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t& desc ) 
	{
		desc.m_pIndices = g_DummyIndices;
		desc.m_nIndexSize = 0;
		desc.m_nFirstIndex = 0;
		desc.m_nOffset = 0;
		return true;
	}

	virtual void Unlock( int nWrittenIndexCount, IndexDesc_t& desc ) {}

	virtual void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc ) 
	{
		desc.m_pIndices = g_DummyIndices;
		desc.m_nIndexSize = 0;
		desc.m_nFirstIndex = 0;
		desc.m_nOffset = 0;
	}

	virtual void ModifyEnd( IndexDesc_t &desc ) {}
	virtual void Spew( int nIndexCount, const IndexDesc_t & desc ) {} 
	virtual void ValidateData( int nIndexCount, const IndexDesc_t &desc ) {}
	virtual bool IsDynamic() const 
	{
		return false;
	}

	virtual void BeginCastBuffer( MaterialIndexFormat_t format ) {}
	virtual void BeginCastBuffer( VertexFormat_t format ) {}
	virtual void EndCastBuffer( ) {}

	// Returns the number of vertices that can still be written into the buffer
	virtual int GetRoomRemaining() const { return 0; }

	virtual int IndexCount() const
	{
		return 0;
	}

	// returns the # of vertices (static meshes only)
	virtual int VertexCount() const
	{
		return 0;
	}

	// returns the vertex format
	virtual bool Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc )
	{
		static float dummyFloat[32];
		static unsigned char dummyChar[32];

		memset( &desc, 0, sizeof( desc ) );

		// Pointers to our current vertex data
		desc.m_pPosition = dummyFloat;
		desc.m_pBoneWeight = dummyFloat;
#ifdef NEW_SKINNING
		desc.m_pBoneMatrixIndex = dummyFloat;
#else
		desc.m_pBoneMatrixIndex = dummyChar;
#endif

		desc.m_pNormal = dummyFloat;
		desc.m_pColor = dummyChar;
		desc.m_pSpecular = dummyChar;
		for( int i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; i++ )
		{
			desc.m_pTexCoord[i] = dummyFloat;
		}

		desc.m_pTangentS = dummyFloat;
		desc.m_pTangentT = dummyFloat;
		desc.m_pWrinkle = dummyFloat;

		// user data
		desc.m_pUserData = dummyFloat;
		desc.m_nFirstVertex = 0;
		desc.m_nOffset = 0;
		return true;
	}

	virtual void Unlock( int nVertexCount, VertexDesc_t &desc ) {}
	virtual void Spew( int nVertexCount, const VertexDesc_t &desc ) {}
	virtual void ValidateData( int nVertexCount, const VertexDesc_t & desc ) {}

	// Sets/gets the primitive type
	virtual void SetPrimitiveType( MaterialPrimitiveType_t type )
	{
	}
	
	// Draws the mesh
	virtual void Draw( int nFirstIndex = -1, int nIndexCount = 0 )
	{
	}
	
	virtual void SetColorMesh( IMesh *pColorMesh, int nVertexOffset )
	{
	}

	virtual void SetFlexMesh( IMesh *pMesh, int nVertexOffset )
	{
	}

	virtual void DisableFlexMesh( )
	{
	}

	// Draw a list of (lists of) primitives. Batching your lists together that use
	// the same lightmap, material, vertex and index buffers with multipass shaders
	// can drastically reduce state-switching overhead.
	// NOTE: this only works with STATIC meshes.
	virtual void Draw( CPrimList *pLists, int nLists )
	{
	}

	// Copy verts and/or indices to a mesh builder. This only works for temp meshes!
	virtual void CopyToMeshBuilder( 
		int iStartVert,		// Which vertices to copy.
		int nVerts, 
		int iStartIndex,	// Which indices to copy.
		int nIndices, 
		int indexOffset,	// This is added to each index.
		CMeshBuilder &builder )
	{
	}

	// Spews the mesh data
	virtual void Spew( int nVertexCount, int nIndexCount, const MeshDesc_t& desc )
	{
	}

	// Call this in debug mode to make sure our data is good.
	virtual void ValidateData( int nVertexCount, int nIndexCount, const MeshDesc_t& desc )
	{
	}

	virtual void MarkAsDrawn() {}

	virtual unsigned ComputeMemoryUsed() { return 0; }

	virtual VertexFormat_t GetVertexFormat() const	
	{	
		return VERTEX_POSITION;
	}
	virtual MaterialIndexFormat_t IndexFormat() const	
	{	
		return MATERIAL_INDEX_FORMAT_16BIT;
	}
	virtual IMesh *GetMesh()
	{
		return this;
	}
};

// We allocate this dynamically because it uses a bunch of memory and we don't want to
// waste the memory unless we need to.
CDummyMesh *g_pDummyMesh = NULL;
CDummyMesh* GetDummyMesh()
{
	if ( !g_pDummyMesh )
	{
		g_pDummyMesh = new CDummyMesh;
	}

	return g_pDummyMesh;
}


// ---------------------------------------------------------------------------------------- //
// ITexture dummy implementation.
// ---------------------------------------------------------------------------------------- //

class CDummyTexture : public ITexture
{
public:
	// Various texture polling methods
	virtual const char *GetName( void ) const { return "DummyTexture"; }
	virtual int GetMappingWidth() const { return 512; }
	virtual int GetMappingHeight() const { return 512; }
	virtual int GetActualWidth() const { return 512; }
	virtual int GetActualHeight() const { return 512; }
	virtual int GetNumAnimationFrames() const { return 0; }
	virtual bool IsTranslucent() const { return false; }
	virtual bool IsMipmapped() const { return false; }

	virtual void GetLowResColorSample( float s, float t, float *color ) const {}

	// Gets texture resource data of the specified type.
	// Params:
	//		eDataType		type of resource to retrieve.
	//		pnumBytes		on return is the number of bytes available in the read-only data buffer or is undefined
	// Returns:
	//		pointer to the resource data, or NULL
	virtual void *GetResourceData( uint32 eDataType, size_t *pNumBytes ) const
	{
		return NULL;
	}


	// Methods associated with reference count
	virtual void IncrementReferenceCount( void ) {}
	virtual void DecrementReferenceCount( void ) {}

	// Used to modify the texture bits (procedural textures only)
	virtual void SetTextureRegenerator( ITextureRegenerator *pTextureRegen ) {}

	// Reconstruct the texture bits in HW memory

	// If rect is not specified, reconstruct all bits, otherwise just
	// reconstruct a subrect.
	virtual void Download( Rect_t *pRect = 0, int nAdditionalCreationFlags = 0 ) {}

	// Uses for stats. . .get the approximate size of the texture in it's current format.
	virtual int GetApproximateVidMemBytes( void ) const { return 64; }

	virtual bool IsError() const { return false; }

	virtual ITexture *GetEmbeddedTexture( int nIndex ) { return NULL; }

	// For volume textures
	virtual bool IsVolumeTexture() const { return false; }
	virtual int GetMappingDepth() const { return 1; }
	virtual int GetActualDepth() const { return 1; }

	virtual ImageFormat GetImageFormat() const { return IMAGE_FORMAT_RGBA8888; }
	virtual NormalDecodeMode_t GetNormalDecodeMode() const { return NORMAL_DECODE_NONE; }

	// Various information about the texture
	virtual bool IsRenderTarget() const { return false; }
	virtual bool IsCubeMap() const { return false; }
	virtual bool IsNormalMap() const { return false; }
	virtual bool IsProcedural() const { return false; }
	virtual void DeleteIfUnreferenced() {}

	virtual void SwapContents( ITexture *pOther ) {}

	virtual unsigned int GetFlags( void ) const { return 0; }
	virtual void ForceLODOverride( int iNumLodsOverrideUpOrDown ) { NULL; }

#if defined( _X360 )
	virtual bool ClearTexture( int r, int g, int b, int a ) { return true; }
	virtual bool CreateRenderTargetSurface( int width, int height, ImageFormat format, bool bSameAsTexture ) { return true; }
#endif

	// Save texture to a file.
	virtual bool SaveToFile( const char *fileName ) { return false; }

	void CopyToStagingTexture( ITexture* pDstTex ) {}

	virtual void SetErrorTexture( bool bIsErrorTexture ) { }
};

CDummyTexture g_DummyTexture;


// ---------------------------------------------------------------------------------------- //
// Dummy implementation of IMaterialVar.
// ---------------------------------------------------------------------------------------- //
static VMatrix g_DummyMatrix( 1,0,0,0,   0,1,0,0,   0,0,1,0,   0,0,0,1 );

class CDummyMaterialVar : public IMaterialVar
{
public:
	virtual char const *	GetName( void ) const { return "DummyMaterialVar"; }
	virtual MaterialVarSym_t	GetNameAsSymbol() const { return 0; }

	virtual void			SetFloatValue( float val ) {}
	virtual float			GetFloatValueInternal( void ) const { return 1; }
	
	virtual void			SetIntValue( int val ) {}
	virtual int				GetIntValueInternal( void ) const { return 1; }
	
	virtual void			SetStringValue( char const *val ) {}
	virtual char const *	GetStringValue( void ) const { return ""; }

	// Use FourCC values to pass app-defined data structures between
	// the proxy and the shader. The shader should ignore the data if
	// its FourCC type not correct.	
	virtual void			SetFourCCValue( FourCC type, void *pData ) {}
	virtual void			GetFourCCValue( FourCC *type, void **ppData ) {}

	// Vec (dim 2-4)
	virtual void			SetVecValue( float const* val, int numcomps ) {}
	virtual void			SetVecValue( float x, float y ) {}
	virtual void			SetVecValue( float x, float y, float z ) {}
	virtual void			SetVecValue( float x, float y, float z, float w ) {}
	virtual void			SetVecComponentValue( float fVal, int nComponent ) {}
	virtual void			GetVecValueInternal( float *val, int numcomps ) const
	{
		for ( int i=0; i < numcomps; i++ )
			val[i] = 1;
	}
	virtual void			GetLinearVecValue( float *val, int numcomps ) const
	{
		for ( int i=0; i < numcomps; i++ )
			val[i] = 1;
	}

	virtual float const*	GetVecValueInternal( ) const
	{
		static float val[4] = {1,1,1,1};
		return val;
	}

	virtual int				VectorSizeInternal() const
	{
		return 3;
	}

	// revisit: is this a good interface for textures?
	virtual ITexture *		GetTextureValue( void )
	{
		return &g_DummyTexture;
	}

	virtual void			SetTextureValue( ITexture * ) {}
	virtual					operator ITexture*()
	{
		return GetTextureValue();
	}

	virtual IMaterial *		GetMaterialValue( void )
	{
		extern IMaterial *g_pDummyMaterial;
		return g_pDummyMaterial;
	}

	virtual void			SetMaterialValue( IMaterial * ) {}

	virtual MaterialVarType_t	GetType() const { return MATERIAL_VAR_TYPE_INT; }
	virtual bool			IsDefined() const { return true; }
	virtual void			SetUndefined() {}

	// Matrix
	virtual void			SetMatrixValue( VMatrix const& matrix ) {}
	virtual const VMatrix  &GetMatrixValue( )
	{
		return g_DummyMatrix;
	}
	virtual bool			MatrixIsIdentity() const
	{
		return false;
	}

	// Copy....
	virtual void			CopyFrom( IMaterialVar *pMaterialVar ) {}

	virtual void			SetValueAutodetectType( char const *val ) {}

	virtual IMaterial *		GetOwningMaterial()
	{
		extern IMaterial *g_pDummyMaterial;
		return g_pDummyMaterial;
	}
};
CDummyMaterialVar g_DummyMaterialVar;


// ---------------------------------------------------------------------------------------- //
// Dummy implementation of IMaterialSystemHardwareConfig
// ---------------------------------------------------------------------------------------- //

class CDummyHardwareConfig : public IMaterialSystemHardwareConfig
{
public:
	virtual bool HasDestAlphaBuffer() const			{ return false; }
	virtual bool HasStencilBuffer() const			{ return false; }
	virtual int  StencilBufferBits() const			{ return 0;		}
	virtual int	 GetFrameBufferColorDepth() const	{ return 0; }
	virtual int  GetSamplerCount() const			{ return 0; }
	virtual bool HasSetDeviceGammaRamp() const		{ return false; }
	virtual bool SupportsCompressedTextures() const	{ return false; }
	virtual VertexCompressionType_t SupportsCompressedVertices() const { return VERTEX_COMPRESSION_NONE; }
	virtual bool SupportsVertexAndPixelShaders() const	{ return false; }
	virtual bool SupportsPixelShaders_1_4() const	{ return false; }
	virtual bool SupportsPixelShaders_2_0() const	{ return false; }
	virtual bool SupportsPixelShaders_2_b() const	{ return false; }
	virtual bool ActuallySupportsPixelShaders_2_b() const	{ return false; }
	virtual bool SupportsStaticControlFlow() const { return false; }
	virtual bool SupportsVertexShaders_2_0() const	{ return false; }
	virtual bool SupportsShaderModel_3_0() const	{ return false; }
	virtual int  MaximumAnisotropicLevel() const	{ return 1; }
	virtual int  MaxTextureWidth() const			{ return 0; }
	virtual int  MaxTextureHeight() const			{ return 0; }
	virtual int  MaxTextureDepth() const			{ return 0; }
	virtual int	 TextureMemorySize() const			{ return 0; }
	virtual bool SupportsOverbright() const			{ return false; }
	virtual bool SupportsCubeMaps() const			{ return false; }
	virtual bool SupportsMipmappedCubemaps() const	{ return false; }
	virtual bool SupportsNonPow2Textures() const	{ return false; }

	// The number of texture stages represents the number of computations
	// we can do in the pixel pipeline, it is *not* related to the
	// simultaneous number of textures we can use
	virtual int  GetTextureStageCount() const		{ return 0; }
	virtual int	 NumVertexShaderConstants() const	{ return 0; }
	virtual int	 NumBooleanVertexShaderConstants() const	{ return 0; }
	virtual int	 NumIntegerVertexShaderConstants() const	{ return 0; }
	virtual int	 NumPixelShaderConstants() const	{ return 0; }
	virtual int	 MaxNumLights() const				{ return 0; }
	virtual bool SupportsHardwareLighting() const	{ return false; }
	virtual int	 MaxBlendMatrices() const			{ return 0; }
	virtual int	 MaxBlendMatrixIndices() const		{ return 0; }
	virtual int	 MaxTextureAspectRatio() const		{ return 0; }
	virtual int	 MaxVertexShaderBlendMatrices() const	{ return 0; }
	virtual int	 MaxUserClipPlanes() const			{ return 0; }
	virtual bool UseFastClipping() const			{ return false; }
	virtual bool UseFastZReject() const				{ return false; }
	virtual bool PreferReducedFillrate() const		{ return false; }

	// This here should be the major item looked at when checking for compat
	// from anywhere other than the material system	shaders
	virtual int	 GetDXSupportLevel() const			{ return 90; }
	virtual const char *GetShaderDLLName() const	{ return NULL; }

	virtual bool ReadPixelsFromFrontBuffer() const	{ return false; }

	// Are dx dynamic textures preferred?
	virtual bool PreferDynamicTextures() const		{ return false; }

	virtual bool SupportsHDR() const				{ return false; }
	virtual HDRType_t GetHDRType() const			{ return HDR_TYPE_NONE; }
	virtual HDRType_t GetHardwareHDRType() const	{ return HDR_TYPE_NONE; }

	virtual bool HasProjectedBumpEnv() const		{ return false; }
	virtual bool SupportsSpheremapping() const		{ return false; }
	virtual bool NeedsAAClamp() const				{ return false; }
	virtual bool HasFastZReject() const				{ return false; }
	virtual bool NeedsATICentroidHack() const		{ return false; }
	virtual bool SupportsColorOnSecondStream() const{ return false; }
	virtual bool SupportsStaticPlusDynamicLighting() const{ return false; }
	virtual bool SupportsStreamOffset() const		{ return false; }
	virtual int	 GetMaxDXSupportLevel() const		{ return 90; }
	virtual bool SpecifiesFogColorInLinearSpace() const { return false; }
	virtual bool SupportsSRGB() const				{ return false; }
	virtual bool FakeSRGBWrite() const				{ return false; }
	virtual bool CanDoSRGBReadFromRTs() const		{ return true; }
	virtual bool SupportsGLMixedSizeTargets() const	{ return false; }
	virtual bool IsAAEnabled() const				{ return false; }
	virtual int GetVertexTextureCount() const		{ return 0; }
	virtual int GetMaxVertexTextureDimension() const { return 0; }
	virtual int MaxViewports() const { return 1; }
	virtual void OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport ) {}
	virtual int GetShadowFilterMode() const { return 0; }
	virtual int NeedsShaderSRGBConversion() const { return 0; }
	bool UsesSRGBCorrectBlending() const { return false; }
	virtual bool HasFastVertexTextures() const { return false; }
	virtual int MaxHWMorphBatchCount() const { return 0; }
	virtual bool SupportsHDRMode( HDRType_t nMode ) const { return 0; }
	virtual bool IsDX10Card() const { return 0; }
	virtual bool GetHDREnabled( void ) const { return true; }
	virtual void SetHDREnabled( bool bEnable ) {}
	virtual bool SupportsBorderColor( void ) const { return true; }
	virtual bool SupportsFetch4( void ) const { return false; }
	virtual bool CanStretchRectFromTextures() const { return false; }
};
CDummyHardwareConfig g_DummyHardwareConfig;


// ---------------------------------------------------------------------------------------- //
// CDummyMaterial.
// ---------------------------------------------------------------------------------------- //

class CDummyMaterial : public IMaterial
{
public:
	virtual const char *	GetName() const { return "dummy material"; }
	virtual const char *	GetTextureGroupName() const { return "dummy group"; }

	virtual PreviewImageRetVal_t GetPreviewImageProperties( int *width, int *height, 
				 			ImageFormat *imageFormat, bool* isTranslucent ) const
	{
		if ( width )
			*width = 4;

		if ( height )
			*height = 4;

		if ( imageFormat )
			*imageFormat = IMAGE_FORMAT_RGBA8888;

		if ( isTranslucent )
			*isTranslucent = false;

		return MATERIAL_PREVIEW_IMAGE_OK;
	}
	
	virtual PreviewImageRetVal_t GetPreviewImage( unsigned char *data, 
												 int width, int height,
												 ImageFormat imageFormat ) const
	{
		return MATERIAL_PREVIEW_IMAGE_OK;
	}
											    
	// 
	virtual int				GetMappingWidth( )
	{
		return 512;
	}

	virtual int				GetMappingHeight( )
	{
		return 512;
	}

	virtual int				GetNumAnimationFrames( )
	{
		return 0;
	}

	virtual bool			InMaterialPage( void ) 
	{
		return false;
	}

	virtual void			GetMaterialOffset( float *pOffset  )
	{
		pOffset[0] = 0.0f;
		pOffset[1] = 0.0f;
	}

	virtual void			GetMaterialScale( float *pScale )
	{
		pScale[0] = 1.0f;
		pScale[1] = 1.0f;
	}

	virtual IMaterial			*GetMaterialPage( void )
	{
		return NULL;
	}

	virtual IMaterialVar *	FindVar( const char *varName, bool *found, bool complain = true )
	{
		if ( found )
			*found = true;

		return &g_DummyMaterialVar;
	}
	virtual IMaterialVar *	FindVarFast( const char *varName, unsigned int *pToken )
	{
		return NULL;
	}

	virtual void			IncrementReferenceCount( void )
	{
	}

	virtual void			DecrementReferenceCount( void )
	{
	}

	virtual int 			GetEnumerationID( void ) const
	{
		return 0;
	}

	virtual void			GetLowResColorSample( float s, float t, float *color ) const
	{
	}

	virtual void			RecomputeStateSnapshots()
	{
	}


	// Are we translucent?
	virtual bool			IsTranslucent()
	{
		return false;
	}

	// Are we alphatested?
	virtual bool			IsAlphaTested()
	{
		return false;
	}

	// Are we vertex lit?
	virtual bool			IsVertexLit()
	{
		return false;
	}

	// Gets the vertex format
	virtual VertexFormat_t	GetVertexFormat() const
	{
		return 0;
	}

	// returns true if this material uses a material proxy
	virtual bool			HasProxy( void ) const
	{
		return false;
	}
	virtual void			CallBindProxy( void* ) {}

	virtual IMaterial		*CheckProxyReplacement( void *proxyData ) 
	{ 
		return this; 
	}

	virtual bool			UsesEnvCubemap( void )
	{
		return false;
	}

	virtual bool			NeedsTangentSpace( void )
	{
		return false;
	}

	virtual bool			NeedsPowerOfTwoFrameBufferTexture( bool bCheckSpecificToThisFrame )
	{
		return false;
	}

	virtual bool			NeedsFullFrameBufferTexture( bool bCheckSpecificToThisFrame )
	{
		return false;
	}

	virtual bool			NeedsSoftwareSkinning( void )
	{
		return false;
	}

	// Apply constant color or alpha modulation
	virtual void			AlphaModulate( float alpha )
	{
	}

	virtual void			ColorModulate( float r, float g, float b )
	{
	}

	float					GetAlphaModulation( )
	{ 
		return 1;
	}

	void					GetColorModulation( float *r, float *g, float *b )
	{ 
		*r = *g = *b = 1;
	}

	// Material Var flags...
	virtual void			SetMaterialVarFlag( MaterialVarFlags_t flag, bool on )
	{
	}

	virtual bool			GetMaterialVarFlag( MaterialVarFlags_t flag ) const
	{
		return true;
	}


	// Gets material reflectivity
	virtual void			GetReflectivity( Vector& reflect )
	{
		reflect.Init(1,0,0);
	}


	// Gets material property flags
	virtual bool			GetPropertyFlag( MaterialPropertyTypes_t type )
	{
		return true;
	}

	// Is the material visible from both sides?
	virtual bool			IsTwoSided()
	{
		return false;
	}

	// Sets the shader associated with the material
	virtual void			SetShader( const char *pShaderName )
	{
	}

	// Can't be const because the material might have to precache itself.
	virtual int				GetNumPasses( void )
	{
		return 1;
	}

	// Can't be const because the material might have to precache itself.
	virtual int				GetTextureMemoryBytes( void )
	{
		return 64;
	}

	// Meant to be used with materials created using CreateMaterial
	// It updates the materials to reflect the current values stored in the material vars
	virtual void			Refresh()
	{
	}

	// GR - returns true is material uses lightmap alpha for blending
	virtual bool			NeedsLightmapBlendAlpha( void )
	{
		return false;
	}

	// returns true if the shader doesn't do lighting itself and requires
	// the data that is sent to it to be prelighted
	virtual bool			NeedsSoftwareLighting( void )
	{
		return false;
	}

	// Gets at the shader parameters
	virtual int				ShaderParamCount() const
	{
		return 0;
	}

	virtual IMaterialVar	**GetShaderParams( void )
	{
		return 0;
	}

	virtual bool IsErrorMaterial() const
	{
		return false;
	}
	virtual void SetUseFixedFunctionBakedLighting( bool bEnable ) 
	{ 
	}
	virtual MorphFormat_t GetMorphFormat() const
	{
		return 0;
	}
	virtual void SetShaderAndParams( KeyValues *pKeyValues )
	{
	}
	virtual const char *GetShaderName() const { return "Wireframe"; }
		
	virtual void		DeleteIfUnreferenced() {}
	virtual bool IsSpriteCard() { return false; }

	virtual void			RefreshPreservingMaterialVars() {};

	virtual bool			WasReloadedFromWhitelist() {return false;}

	virtual bool			IsPrecached() const {return true;}
};

CDummyMaterial g_DummyMaterial;
IMaterial *g_pDummyMaterial = &g_DummyMaterial;


void* DummyMaterialSystemFactory( const char *pName, int *pReturnCode )
{
	if ( stricmp( pName, MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION ) == 0 )
		return &g_DummyHardwareConfig;

	else
		return NULL;
}


// ---------------------------------------------------------------------------------------- //
// Dummy morph
// ---------------------------------------------------------------------------------------- //
class CDummyMorph : public IMorph
{
public:
	virtual void Lock( float flFloatToFixedScale ) {}
	virtual void AddMorph( const MorphVertexInfo_t &info ) {}
	virtual void Unlock(  ) {}
	virtual void AccumulateMorph( int nWeightCount, const MorphWeight_t* pWeights ) {}
};


// ---------------------------------------------------------------------------------------- //
// CDummyMaterialSystem.
// ---------------------------------------------------------------------------------------- //
class CDummyMaterialSystem : public IMaterialSystemStub, public CRefCounted1<IMatRenderContext, CRefCountServiceNull>
{
private:
	IMaterialSystem *m_pRealMaterialSystem;
public:
	CDummyMaterialSystem()
	{
		m_pRealMaterialSystem = 0;
	}

	virtual void	SetRealMaterialSystem( IMaterialSystem *pSys )
	{
		m_pRealMaterialSystem = pSys;
	}

	
	// Call this to initialize the material system
	// returns a method to create interfaces in the shader dll
	virtual CreateInterfaceFn	Init( char const* pShaderDLL, 
									  IMaterialProxyFactory *pMaterialProxyFactory,
									  CreateInterfaceFn fileSystemFactory,
									  CreateInterfaceFn cvarFactory )
	{
		return DummyMaterialSystemFactory;
	}

	virtual void				Shutdown( )
	{
	}

	virtual IMaterialSystemHardwareConfig *GetHardwareConfig( const char *pVersion, int *returnCode )
	{
		if ( returnCode )
			*returnCode = 1;

		return &g_DummyHardwareConfig;
	}

	// Gets the number of adapters...
	virtual int					GetDisplayAdapterCount() const
	{
		return 0;
	}

	// Returns info about each adapter
	virtual void				GetDisplayAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const
	{
	}

	// Returns the number of modes
	virtual int					GetModeCount( int adapter ) const
	{
		return 0;
	}

	// Returns mode information..
	virtual void				GetModeInfo( int adapter, int mode, MaterialVideoMode_t& info ) const
	{
	}

	// Returns the mode info for the current display device
	virtual void				GetDisplayMode( MaterialVideoMode_t& mode ) const
	{
	}
 
	// Sets the mode...
	virtual bool				SetMode( void* hwnd, const MaterialSystem_Config_t &config )
	{
		return true;
	}

	// Creates/ destroys a child window
	virtual bool				AddView( void* hwnd )
	{
		return false;
	}
	virtual void				RemoveView( void* hwnd )
	{
	}

	// Sets the view
	virtual void				SetView( void* hwnd )
	{
	}

	// return true if lightmaps need to be redownloaded
	// Call this before rendering each frame with the current config
	// for the material system.
	// Will do whatever is necessary to get the material system into the correct state
	// upon configuration change. .doesn't much else otherwise.
	virtual bool				UpdateConfig( bool forceUpdate )
	{
		return false;
	}
	
	virtual bool				OverrideConfig( const MaterialSystem_Config_t &config, bool bForceUpdate )
	{
		return false;
	}

	// This is the interface for knowing what materials are available
	// is to use the following functions to get a list of materials.  The
	// material names will have the full path to the material, and that is the 
	// only way that the directory structure of the materials will be seen through this
	// interface.
	// NOTE:  This is mostly for worldcraft to get a list of materials to put
	// in the "texture" browser.in Worldcraft
	virtual MaterialHandle_t	FirstMaterial() const
	{
		return 0;
	}

	// returns InvalidMaterial if there isn't another material.
	// WARNING: you must call GetNextMaterial until it returns NULL, 
	// otherwise there will be a memory leak.
	virtual MaterialHandle_t	NextMaterial( MaterialHandle_t h ) const
	{
		return 0;
	}

	// This is the invalid material
	virtual MaterialHandle_t	InvalidMaterial() const
	{
		if ( m_pRealMaterialSystem )
			return m_pRealMaterialSystem->InvalidMaterial();
		else
			return 0;
	}

	// Returns a particular material
	virtual IMaterial*			GetMaterial( MaterialHandle_t h ) const
	{
		if ( m_pRealMaterialSystem )
			return m_pRealMaterialSystem->GetMaterial( h );
		else
			return &g_DummyMaterial;
	}

	// Find a material by name.
	// The name of a material is a full path to 
	// the vmt file starting from "hl2/materials" (or equivalent) without
	// a file extension.
	// eg. "dev/dev_bumptest" refers to somethign similar to:
	// "d:/hl2/hl2/materials/dev/dev_bumptest.vmt"
	virtual IMaterial *FindMaterial( char const* pMaterialName, const char *pTextureGroupName, bool complain = true, const char *pComplainPrefix = NULL )
	{
		if ( m_pRealMaterialSystem )
			return m_pRealMaterialSystem->FindMaterial( pMaterialName, pTextureGroupName, complain, pComplainPrefix );
		return &g_DummyMaterial;
	}

	virtual bool IsMaterialLoaded( char const* pMaterialName )
	{
		if ( m_pRealMaterialSystem )
			return m_pRealMaterialSystem->IsMaterialLoaded( pMaterialName );
		return true;
	}

	virtual IMaterial *FindMaterialEx( char const* pMaterialName, const char *pTextureGroupName, int nContext, bool complain = true, const char *pComplainPrefix = NULL )
	{
		if ( m_pRealMaterialSystem )
			return m_pRealMaterialSystem->FindMaterialEx( pMaterialName, pTextureGroupName, nContext, complain, pComplainPrefix );
		return &g_DummyMaterial;
	}

	virtual IMaterial *FindProceduralMaterial( const char *pMaterialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues )
	{
		if ( m_pRealMaterialSystem )
			return m_pRealMaterialSystem->FindProceduralMaterial( pMaterialName, pTextureGroupName, pVMTKeyValues );
		return &g_DummyMaterial;
	}

	virtual ITexture *FindTexture( char const* pTextureName, const char *pTextureGroupName, bool complain = true, int nAdditionalCreationFlags = 0)
	{
		if ( m_pRealMaterialSystem )
			return m_pRealMaterialSystem->FindTexture( pTextureName, pTextureGroupName, complain, nAdditionalCreationFlags );
		return &g_DummyTexture;
	}

	virtual void SetAsyncTextureLoadCache( FileCacheHandle_t hFileCache )
	{
	}

	virtual void BindLocalCubemap( ITexture *pTexture )
	{
	}
	virtual ITexture *GetLocalCubemap( )
	{
		return &g_DummyTexture;
	}
	
	// pass in an ITexture (that is build with "rendertarget" "1") or
	// pass in NULL for the regular backbuffer.
	virtual void				SetRenderTarget( ITexture *pTexture )
	{
	}
	
	virtual ITexture *			GetRenderTarget( void )
	{
		return &g_DummyTexture;
	}
	
	virtual void				SetRenderTargetEx( int nRenderTargetID, ITexture *pTexture )
	{
	}
	
	virtual ITexture *			GetRenderTargetEx( int nRenderTargetID )
	{
		return &g_DummyTexture;
	}
	virtual void				GetRenderTargetDimensions( int &width, int &height) const
	{
		width = 256;
		height = 256;
	}
	
	// Get the total number of materials in the system.  These aren't just the used
	// materials, but the complete collection.
	virtual int					GetNumMaterials( ) const
	{
		return m_pRealMaterialSystem->GetNumMaterials();
	}

	// Remove any materials from memory that aren't in use as determined
	// by the IMaterial's reference count.
	virtual void				UncacheUnusedMaterials( bool bRecomputeStateSnapshots )
	{
		if ( m_pRealMaterialSystem )
		{
			m_pRealMaterialSystem->UncacheUnusedMaterials( bRecomputeStateSnapshots );
		}
	}

	virtual void				SuspendTextureStreaming( )
	{
		if ( m_pRealMaterialSystem )
			m_pRealMaterialSystem->SuspendTextureStreaming();
	}

	virtual void				ResumeTextureStreaming( )
	{
		if ( m_pRealMaterialSystem )
			m_pRealMaterialSystem->ResumeTextureStreaming();
	}

	// uncache all materials. .  good for forcing reload of materials.
	virtual void				UncacheAllMaterials( )
	{
		if ( m_pRealMaterialSystem )
			m_pRealMaterialSystem->UncacheAllMaterials();
	}

	// Load any materials into memory that are to be used as determined
	// by the IMaterial's reference count.
	virtual void				CacheUsedMaterials( )
	{
		if ( m_pRealMaterialSystem )
			m_pRealMaterialSystem->CacheUsedMaterials( );
	}
	
	// Force all textures to be reloaded from disk.
	virtual void				ReloadTextures( )
	{
	}
	
	// Allows us to override the depth buffer setting of a material
	virtual void	OverrideDepthEnable( bool bEnable, bool bEnableValue )
	{
	}

	virtual void	OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
	{
	}

	virtual void	OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
	{
	}

	//
	// lightmap allocation stuff
	//

	// To allocate lightmaps, sort the whole world by material twice.
	// The first time through, call AllocateLightmap for every surface.
	// that has a lightmap.
	// The second time through, call AllocateWhiteLightmap for every 
	// surface that expects to use shaders that expect lightmaps.
	virtual void				BeginLightmapAllocation( )
	{
	}
	// returns the sorting id for this surface
	virtual int 				AllocateLightmap( int width, int height, 
		                                          int offsetIntoLightmapPage[2],
												  IMaterial *pMaterial )
	{
		return 0;
	}
	// returns a lightmap page ID for this allocation, -1 if none available
	virtual int	AllocateDynamicLightmap( int lightmapSize[2], int *pOutOffsetIntoPage, int frameID )
	{
		return -1;
	}

	// returns the sorting id for this surface
	virtual int					AllocateWhiteLightmap( IMaterial *pMaterial )
	{
		return 0;
	}
	virtual void				EndLightmapAllocation( )
	{
	}

	// lightmaps are in linear color space
	// lightmapPageID is returned by GetLightmapPageIDForSortID
	// lightmapSize and offsetIntoLightmapPage are returned by AllocateLightmap.
	// You should never call UpdateLightmap for a lightmap allocated through
	// AllocateWhiteLightmap.
	virtual void				UpdateLightmap( int lightmapPageID, int lightmapSize[2],
												int offsetIntoLightmapPage[2], 
												float *pFloatImage, float *pFloatImageBump1,
												float *pFloatImageBump2, float *pFloatImageBump3 )
	{
	}
	// Force the lightmaps updated with UpdateLightmap to be sent to the hardware.
	virtual void				FlushLightmaps( )
	{
	}

	// fixme: could just be an array of ints for lightmapPageIDs since the material
	// for a surface is already known.
	virtual int					GetNumSortIDs( )
	{
		return 10;
	}
//	virtual int					GetLightmapPageIDForSortID( int sortID ) = 0;
	virtual void				GetSortInfo( MaterialSystem_SortInfo_t *pSortInfoArray )
	{
	}

	virtual void				BeginFrame( float )
	{
	}
	virtual void				EndFrame( )
	{
	}
	virtual bool				IsInFrame() const { return false; }

	// Bind a material is current for rendering.
	virtual void				Bind( IMaterial *material, void *proxyData = 0 )
	{
	}
	// Bind a lightmap page current for rendering.  You only have to 
	// do this for materials that require lightmaps.
	virtual void				BindLightmapPage( int lightmapPageID )
	{
	}

	// inputs are between 0 and 1
	virtual void				DepthRange( float zNear, float zFar )
	{
	}

	virtual void				ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil )
	{
	}

	virtual void				ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth )
	{
	}

	virtual void				ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
	{
	}

	virtual void				PerformFullScreenStencilOperation( void )
	{
	}

	// read to a unsigned char rgb image.
	virtual void				ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat )
	{
	}

	// Read w/ stretch to a host-memory buffer
	virtual void				ReadPixelsAndStretch( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pBuffer, ImageFormat dstFormat, int nDstStride )
	{
	}

	// Sets lighting
	virtual void				SetAmbientLight( float r, float g, float b )
	{
	}
	virtual void				SetLight( int lightNum, const LightDesc_t& desc )
	{
	}
	virtual void				SetLightingOrigin( Vector vLightingOrigin )
	{
	}

	// The faces of the cube are specified in the same order as cubemap textures
	virtual void				SetAmbientLightCube( Vector4D cube[6] )
	{
	}
	
	// Blit the backbuffer to the framebuffer texture
	virtual void				CopyRenderTargetToTexture( ITexture * )
	{
	}
	
	virtual void				SetFrameBufferCopyTexture( ITexture *pTexture, int textureIndex )
	{
	}

	virtual	ITexture *          GetFrameBufferCopyTexture( int textureIndex )
	{
		return &g_DummyTexture;
	}

	// do we need this?
	virtual void				Flush( bool flushHardware = false )
	{
	}

	//
	// end vertex array api
	//

	//
	// Debugging tools
	//
	virtual void				DebugPrintUsedMaterials( const char *pSearchSubString, bool bVerbose )
	{
	}
	virtual void				DebugPrintUsedTextures( void )
	{
	}
	virtual void				ToggleSuppressMaterial( char const* pMaterialName )
	{
	}
	virtual void				ToggleDebugMaterial( char const* pMaterialName )
	{
	}

	// matrix api
	virtual void				MatrixMode( MaterialMatrixMode_t matrixMode )
	{
	}
	virtual void				PushMatrix( void )
	{
	}
	virtual void				PopMatrix( void )
	{
	}
/*
	virtual void				LoadMatrix( float * )
	{
	}
*/
	// Methods that use VMatrix
	virtual void		LoadMatrix( const VMatrix& matrix )
	{
	}

	virtual void		LoadMatrix( const matrix3x4_t& matrix )
	{
	}

	virtual void		LoadBoneMatrix( int boneIndex, const matrix3x4_t& matrix )
	{
	}

	virtual void		MultMatrix( const VMatrix& matrix )
	{
	}

	virtual void		MultMatrix( const matrix3x4_t& matrix )
	{
	}
	virtual void		MultMatrixLocal( const VMatrix& matrix )
	{
	}

	virtual void		MultMatrixLocal( const matrix3x4_t& matrix )
	{
	}
	virtual void				GetMatrix( MaterialMatrixMode_t matrixMode, VMatrix *pMatrix )
	{
		pMatrix->Identity();
	}
	virtual void				GetMatrix( MaterialMatrixMode_t matrixMode, matrix3x4_t *pMatrix )
	{
		SetIdentityMatrix( *pMatrix );
	}
	virtual void				LoadIdentity( void )
	{
	}
	virtual void				Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
	{
	}
	virtual void				PerspectiveX( double fovx, double aspect, double zNear, double zFar )
	{
	}
	virtual void				PickMatrix( int x, int y, int width, int height )
	{
	}
	virtual void				Rotate( float angle, float x, float y, float z )
	{
	}
	virtual void				Translate( float x, float y, float z )
	{
	}
	virtual void				Scale( float x, float y, float z )
	{
	}
	// end matrix api

	// Sets/gets the viewport
	virtual void				Viewport( int x, int y, int width, int height )
	{
	}
	virtual void				GetViewport( int& x, int& y, int& width, int& height ) const
	{
		x = y = 0;
		width = height = 640;
	}

	// The cull mode
	virtual void				CullMode( MaterialCullMode_t cullMode )
	{
	}

	// end matrix api

	// Force writes only when z matches. . . useful for stenciling things out
	// by rendering the desired Z values ahead of time.
//	virtual void				ForceDepthFuncEquals( bool bEnable ) = 0;
//	virtual void				RenderZOnlyWithHeightClip( bool bEnable ) = 0;
	// This could easily be extended to a general user clip plane
	virtual void				SetHeightClipMode( MaterialHeightClipMode_t nClipMode )
	{
	}
	virtual MaterialHeightClipMode_t GetHeightClipMode( )
	{
		return MATERIAL_HEIGHTCLIPMODE_DISABLE;
	}
	// garymcthack : fog z is always used for heightclipz for now.
	virtual void				SetHeightClipZ( float z )
	{
	}
	
	// Fog methods...
	virtual void				FogMode( MaterialFogMode_t fogMode )
	{
	}
	MaterialFogMode_t			GetFogMode( void )
	{
		return MATERIAL_FOG_NONE;
	}
	virtual void				FogStart( float fStart )
	{
	}
	virtual void				FogEnd( float fEnd )
	{
	}
	virtual void				FogMaxDensity( float flMaxDensity )
	{
	}

	virtual void				SetFogZ( float fogZ )
	{
	}
	virtual void				GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
	{
	}

	virtual void				FogColor3f( float r, float g, float b )
	{
	}
	virtual void				FogColor3fv( float const* rgb )
	{
	}
	virtual void				FogColor3ub( unsigned char r, unsigned char g, unsigned char b )
	{
	}
	virtual void				FogColor3ubv( unsigned char const* rgb )
	{
	}

	virtual void				GetFogColor( unsigned char *rgb )
	{
	}

	// Sets the number of bones for skinning
	virtual void				SetNumBoneWeights( int numBones ) 
	{
	}
	virtual IMaterialProxyFactory *GetMaterialProxyFactory()
	{
		return NULL;
	}
	
	virtual void	SetMaterialProxyFactory( IMaterialProxyFactory* pFactory )
	{
	}

	// Read the page size of an existing lightmap by sort id (returned from AllocateLightmap())
	virtual void				GetLightmapPageSize( int lightmap, int *width, int *height ) const
	{
		if ( m_pRealMaterialSystem )
			m_pRealMaterialSystem->GetLightmapPageSize( lightmap, width, height );
		else
			*width = *height = 32;
	}
	
	/// FIXME: This stuff needs to be cleaned up and abstracted.
	// Stuff that gets exported to the launcher through the engine
	virtual void				SwapBuffers( )
	{
	}

	// Use this to spew information about the 3D layer 
	virtual void				SpewDriverInfo() const
	{
	}

	// Creates/destroys Mesh
	virtual IMesh* CreateStaticMesh( VertexFormat_t fmt, const char *pTextureBudgetGroup, IMaterial * pMaterial )
	{
		return GetDummyMesh();
	}
	virtual void DestroyStaticMesh( IMesh* mesh )
	{
	}

	// Gets the dynamic mesh associated with the currently bound material
	// note that you've got to render the mesh before calling this function 
	// a second time. Clients should *not* call DestroyStaticMesh on the mesh 
	// returned by this call.
	// Use buffered = false if you want to not have the mesh be buffered,
	// but use it instead in the following pattern:
	//		meshBuilder.Begin
	//		meshBuilder.End
	//		Draw partial
	//		Draw partial
	//		Draw partial
	//		meshBuilder.Begin
	//		meshBuilder.End
	//		etc
	// Use Vertex or Index Override to supply a static vertex or index buffer
	// to use in place of the dynamic buffers.
	//
	// If you pass in a material in pAutoBind, it will automatically bind the
	// material. This can be helpful since you must bind the material you're
	// going to use BEFORE calling GetDynamicMesh.
	virtual IMesh* GetDynamicMesh( bool bBuffered = true, IMesh* pVertexOverride = 0,	
		IMesh* pIndexOverride = 0, IMaterial *pAutoBind = 0 )
	{
		return GetDummyMesh();
	}

	virtual IMesh* GetDynamicMeshEx( VertexFormat_t vertexFormat, bool bBuffered = true, 
		IMesh* pVertexOverride = 0,	IMesh* pIndexOverride = 0, IMaterial *pAutoBind = 0 )
	{
		return GetDummyMesh();
	}

	virtual IMesh *GetFlexMesh()
	{
		return GetDummyMesh();
	}
		
	// Selection mode methods
	virtual int  SelectionMode( bool selectionMode )
	{
		return 0;
	}
	virtual void SelectionBuffer( unsigned int* pBuffer, int size )
	{
	}
	virtual void ClearSelectionNames( )
	{
	}
	virtual void LoadSelectionName( int name )
	{
	}
	virtual void PushSelectionName( int name )
	{
	}
	virtual void PopSelectionName()
	{
	}
	
	// Installs a function to be called when we need to release vertex buffers + textures
	virtual void AddReleaseFunc( MaterialBufferReleaseFunc_t func )
	{
	}
	virtual void RemoveReleaseFunc( MaterialBufferReleaseFunc_t func )
	{
	}

	// Installs a function to be called when we need to restore vertex buffers
	virtual void AddRestoreFunc( MaterialBufferRestoreFunc_t func )
	{
	}
	virtual void RemoveRestoreFunc( MaterialBufferRestoreFunc_t func )
	{
	}

	// Stuff for probing properties of shaders.
	virtual int					GetNumShaders( void ) const 
	{
		return 0;
	}
	virtual const char *		GetShaderName( int shaderID ) const
	{
		return NULL;
	}
	virtual int					GetNumShaderParams( int shaderID ) const
	{
		return 0;
	}
	virtual const char *		GetShaderParamName( int shaderID, int paramID ) const
	{
		return NULL;
	}
	virtual const char *		GetShaderParamHelp( int shaderID, int paramID ) const
	{
		return NULL;
	}
	virtual ShaderParamType_t	GetShaderParamType( int shaderID, int paramID ) const
	{
		return ( enum ShaderParamType_t )0;
	}
	virtual const char *		GetShaderParamDefault( int shaderID, int paramID ) const
	{
		return NULL;
	}

	// Reloads materials
	virtual void	ReloadMaterials( const char *pSubString = NULL )
	{
	}

	virtual void	ResetMaterialLightmapPageInfo()
	{
	}

	virtual ITexture* CreateRenderTargetTexture( 
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
		ImageFormat format, 
		MaterialRenderTargetDepth_t depth )
	{
		return &g_DummyTexture;
	}

	virtual ITexture *CreateProceduralTexture( 
		const char			*pTextureName, 
		const char			*pTextureGroupName, 
		int					w, 
		int					h, 
		ImageFormat			fmt, 
		int					nFlags )
	{
		return &g_DummyTexture;
	}

	// Sets the Clear Color for ClearBuffer....
	virtual void ClearColor3ub( unsigned char r, unsigned char g, unsigned char b )
	{
	}
	virtual void ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
	{
	}
	virtual void SetInStubMode( bool b )
	{
	}

	// Create new materials
	virtual IMaterial	*CreateMaterial( const char *pMaterialName, KeyValues *pVMTKeyValues )
	{
		return &g_DummyMaterial;
	}

	void GetBackBufferDimensions( int &w, int &h ) const
	{
		w = 1024;
		h = 768;
	}
	
	ImageFormat GetBackBufferFormat( void ) const
	{
		return IMAGE_FORMAT_RGBA8888;
	}
	
	// FIXME: This is a hack required for NVidia/XBox, can they fix in drivers?
	virtual void	DrawScreenSpaceQuad( IMaterial* pMaterial ) {}

	// FIXME: Test interface
	virtual bool Connect( CreateInterfaceFn factory ) { return true; }
	virtual void Disconnect() {}
	virtual void *QueryInterface( const char *pInterfaceName ) { return NULL; }
	virtual InitReturnVal_t Init() { return INIT_OK; }
	virtual void SetShaderAPI( const char *pShaderAPIDLL ) {}
	virtual void SetAdapter( int nAdapter, int nFlags ) {}

	// Release temporary HW memory...
	virtual void ResetTempHWMemory( bool bExitingLevel ) {}
	virtual ITexture*	CreateNamedRenderTargetTextureEx( 
		const char *pRTName,				// Pass in NULL here for an unnamed render target.
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
		ImageFormat format, 
		MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED, 
		unsigned int textureFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		unsigned int renderTargetFlags = 0
		)
	{
		return &g_DummyTexture;
	}
	virtual ITexture* CreateNamedRenderTargetTexture( 
		const char *pRTName, 
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
		ImageFormat format, 
		MaterialRenderTargetDepth_t depth,
		bool bClampTexCoords, 
		bool bAutoMipMap
		)
	{
		return &g_DummyTexture;
	}
	virtual void SyncToken( const char *pToken ) {}
	virtual float	ComputePixelWidthOfSphere( const Vector& origin, float flRadius )
	{
		return 1.0f;
	}
	virtual float	ComputePixelDiameterOfSphere( const Vector& origin, float flRadius )
	{
		return 1.0f;
	}

	OcclusionQueryObjectHandle_t CreateOcclusionQueryObject( void )
	{
		return INVALID_OCCLUSION_QUERY_OBJECT_HANDLE;
	}

	void DestroyOcclusionQueryObject( OcclusionQueryObjectHandle_t handle )
	{
	}

	void ResetOcclusionQueryObject( OcclusionQueryObjectHandle_t hOcclusionQuery ) {}

	void BeginOcclusionQueryDrawing( OcclusionQueryObjectHandle_t handle )
	{
	}

	void EndOcclusionQueryDrawing( OcclusionQueryObjectHandle_t handle )
	{
	}

	int OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t handle )
	{
		return 0;
	}
	virtual void SetFlashlightMode( bool )
	{
	}
	virtual bool GetFlashlightMode( void ) const
	{
		return false;
	}
	virtual bool InFlashlightMode( void ) const
	{
		return false;
	}
	virtual void SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture )
	{
	}
	virtual void SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture )
	{
	}

	virtual void SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor )
	{
	}

	virtual void PushDeformation( DeformationBase_t const *Deformation )
	{
	}

	virtual void PopDeformation( )
	{
	}

	virtual int GetNumActiveDeformations() const
	{
		return 0;
	}



	// Get the current config for this video card (as last set by control panel or the default if not)
	virtual const MaterialSystem_Config_t &GetCurrentConfigForVideoCard() const
	{
		static MaterialSystem_Config_t dummy;
		return dummy;
	}

	// Get video card identitier
	virtual const MaterialSystemHardwareIdentifier_t &GetVideoCardIdentifier( void ) const
	{
		static MaterialSystemHardwareIdentifier_t dummy;
		return dummy;
	}
	virtual void AddModeChangeCallBack( ModeChangeCallbackFunc_t func )
	{
	}
	virtual void RemoveModeChangeCallBack( ModeChangeCallbackFunc_t func )
	{
	}
	virtual bool GetRecommendedConfigurationInfo( int nDxLevel, KeyValues *pKeyValues )
	{
		return false;
	}

	virtual void EnableUserClipTransformOverride( bool bEnable )
	{
	}

	virtual void UserClipTransform( const VMatrix &worldToProjection )
	{
	}

	// Used to iterate over all shaders for editing purposes
	virtual int	 ShaderCount() const
	{
		return 0;
	}

	virtual int  GetShaders( int nFirstShader, int nCount, IShader **ppShaderList ) const
	{
		return 0;
	}

	// Used to enable editor materials. Must be called before Init.
	virtual void	EnableEditorMaterials()
	{
	}

	// Used to enable editor materials. Must be called before Init.
	virtual int		GetCurrentAdapter()	const
	{
		return 0;
	}

	virtual char *GetDisplayDeviceName() const OVERRIDE
	{
		return "";
	}

	// Creates/destroys morph data associated w/ a particular material
	IMorph *CreateMorph( MorphFormat_t, const char *pDebugName )
	{
		static CDummyMorph s_DummyMorph;
		return &s_DummyMorph;
	}

	void DestroyMorph( IMorph *pMorph )
	{
	}

	void BindMorph( IMorph *pMorph )
	{
	}

	// Sets morph target factors
	virtual void SetMorphTargetFactors( int nTargetId, float *pValue, int nCount )
	{
	}

	virtual void SetToneMappingScaleLinear( const Vector &scale )
	{
	}

	virtual void EvictManagedResources()
	{
	}

	// Gets the window size
	virtual void	GetWindowSize( int &width, int &height ) const
	{
		width = height = 0;
		if ( m_pRealMaterialSystem )
		{
			CMatRenderContextPtr pRenderContext( m_pRealMaterialSystem );
			pRenderContext->GetWindowSize(width, height);
		}
	}

	// For dealing with device lost in cases where SwapBuffers isn't called all the time (Hammer)
	virtual void HandleDeviceLost()
	{
	}

	virtual void AppUsesRenderTargets()
	{
	}

	virtual void DrawScreenSpaceRectangle( 
		IMaterial *pMaterial,
		int destx, int desty,
		int width, int height,
		float src_texture_x0, float src_texture_y0,
		float src_texture_x1, float src_texture_y1,	
		int src_texture_width, int src_texture_height,
		void *pClientRenderable = NULL,
		int nXDice = 1,
		int nYDice = 1 )
	{
	}

	virtual void BeginRenderTargetAllocation()
	{
	}

	// Simulate an Alt-Tab in here, which causes a release/restore of all resources
	virtual void EndRenderTargetAllocation()
	{
	}

	ITexture *CreateNamedRenderTargetTextureEx2( 
		const char *pRTName,				// Pass in NULL here for an unnamed render target.
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
		ImageFormat format, 
		MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED, 
		unsigned int textureFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		unsigned int renderTargetFlags = 0
		)
	{
		return NULL;
	};

	void PushRenderTargetAndViewport( )
	{
	}

	void PushRenderTargetAndViewport( ITexture *pTexture )
	{
	}

	void PushRenderTargetAndViewport( ITexture *pTexture, int nViewX, int nViewY, int nViewW, int nViewH )
	{
	}

	void PushRenderTargetAndViewport( ITexture *pTexture, ITexture *pDepthTexture, int nViewX, int nViewY, int nViewW, int nViewH )
	{
	}

	void PopRenderTargetAndViewport( void )
	{
	}

	virtual int ShaderFlagCount() const
	{
		return 0;
	}

	virtual const char *ShaderFlagName( int nIndex ) const
	{
		return "";
	}

	virtual void BindLightmapTexture( ITexture *pLightmapTexture )
	{
	}

	// Returns the currently active shader fallback for a particular shader
	virtual void GetShaderFallback( const char *pShaderName, char *pFallbackShader, int nFallbackLength )
	{
		pFallbackShader[0] = 0;
	}

#ifdef DX_TO_GL_ABSTRACTION
	virtual void DoStartupShaderPreloading( void )
	{
	}
#endif

	// Blit a subrect of the current render target to another texture
	virtual void CopyRenderTargetToTextureEx( ITexture *pTexture, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect = NULL )
	{
	}

	virtual void CopyTextureToRenderTargetEx( int nRenderTargetID, ITexture *pTexture, Rect_t *pSrcRect, Rect_t *pDstRect = NULL )
	{
	}

	bool IsTextureLoaded( char const* pTextureName ) const
	{
		return false;
	}

	void PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right )
	{
	}

	void SetFloatRenderingParameter(int parm_number, float value)
	{
	}

	void SetIntRenderingParameter(int parm_number, int value)
	{
	}
	void SetVectorRenderingParameter(int parm_number, Vector const &value)
	{
	}

	float GetFloatRenderingParameter(int parm_number) const
	{
		return 0;
	}

	int GetIntRenderingParameter(int parm_number) const
	{
		return 0;
	}

	Vector GetVectorRenderingParameter(int parm_number) const
	{
		return Vector(0,0,0);
	}
	
	void ReleaseResources(void)
	{
	}

	void ReacquireResources(void)
	{
	}

	Vector GetToneMappingScaleLinear( void )
	{
		return Vector(1,1,1);
	}

	virtual void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
	{
		*pMaxVerts = 32768;
		*pMaxIndices = 32768;
	}

	// Returns the max possible vertices + indices to render in a single draw call
	virtual int GetMaxVerticesToRender( IMaterial *pMaterial )
	{
		return 32768;
	}

	virtual int GetMaxIndicesToRender( )
	{
		return 32768;
	}

	// stencil buffer operations.
	virtual void SetStencilEnable(bool onoff)
	{
	}

	virtual void SetStencilFailOperation(StencilOperation_t op)
	{
	}

	virtual void SetStencilZFailOperation(StencilOperation_t op)
	{
	}

	virtual void SetStencilPassOperation(StencilOperation_t op)
	{
	}

	virtual void SetStencilCompareFunction(StencilComparisonFunction_t cmpfn)
	{
	}

	virtual void SetStencilReferenceValue(int ref)
	{
	}

	virtual void SetStencilTestMask(uint32 msk)
	{
	}

	virtual void SetStencilWriteMask(uint32 msk) 
	{
	}

	virtual void ClearStencilBufferRectangle(int xmin, int ymin, int xmax, int ymax, int value)
	{
	}

	virtual void ModInit()
	{
	}

	virtual void ModShutdown()
	{
	}

	virtual void EnableColorCorrection( bool bEnable ) {}
	virtual ColorCorrectionHandle_t AddLookup( const char *pName ) { return 0; }
	virtual bool RemoveLookup( ColorCorrectionHandle_t handle ) { return true; }
	virtual void LockLookup( ColorCorrectionHandle_t handle ) {}
	virtual void LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName ) {}
	virtual void UnlockLookup( ColorCorrectionHandle_t handle ) {}
	virtual void SetLookupWeight( ColorCorrectionHandle_t handle, float flWeight ) {}
	virtual void ResetLookupWeights( ) {}
	virtual void SetResetable( ColorCorrectionHandle_t handle, bool bResetable ) {}

	virtual void PushCustomClipPlane( const float *pPlane )
	{
	}

	virtual void PopCustomClipPlane( void )
	{
	}

	virtual bool EnableClipping( bool bEnable )
	{
		return true;
	}

	virtual void PushHeightClipPlane( void )
	{
	}
	void ResetToneMappingScale( float sc)
	{
	}
	void TurnOnToneMapping()
	{
	}

	virtual void GetDXLevelDefaults(uint &max_dxlevel,uint &recommended_dxlevel)
	{
		max_dxlevel=recommended_dxlevel=90;
	}

	virtual bool UsingFastClipping( void )
	{
		return true; //true for "crappier" hardware, so true is safer than false
	}

	virtual int StencilBufferBits( void )
	{
		return 0;
	}

	virtual void DisableAllLocalLights() {}
	virtual int CompareMaterialCombos( IMaterial *pMaterial1, IMaterial *pMaterial2, int lightmapID1, int lightmapID2 ) { return 0; }

	virtual bool SupportsMSAAMode( int nMSAAMode ) { return false; }
	virtual bool SupportsCSAAMode( int nNumSamples, int nQualityLevel ) { return false; }
	virtual bool SupportsHDRMode( HDRType_t nHDRModede ) { return false; }
	virtual bool IsDX10Card() { return false; }

	// Hooks for firing PIX events from outside the Material System...
	virtual void BeginPIXEvent( unsigned long color, const char *szName ) {};
	virtual void EndPIXEvent() {};
	virtual void SetPIXMarker( unsigned long color, const char *szName ) {};

	virtual IMatRenderContext *GetRenderContext() { return this; }

	void BeginRender() {}
	void BeginRender( float ) {}
	void EndRender() {}

	virtual void							SetThreadMode( MaterialThreadMode_t, int ) {}
	virtual MaterialThreadMode_t			GetThreadMode( ) { return MATERIAL_SINGLE_THREADED; }
	virtual bool							IsRenderThreadSafe( ) { return true; }
	virtual bool							AllowThreading( bool bAllow, int nServiceThread ) { return false; }
	virtual void							ExecuteQueued() {}

	virtual void BeginBatch( IMesh* pIndices ) {}
	virtual void BindBatch( IMesh* pVertices, IMaterial *pAutoBind = NULL ) {}
	virtual void DrawBatch(int nFirstIndex, int nIndexCount )  {}
	virtual void EndBatch()  {}

	virtual void SetGoalToneMappingScale(float) {}

	virtual bool SupportsShadowDepthTextures( void ) { return false; }

	virtual bool SupportsFetch4( void ) { return false; }


	virtual void SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias ) {}

	virtual ICallQueue *GetCallQueue() { return NULL; }
	virtual void GetWorldSpaceCameraPosition( Vector *pCameraPos )
	{
		pCameraPos->Init();
	}
	virtual void GetWorldSpaceCameraVectors( Vector *pVecForward, Vector *pVecRight, Vector *pVecUp )
	{
		if ( pVecForward )
		{
			pVecForward->Init( 1, 0, 0 );
		}
		if ( pVecRight )
		{
			pVecRight->Init( 0, -1, 0 );
		}
		if ( pVecUp )
		{
			pVecUp->Init( 0, 0, 1 );
		}
	}

	virtual void BeginUpdateLightmaps() {}
	virtual void EndUpdateLightmaps() {}

	virtual MaterialLock_t		Lock() { return NULL; }
	virtual void				Unlock( MaterialLock_t ) {}

	virtual ImageFormat GetShadowDepthTextureFormat() { return IMAGE_FORMAT_UNKNOWN; }

	virtual IMatRenderContext *CreateRenderContext( MaterialContextType_t type )
	{
		return RetAddRef( (IMatRenderContext *)this );
	}

	virtual IMatRenderContext *SetRenderContext( IMatRenderContext *pContext )
	{
		SafeRelease( pContext );
		return RetAddRef( this );
	}
	virtual IVertexBuffer *		GetDynamicVertexBuffer( /*VertexFormat_t vertexFormat, */bool buffered = true )
	{
		Assert( 0 );
		return NULL;
//		return GetDummyMesh();
	}
	virtual IIndexBuffer *		GetDynamicIndexBuffer( /*MaterialIndexFormat_t fmt, */bool buffered = true )
	{
		Assert( 0 );
		return NULL;
//		return GetDummyMesh();
	}

// ------------ New Vertex/Index Buffer interface ----------------------------
	virtual IVertexBuffer *CreateStaticVertexBuffer( VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup )
	{
		Assert( 0 );
		return NULL;
	}
	virtual IIndexBuffer *CreateStaticIndexBuffer( MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup )
	{
		Assert( 0 );
		return NULL;
	}
	virtual void DestroyVertexBuffer( IVertexBuffer * )
	{
	}
	virtual void DestroyIndexBuffer( IIndexBuffer * )
	{
	}
	// Do we need to specify the stream here in the case of locking multiple dynamic VBs on different streams?
	virtual IVertexBuffer *GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered = true )
	{
		Assert( 0 );
		return NULL;
	}
	virtual IIndexBuffer *GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered = true )
	{
		Assert( 0 );
		return NULL;
	}
	virtual void BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1 )
	{
	}
	virtual void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
	{
	}
	virtual void Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount )
	{
	}
	virtual void BeginMorphAccumulation()
	{
	}
	virtual void EndMorphAccumulation()
	{
	}
	virtual void AccumulateMorph( IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights )
	{
	}
	virtual bool GetMorphAccumulatorTexCoord( Vector2D *pTexCoord, IMorph *pMorph, int nVertex )
	{
		pTexCoord->Init();
		return false;
	}
// ------------ End ----------------------------

	virtual ImageFormat GetNullTextureFormat() { return IMAGE_FORMAT_UNKNOWN; }

	virtual void AddTextureAlias( const char *pAlias, const char *pRealName ) {}
	virtual void RemoveTextureAlias( const char *pAlias ) {}

	virtual void SetExcludedTextures( const char *pScriptName ) {}
	virtual void UpdateExcludedTextures( void ) {}

	virtual void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights ) {}

	virtual bool SupportsBorderColor() { return false; }

	virtual IMaterial *GetCurrentMaterial() { return NULL; }
	virtual int  GetCurrentNumBones() const { return 0; }
	virtual void *GetCurrentProxy() { return NULL; }

	virtual void SetFullScreenDepthTextureValidityFlag( bool bIsValid ) {}

	// A special path used to tick the front buffer while loading on the 360
	virtual void SetNonInteractivePacifierTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedSize ) {}
	virtual void SetNonInteractiveTempFullscreenBuffer( ITexture *pTexture, MaterialNonInteractiveMode_t mode ) {}
	virtual void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode ) {}
	virtual void RefreshFrontBufferNonInteractive() {}

	virtual void *			LockRenderData( int nSizeInBytes ) { return NULL; }
	virtual void			UnlockRenderData( void *pData ) {}
	virtual bool			IsRenderData( const void *pData ) const { return false; }
	virtual void			AddRefRenderData() {}
	virtual void			ReleaseRenderData() {}
#if defined( _X360 )
	virtual void				ListUsedMaterials( void ) {}
	virtual HXUIFONT			OpenTrueTypeFont( const char *pFontname, int tall, int style )
	{
		return (HXUIFONT)0;
	}
	virtual void				CloseTrueTypeFont( HXUIFONT hFont ) {}
	virtual bool				GetTrueTypeFontMetrics( HXUIFONT hFont, XUIFontMetrics *pFontMetrics, XUICharMetrics charMetrics[256] ) 
	{
		pFontMetrics->fLineHeight = 0.0f;
		pFontMetrics->fMaxAscent = 0.0f;
		pFontMetrics->fMaxDescent = 0.0f;
		pFontMetrics->fMaxWidth = 0.0f;
		pFontMetrics->fMaxHeight = 0.0f;
		pFontMetrics->fMaxAdvance = 0.0f;
		return true;
	}

	virtual bool				GetTrueTypeGlyphs( HXUIFONT hFont, int numChars, wchar_t *pWch, int *pOffsetX, int *pOffsetY, int *pWidth, int *pHeight, unsigned char *pRGBA, int *pRGBAOffset )
	{
		return false;
	}

	virtual void				PersistDisplay() {}
	virtual void				*GetD3DDevice() { return NULL; }

	virtual void				PushVertexShaderGPRAllocation( int iVertexShaderCount = 64 ) { };
	virtual void				PopVertexShaderGPRAllocation( void ) { };

	virtual bool				OwnGPUResources( bool bEnable ) { return false; }
#endif

	virtual void				CompactMemory() {}

	// For sv_pure mode. The filesystem figures out which files the client needs to reload to be "pure" ala the server's preferences.
	virtual void ReloadFilesInList( IFileList *pFilesToReload )
	{
	}

	virtual void				PrintfVA( char *fmt, va_list vargs ) {}
	virtual void				Printf( const char *fmt, ... ) {}
	virtual float				Knob( char *knobname, float *setvalue=NULL ) { return 0.0f; }

	virtual void				SetRenderTargetFrameBufferSizeOverrides( int nWidth, int nHeight ) OVERRIDE
	{
		// Nope.
	}
	virtual void				GetRenderTargetFrameBufferDimensions( int & nWidth, int & nHeight ) OVERRIDE
	{
		GetBackBufferDimensions( nWidth, nHeight );
	}

	virtual ITexture*			CreateTextureFromBits(int w, int h, int mips, ImageFormat fmt, int srcBufferSize, byte* srcBits)
	{
		return NULL;	
	}

	virtual void OverrideRenderTargetAllocation( bool )
	{
		// anda
	
	}

	virtual ITextureCompositor*	NewTextureCompositor( int w, int h, const char* pCompositeName, int nTeamNum, uint64 randomSeed, KeyValues* stageDesc, uint texCompositeCreateFlags ) OVERRIDE
	{
		return NULL;	
	}

	virtual void AsyncFindTexture( const char* pFilename, const char *pTextureGroupName, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs, bool bComplain = true, int nAdditionalCreationFlags = 0 )
	{

	}

	virtual void AsyncCreateTextureFromRenderTarget( ITexture* pSrcRt, const char* pDstName, ImageFormat dstFmt, bool bGenMips, int nAdditionalCreationFlags, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs )
	{

	}
	
	virtual ITexture*			CreateNamedTextureFromBitsEx( const char* pName, const char *pTextureGroupName, int w, int h, int mips, ImageFormat fmt, int srcBufferSize, byte* srcBits, int nFlags )
	{
		return NULL;
	}

	virtual bool				AddTextureCompositorTemplate( const char* pName, KeyValues* pTmplDesc, int nTexCompositeTemplateFlags ) OVERRIDE
	{
		return false;
	}

	virtual bool VerifyTextureCompositorTemplates() OVERRIDE
	{
		return false;	
	}

};


static CDummyMaterialSystem g_DummyMaterialSystem;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CDummyMaterialSystem, IMaterialSystemStub, 
						MATERIAL_SYSTEM_STUB_INTERFACE_VERSION, g_DummyMaterialSystem );


