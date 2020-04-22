//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include <stdlib.h>
#include <malloc.h>
#include "materialsystem_global.h"
#include "string.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/materialsystem_config.h"
#include "IHardwareConfigInternal.h"
#include "texturemanager.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/IColorCorrection.h"
#include "tier1/strtools.h"
#include "utlvector.h"
#include "utldict.h"
#include "itextureinternal.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "basetypes.h"
#include "utlbuffer.h"
#include "filesystem.h"
#include "materialsystem/imesh.h"
#include "materialsystem/ishaderapi.h"
#include "vstdlib/random.h"
#include "imorphinternal.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlpair.h"
#include "ctype.h"
#include "utlqueue.h"
#include "tier0/icommandline.h"
#include "ctexturecompositor.h"

#include "vprof_telemetry.h"

// Need lightmaps access here
#define MATSYS_INTERNAL
#include "cmatlightmaps.h"
#include "cmaterialsystem.h"
#undef MATSYS_INTERNAL

#include "tier0/memdbgon.h"

#define ERROR_TEXTURE_SIZE				32
#define WHITE_TEXTURE_SIZE				1
#define BLACK_TEXTURE_SIZE				1
#define GREY_TEXTURE_SIZE				1
#define NORMALIZATION_CUBEMAP_SIZE		32

struct AsyncLoadJob_t;
struct AsyncReadJob_t;
class AsyncLoader;
class AsyncReader;

#define MAX_READS_OUTSTANDING 2

static ImageFormat GetImageFormatRawReadback( ImageFormat fmt );

#ifdef STAGING_ONLY
	static ConVar mat_texture_list_dump( "mat_texture_list_dump", "0" );
#endif

const char* cTextureCachePathDir = "__texture_cache";

// TODO: Relocate this somewhere else. It works like python's "strip" function,
// removing leading and trailing whitespace, including newlines. Whitespace between
// non-whitespace characters is preserved.
void V_StripWhitespace( char* pBuffer )
{
	Assert( pBuffer );

	char* pSrc = pBuffer;
	char* pDst = pBuffer;
	char* pDstFirstTrailingWhitespace = NULL;
	
	// Remove leading whitespace
	bool leading = true;
	while ( *pSrc )
	{
		if ( leading )
		{
			if ( V_isspace( *pSrc ) )
			{
				++pSrc;
				continue;
			}
			else
			{
				leading = false;
				// Drop through
			}
		}

		if  ( pDst != pSrc )
			*pDst = *pSrc;

		if ( !leading && V_isspace( *pDst ) && pDstFirstTrailingWhitespace == NULL )
			pDstFirstTrailingWhitespace = pDst;
		else if ( !leading && !V_isspace( *pDst ) && pDstFirstTrailingWhitespace != NULL )
			pDstFirstTrailingWhitespace = NULL;

		++pSrc;
		++pDst;
	}

	(*pDst) = 0;

	if ( pDstFirstTrailingWhitespace )
		( *pDstFirstTrailingWhitespace ) = 0;
}

//-----------------------------------------------------------------------------
//
// Various procedural texture regeneration classes
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Creates a checkerboard texture
//-----------------------------------------------------------------------------
class CCheckerboardTexture : public ITextureRegenerator
{
public:
	CCheckerboardTexture( int nCheckerSize, color32 color1, color32 color2 ) :
		m_nCheckerSize( nCheckerSize ), m_Color1(color1), m_Color2(color2)
	{
	}

	virtual ~CCheckerboardTexture() { }

	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		for (int iFrame = 0; iFrame < pVTFTexture->FrameCount(); ++iFrame )
		{
			for (int iFace = 0; iFace < pVTFTexture->FaceCount(); ++iFace )
			{
				int nWidth = pVTFTexture->Width();
				int nHeight = pVTFTexture->Height();
				int nDepth = pVTFTexture->Depth();
				for (int z = 0; z < nDepth; ++z)
				{
					// Fill mip 0 with a checkerboard
					CPixelWriter pixelWriter;
					pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
						pVTFTexture->ImageData( iFrame, iFace, 0, 0, 0, z ), pVTFTexture->RowSizeInBytes( 0 ) );
					
					for (int y = 0; y < nHeight; ++y)
					{
						pixelWriter.Seek( 0, y );
						for (int x = 0; x < nWidth; ++x)
						{
							if ( ((x & m_nCheckerSize) ^ (y & m_nCheckerSize)) ^ (z & m_nCheckerSize) )
							{
								pixelWriter.WritePixel( m_Color1.r, m_Color1.g, m_Color1.b, m_Color1.a );
							}
							else
							{
								pixelWriter.WritePixel( m_Color2.r, m_Color2.g, m_Color2.b, m_Color2.a );
							}
						}
					}
				}
			}
		}
	}

	virtual void Release()
	{
		delete this;
	}

private:
	int		m_nCheckerSize;
	color32 m_Color1;
	color32 m_Color2;
};

static void CreateCheckerboardTexture( ITextureInternal *pTexture, int nCheckerSize, color32 color1, color32 color2 )
{
	ITextureRegenerator *pRegen = new CCheckerboardTexture( nCheckerSize, color1, color2 );
	pTexture->SetTextureRegenerator( pRegen );
}


//-----------------------------------------------------------------------------
// Creates a solid texture
//-----------------------------------------------------------------------------
class CSolidTexture : public ITextureRegenerator
{
public:
	CSolidTexture( color32 color ) : m_Color(color)
	{
	}

	virtual ~CSolidTexture() { }

	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		int nMipCount = pTexture->IsMipmapped() ? pVTFTexture->MipCount() : 1;
		for (int iFrame = 0; iFrame < pVTFTexture->FrameCount(); ++iFrame )
		{
			for (int iFace = 0; iFace < pVTFTexture->FaceCount(); ++iFace )
			{
				for (int iMip = 0; iMip < nMipCount; ++iMip )
				{
					int nWidth, nHeight, nDepth;
					pVTFTexture->ComputeMipLevelDimensions( iMip, &nWidth, &nHeight, &nDepth );
					for (int z = 0; z < nDepth; ++z)
					{
						CPixelWriter pixelWriter;
						pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
							pVTFTexture->ImageData( iFrame, iFace, iMip, 0, 0, z ), pVTFTexture->RowSizeInBytes( iMip ) );
					
						for (int y = 0; y < nHeight; ++y)
						{
							pixelWriter.Seek( 0, y );
							for (int x = 0; x < nWidth; ++x)
							{
								pixelWriter.WritePixel( m_Color.r, m_Color.g, m_Color.b, m_Color.a );
							}
						}
					}
				}
			}
		}
	}

	virtual void Release()
	{
		delete this;
	}

private:
	color32 m_Color;
};

static void CreateSolidTexture( ITextureInternal *pTexture, color32 color )
{
	ITextureRegenerator *pRegen = new CSolidTexture( color );
	pTexture->SetTextureRegenerator( pRegen );
}

//-----------------------------------------------------------------------------
// Creates a normalization cubemap texture
//-----------------------------------------------------------------------------
class CNormalizationCubemap : public ITextureRegenerator
{
public:
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		// Normalization cubemap doesn't make sense on low-end hardware
		// So we won't construct a spheremap out of this
		CPixelWriter pixelWriter;

		Vector direction;
		for (int iFace = 0; iFace < 6; ++iFace)
		{
			pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
				pVTFTexture->ImageData( 0, iFace, 0 ), pVTFTexture->RowSizeInBytes( 0 ) );
			
			int nWidth = pVTFTexture->Width();
			int nHeight = pVTFTexture->Height();

			float flInvWidth = 2.0f / (float)(nWidth-1);
			float flInvHeight = 2.0f / (float)(nHeight-1);

			for (int y = 0; y < nHeight; ++y)
			{
				float v = y * flInvHeight - 1.0f;

				pixelWriter.Seek( 0, y );
				for (int x = 0; x < nWidth; ++x)
				{
					float u = x * flInvWidth - 1.0f;
					float oow = 1.0f / sqrt( 1.0f + u*u + v*v );

					int ix = (int)(255.0f * 0.5f * (u*oow + 1.0f) + 0.5f);
					ix = clamp( ix, 0, 255 );
					int iy = (int)(255.0f * 0.5f * (v*oow + 1.0f) + 0.5f);
					iy = clamp( iy, 0, 255 );
					int iz = (int)(255.0f * 0.5f * (oow + 1.0f) + 0.5f);
					iz = clamp( iz, 0, 255 );

					switch (iFace)
					{
					case CUBEMAP_FACE_RIGHT:
						pixelWriter.WritePixel( iz, 255 - iy, 255 - ix, 255 );
						break;
					case CUBEMAP_FACE_LEFT:
						pixelWriter.WritePixel( 255 - iz, 255 - iy, ix, 255 );
						break;
					case CUBEMAP_FACE_BACK:	
						pixelWriter.WritePixel( ix, iz, iy, 255 );
						break;
					case CUBEMAP_FACE_FRONT:
						pixelWriter.WritePixel( ix, 255 - iz, 255 - iy, 255 );
						break;
					case CUBEMAP_FACE_UP:
						pixelWriter.WritePixel( ix, 255 - iy, iz, 255 );
						break;
					case CUBEMAP_FACE_DOWN:
						pixelWriter.WritePixel( 255 - ix, 255 - iy, 255 - iz, 255 );
						break;
					default:
						break;
					}
				}
			}
		}
	}

	// NOTE: The normalization cubemap regenerator is stateless
	// so there's no need to allocate + deallocate them
	virtual void Release() {}
};

//-----------------------------------------------------------------------------
// Creates a normalization cubemap texture
//-----------------------------------------------------------------------------
class CSignedNormalizationCubemap : public ITextureRegenerator
{
public:
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		// Normalization cubemap doesn't make sense on low-end hardware
		// So we won't construct a spheremap out of this
		CPixelWriter pixelWriter;

		Vector direction;
		for (int iFace = 0; iFace < 6; ++iFace)
		{
			pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
				pVTFTexture->ImageData( 0, iFace, 0 ), pVTFTexture->RowSizeInBytes( 0 ) );
			
			int nWidth = pVTFTexture->Width();
			int nHeight = pVTFTexture->Height();

			float flInvWidth = 2.0f / (float)(nWidth-1);
			float flInvHeight = 2.0f / (float)(nHeight-1);

			for (int y = 0; y < nHeight; ++y)
			{
				float v = y * flInvHeight - 1.0f;

				pixelWriter.Seek( 0, y );
				for (int x = 0; x < nWidth; ++x)
				{
					float u = x * flInvWidth - 1.0f;
					float oow = 1.0f / sqrt( 1.0f + u*u + v*v );

#ifdef DX_TO_GL_ABSTRACTION
					float flX = (255.0f * 0.5 * (u*oow + 1.0f) + 0.5f);
					float flY = (255.0f * 0.5 * (v*oow + 1.0f) + 0.5f);
					float flZ = (255.0f * 0.5 * (oow + 1.0f) + 0.5f);

					switch (iFace)
					{
						case CUBEMAP_FACE_RIGHT:
							flX = 255.0f - flX;
							flY = 255.0f - flY;
							break;
						case CUBEMAP_FACE_LEFT:
							flY = 255.0f - flY;
							flZ = 255.0f - flZ;
							break;
						case CUBEMAP_FACE_BACK:	
							break;
						case CUBEMAP_FACE_FRONT:
							flY = 255.0f - flY;
							flZ = 255.0f - flZ;
							break;
						case CUBEMAP_FACE_UP:
							flY = 255.0f - flY;
							break;
						case CUBEMAP_FACE_DOWN:
							flX = 255.0f - flX;
							flY = 255.0f - flY;
							flZ = 255.0f - flZ;
							break;
						default:
							break;
					}

					flX -= 128.0f;
					flY -= 128.0f;
					flZ -= 128.0f;

					flX /= 128.0f;
					flY /= 128.0f;
					flZ /= 128.0f;

					switch ( iFace )
					{
						case CUBEMAP_FACE_RIGHT:
							pixelWriter.WritePixelF( flZ, flY, flX, 0.0f );
							break;
						case CUBEMAP_FACE_LEFT:
							pixelWriter.WritePixelF( flZ, flY, flX, 0.0f );
							break;
						case CUBEMAP_FACE_BACK:	
							pixelWriter.WritePixelF( flX,  flZ,  flY, 0.0f );
							break;
						case CUBEMAP_FACE_FRONT:
							pixelWriter.WritePixelF( flX,  flZ,  flY, 0.0f );
							break;
						case CUBEMAP_FACE_UP:
							pixelWriter.WritePixelF( flX, flY,  flZ, 0.0f );
							break;
						case CUBEMAP_FACE_DOWN:
							pixelWriter.WritePixelF( flX, flY, flZ, 0.0f );
							break;
						default:
							break;
					}
#else
					int ix = (int)(255 * 0.5 * (u*oow + 1.0f) + 0.5f);
					ix = clamp( ix, 0, 255 );
					int iy = (int)(255 * 0.5 * (v*oow + 1.0f) + 0.5f);
					iy = clamp( iy, 0, 255 );
					int iz = (int)(255 * 0.5 * (oow + 1.0f) + 0.5f);
					iz = clamp( iz, 0, 255 );

					switch (iFace)
					{
					case CUBEMAP_FACE_RIGHT:
						ix = 255 - ix;
						iy = 255 - iy;
						break;
					case CUBEMAP_FACE_LEFT:
						iy = 255 - iy;
						iz = 255 - iz;
						break;
					case CUBEMAP_FACE_BACK:	
						break;
					case CUBEMAP_FACE_FRONT:
						iy = 255 - iy;
						iz = 255 - iz;
						break;
					case CUBEMAP_FACE_UP:
						iy = 255 - iy;
						break;
					case CUBEMAP_FACE_DOWN:
						ix = 255 - ix;
						iy = 255 - iy;
						iz = 255 - iz;
						break;
					default:
						break;
					}

					ix -= 128;
					iy -= 128;
					iz -= 128;

					Assert( ix >= -128 && ix <= 127 );
					Assert( iy >= -128 && iy <= 127 );
					Assert( iz >= -128 && iz <= 127 );

					switch (iFace)
					{
					case CUBEMAP_FACE_RIGHT:
						// correct
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( iz, iy, ix, 0 );
						break;
					case CUBEMAP_FACE_LEFT:
						// correct
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( iz, iy, ix, 0 );
						break;
					case CUBEMAP_FACE_BACK:	
						// wrong
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( ix, iz, iy, 0 );
//						pixelWriter.WritePixelSigned( -127, -127, 127, 0 );
						break;
					case CUBEMAP_FACE_FRONT:
						// wrong
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( ix, iz, iy, 0 );
						break;
					case CUBEMAP_FACE_UP:
						// correct
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( ix, iy, iz, 0 );
						break;
					case CUBEMAP_FACE_DOWN:
						// correct
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( ix, iy, iz, 0 );
						break;
					default:
						break;
					}
#endif
				} // x
			} // y
		} // iFace
	}

	// NOTE: The normalization cubemap regenerator is stateless
	// so there's no need to allocate + deallocate them
	virtual void Release() {}
};

