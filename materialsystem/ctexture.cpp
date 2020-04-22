//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifdef PROTECTED_THINGS_ENABLE
	#undef PROTECTED_THINGS_ENABLE
#endif

#include "platform.h"

// HACK: Need ShellExecute for PSD updates
#ifdef IS_WINDOWS_PC
#include <windows.h>
#include <shellapi.h>
#pragma comment ( lib, "shell32"  )
#endif

#include "materialsystem_global.h"
#include "shaderapi/ishaderapi.h"
#include "itextureinternal.h"
#include "utlsymbol.h"
#include "time.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "bitmap/imageformat.h"
#include "bitmap/tgaloader.h"
#include "bitmap/tgawriter.h"
#ifdef _WIN32
#include "direct.h"
#endif
#include "colorspace.h"
#include "string.h"
#include <malloc.h>
#include <stdlib.h>
#include "utlmemory.h"
#include "IHardwareConfigInternal.h"
#include "filesystem.h"
#include "tier1/strtools.h"
#include "vtf/vtf.h"
#include "materialsystem/materialsystem_config.h"
#include "mempool.h"
#include "texturemanager.h"
#include "utlbuffer.h"
#include "pixelwriter.h"
#include "tier1/callqueue.h"
#include "tier1/UtlStringMap.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/fileutils.h"
#include "filesystem.h"
#include "tier2/p4helpers.h"
#include "tier2/tier2.h"
#include "p4lib/ip4.h"
#include "ctype.h"
#include "ifilelist.h"
#include "tier0/icommandline.h"
#include "tier0/vprof.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

// this allows the command line to force the "all mips" flag to on for all textures
bool g_bForceTextureAllMips = false;

#if defined(IS_WINDOWS_PC)
static void ConVarChanged_mat_managedtextures( IConVar *var, const char *pOldValue, float flOldValue );
static ConVar mat_managedtextures( "mat_managedtextures", "1", FCVAR_ARCHIVE, "If set, allows Direct3D to manage texture uploading at the cost of extra system memory", &ConVarChanged_mat_managedtextures );
static void ConVarChanged_mat_managedtextures( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( mat_managedtextures.GetBool() != (flOldValue != 0) )
	{
		materials->ReleaseResources();
		materials->ReacquireResources();
	}
}
#endif

static ConVar mat_spew_on_texture_size( "mat_spew_on_texture_size", "0", 0, "Print warnings about vtf content that isn't of the expected size" );
static ConVar mat_lodin_time( "mat_lodin_time", "5.0", FCVAR_DEVELOPMENTONLY );
static ConVar mat_lodin_hidden_pop( "mat_lodin_hidden_pop", "1", FCVAR_DEVELOPMENTONLY );

#define TEXTURE_FNAME_EXTENSION			".vtf"
#define TEXTURE_FNAME_EXTENSION_LEN		4
#define TEXTURE_FNAME_EXTENSION_NORMAL	"_normal.vtf"

#ifdef STAGING_ONLY
	ConVar mat_spewalloc( "mat_spewalloc", "0" );
#else
	ConVar mat_spewalloc( "mat_spewalloc", "0", FCVAR_ARCHIVE | FCVAR_DEVELOPMENTONLY );
#endif

struct TexDimensions_t
{
	uint16 m_nWidth;
	uint16 m_nHeight;
	uint16 m_nMipCount;
	uint16 m_nDepth;

	TexDimensions_t( uint16 nWidth = 0, uint nHeight = 0, uint nMipCount = 0, uint16 nDepth = 1 )
	: m_nWidth( nWidth )
	, m_nHeight( nHeight )
	, m_nMipCount( nMipCount )
	, m_nDepth( nDepth )
	{ }
};

#ifdef STAGING_ONLY
	struct TexInfo_t 
	{
		CUtlString m_Name;
		unsigned short m_nWidth;
		unsigned short m_nHeight;
		unsigned short m_nDepth;
		unsigned short m_nMipCount;
		unsigned short m_nFrameCount;
		unsigned short m_nCopies;
		ImageFormat m_Format;

		uint64 ComputeTexSize() const 
		{
			uint64 total = 0;
			unsigned short width = m_nWidth;
			unsigned short height = m_nHeight;
			unsigned short depth = m_nDepth;

			for ( int mip = 0; mip < m_nMipCount; ++mip ) 
			{
				// Make sure that mip count lines up with the count
				Assert( width > 1 || height > 1 || depth > 1 || ( mip == ( m_nMipCount - 1 ) ) );

				total += ImageLoader::GetMemRequired( width, height, depth, m_Format, false );

				width = Max( 1, width >> 1 );
				height = Max( 1, height >> 1 );
				depth = Max( 1, depth >> 1 );
			}

			return total * Min( (unsigned short) 1, m_nFrameCount ) * Min( (unsigned short) 1, m_nCopies );
		}

		TexInfo_t( const char* name = "", unsigned short w = 0, unsigned short h = 0, unsigned short d = 0, unsigned short mips = 0, unsigned short frames = 0, unsigned short copies = 0, ImageFormat fmt = IMAGE_FORMAT_UNKNOWN )
		: m_nWidth( w )
		, m_nHeight( h )
		, m_nDepth( d )
		, m_nMipCount( mips )
		, m_nFrameCount( frames )
		, m_nCopies( copies )
		, m_Format( fmt )
		{ 
			if ( name && name[0] )
				m_Name = name;
			else
				m_Name = "<unnamed>";
		}
	};

	CUtlMap< ITexture*, TexInfo_t > g_currentTextures( DefLessFunc( ITexture* ) );
#endif

//-----------------------------------------------------------------------------
// Internal texture flags
//-----------------------------------------------------------------------------
enum InternalTextureFlags
{
	TEXTUREFLAGSINTERNAL_ERROR				= 0x00000001,
	TEXTUREFLAGSINTERNAL_ALLOCATED			= 0x00000002,
	TEXTUREFLAGSINTERNAL_PRELOADED			= 0x00000004, // 360: textures that went through the preload process
	TEXTUREFLAGSINTERNAL_QUEUEDLOAD			= 0x00000008, // 360: load using the queued loader
	TEXTUREFLAGSINTERNAL_EXCLUDED			= 0x00000020, // actual exclusion state
	TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE		= 0x00000040, // desired exclusion state
	TEXTUREFLAGSINTERNAL_TEMPRENDERTARGET	= 0x00000080, // 360: should only allocate texture bits upon first resolve, destroy at level end
};

static int  GetThreadId();
static bool SLoadTextureBitsFromFile( IVTFTexture **ppOutVtfTexture, FileHandle_t hFile, unsigned int nFlags, TextureLODControlSettings_t* pInOutCachedFileLodSettings, int nDesiredDimensionLimit, unsigned short* pOutStreamedMips, const char* pName, const char* pCacheFileName, TexDimensions_t* pOptOutMappingDims = NULL, TexDimensions_t* pOptOutActualDims = NULL, TexDimensions_t* pOptOutAllocatedDims = NULL, unsigned int* pOptOutStripFlags = NULL );
static int  ComputeActualMipCount( const TexDimensions_t& actualDims, unsigned int nFlags );
static int  ComputeMipSkipCount( const char* pName, const TexDimensions_t& mappingDims, bool bIgnorePicmip, IVTFTexture *pOptVTFTexture, unsigned int nFlags, int nDesiredDimensionLimit, unsigned short* pOutStreamedMips, TextureLODControlSettings_t* pInOutCachedFileLodSettings, TexDimensions_t* pOptOutActualDims, TexDimensions_t* pOptOutAllocatedDims, unsigned int* pOptOutStripFlags  );
static int  GetOptimalReadBuffer( CUtlBuffer *pOutOptimalBuffer, FileHandle_t hFile, int nFileSize );
static void FreeOptimalReadBuffer( int nMaxSize );

//-----------------------------------------------------------------------------
// Use Warning to show texture flags.
//-----------------------------------------------------------------------------
static void PrintFlags( unsigned int flags )
{
	if ( flags & TEXTUREFLAGS_NOMIP )
	{
		Warning( "TEXTUREFLAGS_NOMIP|" );
	}
	if ( flags & TEXTUREFLAGS_NOLOD )
	{
		Warning( "TEXTUREFLAGS_NOLOD|" );
	}
	if ( flags & TEXTUREFLAGS_SRGB )
	{
		Warning( "TEXTUREFLAGS_SRGB|" );
	}
	if ( flags & TEXTUREFLAGS_POINTSAMPLE )
	{
		Warning( "TEXTUREFLAGS_POINTSAMPLE|" );
	}
	if ( flags & TEXTUREFLAGS_TRILINEAR )
	{
		Warning( "TEXTUREFLAGS_TRILINEAR|" );
	}
	if ( flags & TEXTUREFLAGS_CLAMPS )
	{
		Warning( "TEXTUREFLAGS_CLAMPS|" );
	}
	if ( flags & TEXTUREFLAGS_CLAMPT )
	{
		Warning( "TEXTUREFLAGS_CLAMPT|" );
	}
	if ( flags & TEXTUREFLAGS_HINT_DXT5 )
	{
		Warning( "TEXTUREFLAGS_HINT_DXT5|" );
	}
	if ( flags & TEXTUREFLAGS_ANISOTROPIC )
	{
		Warning( "TEXTUREFLAGS_ANISOTROPIC|" );
	}
	if ( flags & TEXTUREFLAGS_PROCEDURAL )
	{
		Warning( "TEXTUREFLAGS_PROCEDURAL|" );
	}
	if ( flags & TEXTUREFLAGS_ALL_MIPS )
	{
		Warning( "TEXTUREFLAGS_ALL_MIPS|" );
	}
	if ( flags & TEXTUREFLAGS_SINGLECOPY )
	{
		Warning( "TEXTUREFLAGS_SINGLECOPY|" );
	}
	if ( flags & TEXTUREFLAGS_STAGING_MEMORY )
	{
		Warning( "TEXTUREFLAGS_STAGING_MEMORY|" );
	}
	if ( flags & TEXTUREFLAGS_IGNORE_PICMIP )
	{
		Warning( "TEXTUREFLAGS_IGNORE_PICMIP|" );
	}
	if ( flags & TEXTUREFLAGS_IMMEDIATE_CLEANUP )
	{
		Warning( "TEXTUREFLAGS_IMMEDIATE_CLEANUP|" );
	}
}


namespace TextureLodOverride
{
	struct OverrideInfo
	{
		OverrideInfo() : x( 0 ), y( 0 ) {}
		OverrideInfo( int8 x_, int8 y_ ) : x( x_ ), y( y_ ) {}
		int8 x, y;
	};

	// Override map
	typedef CUtlStringMap< OverrideInfo > OverrideMap_t;
	OverrideMap_t s_OverrideMap;

	// Retrieves the override info adjustments
	OverrideInfo Get( char const *szName )
	{
		UtlSymId_t idx = s_OverrideMap.Find( szName );
		if ( idx != s_OverrideMap.InvalidIndex() )
			return s_OverrideMap[ idx ];
		else
			return OverrideInfo();
	}

	// Combines the existing override info adjustments with the given one
	void Add( char const *szName, OverrideInfo oi )
	{
		OverrideInfo oiex = Get( szName );
		oiex.x += oi.x;
		oiex.y += oi.y;
		s_OverrideMap[ szName ] = oiex;
	}

}; // end namespace TextureLodOverride

class CTextureStreamingJob;

//-----------------------------------------------------------------------------
// Base texture class
//-----------------------------------------------------------------------------

class CTexture : public ITextureInternal
{
public:
	CTexture();
	virtual ~CTexture();

	virtual const char *GetName( void ) const;
	const char *GetTextureGroupName( void ) const;

	// Stats about the texture itself
	virtual ImageFormat GetImageFormat() const;
	NormalDecodeMode_t GetNormalDecodeMode() const { return NORMAL_DECODE_NONE; }
	virtual int GetMappingWidth() const;
	virtual int GetMappingHeight() const;
	virtual int GetActualWidth() const;
	virtual int GetActualHeight() const;
	virtual int GetNumAnimationFrames() const;
	virtual bool IsTranslucent() const;
	virtual void GetReflectivity( Vector& reflectivity );

	// Reference counting
	virtual void IncrementReferenceCount( );
	virtual void DecrementReferenceCount( );
	virtual int GetReferenceCount( );

	// Used to modify the texture bits (procedural textures only)
	virtual void SetTextureRegenerator( ITextureRegenerator *pTextureRegen );

	// Little helper polling methods
	virtual bool IsNormalMap( ) const;
	virtual bool IsCubeMap( void ) const;
	virtual bool IsRenderTarget( ) const;
	virtual bool IsTempRenderTarget( void ) const;
	virtual bool IsProcedural() const;
	virtual bool IsMipmapped() const;
	virtual bool IsError() const;

	// For volume textures
	virtual bool IsVolumeTexture() const;
	virtual int GetMappingDepth() const;
	virtual int GetActualDepth() const;

	// Various ways of initializing the texture
	void InitFileTexture( const char *pTextureFile, const char *pTextureGroupName );
	void InitProceduralTexture( const char *pTextureName, const char *pTextureGroupName, int w, int h, int d, ImageFormat fmt, int nFlags, ITextureRegenerator* generator = NULL );

	// Releases the texture's hw memory
	void ReleaseMemory();

	virtual void OnRestore();

	// Sets the filtering modes on the texture we're modifying
	void SetFilteringAndClampingMode( bool bOnlyLodValues = false );
	void Download( Rect_t *pRect = NULL, int nAdditionalCreationFlags = 0 );

	// Loads up information about the texture 
	virtual void Precache();

	// FIXME: Bogus methods... can we please delete these?
	virtual void GetLowResColorSample( float s, float t, float *color ) const;

	// Gets texture resource data of the specified type.
	// Params:
	//		eDataType		type of resource to retrieve.
	//		pnumBytes		on return is the number of bytes available in the read-only data buffer or is undefined
	// Returns:
	//		pointer to the resource data, or NULL. Note that the data from this pointer can disappear when
	// the texture goes away - you want to copy this data!
	virtual void *GetResourceData( uint32 eDataType, size_t *pNumBytes ) const;

	virtual int GetApproximateVidMemBytes( void ) const;

	// Stretch blit the framebuffer into this texture.
	virtual void CopyFrameBufferToMe( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL );
	virtual void CopyMeToFrameBuffer( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL );

	virtual ITexture *GetEmbeddedTexture( int nIndex );

	// Get the shaderapi texture handle associated w/ a particular frame
	virtual ShaderAPITextureHandle_t GetTextureHandle( int nFrame, int nChannel = 0 );

	// Sets the texture as the render target
	virtual void Bind( Sampler_t sampler );
	virtual void Bind( Sampler_t sampler1, int nFrame, Sampler_t sampler2 = (Sampler_t) -1 );
	virtual void BindVertexTexture( VertexTextureSampler_t stage, int nFrame );

	// Set this texture as a render target	
	bool SetRenderTarget( int nRenderTargetID );

	// Set this texture as a render target (optionally set depth texture as depth buffer as well)
	bool SetRenderTarget( int nRenderTargetID, ITexture *pDepthTexture);

	virtual void MarkAsPreloaded( bool bSet );
	virtual bool IsPreloaded() const;

	virtual void MarkAsExcluded( bool bSet, int nDimensionsLimit );
	virtual bool UpdateExcludedState( void );

	// Retrieve the vtf flags mask
	virtual unsigned int GetFlags( void ) const;

	virtual void ForceLODOverride( int iNumLodsOverrideUpOrDown );

	void GetFilename( char *pOut, int maxLen ) const;
	virtual void ReloadFilesInList( IFileList *pFilesToReload );

	// Save texture to a file.
	virtual bool SaveToFile( const char *fileName );
	
	// Load the texture from a file.
	bool AsyncReadTextureFromFile( IVTFTexture* pVTFTexture, unsigned int nAdditionalCreationFlags );
	void AsyncCancelReadTexture( );

	virtual void Map( void** pOutBits, int* pOutPitch );
	virtual void Unmap();

	virtual ResidencyType_t GetCurrentResidence() const { return m_residenceCurrent; }
	virtual ResidencyType_t GetTargetResidence() const { return m_residenceTarget; }
	virtual bool MakeResident( ResidencyType_t newResidence );
	virtual void UpdateLodBias();

protected:
	bool IsDepthTextureFormat( ImageFormat fmt );
	void ReconstructTexture( bool bCopyFromCurrent );
	void GetCacheFilename( char* pOutBuffer, int bufferSize ) const;
	bool GetFileHandle( FileHandle_t *pOutFileHandle, char *pCacheFilename, char **ppResolvedFilename ) const;

	void ReconstructPartialTexture( const Rect_t *pRect );
	bool HasBeenAllocated() const;
	void WriteDataToShaderAPITexture( int nFrameCount, int nFaceCount, int nFirstFace, int nMipCount, IVTFTexture *pVTFTexture, ImageFormat fmt );

	// Initializes/shuts down the texture
	void Init( int w, int h, int d, ImageFormat fmt, int iFlags, int iFrameCount );
	void Shutdown();

	// Sets the texture name
	void SetName( const char* pName );

	// Assigns/releases texture IDs for our animation frames
	void AllocateTextureHandles( );
	void ReleaseTextureHandles( );

	// Calculates info about whether we can make the texture smaller and by how much
	// Returns the number of skipped mip levels
	int ComputeActualSize( bool bIgnorePicmip = false, IVTFTexture *pVTFTexture = NULL, bool bTextureMigration = false );

	// Computes the actual format of the texture given a desired src format
	ImageFormat ComputeActualFormat( ImageFormat srcFormat );

	// Creates/releases the shader api texture
	bool AllocateShaderAPITextures();
	void FreeShaderAPITextures();
	void MigrateShaderAPITextures();
	void NotifyUnloadedFile();

	// Download bits
	void DownloadTexture( Rect_t *pRect, bool bCopyFromCurrent = false );
	void ReconstructTextureBits(Rect_t *pRect);

	// Gets us modifying a particular frame of our texture
	void Modify( int iFrame );

	// Sets the texture clamping state on the currently modified frame
	void SetWrapState( );

	// Sets the texture filtering state on the currently modified frame
	void SetFilterState();

	// Sets the lod state on the currently modified frame
	void SetLodState();

	// Loads the texture bits from a file. Optionally provides absolute path
	IVTFTexture *LoadTextureBitsFromFile( char *pCacheFileName, char **pResolvedFilename );
	IVTFTexture *HandleFileLoadFailedTexture( IVTFTexture *pVTFTexture );

	// Generates the procedural bits
	IVTFTexture *ReconstructProceduralBits( );
	IVTFTexture *ReconstructPartialProceduralBits( const Rect_t *pRect, Rect_t *pActualRect );

	// Sets up debugging texture bits, if appropriate
	bool SetupDebuggingTextures( IVTFTexture *pTexture );

	// Generate a texture that shows the various mip levels
	void GenerateShowMipLevelsTextures( IVTFTexture *pTexture );

	void Cleanup( void );

	// Converts a source image read from disk into its actual format
	bool ConvertToActualFormat( IVTFTexture *pTexture );

	// Builds the low-res image from the texture 
	void LoadLowResTexture( IVTFTexture *pTexture );
	void CopyLowResImageToTexture( IVTFTexture *pTexture );
	
	void GetDownloadFaceCount( int &nFirstFace, int &nFaceCount );
	void ComputeMipLevelSubRect( const Rect_t* pSrcRect, int nMipLevel, Rect_t *pSubRect );

	IVTFTexture *GetScratchVTFTexture( );
	void ReleaseScratchVTFTexture( IVTFTexture* tex );

	void ApplyRenderTargetSizeMode( int &width, int &height, ImageFormat fmt );

	virtual void CopyToStagingTexture( ITexture* pDstTex );

	virtual void SetErrorTexture( bool _isErrorTexture );

	// Texture streaming
	void MakeNonResident();
	void MakePartiallyResident();
	bool MakeFullyResident();

	void CancelStreamingJob( bool bJobMustExist = true );
	void OnStreamingJobComplete( ResidencyType_t newResidenceCurrent );

protected:
#ifdef _DEBUG
	char *m_pDebugName;
#endif

	// Reflectivity vector
	Vector m_vecReflectivity;

	CUtlSymbol m_Name;

	// What texture group this texture is in (winds up setting counters based on the group name,
	// then the budget panel views the counters).
	CUtlSymbol m_TextureGroupName;

	unsigned int m_nFlags;
	unsigned int m_nInternalFlags;

	CInterlockedInt m_nRefCount;

	// This is the *desired* image format, which may or may not represent reality
	ImageFormat m_ImageFormat;

	// mapping dimensions and actual dimensions can/will vary due to user settings, hardware support, etc.
	// Allocated is what is physically allocated on the hardware at this instant, and considers texture streaming.
	TexDimensions_t m_dimsMapping;
	TexDimensions_t m_dimsActual;
	TexDimensions_t m_dimsAllocated;

	// This is the iWidth/iHeight for whatever is downloaded to the card, ignoring current streaming settings
	// Some callers want to know how big the texture is if all data was present, and that's this. 
	// TODO: Rename this before check in.
	unsigned short m_nFrameCount;

	// These are the values for what is truly allocated on the card, including streaming settings. 
	unsigned short m_nStreamingMips;

	unsigned short m_nOriginalRTWidth;	// The values they initially specified. We generated a different width
	unsigned short m_nOriginalRTHeight;	// and height based on screen size and the flags they specify.

	unsigned char m_LowResImageWidth;
	unsigned char m_LowResImageHeight;

	unsigned short m_nDesiredDimensionLimit;	// part of texture exclusion
	unsigned short m_nActualDimensionLimit;		// value not necessarily accurate, but mismatch denotes dirty state

	// m_pStreamingJob is refcounted, but it is not safe to call SafeRelease directly on it--you must call 
	// CancelStreamingJob to ensure that releasing it doesn't cause a crash.
	CTextureStreamingJob* m_pStreamingJob;
	IVTFTexture* m_pStreamingVTF;
	ResidencyType_t m_residenceTarget;
	ResidencyType_t m_residenceCurrent;
	int m_lodClamp;
	int m_lastLodBiasAdjustFrame;
	float m_lodBiasInitial;
	float m_lodBiasCurrent; 
	double m_lodBiasStartTime;

	// If the read failed, this will be true. We can't just return from the function because the call may
	// happen in the async thread.
	bool m_bStreamingFileReadFailed;


	// The set of texture ids for each animation frame
	ShaderAPITextureHandle_t *m_pTextureHandles;

	TextureLODControlSettings_t m_cachedFileLodSettings;

	// lowresimage info - used for getting color data from a texture
	// without having a huge system mem overhead.
	// FIXME: We should keep this in compressed form. .is currently decompressed at load time.
	unsigned char *m_pLowResImage;

	ITextureRegenerator *m_pTextureRegenerator;

	// Used to help decide whether or not to recreate the render target if AA changes.
	RenderTargetType_t m_nOriginalRenderTargetType;
	RenderTargetSizeMode_t m_RenderTargetSizeMode;

