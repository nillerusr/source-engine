//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef CMATERIAL_QUEUEFRIENDLY_H
#define CMATERIAL_QUEUEFRIENDLY_H

#ifdef _WIN32
#pragma once
#endif

#include "imaterialinternal.h"

class CMaterial_QueueFriendly : public IMaterialInternal //wraps a CMaterial with queue friendly functions for game/engine code. materialsystem/shaderapi code should use CMaterial directly.
{
public:
	virtual const char *	GetName() const;
	virtual const char *	GetTextureGroupName() const;
	virtual PreviewImageRetVal_t GetPreviewImageProperties( int *width, int *height, ImageFormat *imageFormat, bool* isTranslucent ) const;
	virtual PreviewImageRetVal_t GetPreviewImage( unsigned char *data, int width, int height, ImageFormat imageFormat ) const;

	virtual int				GetMappingWidth( );
	virtual int				GetMappingHeight( );
	virtual int				GetNumAnimationFrames( );
	virtual bool			InMaterialPage( void );
	virtual	void			GetMaterialOffset( float *pOffset );
	virtual void			GetMaterialScale( float *pScale );
	virtual IMaterial		*GetMaterialPage( void );
	virtual void			IncrementReferenceCount( void );
	virtual int 			GetEnumerationID( void ) const;
	virtual bool			HasProxy( void ) const;
	virtual void			GetReflectivity( Vector& reflect );
	virtual bool			GetPropertyFlag( MaterialPropertyTypes_t type );
	virtual bool			IsTwoSided();
	virtual int				ShaderParamCount() const;
	virtual bool			IsErrorMaterial() const; //should probably return the realtime error material instead of this wrapper for it
	virtual bool			IsSpriteCard(); //lets just assume nobody changes the shader to spritecard and immediately asks if it's a spritecard



	//TODO: Investigate if these are likely to change at all when setting vars/flags
	virtual bool			IsAlphaTested();
	virtual bool			IsVertexLit();
	virtual VertexFormat_t	GetVertexFormat() const;
	virtual bool			UsesEnvCubemap( void );
	virtual bool			NeedsTangentSpace( void );
	virtual bool			NeedsSoftwareSkinning( void );
	virtual int				GetNumPasses( void );
	virtual int				GetTextureMemoryBytes( void );
	virtual bool			NeedsLightmapBlendAlpha( void );
	virtual bool			NeedsSoftwareLighting( void );
	virtual MorphFormat_t	GetMorphFormat() const;

	//TODO: Investigate if this can change over the course of a frame.
	virtual void			GetLowResColorSample( float s, float t, float *color ) const;


	//Functions that need to be queue friendly, the whole reason for this wrapper class.
	virtual IMaterialVar *	FindVar( const char *varName, bool *found, bool complain = true );
	virtual IMaterialVar *	FindVarFast( char const *pVarName, unsigned int *pToken );
	virtual IMaterialVar	**GetShaderParams( void );
	virtual void			DecrementReferenceCount( void );
	virtual void			DeleteIfUnreferenced();	
	virtual void			RecomputeStateSnapshots();
	virtual bool			IsTranslucent();
	virtual bool			NeedsPowerOfTwoFrameBufferTexture( bool bCheckSpecificToThisFrame = true );
	virtual bool			NeedsFullFrameBufferTexture( bool bCheckSpecificToThisFrame = true );
	virtual void			AlphaModulate( float alpha );
	virtual void			ColorModulate( float r, float g, float b );
	virtual void			SetMaterialVarFlag( MaterialVarFlags_t flag, bool on );
	virtual bool			GetMaterialVarFlag( MaterialVarFlags_t flag ) const;
	virtual void			SetShader( const char *pShaderName );
	virtual void			SetShaderAndParams( KeyValues *pKeyValues );
	virtual const char *	GetShaderName() const;
	virtual void			Refresh();
	virtual void			RefreshPreservingMaterialVars();
	virtual void			SetUseFixedFunctionBakedLighting( bool bEnable );
	virtual float			GetAlphaModulation();
	virtual void			GetColorModulation( float *r, float *g, float *b );
	virtual void			CallBindProxy( void *proxyData );
	virtual IMaterial		*CheckProxyReplacement( void *proxyData );
	virtual void			PrecacheMappingDimensions( );
	virtual void			FindRepresentativeTexture( void );
	virtual bool			WasReloadedFromWhitelist() { return m_pRealTimeVersion->WasReloadedFromWhitelist(); }
	virtual bool			IsPrecached( ) const { return m_pRealTimeVersion->IsPrecached(); }


#define QUEUEFRIENDLY_USED_INTERNALLY_ASSERT AssertMsg( 0, "CMaterial_QueueFriendly used internally within materialsystem. Update the calling code to use a realtime CMaterial." )