static void CreateNormalizationCubemap( ITextureInternal *pTexture )
{
	// NOTE: The normalization cubemap regenerator is stateless
	// so there's no need to allocate + deallocate them
	static CNormalizationCubemap s_NormalizationCubemap;
	pTexture->SetTextureRegenerator( &s_NormalizationCubemap );
}

static void CreateSignedNormalizationCubemap( ITextureInternal *pTexture )
{
	// NOTE: The normalization cubemap regenerator is stateless
	// so there's no need to allocate + deallocate them
	static CSignedNormalizationCubemap s_SignedNormalizationCubemap;
	pTexture->SetTextureRegenerator( &s_SignedNormalizationCubemap );
}

//-----------------------------------------------------------------------------
// Creates a color correction texture
//-----------------------------------------------------------------------------
class CColorCorrectionTexture : public ITextureRegenerator
{
public:
	CColorCorrectionTexture( ColorCorrectionHandle_t handle ) : m_ColorCorrectionHandle(handle)
	{
	}

	virtual ~CColorCorrectionTexture() { }

	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		int nWidth = pVTFTexture->Width();
		int nHeight = pVTFTexture->Height();
		int nDepth = pVTFTexture->Depth();
		Assert( nWidth == COLOR_CORRECTION_TEXTURE_SIZE && nHeight == COLOR_CORRECTION_TEXTURE_SIZE && nDepth == COLOR_CORRECTION_TEXTURE_SIZE );

		for ( int z = 0; z < nDepth; ++z )
		{
			CPixelWriter pixelWriter;
			pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
				pVTFTexture->ImageData( 0, 0, 0, 0, 0, z ), pVTFTexture->RowSizeInBytes( 0 ) );

			for ( int y = 0; y < nHeight; ++y )
			{
				pixelWriter.Seek( 0, y );
				for (int x = 0; x < nWidth; ++x)
				{
					RGBX5551_t inColor;
					inColor.r = x;
					inColor.g = y;
					inColor.b = z;

					color24 col = ColorCorrectionSystem()->GetLookup( m_ColorCorrectionHandle, inColor );
					pixelWriter.WritePixel( col.r, col.g, col.b, 255 );
				}
			}
		}
	}

	virtual void Release() 
	{
		delete this;
	}

private:
	ColorCorrectionHandle_t	m_ColorCorrectionHandle;
};


void CreateColorCorrectionTexture( ITextureInternal *pTexture, ColorCorrectionHandle_t handle )
{
	ITextureRegenerator *pRegen = new CColorCorrectionTexture( handle );
	pTexture->SetTextureRegenerator( pRegen );
}

//-----------------------------------------------------------------------------
// Implementation of the texture manager
//-----------------------------------------------------------------------------
class CTextureManager : public ITextureManager
{
public:
	CTextureManager( void );

	// Initialization + shutdown
	virtual void Init( int nFlags ) OVERRIDE;
	virtual void Shutdown();

	virtual void AllocateStandardRenderTargets( );
	virtual void FreeStandardRenderTargets();

	virtual void CacheExternalStandardRenderTargets();

	virtual ITextureInternal *CreateProceduralTexture( const char *pTextureName, const char *pTextureGroupName, int w, int h, int d, ImageFormat fmt, int nFlags, ITextureRegenerator* generator = NULL );
	virtual ITextureInternal *FindOrLoadTexture( const char *textureName, const char *pTextureGroupName, int nAdditionalCreationFlags = 0 );
 	virtual bool IsTextureLoaded( const char *pTextureName );

	virtual void AddTextureAlias( const char *pAlias, const char *pRealName );
	virtual void RemoveTextureAlias( const char *pAlias );

	virtual void SetExcludedTextures( const char *pScriptName );
	virtual void UpdateExcludedTextures();

	virtual void ResetTextureFilteringState();
	void ReloadTextures( void );

	// These are used when we lose our video memory due to a mode switch etc
	void ReleaseTextures( void );
	void RestoreNonRenderTargetTextures( void );
	void RestoreRenderTargets( void );

	// Suspend or resume texture streaming requests
	void SuspendTextureStreaming( void );
	void ResumeTextureStreaming( void );

	// delete any texture that has a refcount <= 0
	void RemoveUnusedTextures( void );
	void DebugPrintUsedTextures( void );

	// Request a texture ID
	virtual int	RequestNextTextureID();

	// Get at a couple standard textures
	virtual ITextureInternal *ErrorTexture();
	virtual ITextureInternal *NormalizationCubemap();
	virtual ITextureInternal *SignedNormalizationCubemap();
	virtual ITextureInternal *ShadowNoise2D();
	virtual ITextureInternal *IdentityLightWarp();
	virtual ITextureInternal *ColorCorrectionTexture( int i );
	virtual ITextureInternal *FullFrameDepthTexture();
	virtual ITextureInternal *DebugLuxels2D();


	// Generates an error texture pattern
	virtual void GenerateErrorTexture( ITexture *pTexture, IVTFTexture *pVTFTexture );

	// Updates the color correction state
	virtual void SetColorCorrectionTexture( int i, ITextureInternal *pTexture );

	virtual void ForceAllTexturesIntoHardware( void );

	virtual ITextureInternal *CreateRenderTargetTexture( 
		const char *pRTName,	// NULL for auto-generated name
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode, 
		ImageFormat fmt, 
		RenderTargetType_t type, 
		unsigned int textureFlags, 
		unsigned int renderTargetFlags );

	virtual bool HasPendingTextureDestroys() const;
	virtual void MarkUnreferencedTextureForCleanup( ITextureInternal *pTexture );
	virtual void RemoveTexture( ITextureInternal *pTexture );
	virtual void ReloadFilesInList( IFileList *pFilesToReload );

	// start with -1, list terminates with -1
	virtual int FindNext( int iIndex, ITextureInternal **ppTexture );

	virtual void ReleaseTempRenderTargetBits( void );

	// Called once per frame by material system "somewhere."
	virtual void Update();

	// Load a texture asynchronously and then call the provided callback.
	virtual void AsyncFindOrLoadTexture( const char *pTextureName, const char *pTextureGroupName, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs, bool bComplain, int nAdditionalCreationFlags );
	void CompleteAsyncLoad( AsyncLoadJob_t* pJob );

	virtual void AsyncCreateTextureFromRenderTarget( ITexture* pSrcRt, const char* pDstName, ImageFormat dstFmt, bool bGenMips, int nAdditionalCreationFlags, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs );
	void CompleteAsyncRead( AsyncReadJob_t* pJob );

	ITextureInternal* AcquireReadbackTexture( int w, int h, ImageFormat fmt );
	void ReleaseReadbackTexture( ITextureInternal* pTex );

	void WarmTextureCache();
	void CoolTextureCache();

	virtual void RequestAllMipmaps( ITextureInternal* pTex );
	virtual void EvictAllTextures();
	virtual void UpdatePostAsync();

	virtual void ReleaseAsyncScratchVTF( IVTFTexture* pScratchVTF );

	virtual bool ThreadInAsyncLoadThread() const;
	virtual bool ThreadInAsyncReadThread() const;

	virtual bool AddTextureCompositorTemplate( const char* pName, KeyValues* pTmplDesc ) OVERRIDE;
	virtual bool VerifyTextureCompositorTemplates() OVERRIDE;

	virtual CTextureCompositorTemplate* FindTextureCompositorTemplate( const char* pName ) OVERRIDE;

protected:
	ITextureInternal *FindTexture( const char *textureName );
	ITextureInternal *LoadTexture( const char *textureName, const char *pTextureGroupName, int nAdditionalCreationFlags = 0, bool bDownload = true );

	void AsyncLoad( const AsyncLoadJob_t& job );
	void AsyncReadTexture( AsyncReadJob_t* job );

	// Restores a single texture
	void RestoreTexture( ITextureInternal* pTex );

	void CleanupPossiblyUnreferencedTextures();

#ifdef STAGING_ONLY
	void DumpTextureList( );
#endif

	void FindFilesToLoad( CUtlDict< int >* pOutFilesToLoad, const char* pFilename );
	void ReadFilesToLoad( CUtlDict< int >* pOutFilesToLoad, const char* pFilename );

	CUtlDict< ITextureInternal *, unsigned short > m_TextureList;
	CUtlDict< const char *, unsigned short > m_TextureAliases;
	CUtlDict< int, unsigned short > m_TextureExcludes;	
	CUtlDict< CCopyableUtlVector<AsyncLoadJob_t> > m_PendingAsyncLoads;
	CUtlVector< ITextureInternal* > m_ReadbackTextures;
	CUtlVector< ITextureInternal* >			m_preloadedTextures;
	CUtlMap< ITextureInternal*, int >		m_textureStreamingRequests;
	CTSQueue< ITextureInternal* >			m_asyncStreamingRequests;
	CTSQueue< ITextureInternal * >			m_PossiblyUnreferencedTextures;

	CUtlDict< CTextureCompositorTemplate *, unsigned short > m_TexCompTemplates;


	int m_iNextTexID;
	int m_nFlags;

	ITextureInternal *m_pErrorTexture;
	ITextureInternal *m_pBlackTexture;
	ITextureInternal *m_pWhiteTexture;
	ITextureInternal *m_pGreyTexture;
	ITextureInternal *m_pGreyAlphaZeroTexture;
	ITextureInternal *m_pNormalizationCubemap;
	ITextureInternal *m_pFullScreenTexture;
	ITextureInternal *m_pSignedNormalizationCubemap;
	ITextureInternal *m_pShadowNoise2D;
	ITextureInternal *m_pIdentityLightWarp;
	ITextureInternal *m_pColorCorrectionTextures[ COLOR_CORRECTION_MAX_TEXTURES ];
	ITextureInternal *m_pFullScreenDepthTexture;
	ITextureInternal *m_pDebugLuxels2D;

	// Used to generate various error texture patterns when necessary
	CCheckerboardTexture *m_pErrorRegen;

	friend class AsyncLoader;
	AsyncLoader* m_pAsyncLoader;

	friend class AsyncReader;
	AsyncReader* m_pAsyncReader;

	uint m_nAsyncLoadThread;
	uint m_nAsyncReadThread;

	int m_iSuspendTextureStreaming;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CTextureManager s_TextureManager;
ITextureManager *g_pTextureManager = &s_TextureManager;

struct AsyncLoadJob_t 
{
	CUtlString m_TextureName;
	CUtlString m_TextureGroupName;
	IAsyncTextureOperationReceiver* m_pRecipient;
	void* m_pExtraArgs;
	bool m_bComplain;
	int m_nAdditionalCreationFlags;
	ITextureInternal* m_pResultData;

	AsyncLoadJob_t()
	: m_pRecipient( NULL )
	, m_pExtraArgs( NULL )
	, m_bComplain( false )
	, m_nAdditionalCreationFlags( 0 )
	, m_pResultData( NULL )
	{ }