	// Fixed-size allocator
//	DECLARE_FIXEDSIZE_ALLOCATOR( CTexture );
public:
	void InitRenderTarget( const char *pRTName, int w, int h, RenderTargetSizeMode_t sizeMode, 
		ImageFormat fmt, RenderTargetType_t type, unsigned int textureFlags,
		unsigned int renderTargetFlags );
	
	virtual void DeleteIfUnreferenced();

	void FixupTexture( const void *pData, int nSize, LoaderError_t loaderError );

	void SwapContents( ITexture *pOther );

protected:
	// private data, generally from VTF resource extensions
	struct DataChunk
	{
		void Allocate( unsigned int numBytes ) 
		{
			m_pvData = new unsigned char[ numBytes ];
			m_numBytes = numBytes;
		}
		void Deallocate() const { delete [] m_pvData; }
		
		unsigned int m_eType;
		unsigned int m_numBytes;
		unsigned char *m_pvData;
	};
	CUtlVector< DataChunk > m_arrDataChunks;

	struct ScratchVTF
	{
		ScratchVTF( CTexture* _tex ) : m_pParent( _tex ), m_pScratchVTF( _tex->GetScratchVTFTexture( ) ) { }
		~ScratchVTF( ) 
		{
			if ( m_pScratchVTF ) 
				m_pParent->ReleaseScratchVTFTexture( m_pScratchVTF ); 
			m_pScratchVTF = NULL; 
		}

		IVTFTexture* Get() const { return m_pScratchVTF; }
		void TakeOwnership() { m_pScratchVTF = NULL; }

		CTexture* m_pParent;
		IVTFTexture* m_pScratchVTF;
	};

	friend class CTextureStreamingJob;
};

class CTextureStreamingJob : public IAsyncTextureOperationReceiver
{
public:
	CTextureStreamingJob( CTexture* pTex ) : m_referenceCount( 0 ), m_pOwner( pTex ) { Assert( m_pOwner != NULL ); m_pOwner->AddRef(); } 
	virtual ~CTextureStreamingJob() { SafeRelease( &m_pOwner ); }

	virtual int AddRef() OVERRIDE { return ++m_referenceCount; }
	virtual int Release() OVERRIDE { int retVal = --m_referenceCount; Assert( retVal >= 0 ); if ( retVal == 0 ) { delete this; } return retVal; }
	virtual int GetRefCount() const OVERRIDE { return m_referenceCount; }

	virtual void OnAsyncCreateComplete( ITexture* pTex, void* pExtraArgs ) OVERRIDE { Assert( !"unimpl" ); }
	virtual void OnAsyncFindComplete( ITexture* pTex, void* pExtraArgs ) OVERRIDE;
	virtual void OnAsyncMapComplete( ITexture* pTex, void* pExtraArgs, void* pMemory, int nPitch ) { Assert( !"unimpl" ); }
	virtual void OnAsyncReadbackBegin( ITexture* pDst, ITexture* pSrc, void* pExtraArgs ) OVERRIDE { Assert( !"unimpl" ); }

	void ForgetOwner( ITextureInternal* pTex ) { Assert( pTex == m_pOwner ); SafeRelease( &m_pOwner ); }

private:
	CInterlockedInt m_referenceCount;
	CTexture* m_pOwner;
};

//////////////////////////////////////////////////////////////////////////
//
// CReferenceToHandleTexture is a special implementation of ITexture
// to be used solely for binding the texture handle when rendering.
// It is used when a D3D texture handle is available, but should be used
// at a higher level of abstraction requiring an ITexture or ITextureInternal.
//
//////////////////////////////////////////////////////////////////////////
class CReferenceToHandleTexture : public ITextureInternal
{
public:
	CReferenceToHandleTexture();
	virtual ~CReferenceToHandleTexture();

	virtual const char *GetName( void ) const { return m_Name.String(); }
	const char *GetTextureGroupName( void ) const { return m_TextureGroupName.String(); }

	// Stats about the texture itself
	virtual ImageFormat GetImageFormat() const { return IMAGE_FORMAT_UNKNOWN; }
	virtual NormalDecodeMode_t GetNormalDecodeMode() const { return NORMAL_DECODE_NONE; }
	virtual int GetMappingWidth() const { return 1; }
	virtual int GetMappingHeight() const { return 1; }
	virtual int GetActualWidth() const { return 1; }
	virtual int GetActualHeight() const { return 1; }
	virtual int GetNumAnimationFrames() const { return 1; }
	virtual bool IsTranslucent() const { return false; }
	virtual void GetReflectivity( Vector& reflectivity ) { reflectivity.Zero(); }

	// Reference counting
	virtual void IncrementReferenceCount( ) { ++ m_nRefCount; }
	virtual void DecrementReferenceCount( ) { -- m_nRefCount; }
	virtual int GetReferenceCount( ) { return m_nRefCount; }

	// Used to modify the texture bits (procedural textures only)
	virtual void SetTextureRegenerator( ITextureRegenerator *pTextureRegen ) { NULL; }

	// Little helper polling methods
	virtual bool IsNormalMap( ) const { return false; }
	virtual bool IsCubeMap( void ) const { return false; }
	virtual bool IsRenderTarget( ) const { return false; }
	virtual bool IsTempRenderTarget( void ) const { return false; }
	virtual bool IsProcedural() const { return true; }
	virtual bool IsMipmapped() const { return false; }
	virtual bool IsError() const { return false; }

	// For volume textures
	virtual bool IsVolumeTexture() const { return false; }
	virtual int GetMappingDepth() const { return 1; }
	virtual int GetActualDepth() const { return 1; }

	// Releases the texture's hw memory
	void ReleaseMemory() { NULL; }

	virtual void OnRestore() { NULL; }

	// Sets the filtering modes on the texture we're modifying
	void SetFilteringAndClampingMode( bool bOnlyLodValues = false ) { NULL; }
	void Download( Rect_t *pRect = NULL, int nAdditionalCreationFlags = 0 ) { NULL; }

	// Loads up information about the texture 
	virtual void Precache() { NULL; }

	// FIXME: Bogus methods... can we please delete these?
	virtual void GetLowResColorSample( float s, float t, float *color ) const { NULL; }

	// Gets texture resource data of the specified type.
	// Params:
	//		eDataType		type of resource to retrieve.
	//		pnumBytes		on return is the number of bytes available in the read-only data buffer or is undefined
	// Returns:
	//		pointer to the resource data, or NULL. Note that the data from this pointer can disappear when
	// the texture goes away - you want to copy this data!
	virtual void *GetResourceData( uint32 eDataType, size_t *pNumBytes ) const { return NULL; }

	virtual int GetApproximateVidMemBytes( void ) const { return 32; }

	// Stretch blit the framebuffer into this texture.
	virtual void CopyFrameBufferToMe( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) { NULL; }
	virtual void CopyMeToFrameBuffer( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) { NULL; }

	virtual ITexture *GetEmbeddedTexture( int nIndex ) { return ( nIndex == 0 ) ? this : NULL; }

	// Get the shaderapi texture handle associated w/ a particular frame
	virtual ShaderAPITextureHandle_t GetTextureHandle( int nFrame, int nTextureChannel = 0 ) { return m_hTexture; }

	// Bind the texture
	virtual void Bind( Sampler_t sampler );
	virtual void Bind( Sampler_t sampler1, int nFrame, Sampler_t sampler2 = (Sampler_t) -1 );
	virtual void BindVertexTexture( VertexTextureSampler_t stage, int nFrame );

	// Set this texture as a render target	
	bool SetRenderTarget( int nRenderTargetID ) { return SetRenderTarget( nRenderTargetID, NULL ); }

	// Set this texture as a render target (optionally set depth texture as depth buffer as well)
	bool SetRenderTarget( int nRenderTargetID, ITexture *pDepthTexture) { return false; }

	virtual void MarkAsPreloaded( bool bSet ) { NULL; }
	virtual bool IsPreloaded() const { return true; }

	virtual void MarkAsExcluded( bool bSet, int nDimensionsLimit ) { NULL; }
	virtual bool UpdateExcludedState( void ) { return true; }

	// Retrieve the vtf flags mask
	virtual unsigned int GetFlags( void ) const { return 0; }

	virtual void ForceLODOverride( int iNumLodsOverrideUpOrDown ) { NULL; }

	virtual void ReloadFilesInList( IFileList *pFilesToReload ) {}

	// Save texture to a file.
	virtual bool SaveToFile( const char *fileName ) { return false; }

	virtual bool AsyncReadTextureFromFile( IVTFTexture* pVTFTexture, unsigned int nAdditionalCreationFlags ) { Assert( !"Should never get here." ); return false; }
	virtual void AsyncCancelReadTexture() { Assert( !"Should never get here." ); }

	virtual void CopyToStagingTexture( ITexture* pDstTex ) { Assert( !"Should never get here." ); };

	// Map and unmap. These can fail. And can cause a very significant perf penalty. Be very careful with them.
	virtual void Map( void** pOutBits, int* pOutPitch ) { }
	virtual void Unmap() { }

	virtual ResidencyType_t GetCurrentResidence() const { return RESIDENT_FULL; }
	virtual ResidencyType_t GetTargetResidence() const { return RESIDENT_FULL; }
	virtual bool MakeResident( ResidencyType_t newResidence ) { Assert( !"Unimpl" ); return true; }
	virtual void UpdateLodBias() {}

	virtual void SetErrorTexture( bool isErrorTexture ) { }

protected:
#ifdef _DEBUG
	char *m_pDebugName;
#endif

	CUtlSymbol m_Name;

	// What texture group this texture is in (winds up setting counters based on the group name,
	// then the budget panel views the counters).
	CUtlSymbol m_TextureGroupName;

	// The set of texture ids for each animation frame
	ShaderAPITextureHandle_t m_hTexture;

	// Refcount
	int m_nRefCount;

public:
	virtual void DeleteIfUnreferenced();

	void FixupTexture( const void *pData, int nSize, LoaderError_t loaderError ) { NULL; }

	void SwapContents( ITexture *pOther ) { NULL; }

public:
	void SetName( char const *szName );
	void InitFromHandle(
		const char *pTextureName,
		const char *pTextureGroupName,
		ShaderAPITextureHandle_t hTexture );
};

CReferenceToHandleTexture::CReferenceToHandleTexture() :
	m_hTexture( INVALID_SHADERAPI_TEXTURE_HANDLE ),
#ifdef _DEBUG
	m_pDebugName( NULL ),
#endif
	m_nRefCount( 0 )
{
	NULL;
}

CReferenceToHandleTexture::~CReferenceToHandleTexture()
{
#ifdef _DEBUG
	if ( m_nRefCount != 0 )
	{
		Warning( "Reference Count(%d) != 0 in ~CReferenceToHandleTexture for texture \"%s\"\n", m_nRefCount, m_Name.String() );
	}
	if ( m_pDebugName )
	{
		delete [] m_pDebugName;
	}
#endif
}

void CReferenceToHandleTexture::SetName( char const *szName )
{
	// normalize and convert to a symbol
	char szCleanName[MAX_PATH];
	m_Name = NormalizeTextureName( szName, szCleanName, sizeof( szCleanName ) );

#ifdef _DEBUG
	if ( m_pDebugName )
	{
		delete [] m_pDebugName;
	}
	int nLen = V_strlen( szCleanName ) + 1;
	m_pDebugName = new char[nLen];
	V_memcpy( m_pDebugName, szCleanName, nLen );
#endif
}

void CReferenceToHandleTexture::InitFromHandle( const char *pTextureName, const char *pTextureGroupName, ShaderAPITextureHandle_t hTexture )
{
	SetName( pTextureName );
	m_TextureGroupName = pTextureGroupName;
	m_hTexture = hTexture;
}

void CReferenceToHandleTexture::Bind( Sampler_t sampler )
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		g_pShaderAPI->BindTexture( sampler, m_hTexture );
	}
}


// TODO: make paired textures work with mat_texture_list
void CReferenceToHandleTexture::Bind( Sampler_t sampler1, int nFrame, Sampler_t sampler2 /* = -1 */ )
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		g_pShaderAPI->BindTexture( sampler1, m_hTexture );
	}
}


void CReferenceToHandleTexture::BindVertexTexture( VertexTextureSampler_t sampler, int nFrame )
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		g_pShaderAPI->BindVertexTexture( sampler, m_hTexture );
	}
}

void CReferenceToHandleTexture::DeleteIfUnreferenced()
{
	if ( m_nRefCount > 0 )
		return;

	TextureManager()->RemoveTexture( this );
}


//-----------------------------------------------------------------------------
// Fixed-size allocator
//-----------------------------------------------------------------------------
//DEFINE_FIXEDSIZE_ALLOCATOR( CTexture, 1024, true );


//-----------------------------------------------------------------------------
// Static instance of VTF texture
//-----------------------------------------------------------------------------
#define MAX_RENDER_THREADS 4

// For safety's sake, we allow any of the threads that intersect with rendering
// to have their own state vars. In practice, we expect only the matqueue thread 
// and the main thread to ever hit s_pVTFTexture. 
static IVTFTexture *s_pVTFTexture[ MAX_RENDER_THREADS ] = { NULL };

// We only expect that the main thread or the matqueue thread to actually touch 
// these, but we still need a NULL and size of 0 for the other threads. 
static void *s_pOptimalReadBuffer[ MAX_RENDER_THREADS ] = { NULL };
static int s_nOptimalReadBufferSize[ MAX_RENDER_THREADS ] = { 0 };

//-----------------------------------------------------------------------------
// Class factory methods
//-----------------------------------------------------------------------------
ITextureInternal *ITextureInternal::CreateFileTexture( const char *pFileName, const char *pTextureGroupName )
{
	CTexture *pTex = new CTexture;
	pTex->InitFileTexture( pFileName, pTextureGroupName );
	return pTex;
}

ITextureInternal *ITextureInternal::CreateReferenceTextureFromHandle(
	const char *pTextureName,
	const char *pTextureGroupName,
	ShaderAPITextureHandle_t hTexture )
{
	CReferenceToHandleTexture *pTex = new CReferenceToHandleTexture;
	pTex->InitFromHandle( pTextureName, pTextureGroupName, hTexture );
	return pTex;
}

ITextureInternal *ITextureInternal::CreateProceduralTexture( 
	const char			*pTextureName, 
	const char			*pTextureGroupName, 
	int					w, 
	int					h, 
	int					d,
	ImageFormat			fmt, 
	int					nFlags,
	ITextureRegenerator *generator)
{
	CTexture *pTex = new CTexture;
	pTex->InitProceduralTexture( pTextureName, pTextureGroupName, w, h, d, fmt, nFlags, generator );
	pTex->IncrementReferenceCount();
	return pTex;
}

// GR - named RT
ITextureInternal *ITextureInternal::CreateRenderTarget( 
	const char *pRTName, 
	int w, 
	int h, 
	RenderTargetSizeMode_t sizeMode, 
	ImageFormat fmt, 
	RenderTargetType_t type, 
	unsigned int textureFlags, 
	unsigned int renderTargetFlags )
{
	CTexture *pTex = new CTexture;
	pTex->InitRenderTarget( pRTName, w, h, sizeMode, fmt, type, textureFlags, renderTargetFlags );

	return pTex;
}

//-----------------------------------------------------------------------------
// Rebuild and exisiting render target in place.
//-----------------------------------------------------------------------------
void ITextureInternal::ChangeRenderTarget( 
	ITextureInternal *pTex,
	int w,
	int	h,
	RenderTargetSizeMode_t sizeMode, 
	ImageFormat fmt, 
	RenderTargetType_t type, 
	unsigned int textureFlags, 
	unsigned int renderTargetFlags )
{
	pTex->ReleaseMemory();
	dynamic_cast< CTexture * >(pTex)->InitRenderTarget( pTex->GetName(), w, h, sizeMode, fmt, type, textureFlags, renderTargetFlags );
}

void ITextureInternal::Destroy( ITextureInternal *pTex, bool bSkipTexMgrCheck )
{
	#ifdef STAGING_ONLY
		if ( !bSkipTexMgrCheck && TextureManager()->HasPendingTextureDestroys() )
		{
			// Multithreading badness. This will cause a crash later! Grab JohnS or McJohn know!
			DebuggerBreakIfDebugging_StagingOnly();
		}
	#endif

	int iIndex = g_pTextureRefList->Find( static_cast<ITexture*>( pTex ) );
	if ( iIndex != g_pTextureRefList->InvalidIndex () )
	{
		if ( g_pTextureRefList->Element(iIndex) != 0 )
		{
			int currentCount = g_pTextureRefList->Element( iIndex );
			Warning( "Destroying a texture that is in the queue: %s (%p): %d!\n", pTex->GetName(), pTex, currentCount );
		}
	}

	delete pTex;
}

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CTexture::CTexture() : m_ImageFormat( IMAGE_FORMAT_UNKNOWN )
{
	m_dimsActual.m_nMipCount = 0;
	m_dimsMapping.m_nWidth = 0;
	m_dimsMapping.m_nHeight = 0;
	m_dimsMapping.m_nDepth = 1;
	m_dimsActual.m_nWidth = 0;
	m_dimsActual.m_nHeight = 0;
	m_dimsActual.m_nDepth = 1;
	m_dimsAllocated.m_nWidth = 0;
	m_dimsAllocated.m_nHeight = 0;
	m_dimsAllocated.m_nDepth = 0;
	m_dimsAllocated.m_nMipCount = 0;
	m_nStreamingMips = 0;
	m_nRefCount = 0;
	m_nFlags = 0;
	m_nInternalFlags = 0;
	m_pTextureHandles = NULL;
	m_nFrameCount = 0;
	VectorClear( m_vecReflectivity );
	m_pTextureRegenerator = NULL;
	m_nOriginalRenderTargetType = NO_RENDER_TARGET;
	m_RenderTargetSizeMode = RT_SIZE_NO_CHANGE;
	m_nOriginalRTWidth = m_nOriginalRTHeight = 1;

	m_LowResImageWidth = 0;
	m_LowResImageHeight = 0;
	m_pLowResImage = NULL;

	m_pStreamingJob = NULL;
	m_residenceTarget = RESIDENT_NONE;
	m_residenceCurrent = RESIDENT_NONE;
	m_lodClamp = 0;	
	m_lodBiasInitial = 0;
	m_lodBiasCurrent = 0;

	m_nDesiredDimensionLimit = 0;
	m_nActualDimensionLimit = 0;

	memset( &m_cachedFileLodSettings, 0, sizeof( m_cachedFileLodSettings ) );

#ifdef _DEBUG
	m_pDebugName = NULL;
#endif

	m_pStreamingVTF = NULL;
	m_bStreamingFileReadFailed = false;
}

CTexture::~CTexture()
{
#ifdef _DEBUG
	if ( m_nRefCount != 0 )
	{
		Warning( "Reference Count(%d) != 0 in ~CTexture for texture \"%s\"\n", (int)m_nRefCount, m_Name.String() );
	}
#endif

	Shutdown();

#ifdef _DEBUG
	if ( m_pDebugName )
	{
		// delete[] m_pDebugName;
	}
#endif

	// Deliberately stomp our VTable so that we can detect cases where code tries to access freed materials.
	int *p = (int *)this;
	*p = 0xdeadbeef;
}


//-----------------------------------------------------------------------------
// Initializes the texture
//-----------------------------------------------------------------------------
void CTexture::Init( int w, int h, int d, ImageFormat fmt, int iFlags, int iFrameCount )
{
	Assert( iFrameCount > 0 );

	// This is necessary to prevent blowing away the allocated state,
	// which is necessary for the ReleaseTextureHandles call below to work.
	SetErrorTexture( false );

	// free and release previous data
	// cannot change to new intialization parameters yet
	FreeShaderAPITextures();
	ReleaseTextureHandles();

	// update to new initialization parameters
	// these are the *desired* new values
	m_dimsMapping.m_nWidth = w;
	m_dimsMapping.m_nHeight = h;
	m_dimsMapping.m_nDepth = d;
	m_ImageFormat = fmt;
	m_nFrameCount = iFrameCount;
	// We don't know the actual width and height until we get it ready to render
	m_dimsActual.m_nWidth = m_dimsActual.m_nHeight = 0;
	m_dimsActual.m_nDepth = 1;
	m_dimsActual.m_nMipCount = 0;

	m_dimsAllocated.m_nWidth = 0;
	m_dimsAllocated.m_nHeight = 0;
	m_dimsAllocated.m_nDepth = 0;
	m_dimsAllocated.m_nMipCount = 0;
	m_nStreamingMips = 0;

	// Clear the m_nFlags bit. If we don't, then m_nFrameCount may end up being 1, and
	//  TEXTUREFLAGS_DEPTHRENDERTARGET could be set.
	m_nFlags &= ~TEXTUREFLAGS_DEPTHRENDERTARGET;
	m_nFlags |= iFlags;

	CancelStreamingJob( false );
	m_residenceTarget = RESIDENT_NONE;
	m_residenceCurrent = RESIDENT_NONE;
	m_lodClamp = 0;
	m_lodBiasInitial = 0;
	m_lodBiasCurrent = 0;

	AllocateTextureHandles();
}


//-----------------------------------------------------------------------------
// Shuts down the texture
//-----------------------------------------------------------------------------
void CTexture::Shutdown()
{
	Assert( m_pStreamingVTF == NULL );
	
	// Clean up the low-res texture
	delete[] m_pLowResImage;
	m_pLowResImage = 0;

	// Clean up the resources data
	for ( DataChunk const *pDataChunk = m_arrDataChunks.Base(),
		  *pDataChunkEnd = pDataChunk + m_arrDataChunks.Count();
		  pDataChunk < pDataChunkEnd; ++pDataChunk )
	{
		pDataChunk->Deallocate();
	}
	m_arrDataChunks.RemoveAll();

	// Frees the texture regen class
	if ( m_pTextureRegenerator )
	{
		m_pTextureRegenerator->Release();
		m_pTextureRegenerator = NULL;
	}

	CancelStreamingJob( false );

	m_residenceTarget = RESIDENT_NONE;
	m_residenceCurrent = RESIDENT_NONE;
	m_lodClamp = 0;
	m_lodBiasInitial = 0;
	m_lodBiasCurrent = 0;

	// This deletes the textures
	FreeShaderAPITextures();
	ReleaseTextureHandles();
	NotifyUnloadedFile();
}

void CTexture::ReleaseMemory()
{
	FreeShaderAPITextures();
	NotifyUnloadedFile();
}

IVTFTexture *CTexture::GetScratchVTFTexture( )
{
	const bool cbThreadInMatQueue = ( MaterialSystem()->GetRenderThreadId() == ThreadGetCurrentId() ); cbThreadInMatQueue;
	Assert( cbThreadInMatQueue || ThreadInMainThread() );

	const int ti = GetThreadId();

	if ( !s_pVTFTexture[ ti ] )
		s_pVTFTexture[ ti ] = CreateVTFTexture();
	return s_pVTFTexture[ ti ];
}

