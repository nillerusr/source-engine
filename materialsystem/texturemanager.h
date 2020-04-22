//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header:     $
// $NoKeywords: $
//===========================================================================//

#ifndef TEXTUREMANAGER_H
#define TEXTUREMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "itextureinternal.h"
class ITexture;
class ITextureInternal;
class IVTFTexture;

enum
{
	COLOR_CORRECTION_MAX_TEXTURES = 4,
	COLOR_CORRECTION_TEXTURE_SIZE = 32
};

class CTextureCompositorTemplate;

//-----------------------------------------------------------------------------
// Texture manager interface
//-----------------------------------------------------------------------------
abstract_class ITextureManager
{
public:
	// Initialization + shutdown
	virtual void Init( int nFlags ) = 0;
	virtual void Shutdown() = 0;

	// Allocate, free standard render target textures
	virtual void AllocateStandardRenderTargets( ) = 0;
	virtual void FreeStandardRenderTargets() = 0;

	//Some render targets are managed by code outside of the materialsystem but are used by the materialsystem all the time.
	virtual void CacheExternalStandardRenderTargets() = 0;

	// Creates a procedural texture
	// NOTE: Passing in NULL as a texture name will cause it to not
	// be able to be looked up by name using FindOrLoadTexture.
	// Also, you may not get a texture with the requested size or format;
	// you'll get something close though.
	virtual ITextureInternal *CreateProceduralTexture( 
		const char			*pTextureName, 
		const char			*pTextureGroupName,
		int					w,
		int					h, 
		int					d,
		ImageFormat			fmt,
		int					nFlags,
		ITextureRegenerator *generator = NULL) = 0;

	// Creates a texture which is a render target
	virtual ITextureInternal *CreateRenderTargetTexture( 
		const char				*pRTName,	// NULL for auto-generated name
		int						w, 
		int						h, 
		RenderTargetSizeMode_t	sizeMode, 
		ImageFormat				fmt, 
		RenderTargetType_t		type, 
		unsigned int			textureFlags,
		unsigned int			renderTargetFlags ) = 0;

	// Loads a texture from disk
	virtual ITextureInternal *FindOrLoadTexture( const char *pTextureName, const char *pTextureGroupName, int nAdditionalCreationFlags = 0 ) = 0;

	// Call this to reset the filtering state
	virtual void ResetTextureFilteringState() = 0;

	// Reload all textures
	virtual void ReloadTextures( void ) = 0;

	// These two are used when we lose our video memory due to a mode switch etc
	virtual void ReleaseTextures( void ) = 0;
	virtual void RestoreRenderTargets( void ) = 0;
	virtual void RestoreNonRenderTargetTextures( void ) = 0;

	// Suspend or resume texture streaming requests
	virtual void SuspendTextureStreaming( void ) = 0;
	virtual void ResumeTextureStreaming( void ) = 0;

	// delete any texture that has a refcount <= 0
	virtual void RemoveUnusedTextures( void ) = 0;
	virtual void DebugPrintUsedTextures( void ) = 0;

	// Request a texture ID
	virtual int	RequestNextTextureID() = 0;

	// Get at a couple standard textures
	virtual ITextureInternal *ErrorTexture() = 0;
	virtual ITextureInternal *NormalizationCubemap() = 0;
	virtual ITextureInternal *SignedNormalizationCubemap() = 0;
	virtual ITextureInternal *ColorCorrectionTexture( int index ) = 0;
	virtual ITextureInternal *ShadowNoise2D() = 0;
	virtual ITextureInternal *IdentityLightWarp() = 0;
	virtual ITextureInternal *FullFrameDepthTexture() = 0;
	virtual ITextureInternal *DebugLuxels2D() = 0;

	// Generates an error texture pattern
	virtual void GenerateErrorTexture( ITexture *pTexture, IVTFTexture *pVTFTexture ) = 0;

	// Updates the color correction state
	virtual void SetColorCorrectionTexture( int i, ITextureInternal *pTexture ) = 0;

	virtual void ForceAllTexturesIntoHardware( void ) = 0;

	virtual bool IsTextureLoaded( const char *pTextureName ) = 0;

	// Mark a texture as now-unreferenced, so it can be checked for removal at a later (and thread-safe) time.
	virtual void MarkUnreferencedTextureForCleanup( ITextureInternal *pTexture ) = 0;

	virtual void RemoveTexture( ITextureInternal *pTexture ) = 0;

	// start with -1, list terminates with -1
	virtual int	FindNext( int iIndex, ITextureInternal **ppTexture ) = 0;

	virtual void AddTextureAlias( const char *pAlias, const char *pRealName ) = 0;
	virtual void RemoveTextureAlias( const char *pAlias ) = 0;

	virtual void SetExcludedTextures( const char *pScriptName ) = 0;
	virtual void UpdateExcludedTextures( void ) = 0;

	//Releases texture memory bits for temporary render targets, does NOT destroy the CTexture entirely
	virtual void ReleaseTempRenderTargetBits( void ) = 0;

	// See CL_HandlePureServerWhitelist for a description of the pure server stuff.
	virtual void ReloadFilesInList( IFileList *pFilesToReload ) = 0;

	// Called once per frame by material system "somewhere."
	virtual void Update( ) = 0;

	// Load a texture asynchronously and then call the provided callback.
	virtual void AsyncFindOrLoadTexture( const char *pTextureName, const char *pTextureGroupName, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs, bool bComplain, int nAdditionalCreationFlags ) = 0;

	// Stream a render target back to system memory, perform format conversion to the specified destination format, 
	virtual void AsyncCreateTextureFromRenderTarget( ITexture* pSrcRt, const char* pDstName, ImageFormat dstFmt, bool bGenMips, int nAdditionalCreationFlags, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs ) = 0;

	virtual void WarmTextureCache() = 0;
	virtual void CoolTextureCache() = 0;

	virtual void RequestAllMipmaps( ITextureInternal* pTex ) = 0;
	
	virtual void EvictAllTextures() = 0;

	virtual void UpdatePostAsync() = 0;

	virtual void ReleaseAsyncScratchVTF( IVTFTexture* pScratchVTF ) = 0;

	virtual bool ThreadInAsyncLoadThread() const = 0;
	virtual bool ThreadInAsyncReadThread() const = 0;

	virtual bool HasPendingTextureDestroys() const = 0;

	virtual bool AddTextureCompositorTemplate( const char* pName, KeyValues* pTmplDesc ) = 0;
	virtual bool VerifyTextureCompositorTemplates() = 0;

	virtual CTextureCompositorTemplate* FindTextureCompositorTemplate( const char* pName ) = 0;

};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
inline ITextureManager *TextureManager()
{
	extern ITextureManager *g_pTextureManager;
	return g_pTextureManager;
}


#endif // TEXTUREMANAGER_H