	AsyncLoadJob_t( const char *pTextureName, const char *pTextureGroupName, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs, bool bComplain, int nAdditionalCreationFlags )
	: m_TextureName( pTextureName )
	, m_TextureGroupName( pTextureGroupName )
	, m_pRecipient( pRecipient )
	, m_pExtraArgs( pExtraArgs )
	, m_bComplain( bComplain )
	, m_nAdditionalCreationFlags( nAdditionalCreationFlags )
	, m_pResultData( NULL )
	{

	}
};


class CAsyncCopyRequest : public IAsyncTextureOperationReceiver
{
public:
	CAsyncCopyRequest()
	: m_nReferenceCount( 0 )
	, m_bSignalled( false )
	{ }

	virtual ~CAsyncCopyRequest() { }

	virtual int AddRef() OVERRIDE{ return ++m_nReferenceCount; }
	virtual int Release() OVERRIDE
	{
		int retVal = --m_nReferenceCount;
		if ( retVal == 0 )
			delete this;

		return retVal;
	}

	virtual int GetRefCount() const OVERRIDE{ return m_nReferenceCount; }

	virtual void OnAsyncCreateComplete( ITexture* pTex, void* pExtraArgs ) OVERRIDE { }
	virtual void OnAsyncFindComplete( ITexture* pTex, void* pExtraArgs ) OVERRIDE { }
	virtual void OnAsyncMapComplete( ITexture* pTex, void* pExtraArgs, void* pMemory, int nPitch ) OVERRIDE { }
	virtual void OnAsyncReadbackBegin( ITexture* pDst, ITexture* pSrc, void* pExtraArgs ) OVERRIDE
	{
		m_bSignalled = true;
	}

	bool IsSignalled() const { return m_bSignalled; }

private:
	CInterlockedInt m_nReferenceCount;
	volatile bool m_bSignalled;
};

class CAsyncMapResult : public IAsyncTextureOperationReceiver
{
public:
	CAsyncMapResult( ITextureInternal* pTex ) 
	: m_pTexToMap( pTex ) 
	, m_nReferenceCount( 0 )
	, m_pMemory( NULL )
	, m_nPitch( 0 )
	, m_bSignalled( false )
	{ }

	virtual ~CAsyncMapResult() { }

	virtual int AddRef() OVERRIDE { return ++m_nReferenceCount; }
	virtual int Release() OVERRIDE
	{
		int retVal = --m_nReferenceCount;
		if ( retVal == 0 )
			delete this;

		return retVal;	
	}

	virtual int GetRefCount() const OVERRIDE{ return m_nReferenceCount; }

	virtual void OnAsyncCreateComplete( ITexture* pTex, void* pExtraArgs ) OVERRIDE { }
	virtual void OnAsyncFindComplete( ITexture* pTex, void* pExtraArgs ) OVERRIDE { }
	virtual void OnAsyncMapComplete( ITexture* pTex, void* pExtraArgs, void* pMemory, int nPitch ) OVERRIDE
	{
		Assert( pTex == m_pTexToMap );
		m_pMemory = pMemory;
		m_nPitch = nPitch;
		m_bSignalled = true;
	}

	virtual void OnAsyncReadbackBegin( ITexture* pDst, ITexture* pSrc, void* pExtraArgs ) OVERRIDE { }


	bool IsSignalled() const { return m_bSignalled; }

	ITextureInternal* const m_pTexToMap;
	CInterlockedInt m_nReferenceCount;
	volatile void* m_pMemory; 
	volatile int m_nPitch;

private:
	volatile bool m_bSignalled;
};

struct AsyncReadJob_t 
{
	ITexture* m_pSrcRt;
	ITextureInternal* m_pSysmemTex;
	CAsyncCopyRequest* m_pAsyncRead;
	CAsyncMapResult* m_pAsyncMap;
	const char* m_pDstName;
	ImageFormat m_dstFmt;
	bool m_bGenMips;
	int m_nAdditionalCreationFlags;
	IAsyncTextureOperationReceiver* m_pRecipient;
	void* m_pExtraArgs;

	CUtlMemory<unsigned char> m_finalTexelData;

	AsyncReadJob_t()
	: m_pSrcRt( NULL )
	, m_pSysmemTex( NULL )
	, m_pAsyncRead( NULL )
	, m_pAsyncMap( NULL )
	, m_pDstName( NULL )
	, m_dstFmt( IMAGE_FORMAT_UNKNOWN )
	, m_bGenMips( false )
	, m_nAdditionalCreationFlags( 0 )
	, m_pRecipient( NULL )
	, m_pExtraArgs( NULL )
	{ }

	AsyncReadJob_t( ITexture* pSrcRt, const char* pDstName, ImageFormat dstFmt, bool bGenMips, int nAdditionalCreationFlags, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs )
	: m_pSrcRt( pSrcRt )
	, m_pSysmemTex( NULL )
	, m_pAsyncRead( NULL )
	, m_pAsyncMap( NULL )
	, m_pDstName( pDstName ) // We take ownership of this string.
	, m_dstFmt( dstFmt )
	, m_bGenMips( bGenMips )
	, m_nAdditionalCreationFlags( nAdditionalCreationFlags )
	, m_pRecipient( pRecipient )
	, m_pExtraArgs( pExtraArgs )
	{ 
	
	}

	~AsyncReadJob_t()
	{
		Assert( ThreadInMainThread() );

		delete [] m_pDstName; 

		SafeRelease( &m_pRecipient );

		if ( m_pSysmemTex )
		{
			if ( m_pAsyncMap )
			{
				extern CMaterialSystem g_MaterialSystem;
				g_MaterialSystem.GetRenderContextInternal()->AsyncUnmap( m_pSysmemTex );
			}

			assert_cast< CTextureManager* >( g_pTextureManager )->ReleaseReadbackTexture( m_pSysmemTex );
			m_pSysmemTex = NULL;
		}

		SafeRelease( &m_pAsyncMap );
	}

};

bool IsJobCancelled( AsyncReadJob_t* pJob )
{
	Assert( pJob != NULL );

	// The texture manager holds a reference to the object, so if we're the only one who is holding a ref
	// then the job has been abandoned. This gives us the opportunity to cleanup and skip some work.
	if ( pJob->m_pRecipient->GetRefCount() == 1 )
	{
		return true;
	}

	return false;
}

bool IsJobCancelled( AsyncLoadJob_t* pJob )
{
	Assert( pJob != NULL );

	// The texture manager holds a reference to the object, so if we're the only one who is holding a ref
	// then the job has been abandoned. This gives us the opportunity to cleanup and skip some work.
	if ( pJob->m_pRecipient->GetRefCount() == 1 )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Functions can be called from any thread, unless they are prefixed with a thread name. 
class AsyncLoader
{
public:
	AsyncLoader()
	: m_bQuit( false )
	{
		for ( int i = 0; i < MAX_READS_OUTSTANDING; ++i )
		{
			m_asyncScratchVTFs.PushItem( CreateVTFTexture() );
		}

		// Do this after everything else. 
		m_LoaderThread = CreateSimpleThread( AsyncLoader::LoaderMain, this );
	}

	~AsyncLoader()
	{
		Assert( m_asyncScratchVTFs.Count() == MAX_READS_OUTSTANDING );
		while ( m_asyncScratchVTFs.Count() > 0 )
		{
			IVTFTexture* pScratchVTF = NULL;
			m_asyncScratchVTFs.PopItem( &pScratchVTF );
			delete pScratchVTF;
		}
	}

	void AsyncLoad( const AsyncLoadJob_t& job )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		// TODO: This could be made faster by keeping a pool of these things.
		m_pendingJobs.PushItem( new AsyncLoadJob_t( job ) );
	}

	void Shutdown()
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		m_bQuit = true;
		ThreadJoin( m_LoaderThread );		
	}

	void ThreadMain_Update()
	{
		Assert( ThreadInMainThread() );
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
		
		AsyncLoadJob_t *pJob = NULL;
		if ( m_completedJobs.PopItem( &pJob ) )
		{
			Assert( pJob != NULL );

			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - CompleteAsyncLoad", __FUNCTION__ );
			// Complete the load, then make the callback.
			assert_cast< CTextureManager* >( g_pTextureManager )->CompleteAsyncLoad( pJob );
			delete pJob;
			pJob = NULL;
		}
	}

	void ReleaseAsyncReadBuffer( IVTFTexture *pScratchVTF )
	{
		Assert( pScratchVTF != NULL );
		m_asyncScratchVTFs.PushItem( pScratchVTF  );	
	}

private:
	inline bool ThreadInLoaderThread() 
	{
		return s_TextureManager.ThreadInAsyncLoadThread();
	}

	void ThreadLoader_Main( )
	{
		Assert( ThreadInLoaderThread() );

		while ( !m_bQuit )
		{
			AsyncLoadJob_t *pJob = NULL;
			IVTFTexture *pScratchVTF = NULL;
			while ( !m_pendingJobs.PopItem( &pJob ) )
			{
				// "awhile"
				ThreadSleep( 8 );
				if ( m_bQuit )
					return;
			}
			Assert( pJob != NULL );

			while ( !m_asyncScratchVTFs.PopItem( &pScratchVTF ) )
			{
				// Also awhile, but not as long..
				ThreadSleep( 4 );
				if ( m_bQuit )
					return;
			}
			Assert( pScratchVTF != NULL );

			ThreadLoader_ProcessLoad( pJob, pScratchVTF );
		}
	}

	void ThreadLoader_ProcessLoad( AsyncLoadJob_t *pJob, IVTFTexture* pScratchVTF )
	{
		Assert( ThreadInLoaderThread() );
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		Assert( pJob->m_pResultData );

		if ( !pJob->m_pResultData->AsyncReadTextureFromFile( pScratchVTF, pJob->m_nAdditionalCreationFlags ) )
			m_asyncScratchVTFs.PushItem( pScratchVTF );

		m_completedJobs.PushItem( pJob );
	}

	static unsigned LoaderMain( void* _this )
	{
		ThreadSetDebugName( "Loader" );

		s_TextureManager.m_nAsyncLoadThread = ThreadGetCurrentId();
		( ( AsyncLoader* )_this )->ThreadLoader_Main();
		s_TextureManager.m_nAsyncLoadThread = 0xFFFFFFFF;
		return 0;
	}

	ThreadHandle_t m_LoaderThread; 
	volatile bool m_bQuit;

	CTSQueue< AsyncLoadJob_t *> m_pendingJobs;
	CTSQueue< AsyncLoadJob_t *> m_completedJobs;
	CTSQueue< IVTFTexture *> m_asyncScratchVTFs;
};

//-----------------------------------------------------------------------------
// Functions can be called from any thread, unless they are prefixed with a thread name. 
class AsyncReader
{
public:
	AsyncReader()
	: m_bQuit( false )
	{

		// Do this after everything else. 
		m_HelperThread = CreateSimpleThread( AsyncReader::ReaderMain, this );
	}

	void AsyncReadback( AsyncReadJob_t* job )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		m_requestedCopies.PushItem( job );
	}

	void Shutdown()
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		m_bQuit = true;
		ThreadJoin( m_HelperThread );
	}

	void ThreadMain_Update()
	{
		Assert( ThreadInMainThread() );
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		while ( !m_queuedMaps.IsEmpty() )
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "CompleteMap" );
			AsyncReadJob_t* pMapped = m_queuedMaps.Head();
			Assert( pMapped != NULL );
			{
				if ( IsJobCancelled( pMapped ) )
				{
					// Remove the head, which is pMapped
					m_queuedMaps.RemoveAtHead();
					delete pMapped;
					continue;
				}
				
				if ( pMapped->m_pAsyncMap->IsSignalled() )
				{
					if ( pMapped->m_pAsyncMap->m_pMemory != 0 && pMapped->m_pAsyncMap->m_nPitch != 0 )
					{
						// Stick it in the queue for the other thread to work on it.
						m_pendingJobs.PushItem( pMapped );
					}
					else
					{
						Assert( !"Failed to perform a map that shouldn't fail, need to deal with this if it ever happens." );
						DevWarning( "Failed to perform a map that shouldn't fail, need to deal with this if it ever happens." );
					}
					
					// Remove the head, which is pMapped
					m_queuedMaps.RemoveAtHead();
				}

				// Stop as soon as we complete one, regardless of success.
				break;
			}

		}

		// This is ugly, but basically we need to do map and unmap on the main thread. Other
		// stuff can (mostly) happen on the async thread
		while ( !m_queuedReads.IsEmpty() )
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "CompleteQueuedRead" );

			AsyncReadJob_t* pRead = NULL;
			if ( m_queuedReads.RemoveAtHead( pRead ) )
			{
				if ( IsJobCancelled( pRead ) )
				{
					delete pRead;
					continue;
				}

				SafeAssign( &pRead->m_pAsyncMap, new CAsyncMapResult( pRead->m_pSysmemTex ) );
				// Trigger the map.
				extern CMaterialSystem g_MaterialSystem;
				g_MaterialSystem.GetRenderContextInternal()->AsyncMap( pRead->m_pSysmemTex, pRead->m_pAsyncMap, NULL );
				m_queuedMaps.Insert( pRead );

				// Stop as soon as we complete one successfully.
				break;
			}
		}

		if ( !m_scheduledReads.IsEmpty() )
		{
			if ( m_scheduledReads.Head()->m_pAsyncRead->IsSignalled() )
			{
				AsyncReadJob_t* pScheduledRead = m_scheduledReads.RemoveAtHead();
				SafeRelease( &pScheduledRead->m_pAsyncRead );

				m_queuedReads.Insert( pScheduledRead );
			}
		}

		AsyncReadJob_t* pRequestCopy = NULL;
		if ( m_requestedCopies.PopItem( &pRequestCopy ) )
		{
			SafeAssign( &pRequestCopy->m_pAsyncRead, new CAsyncCopyRequest );
			extern CMaterialSystem g_MaterialSystem;
			g_MaterialSystem.GetRenderContextInternal()->AsyncCopyRenderTargetToStagingTexture( pRequestCopy->m_pSysmemTex, pRequestCopy->m_pSrcRt, pRequestCopy->m_pAsyncRead, NULL );

			m_scheduledReads.Insert( pRequestCopy );
		}
		
		while ( m_completedJobs.Count() > 0 )
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "CreateTextureFromBits" );
			
			AsyncReadJob_t* pCreate = NULL;
			if ( m_completedJobs.PopItem( &pCreate ) ) 
			{
				// Check after we do the unmap, we need to do that here.
				if ( IsJobCancelled( pCreate ) )
				{
					delete pCreate;
					continue;
				}

				extern CMaterialSystem g_MaterialSystem;
				g_MaterialSystem.GetRenderContextInternal()->AsyncUnmap( pCreate->m_pSysmemTex );
				SafeRelease( &pCreate->m_pAsyncMap );

				assert_cast< CTextureManager* >( g_pTextureManager )->CompleteAsyncRead( pCreate );
				delete pCreate;
				pCreate = NULL;
				// Stop as soon as we complete one successfully.
				break;
			}
		}
	}