void CTexture::ReleaseScratchVTFTexture( IVTFTexture* tex )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	const bool cbThreadInMatQueue = ( MaterialSystem()->GetRenderThreadId() == ThreadGetCurrentId() ); cbThreadInMatQueue;
	Assert( cbThreadInMatQueue || ThreadInMainThread() );
	Assert( m_pStreamingVTF == NULL || ThreadInMainThread() );	// Can only manipulate m_pStreamingVTF to release safely in main thread.

	if ( m_pStreamingVTF )
	{
		Assert( tex == m_pStreamingVTF );
		TextureManager()->ReleaseAsyncScratchVTF( m_pStreamingVTF );
		m_pStreamingVTF = NULL;
		return;
	}

	// Normal scratch main-thread vtf doesn't need to do anything.

}

//-----------------------------------------------------------------------------
//
// Various initialization methods
//
//-----------------------------------------------------------------------------
void CTexture::ApplyRenderTargetSizeMode( int &width, int &height, ImageFormat fmt )
{
	width = m_nOriginalRTWidth;
	height = m_nOriginalRTHeight;

	switch ( m_RenderTargetSizeMode )
	{
		case RT_SIZE_FULL_FRAME_BUFFER:
		{
			MaterialSystem()->GetRenderTargetFrameBufferDimensions( width, height );
			if( !HardwareConfig()->SupportsNonPow2Textures() )
			{
				width = FloorPow2( width + 1 );
				height = FloorPow2( height + 1 );
			}
		}
		break;

		case RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP:
		{
			MaterialSystem()->GetRenderTargetFrameBufferDimensions( width, height );
			if( !HardwareConfig()->SupportsNonPow2Textures() )
			{
				width = CeilPow2( width );
				height = CeilPow2( height );
			}
		}
		break;

		case RT_SIZE_PICMIP:
		{
			int fbWidth, fbHeight;
			MaterialSystem()->GetRenderTargetFrameBufferDimensions( fbWidth, fbHeight );
			int picmip = g_config.skipMipLevels;
			while( picmip > 0 )
			{
				width >>= 1;
				height >>= 1;
				picmip--;
			}

			while( width > fbWidth )
			{
				width >>= 1;
			}
			while( height > fbHeight )
			{
				height >>= 1;
			}
		}
		break;

		case RT_SIZE_DEFAULT:
		{
			// Assume that the input is pow2.
			Assert( ( width & ( width - 1 ) ) == 0 );
			Assert( ( height & ( height - 1 ) ) == 0 );
			int fbWidth, fbHeight;
			MaterialSystem()->GetRenderTargetFrameBufferDimensions( fbWidth, fbHeight );
			while( width > fbWidth )
			{
				width >>= 1;
			}
			while( height > fbHeight )
			{
				height >>= 1;
			}
		}
		break;

		case RT_SIZE_HDR:
		{
			MaterialSystem()->GetRenderTargetFrameBufferDimensions( width, height );
			width = width / 4;
			height = height / 4;
		}
		break;

		case RT_SIZE_OFFSCREEN:
		{
			int fbWidth, fbHeight;
			MaterialSystem()->GetRenderTargetFrameBufferDimensions( fbWidth, fbHeight );

			// Shrink the buffer if it's bigger than back buffer.  Otherwise, don't mess with it.
			while( (width > fbWidth) || (height > fbHeight) )
			{
				width >>= 1;
				height >>= 1;
			}
		}
		break;

		case RT_SIZE_LITERAL:
		{
			// Literal means literally don't mess with the dimensions. Unlike what OFFSCREEN does,
			// which is totally to mess with the dimensions.
		}
		break;

		case RT_SIZE_LITERAL_PICMIP:
		{
			// Don't do anything here, like literal. Later, we will pay attention to picmip settings s.t. 
			// these render targets look like other textures wrt Mapping Dimensions vs Actual Dimensions.
		}
		break;

		case RT_SIZE_REPLAY_SCREENSHOT:
			{
				// Compute all possible resolutions if first time we're running this function
				static bool			bReplayInit = false;
				static int			m_aScreenshotWidths[ 3 ][ 2 ];
				static ConVarRef	replay_screenshotresolution( "replay_screenshotresolution" );

				if ( !bReplayInit )
				{
					bReplayInit = true;
					for ( int iAspect = 0; iAspect < 3; ++iAspect )
					{
						for ( int iRes = 0; iRes < 2; ++iRes )
						{
							int nWidth = (int)FastPow2( 9 + iRes );
							m_aScreenshotWidths[ iAspect ][ iRes ] = nWidth;
						}
					}
				}

				// Get dimensions for unpadded image
				int nUnpaddedWidth, nUnpaddedHeight;

				// Figure out the proper screenshot size to use based on the aspect ratio
				int nScreenWidth, nScreenHeight;
				MaterialSystem()->GetRenderTargetFrameBufferDimensions( nScreenWidth, nScreenHeight );
				float flAspectRatio = (float)nScreenWidth / nScreenHeight;

				// Get the screenshot res
				int iRes = clamp( replay_screenshotresolution.GetInt(), 0, 1 );

				int iAspect;
				if ( flAspectRatio == 16.0f/9 )
				{
					iAspect = 0;
				}
				else if ( flAspectRatio == 16.0f/10 )
				{
					iAspect = 1;
				}
				else
				{
					iAspect = 2;	// 4:3
				}

				static float s_flInvAspectRatios[3] = { 9.0f/16.0f, 10.0f/16, 3.0f/4 };
				nUnpaddedWidth = min( nScreenWidth, m_aScreenshotWidths[ iAspect ][ iRes ] );
				nUnpaddedHeight = m_aScreenshotWidths[ iAspect ][ iRes ] * s_flInvAspectRatios[ iAspect ];

				// Get dimensions for padded image based on unpadded size - must be power of 2 for a material/texture
				width = SmallestPowerOfTwoGreaterOrEqual( nUnpaddedWidth );
				height = SmallestPowerOfTwoGreaterOrEqual( nUnpaddedHeight ); 
			}
			break;

		default:
		{
			if ( !HushAsserts() )
			{
				Assert( m_RenderTargetSizeMode == RT_SIZE_NO_CHANGE );
				Assert( m_nOriginalRenderTargetType == RENDER_TARGET_NO_DEPTH );	// Only can use NO_CHANGE if we don't have a depth buffer.
			}
		}
		break;
	}
}

void CTexture::CopyToStagingTexture( ITexture* pDstTex )
{
	Assert( pDstTex );

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	// Need to flush any commands in flight on our side of things
	materials->Flush( false );

	CTexture* pDstTexActual = assert_cast< CTexture* >( pDstTex );

	// Then do the copy if everything is on the up and up.
	if ( ( m_pTextureHandles == NULL || m_nFrameCount == 0 ) || ( pDstTexActual->m_pTextureHandles == NULL || pDstTexActual->m_nFrameCount == 0 ) ) 
	{
		Assert( !"Can't copy to a non-existent texture, may need to generate or something." );
		return;
	}

	// Make sure we've actually got the right surface types.
	Assert( m_nFlags & TEXTUREFLAGS_RENDERTARGET );
	Assert( pDstTex->GetFlags() & TEXTUREFLAGS_STAGING_MEMORY );

	g_pShaderAPI->CopyRenderTargetToScratchTexture( m_pTextureHandles[0], pDstTexActual->m_pTextureHandles[0] );
}

void CTexture::Map( void** pOutBits, int* pOutPitch )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	// Must be a staging texture to avoid catastrophic perf fail.
	Assert( m_nFlags & TEXTUREFLAGS_STAGING_MEMORY );

	if ( m_pTextureHandles == NULL || m_nFrameCount == 0 )
	{
		Assert( !"Can't map a non-existent texture, may need to generate or something." );
		return;
	}

	g_pShaderAPI->LockRect( pOutBits, pOutPitch, m_pTextureHandles[ 0 ], 0, 0, 0, GetActualWidth(), GetActualHeight(), false, true );
}

void CTexture::Unmap()
{
	if ( m_pTextureHandles == NULL || m_nFrameCount == 0 )
	{
		Assert( !"Can't unmap a non-existent texture, may need to generate or something." );
		return;
	}

	g_pShaderAPI->UnlockRect( m_pTextureHandles[ 0 ], 0 );
}

bool CTexture::MakeResident( ResidencyType_t newResidence )
{
	Assert( ( GetFlags() & TEXTUREFLAGS_STREAMABLE ) != 0 );

	// If we already think we're supposed to go here, nothing to do and we should report success.
	if ( m_residenceTarget == newResidence )
		return true;

	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	// What are we moving towards?
	switch ( newResidence )
	{
	case RESIDENT_NONE:
		MakeNonResident();
		return true;

	case RESIDENT_PARTIAL:
		MakePartiallyResident();
		return true;

	case RESIDENT_FULL:
		return MakeFullyResident();

	default:
		Assert( !"Missing switch statement" );
	};

	return false;
}

void CTexture::UpdateLodBias()
{
	if ( m_lodBiasInitial == 0.0f )
		return;

	// Only perform adjustment once per frame.
	if ( m_lastLodBiasAdjustFrame == g_FrameNum )
		return;

	bool bPopIn = mat_lodin_time.GetFloat() == 0;

	if ( bPopIn && m_lodBiasInitial == 0.0f )
		return;

	if ( !bPopIn )
		m_lodBiasCurrent = m_lodBiasInitial - ( Plat_FloatTime() - m_lodBiasStartTime ) / mat_lodin_time.GetFloat() * m_lodBiasInitial;
	else
		m_lodBiasCurrent = m_lodBiasInitial = 0.0f;

	// If we're supposed to pop in when the object isn't visible and we have the opportunity...
	if ( mat_lodin_hidden_pop.GetBool() && m_lastLodBiasAdjustFrame != g_FrameNum - 1 )
		m_lodBiasCurrent = m_lodBiasInitial = 0.0f;

	if ( m_lodBiasCurrent <= 0.0f )
	{
		m_lodBiasCurrent = m_lodBiasInitial = 0.0f;
		m_lodBiasStartTime = 0;
	}

	m_lastLodBiasAdjustFrame = g_FrameNum;
	SetFilteringAndClampingMode( true );
}

void CTexture::MakeNonResident()
{
	if ( m_residenceCurrent != RESIDENT_NONE )
		Shutdown();

	m_residenceCurrent = m_residenceTarget = RESIDENT_NONE;

	// Clear our the streamable fine flag to ensure we reload properly.
	m_nFlags &= ~TEXTUREFLAGS_STREAMABLE_FINE;
}

void CTexture::MakePartiallyResident()
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	ResidencyType_t oldCurrentResidence = m_residenceCurrent;
	ResidencyType_t oldTargetResidence = m_residenceTarget;

	m_residenceCurrent = m_residenceTarget = RESIDENT_PARTIAL;

	if ( oldCurrentResidence == RESIDENT_PARTIAL )
	{
		Assert( oldTargetResidence == RESIDENT_FULL ); oldTargetResidence;
		// If we are already partially resident, then just cancel our job to stream in, 
		// cause we don't need that data anymore.
		CancelStreamingJob();

		return;
	}

	Assert( oldCurrentResidence == RESIDENT_FULL );

	// Clear the fine bit.
	m_nFlags &= ~TEXTUREFLAGS_STREAMABLE_FINE;

	if ( HardwareConfig()->CanStretchRectFromTextures() )
	{
		m_lodClamp = 0;
		m_lodBiasInitial = m_lodBiasCurrent = 0;
		m_lastLodBiasAdjustFrame = g_FrameNum;
		DownloadTexture( NULL, true );
	}
	else
	{
		// Oops. We were overzealous above--restore the residency to what it was.
		m_residenceCurrent = oldCurrentResidence;

		// Immediately display it as lower res (for consistency) but if we can't (efficiently)
		// copy we just have to re-read everything from disk. Lame!
		m_lodClamp = 3;
		m_lodBiasInitial = m_lodBiasCurrent = 0;
		m_lastLodBiasAdjustFrame = g_FrameNum;
		SetFilteringAndClampingMode( true );

		SafeAssign( &m_pStreamingJob, new CTextureStreamingJob( this ) );
		MaterialSystem()->AsyncFindTexture( GetName(), GetTextureGroupName(), m_pStreamingJob, (void*) RESIDENT_PARTIAL, false, TEXTUREFLAGS_STREAMABLE_COARSE );
	}
}

bool CTexture::MakeFullyResident()
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	ResidencyType_t oldCurrentResidence = m_residenceCurrent;
	ResidencyType_t oldTargetResidence = m_residenceTarget;

	if ( oldCurrentResidence == RESIDENT_FULL )
	{
		// This isn't a requirement, but right now it would be a mistake 
		Assert( !HardwareConfig()->CanStretchRectFromTextures() );
		Assert( oldTargetResidence == RESIDENT_PARTIAL ); oldTargetResidence;
		
		m_residenceCurrent = m_residenceTarget = RESIDENT_FULL;
		m_lodClamp = 0;
		m_lodBiasInitial = m_lodBiasCurrent = 0;
		m_lastLodBiasAdjustFrame = g_FrameNum;
		SetFilteringAndClampingMode( true );

		CancelStreamingJob();
		return true;
	}

	Assert( m_residenceTarget == RESIDENT_PARTIAL && m_residenceCurrent == RESIDENT_PARTIAL );
	Assert( m_pStreamingJob == NULL );

	SafeAssign( &m_pStreamingJob, new CTextureStreamingJob( this ) );
	MaterialSystem()->AsyncFindTexture( GetName(), GetTextureGroupName(), m_pStreamingJob, (void*) RESIDENT_FULL, false, TEXTUREFLAGS_STREAMABLE_FINE );

	m_residenceTarget = RESIDENT_FULL;

	return true;
}

void CTexture::CancelStreamingJob( bool bJobMustExist )
{
	bJobMustExist; // Only used by asserts ensuring correctness, so reference it for release builds. 

	// Most callers should be aware of whether the job exists, but for cleanup we don't know and we 
	// should be safe in that case.
	Assert( !bJobMustExist || m_pStreamingJob ); 
	if ( !m_pStreamingJob )
		return;

	// The streaming job and this (this texture) have a circular reference count--each one holds one for the other.
	// As a result, this means that having the streaming job forget about the texture may cause the texture to go
	// away completely! So we need to ensure that after we call "ForgetOwner" that we don't touch any instance 
	// variables.
	CTextureStreamingJob* pJob = m_pStreamingJob;
	m_pStreamingJob = NULL;

	pJob->ForgetOwner( this );
	SafeRelease( &pJob );
}

void CTexture::OnStreamingJobComplete( ResidencyType_t newResidenceCurrent )
{
	Assert( m_pStreamingJob );

	// It's probable that if this assert fires, we should just do nothing in here and return--but I'd 
	// like to see that happen to be sure. 
	Assert( newResidenceCurrent == m_residenceTarget );

	m_residenceCurrent = newResidenceCurrent;

	// Only do lod biasing for stream in. For stream out, just dump to lowest quality right away.
	if ( m_residenceCurrent == RESIDENT_FULL )
	{
		if ( mat_lodin_time.GetFloat() > 0 )
		{
			m_lodBiasCurrent = m_lodBiasInitial = 1.0 * m_nStreamingMips;
			m_lodBiasStartTime = Plat_FloatTime();
		}
		else
			m_lodBiasCurrent = m_lodBiasInitial = 0.0f;

		m_lastLodBiasAdjustFrame = g_FrameNum;
	}
	
	m_lodClamp = 0;
	m_nStreamingMips = 0;

	SetFilteringAndClampingMode( true );

	// The job is complete, Cancel handles cleanup correctly.
	CancelStreamingJob();
}

void CTexture::SetErrorTexture( bool bIsErrorTexture )
{
	if ( bIsErrorTexture )
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_ERROR;
	else
		m_nInternalFlags &= ( ~TEXTUREFLAGSINTERNAL_ERROR );
}




//-----------------------------------------------------------------------------
// Creates named render target texture
//-----------------------------------------------------------------------------
void CTexture::InitRenderTarget( 
	const char *pRTName, 
	int w, 
	int h, 
	RenderTargetSizeMode_t sizeMode, 
	ImageFormat fmt, 
	RenderTargetType_t type, 
	unsigned int textureFlags,
	unsigned int renderTargetFlags )
{
	if ( pRTName )
	{
		SetName( pRTName );
	}
	else
	{
		static int id = 0;
		char pName[128];
		Q_snprintf( pName, sizeof( pName ), "__render_target_%d", id );
		++id;
		SetName( pName );
	}

	if ( renderTargetFlags & CREATERENDERTARGETFLAGS_HDR )
	{
		if ( HardwareConfig()->GetHDRType() == HDR_TYPE_FLOAT )
		{
			// slam the format
			fmt = IMAGE_FORMAT_RGBA16161616F;
		}
	}

	int nFrameCount = 1;

	int nFlags = TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_RENDERTARGET;
	nFlags |= textureFlags;

	if ( type == RENDER_TARGET_NO_DEPTH )
	{
		nFlags |= TEXTUREFLAGS_NODEPTHBUFFER;
	}
	else if ( type == RENDER_TARGET_WITH_DEPTH || type == RENDER_TARGET_ONLY_DEPTH || g_pShaderAPI->DoRenderTargetsNeedSeparateDepthBuffer() )
	{
		nFlags |= TEXTUREFLAGS_DEPTHRENDERTARGET;
		++nFrameCount;
	}

	if ( renderTargetFlags & CREATERENDERTARGETFLAGS_TEMP )
	{
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_TEMPRENDERTARGET;
	}

	m_nOriginalRenderTargetType = type;
	m_RenderTargetSizeMode = sizeMode;
	m_nOriginalRTWidth = w;
	m_nOriginalRTHeight = h;

	if ( ImageLoader::ImageFormatInfo(fmt).m_NumAlphaBits > 1 )
	{
		nFlags  |= TEXTUREFLAGS_EIGHTBITALPHA;
	}
	else if ( ImageLoader::ImageFormatInfo(fmt).m_NumAlphaBits == 1 )
	{
		nFlags  |= TEXTUREFLAGS_ONEBITALPHA;
	}

	ApplyRenderTargetSizeMode( w, h, fmt );

	Init( w, h, 1, fmt, nFlags, nFrameCount );
	m_TextureGroupName = TEXTURE_GROUP_RENDER_TARGET;
}


void CTexture::OnRestore()
{ 
	// May have to change whether or not we have a depth buffer.
	// Are we a render target?
	if ( m_nFlags & TEXTUREFLAGS_RENDERTARGET )
	{
		// Did they not ask for a depth buffer?
		if ( m_nOriginalRenderTargetType == RENDER_TARGET )
		{
			// But, did we force them to have one, or should we force them to have one this time around?
			bool bShouldForce = g_pShaderAPI->DoRenderTargetsNeedSeparateDepthBuffer();
			bool bDidForce = ((m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET) != 0);
			if ( bShouldForce != bDidForce )
			{
				int nFlags = m_nFlags;
				int iFrameCount = m_nFrameCount;
				if ( bShouldForce )
				{
					Assert( !( nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET ) );
					iFrameCount = 2;
					nFlags |= TEXTUREFLAGS_DEPTHRENDERTARGET;
				}
				else
				{
					Assert( ( nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET ) );
					iFrameCount = 1;
					nFlags &= ~TEXTUREFLAGS_DEPTHRENDERTARGET;
				}

				Shutdown();
				
				int newWidth, newHeight;
				ApplyRenderTargetSizeMode( newWidth, newHeight, m_ImageFormat );
				
				Init( newWidth, newHeight, 1, m_ImageFormat, nFlags, iFrameCount );
				return;
			}
		}

		// If we didn't recreate it up above, then we may need to resize it anyway if the framebuffer
		// got smaller than we are.
		int newWidth, newHeight;
		ApplyRenderTargetSizeMode( newWidth, newHeight, m_ImageFormat );
		if ( newWidth != m_dimsMapping.m_nWidth || newHeight != m_dimsMapping.m_nHeight )
		{
			Shutdown();
			Init( newWidth, newHeight, 1, m_ImageFormat, m_nFlags, m_nFrameCount );
			return;
		}
	}
	else
	{
		if ( m_nFlags & TEXTUREFLAGS_STREAMABLE_FINE )
		{
			MakeResident( RESIDENT_NONE );
		}
	}
}


//-----------------------------------------------------------------------------
// Creates a procedural texture
//-----------------------------------------------------------------------------
void CTexture::InitProceduralTexture( const char *pTextureName, const char *pTextureGroupName, int w, int h, int d, ImageFormat fmt, int nFlags, ITextureRegenerator* generator )
{
	// We shouldn't be asking for render targets here
	Assert( (nFlags & (TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_DEPTHRENDERTARGET)) == 0 );

	SetName( pTextureName );

	// Eliminate flags that are inappropriate...
	nFlags &= ~TEXTUREFLAGS_HINT_DXT5 | TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA | 
		TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_DEPTHRENDERTARGET;

	// Insert required flags
	nFlags |= TEXTUREFLAGS_PROCEDURAL;
	int nAlphaBits = ImageLoader::ImageFormatInfo(fmt).m_NumAlphaBits;
	if (nAlphaBits > 1)
	{
		nFlags |= TEXTUREFLAGS_EIGHTBITALPHA;
	}
	else if (nAlphaBits == 1)
	{
		nFlags |= TEXTUREFLAGS_ONEBITALPHA;
	}
		
	// Procedural textures are always one frame only
	Init( w, h, d, fmt, nFlags, 1 );

	SetTextureRegenerator(generator);

	m_TextureGroupName = pTextureGroupName;
}


//-----------------------------------------------------------------------------
// Creates a file texture
//-----------------------------------------------------------------------------
void CTexture::InitFileTexture( const char *pTextureFile, const char *pTextureGroupName )
{
	// For files, we only really know about the file name
	// At any time, the file contents could change, and we could have
	// a different size, number of frames, etc.
	SetName( pTextureFile );
	m_TextureGroupName = pTextureGroupName;
}

//-----------------------------------------------------------------------------
// Assigns/releases texture IDs for our animation frames
//-----------------------------------------------------------------------------
void CTexture::AllocateTextureHandles()
{
	Assert( !m_pTextureHandles );
	Assert( m_nFrameCount > 0 );
#ifdef DBGFLAG_ASSERT
	if( m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET )
	{
		Assert( m_nFrameCount >= 2 );
	}
#endif

	m_pTextureHandles = new ShaderAPITextureHandle_t[m_nFrameCount];
	for( int i = 0; i != m_nFrameCount; ++i )
		m_pTextureHandles[i] = INVALID_SHADERAPI_TEXTURE_HANDLE;
}

void CTexture::ReleaseTextureHandles()
{
	if ( m_pTextureHandles )
	{
		delete[] m_pTextureHandles;
		m_pTextureHandles = NULL;
	}
}


