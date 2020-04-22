#ifndef MAT_STUB_H
#define MAT_STUB_H
#include "tier1/convar.h"
#include "materialsystem/itexture.h"
#include "itextureinternal.h"

// ---------------------------------------------------------------------------------------- //
// ITexture dummy implementation.
// ---------------------------------------------------------------------------------------- //

class CDummyTextureInternal : public ITextureInternal
{
public:
	CDummyTextureInternal( const char *texture_name = "dummy_texture" )
	{
		Q_strncpy( m_szTextureName, texture_name, sizeof(m_szTextureName) );
	}

	virtual void Bind( Sampler_t sampler ) {};
	virtual void Bind( Sampler_t sampler1, int nFrame, Sampler_t sampler2 = (Sampler_t) -1 ) { };

	// Methods associated with reference counting
	virtual int GetReferenceCount() { return 0; };

	virtual void GetReflectivity( Vector& reflectivity ) {};

	// Set this as the render target, return false for failure
	virtual bool SetRenderTarget( int nRenderTargetID ) { return false; };

	// Releases the texture's hw memory
	virtual void ReleaseMemory() {};

	// Called before Download() on restore. Gives render targets a change to change whether or
	// not they force themselves to have a separate depth buffer due to AA.
	virtual void OnRestore() {};

	// Resets the texture's filtering and clamping mode
	virtual void SetFilteringAndClampingMode( bool bOnlyLodValues = false ) {};

	// Used by tools.... loads up the non-fallback information about the texture 
	virtual void Precache() {};

	// Stretch blit the framebuffer into this texture.
	virtual void CopyFrameBufferToMe( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) {};
	virtual void CopyMeToFrameBuffer( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) {};

	// Get the shaderapi texture handle associated w/ a particular frame
	virtual ShaderAPITextureHandle_t GetTextureHandle( int nFrame, int nTextureChannel =0 ) { return 0; };

	static void Destroy( ITextureInternal *pTexture, bool bSkipTexMgrCheck = false ) { };

	// Set this as the render target, return false for failure
	virtual bool SetRenderTarget( int nRenderTargetID, ITexture* pDepthTexture ) { return true; };

	// Bind this to a vertex texture sampler
	virtual void BindVertexTexture( VertexTextureSampler_t sampler, int frameNum = 0 ) {};

	virtual void MarkAsPreloaded( bool bSet ) {};
	virtual bool IsPreloaded() const { return true; };

	virtual void MarkAsExcluded( bool bSet, int nDimensionsLimit ) {};
	virtual bool UpdateExcludedState( void ) { return false; };

	virtual bool IsTempRenderTarget( void ) const { return false; };

	// Reload any files the texture is responsible for.
	virtual void ReloadFilesInList( IFileList *pFilesToReload ) { };

	virtual bool AsyncReadTextureFromFile( IVTFTexture* pVTFTexture, unsigned int nAdditionalCreationFlags ) { return false; };
	virtual void AsyncCancelReadTexture() {};

	// Map and unmap. These can fail. And can cause a very significant perf penalty. Be very careful with them.
	virtual void Map( void** pOutDst, int* pOutPitch ) {};
	virtual void Unmap() {};

	// Texture streaming!
	virtual ResidencyType_t GetCurrentResidence() const { return RESIDENT_NONE; };
	virtual ResidencyType_t GetTargetResidence() const { return RESIDENT_NONE; };
	virtual bool MakeResident( ResidencyType_t newResidence ) { return false; };
	virtual void UpdateLodBias() {};

	// Various texture polling methods
	virtual const char *GetName( void ) const { return m_szTextureName; }
	virtual int GetMappingWidth() const { return 512; }
	virtual int GetMappingHeight() const { return 512; }
	virtual int GetActualWidth() const { return 512; }
	virtual int GetActualHeight() const { return 512; }
	virtual int GetNumAnimationFrames() const { return 0; }
	virtual bool IsTranslucent() const { return false; }
	virtual bool IsMipmapped() const { return false; }

	virtual void GetLowResColorSample( float s, float t, float *color ) const {}

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

private:
	char m_szTextureName[128];
};

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

extern CDummyTexture g_DummyTexture;



#endif // MAT_STUB_H