private:
	inline bool ThreadInReaderThread()
	{
		return s_TextureManager.ThreadInAsyncReadThread();
	}

	void ThreadReader_Main()
	{
		Assert( ThreadInReaderThread() );

		while ( !m_bQuit )
		{
			AsyncReadJob_t *pJob = NULL;
			if ( m_pendingJobs.PopItem( &pJob ) )
			{
				Assert( pJob != NULL );
				ThreadReader_ProcessRead( pJob );
			}
			else
			{
				// "awhile"
				ThreadSleep( 8 );
			}
		}
	}

	void ThreadReader_ProcessRead( AsyncReadJob_t *pJob )
	{
		Assert( ThreadInReaderThread() );
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		// This code does a few things:
		// 1. Reads from a previously mapped scratch buffer texture and performs byte swapping (if necessary).
		// 2. Uses byteswapped data to generate mipmaps
		// 3. Encodes mipmapped data into the destination format.

		const int h = pJob->m_pSysmemTex->GetActualHeight();
		const int w = pJob->m_pSysmemTex->GetActualWidth();
		const ImageFormat srcFmt = pJob->m_pSysmemTex->GetImageFormat();

		// Convert the data
		CUtlMemory< unsigned char > srcBufferFinestMip;
		CUtlMemory< unsigned char > srcBufferAllMips;
		const int srcFinestMemRequired = ImageLoader::GetMemRequired( w, h, 1, srcFmt, false );
		const int srcAllMemRequired = ImageLoader::GetMemRequired( w, h, 1, srcFmt, pJob->m_bGenMips );
		const int srcPitch = ImageLoader::GetMemRequired( w, 1, 1, srcFmt, false );

		const ImageFormat dstFmt = pJob->m_dstFmt;
		CUtlMemory< unsigned char > dstBufferAllMips;
		const int dstMemRequried = ImageLoader::GetMemRequired( w, h, 1, dstFmt, pJob->m_bGenMips );
		
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s-Allocations", __FUNCTION__ );
			srcBufferFinestMip.EnsureCapacity( srcFinestMemRequired );
			if ( srcFinestMemRequired != srcAllMemRequired )
			{
				srcBufferAllMips.EnsureCapacity( srcAllMemRequired );
			}
			else
			{
				Assert( !pJob->m_bGenMips );
			}

			if ( srcFmt != dstFmt )
			{
				dstBufferAllMips.EnsureCapacity( dstMemRequried );
			}
		}

		// If this fires, you will get data corruption below. We can fix this case, it just doesn't seem
		// to be needed right now.
		Assert( pJob->m_pAsyncMap->m_nPitch == srcPitch );
		srcPitch; // Hush compiler.

		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s-ByteSwapInPlace", __FUNCTION__ );
			ImageLoader::ConvertImageFormat( (unsigned char*) pJob->m_pAsyncMap->m_pMemory, GetImageFormatRawReadback( srcFmt ), srcBufferFinestMip.Base(), srcFmt, w, h );
		}
		
		if ( pJob->m_bGenMips )
		{
			GenerateMipmaps( &srcBufferAllMips, srcBufferFinestMip.Base(), w, h, srcFmt );
		}
		else
		{
			// If we're not generating mips, then allmips == finest mip, but the code below expects everything to 
			// be in all mips.
			srcBufferAllMips.Swap( srcBufferFinestMip );
		}

		// Code below expects that the data is here one way or another.
		Assert( srcBufferAllMips.Count() == srcAllMemRequired );
		
		if ( srcFmt != dstFmt ) 
		{
			ConvertTexelData( &dstBufferAllMips, dstFmt, srcBufferAllMips, w, h, srcFmt, pJob->m_bGenMips );
			pJob->m_finalTexelData.Swap( dstBufferAllMips );
		}
		else
		{
			// Just swap out the buffers. 
			pJob->m_finalTexelData.Swap( srcBufferAllMips );
		}

		// At this point, the data should be ready to go. Quick sanity check.
		Assert( pJob->m_finalTexelData.Count() == dstMemRequried );

		m_completedJobs.PushItem( pJob );
	}

	void GenerateMipmaps( CUtlMemory< unsigned char >* outBuffer, unsigned char* pSrc, int w, int h, ImageFormat fmt ) const
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		ImageLoader::GenerateMipmapLevelsLQ( pSrc, outBuffer->Base(), w, h, fmt, 0 );
	}

	void ConvertTexelData( CUtlMemory< unsigned char > *outBuffer, ImageFormat dstFmt, /* const */ CUtlMemory< unsigned char > &inBuffer, int w, int h, ImageFormat srcFmt, bool bGenMips )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		const int mipmapCount = bGenMips ? ImageLoader::GetNumMipMapLevels( w, h ) : 1;

		unsigned char* pSrc = inBuffer.Base();
		unsigned char* pDst = (*outBuffer).Base();
		int mip_w = w;
		int mip_h = h;

		for ( int i = 0; i < mipmapCount; ++i )
		{
			ImageLoader::ConvertImageFormat( pSrc, srcFmt, pDst, dstFmt, mip_w, mip_h );

			pSrc += ImageLoader::GetMemRequired( mip_w, mip_h, 1, srcFmt, false );
			pDst += ImageLoader::GetMemRequired( mip_w, mip_h, 1, dstFmt, false );

			mip_w = Max( 1, mip_w >> 1 );
			mip_h = Max( 1, mip_h >> 1 );
		}
	}
	static unsigned ReaderMain( void* _this )
	{
		ThreadSetDebugName( "Helper" );

		s_TextureManager.m_nAsyncReadThread = ThreadGetCurrentId();
		( ( AsyncReader* ) _this )->ThreadReader_Main();
		s_TextureManager.m_nAsyncReadThread = 0xFFFFFFFF;
		return 0;
	}

	ThreadHandle_t m_HelperThread;
	volatile bool m_bQuit;

	CTSQueue< AsyncReadJob_t*> m_requestedCopies;
	CUtlQueue< AsyncReadJob_t* > m_queuedReads;
	CUtlQueue< AsyncReadJob_t* > m_scheduledReads;
	CUtlQueue< AsyncReadJob_t* > m_queuedMaps;

	CTSQueue< AsyncReadJob_t* > m_pendingJobs;
	CTSQueue< AsyncReadJob_t* > m_completedJobs;
};

//-----------------------------------------------------------------------------
// Texture manager
//-----------------------------------------------------------------------------
CTextureManager::CTextureManager( void ) 
: m_TextureList( true )
, m_TextureAliases( true )
, m_TextureExcludes( true )
, m_PendingAsyncLoads( true ) 
, m_textureStreamingRequests( DefLessFunc( ITextureInternal* ) )
, m_nAsyncLoadThread( 0xFFFFFFFF )
, m_nAsyncReadThread( 0xFFFFFFFF )
{
	m_pErrorTexture = NULL;
	m_pBlackTexture = NULL;
	m_pWhiteTexture = NULL;
	m_pGreyTexture  = NULL;
	m_pGreyAlphaZeroTexture  = NULL;
	m_pNormalizationCubemap = NULL;
	m_pErrorRegen = NULL;
	m_pFullScreenTexture = NULL;
	m_pSignedNormalizationCubemap = NULL;
	m_pShadowNoise2D = NULL;
	m_pIdentityLightWarp = NULL;
	m_pFullScreenDepthTexture = NULL;
	m_pDebugLuxels2D = NULL;
	m_pAsyncLoader = new AsyncLoader;
	m_pAsyncReader = new AsyncReader;
	m_iSuspendTextureStreaming = 0;
}


//-----------------------------------------------------------------------------
// Initialization + shutdown
//-----------------------------------------------------------------------------
void CTextureManager::Init( int nFlags )
{
	m_nFlags = nFlags;
	color32 color, color2;
	m_iNextTexID = 4096;

	// setup the checkerboard generator for failed texture loading
	color.r = color.g = color.b = 0; color.a = 128;
	color2.r = color2.b = color2.a = 255; color2.g = 0;
	m_pErrorRegen = new CCheckerboardTexture( 4, color, color2 );

	// Create an error texture
	m_pErrorTexture = CreateProceduralTexture( "error", TEXTURE_GROUP_OTHER,
		ERROR_TEXTURE_SIZE, ERROR_TEXTURE_SIZE, 1, IMAGE_FORMAT_BGRA8888, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY );
	CreateCheckerboardTexture( m_pErrorTexture, 4, color, color2 );
	m_pErrorTexture->SetErrorTexture( true );

	// Create a white texture
	m_pWhiteTexture = CreateProceduralTexture( "white", TEXTURE_GROUP_OTHER,
		WHITE_TEXTURE_SIZE, WHITE_TEXTURE_SIZE, 1, IMAGE_FORMAT_BGRX8888, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY );
	color.r = color.g = color.b = color.a = 255;
	CreateSolidTexture( m_pWhiteTexture, color );

	// Create a black texture
	m_pBlackTexture = CreateProceduralTexture( "black", TEXTURE_GROUP_OTHER,
		BLACK_TEXTURE_SIZE, BLACK_TEXTURE_SIZE, 1, IMAGE_FORMAT_BGRX8888, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY );
	color.r = color.g = color.b = 0;
	CreateSolidTexture( m_pBlackTexture, color );

	// Create a grey texture
	m_pGreyTexture = CreateProceduralTexture( "grey", TEXTURE_GROUP_OTHER,
		GREY_TEXTURE_SIZE, GREY_TEXTURE_SIZE, 1, IMAGE_FORMAT_BGRA8888, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY );
	color.r = color.g = color.b = 128;
	color.a = 255;
	CreateSolidTexture( m_pGreyTexture, color );

	// Create a grey texture
	m_pGreyAlphaZeroTexture = CreateProceduralTexture( "greyalphazero", TEXTURE_GROUP_OTHER,
		GREY_TEXTURE_SIZE, GREY_TEXTURE_SIZE, 1, IMAGE_FORMAT_BGRA8888, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY );
	color.r = color.g = color.b = 128;
	color.a = 0;
	CreateSolidTexture( m_pGreyAlphaZeroTexture, color );

	if ( HardwareConfig()->GetMaxDXSupportLevel() >= 80 )
	{
		// Create a normalization cubemap
		m_pNormalizationCubemap = CreateProceduralTexture( "normalize", TEXTURE_GROUP_CUBE_MAP,
			NORMALIZATION_CUBEMAP_SIZE, NORMALIZATION_CUBEMAP_SIZE, 1, IMAGE_FORMAT_BGRX8888,
			TEXTUREFLAGS_ENVMAP | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY |
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_CLAMPU );
		CreateNormalizationCubemap( m_pNormalizationCubemap );
	}

	if ( HardwareConfig()->GetMaxDXSupportLevel() >= 90 )
	{
		// In GL, we have poor format support, so we ask for signed float
		ImageFormat fmt = IsOpenGL() ? IMAGE_FORMAT_RGBA16161616F : IMAGE_FORMAT_UVWQ8888;

		int nTextureFlags = TEXTUREFLAGS_ENVMAP | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_CLAMPU;

#ifdef OSX
		// JasonM - ridiculous hack around R500 lameness...we never use this texture on OSX anyways (right?)
		// Now assuming this was an OSX specific workaround.
		nTextureFlags |= TEXTUREFLAGS_POINTSAMPLE;
#endif

		// Create a normalization cubemap
		m_pSignedNormalizationCubemap = CreateProceduralTexture( "normalizesigned", TEXTURE_GROUP_CUBE_MAP,
																NORMALIZATION_CUBEMAP_SIZE, NORMALIZATION_CUBEMAP_SIZE, 1, fmt, nTextureFlags );
		CreateSignedNormalizationCubemap( m_pSignedNormalizationCubemap );
		
		m_pIdentityLightWarp = FindOrLoadTexture( "dev/IdentityLightWarp", TEXTURE_GROUP_OTHER );
		m_pIdentityLightWarp->IncrementReferenceCount();
	}

	// High end hardware needs this texture for shadow mapping
	if ( HardwareConfig()->ActuallySupportsPixelShaders_2_b() )
	{
		m_pShadowNoise2D = FindOrLoadTexture( "engine/NormalizedRandomDirections2D", TEXTURE_GROUP_OTHER );
		m_pShadowNoise2D->IncrementReferenceCount();
	}

	m_pDebugLuxels2D = FindOrLoadTexture( "debug/debugluxelsnoalpha", TEXTURE_GROUP_OTHER );
	m_pDebugLuxels2D->IncrementReferenceCount();
}