//-----------------------------------------------------------------------------
// Creates the texture
//-----------------------------------------------------------------------------
bool CTexture::AllocateShaderAPITextures()
{
	Assert( !HasBeenAllocated() );
	
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	int nCount = m_nFrameCount;

	int nCreateFlags = 0;
	if ( ( m_nFlags & TEXTUREFLAGS_ENVMAP ) && HardwareConfig()->SupportsCubeMaps() )
	{
		nCreateFlags |= TEXTURE_CREATE_CUBEMAP;
	}
	
	bool bIsFloat = ( m_ImageFormat == IMAGE_FORMAT_RGBA16161616F ) || ( m_ImageFormat == IMAGE_FORMAT_R32F ) || 
					( m_ImageFormat == IMAGE_FORMAT_RGB323232F ) || ( m_ImageFormat == IMAGE_FORMAT_RGBA32323232F );
	
	// Don't do sRGB on floating point textures
	if ( ( m_nFlags & TEXTUREFLAGS_SRGB ) && !bIsFloat )
	{
		nCreateFlags |= TEXTURE_CREATE_SRGB;	// for Posix/GL only
	}

	if ( m_nFlags & TEXTUREFLAGS_RENDERTARGET )
	{
		nCreateFlags |= TEXTURE_CREATE_RENDERTARGET;

		// This here is simply so we can use a different call to
		// create the depth texture below	
		if ( ( m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET ) && ( nCount == 2 ) ) //nCount must be 2 on pc
		{
			--nCount;
		}
	}
	else
	{
		// If it's not a render target, use the texture manager in dx
		if ( m_nFlags & TEXTUREFLAGS_STAGING_MEMORY )
			nCreateFlags |= TEXTURE_CREATE_SYSMEM;
		else
		{
#if defined(IS_WINDOWS_PC)
			static ConVarRef mat_dxlevel("mat_dxlevel");
			if ( mat_dxlevel.GetInt() < 90 || mat_managedtextures.GetBool() )
#endif
			{
				nCreateFlags |= TEXTURE_CREATE_MANAGED;
			}
		}
	}

	if ( m_nFlags & TEXTUREFLAGS_POINTSAMPLE )
	{
		nCreateFlags |= TEXTURE_CREATE_UNFILTERABLE_OK;
	}

	if ( m_nFlags & TEXTUREFLAGS_VERTEXTEXTURE )
	{
		nCreateFlags |= TEXTURE_CREATE_VERTEXTEXTURE;
	}

	int nCopies = 1;
	if ( IsProcedural() )
	{
		// This is sort of hacky... should we store the # of copies in the VTF?
		if ( !( m_nFlags & TEXTUREFLAGS_SINGLECOPY ) )
		{
			// FIXME: That 6 there is heuristically what I came up with what I
			// need to get eyes not to stall on map alyx3. We need a better way
			// of determining how many copies of the texture we should store.
			nCopies = 6;
		}
	}

	// For depth only render target: adjust texture width/height
	// Currently we just leave it the same size, will update with further testing
	int nShaderApiCreateTextureDepth = ( ( m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET ) && ( m_nOriginalRenderTargetType == RENDER_TARGET_ONLY_DEPTH ) ) ? 1 : m_dimsAllocated.m_nDepth;

	// Create all animated texture frames in a single call
	g_pShaderAPI->CreateTextures(
		m_pTextureHandles, nCount,
		m_dimsAllocated.m_nWidth, m_dimsAllocated.m_nHeight, nShaderApiCreateTextureDepth, m_ImageFormat, m_dimsAllocated.m_nMipCount,
		nCopies, nCreateFlags, GetName(), GetTextureGroupName() );

	int accountingCount = nCount;

	// Create the depth render target buffer
	if ( m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET )
	{
		MEM_ALLOC_CREDIT();
		Assert( nCount == 1 );

		char debugName[128];
		Q_snprintf( debugName, ARRAYSIZE( debugName ), "%s_ZBuffer", GetName() );
		Assert( m_nFrameCount >= 2 );
		m_pTextureHandles[1] = g_pShaderAPI->CreateDepthTexture( 
				m_ImageFormat, 
				m_dimsAllocated.m_nWidth, 
				m_dimsAllocated.m_nHeight,
				debugName,
				( m_nOriginalRenderTargetType == RENDER_TARGET_ONLY_DEPTH ) );
		accountingCount += 1;
	}

	STAGING_ONLY_EXEC( g_currentTextures.InsertOrReplace( this, TexInfo_t( GetName(), m_dimsAllocated.m_nWidth, m_dimsAllocated.m_nHeight, m_dimsAllocated.m_nDepth, m_dimsAllocated.m_nMipCount, accountingCount, nCopies, m_ImageFormat ) ) );

	m_nInternalFlags |= TEXTUREFLAGSINTERNAL_ALLOCATED;

	return true;
}

//-----------------------------------------------------------------------------
// Releases the texture's hardware memory
//-----------------------------------------------------------------------------
void CTexture::FreeShaderAPITextures()
{
	if ( m_pTextureHandles && HasBeenAllocated() )
	{
		#ifdef STAGING_ONLY
			// If this hits, there's a leak because we're not deallocating enough textures. Yikes!
			Assert( g_currentTextures[ g_currentTextures.Find( this ) ].m_nFrameCount == m_nFrameCount );
			// Remove ourselves from the list.
			g_currentTextures.Remove( this );
		#endif

		// Release the frames
		for ( int i = m_nFrameCount; --i >= 0; )
		{
			if ( g_pShaderAPI->IsTexture( m_pTextureHandles[i] ) )
			{
#ifdef WIN32
				Assert( _heapchk() == _HEAPOK );
#endif

				g_pShaderAPI->DeleteTexture( m_pTextureHandles[i] );
				m_pTextureHandles[i] = INVALID_SHADERAPI_TEXTURE_HANDLE;
			}
		}
	}
	m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_ALLOCATED;

	// Clear texture streaming stuff, too. 
	if ( ( m_nFlags & TEXTUREFLAGS_STREAMABLE ) != 0 )
	{
		m_nFlags &= ~TEXTUREFLAGS_STREAMABLE_FINE;
		m_residenceCurrent = m_residenceTarget = RESIDENT_NONE;
		m_lodClamp = 0;
		m_lodBiasCurrent = m_lodBiasInitial = 0;
		m_lodBiasStartTime = 0;
	}
}

void CTexture::MigrateShaderAPITextures()
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	const int cBytes = m_nFrameCount * sizeof ( ShaderAPITextureHandle_t );

	ShaderAPITextureHandle_t *pTextureHandles =	( ShaderAPITextureHandle_t * ) stackalloc( cBytes );

	Assert( pTextureHandles );
	if ( !pTextureHandles )
		return;

	V_memcpy( pTextureHandles, m_pTextureHandles, cBytes );

	// Pretend we haven't been allocated yet.
	m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_ALLOCATED;

	AllocateShaderAPITextures();

	for ( int i = 0; i < m_nFrameCount; ++i ) 
	{
		Assert( g_pShaderAPI->IsTexture( pTextureHandles[ i ] ) == g_pShaderAPI->IsTexture( m_pTextureHandles[ i ] ) );
		if ( !g_pShaderAPI->IsTexture( pTextureHandles[ i ] ) )
			continue;

		g_pShaderAPI->CopyTextureToTexture( pTextureHandles[ i ], m_pTextureHandles[ i ] );
	}

	for ( int i = 0; i < m_nFrameCount; ++i )
	{
		if ( !g_pShaderAPI->IsTexture( pTextureHandles[ i ] ) )
			continue;

		g_pShaderAPI->DeleteTexture( pTextureHandles[ i ] );
	}
}

//-----------------------------------------------------------------------------
// Computes the actual format of the texture
//-----------------------------------------------------------------------------
ImageFormat CTexture::ComputeActualFormat( ImageFormat srcFormat )
{
	ImageFormat dstFormat;
	bool bIsCompressed = ImageLoader::IsCompressed( srcFormat );
	if ( g_config.bCompressedTextures && HardwareConfig()->SupportsCompressedTextures() && bIsCompressed )
	{
		// for the runtime compressed formats the srcFormat won't equal the dstFormat, and we need to return srcFormat here
		if ( ImageLoader::IsRuntimeCompressed( srcFormat ) )
		{
			return srcFormat;
		}

		// don't do anything since we are already in a compressed format.
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( srcFormat );
		Assert( dstFormat == srcFormat );
		return dstFormat;
	}

	// NOTE: Below this piece of code is only called when compressed textures are
	// turned off, or if the source texture is not compressed.

#ifdef DX_TO_GL_ABSTRACTION
	if ( ( srcFormat == IMAGE_FORMAT_UVWQ8888 ) || ( srcFormat == IMAGE_FORMAT_UV88 ) || ( srcFormat == IMAGE_FORMAT_UVLX8888 )  )
	{
		// Danger, this is going to blow up on the Mac.  You better know what you're
		// doing with these exotic formats...which were introduced in 1999
		Assert( 0 );
	}
#endif

	// We use the TEXTUREFLAGS_EIGHTBITALPHA and TEXTUREFLAGS_ONEBITALPHA flags
	// to decide how many bits of alpha we need; vtex checks the alpha channel
	// for all white, etc.
	if( ( srcFormat == IMAGE_FORMAT_UVWQ8888 ) || ( srcFormat == IMAGE_FORMAT_UV88 ) || 
		( srcFormat == IMAGE_FORMAT_UVLX8888 ) || ( srcFormat == IMAGE_FORMAT_RGBA16161616 ) ||
		( srcFormat == IMAGE_FORMAT_RGBA16161616F ) )
	{
#ifdef DX_TO_GL_ABSTRACTION		
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( srcFormat, false );  // Stupid HACK!
#else
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( srcFormat, true );  // Stupid HACK!
#endif
	} 
	else if ( m_nFlags & ( TEXTUREFLAGS_EIGHTBITALPHA | TEXTUREFLAGS_ONEBITALPHA ) )
	{
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( IMAGE_FORMAT_BGRA8888 );
	}
	else if ( srcFormat == IMAGE_FORMAT_I8 )
	{
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( IMAGE_FORMAT_I8 );
	}
	else
	{
		dstFormat = g_pShaderAPI->GetNearestSupportedFormat( IMAGE_FORMAT_BGR888 );
	}
	return dstFormat;
}

//-----------------------------------------------------------------------------
// Calculates info about whether we can make the texture smaller and by how much
//-----------------------------------------------------------------------------
int CTexture::ComputeActualSize( bool bIgnorePicmip, IVTFTexture *pVTFTexture, bool bTextureMigration )
{
	unsigned int stripFlags = 0;
	return ComputeMipSkipCount( GetName(), m_dimsMapping, bIgnorePicmip, pVTFTexture, m_nFlags, m_nDesiredDimensionLimit, &m_nStreamingMips, &m_cachedFileLodSettings, &m_dimsActual, &m_dimsAllocated, &stripFlags );
	Assert( stripFlags == 0 ); // Not necessarily illegal, just needs investigating. 
}


//-----------------------------------------------------------------------------
// Used to modify the texture bits (procedural textures only)
//-----------------------------------------------------------------------------
void CTexture::SetTextureRegenerator( ITextureRegenerator *pTextureRegen )
{
	// NOTE: These can only be used by procedural textures
	Assert( IsProcedural() );

	if (m_pTextureRegenerator)
	{
		m_pTextureRegenerator->Release();
	}
	m_pTextureRegenerator = pTextureRegen; 
}


//-----------------------------------------------------------------------------
// Gets us modifying a particular frame of our texture
//-----------------------------------------------------------------------------
void CTexture::Modify( int iFrame )
{
	Assert( iFrame >= 0 && iFrame < m_nFrameCount );
	Assert( HasBeenAllocated() );
	g_pShaderAPI->ModifyTexture( m_pTextureHandles[iFrame] );
}


//-----------------------------------------------------------------------------
// Sets the texture clamping state on the currently modified frame
//-----------------------------------------------------------------------------
void CTexture::SetWrapState( )
{
	// Border clamp applies to all texture coordinates
	if ( m_nFlags & TEXTUREFLAGS_BORDER )
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_S, SHADER_TEXWRAPMODE_BORDER );
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_T, SHADER_TEXWRAPMODE_BORDER );
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_U, SHADER_TEXWRAPMODE_BORDER );
		return;
	}

	// Clamp mode in S
	if ( m_nFlags & TEXTUREFLAGS_CLAMPS )
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_S, SHADER_TEXWRAPMODE_CLAMP );
	}
	else
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_S, SHADER_TEXWRAPMODE_REPEAT );
	}

	// Clamp mode in T
	if ( m_nFlags & TEXTUREFLAGS_CLAMPT )
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_T, SHADER_TEXWRAPMODE_CLAMP );
	}
	else
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_T, SHADER_TEXWRAPMODE_REPEAT );
	}

	// Clamp mode in U
	if ( m_nFlags & TEXTUREFLAGS_CLAMPU )
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_U, SHADER_TEXWRAPMODE_CLAMP );
	}
	else
	{
		g_pShaderAPI->TexWrap( SHADER_TEXCOORD_U, SHADER_TEXWRAPMODE_REPEAT );
	}
}


//-----------------------------------------------------------------------------
// Sets the texture filtering state on the currently modified frame
//-----------------------------------------------------------------------------
void CTexture::SetFilterState()
{
	// Turns off filtering when we're point sampling
	if( m_nFlags & TEXTUREFLAGS_POINTSAMPLE )
	{
		g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_NEAREST );
		g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_NEAREST );
		return;
	}

	// NOTE: config.bMipMapTextures and config.bFilterTextures is handled in ShaderAPIDX8
	bool bEnableMipmapping = ( m_nFlags & TEXTUREFLAGS_NOMIP ) ? false : true;
	if( !bEnableMipmapping )
	{
		g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
		g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
		return;
	}

	// Determing the filtering mode
	bool bIsAnisotropic = false, bIsTrilinear = false;
	if ( HardwareConfig()->GetDXSupportLevel() >= 80 && (g_config.m_nForceAnisotropicLevel > 1) && 
		(HardwareConfig()->MaximumAnisotropicLevel() > 1) )
	{
		bIsAnisotropic = true;
	}
	else if ( g_config.ForceTrilinear() )
	{
		bIsAnisotropic = (( m_nFlags & TEXTUREFLAGS_ANISOTROPIC ) != 0) && (HardwareConfig()->MaximumAnisotropicLevel() > 1);
		bIsTrilinear = true;
	}
	else
	{
		bIsAnisotropic = (( m_nFlags & TEXTUREFLAGS_ANISOTROPIC ) != 0) && (HardwareConfig()->MaximumAnisotropicLevel() > 1);
		bIsTrilinear = ( m_nFlags & TEXTUREFLAGS_TRILINEAR ) != 0;
	}

	if ( bIsAnisotropic )
	{		    
		g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_ANISOTROPIC );
		g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_ANISOTROPIC );
	}
	else
	{
		// force trilinear if we are on a dx8 card or above
		if ( bIsTrilinear )
		{
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR_MIPMAP_LINEAR );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
		}
		else
		{
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR_MIPMAP_NEAREST );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
		}
	}

	SetLodState();
}

//-----------------------------------------------------------------------------
// Sets the lod state on the currently modified frame
//-----------------------------------------------------------------------------
void CTexture::SetLodState()
{
	// Set the lod clamping value to ensure we don't see anything we're not supposed to.
	g_pShaderAPI->TexLodClamp( m_lodClamp );
	g_pShaderAPI->TexLodBias( m_lodBiasCurrent );
}

//-----------------------------------------------------------------------------
// Download bits main entry point!!
//-----------------------------------------------------------------------------
void CTexture::DownloadTexture( Rect_t *pRect, bool bCopyFromCurrent )
{
	// No downloading necessary if there's no graphics
	if ( !g_pShaderDevice->IsUsingGraphics() )
		return;

	// We don't know the actual size of the texture at this stage...
	if ( !pRect )
	{
		ReconstructTexture( bCopyFromCurrent );
	}
	else
	{
		// Not implemented yet.
		Assert( bCopyFromCurrent == false );
		ReconstructPartialTexture( pRect );
	}

	// Iterate over all the frames and set the appropriate wrapping + filtering state
	SetFilteringAndClampingMode();

	// texture bits have been updated, update the exclusion state
	if ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE )
	{
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_EXCLUDED;
	}
	else
	{
		m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_EXCLUDED;
	}

	// texture bits have been picmipped, update the picmip state
	m_nActualDimensionLimit = m_nDesiredDimensionLimit;
}

void CTexture::Download( Rect_t *pRect, int nAdditionalCreationFlags /* = 0 */ )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	// Only download the bits if we can...
	if ( g_pShaderAPI->CanDownloadTextures() )
	{
		MaterialLock_t hLock = MaterialSystem()->Lock();
		m_nFlags |= nAdditionalCreationFlags; // Path to let stdshaders drive settings like sRGB-ness at creation time
		DownloadTexture( pRect );
		MaterialSystem()->Unlock( hLock );
	}
}

// Save texture to a file.
bool CTexture::SaveToFile( const char *fileName )
{
	bool bRet = false;
	ITexture *pTexture = materials->FindTexture( "_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET );

	if ( !pTexture )
		return bRet;

	const int width = GetActualWidth();
	const int height = GetActualHeight();

	if ( pTexture->GetImageFormat() == IMAGE_FORMAT_RGBA8888 ||
		 pTexture->GetImageFormat() == IMAGE_FORMAT_ABGR8888 ||
		 pTexture->GetImageFormat() == IMAGE_FORMAT_ARGB8888 ||
		 pTexture->GetImageFormat() == IMAGE_FORMAT_BGRA8888 ||
		 pTexture->GetImageFormat() == IMAGE_FORMAT_BGRX8888 )
	{
		bool bCleanupTexture = false;

		// Need to allocate a temporarily renderable surface. Sadness.
		if ( width > pTexture->GetActualWidth() || height > pTexture->GetActualHeight() )
		{
			materials->OverrideRenderTargetAllocation( true );
			// This one bumps the ref automatically for us.
			pTexture = materials->CreateNamedRenderTargetTextureEx( "_rt_savetofile", width, height, RT_SIZE_LITERAL, IMAGE_FORMAT_BGRA8888, MATERIAL_RT_DEPTH_NONE, TEXTUREFLAGS_IMMEDIATE_CLEANUP );
			materials->OverrideRenderTargetAllocation( false );

			if ( !pTexture || pTexture->IsError() )
			{
				SafeRelease( &pTexture );
				Msg( "SaveToFile: texture '_rt_FullFrameFB1' failed. Ptr:%p Format:%d\n", pTexture, ( pTexture ? pTexture->GetImageFormat() : 0 ) );
				return false;
			}

			bCleanupTexture = true;
		}

		Rect_t SrcRect = { 0, 0, width, height };
		Rect_t DstRect = SrcRect;

		if ( ( width > 0 ) && ( height > 0 ) )
		{
			void *pixelValue = malloc( width * height * 2 * sizeof( BGRA8888_t ) );

			if( pixelValue )
			{
				CMatRenderContextPtr pRenderContext( MaterialSystem() );

				// Set the clear color to opaque black
				pRenderContext->ClearColor4ub( 0, 0, 0, 0xFF );
				pRenderContext->ClearBuffers( true, true, true );
				pRenderContext->PushRenderTargetAndViewport( pTexture, 0, 0, width, height );
				pRenderContext->CopyTextureToRenderTargetEx( 0, this, &SrcRect, &DstRect );

				pRenderContext->ReadPixels( 0, 0, width, height, ( unsigned char * )pixelValue, pTexture->GetImageFormat() );

				// Slap the alpha channel at the bottom of the tga file so we don't have to deal with crappy tools that can't
				//  handle rgb + alpha well. This means we can just do a "mat_texture_save_fonts" concommand, and then use
				//	something like Beyond Compare to look at the fonts differences between various platforms, etc.
				CPixelWriter pixelWriterSrc;
				CPixelWriter pixelWriterDst;
				pixelWriterSrc.SetPixelMemory( pTexture->GetImageFormat(), pixelValue, width * sizeof( BGRA8888_t ) );
				pixelWriterDst.SetPixelMemory( pTexture->GetImageFormat(), pixelValue, width * sizeof( BGRA8888_t ) );

				for (int y = 0; y < height; ++y)
				{
					pixelWriterSrc.Seek( 0, y );
					pixelWriterDst.Seek( 0, y + height );

					for (int x = 0; x < width; ++x)
					{
						int r, g, b, a;

						pixelWriterSrc.ReadPixelNoAdvance( r, g, b, a );
						pixelWriterSrc.WritePixel( a, a, a, 255 );
						pixelWriterDst.WritePixel( r, g, b, 255 );
					}
				}

				if ( TGAWriter::WriteTGAFile( fileName, width, height * 2, pTexture->GetImageFormat(), ( uint8 * )pixelValue, width * sizeof( BGRA8888_t ) ) )
				{
					bRet = true;
				}

				// restore our previous state
				pRenderContext->PopRenderTargetAndViewport();

				free( pixelValue );
			}
		}

		if ( bCleanupTexture )
			SafeRelease( &pTexture );
	}
	else
	{
		Msg( "SaveToFile: texture '_rt_FullFrameFB1' failed. Ptr:%p Format:%d\n", pTexture, ( pTexture ? pTexture->GetImageFormat() : 0 ) );
	}

	return bRet;
}

bool CTexture::AsyncReadTextureFromFile( IVTFTexture* pVTFTexture, unsigned int nAdditionalCreationFlags )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
	
	m_bStreamingFileReadFailed = false; // Optimism!

	char pCacheFileName[ MATERIAL_MAX_PATH ];
	FileHandle_t fileHandle = FILESYSTEM_INVALID_HANDLE;

	GetCacheFilename( pCacheFileName, MATERIAL_MAX_PATH );
	if ( !GetFileHandle( &fileHandle, pCacheFileName, NULL ) )
	{
		m_bStreamingFileReadFailed = true;
		return false;
	}

	if ( V_strstr( GetName(), "c_sniperrifle_scope" ) )
	{
		int i = 0;
		i = 3;
	}


	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - %s", __FUNCTION__, tmDynamicString( TELEMETRY_LEVEL0, pCacheFileName ) );
	
	// OSX hackery
	int nPreserveFlags = nAdditionalCreationFlags;
	if ( m_nFlags & TEXTUREFLAGS_SRGB )
		nPreserveFlags |= TEXTUREFLAGS_SRGB;

	uint16 dontCareStreamedMips = m_nStreamingMips;
	TextureLODControlSettings_t settings = m_cachedFileLodSettings;

	if ( !SLoadTextureBitsFromFile( &pVTFTexture, fileHandle, m_nFlags | nPreserveFlags, &settings, m_nDesiredDimensionLimit, &dontCareStreamedMips, GetName(), pCacheFileName, &m_dimsMapping ) )
	{
		g_pFullFileSystem->Close( fileHandle );
		m_bStreamingFileReadFailed = true;
		return false;
	}

	g_pFullFileSystem->Close( fileHandle );

	m_pStreamingVTF = pVTFTexture;

	return true;
}

void CTexture::AsyncCancelReadTexture( )
{
	Assert( m_bStreamingFileReadFailed || m_pStreamingVTF != NULL );
	if ( m_pStreamingVTF )
	{
		TextureManager()->ReleaseAsyncScratchVTF( m_pStreamingVTF );
		m_pStreamingVTF = NULL;
	}
}

void CTexture::Bind( Sampler_t sampler )
{
	Bind( sampler, 0 );
}