	//------------------------------------------------------------------------------
	// IMaterialInternal interfaces. Internal systems should not be using this queue
	// wrapper class at all. Switch to the real time pointer in the calling code.
	//------------------------------------------------------------------------------
	virtual int		GetReferenceCount( ) const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetReferenceCount(); }
	virtual void	SetEnumerationID( int id ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->SetEnumerationID( id ); }
	virtual void	SetNeedsWhiteLightmap( bool val ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->SetNeedsWhiteLightmap( val ); }
	virtual bool	GetNeedsWhiteLightmap( ) const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetNeedsWhiteLightmap(); }
	virtual void	Uncache( bool bPreserveVars = false  ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->Uncache( bPreserveVars ); }
	virtual void	Precache() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->Precache(); }
	// If provided, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	virtual bool	PrecacheVars( KeyValues *pKeyValues = NULL, KeyValues *pPatchKeyValues = NULL, CUtlVector<FileNameHandle_t> *pIncludes = NULL, int nFindContext = MATERIAL_FINDCONTEXT_NONE ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->PrecacheVars( pKeyValues, pPatchKeyValues, pIncludes, nFindContext ); }
	virtual void	ReloadTextures() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ReloadTextures(); }
	virtual void	SetMinLightmapPageID( int pageID ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->SetMinLightmapPageID( pageID ); }
	virtual void	SetMaxLightmapPageID( int pageID ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->SetMaxLightmapPageID( pageID ); }
	virtual int		GetMinLightmapPageID( ) const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetMinLightmapPageID(); }
	virtual int		GetMaxLightmapPageID( ) const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetMaxLightmapPageID(); }
	virtual IShader *GetShader() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetShader(); }
	virtual bool	IsPrecachedVars() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsPrecachedVars(); }
	virtual void	DrawMesh( VertexCompressionType_t vertexCompression ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->DrawMesh( vertexCompression ); }
	virtual VertexFormat_t GetVertexUsage() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetVertexUsage(); }
	virtual bool PerformDebugTrace() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->PerformDebugTrace(); }
	virtual bool NoDebugOverride() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->NoDebugOverride(); }
	virtual void ToggleSuppression() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ToggleSuppression(); }
	virtual bool IsSuppressed() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsSuppressed(); }
	virtual void ToggleDebugTrace() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ToggleDebugTrace(); }
	virtual bool UseFog() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->UseFog(); }
	virtual void AddMaterialVar( IMaterialVar *pMaterialVar ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->AddMaterialVar( pMaterialVar ); }
	virtual ShaderRenderState_t *GetRenderState() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetRenderState(); }
	virtual bool IsManuallyCreated() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsManuallyCreated(); }
	virtual bool NeedsFixedFunctionFlashlight() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->NeedsFixedFunctionFlashlight(); }
	virtual bool IsUsingVertexID() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsUsingVertexID(); }
	virtual void MarkAsPreloaded( bool bSet ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->MarkAsPreloaded( bSet ); }
	virtual bool IsPreloaded() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsPreloaded(); }
	virtual void ArtificialAddRef() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ArtificialAddRef(); }
	virtual void ArtificialRelease() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ArtificialRelease(); }
	virtual void ReportVarChanged( IMaterialVar *pVar ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; m_pRealTimeVersion->ReportVarChanged( pVar ); }
	virtual uint32 GetChangeID() const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->GetChangeID(); }
	virtual bool IsTranslucentInternal( float fAlphaModulation ) const { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->IsTranslucentInternal( fAlphaModulation ); }

	virtual void DecideShouldReloadFromWhitelist( IFileList *pFileList ) { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->DecideShouldReloadFromWhitelist( pFileList ); }
	virtual void ReloadFromWhitelistIfMarked() { QUEUEFRIENDLY_USED_INTERNALLY_ASSERT; return m_pRealTimeVersion->ReloadFromWhitelistIfMarked(); }

	virtual bool IsRealTimeVersion( void ) const;
	virtual IMaterialInternal *GetRealTimeVersion( void );
	virtual IMaterialInternal *GetQueueFriendlyVersion( void );

	void SetRealTimeVersion( IMaterialInternal *pRealTimeVersion )
	{
		m_pRealTimeVersion = pRealTimeVersion;
		m_nReferenceCount = m_pRealTimeVersion->GetReferenceCount();
	}
	void UpdateToRealTime( void ); //update cached off variables using the real time version as a base.


protected:
	IMaterialInternal *m_pRealTimeVersion; //the material we're wrapping with queued delays

private:	
	//some calls need to know what state the material would be in right now if the queue had completed.
	float m_fAlphaModulationOnQueueCompletion;
	Vector m_vColorModulationOnQueueCompletion;
	int m_nReferenceCount; // Only ever accessed from the main thread.
};



#endif //#ifndef CMATERIAL_QUEUEFRIENDLY_H