void CTextureManager::Shutdown()
{
	// Clean up any textures we have hanging around that are waiting to go.
	CleanupPossiblyUnreferencedTextures();

	// Cool the texture cache first to drop all the refs back to 0 for the streamable things.
	CoolTextureCache();

	if ( m_pAsyncLoader )
	{
		m_pAsyncLoader->Shutdown();
		delete m_pAsyncLoader;
		m_pAsyncLoader = NULL;
	}

	if ( m_pAsyncReader )
	{
		m_pAsyncReader->Shutdown();
		delete m_pAsyncReader;
		m_pAsyncReader = NULL;
	}

	FreeStandardRenderTargets();

	FOR_EACH_VEC( m_ReadbackTextures, i )
	{
		m_ReadbackTextures[ i ]->Release();
	}

	if ( m_pDebugLuxels2D )
	{
		m_pDebugLuxels2D->DecrementReferenceCount();
		m_pDebugLuxels2D = NULL;
	}

	// These checks added because it's possible for shutdown to be called before the material system is 
	// fully initialized.
	if ( m_pWhiteTexture )
	{
		m_pWhiteTexture->DecrementReferenceCount();
		m_pWhiteTexture = NULL;
	}

	if ( m_pBlackTexture )
	{
		m_pBlackTexture->DecrementReferenceCount();
		m_pBlackTexture = NULL;
	}

	if ( m_pGreyTexture )
	{
		m_pGreyTexture->DecrementReferenceCount();
		m_pGreyTexture = NULL;
	}

	if ( m_pGreyAlphaZeroTexture )
	{
		m_pGreyAlphaZeroTexture->DecrementReferenceCount();
		m_pGreyAlphaZeroTexture = NULL;
	}

	if ( m_pNormalizationCubemap )
	{
		m_pNormalizationCubemap->DecrementReferenceCount();
		m_pNormalizationCubemap = NULL;
	}

	if ( m_pSignedNormalizationCubemap )
	{
		m_pSignedNormalizationCubemap->DecrementReferenceCount();
		m_pSignedNormalizationCubemap = NULL;
	}

	if ( m_pShadowNoise2D )
	{
		m_pShadowNoise2D->DecrementReferenceCount();
		m_pShadowNoise2D = NULL;
	}

	if ( m_pIdentityLightWarp )
	{
		m_pIdentityLightWarp->DecrementReferenceCount();
		m_pIdentityLightWarp = NULL;
	}

	if ( m_pErrorTexture )
	{
		m_pErrorTexture->DecrementReferenceCount();
		m_pErrorTexture = NULL;
	}

	ReleaseTextures();

	if ( m_pErrorRegen )
	{
		m_pErrorRegen->Release();
		m_pErrorRegen = NULL;
	}

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		ITextureInternal::Destroy( m_TextureList[i], true );
	}
	m_TextureList.RemoveAll();

	for( int i = m_TextureAliases.First(); i != m_TextureAliases.InvalidIndex(); i = m_TextureAliases.Next( i ) )
	{
		delete []m_TextureAliases[i];
	}
	m_TextureAliases.RemoveAll();

	m_TextureExcludes.RemoveAll();
}


//-----------------------------------------------------------------------------
// Allocate, free standard render target textures
//-----------------------------------------------------------------------------
void CTextureManager::AllocateStandardRenderTargets( )
{
	bool bAllocateFullscreenTexture = ( m_nFlags & MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE ) != 0;
	bool bAllocateMorphAccumTexture = g_pMorphMgr->ShouldAllocateScratchTextures();

	if ( IsPC() && ( bAllocateFullscreenTexture || bAllocateMorphAccumTexture ) )
	{
		MaterialSystem()->BeginRenderTargetAllocation();

		// A offscreen render target which is the size + format of the back buffer (*not* HDR format!)
		if ( bAllocateFullscreenTexture )
		{
			m_pFullScreenTexture = CreateRenderTargetTexture( "_rt_FullScreen", 1, 1, RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP, 
				MaterialSystem()->GetBackBufferFormat(), RENDER_TARGET, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT, 0 );
			m_pFullScreenTexture->IncrementReferenceCount();
		}

		// This texture is the one we accumulate morph deltas into
		if ( bAllocateMorphAccumTexture )
		{
			g_pMorphMgr->AllocateScratchTextures();
			g_pMorphMgr->AllocateMaterials();
		}

		MaterialSystem()->EndRenderTargetAllocation();
	}
}


void CTextureManager::FreeStandardRenderTargets()
{
	if ( m_pFullScreenTexture )
	{
		m_pFullScreenTexture->DecrementReferenceCount();
		m_pFullScreenTexture = NULL;
	}

	g_pMorphMgr->FreeMaterials();
	g_pMorphMgr->FreeScratchTextures();
}


void CTextureManager::CacheExternalStandardRenderTargets()
{
	m_pFullScreenDepthTexture = FindTexture( "_rt_FullFrameDepth" ); //created/destroyed in engine/matsys_interface.cpp to properly track hdr changes
}


//-----------------------------------------------------------------------------
// Generates an error texture pattern
//-----------------------------------------------------------------------------
void CTextureManager::GenerateErrorTexture( ITexture *pTexture, IVTFTexture *pVTFTexture )
{
	m_pErrorRegen->RegenerateTextureBits( pTexture, pVTFTexture, NULL );
}

//-----------------------------------------------------------------------------
// Updates the color correction state
//-----------------------------------------------------------------------------
ITextureInternal *CTextureManager::ColorCorrectionTexture( int i )
{
	Assert( i<COLOR_CORRECTION_MAX_TEXTURES );
	return m_pColorCorrectionTextures[ i ];
}

void CTextureManager::SetColorCorrectionTexture( int i, ITextureInternal *pTexture )
{
	Assert( i<COLOR_CORRECTION_MAX_TEXTURES );

	if( m_pColorCorrectionTextures[i] )
	{
		m_pColorCorrectionTextures[i]->DecrementReferenceCount();
	}

	m_pColorCorrectionTextures[i] = pTexture;
	if( pTexture )
		pTexture->IncrementReferenceCount();
}


//-----------------------------------------------------------------------------
// Releases all textures (cause we've lost video memory)
//-----------------------------------------------------------------------------
void CTextureManager::ReleaseTextures( void )
{
	g_pShaderAPI->SetFullScreenTextureHandle( INVALID_SHADERAPI_TEXTURE_HANDLE );

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		// Release the texture...
		m_TextureList[i]->ReleaseMemory();
	}
}


//-----------------------------------------------------------------------------
// Request a texture ID
//-----------------------------------------------------------------------------
int CTextureManager::RequestNextTextureID()
{
	// FIXME: Deal better with texture ids
	// The range between 19000 and 21000 are used for standard textures + lightmaps
	if (m_iNextTexID == 19000)
	{
		m_iNextTexID = 21000;
	}

	return m_iNextTexID++;
}


//-----------------------------------------------------------------------------
// Restores a single texture
//-----------------------------------------------------------------------------
void CTextureManager::RestoreTexture( ITextureInternal* pTexture )
{
	// Put the texture back onto the board
	pTexture->OnRestore();	// Give render targets a chance to reinitialize themselves if necessary (due to AA changes).
	pTexture->Download();
}

//-----------------------------------------------------------------------------
// Purges our complete list of textures that might currently be unreferenced
//-----------------------------------------------------------------------------
void CTextureManager::CleanupPossiblyUnreferencedTextures()
{
	if ( !ThreadInMainThread() || MaterialSystem()->GetRenderThreadId() != 0xFFFFFFFF )
	{
		Assert( !"CTextureManager::CleanupPossiblyUnreferencedTextures should never be called here" );
		// This is catastrophically bad, don't do this. Someone needs to fix this. See JohnS or McJohn
		DebuggerBreakIfDebugging_StagingOnly();
		return;
	}

	// It is perfectly valid for a texture to become referenced again (it lives on in our texture list, and can be
	// re-loaded) and then free'd again, so ensure we don't have any duplicates in queue.
	CUtlVector< ITextureInternal * > texturesToDelete( /* growSize */ 0, /* initialSize */ m_PossiblyUnreferencedTextures.Count() );
	ITextureInternal *pMaybeUnreferenced = NULL;
	while ( m_PossiblyUnreferencedTextures.PopItem( &pMaybeUnreferenced ) )
	{
		Assert( pMaybeUnreferenced->GetReferenceCount() >= 0 );
		if ( pMaybeUnreferenced->GetReferenceCount() == 0 && texturesToDelete.Find( pMaybeUnreferenced ) == texturesToDelete.InvalidIndex() )
		{
			texturesToDelete.AddToTail( pMaybeUnreferenced );
		}
	}

	// Free them
	FOR_EACH_VEC( texturesToDelete, i )
	{
		RemoveTexture( texturesToDelete[ i ] );
	}
}

//-----------------------------------------------------------------------------
// Restore all textures (cause we've got video memory again)
//-----------------------------------------------------------------------------
void CTextureManager::RestoreNonRenderTargetTextures( )
{
	// 360 should not have gotten here
	Assert( !IsX360() );

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		if ( !m_TextureList[i]->IsRenderTarget() )
		{
			RestoreTexture( m_TextureList[i] );
		}
	}
}

//-----------------------------------------------------------------------------
// Restore just the render targets (cause we've got video memory again)
//-----------------------------------------------------------------------------
void CTextureManager::RestoreRenderTargets()
{
	// 360 should not have gotten here
	Assert( !IsX360() );

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		if ( m_TextureList[i]->IsRenderTarget() )
		{
			RestoreTexture( m_TextureList[i] );
		}
	}

	if ( m_pFullScreenTexture )
	{
		g_pShaderAPI->SetFullScreenTextureHandle( m_pFullScreenTexture->GetTextureHandle( 0 ) );
	}

	CacheExternalStandardRenderTargets();
}


//-----------------------------------------------------------------------------
// Reloads all textures
//-----------------------------------------------------------------------------
void CTextureManager::ReloadTextures()
{
	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		// Put the texture back onto the board
		m_TextureList[i]->Download();
	}
}

static void ForceTextureIntoHardware( ITexture *pTexture, IMaterial *pMaterial, IMaterialVar *pBaseTextureVar )
{
	if ( IsX360() )
		return;

	pBaseTextureVar->SetTextureValue( pTexture );

	CMatRenderContextPtr pRenderContext( MaterialSystem()->GetRenderContext() );
	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 1 );

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
	meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
	meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
	meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
	meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
	meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
	meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Reloads all textures
//-----------------------------------------------------------------------------
void CTextureManager::ForceAllTexturesIntoHardware( void )
{
	if ( IsX360() )
		return;
	
	IMaterial *pMaterial = MaterialSystem()->FindMaterial( "engine/preloadtexture", "texture preload" );
	pMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion(); //always work with the realtime material internally
	pMaterial->IncrementReferenceCount();
	bool bFound;
	IMaterialVar *pBaseTextureVar = pMaterial->FindVar( "$basetexture", &bFound );
	if( !bFound )
	{
		return;
	}

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		// Put the texture back onto the board
		ForceTextureIntoHardware( m_TextureList[i], pMaterial, pBaseTextureVar );
	}
	pMaterial->DecrementReferenceCount();
}

//-----------------------------------------------------------------------------
// Get at a couple standard textures
//-----------------------------------------------------------------------------
ITextureInternal *CTextureManager::ErrorTexture()
{
	return m_pErrorTexture;
}

ITextureInternal *CTextureManager::NormalizationCubemap()
{
	return m_pNormalizationCubemap; 
}

ITextureInternal *CTextureManager::SignedNormalizationCubemap()
{
	return m_pSignedNormalizationCubemap; 
}

ITextureInternal *CTextureManager::ShadowNoise2D()
{
	return m_pShadowNoise2D; 
}

ITextureInternal *CTextureManager::IdentityLightWarp()
{
	return m_pIdentityLightWarp; 
}