//-----------------------------------------------------------------------------
// Binds a particular texture (possibly paired)
//-----------------------------------------------------------------------------
void CTexture::Bind( Sampler_t sampler1, int nFrame, Sampler_t sampler2 /* = -1 */ )
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		TextureManager()->RequestAllMipmaps( this );

		if ( nFrame < 0 || nFrame >= m_nFrameCount )
		{
			// FIXME: Use the well-known 'error' id instead of frame 0
			nFrame = 0;
			//			Assert(0);
		}

		// Make sure we've actually allocated the texture handle
		if ( HasBeenAllocated() )
		{
			g_pShaderAPI->BindTexture( sampler1, m_pTextureHandles[nFrame] );
		}
		else
		{
			ExecuteNTimes( 20, Warning( "Trying to bind texture %s, but texture handles are not valid. Binding a white texture!\n", GetName() ) );
			g_pShaderAPI->BindStandardTexture( sampler1, TEXTURE_WHITE );
		}
	}
}



void CTexture::BindVertexTexture( VertexTextureSampler_t sampler, int nFrame )
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		if ( nFrame < 0 || nFrame >= m_nFrameCount )
		{
			// FIXME: Use the well-known 'error' id instead of frame 0
			nFrame = 0;
			//			Assert(0);
		}

		// Make sure we've actually allocated the texture
		Assert( HasBeenAllocated() );

		g_pShaderAPI->BindVertexTexture( sampler, m_pTextureHandles[nFrame] );
	}
}


//-----------------------------------------------------------------------------
// Set this texture as a render target
//-----------------------------------------------------------------------------
bool CTexture::SetRenderTarget( int nRenderTargetID )
{
	return SetRenderTarget( nRenderTargetID, NULL );
}

//-----------------------------------------------------------------------------
// Set this texture as a render target
// Optionally bind pDepthTexture as depth buffer
//-----------------------------------------------------------------------------
bool CTexture::SetRenderTarget( int nRenderTargetID, ITexture *pDepthTexture )
{
	if ( ( m_nFlags & TEXTUREFLAGS_RENDERTARGET ) == 0 )
		return false;

	// Make sure we've actually allocated the texture handles
	Assert( HasBeenAllocated() );

	ShaderAPITextureHandle_t textureHandle = m_pTextureHandles[0];

	ShaderAPITextureHandle_t depthTextureHandle = (unsigned int)SHADER_RENDERTARGET_DEPTHBUFFER;

	if ( m_nFlags & TEXTUREFLAGS_DEPTHRENDERTARGET )
	{
		Assert( m_nFrameCount >= 2 );
		depthTextureHandle = m_pTextureHandles[1];
	} 
	else if ( m_nFlags & TEXTUREFLAGS_NODEPTHBUFFER )
	{
		// GR - render target without depth buffer	
		depthTextureHandle = (unsigned int)SHADER_RENDERTARGET_NONE;
	}

	if ( pDepthTexture)
	{
		depthTextureHandle = static_cast<ITextureInternal *>(pDepthTexture)->GetTextureHandle(0);
	}

	g_pShaderAPI->SetRenderTargetEx( nRenderTargetID, textureHandle, depthTextureHandle );
	return true;
}


//-----------------------------------------------------------------------------
// Reference counting
//-----------------------------------------------------------------------------
void CTexture::IncrementReferenceCount( void )
{
	++m_nRefCount;
}

void CTexture::DecrementReferenceCount( void )
{
	if ( ( --m_nRefCount <= 0 ) && ( m_nFlags & TEXTUREFLAGS_IMMEDIATE_CLEANUP ) != 0 )
	{
		Assert( m_nRefCount == 0 );
		// Just inform the texture manager, it will decide to free us at a later date.
		TextureManager()->MarkUnreferencedTextureForCleanup( this );
	}
}

int CTexture::GetReferenceCount( )
{
	return m_nRefCount;
}


//-----------------------------------------------------------------------------
// Various accessor methods
//-----------------------------------------------------------------------------
const char* CTexture::GetName( ) const
{
	return m_Name.String();
}

const char* CTexture::GetTextureGroupName( ) const
{
	return m_TextureGroupName.String();
}

void CTexture::SetName( const char* pName )
{
	// normalize and convert to a symbol
	char szCleanName[MAX_PATH];
	m_Name = NormalizeTextureName( pName, szCleanName, sizeof( szCleanName ) );

#ifdef _DEBUG
	if ( m_pDebugName )
	{
		delete [] m_pDebugName;
	}
	int nLen = V_strlen( szCleanName ) + 1;
	m_pDebugName = new char[nLen];
	V_memcpy( m_pDebugName, szCleanName, nLen );
#endif
}

ImageFormat CTexture::GetImageFormat()	const
{
	return m_ImageFormat;
}

int CTexture::GetMappingWidth()	const
{
	return m_dimsMapping.m_nWidth;
}

int CTexture::GetMappingHeight() const
{
	return m_dimsMapping.m_nHeight;
}

int CTexture::GetMappingDepth() const
{
	return m_dimsMapping.m_nDepth;
}

int CTexture::GetActualWidth() const
{
	return m_dimsActual.m_nWidth;
}

int CTexture::GetActualHeight()	const
{
	return m_dimsActual.m_nHeight;
}

int CTexture::GetActualDepth()	const
{
	return m_dimsActual.m_nDepth;
}

int CTexture::GetNumAnimationFrames() const
{
	return m_nFrameCount;
}

void CTexture::GetReflectivity( Vector& reflectivity )
{
	Precache();
	VectorCopy( m_vecReflectivity, reflectivity );
}

//-----------------------------------------------------------------------------
// Little helper polling methods
//-----------------------------------------------------------------------------
bool CTexture::IsTranslucent() const
{
	return ( m_nFlags & (TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA) ) != 0;
}

bool CTexture::IsNormalMap( void ) const
{
	return ( ( m_nFlags & TEXTUREFLAGS_NORMAL ) != 0 );
}

bool CTexture::IsCubeMap( void ) const
{
	return ( ( m_nFlags & TEXTUREFLAGS_ENVMAP ) != 0 );
}

bool CTexture::IsRenderTarget( void ) const
{
	return ( ( m_nFlags & TEXTUREFLAGS_RENDERTARGET ) != 0 );
}

bool CTexture::IsTempRenderTarget( void ) const
{
	return ( ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_TEMPRENDERTARGET ) != 0 );
}

bool CTexture::IsProcedural() const
{
	return ( (m_nFlags & TEXTUREFLAGS_PROCEDURAL) != 0 );
}

bool CTexture::IsMipmapped() const
{
	return ( (m_nFlags & TEXTUREFLAGS_NOMIP) == 0 );
}

unsigned int CTexture::GetFlags() const
{
	return m_nFlags;
}

void CTexture::ForceLODOverride( int iNumLodsOverrideUpOrDown )
{
	TextureLodOverride::OverrideInfo oi( iNumLodsOverrideUpOrDown, iNumLodsOverrideUpOrDown );
	TextureLodOverride::Add( GetName(), oi );
	Download( NULL );
}


bool CTexture::IsError() const
{
	return ( (m_nInternalFlags & TEXTUREFLAGSINTERNAL_ERROR) != 0 );
}

bool CTexture::HasBeenAllocated() const
{
	return ( (m_nInternalFlags & TEXTUREFLAGSINTERNAL_ALLOCATED) != 0 );
}

bool CTexture::IsVolumeTexture() const
{
	return (m_dimsMapping.m_nDepth > 1);
}

//-----------------------------------------------------------------------------
// Sets the filtering + clamping modes on the texture
//-----------------------------------------------------------------------------
void CTexture::SetFilteringAndClampingMode( bool bOnlyLodValues )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	if( !HasBeenAllocated() )
		return;

	for ( int iFrame = 0; iFrame < m_nFrameCount; ++iFrame )
	{
		Modify( iFrame );			// Indicate we're changing state with respect to a particular frame
		if ( !bOnlyLodValues )
		{
			SetWrapState();			// Send the appropriate wrap/clamping modes to the shaderapi.
			SetFilterState();		// Set the filtering mode for the texture after downloading it.
									// NOTE: Apparently, the filter state cannot be set until after download
		}
		else
			SetLodState();
	}
}

//-----------------------------------------------------------------------------
// Loads up the non-fallback information about the texture 
//-----------------------------------------------------------------------------
void CTexture::Precache()
{
	// We only have to do something in the case of a file texture
	if ( IsRenderTarget() || IsProcedural() )
		return;

	if ( HasBeenAllocated() )
		return;

	// Blow off env_cubemap too...
	if ( !Q_strnicmp( m_Name.String(), "env_cubemap", 12 ))
		return;
	
	int nAdditionalFlags = 0;
	if ( ( m_nFlags & TEXTUREFLAGS_STREAMABLE ) != 0 ) 
	{
		// If we were previously streamed in, make sure we still do this time around.
		nAdditionalFlags = TEXTUREFLAGS_STREAMABLE_COARSE;
		Assert( ( m_nFlags & TEXTUREFLAGS_STREAMABLE_FINE ) == 0 );
		Assert( m_residenceCurrent == RESIDENT_NONE && m_residenceTarget == RESIDENT_NONE );
		Assert( m_lodClamp == 0 );
		Assert( m_lodBiasCurrent == 0 && m_lodBiasInitial == 0 );
		Assert( m_lodBiasStartTime == 0 );
	}

	ScratchVTF scratch( this );
	IVTFTexture *pVTFTexture = scratch.Get();

	// The texture name doubles as the relative file name
	// It's assumed to have already been set by this point	
	// Compute the cache name
	char pCacheFileName[MATERIAL_MAX_PATH];
	Q_snprintf( pCacheFileName, sizeof( pCacheFileName ), "materials/%s" TEXTURE_FNAME_EXTENSION, m_Name.String() );

	int nHeaderSize = VTFFileHeaderSize( VTF_MAJOR_VERSION );
	unsigned char *pMem = (unsigned char *)stackalloc( nHeaderSize );
	CUtlBuffer buf( pMem, nHeaderSize );
	if ( !g_pFullFileSystem->ReadFile( pCacheFileName, NULL, buf, nHeaderSize ) )	
	{
		goto precacheFailed;
	}

	if ( !pVTFTexture->Unserialize( buf, true ) )
	{
		Warning( "Error reading material \"%s\"\n", pCacheFileName );
		goto precacheFailed;
	}

	// NOTE: Don't set the image format in case graphics are active
	VectorCopy( pVTFTexture->Reflectivity(), m_vecReflectivity );
	m_dimsMapping.m_nWidth = pVTFTexture->Width();
	m_dimsMapping.m_nHeight = pVTFTexture->Height();
	m_dimsMapping.m_nDepth = pVTFTexture->Depth();
	m_nFlags = pVTFTexture->Flags() | nAdditionalFlags;
	m_nFrameCount = pVTFTexture->FrameCount();
	if ( !m_pTextureHandles )
	{
		// NOTE: m_nFrameCount and m_pTextureHandles are strongly associated
		// whenever one is modified the other must also be modified
		AllocateTextureHandles();
	}

	return;

precacheFailed:
	m_vecReflectivity.Init( 0, 0, 0 );
	m_dimsMapping.m_nWidth = 32;
	m_dimsMapping.m_nHeight = 32;
	m_dimsMapping.m_nDepth = 1;
	m_nFlags = TEXTUREFLAGS_NOMIP;
	SetErrorTexture( true );
	m_nFrameCount = 1;
	if ( !m_pTextureHandles )
	{
		// NOTE: m_nFrameCount and m_pTextureHandles are strongly associated
		// whenever one is modified the other must also be modified
		AllocateTextureHandles();
	}
}



//-----------------------------------------------------------------------------
// Loads the low-res image from the texture 
//-----------------------------------------------------------------------------
void CTexture::LoadLowResTexture( IVTFTexture *pTexture )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	delete [] m_pLowResImage;
	m_pLowResImage = NULL;

	if ( pTexture->LowResWidth() == 0 || pTexture->LowResHeight() == 0 )
	{
		m_LowResImageWidth = m_LowResImageHeight = 0;
		return;
	}

	m_LowResImageWidth = pTexture->LowResWidth();
	m_LowResImageHeight = pTexture->LowResHeight();

	m_pLowResImage = new unsigned char[m_LowResImageWidth * m_LowResImageHeight * 3];
#ifdef DBGFLAG_ASSERT
	bool retVal = 
#endif
		ImageLoader::ConvertImageFormat( pTexture->LowResImageData(), pTexture->LowResFormat(), 
			m_pLowResImage, IMAGE_FORMAT_RGB888, m_LowResImageWidth, m_LowResImageHeight );
	Assert( retVal );
}

void *CTexture::GetResourceData( uint32 eDataType, size_t *pnumBytes ) const
{
	for ( DataChunk const *pDataChunk = m_arrDataChunks.Base(),
		  *pDataChunkEnd = pDataChunk + m_arrDataChunks.Count();
		  pDataChunk < pDataChunkEnd; ++pDataChunk )
	{
		if ( ( pDataChunk->m_eType & ~RSRCF_MASK ) == eDataType )
		{
			if ( ( pDataChunk->m_eType & RSRCF_HAS_NO_DATA_CHUNK ) == 0 )
			{
				if ( pnumBytes)
					*pnumBytes = pDataChunk->m_numBytes;
				return pDataChunk->m_pvData;
			}
			else
			{
				if ( pnumBytes )
					*pnumBytes = sizeof( pDataChunk->m_numBytes );

				return ( void *)( &pDataChunk->m_numBytes );
			}
		}
	}
	if ( pnumBytes )
		pnumBytes = 0;
	return NULL;
}

#pragma pack(1)

struct DXTColBlock
{
	unsigned short col0;
	unsigned short col1;

	// no bit fields - use bytes
	unsigned char row[4];
};

struct DXTAlphaBlock3BitLinear
{
	unsigned char alpha0;
	unsigned char alpha1;

	unsigned char stuff[6];
};

#pragma pack()

static void FillCompressedTextureWithSingleColor( int red, int green, int blue, int alpha, unsigned char *pImageData, 
												 int width, int height, int depth, ImageFormat imageFormat )
{
	Assert( ( width < 4 ) || !( width % 4 ) );
	Assert( ( height < 4 ) || !( height % 4 ) );
	Assert( ( depth < 4 ) || !( depth % 4 ) );

	if ( width < 4 && width > 0 )
	{
		width = 4;
	}
	if ( height < 4 && height > 0 )
	{
		height = 4;
	}
	if ( depth < 4 && depth > 1 )
	{
		depth = 4;
	}
	int numBlocks = ( width * height ) >> 4;
	numBlocks *= depth;
	
	DXTColBlock colorBlock;
	memset( &colorBlock, 0, sizeof( colorBlock ) );
	( ( BGR565_t * )&( colorBlock.col0 ) )->Set( red, green, blue );
	( ( BGR565_t * )&( colorBlock.col1 ) )->Set( red, green, blue );

	switch( imageFormat )
	{
	case IMAGE_FORMAT_DXT1:
	case IMAGE_FORMAT_ATI1N:	// Invalid block data, but correct memory footprint
		{
			int i;
			for( i = 0; i < numBlocks; i++ )
			{
				memcpy( pImageData + i * 8, &colorBlock, sizeof( colorBlock ) );
			}
		}
		break;
	case IMAGE_FORMAT_DXT5:
	case IMAGE_FORMAT_ATI2N:
		{
			int i;
			for( i = 0; i < numBlocks; i++ )
			{
//				memset( pImageData + i * 16, 0, 16 );
				memcpy( pImageData + i * 16 + 8, &colorBlock, sizeof( colorBlock ) );
//				memset( pImageData + i * 16 + 8, 0xffff, 8 ); // alpha block
			}
		}
		break;
	default:
		Assert( 0 );
		break;
	}
}

// This table starts out like the programmatic logic that used to be here,
// but then has some other colors, so that we don't see repeats.
// Also, there is no black, which seems to be an error condition on OpenGL.
// There also aren't any zeros in this table, since these colors may get
// multiplied with, say, vertex colors which are tinted, resulting in black pixels.
int sg_nMipLevelColors[14][3] = { {  64, 255,  64 },  // Green
								  { 255,  64,  64 },  // Red
								  { 255, 255,  64 },  // Yellow
								  {  64,  64, 255 },  // Blue
								  {  64, 255, 255 },  // Cyan
								  { 255,  64, 255 },  // Magenta
								  { 255, 255, 255 },  // White
								  { 255, 150, 150 },  // Light Red
								  { 255, 255, 150 },  // Light Yellow
								  { 150, 150, 255 },  // Light Blue
								  { 150, 255, 255 },  // Light Cyan
								  { 255, 150, 255 },  // Light Magenta
								  { 150, 150, 128 },  // Light Gray
								  { 138, 131,  64 } };// Brown