ITextureInternal *CTextureManager::FullFrameDepthTexture()
{
	return m_pFullScreenDepthTexture;
}

ITextureInternal *CTextureManager::DebugLuxels2D()
{
	return m_pDebugLuxels2D;
}



//-----------------------------------------------------------------------------
// Creates a procedural texture
//-----------------------------------------------------------------------------
ITextureInternal *CTextureManager::CreateProceduralTexture( 
	const char			*pTextureName, 
	const char			*pTextureGroupName, 
	int					w, 
	int					h, 
	int					d, 
	ImageFormat			fmt, 
	int					nFlags,
	ITextureRegenerator *generator )
{
	ITextureInternal *pNewTexture = ITextureInternal::CreateProceduralTexture( pTextureName, pTextureGroupName, w, h, d, fmt, nFlags, generator );
	if ( !pNewTexture )
		return NULL;

	// Add it to the list of textures so it can be restored, etc.
	m_TextureList.Insert( pNewTexture->GetName(), pNewTexture );

	// NOTE: This will download the texture only if the shader api is ready
	pNewTexture->Download();

	return pNewTexture;
}

//-----------------------------------------------------------------------------
// FIXME: Need some better understanding of when textures should be added to
// the texture dictionary here. Is it only for files, for example?
// Texture dictionary...
//-----------------------------------------------------------------------------
ITextureInternal *CTextureManager::LoadTexture( const char *pTextureName, const char *pTextureGroupName, int nAdditionalCreationFlags /* = 0 */, bool bDownload /* = true */  )
{
	ITextureInternal *pNewTexture = ITextureInternal::CreateFileTexture( pTextureName, pTextureGroupName );
	if ( pNewTexture )
	{
		int iIndex = m_TextureExcludes.Find( pNewTexture->GetName() );
		if ( m_TextureExcludes.IsValidIndex( iIndex ) )
		{
			// mark the new texture as excluded
			int nDimensionsLimit = m_TextureExcludes[iIndex];
			pNewTexture->MarkAsExcluded( ( nDimensionsLimit == 0 ), nDimensionsLimit );
		}

		// Stick the texture onto the board
		if ( bDownload )
			pNewTexture->Download( NULL, nAdditionalCreationFlags );

		// FIXME: If there's been an error loading, we don't also want this error...
	}

	return pNewTexture;
}

ITextureInternal *CTextureManager::FindTexture( const char *pTextureName )
{
	if ( !pTextureName || pTextureName[0] == 0 )
		return NULL;
	
	char szCleanName[MAX_PATH];
	NormalizeTextureName( pTextureName, szCleanName, sizeof( szCleanName ) );

	int i = m_TextureList.Find( szCleanName );
	if ( i != m_TextureList.InvalidIndex() )
	{
		return m_TextureList[i];
	}

	i = m_TextureAliases.Find( szCleanName );
	if ( i != m_TextureAliases.InvalidIndex() )
	{
		return FindTexture( m_TextureAliases[i] );
	}

	// Special handling: lightmaps
	if ( char const *szLightMapNum = StringAfterPrefix( szCleanName, "[lightmap" ) )
	{
		int iLightMapNum = atoi( szLightMapNum );
		extern CMaterialSystem g_MaterialSystem;
		CMatLightmaps *plm = g_MaterialSystem.GetLightmaps();
		if ( iLightMapNum >= 0 &&
			 iLightMapNum < plm->GetNumLightmapPages() )
		{
			ShaderAPITextureHandle_t hTex = plm->GetLightmapPageTextureHandle( iLightMapNum );
			if ( hTex != INVALID_SHADERAPI_TEXTURE_HANDLE )
			{
				// Establish the lookup linking in the dictionary
				ITextureInternal *pTxInt = ITextureInternal::CreateReferenceTextureFromHandle( pTextureName, TEXTURE_GROUP_LIGHTMAP, hTex );
				m_TextureList.Insert( pTextureName, pTxInt );
				return pTxInt;
			}
		}
	}

	return NULL;
}

void CTextureManager::AddTextureAlias( const char *pAlias, const char *pRealName )
{
	if	( (pAlias == NULL) || (pRealName == NULL) )
		return; //invalid alias

	char szCleanName[MAX_PATH];
	int index = m_TextureAliases.Find( NormalizeTextureName( pAlias, szCleanName, sizeof( szCleanName ) ) );

	if	( index != m_TextureAliases.InvalidIndex() )
	{
		AssertMsg( Q_stricmp( pRealName, m_TextureAliases[index] ) == 0, "Trying to use one name to alias two different textures." );
		RemoveTextureAlias( pAlias ); //remove the old alias to make room for the new one.
	}

	size_t iRealNameLength = strlen( pRealName ) + 1;
	char *pRealNameCopy = new char [iRealNameLength];
	memcpy( pRealNameCopy, pRealName, iRealNameLength );

	m_TextureAliases.Insert( szCleanName, pRealNameCopy );
}

void CTextureManager::RemoveTextureAlias( const char *pAlias )
{
	if ( pAlias == NULL )
		return;

	char szCleanName[MAX_PATH];
	int index = m_TextureAliases.Find( NormalizeTextureName( pAlias, szCleanName, sizeof( szCleanName ) ) );
	if ( index == m_TextureAliases.InvalidIndex() )
		return; //not found

	delete []m_TextureAliases[index];
	m_TextureAliases.RemoveAt( index );
}

void CTextureManager::SetExcludedTextures( const char *pScriptName )
{
	// clear all exisiting texture's exclusion
	for ( int i = m_TextureExcludes.First(); i != m_TextureExcludes.InvalidIndex(); i = m_TextureExcludes.Next( i ) )
	{
		ITextureInternal *pTexture = FindTexture( m_TextureExcludes.GetElementName( i ) );
		if ( pTexture )
		{
			pTexture->MarkAsExcluded( false, 0 );
		}
	}
	m_TextureExcludes.RemoveAll();

	MEM_ALLOC_CREDIT();

	// get optional script
	CUtlBuffer excludeBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( g_pFullFileSystem->ReadFile( pScriptName, NULL, excludeBuffer ) )
	{
		char szToken[MAX_PATH];
		while ( 1 )
		{
			// must support spaces in names without quotes
			// have to brute force parse up to a valid line
			while ( 1 )
			{
				excludeBuffer.EatWhiteSpace();
				if ( !excludeBuffer.EatCPPComment() )
				{
					// not a comment
					break;
				}
			}
			excludeBuffer.GetLine( szToken, sizeof( szToken ) );
			int tokenLength = strlen( szToken );
			if ( !tokenLength )
			{
				// end of list
				break;
			}

			// remove all trailing whitespace
			while ( tokenLength > 0 )
			{
				tokenLength--;
				if ( isgraph( szToken[tokenLength] ) )
				{
					break;
				}
				szToken[tokenLength] = '\0';
			}

			// first optional token may be a dimension limit hint
			int nDimensionsLimit = 0;
			char *pTextureName = szToken;
			if ( pTextureName[0] != 0 && isdigit( pTextureName[0] ) )
			{
				nDimensionsLimit = atoi( pTextureName );
				
				// skip forward to name
				for ( ;; )
				{
					char ch = *pTextureName;
					if ( !ch || ( !isdigit( ch ) && !isspace( ch ) ) )
					{
						break;
					}
					pTextureName++;
				}
			}

			char szCleanName[MAX_PATH];
			NormalizeTextureName( pTextureName, szCleanName, sizeof( szCleanName ) );

			if ( m_TextureExcludes.Find( szCleanName ) != m_TextureExcludes.InvalidIndex() )
			{
				// avoid duplicates
				continue;
			}

			m_TextureExcludes.Insert( szCleanName, nDimensionsLimit );

			// set any existing texture's exclusion
			// textures that don't exist yet will get caught during their creation path
			ITextureInternal *pTexture = FindTexture( szCleanName );
			if ( pTexture )
			{
				pTexture->MarkAsExcluded( ( nDimensionsLimit == 0 ), nDimensionsLimit );
			}
		}
	}
}

void CTextureManager::UpdateExcludedTextures( void )
{
	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		m_TextureList[i]->UpdateExcludedState();
	}
}

ITextureInternal *CTextureManager::FindOrLoadTexture( const char *pTextureName, const char *pTextureGroupName, int nAdditionalCreationFlags /* = 0 */ )
{
	ITextureInternal *pTexture = FindTexture( pTextureName );
	if ( !pTexture )
	{
		pTexture = LoadTexture( pTextureName, pTextureGroupName, nAdditionalCreationFlags );
		if ( pTexture )
		{
			// insert into the dictionary using the processed texture name
			m_TextureList.Insert( pTexture->GetName(), pTexture );
		}
	}

	return pTexture;
}

bool CTextureManager::IsTextureLoaded( const char *pTextureName )
{
	ITextureInternal *pTexture = FindTexture( pTextureName );
	return ( pTexture != NULL );
}


//-----------------------------------------------------------------------------
// Creates a texture that's a render target
//-----------------------------------------------------------------------------
ITextureInternal *CTextureManager::CreateRenderTargetTexture( 
	const char *pRTName,	// NULL for auto-generated name
	int w, 
	int h, 
	RenderTargetSizeMode_t sizeMode, 
	ImageFormat fmt, 
	RenderTargetType_t type, 
	unsigned int textureFlags, 
	unsigned int renderTargetFlags )
{
	MEM_ALLOC_CREDIT_( __FILE__ ": Render target" );

	ITextureInternal *pTexture;
	if ( pRTName )
	{
		// caller is re-initing or changing
		pTexture = FindTexture( pRTName );
		if ( pTexture )
		{
			// Changing the underlying render target, but leaving the pointer and refcount
			// alone fixes callers that have exisiting references to this object.
			ITextureInternal::ChangeRenderTarget( pTexture, w, h, sizeMode, fmt, type, 
					textureFlags, renderTargetFlags );

			// download if ready
			pTexture->Download();
			return pTexture;
		}
	}
 
	pTexture = ITextureInternal::CreateRenderTarget( pRTName, w, h, sizeMode, fmt, type, 
											  textureFlags, renderTargetFlags );
	if ( !pTexture )
		return NULL;

	// Add the render target to the list of textures
	// that way it'll get cleaned up correctly in case of a task switch
	m_TextureList.Insert( pTexture->GetName(), pTexture );

	// NOTE: This will download the texture only if the shader api is ready
	pTexture->Download();

	return pTexture;
}

void CTextureManager::ResetTextureFilteringState( )
{
	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		m_TextureList[i]->SetFilteringAndClampingMode();
	}
}

void CTextureManager::SuspendTextureStreaming( void )
{
	m_iSuspendTextureStreaming++;
}

void CTextureManager::ResumeTextureStreaming( void )
{
	AssertMsg( m_iSuspendTextureStreaming, "Mismatched Suspend/Resume texture streaming calls" );
	if ( m_iSuspendTextureStreaming )
	{
		m_iSuspendTextureStreaming--;
	}
}

void CTextureManager::RemoveUnusedTextures( void )
{
	// First, need to flush all of our textures that are pending cleanup.
	CleanupPossiblyUnreferencedTextures();

	int iNext;
	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = iNext )
	{
		iNext = m_TextureList.Next( i );

#ifdef _DEBUG
		if ( m_TextureList[i]->GetReferenceCount() < 0 )
		{
			Warning( "RemoveUnusedTextures: pTexture->m_referenceCount < 0 for %s\n", m_TextureList[i]->GetName() );
		}
#endif
		if ( m_TextureList[i]->GetReferenceCount() <= 0 )
		{
			ITextureInternal::Destroy( m_TextureList[i] );
			m_TextureList.RemoveAt( i );
		}
	}
}

void CTextureManager::MarkUnreferencedTextureForCleanup( ITextureInternal *pTexture )
{
	Assert( pTexture->GetReferenceCount() == 0 );
	m_PossiblyUnreferencedTextures.PushItem( pTexture );
}

void CTextureManager::RemoveTexture( ITextureInternal *pTexture )
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	Assert( pTexture->GetReferenceCount() <= 0 );

	if ( !ThreadInMainThread() || MaterialSystem()->GetRenderThreadId() != 0xFFFFFFFF )
	{
		Assert( !"CTextureManager::RemoveTexture should never be called here");
		// This is catastrophically bad, don't do this. Someone needs to fix this. 
		DebuggerBreakIfDebugging_StagingOnly();
		return;
	}

	bool bTextureFound = false;

	// If the queue'd rendering thread is running, RemoveTexture() is going to explode. If it isn't, calling
	// RemoveTexture while still dealing with immediate removal textures seems fishy, but could be legit, in which case
	// this assert could be softened.
	int nUnreferencedQueue = m_PossiblyUnreferencedTextures.Count();
	if ( nUnreferencedQueue )
	{
		Assert( !"RemoveTexture() being called while textures sitting in possibly unreferenced queue" );
		// Assuming that this is all a wholesome main-thread misunderstanding, we can try to continue after filtering
		// this texture from the queue.
		ITextureInternal *pPossiblyUnreferenced = NULL;
		for ( int i = 0; i < nUnreferencedQueue && m_PossiblyUnreferencedTextures.PopItem( &pPossiblyUnreferenced ); i++ )
		{
			m_PossiblyUnreferencedTextures.PushItem( pPossiblyUnreferenced );

			if ( pPossiblyUnreferenced == pTexture )
			{
				bTextureFound = true;
				break;
			}
		}
	}

	if ( bTextureFound ) 
	{
		Assert( !"CTextureManager::RemoveTexture has been called for a texture that has already requested cleanup. That's a paddlin'." );
		// This is catastrophically bad, don't do this. Someone needs to fix this. 
		DebuggerBreakIfDebugging_StagingOnly();
		return;
	}

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		// search by object
		if ( m_TextureList[i] == pTexture )
		{
			// This code is always sure that the texture we're tryign to clean up is no longer in the the possibly unreferenced list,
			// So let Destroy work without checking.
			ITextureInternal::Destroy( m_TextureList[i], true );
			m_TextureList.RemoveAt( i );
			break;
		}
	}
}