//-----------------------------------------------------------------------------
// Generate a texture that shows the various mip levels
//-----------------------------------------------------------------------------
void CTexture::GenerateShowMipLevelsTextures( IVTFTexture *pTexture )
{
	if( pTexture->FaceCount() > 1 )
		return;

	switch( pTexture->Format() )
	{
	// These are formats that we don't bother with for generating mip level textures.
	case IMAGE_FORMAT_RGBA16161616F:
	case IMAGE_FORMAT_R32F:
	case IMAGE_FORMAT_RGB323232F:
	case IMAGE_FORMAT_RGBA32323232F:
	case IMAGE_FORMAT_UV88:
		break;
	default:
		for (int iFrame = 0; iFrame < pTexture->FrameCount(); ++iFrame )
		{
			for (int iFace = 0; iFace < pTexture->FaceCount(); ++iFace )
			{
				for (int iMip = 0; iMip < pTexture->MipCount(); ++iMip )
				{
					int red	  = sg_nMipLevelColors[iMip][0];//( ( iMip + 1 ) & 2 ) ? 255 : 0;
					int green = sg_nMipLevelColors[iMip][1];//( ( iMip + 1 ) & 1 ) ? 255 : 0;
					int blue  = sg_nMipLevelColors[iMip][2];//( ( iMip + 1 ) & 4 ) ? 255 : 0;

					int nWidth, nHeight, nDepth;
					pTexture->ComputeMipLevelDimensions( iMip, &nWidth, &nHeight, &nDepth );
					if( pTexture->Format() == IMAGE_FORMAT_DXT1  || pTexture->Format() == IMAGE_FORMAT_DXT5 ||
					    pTexture->Format() == IMAGE_FORMAT_ATI1N || pTexture->Format() == IMAGE_FORMAT_ATI2N )
					{
						unsigned char *pImageData = pTexture->ImageData( iFrame, iFace, iMip, 0, 0, 0 );
						int alpha = 255;
						FillCompressedTextureWithSingleColor( red, green, blue, alpha, pImageData, nWidth, nHeight, nDepth, pTexture->Format() );
					}
					else
					{
						for ( int z = 0; z < nDepth; ++z )
						{
							CPixelWriter pixelWriter;
							pixelWriter.SetPixelMemory( pTexture->Format(), 
								pTexture->ImageData( iFrame, iFace, iMip, 0, 0, z ), pTexture->RowSizeInBytes( iMip ) );

							for (int y = 0; y < nHeight; ++y)
							{
								pixelWriter.Seek( 0, y );
								for (int x = 0; x < nWidth; ++x)
								{
									pixelWriter.WritePixel( red, green, blue, 255 );
								}
							}
						}
					}
				}
			}
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Generate a texture that shows the various mip levels
//-----------------------------------------------------------------------------
void CTexture::CopyLowResImageToTexture( IVTFTexture *pTexture )
{
	int nFlags = pTexture->Flags();
	nFlags |= TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_POINTSAMPLE;
	nFlags &= ~(TEXTUREFLAGS_TRILINEAR | TEXTUREFLAGS_ANISOTROPIC | TEXTUREFLAGS_HINT_DXT5);
	nFlags &= ~(TEXTUREFLAGS_NORMAL | TEXTUREFLAGS_ENVMAP);
	nFlags &= ~(TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA);

	Assert( pTexture->FrameCount() == 1 );

	Init( pTexture->Width(), pTexture->Height(), 1, IMAGE_FORMAT_BGR888, nFlags, 1 );
	pTexture->Init( m_LowResImageWidth, m_LowResImageHeight, 1, IMAGE_FORMAT_BGR888, nFlags, 1 );

	// Don't bother computing the actual size; it's actually equal to the low-res size
	// With only one mip level
	m_dimsActual.m_nWidth = m_LowResImageWidth;
	m_dimsActual.m_nHeight = m_LowResImageHeight;
	m_dimsActual.m_nDepth = 1;
	m_dimsActual.m_nMipCount = 1;

	// Copy the row-res image into the VTF Texture
	CPixelWriter pixelWriter;
	pixelWriter.SetPixelMemory( pTexture->Format(), 
		pTexture->ImageData( 0, 0, 0 ), pTexture->RowSizeInBytes( 0 ) );

	for ( int y = 0; y < m_LowResImageHeight; ++y )
	{
		pixelWriter.Seek( 0, y );
		for ( int x = 0; x < m_LowResImageWidth; ++x )
		{
			int red = m_pLowResImage[0];
			int green = m_pLowResImage[1];
			int blue = m_pLowResImage[2];
			m_pLowResImage += 3;

			pixelWriter.WritePixel( red, green, blue, 255 );
		}
	}
}

//-----------------------------------------------------------------------------
// Sets up debugging texture bits, if appropriate
//-----------------------------------------------------------------------------
bool CTexture::SetupDebuggingTextures( IVTFTexture *pVTFTexture )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	if ( pVTFTexture->Flags() & TEXTUREFLAGS_NODEBUGOVERRIDE )
		return false;

	// The all mips flag is typically used on detail textures, which can
	// really mess up visualization if we apply the debug-colorized
	// versions of them to debug-colorized base textures, so skip 'em
	if ( g_config.nShowMipLevels && !(pVTFTexture->Flags() & TEXTUREFLAGS_ALL_MIPS) )
	{
		// mat_showmiplevels 1 means don't do normal maps
		if ( ( g_config.nShowMipLevels == 1 ) && ( pVTFTexture->Flags() & ( TEXTUREFLAGS_NORMAL | TEXTUREFLAGS_SSBUMP ) ) )
			return false;

		// mat_showmiplevels 2 means don't do base textures
		if ( ( g_config.nShowMipLevels == 2 ) && !( pVTFTexture->Flags() & ( TEXTUREFLAGS_NORMAL | TEXTUREFLAGS_SSBUMP ) ) )
			return false;

		// This mode shows the mip levels as different colors
		GenerateShowMipLevelsTextures( pVTFTexture );
		return true;
	}
	else if ( g_config.bShowLowResImage && pVTFTexture->FrameCount() == 1 && 
		pVTFTexture->FaceCount() == 1 && ((pVTFTexture->Flags() & TEXTUREFLAGS_NORMAL) == 0) &&
		m_LowResImageWidth != 0 && m_LowResImageHeight != 0 )
	{
		// This mode just uses the low res texture
		CopyLowResImageToTexture( pVTFTexture );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Converts the texture to the actual format
// Returns true if conversion applied, false otherwise
//-----------------------------------------------------------------------------
bool CTexture::ConvertToActualFormat( IVTFTexture *pVTFTexture )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	if ( !g_pShaderDevice->IsUsingGraphics() )
		return false;

	bool bConverted = false;

	ImageFormat fmt = m_ImageFormat;

	ImageFormat dstFormat = ComputeActualFormat( pVTFTexture->Format() );
	if ( fmt != dstFormat )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - conversion from (%d to %d)", __FUNCTION__, fmt, dstFormat );

		pVTFTexture->ConvertImageFormat( dstFormat, false );

		m_ImageFormat = dstFormat;
		bConverted = true;
	}
	else if ( HardwareConfig()->GetHDRType() == HDR_TYPE_INTEGER &&
		     fmt == dstFormat && dstFormat == IMAGE_FORMAT_RGBA16161616F )
	{
		// This is to force at most the precision of int16 for fp16 texture when running the integer path.
		pVTFTexture->ConvertImageFormat( IMAGE_FORMAT_RGBA16161616, false );
		pVTFTexture->ConvertImageFormat( IMAGE_FORMAT_RGBA16161616F, false );
		bConverted = true;
	}

	return bConverted;
}

void CTexture::GetFilename( char *pOut, int maxLen ) const
{
	const char *pName = m_Name.String();
	bool bIsUNCName = ( pName[0] == '/' && pName[1] == '/' && pName[2] != '/' );

	if ( !bIsUNCName )
	{
		Q_snprintf( pOut, maxLen, 
			"materials/%s" TEXTURE_FNAME_EXTENSION, pName );
	}
	else
	{
		Q_snprintf( pOut, maxLen, "%s" TEXTURE_FNAME_EXTENSION, pName );
	}
}


void CTexture::ReloadFilesInList( IFileList *pFilesToReload )
{
	if ( IsProcedural() || IsRenderTarget() )
		return;

	char filename[MAX_PATH];
	GetFilename( filename, sizeof( filename ) );
	if ( pFilesToReload->IsFileInList( filename ) )
	{
		Download();
	}
}

//-----------------------------------------------------------------------------
// Loads the texture bits from a file.
//-----------------------------------------------------------------------------
IVTFTexture *CTexture::LoadTextureBitsFromFile( char *pCacheFileName, char **ppResolvedFilename )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s %s", __FUNCTION__, tmDynamicString( TELEMETRY_LEVEL0, pCacheFileName ) );
	
	if ( m_bStreamingFileReadFailed )
	{
		Assert( m_pStreamingVTF == NULL );
		return HandleFileLoadFailedTexture( GetScratchVTFTexture() );
	}

	// OSX hackery
	int nPreserveFlags = 0;
	if ( m_nFlags & TEXTUREFLAGS_SRGB )
		nPreserveFlags |= TEXTUREFLAGS_SRGB;

	unsigned int stripFlags = 0;

	IVTFTexture *pVTFTexture = m_pStreamingVTF;
	if ( !pVTFTexture )
	{
		pVTFTexture = GetScratchVTFTexture();

		FileHandle_t fileHandle = FILESYSTEM_INVALID_HANDLE;

		if ( !GetFileHandle( &fileHandle, pCacheFileName, ppResolvedFilename ) )
			return HandleFileLoadFailedTexture( pVTFTexture );

		TextureLODControlSettings_t settings = m_cachedFileLodSettings;
		if ( !SLoadTextureBitsFromFile( &pVTFTexture, fileHandle, m_nFlags | nPreserveFlags, &settings, m_nDesiredDimensionLimit, &m_nStreamingMips, GetName(), pCacheFileName, &m_dimsMapping, &m_dimsActual, &m_dimsAllocated, &stripFlags ) )
		{
			g_pFullFileSystem->Close( fileHandle );
			return HandleFileLoadFailedTexture( pVTFTexture );
		}

		g_pFullFileSystem->Close( fileHandle );
	}


	// Don't reinitialize here if we're streaming in the fine levels, we already have been initialized with coarse.
	if ( ( m_nFlags & TEXTUREFLAGS_STREAMABLE_FINE ) == 0 )
	{
		// Initing resets these, but we're happy with the values now--so store and restore them around the Init call.
		TexDimensions_t actual = m_dimsActual,
					 allocated = m_dimsAllocated;

		// Initialize the texture class with vtf header data before operations
		Init( m_dimsMapping.m_nWidth,
			  m_dimsMapping.m_nHeight,
			  m_dimsMapping.m_nDepth,
			  pVTFTexture->Format(),
			  pVTFTexture->Flags() | nPreserveFlags,
			  pVTFTexture->FrameCount()
		);

		m_dimsActual = actual;
		m_dimsAllocated = allocated;

		m_nFlags &= ~stripFlags;
	}
	else
	{
		// Not illegal, just needs investigation.
		Assert( stripFlags == 0 );
	}

	if ( m_pStreamingVTF )
		ComputeActualSize( false, pVTFTexture, ( m_nFlags & TEXTUREFLAGS_STREAMABLE ) != 0 );

	VectorCopy( pVTFTexture->Reflectivity(), m_vecReflectivity );

	// If we've only streamed in coarse but haven't started on fine yet, go ahead and mark us as
	// partially resident and set up our clamping values. 
	if ( ( m_nFlags & TEXTUREFLAGS_STREAMABLE ) == TEXTUREFLAGS_STREAMABLE_COARSE )
	{
		pVTFTexture->GetMipmapRange( &m_lodClamp, NULL );
		m_residenceTarget = RESIDENT_PARTIAL;
		m_residenceCurrent = RESIDENT_PARTIAL;
	}

	// Build the low-res texture
	LoadLowResTexture( pVTFTexture );

	// Load the resources
	if ( unsigned int uiRsrcCount = pVTFTexture->GetResourceTypes( NULL, 0 ) )
	{
		uint32 *arrRsrcTypes = ( uint32 * )_alloca( uiRsrcCount * sizeof( unsigned int ) );
		pVTFTexture->GetResourceTypes( arrRsrcTypes, uiRsrcCount );

		m_arrDataChunks.EnsureCapacity( uiRsrcCount );
		for ( uint32 *arrRsrcTypesEnd = arrRsrcTypes + uiRsrcCount;
			  arrRsrcTypes < arrRsrcTypesEnd; ++arrRsrcTypes )
		{
			switch ( *arrRsrcTypes )
			{
			case VTF_LEGACY_RSRC_LOW_RES_IMAGE:
			case VTF_LEGACY_RSRC_IMAGE:
				// These stock types use specific load routines
				continue;
					
			default:
				{
					DataChunk dc;
					dc.m_eType = *arrRsrcTypes;
					dc.m_eType &= ~RSRCF_MASK;

					size_t numBytes;
					if ( void *pvData = pVTFTexture->GetResourceData( dc.m_eType, &numBytes ) )
					{
						Assert( numBytes >= sizeof( uint32 ) );
						if ( numBytes == sizeof( dc.m_numBytes ) )
						{
							dc.m_eType |= RSRCF_HAS_NO_DATA_CHUNK;
							dc.m_pvData = NULL;
							memcpy( &dc.m_numBytes, pvData, numBytes );
						}
						else
						{
							dc.Allocate( numBytes );
							memcpy( dc.m_pvData, pvData, numBytes );
						}

						m_arrDataChunks.AddToTail( dc );
					}
				}
			}
		}
	}

	// Try to set up debugging textures, if we're in a debugging mode
	if ( !IsProcedural() )
		SetupDebuggingTextures( pVTFTexture );

	if ( ConvertToActualFormat( pVTFTexture ) )
		pVTFTexture; // STAGING_ONLY_EXEC ( Warning( "\"%s\" not in final format, this is causing stutters or load time bloat!\n", pCacheFileName ) );

	return pVTFTexture;
}


IVTFTexture *CTexture::HandleFileLoadFailedTexture( IVTFTexture *pVTFTexture )
{
	// create the error texture

	// This will make a checkerboard texture to indicate failure
	pVTFTexture->Init( 32, 32, 1, IMAGE_FORMAT_BGRA8888, m_nFlags, 1 );
	Init( pVTFTexture->Width(), pVTFTexture->Height(), pVTFTexture->Depth(), pVTFTexture->Format(), 
		  pVTFTexture->Flags(), pVTFTexture->FrameCount() );
	m_vecReflectivity.Init( 0.5f, 0.5f, 0.5f );

	// NOTE: For mat_picmip to work, we must use the same size (32x32)
	// Which should work since every card can handle textures of that size
	m_dimsAllocated.m_nWidth = m_dimsActual.m_nWidth = pVTFTexture->Width();
	m_dimsAllocated.m_nHeight = m_dimsActual.m_nHeight = pVTFTexture->Height();
	m_dimsAllocated.m_nDepth = 1;
	m_dimsAllocated.m_nMipCount = m_dimsActual.m_nMipCount = 1;
	m_nStreamingMips = 0;

	

	// generate the checkerboard
	TextureManager()->GenerateErrorTexture( this, pVTFTexture );
	ConvertToActualFormat( pVTFTexture );

	// Deactivate procedural texture...
	m_nFlags &= ~TEXTUREFLAGS_PROCEDURAL;
	SetErrorTexture( true );

	return pVTFTexture;
}

//-----------------------------------------------------------------------------
// Computes subrect for a particular miplevel
//-----------------------------------------------------------------------------
void CTexture::ComputeMipLevelSubRect( const Rect_t* pSrcRect, int nMipLevel, Rect_t *pSubRect )
{
	if (nMipLevel == 0)
	{
		*pSubRect = *pSrcRect;
		return;
	}

	float flInvShrink = 1.0f / (float)(1 << nMipLevel);
	pSubRect->x = pSrcRect->x * flInvShrink;
	pSubRect->y = pSrcRect->y * flInvShrink;
	pSubRect->width = (int)ceil( (pSrcRect->x + pSrcRect->width) * flInvShrink ) - pSubRect->x;
	pSubRect->height = (int)ceil( (pSrcRect->y + pSrcRect->height) * flInvShrink ) - pSubRect->y;
}


//-----------------------------------------------------------------------------
// Computes the face count + first face
//-----------------------------------------------------------------------------
void CTexture::GetDownloadFaceCount( int &nFirstFace, int &nFaceCount )
{
	nFaceCount = 1;
	nFirstFace = 0;
	if ( IsCubeMap() )
	{
		if ( HardwareConfig()->SupportsCubeMaps() )
		{
			nFaceCount = CUBEMAP_FACE_COUNT-1;
		}
		else
		{
			// This will cause us to use the spheremap instead of the cube faces
			// in the case where we don't support cubemaps
			nFirstFace = CUBEMAP_FACE_SPHEREMAP;
		}
	}
}

//-----------------------------------------------------------------------------
// Fixup a queue loaded texture with the delayed hi-res data
//-----------------------------------------------------------------------------
void CTexture::FixupTexture( const void *pData, int nSize, LoaderError_t loaderError )
{
	if ( loaderError != LOADERERROR_NONE )
	{
		// mark as invalid
		nSize = 0;
	}

	m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_QUEUEDLOAD;

	// Make sure we've actually allocated the texture handles
	Assert( HasBeenAllocated() );
} 

static void QueuedLoaderCallback( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError )
{
	reinterpret_cast< CTexture * >( pContext )->FixupTexture( pData, nSize, loaderError );
}

//-----------------------------------------------------------------------------
// Generates the procedural bits
//-----------------------------------------------------------------------------
IVTFTexture *CTexture::ReconstructPartialProceduralBits( const Rect_t *pRect, Rect_t *pActualRect )
{
	// Figure out the actual size for this texture based on the current mode
	bool bIgnorePicmip = ( m_nFlags & ( TEXTUREFLAGS_STAGING_MEMORY | TEXTUREFLAGS_IGNORE_PICMIP ) ) != 0;
	ComputeActualSize( bIgnorePicmip );

	// Figure out how many mip levels we're skipping...
	int nSizeFactor = 1;
	int nWidth = GetActualWidth();
	if ( nWidth != 0 )
	{
		nSizeFactor = GetMappingWidth() / nWidth;
	}
	int nMipSkipCount = 0;
	while (nSizeFactor > 1)
	{
		nSizeFactor >>= 1;
		++nMipSkipCount;
	}

	// Determine a rectangle appropriate for the actual size...
	// It must bound all partially-covered pixels..
	ComputeMipLevelSubRect( pRect, nMipSkipCount, pActualRect );

	// Create the texture
	IVTFTexture *pVTFTexture = GetScratchVTFTexture();

	// Initialize the texture
	pVTFTexture->Init( m_dimsActual.m_nWidth, m_dimsActual.m_nHeight, m_dimsActual.m_nDepth,
		ComputeActualFormat( m_ImageFormat ), m_nFlags, m_nFrameCount );

	// Generate the bits from the installed procedural regenerator
	if ( m_pTextureRegenerator )
	{
		m_pTextureRegenerator->RegenerateTextureBits( this, pVTFTexture, pActualRect );
	}
	else
	{
		// In this case, we don't have one, so just use a checkerboard...
		TextureManager()->GenerateErrorTexture( this, pVTFTexture );
	}

	return pVTFTexture;
}


//-----------------------------------------------------------------------------
// Regenerates the bits of a texture within a particular rectangle
//-----------------------------------------------------------------------------
void CTexture::ReconstructPartialTexture( const Rect_t *pRect )
{
	// FIXME: for now, only procedural textures can handle sub-rect specification.
	Assert( IsProcedural() );

	// Also, we need procedural textures that have only a single copy!!
	// Otherwise this partial upload will not occur on all copies
	Assert( m_nFlags & TEXTUREFLAGS_SINGLECOPY );

	Rect_t vtfRect;
	IVTFTexture *pVTFTexture = ReconstructPartialProceduralBits( pRect, &vtfRect );

	// FIXME: for now, depth textures do not work with this.
	Assert( pVTFTexture->Depth() == 1 );

	// Make sure we've allocated the API textures
	if ( !HasBeenAllocated() )
	{
		if ( !AllocateShaderAPITextures() )
			return;
	}

	int nFaceCount, nFirstFace;
	GetDownloadFaceCount( nFirstFace, nFaceCount );
	
	// Blit down portions of the various VTF frames into the board memory
	int nStride;
	Rect_t mipRect;
	for ( int iFrame = 0; iFrame < m_nFrameCount; ++iFrame )
	{
		Modify( iFrame );

		for ( int iFace = 0; iFace < nFaceCount; ++iFace )
		{
			for ( int iMip = 0; iMip < m_dimsActual.m_nMipCount;  ++iMip )
			{
				pVTFTexture->ComputeMipLevelSubRect( &vtfRect, iMip, &mipRect );
				nStride = pVTFTexture->RowSizeInBytes( iMip );
				unsigned char *pBits = pVTFTexture->ImageData( iFrame, iFace + nFirstFace, iMip, mipRect.x, mipRect.y, 0 );
				g_pShaderAPI->TexSubImage2D( 
					iMip, 
					iFace, 
					mipRect.x, 
					mipRect.y,
					0,
					mipRect.width, 
					mipRect.height, 
					pVTFTexture->Format(), 
					nStride, 
					false,
					pBits );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Generates the procedural bits
//-----------------------------------------------------------------------------
IVTFTexture *CTexture::ReconstructProceduralBits()
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	// Figure out the actual size for this texture based on the current mode
	bool bIgnorePicmip = ( m_nFlags & ( TEXTUREFLAGS_STAGING_MEMORY | TEXTUREFLAGS_IGNORE_PICMIP ) ) != 0;
	ComputeActualSize( bIgnorePicmip );

	// Create the texture
	IVTFTexture *pVTFTexture = NULL;
	
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - GetScratchVTFTexture", __FUNCTION__ );
		pVTFTexture = GetScratchVTFTexture();
	}

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Init", __FUNCTION__ );
		// Initialize the texture
		pVTFTexture->Init( m_dimsActual.m_nWidth, m_dimsActual.m_nHeight, m_dimsActual.m_nDepth,
			ComputeActualFormat( m_ImageFormat ), m_nFlags, m_nFrameCount );
	}

	// Generate the bits from the installed procedural regenerator
	if ( m_pTextureRegenerator )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - RegenerateTextureBits", __FUNCTION__ );

		Rect_t rect;
		rect.x = 0; rect.y = 0;
		rect.width = m_dimsActual.m_nWidth; 
		rect.height = m_dimsActual.m_nHeight; 
		m_pTextureRegenerator->RegenerateTextureBits( this, pVTFTexture, &rect );
	}
	else
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - GenerateErrorTexture", __FUNCTION__ );

		// In this case, we don't have one, so just use a checkerboard...
		TextureManager()->GenerateErrorTexture( this, pVTFTexture );
	}

	return pVTFTexture;
}

void CTexture::WriteDataToShaderAPITexture( int nFrameCount, int nFaceCount, int nFirstFace, int nMipCount, IVTFTexture *pVTFTexture, ImageFormat fmt )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	// If we're a staging texture, there's nothing to do.
	if ( ( m_nFlags & TEXTUREFLAGS_STAGING_MEMORY ) != 0 )
		return;

	for ( int iFrame = 0; iFrame < m_nFrameCount; ++iFrame )
	{
		Modify( iFrame );
		g_pShaderAPI->TexImageFromVTF( pVTFTexture, iFrame );
	}
}

bool CTexture::IsDepthTextureFormat( ImageFormat fmt )
{
	return ( ( m_ImageFormat == IMAGE_FORMAT_NV_DST16  ) ||
			 ( m_ImageFormat == IMAGE_FORMAT_NV_DST24  ) ||
			 ( m_ImageFormat == IMAGE_FORMAT_NV_INTZ   ) ||
			 ( m_ImageFormat == IMAGE_FORMAT_NV_RAWZ   ) ||
			 ( m_ImageFormat == IMAGE_FORMAT_ATI_DST16 ) ||
			 ( m_ImageFormat == IMAGE_FORMAT_ATI_DST24 ) );
}

//-----------------------------------------------------------------------------
void CTexture::NotifyUnloadedFile()
{
	// Make sure we have a regular texture that was loaded from a file
	if ( IsProcedural() || IsRenderTarget() || !m_Name.IsValid() )
		return;
	const char *pName = m_Name.String();
	if ( *pName == '\0' )
		return;
	bool bIsUNCName = ( pName[0] == '/' && pName[1] == '/' && pName[2] != '/' );
	if ( bIsUNCName )
		return;

	// Generate the filename
	char pCacheFileName[MATERIAL_MAX_PATH];
	Q_snprintf( pCacheFileName, sizeof( pCacheFileName ), "materials/%s" TEXTURE_FNAME_EXTENSION, pName );

	// Let filesystem know that the file is uncached, so it knows
	// what to do with tracking info
	g_pFullFileSystem->NotifyFileUnloaded( pCacheFileName, "GAME" );
}

//-----------------------------------------------------------------------------
// Sets or updates the texture bits
//-----------------------------------------------------------------------------
void CTexture::ReconstructTexture( bool bCopyFromCurrent )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	Assert( !bCopyFromCurrent || HardwareConfig()->CanStretchRectFromTextures() );

	int oldWidth = m_dimsAllocated.m_nWidth;
	int oldHeight = m_dimsAllocated.m_nHeight;
	int oldDepth = m_dimsAllocated.m_nDepth;
	int oldMipCount = m_dimsAllocated.m_nMipCount;
	int oldFrameCount = m_nFrameCount;

	// FIXME: Should RenderTargets be a special case of Procedural?
	char *pResolvedFilename = NULL;
	IVTFTexture *pVTFTexture = NULL;
	
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Begin", __FUNCTION__ );
		if ( IsProcedural() )
		{
			// This will call the installed texture bit regeneration interface
			pVTFTexture = ReconstructProceduralBits();
		}
		else if ( IsRenderTarget() )
		{
			// Compute the actual size + format based on the current mode
			bool bIgnorePicmip = m_RenderTargetSizeMode != RT_SIZE_LITERAL_PICMIP;
			ComputeActualSize( bIgnorePicmip );
		}
		else if ( bCopyFromCurrent )
		{
			ComputeActualSize( false, NULL, true );
		}
		else
		{
			NotifyUnloadedFile();

			char pCacheFileName[ MATERIAL_MAX_PATH ] = { 0 };
			GetCacheFilename( pCacheFileName, ARRAYSIZE( pCacheFileName ) );
		
			// Get the data from disk...
			// NOTE: Reloading the texture bits can cause the texture size, frames, format, pretty much *anything* can change.
			pVTFTexture = LoadTextureBitsFromFile( pCacheFileName, &pResolvedFilename );
		}
	}

	if ( !HasBeenAllocated() ||
		m_dimsAllocated.m_nWidth != oldWidth ||
		m_dimsAllocated.m_nHeight != oldHeight ||
		m_dimsAllocated.m_nDepth != oldDepth ||
		m_dimsAllocated.m_nMipCount != oldMipCount ||
		m_nFrameCount != oldFrameCount )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Allocation", __FUNCTION__ );

		const bool cbCanStretchRectTextures = HardwareConfig()->CanStretchRectFromTextures();
		const bool cbShouldMigrateTextures = ( ( m_nFlags & TEXTUREFLAGS_STREAMABLE_FINE ) != 0 ) && m_nFrameCount == oldFrameCount;

		// If we're just streaming in more data--or demoting ourselves, do a migration instead. 
		if ( bCopyFromCurrent || ( cbCanStretchRectTextures && cbShouldMigrateTextures ) )
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Migration", __FUNCTION__ );

			MigrateShaderAPITextures();

			// Ahh--I feel terrible about this, but we genuinely don't need anything else if we're streaming.
			if ( bCopyFromCurrent )
				return;
		}
		else
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Deallocate / Allocate", __FUNCTION__ );

			// If we're doing a wholesale copy, we need to restore these values that will be cleared by FreeShaderAPITextures.
			// Record them here, restore them below.
			unsigned int restoreStreamingFlag = ( m_nFlags & TEXTUREFLAGS_STREAMABLE );
			ResidencyType_t restoreResidenceCurrent = m_residenceCurrent;
			ResidencyType_t restoreResidenceTarget = m_residenceTarget;

			if ( HasBeenAllocated() )
			{
				tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Deallocate", __FUNCTION__ );

				// This is necessary for the reload case, we may discover there
				// are more frames of a texture animation, for example, which means
				// we can't rely on having the same number of texture frames.
				FreeShaderAPITextures();
			}

			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Allocate", __FUNCTION__ );

			// Create the shader api textures
			if ( !AllocateShaderAPITextures() )
				return;

			// Restored once we successfully allocate the shader api textures, but only if we're 
			// 
			if ( !cbCanStretchRectTextures && cbShouldMigrateTextures )
			{
				m_nFlags |= restoreStreamingFlag;
				m_residenceCurrent = restoreResidenceCurrent;
				m_residenceTarget = restoreResidenceTarget;
			}
		}
	} 
	else if ( bCopyFromCurrent )
	{
		Assert( !"We're about to crash, last chance to examine this texture." );
	}


	// Render Targets just need to be cleared, they have no upload
	if ( IsRenderTarget() )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - RT Stuff", __FUNCTION__ );

		// Clear the render target to opaque black

		// Only clear if we're not a depth-stencil texture
		if ( !IsDepthTextureFormat( m_ImageFormat ) )
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Clearing", __FUNCTION__ );

			CMatRenderContextPtr pRenderContext( MaterialSystem() );
			ITexture *pThisTexture = GetEmbeddedTexture( 0 );
			pRenderContext->PushRenderTargetAndViewport( pThisTexture );						// Push this texture on the stack
			g_pShaderAPI->ClearColor4ub( 0, 0, 0, 0xFF );										// Set the clear color to opaque black
			g_pShaderAPI->ClearBuffers( true, false, false, m_dimsActual.m_nWidth, m_dimsActual.m_nHeight );	// Clear the target
			pRenderContext->PopRenderTargetAndViewport();										// Pop back to previous target
		}
		// no upload
		return;
	}

	// Blit down the texture faces, frames, and mips into the board memory
	int nFirstFace, nFaceCount;
	GetDownloadFaceCount( nFirstFace, nFaceCount );
	
	WriteDataToShaderAPITexture( m_nFrameCount, nFaceCount, nFirstFace, m_dimsActual.m_nMipCount, pVTFTexture, m_ImageFormat );

	ReleaseScratchVTFTexture( pVTFTexture );
	pVTFTexture = NULL;

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Final Cleanup", __FUNCTION__ );

	// allocated by strdup
	free( pResolvedFilename );

	// the pc can afford to persist a large buffer
	FreeOptimalReadBuffer( 6*1024*1024 );
}