void CTextureManager::ReloadFilesInList( IFileList *pFilesToReload )
{
	if ( !IsPC() )
		return;

	for ( int i=m_TextureList.First(); i != m_TextureList.InvalidIndex(); i=m_TextureList.Next( i ) )
	{
		ITextureInternal *pTex = m_TextureList[i];

		pTex->ReloadFilesInList( pFilesToReload );
	}
}

void CTextureManager::ReleaseTempRenderTargetBits( void )
{
	if( IsX360() ) //only sane on 360
	{
		int iNext;
		for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = iNext )
		{
			iNext = m_TextureList.Next( i );

			if ( m_TextureList[i]->IsTempRenderTarget() )
			{
				m_TextureList[i]->ReleaseMemory();
			}
		}
	}
}

void CTextureManager::DebugPrintUsedTextures( void )
{
	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		ITextureInternal *pTexture = m_TextureList[i];
		Msg( "Texture: '%s' RefCount: %d\n", pTexture->GetName(), pTexture->GetReferenceCount() );
	}

	if ( m_TextureExcludes.Count() )
	{
		Msg( "\nExcluded Textures: (%d)\n", m_TextureExcludes.Count() );
		for ( int i = m_TextureExcludes.First(); i != m_TextureExcludes.InvalidIndex(); i = m_TextureExcludes.Next( i ) )
		{
			char buff[256];
			const char *pName = m_TextureExcludes.GetElementName( i );
			V_snprintf( buff, sizeof( buff ), "Excluded: %d '%s' \n", m_TextureExcludes[i], pName );
	
			// an excluded texture is valid, but forced tiny
			if ( IsTextureLoaded( pName ) )
			{
				Msg( "%s", buff );
			}
			else
			{
				// warn as unknown, could be a spelling error
				Warning( "%s", buff );
			}
		}
	}
}

int CTextureManager::FindNext( int iIndex, ITextureInternal **pTexInternal )
{
	if ( iIndex == -1 && m_TextureList.Count() )
	{
		iIndex = m_TextureList.First();
	}
	else if ( !m_TextureList.Count() || !m_TextureList.IsValidIndex( iIndex ) )
	{
		*pTexInternal = NULL;
		return -1;
	}

	*pTexInternal = m_TextureList[iIndex];

	iIndex = m_TextureList.Next( iIndex );
	if ( iIndex == m_TextureList.InvalidIndex() )
	{
		// end of list
		iIndex = -1;
	}

	return iIndex;
}

void CTextureManager::Update()
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	#ifdef STAGING_ONLY
		if ( mat_texture_list_dump.GetBool() )
		{
			DumpTextureList();
			mat_texture_list_dump.SetValue( 0 );
		}
	#endif

	if ( m_pAsyncReader )
		m_pAsyncReader->ThreadMain_Update();
}

// Load a texture asynchronously and then call the provided callback.
void CTextureManager::AsyncFindOrLoadTexture( const char *pTextureName, const char *pTextureGroupName, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs, bool bComplain, int nAdditionalCreationFlags )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	bool bStreamingRequest = ( nAdditionalCreationFlags & TEXTUREFLAGS_STREAMABLE ) != 0;

	ITextureInternal* pLoadedTex = FindTexture( pTextureName );
	// It'd be weird to indicate that we're streaming and not actually have a texture that already exists.
	Assert( !bStreamingRequest || pLoadedTex != NULL );

	if ( pLoadedTex )
	{
		if ( !bStreamingRequest )
		{
			if ( pLoadedTex->IsError() && bComplain )
				DevWarning( "Texture '%s' not found.\n", pTextureName );
			pRecipient->OnAsyncFindComplete( pLoadedTex, pExtraArgs );
			SafeRelease( pRecipient );
			return;
		}
	}
	
	AsyncLoadJob_t asyncLoad( pTextureName, pTextureGroupName, pRecipient, pExtraArgs, bComplain, nAdditionalCreationFlags );

	// If this is the first person asking to load this, then remember so we don't load the same thing over and over again.
	int pendingIndex = m_PendingAsyncLoads.Find( pTextureName );
	if ( pendingIndex == m_PendingAsyncLoads.InvalidIndex() )
	{
		// Create the texture here, we'll load the data in the async thread. Load is a misnomer, because it doesn't actually
		// load the data--Download does.
		if ( bStreamingRequest )
			asyncLoad.m_pResultData = pLoadedTex;
		else
			asyncLoad.m_pResultData = LoadTexture( pTextureName, pTextureGroupName, nAdditionalCreationFlags, false );
		AsyncLoad( asyncLoad );
		pendingIndex = m_PendingAsyncLoads.Insert( pTextureName );
	}
	else 
	{
		// If this is a thing we've seen before, just note that we also need it.
		m_PendingAsyncLoads[ pendingIndex ].AddToTail( asyncLoad );
	}
}

void CTextureManager::CompleteAsyncLoad( AsyncLoadJob_t* pJob )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	Assert( pJob );
	bool bDownloaded = false;

	if ( !IsJobCancelled( pJob ) )
	{
		// Perform the download. We did the read already.
		pJob->m_pResultData->Download( NULL, pJob->m_nAdditionalCreationFlags );
		bDownloaded = true;
	}
	
	// Then notify the caller that they're finished.
	pJob->m_pRecipient->OnAsyncFindComplete( pJob->m_pResultData, pJob->m_pExtraArgs );
	
	// Finally, deal with any other stragglers that asked for the same surface we did.
	int pendingIndex = m_PendingAsyncLoads.Find( pJob->m_TextureName.Get() );
	Assert( pendingIndex != m_PendingAsyncLoads.InvalidIndex() );

	FOR_EACH_VEC( m_PendingAsyncLoads[ pendingIndex ], i )
	{
		AsyncLoadJob_t& straggler = m_PendingAsyncLoads[ pendingIndex ][ i ];
		straggler.m_pResultData = pJob->m_pResultData;

		if ( !bDownloaded && !IsJobCancelled( &straggler ) )
		{
			bDownloaded = true;
			straggler.m_pResultData->Download( NULL, straggler.m_nAdditionalCreationFlags );
		}

		straggler.m_pRecipient->OnAsyncFindComplete( straggler.m_pResultData, straggler.m_pExtraArgs );
		SafeRelease( &straggler.m_pRecipient );
	}

	// Add ourselves to the list of loaded things.
	if ( bDownloaded )
	{
		// The texture list has to be protected by the materials lock.
		MaterialLock_t hMaterialLock = materials->Lock();

		// It's possible that the texture wasn't actually unloaded, so we may have reloaded something unnecessarily.
		// If so, just don't re-add it.
		if ( m_TextureList.Find( pJob->m_pResultData->GetName() ) == m_TextureList.InvalidIndex() )
			m_TextureList.Insert( pJob->m_pResultData->GetName(), pJob->m_pResultData );

		materials->Unlock( hMaterialLock );
	}
	else
	{
		// If we didn't download, need to clean up the leftover file data that we loaded on the other thread
		pJob->m_pResultData->AsyncCancelReadTexture();
	}

	// Can't release the Recipient until after we tell the stragglers, because the recipient may be the only
	// ref to the texture, and cleaning it up may clean up the texture but leave us with a seemingly valid pointer.
	SafeRelease( &pJob->m_pRecipient );

	// Dump out the whole lot.
	m_PendingAsyncLoads.RemoveAt( pendingIndex );
}

void CTextureManager::AsyncLoad( const AsyncLoadJob_t& job )
{
	Assert( m_pAsyncLoader );
	m_pAsyncLoader->AsyncLoad( job );
}

void CTextureManager::AsyncCreateTextureFromRenderTarget( ITexture* pSrcRt, const char* pDstName, ImageFormat dstFmt, bool bGenMips, int nAdditionalCreationFlags, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	Assert( pSrcRt );

	AsyncReadJob_t* pAsyncRead = new AsyncReadJob_t( pSrcRt, pDstName, dstFmt, bGenMips, nAdditionalCreationFlags, pRecipient, pExtraArgs );
	AsyncReadTexture( pAsyncRead );
}

void CTextureManager::CompleteAsyncRead( AsyncReadJob_t* pJob )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	// Release the texture back into the pool.
	ReleaseReadbackTexture( pJob->m_pSysmemTex );
	pJob->m_pSysmemTex = NULL;

	int w = pJob->m_pSrcRt->GetActualWidth();
	int h = pJob->m_pSrcRt->GetActualHeight();

	int mips = pJob->m_bGenMips ? ImageLoader::GetNumMipMapLevels( w, h ) : 1;

	int nFlags = pJob->m_nAdditionalCreationFlags 
		       | TEXTUREFLAGS_SINGLECOPY
			   | TEXTUREFLAGS_IGNORE_PICMIP
			   | ( mips > 1
			       ? TEXTUREFLAGS_ALL_MIPS
				   : TEXTUREFLAGS_NOMIP 
			     )
	;

	// Create the texture
	ITexture* pFinalTex = materials->CreateNamedTextureFromBitsEx( pJob->m_pDstName, TEXTURE_GROUP_RUNTIME_COMPOSITE, w, h, mips, pJob->m_dstFmt, pJob->m_finalTexelData.Count(), pJob->m_finalTexelData.Base(), nFlags );
	Assert( pFinalTex );

	// Make the callback!
	pJob->m_pRecipient->OnAsyncCreateComplete( pFinalTex, pJob->m_pExtraArgs );
	SafeRelease( &pJob->m_pSrcRt );
	SafeRelease( &pJob->m_pRecipient );
	SafeRelease( &pFinalTex );
}

void CTextureManager::AsyncReadTexture( AsyncReadJob_t* pJob )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	Assert( m_pAsyncReader );
	Assert( pJob );

	pJob->m_pSysmemTex = AcquireReadbackTexture( pJob->m_pSrcRt->GetActualWidth(), pJob->m_pSrcRt->GetActualHeight(), pJob->m_pSrcRt->GetImageFormat() );
	Assert( pJob->m_pSysmemTex );

	if ( !pJob->m_pSysmemTex )
	{
		Assert( !"Need to deal with this error case" ); // TODOERROR
		return;
	}

	m_pAsyncReader->AsyncReadback( pJob );
}


ITextureInternal* CTextureManager::AcquireReadbackTexture( int w, int h, ImageFormat fmt )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s-TryExisting", __FUNCTION__ );
		MaterialLock_t hMaterialLock = materials->Lock();

		FOR_EACH_VEC( m_ReadbackTextures, i )
		{
			ITextureInternal* pTex = m_ReadbackTextures[ i ];
			Assert( pTex );

			if ( pTex->GetActualWidth() == w 
			  && pTex->GetActualHeight() == h 
			  && pTex->GetImageFormat() == fmt )
			{
				// Found one in the cache already
				pTex->AddRef();
				m_ReadbackTextures.Remove( i );

				materials->Unlock( hMaterialLock );
				return pTex;
			}
		}

		materials->Unlock( hMaterialLock );
	}

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s-CreateNew", __FUNCTION__ );
	ITextureInternal* stagingTex = CreateProceduralTexture( "readbacktex", TEXTURE_GROUP_OTHER, w, h, 1, fmt, TEXTUREFLAGS_STAGING_MEMORY | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_IMMEDIATE_CLEANUP );
	// AddRef here for caller.
	stagingTex->AddRef();
	return stagingTex;
}

void CTextureManager::ReleaseReadbackTexture( ITextureInternal* pTex )
{
	Assert( pTex );

	MaterialLock_t hMaterialLock = materials->Lock();
	// Release matching AddRef in AcquireReadbackTexture
	pTex->Release();
	m_ReadbackTextures.AddToTail( pTex );
	materials->Unlock( hMaterialLock );
}