void CTexture::GetCacheFilename( char* pOutBuffer, int nBufferSize ) const
{
	Assert( pOutBuffer );

	if ( IsProcedural() || IsRenderTarget() )
	{
		pOutBuffer[0] = 0;
		return;
	}
	else
	{
		const char *pName;
		if ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE )
		{
			pName = "dev/dev_exclude_error";
		}
		else
		{
			pName = m_Name.String();
		}

		bool bIsUNCName = ( pName[ 0 ] == '/' && pName[ 1 ] == '/' && pName[ 2 ] != '/' );
		if ( !bIsUNCName )
		{
			Q_snprintf( pOutBuffer, nBufferSize, "materials/%s" TEXTURE_FNAME_EXTENSION, pName );
		}
		else
		{
			Q_snprintf( pOutBuffer, nBufferSize, "%s" TEXTURE_FNAME_EXTENSION, pName );
		}
	}
}

bool CTexture::GetFileHandle( FileHandle_t *pOutFileHandle, char *pCacheFileName, char **ppResolvedFilename ) const
{
	Assert( pOutFileHandle );
	FileHandle_t& fileHandle = *pOutFileHandle;
	fileHandle = FILESYSTEM_INVALID_HANDLE;

	while ( fileHandle == FILESYSTEM_INVALID_HANDLE )			// run until found a file or out of rules
	{
		fileHandle = g_pFullFileSystem->OpenEx( pCacheFileName, "rb", 0, MaterialSystem()->GetForcedTextureLoadPathID(), ppResolvedFilename );
		if ( fileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			// try any fallbacks.
			char *pHdrExt = Q_stristr( pCacheFileName, ".hdr" TEXTURE_FNAME_EXTENSION );
			if ( pHdrExt )
			{
				DevWarning( "A custom HDR cubemap \"%s\": cannot be found on disk.\n"
					"This really should have a HDR version, trying a fall back to a non-HDR version.\n", pCacheFileName );
				strcpy( pHdrExt, TEXTURE_FNAME_EXTENSION );
			}
			else
			{
				// no more fallbacks
				break;
			}
		}
	}

	if ( fileHandle == FILESYSTEM_INVALID_HANDLE )
	{
		if ( Q_strnicmp( m_Name.String(), "env_cubemap", 12 ) )
		{
			if ( IsPosix() )
			{
				Msg( "\n ##### CTexture::LoadTextureBitsFromFile couldn't find %s\n", pCacheFileName );
			}
			DevWarning( "\"%s\": can't be found on disk\n", pCacheFileName );
		}

		return false;
	}

	return true;
}


// Get the shaderapi texture handle associated w/ a particular frame
ShaderAPITextureHandle_t CTexture::GetTextureHandle( int nFrame, int nTextureChannel )
{
	if ( nFrame < 0 )
	{
		nFrame = 0;
		Warning( "CTexture::GetTextureHandle(): nFrame is < 0!\n" );
	}
	if ( nFrame >= m_nFrameCount )
	{
		// NOTE: This can happen during alt-tab.  If you alt-tab while loading a level then the first local cubemap bind will do this, for example.
		Assert( nFrame < m_nFrameCount );
		return INVALID_SHADERAPI_TEXTURE_HANDLE;
	}
	Assert( nTextureChannel < 2 );

	// Make sure we've actually allocated the texture handles
	Assert( m_pTextureHandles );
	Assert( HasBeenAllocated() );
	if ( m_pTextureHandles == NULL || !HasBeenAllocated() )
	{
		return INVALID_SHADERAPI_TEXTURE_HANDLE;
	}

	// Don't get paired handle here...callers of this function don't know about paired textures
	return m_pTextureHandles[nFrame];
}

void CTexture::GetLowResColorSample( float s, float t, float *color ) const
{
	if ( m_LowResImageWidth <= 0 || m_LowResImageHeight <= 0 )
	{
//		Warning( "Programming error: GetLowResColorSample \"%s\": %dx%d\n", m_pName, ( int )m_LowResImageWidth, ( int )m_LowResImageHeight );
		return;
	}

	// force s and t into [0,1)
	if ( s < 0.0f )
	{
		s = ( 1.0f - ( float )( int )s ) + s;
	}
	if ( t < 0.0f )
	{
		t = ( 1.0f - ( float )( int )t ) + t;
	}
	s = s - ( float )( int )s;
	t = t - ( float )( int )t;
	
	s *= m_LowResImageWidth;
	t *= m_LowResImageHeight;
	
	int wholeS, wholeT;
	wholeS = ( int )s;
	wholeT = ( int )t;
	float fracS, fracT;
	fracS = s - ( float )( int )s;
	fracT = t - ( float )( int )t;
	
	// filter twice in the s dimension.
	float sColor[2][3];
	int wholeSPlusOne = ( wholeS + 1 ) % m_LowResImageWidth;
	int wholeTPlusOne = ( wholeT + 1 ) % m_LowResImageHeight;
	sColor[0][0] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeT * m_LowResImageWidth ) * 3 + 0] * ( 1.0f / 255.0f ) );
	sColor[0][1] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeT * m_LowResImageWidth ) * 3 + 1] * ( 1.0f / 255.0f ) );
	sColor[0][2] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeT * m_LowResImageWidth ) * 3 + 2] * ( 1.0f / 255.0f ) );
	sColor[0][0] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeT * m_LowResImageWidth ) * 3 + 0] * ( 1.0f / 255.0f ) );
	sColor[0][1] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeT * m_LowResImageWidth ) * 3 + 1] * ( 1.0f / 255.0f ) );
	sColor[0][2] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeT * m_LowResImageWidth ) * 3 + 2] * ( 1.0f / 255.0f ) );

	sColor[1][0] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeTPlusOne * m_LowResImageWidth ) * 3 + 0] * ( 1.0f / 255.0f ) );
	sColor[1][1] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeTPlusOne * m_LowResImageWidth ) * 3 + 1] * ( 1.0f / 255.0f ) );
	sColor[1][2] = ( 1.0f - fracS ) * ( m_pLowResImage[( wholeS + wholeTPlusOne * m_LowResImageWidth ) * 3 + 2] * ( 1.0f / 255.0f ) );
	sColor[1][0] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeTPlusOne * m_LowResImageWidth ) * 3 + 0] * ( 1.0f / 255.0f ) );
	sColor[1][1] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeTPlusOne * m_LowResImageWidth ) * 3 + 1] * ( 1.0f / 255.0f ) );
	sColor[1][2] += fracS * ( m_pLowResImage[( wholeSPlusOne + wholeTPlusOne * m_LowResImageWidth ) * 3 + 2] * ( 1.0f / 255.0f ) );

	color[0] = sColor[0][0] * ( 1.0f - fracT ) + sColor[1][0] * fracT;
	color[1] = sColor[0][1] * ( 1.0f - fracT ) + sColor[1][1] * fracT;
	color[2] = sColor[0][2] * ( 1.0f - fracT ) + sColor[1][2] * fracT;
}

int CTexture::GetApproximateVidMemBytes( void ) const
{
	ImageFormat format = GetImageFormat();
	int width = GetActualWidth();
	int height = GetActualHeight();
	int depth = GetActualDepth();
	int numFrames = GetNumAnimationFrames();
	bool isMipmapped = IsMipmapped();

	return numFrames * ImageLoader::GetMemRequired( width, height, depth, format, isMipmapped );
}

void CTexture::CopyFrameBufferToMe( int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	Assert( m_pTextureHandles && m_nFrameCount >= 1 );

	if ( m_pTextureHandles && m_nFrameCount >= 1 )
	{
		g_pShaderAPI->CopyRenderTargetToTextureEx( m_pTextureHandles[0], nRenderTargetID, pSrcRect, pDstRect );
	}
}

void CTexture::CopyMeToFrameBuffer( int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	Assert( m_pTextureHandles && m_nFrameCount >= 1 );

	if ( m_pTextureHandles && m_nFrameCount >= 1 )
	{
		g_pShaderAPI->CopyTextureToRenderTargetEx( nRenderTargetID, m_pTextureHandles[0], pSrcRect, pDstRect );
	}
}

ITexture *CTexture::GetEmbeddedTexture( int nIndex )
{
	return ( nIndex == 0 ) ? this : NULL;
}

void CTexture::DeleteIfUnreferenced()
{
	if ( m_nRefCount > 0 )
		return;

	if ( ThreadInMainThread() )
	{
		// Render thread better not be active or bad things can happen.
		Assert( MaterialSystem()->GetRenderThreadId() == 0xFFFFFFFF );
		TextureManager()->RemoveTexture( this );
		return;
	}

	// Can't actually clean up from render thread--just safely mark this texture as
	// one we should check for cleanup next EndFrame when it's safe.
	TextureManager()->MarkUnreferencedTextureForCleanup( this );	
}

//Swap everything about a texture except the name. Created to support Portal mod's need for swapping out water render targets in recursive stencil views
void CTexture::SwapContents( ITexture *pOther )
{
	if( (pOther == NULL) || (pOther == this) )
		return;

	ICallQueue *pCallQueue = materials->GetRenderContext()->GetCallQueue();
	if ( pCallQueue )
	{
		pCallQueue->QueueCall( this, &CTexture::SwapContents, pOther );
		return;
	}

	AssertMsg( dynamic_cast<CTexture *>(pOther) != NULL, "Texture swapping broken" );

	CTexture *pOtherAsCTexture = (CTexture *)pOther;

	CTexture *pTemp = (CTexture *)stackalloc( sizeof( CTexture ) );
	
	//swap everything. Note that this copies the entire object including the
	// vtable pointer, thus ruining polymorphism. Use with care.
	// The unnecessary casts to (void*) hint to clang that we know what we
	// are doing.
	memcpy( (void*)pTemp, (const void*)this, sizeof( CTexture ) );
	memcpy( (void*)this, (const void*)pOtherAsCTexture, sizeof( CTexture ) );
	memcpy( (void*)pOtherAsCTexture, (const void*)pTemp, sizeof( CTexture ) );

	//we have the other's name, give it back
	memcpy( &pOtherAsCTexture->m_Name, &m_Name, sizeof( m_Name ) );

	//pTemp still has our name
	memcpy( &m_Name, &pTemp->m_Name, sizeof( m_Name ) );
}

void CTexture::MarkAsPreloaded( bool bSet )
{
	if ( bSet )
	{
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_PRELOADED;
	}
	else
	{
		m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_PRELOADED;
	}
}

bool CTexture::IsPreloaded() const
{
	return ( ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_PRELOADED ) != 0 );
}

void CTexture::MarkAsExcluded( bool bSet, int nDimensionsLimit )
{
	if ( bSet )
	{
		// exclusion trumps picmipping
		m_nInternalFlags |= TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE;
		m_nDesiredDimensionLimit = 0;
	}
	else
	{
		// not excluding, but can optionally picmip
		m_nInternalFlags &= ~TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE;
		m_nDesiredDimensionLimit = nDimensionsLimit;
	}
}

bool CTexture::UpdateExcludedState( void )
{
	bool bDesired = ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_SHOULDEXCLUDE ) != 0;
	bool bActual = ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_EXCLUDED ) != 0;
	if ( ( bDesired == bActual ) && ( m_nDesiredDimensionLimit == m_nActualDimensionLimit ) )
	{
 		return false;
	}

	if ( m_nInternalFlags & TEXTUREFLAGSINTERNAL_QUEUEDLOAD )
	{
		// already scheduled
		return true;
	}

	// force the texture to re-download, causes the texture bits to match its desired exclusion state
	Download();

	return true;
}

void CTextureStreamingJob::OnAsyncFindComplete( ITexture* pTex, void* pExtraArgs )
{
	const int cArgsAsInt = ( int ) pExtraArgs;

	Assert( m_pOwner == NULL || m_pOwner == pTex );
	if ( m_pOwner )
		m_pOwner->OnStreamingJobComplete( static_cast<ResidencyType_t>( cArgsAsInt ) );

	// OnStreamingJobComplete should've cleaned us up
	Assert( m_pOwner == NULL );
}

// ------------------------------------------------------------------------------------------------
int GetThreadId()
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	// Turns the current thread into a 0-based index for use in accessing statics in this file.
	int retVal = INT_MAX;
	if ( ThreadInMainThread() )
		retVal = 0;
	else if ( MaterialSystem()->GetRenderThreadId() == ThreadGetCurrentId() )
		retVal = 1;
	else if ( TextureManager()->ThreadInAsyncLoadThread() )
		retVal = 2;
	else if ( TextureManager()->ThreadInAsyncReadThread() )
		retVal = 3;
	else
	{
		STAGING_ONLY_EXEC( AssertAlways( !"Unexpected thread in GetThreadId, need to debug this--crash is next. Tell McJohn." ) );
		DebuggerBreakIfDebugging_StagingOnly();
	}
	
	Assert( retVal < MAX_RENDER_THREADS );
	return retVal;
}

// ------------------------------------------------------------------------------------------------
bool SLoadTextureBitsFromFile( IVTFTexture **ppOutVtfTexture, FileHandle_t hFile, unsigned int nFlags, 
							   TextureLODControlSettings_t* pInOutCachedFileLodSettings, 
							   int nDesiredDimensionLimit, unsigned short* pOutStreamedMips, 
							   const char* pName, const char* pCacheFileName, 
							   TexDimensions_t* pOptOutDimsMapping,
							   TexDimensions_t* pOptOutDimsActual, 
							   TexDimensions_t* pOptOutDimsAllocated,
							   unsigned int* pOptOutStripFlags )
{
	// NOTE! NOTE! NOTE! If you are making changes to this function, be aware that it has threading
	// NOTE! NOTE! NOTE! implications. It can be called synchronously by the Main thread, 
	// NOTE! NOTE! NOTE! or by the streaming texture code!
	Assert( ppOutVtfTexture != NULL && *ppOutVtfTexture != NULL );

	if ( V_strstr( pName, "c_rocketlauncher/c_rocketlauncher" ) )
	{
		int i = 0;
		i = 3;
	}

	CUtlBuffer buf;

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - ReadHeaderFromFile", __FUNCTION__ );
		int nHeaderSize = VTFFileHeaderSize( VTF_MAJOR_VERSION );

		// restrict read to the header only!
		// header provides info to avoid reading the entire file
		int nBytesOptimalRead = GetOptimalReadBuffer( &buf, hFile, nHeaderSize );
		int nBytesRead = g_pFullFileSystem->ReadEx( buf.Base(), nBytesOptimalRead, Min( nHeaderSize, ( int ) g_pFullFileSystem->Size( hFile ) ), hFile ); // only read as much as the file has
		buf.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );
		nBytesRead = nHeaderSize = ( ( VTFFileBaseHeader_t * ) buf.Base() )->headerSize;
		g_pFullFileSystem->Seek( hFile, nHeaderSize, FILESYSTEM_SEEK_HEAD );
	}

	// Unserialize the header only
	// need the header first to determine remainder of data
	if ( !( *ppOutVtfTexture )->Unserialize( buf, true ) )
	{
		Warning( "Error reading texture header \"%s\"\n", pCacheFileName );
		return false;
	}

	// Need to record this now, before we ask for the trimmed down data to potentially be loaded.
	TexDimensions_t dimsMappingCurrent( ( *ppOutVtfTexture )->Width(), ( *ppOutVtfTexture )->Height(), ( *ppOutVtfTexture )->MipCount(), ( *ppOutVtfTexture )->Depth() );
	if ( pOptOutDimsMapping )
		( *pOptOutDimsMapping ) = dimsMappingCurrent;


	int nFullFlags = ( *ppOutVtfTexture )->Flags() 
		           | nFlags;

	// Seek the reading back to the front of the buffer
	buf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

	// Compute the actual texture dimensions
	int nMipSkipCount = ComputeMipSkipCount( pName, dimsMappingCurrent, false, *ppOutVtfTexture, nFullFlags, nDesiredDimensionLimit, pOutStreamedMips, pInOutCachedFileLodSettings, pOptOutDimsActual, pOptOutDimsAllocated, pOptOutStripFlags );
	
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - ReadDataFromFile", __FUNCTION__ );

	// Determine how much of the file to read in
	int nFileSize = ( *ppOutVtfTexture )->FileSize( nMipSkipCount );
	int nActualFileSize = (int)g_pFullFileSystem->Size( hFile );
	if ( nActualFileSize < nFileSize )
	{
		if ( mat_spew_on_texture_size.GetInt() )
			DevMsg( "Bad VTF data for %s, expected file size:%d actual file size:%d \n", pCacheFileName, nFileSize, nActualFileSize );
		nFileSize = nActualFileSize;
	}

	// Read only the portion of the file that we care about
	g_pFullFileSystem->Seek( hFile, 0, FILESYSTEM_SEEK_HEAD );
	int nBytesOptimalRead = GetOptimalReadBuffer( &buf, hFile, nFileSize );
	int nBytesRead = g_pFullFileSystem->ReadEx( buf.Base(), nBytesOptimalRead, nFileSize, hFile );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );

	// Some hardware doesn't support copying textures to other textures. For them, we need to reread the 
	// whole file, so if they are doing the final read (the fine levels) then reread everything by stripping
	// off the flags we are trying to pass in.
	unsigned int nForceFlags = nFullFlags & TEXTUREFLAGS_STREAMABLE;
	if ( !HardwareConfig()->CanStretchRectFromTextures() && ( nForceFlags & TEXTUREFLAGS_STREAMABLE_FINE ) )
		nForceFlags = 0;

	// NOTE: Skipping mip levels here will cause the size to be changed
	bool bRetVal = ( *ppOutVtfTexture )->UnserializeEx( buf, false, nForceFlags, nMipSkipCount );

	FreeOptimalReadBuffer( 6*1024*1024 );

	if ( !bRetVal )
	{
		Warning( "Error reading texture data \"%s\"\n", pCacheFileName );
	}

	return bRetVal;
}

//-----------------------------------------------------------------------------
// Compute the actual mip count based on the actual size
//-----------------------------------------------------------------------------
int ComputeActualMipCount( const TexDimensions_t& actualDims, unsigned int nFlags )
{
	if ( nFlags & TEXTUREFLAGS_ENVMAP )
	{
		if ( !HardwareConfig()->SupportsMipmappedCubemaps() )
		{
			return 1;
		}
	}

	if ( nFlags & TEXTUREFLAGS_NOMIP )
	{
		return 1;
	}

	// Unless ALLMIPS is set, we stop mips at 32x32
	const int nMaxMipSize = 32;
	// Clamp border textures on Posix to fix L4D2 flashlight cookie issue
#ifdef DX_TO_GL_ABSTRACTION
	if ( ( false && !g_bForceTextureAllMips && !( nFlags & TEXTUREFLAGS_ALL_MIPS ) ) || ( true && ( nFlags & TEXTUREFLAGS_BORDER ) ) )
#else
	if ( ( true && !g_bForceTextureAllMips && !( nFlags & TEXTUREFLAGS_ALL_MIPS ) ) || ( false && ( nFlags & TEXTUREFLAGS_BORDER ) ) )
#endif
	{
		int nNumMipLevels = 1;
		int h = actualDims.m_nWidth;
		int w = actualDims.m_nHeight;
		while ( MIN( w, h ) > nMaxMipSize )
		{
			++nNumMipLevels;

			w >>= 1;
			h >>= 1;
		}
		return nNumMipLevels;
	}

	return ImageLoader::GetNumMipMapLevels( actualDims.m_nWidth, actualDims.m_nHeight, actualDims.m_nDepth );
}