#ifdef STAGING_ONLY
	static int SortTexturesForDump( const CUtlPair< CUtlString, void* >* sz1, const CUtlPair< CUtlString, void* >* sz2 )
	{
		int sortVal = CUtlString::SortCaseSensitive( &sz1->first, &sz2->first );
		if ( sortVal != 0 )
			return sortVal;

		return int( ( int ) sz1->second - ( int ) sz2->second );
	}

	void CTextureManager::DumpTextureList()
	{
		CUtlVector< CUtlPair< CUtlString, void* > > textures;
		MaterialLock_t hMaterialLock = materials->Lock();
		FOR_EACH_DICT( m_TextureList, i )
		{
			textures.AddToTail( MakeUtlPair( CUtlString( m_TextureList[i]->GetName() ), (void*) m_TextureList[i] ) );
		}
		materials->Unlock( hMaterialLock );

		// Now dump them out, sorted first by the texture name, then by address.
		textures.Sort( SortTexturesForDump );
		FOR_EACH_VEC( textures, i )
		{
			CUtlPair< CUtlString, void* >& pair = textures[i]; 
			Warning( "[%p]: %s\n", pair.second, pair.first.Get() )	;
		}
	}
#endif

//-----------------------------------------------------------------------------
// Warms the texture cache from a vpk. This will cause coarse mipmaps to be 
// available all the time, starting with mipmap level 3. This allows us to have
// all the textures available all the time, but we only pay for fine levels when
// we actually need them.
//-----------------------------------------------------------------------------
void CTextureManager::WarmTextureCache()
{
	// Disable cache for osx/linux for now.
	if ( CommandLine()->CheckParm( "-no_texture_stream" ) )
		return;
	MemoryInformation memInfo;
	if ( GetMemoryInformation( &memInfo ) )
	{
		if ( memInfo.m_nPhysicalRamMbTotal <= 3584 )
			return;
	}

	COM_TimestampedLog( "WarmTextureCache() - Begin" );

	// If this fires, we need to relocate this elsewhere--there's no point in doing the loading
	// if we're not going to be able to download them right now.
	Assert( g_pShaderAPI->CanDownloadTextures() );

	g_pFullFileSystem->AddSearchPath( "tf2_texture_cache.vpk", cTextureCachePathDir, PATH_ADD_TO_TAIL );
	
	CUtlDict< int > filesToLoad( k_eDictCompareTypeCaseSensitive );
	
	// TODO: Maybe work directly with VPK (still need to add to the filesystem for LoadTexture)?
	// CPackFile

	// Add the pak and then walk through the contents.
	FindFilesToLoad( &filesToLoad, "*.*" );

	// Then add the list of files from the cache, which will deal with running without a VPK and also 
	// allow us to add late stragglers.
	ReadFilesToLoad( &filesToLoad, "texture_preload_list.txt" );

	if ( filesToLoad.Count() == 0 )
	{
		COM_TimestampedLog( "WarmTextureCache() - End (No files loaded)" );
		return;
	}

	Assert( filesToLoad.Count() > 0 );

	// Now read all of the files. 
	// TODO: This needs to read in specific order to ensure peak performance.
	FOR_EACH_DICT( filesToLoad, i )
	{
		const char* pFilename = filesToLoad.GetElementName( i );

		// Load the texture. This will only load the lower mipmap levels because that's the file we'll find now.
		ITextureInternal* pTex = LoadTexture( pFilename, TEXTURE_GROUP_PRECACHED, TEXTUREFLAGS_STREAMABLE_COARSE );
		COM_TimestampedLog( "WarmTextureCache(): LoadTexture( %s ): Complete", pFilename );

		if ( ( pTex->GetFlags() & TEXTUREFLAGS_STREAMABLE ) == 0 )
		{
			STAGING_ONLY_EXEC( Warning( "%s is listed in texture_preload_list.txt or is otherwise marked for streaming. It cannot be streamed and should be removed from the streaming system.\n", pFilename ) );
			ITextureInternal::Destroy( pTex );
			continue;
		}

		if ( !pTex->IsError() )
		{
			m_TextureList.Insert( pTex->GetName(), pTex );
			pTex->AddRef();
			m_preloadedTextures.AddToTail( pTex );
		}
		else
		{
			// Don't preload broken textures
			ITextureInternal::Destroy( pTex );
		}
	}

	g_pFullFileSystem->RemoveSearchPath( "tf2_texture_cache.vpk", cTextureCachePathDir );

	COM_TimestampedLog( "WarmTextureCache() - End" );
}

//-----------------------------------------------------------------------------
// Reads the list of files contained in the vpk loaded above, and adds them to the
// list of files we need to load (passing in as pOutFilesToLoad). The map contains
// the 
//-----------------------------------------------------------------------------
void CTextureManager::FindFilesToLoad( CUtlDict< int >* pOutFilesToLoad, const char* pFilename )
{
	Assert( pOutFilesToLoad != NULL );

	FileFindHandle_t fh;
	pFilename = g_pFullFileSystem->FindFirstEx( pFilename, cTextureCachePathDir, &fh );

	while ( pFilename != NULL )
	{
		if ( g_pFullFileSystem->FindIsDirectory( fh ) )
		{
			if ( pFilename[0] != '.' ) 
			{
				char childFilename[_MAX_PATH];
				V_sprintf_safe( childFilename, "%s/*.*", pFilename );
				FindFilesToLoad( pOutFilesToLoad, childFilename );
			}
		}
		else
		{
			char filenameNoExtension[_MAX_PATH];
			V_StripExtension( pFilename, filenameNoExtension, _MAX_PATH );
			// Add the file to the list, which we will later traverse in order to ensure we're hitting these in the expected order for the VPK. 
			( *pOutFilesToLoad ).Insert( CUtlString( filenameNoExtension ), 0 );
		}

		pFilename = g_pFullFileSystem->FindNext( fh );			
	}
}

//-----------------------------------------------------------------------------
// Read the contents of pFilename, which should just be a list of texture names 
// that we should load.
//-----------------------------------------------------------------------------
void CTextureManager::ReadFilesToLoad( CUtlDict< int >* pOutFilesToLoad, const char* pFilename )
{
	Assert( pOutFilesToLoad != NULL );

	FileHandle_t fh = g_pFullFileSystem->Open( pFilename, "r" );
	if ( !fh )
		return;

	CUtlBuffer fileContents( 0, 0, CUtlBuffer::TEXT_BUFFER ); 
	if ( !g_pFullFileSystem->ReadToBuffer( fh, fileContents ) )
		goto cleanup;

	char buffer[_MAX_PATH + 1];
	while ( 1 ) 
	{
		fileContents.GetLine( buffer, _MAX_PATH );
		if ( buffer[ 0 ] == 0 )
			break;

		V_StripWhitespace( buffer );

		if ( buffer[ 0 ] == 0 )
			continue;

		// If it's not in the map already, add it.
		if ( pOutFilesToLoad->Find( buffer ) == pOutFilesToLoad->InvalidIndex() )
			( *pOutFilesToLoad ).Insert( buffer, 0 );
	}

cleanup:
	g_pFullFileSystem->Close( fh );
}

void CTextureManager::UpdatePostAsync()
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	// Update the async loader, which affects streaming in (streaming out is handled below).
	// Both stream in and stream out have to happen while the async job is not running because
	// they muck with shaderapi texture handles which could be in use if the async job is currently
	// being run
	if ( m_pAsyncLoader )
		m_pAsyncLoader->ThreadMain_Update();

	// First, move everything from the async request queue to active list
	ITextureInternal* pRequest = NULL;
	while ( m_asyncStreamingRequests.PopItem( &pRequest ) )
	{
		Assert( pRequest != NULL );

		// Update the LOD bias to smoothly stream the texture in. We only need to do this on frames that
		// we actually have been requested to draw--other frames it doesn't matter (see, because we're not drawing?) 
		pRequest->UpdateLodBias();
		m_textureStreamingRequests.InsertOrReplace( pRequest, g_FrameNum );	
	}

	// Then update streaming
	const int cThirtySecondsOrSoInFrames = 2000;

	// First, remove old stuff.
	FOR_EACH_MAP_FAST( m_textureStreamingRequests, i )
	{
		if ( m_textureStreamingRequests[ i ] + cThirtySecondsOrSoInFrames < g_FrameNum )
		{
			ITextureInternal* pTex = m_textureStreamingRequests.Key( i );

			// It's been awhile since we were asked to full res this texture, so let's evict 
			// if it's still full res.
			
			if ( pTex->GetTargetResidence() == RESIDENT_FULL )
				pTex->MakeResident( RESIDENT_PARTIAL );

			m_textureStreamingRequests.RemoveAt( i );
		}
	}

	// Then, start allowing new stuff to ask for data. 
	FOR_EACH_MAP_FAST( m_textureStreamingRequests, i )
	{
		int requestFrame = m_textureStreamingRequests[ i ];

		if ( g_FrameNum == requestFrame )
		{
			ITextureInternal* pTex = m_textureStreamingRequests.Key( i );

			if ( pTex->GetTargetResidence() == RESIDENT_FULL )
				continue;

			// TODO: What to do if this fails? Auto-reask next frame? 
			pTex->MakeResident( RESIDENT_FULL );
		}
	}

	// Finally, flush any immediate release textures marked for cleanup that are still unreferenced.
	CleanupPossiblyUnreferencedTextures();
}

void CTextureManager::ReleaseAsyncScratchVTF( IVTFTexture *pScratchVTF )
{
	Assert( m_pAsyncLoader != NULL && pScratchVTF != NULL );
	m_pAsyncLoader->ReleaseAsyncReadBuffer( pScratchVTF );
}

bool CTextureManager::ThreadInAsyncLoadThread() const
{
	return ThreadGetCurrentId() == m_nAsyncLoadThread;
}

bool CTextureManager::ThreadInAsyncReadThread() const
{
	return ThreadGetCurrentId() == m_nAsyncReadThread;
}

bool CTextureManager::AddTextureCompositorTemplate( const char* pName, KeyValues* pTmplDesc )
{
	Assert( pName && pTmplDesc );

	int ndx = m_TexCompTemplates.Find( pName );
	if ( ndx != m_TexCompTemplates.InvalidIndex() )
	{
		// Later definitions stomp earlier ones. This lets the GC win.
		delete m_TexCompTemplates[ ndx ];
		m_TexCompTemplates.RemoveAt( ndx );
	}

	CTextureCompositorTemplate* pNewTmpl = CTextureCompositorTemplate::Create( pName, pTmplDesc );
	
	// If this is the case, the logging has already been done.
	if ( pNewTmpl == NULL )
		return false;

	m_TexCompTemplates.Insert( pName, pNewTmpl );
	return true;
}

bool CTextureManager::VerifyTextureCompositorTemplates()
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL1 );

	bool allSuccess = true;

	FOR_EACH_DICT_FAST( m_TexCompTemplates, i )
	{
		if ( m_TexCompTemplates[ i ]->ResolveDependencies() )
		{
			if ( m_TexCompTemplates[ i ]->HasDependencyCycles() )
			{
				allSuccess = false;
			}
		}
		else
		{
			allSuccess = false;
		}
	}

	return allSuccess;
}


CTextureCompositorTemplate* CTextureManager::FindTextureCompositorTemplate( const char* pName )
{
	unsigned short i = m_TexCompTemplates.Find( pName );
	if ( m_TexCompTemplates.IsValidIndex( i ) )
		return m_TexCompTemplates[ i ];

	return NULL;
}

bool CTextureManager::HasPendingTextureDestroys() const
{
	return m_PossiblyUnreferencedTextures.Count() != 0;
}

void CTextureManager::CoolTextureCache()
{
	FOR_EACH_VEC( m_preloadedTextures, i )
	{
		m_preloadedTextures[ i ]->Release();
	}

	m_preloadedTextures.RemoveAll();
}

void CTextureManager::RequestAllMipmaps( ITextureInternal* pTex )
{
	Assert( pTex );

	// Don't mark these for load if suspended
	if ( m_iSuspendTextureStreaming )
		return;

	unsigned int nTexFlags = pTex->GetFlags();

	// If this isn't a streamable texture or if there are no mipmaps, there's nothing to do. 
	if ( !( nTexFlags & TEXTUREFLAGS_STREAMABLE ) || ( nTexFlags & TEXTUREFLAGS_NOMIP ) )
		return;

	m_asyncStreamingRequests.PushItem( pTex );	
}

void CTextureManager::EvictAllTextures()
{
	FOR_EACH_DICT_FAST( m_TextureList, i )
	{
		ITextureInternal* pTex = m_TextureList[ i ];
		if ( !pTex )
			continue;

		// If the fine mipmaps are present
		if ( ( ( pTex->GetFlags() & TEXTUREFLAGS_STREAMABLE ) != 0 ) && pTex->GetTargetResidence() == RESIDENT_FULL )
			pTex->MakeResident( RESIDENT_PARTIAL );
	}
}

CON_COMMAND( mat_evict_all, "Evict all fine mipmaps from the gpu" )
{
	TextureManager()->EvictAllTextures();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
static ImageFormat GetImageFormatRawReadback( ImageFormat fmt )
{
	switch ( fmt )
	{
	case IMAGE_FORMAT_RGBA8888:
		return IMAGE_FORMAT_BGRA8888;
	case IMAGE_FORMAT_BGRA8888:
		return IMAGE_FORMAT_BGRA8888;
	default:
		Assert( !"Unsupported format in GetImageFormatRawReadback, this will likely result in color-swapped textures" );
	};

	return fmt;
}