// ------------------------------------------------------------------------------------------------
int ComputeMipSkipCount( const char* pName, const TexDimensions_t& mappingDims, bool bIgnorePicmip, IVTFTexture *pOptVTFTexture, unsigned int nFlags, int nDesiredDimensionLimit, unsigned short* pOutStreamedMips, TextureLODControlSettings_t* pInOutCachedFileLodSettings, TexDimensions_t* pOptOutActualDims, TexDimensions_t* pOptOutAllocatedDims, unsigned int* pOptOutStripFlags )
{
	// NOTE! NOTE! NOTE! If you are making changes to this function, be aware that it has threading
	// NOTE! NOTE! NOTE! implications. It can be called synchronously by the Main thread, 
	// NOTE! NOTE! NOTE! or by the streaming texture code!

	Assert( pName != NULL );
	Assert( pOutStreamedMips != NULL );
	Assert( pInOutCachedFileLodSettings != NULL );

	TexDimensions_t actualDims = mappingDims,
		            allocatedDims;

	const bool bTextureMigration = ( nFlags & TEXTUREFLAGS_STREAMABLE ) != 0;
	unsigned int stripFlags = 0;

	int nClampX = actualDims.m_nWidth;	// no clamping (clamp to texture dimensions)
	int nClampY = actualDims.m_nHeight;
	int nClampZ = actualDims.m_nDepth;

	// Fetch LOD settings from the VTF if available
	TextureLODControlSettings_t lcs;
	memset( &lcs, 0, sizeof( lcs ) );
	TextureLODControlSettings_t const *pLODInfo = NULL;
	if ( pOptVTFTexture )
	{
		pLODInfo = reinterpret_cast<TextureLODControlSettings_t const *> (
				pOptVTFTexture->GetResourceData( VTF_RSRC_TEXTURE_LOD_SETTINGS, NULL ) );

		// Texture streaming means there are times we call this where we don't have a VTFTexture, even though 
		// we're a file. So we need to store off the LOD settings whenever we get in here with a file that has them
		// so that we can use the correct values for when we don't. Otherwise, the texture will be confused about
		// what size to use and everything will die a horrible, horrible death.
		if ( pLODInfo )
			( *pInOutCachedFileLodSettings ) = ( *pLODInfo );
	}
	else if ( bTextureMigration ) 
	{
		pLODInfo = pInOutCachedFileLodSettings;
	}

	if ( pLODInfo )
		lcs = *pLODInfo;

	// Prepare the default LOD settings (that essentially result in no clamping)
	TextureLODControlSettings_t default_lod_settings;
	memset( &default_lod_settings, 0, sizeof( default_lod_settings ) );
	{
		for ( int w = actualDims.m_nWidth; w > 1; w >>= 1 )
				++ default_lod_settings.m_ResolutionClampX;
		for ( int h = actualDims.m_nHeight; h > 1; h >>= 1 )
				++ default_lod_settings.m_ResolutionClampY;
	}

	// Check for LOD control override
	{
		TextureLodOverride::OverrideInfo oi = TextureLodOverride::Get( pName );
			
		if ( oi.x && oi.y && !pLODInfo )	// If overriding texture that doesn't have lod info yet, then use default
			lcs = default_lod_settings;

		lcs.m_ResolutionClampX += oi.x;
		lcs.m_ResolutionClampY += oi.y;
		if ( int8( lcs.m_ResolutionClampX ) < 0 )
			lcs.m_ResolutionClampX = 0;
		if ( int8( lcs.m_ResolutionClampY ) < 0 )
			lcs.m_ResolutionClampY = 0;
	}

	// Compute the requested mip0 dimensions
	if ( lcs.m_ResolutionClampX && lcs.m_ResolutionClampY )
	{
		nClampX = (1 << lcs.m_ResolutionClampX );
		nClampY = (1 << lcs.m_ResolutionClampY );
	}

	// In case clamp values exceed texture dimensions, then fix up
	// the clamping values
	nClampX = min( nClampX, (int)actualDims.m_nWidth );
	nClampY = min( nClampY, (int)actualDims.m_nHeight );

	//
	// Honor dimension limit restrictions
	//
	if ( nDesiredDimensionLimit > 0 )
	{
		while ( nClampX > nDesiredDimensionLimit ||
				nClampY > nDesiredDimensionLimit )
		{
			nClampX >>= 1;
			nClampY >>= 1;
		}
	}

	//
	// Unless ignoring picmip, reflect the global picmip level in clamp dimensions
	//
	if ( !bIgnorePicmip )
	{
		// If picmip requests texture degradation, then honor it
		// for loddable textures only
		if ( !( nFlags & TEXTUREFLAGS_NOLOD ) &&
			  ( g_config.skipMipLevels > 0 ) )
		{
			for ( int iDegrade = 0; iDegrade < g_config.skipMipLevels; ++ iDegrade )
			{
				// don't go lower than 4, or dxt textures won't work properly
				if ( nClampX > 4 &&
					 nClampY > 4 )
				{
					nClampX >>= 1;
					nClampY >>= 1;
				}
			}
		}

		// If picmip requests quality upgrade, then always honor it
		if ( g_config.skipMipLevels < 0 )
		{
			for ( int iUpgrade = 0; iUpgrade < - g_config.skipMipLevels; ++ iUpgrade )
			{
				if ( nClampX < actualDims.m_nWidth &&
					 nClampY < actualDims.m_nHeight )
				{
					nClampX <<= 1;
					nClampY <<= 1;
				}
				else
					break;
			}
		}
	}

	//
	// Now use hardware settings to clamp our "clamping dimensions"
	//
	int iHwWidth = HardwareConfig()->MaxTextureWidth();
	int iHwHeight = HardwareConfig()->MaxTextureHeight();
	int iHwDepth = HardwareConfig()->MaxTextureDepth();

	nClampX = min( nClampX, max( iHwWidth, 4 ) );
	nClampY = min( nClampY, max( iHwHeight, 4 ) );
	nClampZ = min( nClampZ, max( iHwDepth, 1 ) );

	// In case clamp values exceed texture dimensions, then fix up
	// the clamping values.
	nClampX = min( nClampX, (int)actualDims.m_nWidth );
	nClampY = min( nClampY, (int)actualDims.m_nHeight );
	nClampZ = min( nClampZ, (int)actualDims.m_nDepth );
	
	//
	// Clamp to the determined dimensions
	//
	int numMipsSkipped = 0; // will compute now when clamping how many mips we drop
	while ( ( actualDims.m_nWidth  > nClampX ) ||
		    ( actualDims.m_nHeight > nClampY ) ||
			( actualDims.m_nDepth  > nClampZ ) )
	{
		actualDims.m_nWidth  >>= 1;
		actualDims.m_nHeight >>= 1;
		actualDims.m_nDepth = Max( 1, actualDims.m_nDepth >> 1 );

		++ numMipsSkipped;
	}

	Assert( actualDims.m_nWidth > 0 && actualDims.m_nHeight > 0 && actualDims.m_nDepth > 0 );

	// Now that we've got the actual size, we can figure out the mip count
	actualDims.m_nMipCount = ComputeActualMipCount( actualDims, nFlags );

	// If we're streaming, cut down what we're loading.
	// We can only stream things that have a mipmap pyramid (not just a single mipmap).
	bool bHasSetAllocation = false;
	if ( ( nFlags & TEXTUREFLAGS_STREAMABLE ) == TEXTUREFLAGS_STREAMABLE_COARSE )
	{
		if ( actualDims.m_nMipCount > 1 )
		{
			allocatedDims.m_nWidth    = actualDims.m_nWidth;
			allocatedDims.m_nHeight   = actualDims.m_nHeight;
			allocatedDims.m_nDepth    = actualDims.m_nDepth;
			allocatedDims.m_nMipCount = actualDims.m_nMipCount;

			for ( int i = 0; i < STREAMING_START_MIPMAP; ++i ) 
			{
				// Stop when width or height is at 4 pixels (or less). We could do better, 
				// but some textures really can't function if they're less than 4 pixels (compressed textures, for example).
				if ( allocatedDims.m_nWidth <= 4 || allocatedDims.m_nHeight <= 4 )
					break;

				allocatedDims.m_nWidth  >>= 1;
				allocatedDims.m_nHeight >>= 1;
				allocatedDims.m_nDepth    = Max( 1, allocatedDims.m_nDepth  >> 1 );
				allocatedDims.m_nMipCount = Max( 1, allocatedDims.m_nMipCount - 1 );

				++numMipsSkipped;
				++( *pOutStreamedMips );
			}

			bHasSetAllocation = true;
		}
		else
		{
			// Clear out that we're streaming, this isn't a texture we can stream.
			stripFlags |= TEXTUREFLAGS_STREAMABLE_COARSE;
		}
	} 

	if ( !bHasSetAllocation )
	{
		allocatedDims.m_nWidth    = actualDims.m_nWidth;
		allocatedDims.m_nHeight   = actualDims.m_nHeight;
		allocatedDims.m_nDepth    = actualDims.m_nDepth;
		allocatedDims.m_nMipCount = actualDims.m_nMipCount;
	}

	if ( pOptOutActualDims )
		*pOptOutActualDims = actualDims;

	if ( pOptOutAllocatedDims )
		*pOptOutAllocatedDims = allocatedDims;

	if ( pOptOutStripFlags )
		( *pOptOutStripFlags ) = stripFlags;
		
	// Returns the number we skipped
	return numMipsSkipped;
}

//-----------------------------------------------------------------------------
// Get an optimal read buffer, persists and avoids excessive allocations
//-----------------------------------------------------------------------------
int GetOptimalReadBuffer( CUtlBuffer* pOutOptimalBuffer, FileHandle_t hFile, int nSize )
{
	// NOTE! NOTE! NOTE! If you are making changes to this function, be aware that it has threading
	// NOTE! NOTE! NOTE! implications. It can be called synchronously by the Main thread, 
	// NOTE! NOTE! NOTE! or by the streaming texture code!
	Assert( GetThreadId() < MAX_RENDER_THREADS );

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s (%d bytes)", __FUNCTION__, nSize );
	Assert( pOutOptimalBuffer != NULL );

	// get an optimal read buffer, only resize if necessary
	const int minSize = 2 * 1024 * 1024;	// Uses 2MB min to avoid fragmentation
	nSize = max( nSize, minSize );
	int nBytesOptimalRead = g_pFullFileSystem->GetOptimalReadSize( hFile, nSize );

	const int ti = GetThreadId();

	if ( nBytesOptimalRead > s_nOptimalReadBufferSize[ ti ] )
	{
		FreeOptimalReadBuffer( 0 );

		s_nOptimalReadBufferSize[ ti ] = nBytesOptimalRead;
		s_pOptimalReadBuffer[ ti ] = g_pFullFileSystem->AllocOptimalReadBuffer( hFile, nSize );
		if ( mat_spewalloc.GetBool() )
		{
			Msg( "Allocated optimal read buffer of %d bytes @ 0x%p for thread %d\n", s_nOptimalReadBufferSize[ ti ], s_pOptimalReadBuffer[ ti ], ti );
		}
	}

	// set external buffer and reset to empty
	( *pOutOptimalBuffer ).SetExternalBuffer( s_pOptimalReadBuffer[ ti ], s_nOptimalReadBufferSize[ ti ], 0 );

	// return the optimal read size
	return nBytesOptimalRead;
}

//-----------------------------------------------------------------------------
// Free the optimal read buffer if it grows too large
//-----------------------------------------------------------------------------
void FreeOptimalReadBuffer( int nMaxSize )
{
	// NOTE! NOTE! NOTE! If you are making changes to this function, be aware that it has threading
	// NOTE! NOTE! NOTE! implications. It can be called synchronously by the Main thread, 
	// NOTE! NOTE! NOTE! or by the streaming texture code!
	Assert( GetThreadId() < MAX_RENDER_THREADS );

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	const int ti = GetThreadId();

	if ( s_pOptimalReadBuffer[ ti ] && s_nOptimalReadBufferSize[ ti ] >= nMaxSize )
	{
		if ( mat_spewalloc.GetBool() )
		{
			Msg( "Freeing optimal read buffer of %d bytes @ 0x%p for thread %d\n", s_nOptimalReadBufferSize[ ti ], s_pOptimalReadBuffer[ ti ], ti );
		}
		g_pFullFileSystem->FreeOptimalReadBuffer( s_pOptimalReadBuffer[ ti ] );
		s_pOptimalReadBuffer[ ti ] = NULL;
		s_nOptimalReadBufferSize[ ti ] = 0;
	}
}


#if defined( STAGING_ONLY )
	CON_COMMAND( dumptexallocs, "List currently allocated textures and properties about them" )
	{
		Msg( "Texture Memory Statistics follow:\n" );
		uint64 totalTexMemAllocated = 0;
		FOR_EACH_MAP_FAST( g_currentTextures, i )
		{
			const TexInfo_t& tex = g_currentTextures[ i ];
			uint64 thisTexMem = tex.ComputeTexSize();

			totalTexMemAllocated += thisTexMem;
			Msg( "%s: %llu bytes\n", ( const char * ) tex.m_Name, thisTexMem );
		}

		Msg( "Total Memory Allocated: %llu bytes\n", totalTexMemAllocated );
	}
#endif
	

//////////////////////////////////////////////////////////////////////////
//
// Saving all the texture LOD modifications to content
//
//////////////////////////////////////////////////////////////////////////

#ifdef IS_WINDOWS_PC
static bool SetBufferValue( char *chTxtFileBuffer, char const *szLookupKey, char const *szNewValue )
{
	bool bResult = false;
	
	size_t lenTmp = strlen( szNewValue );
	size_t nTxtFileBufferLen = strlen( chTxtFileBuffer );
	
	for ( char *pch = chTxtFileBuffer;
		( NULL != ( pch = strstr( pch, szLookupKey ) ) );
		++ pch )
	{
		char *val = pch + strlen( szLookupKey );
		if ( !V_isspace( *val ) )
			continue;
		else
			++ val;
		char *pValStart = val;

		// Okay, here comes the value
		while ( *val && V_isspace( *val ) )
			++ val;
		while ( *val && !V_isspace( *val ) )
			++ val;

		char *pValEnd = val; // Okay, here ends the value

		memmove( pValStart + lenTmp, pValEnd, chTxtFileBuffer + nTxtFileBufferLen + 1 - pValEnd );
		memcpy( pValStart, szNewValue, lenTmp );

		nTxtFileBufferLen += ( lenTmp - ( pValEnd - pValStart ) );
		bResult = true;
	}

	if ( !bResult )
	{
		char *pchAdd = chTxtFileBuffer + nTxtFileBufferLen;
		strcpy( pchAdd + strlen( pchAdd ), "\n" );
		strcpy( pchAdd + strlen( pchAdd ), szLookupKey );
		strcpy( pchAdd + strlen( pchAdd ), " " );
		strcpy( pchAdd + strlen( pchAdd ), szNewValue );
		strcpy( pchAdd + strlen( pchAdd ), "\n" );
		bResult = true;
	}

	return bResult;
}

// Replaces the first occurrence of "szFindData" with "szNewData"
// Returns the remaining buffer past the replaced data or NULL if
// no replacement occurred.
static char * BufferReplace( char *buf, char const *szFindData, char const *szNewData )
{
	size_t len = strlen( buf ), lFind = strlen( szFindData ), lNew = strlen( szNewData );
	if ( char *pBegin = strstr( buf, szFindData ) )
	{
		memmove( pBegin + lNew, pBegin + lFind, buf + len - ( pBegin + lFind ) );
		memmove( pBegin, szNewData, lNew );
		return pBegin + lNew;
	}
	return NULL;
}


class CP4Requirement
{
public:
	CP4Requirement();
	~CP4Requirement();

protected:
	bool m_bLoadedModule;
	CSysModule *m_pP4Module;
};

CP4Requirement::CP4Requirement() :
	m_bLoadedModule( false ),
	m_pP4Module( NULL )
{
#ifdef STAGING_ONLY
	if ( p4 )
		return;

	// load the p4 lib
	m_pP4Module = Sys_LoadModule( "p4lib" );
	m_bLoadedModule = true;
		
	if ( m_pP4Module )
	{
		CreateInterfaceFn factory = Sys_GetFactory( m_pP4Module );
		if ( factory )
		{
			p4 = ( IP4 * )factory( P4_INTERFACE_VERSION, NULL );

			if ( p4 )
			{
				extern CreateInterfaceFn g_fnMatSystemConnectCreateInterface;
				p4->Connect( g_fnMatSystemConnectCreateInterface );
				p4->Init();
			}
		}
	}
#endif // STAGING_ONLY

	if ( !p4 )
	{
		Warning( "Can't load p4lib.dll\n" );
	}
}

CP4Requirement::~CP4Requirement()
{
	if ( m_bLoadedModule && m_pP4Module )
	{
		if ( p4 )
		{
			p4->Shutdown();
			p4->Disconnect();
		}

		Sys_UnloadModule( m_pP4Module );
		m_pP4Module = NULL;
		p4 = NULL;
	}
}

static ConVar mat_texture_list_content_path( "mat_texture_list_content_path", "", FCVAR_ARCHIVE, "The content path to the materialsrc directory. If left unset, it'll assume your content directory is next to the currently running game dir." );

CON_COMMAND_F( mat_texture_list_txlod_sync, "'reset' - resets all run-time changes to LOD overrides, 'save' - saves all changes to material content files", FCVAR_DONTRECORD )
{
	using namespace TextureLodOverride;

	if ( args.ArgC() != 2 )
		goto usage;

	char const *szCmd = args.Arg( 1 );
	Msg( "mat_texture_list_txlod_sync %s...\n", szCmd );

	if ( !stricmp( szCmd, "reset" ) )
	{
		for ( int k = 0; k < s_OverrideMap.GetNumStrings(); ++ k )
		{
			char const *szTx = s_OverrideMap.String( k );
			s_OverrideMap[ k ] = OverrideInfo(); // Reset the override info

			// Force the texture LOD override to get re-processed
			if ( ITexture *pTx = materials->FindTexture( szTx, "" ) )
				pTx->ForceLODOverride( 0 );
			else
				Warning( " mat_texture_list_txlod_sync reset - texture '%s' no longer found.\n", szTx );
		}

		s_OverrideMap.Purge();
		Msg("mat_texture_list_txlod_sync reset : completed.\n");
		return;
	}
	else if ( !stricmp( szCmd, "save" ) )
	{
		CP4Requirement p4req;
		if ( !p4 )
			g_p4factory->SetDummyMode( true );

		for ( int k = 0; k < s_OverrideMap.GetNumStrings(); ++ k )
		{
			char const *szTx = s_OverrideMap.String( k );
			OverrideInfo oi = s_OverrideMap[ k ];
			ITexture *pTx = materials->FindTexture( szTx, "" );
			
			if ( !oi.x || !oi.y )
				continue;

			if ( !pTx )
			{
				Warning( " mat_texture_list_txlod_sync save - texture '%s' no longer found.\n", szTx );
				continue;
			}

			int iMaxWidth = pTx->GetActualWidth(), iMaxHeight = pTx->GetActualHeight();
			
			// Save maxwidth and maxheight
			char chMaxWidth[20], chMaxHeight[20];
			sprintf( chMaxWidth, "%d", iMaxWidth ), sprintf( chMaxHeight, "%d", iMaxHeight );

			// We have the texture and path to its content
			char chResolveName[ MAX_PATH ] = {0}, chResolveNameArg[ MAX_PATH ] = {0};
			Q_snprintf( chResolveNameArg, sizeof( chResolveNameArg ) - 1, "materials/%s" TEXTURE_FNAME_EXTENSION, szTx );
			char *szTextureContentPath;
			if ( !mat_texture_list_content_path.GetString()[0] )
			{
				szTextureContentPath = const_cast< char * >( g_pFullFileSystem->RelativePathToFullPath( chResolveNameArg, "game", chResolveName, sizeof( chResolveName ) - 1 ) );

				if ( !szTextureContentPath )
				{
					Warning( " mat_texture_list_txlod_sync save - texture '%s' is not loaded from file system.\n", szTx );
					continue;
				}
				if ( !BufferReplace( szTextureContentPath, "\\game\\", "\\content\\" ) ||
					 !BufferReplace( szTextureContentPath, "\\materials\\", "\\materialsrc\\" ) )
				{
					Warning( " mat_texture_list_txlod_sync save - texture '%s' cannot be mapped to content directory.\n", szTx );
					continue;
				}
			}
			else
			{
				V_strncpy( chResolveName, mat_texture_list_content_path.GetString(), MAX_PATH );
				V_strncat( chResolveName, "/", MAX_PATH );
				V_strncat( chResolveName, szTx, MAX_PATH );
				V_strncat( chResolveName, TEXTURE_FNAME_EXTENSION, MAX_PATH );

				szTextureContentPath = chResolveName;
			}

			// Figure out what kind of source content is there:
			// 1. look for TGA - if found, get the txt file (if txt file missing, create one)
			// 2. otherwise look for PSD - affecting psdinfo
			// 3. else error
			char *pExtPut = szTextureContentPath + strlen( szTextureContentPath ) - strlen( TEXTURE_FNAME_EXTENSION ); // compensating the TEXTURE_FNAME_EXTENSION(.vtf) extension
			
			// 1.tga
			sprintf( pExtPut, ".tga" );
			if ( g_pFullFileSystem->FileExists( szTextureContentPath ) )
			{
				// Have tga - pump in the txt file
				sprintf( pExtPut, ".txt" );
				
				CUtlBuffer bufTxtFileBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
				g_pFullFileSystem->ReadFile( szTextureContentPath, 0, bufTxtFileBuffer );
				for ( int kCh = 0; kCh < 1024; ++kCh ) bufTxtFileBuffer.PutChar( 0 );

				// Now fix maxwidth/maxheight settings
				SetBufferValue( ( char * ) bufTxtFileBuffer.Base(), "maxwidth", chMaxWidth );
				SetBufferValue( ( char * ) bufTxtFileBuffer.Base(), "maxheight", chMaxHeight );
				bufTxtFileBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, strlen( ( char * ) bufTxtFileBuffer.Base() ) );

				// Check out or add the file
				g_p4factory->SetOpenFileChangeList( "Texture LOD Autocheckout" );
				CP4AutoEditFile autop4_edit( szTextureContentPath );

				// Save the file contents
				if ( g_pFullFileSystem->WriteFile( szTextureContentPath, 0, bufTxtFileBuffer ) )
				{
					Msg(" '%s' : saved.\n", szTextureContentPath );
					CP4AutoAddFile autop4_add( szTextureContentPath );
				}
				else
				{
					Warning( " '%s' : failed to save - set \"maxwidth %d maxheight %d\" manually.\n",
						szTextureContentPath, iMaxWidth, iMaxHeight );
				}

				continue;
			}

			// 2.psd
			sprintf( pExtPut, ".psd" );
			if ( g_pFullFileSystem->FileExists( szTextureContentPath ) )
			{
				char chCommand[MAX_PATH];
				char szTxtFileName[MAX_PATH] = {0};
				GetModSubdirectory( "tmp_lod_psdinfo.txt", szTxtFileName, sizeof( szTxtFileName ) );
				sprintf( chCommand, "/C psdinfo \"%s\" > \"%s\"", szTextureContentPath, szTxtFileName);
				ShellExecute( NULL, NULL, "cmd.exe", chCommand, NULL, SW_HIDE );
				Sleep( 200 );

				CUtlBuffer bufTxtFileBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
				g_pFullFileSystem->ReadFile( szTxtFileName, 0, bufTxtFileBuffer );
				for ( int kCh = 0; kCh < 1024; ++ kCh ) bufTxtFileBuffer.PutChar( 0 );

				// Now fix maxwidth/maxheight settings
				SetBufferValue( ( char * ) bufTxtFileBuffer.Base(), "maxwidth", chMaxWidth );
				SetBufferValue( ( char * ) bufTxtFileBuffer.Base(), "maxheight", chMaxHeight );
				bufTxtFileBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, strlen( ( char * ) bufTxtFileBuffer.Base() ) );

				// Check out or add the file
				// Save the file contents
				if ( g_pFullFileSystem->WriteFile( szTxtFileName, 0, bufTxtFileBuffer ) )
				{
					g_p4factory->SetOpenFileChangeList( "Texture LOD Autocheckout" );
					CP4AutoEditFile autop4_edit( szTextureContentPath );

					sprintf( chCommand, "/C psdinfo -write \"%s\" < \"%s\"", szTextureContentPath, szTxtFileName );
					Sleep( 200 );
					ShellExecute( NULL, NULL, "cmd.exe", chCommand, NULL, SW_HIDE );
					Sleep( 200 );

					Msg(" '%s' : saved.\n", szTextureContentPath );
					CP4AutoAddFile autop4_add( szTextureContentPath );
				}
				else
				{
					Warning( " '%s' : failed to save - set \"maxwidth %d maxheight %d\" manually.\n",
						szTextureContentPath, iMaxWidth, iMaxHeight );
				}

				continue;
			}

			// 3. - error
			sprintf( pExtPut, "" );
			{
				Warning( " '%s' : doesn't specify a valid TGA or PSD file!\n", szTextureContentPath );
				continue;
			}
		}

		Msg("mat_texture_list_txlod_sync save : completed.\n");
		return;
	}
	else
		goto usage;

	return;

usage:
	Warning(
		"Usage:\n"
		"  mat_texture_list_txlod_sync reset - resets all run-time changes to LOD overrides;\n"
		"  mat_texture_list_txlod_sync save  - saves all changes to material content files.\n"
		);
}
#endif
