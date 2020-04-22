//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//
 
#include "imaterialinternal.h"
#include "bitmap/tgaloader.h"
#include "colorspace.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include <string.h>
#include "materialsystem_global.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/imaterialproxy.h"							   
#include "shadersystem.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "IHardwareConfigInternal.h"
#include "utlsymbol.h"
#include <malloc.h>
#include "filesystem.h"
#include <KeyValues.h>
#include "mempool.h"
#include "shaderapi/ishaderutil.h"
#include "vtf/vtf.h"
#include "tier1/strtools.h"
#include <ctype.h>
#include "utlbuffer.h"
#include "mathlib/vmatrix.h"
#include "texturemanager.h"
#include "itextureinternal.h"
#include "cmaterial_queuefriendly.h"
#include "mempool.h"

static IMaterialVar *CreateMaterialVarFromKeyValue( IMaterial* pMaterial, KeyValues* pKeyValue );

//-----------------------------------------------------------------------------
// Material SubRect implementation
//-----------------------------------------------------------------------------
class CMaterialSubRect : public IMaterialInternal
{
public:

	// pVMTKeyValues and pPatchKeyValues should come from LoadVMTFile()
						CMaterialSubRect( char const *pMaterialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues, KeyValues *pPatchKeyValues, bool bAssumeCreateFromFile );
	virtual				~CMaterialSubRect();

	// IMaterial Interface
	const char 			*GetName() const;
	const char			*GetTextureGroupName() const;

	int					GetMappingWidth();
	int					GetMappingHeight();

	bool				InMaterialPage( void )									{ return true; }
	void				GetMaterialOffset( float *pOffset );
	void				GetMaterialScale( float *pScale );
	IMaterial			*GetMaterialPage( void )								{ return m_pMaterialPage; }

	void				IncrementReferenceCount( void );
	void				DecrementReferenceCount( void );

	IMaterialVar		*FindVar( char const *varName, bool *found, bool complain = true );
	IMaterialVar		*FindVarFast( char const *pVarName, unsigned int *pToken );

	// Sets new VMT shader parameters for the material
	virtual void		SetShaderAndParams( KeyValues *pKeyValues );

	int 				GetEnumerationID() const;

	// Maybe!
	void				GetReflectivity( Vector& reflect )				{ m_pMaterialPage->GetReflectivity( reflect ); }


	// IMaterialInternal Interface
	int					GetReferenceCount( void ) const;
	void				Precache();
	void				Uncache( bool bPreserveVars = false );
	// If provided, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	bool				PrecacheVars( KeyValues *pKeyValues = NULL, KeyValues *pPatchKeyValues = NULL, CUtlVector<FileNameHandle_t> *pIncludes = NULL, int nFindContext = MATERIAL_FINDCONTEXT_NONE );
	bool				IsPrecached() const;
	bool				IsPrecachedVars( ) const;
	bool				IsManuallyCreated() const;
	void				SetEnumerationID( int id );
	void				AddMaterialVar( IMaterialVar *pMaterialVar );
	void				MarkAsPreloaded( bool bSet );
	bool				IsPreloaded() const;
	void				ArtificialAddRef();
	void				ArtificialRelease();

	//=============================
	// Chained to the material page.
	//=============================	
	// IMaterial Interface.
	PreviewImageRetVal_t GetPreviewImageProperties( int *width, int *height, ImageFormat *imageFormat, bool* isTranslucent ) const
		{ return m_pMaterialPage->GetPreviewImageProperties( width, height, imageFormat, isTranslucent ); } 
	PreviewImageRetVal_t GetPreviewImage( unsigned char *data, int width, int height, ImageFormat imageFormat ) const
		{ return m_pMaterialPage->GetPreviewImage( data, width, height, imageFormat ); }

	ShaderRenderState_t *GetRenderState()								{ return m_pMaterialPage->GetRenderState(); }
	int					GetNumAnimationFrames()							{ return m_pMaterialPage->GetNumAnimationFrames(); }

	void				GetLowResColorSample( float s, float t, float *color ) const
	{
		if ( m_pMaterialPage ) 
			m_pMaterialPage->GetLowResColorSample( s, t, color );
		else
			color[ 0 ] = color[ 1 ] = color[ 2 ] = 0.0f;
	}

	bool				UsesEnvCubemap( void )							{ return m_pMaterialPage->UsesEnvCubemap(); }
	bool				NeedsSoftwareSkinning( void )					{ return m_pMaterialPage->NeedsSoftwareSkinning(); }
	bool				NeedsSoftwareLighting( void )					{ return m_pMaterialPage->NeedsSoftwareLighting(); }
	bool				NeedsTangentSpace( void )						{ return m_pMaterialPage->NeedsTangentSpace(); }
	bool				NeedsPowerOfTwoFrameBufferTexture( bool bCheckSpecificToThisFrame = true )	{ return m_pMaterialPage->NeedsPowerOfTwoFrameBufferTexture( bCheckSpecificToThisFrame ); }
	bool				NeedsFullFrameBufferTexture( bool bCheckSpecificToThisFrame = true )		{ return m_pMaterialPage->NeedsFullFrameBufferTexture( bCheckSpecificToThisFrame ); }
	bool				NeedsLightmapBlendAlpha( void )					{ return m_pMaterialPage->NeedsLightmapBlendAlpha(); }
	
	void				AlphaModulate( float alpha )					{ m_pMaterialPage->AlphaModulate( alpha ); }
	void				ColorModulate( float r, float g, float b )		{ m_pMaterialPage->ColorModulate( r, g, b ); }
	float				GetAlphaModulation( )							{ return m_pMaterialPage->GetAlphaModulation( ); }
	void				GetColorModulation( float *r, float *g, float *b )		{ m_pMaterialPage->GetColorModulation( r, g, b ); }

	void				SetMaterialVarFlag( MaterialVarFlags_t flag, bool on )	{ m_pMaterialPage->SetMaterialVarFlag( flag, on ); }
	bool				GetMaterialVarFlag( MaterialVarFlags_t flag ) const		{ return m_pMaterialPage->GetMaterialVarFlag( flag ); }

	bool				IsTranslucent()									{ return m_pMaterialPage->IsTranslucent(); }
	bool				IsTranslucentInternal( float fAlphaModulation ) const { return m_pMaterialPage->IsTranslucentInternal( fAlphaModulation ); }
	bool				IsAlphaTested()									{ return m_pMaterialPage->IsAlphaTested(); }
	bool				IsVertexLit()									{ return m_pMaterialPage->IsVertexLit(); }

	bool				GetPropertyFlag( MaterialPropertyTypes_t type ) { return m_pMaterialPage->GetPropertyFlag( type ); }

	bool				IsTwoSided()									{ return m_pMaterialPage->IsTwoSided(); }

	int					GetNumPasses( void )							{ return m_pMaterialPage->GetNumPasses(); }
	int					GetTextureMemoryBytes( void )					{ return m_pMaterialPage->GetTextureMemoryBytes(); }

	// IMaterialInternal Interface.
	void				DrawMesh( VertexCompressionType_t vertexCompression )	{ m_pMaterialPage->DrawMesh( vertexCompression ); }
	void				ReloadTextures( void )									{ m_pMaterialPage->ReloadTextures(); }
	void				SetMinLightmapPageID( int pageID )				
	{
		m_pMaterialPage->SetMinLightmapPageID( pageID );
	}

	void				SetMaxLightmapPageID( int pageID )				
	{ 
		m_pMaterialPage->SetMaxLightmapPageID( pageID ); 
	}

	int					GetMinLightmapPageID( ) const					{ return m_pMaterialPage->GetMinLightmapPageID(); }
	int					GetMaxLightmapPageID( ) const					{ return m_pMaterialPage->GetMaxLightmapPageID(); }
	
	void				SetNeedsWhiteLightmap( bool val )				
	{ 
		m_pMaterialPage->SetNeedsWhiteLightmap( val ); 
	}

	bool				GetNeedsWhiteLightmap( ) const					{ return m_pMaterialPage->GetNeedsWhiteLightmap(); }
	
	IShader *			GetShader() const								{ return m_pMaterialPage->GetShader(); }
	void				CallBindProxy( void *proxyData )				{ m_pMaterialPage->CallBindProxy( proxyData ); }
	IMaterial			*CheckProxyReplacement( void *proxyData )		{ return m_pMaterialPage->CheckProxyReplacement( proxyData ); }
	bool				HasProxy( void ) const							{ return m_pMaterialPage->HasProxy(); }

	// Sets the shader associated with the material
	void				SetShader( const char *pShaderName )			{ m_pMaterialPage->SetShader( pShaderName ); }
	const char *		GetShaderName() const 							{ return m_pMaterialPage->GetShaderName(); }

	virtual void		DeleteIfUnreferenced();
	virtual bool		IsSpriteCard()									{ return m_pMaterialPage->IsSpriteCard(); }

	// Can we override this material in debug?
	bool				NoDebugOverride() const							{ return m_pMaterialPage->NoDebugOverride(); }

	// Gets the vertex format
	VertexFormat_t		GetVertexFormat() const							{ return m_pMaterialPage->GetVertexFormat(); }

	// Gets the morph format
	virtual MorphFormat_t GetMorphFormat() const						{ return m_pMaterialPage->GetMorphFormat(); }

	// diffuse bump lightmap?
//	bool				IsUsingDiffuseBumpedLighting() const			{ return m_pChainMaterial->IsUsingDiffuseBumpedLighting(); }

	// lightmap?
//	bool				IsUsingLightmap() const							{ return m_pChainMaterial->IsUsingLightmap(); }

	// Gets the vertex usage flags
	VertexFormat_t		GetVertexUsage() const							{ return m_pMaterialPage->GetVertexUsage(); }

	// Debugs this material
	bool				PerformDebugTrace() const						{ return m_pMaterialPage->PerformDebugTrace(); }

	// Are we suppressed?
	bool				IsSuppressed() const							{ return m_pMaterialPage->IsSuppressed(); }

	// Do we use fog?
	bool				UseFog( void ) const							{ return m_pMaterialPage->UseFog(); }
	
	// Should we draw?
	void				ToggleSuppression()								{ m_pMaterialPage->ToggleSuppression(); }
	void				ToggleDebugTrace()								{ m_pMaterialPage->ToggleDebugTrace(); }
	
	// Refresh material based on current var values
	void				Refresh()										{ m_pMaterialPage->Refresh(); }
	void				RefreshPreservingMaterialVars()					{ m_pMaterialPage->RefreshPreservingMaterialVars(); }

	// This computes the state snapshots for this material
	void				RecomputeStateSnapshots()						{ m_pMaterialPage->RecomputeStateSnapshots(); }

	// Gets at the shader parameters
	int					ShaderParamCount() const						{ return m_pMaterialPage->ShaderParamCount(); }
	IMaterialVar		**GetShaderParams( void )						{ return m_pMaterialPage->GetShaderParams(); }

	bool				IsErrorMaterial() const							{ return false; }

	void				SetUseFixedFunctionBakedLighting( bool bEnable ) { m_pMaterialPage->SetUseFixedFunctionBakedLighting( bEnable ); }
	bool				NeedsFixedFunctionFlashlight() const			{ return m_pMaterialPage->NeedsFixedFunctionFlashlight(); }

	virtual void		DecideShouldReloadFromWhitelist( IFileList *pFileList )		{ m_pMaterialPage->DecideShouldReloadFromWhitelist( pFileList ); }
	virtual void		ReloadFromWhitelistIfMarked()								{ return m_pMaterialPage->ReloadFromWhitelistIfMarked(); }
	virtual bool		WasReloadedFromWhitelist()								{ return m_pMaterialPage->WasReloadedFromWhitelist(); }

	bool				IsUsingVertexID( ) const						{ return m_pMaterialPage->IsUsingVertexID(); }

	virtual void ReportVarChanged( IMaterialVar *pVar ) { m_pMaterialPage->ReportVarChanged(pVar); }
	virtual uint32 GetChangeID() const { return m_pMaterialPage->GetChangeID(); }

	virtual bool IsRealTimeVersion( void ) const { return true; }
	virtual IMaterialInternal *GetRealTimeVersion( void ) { return this; }
	virtual IMaterialInternal *GetQueueFriendlyVersion( void ) { return &m_QueueFriendlyVersion; };

	virtual void PrecacheMappingDimensions( void ) { m_pMaterialPage->PrecacheMappingDimensions(); }
	virtual void FindRepresentativeTexture( void ) { m_pMaterialPage->FindRepresentativeTexture(); }

private:

	void				ParseMaterialVars( KeyValues &keyValues );
	void				SetupMaterialVars( void );

	// Do we use a UNC-specified materal name?
	bool				UsesUNCFileName() const;

	IMaterialVar		*GetDummyMaterialVar();

private:

	enum
	{
		MATERIALSUBRECT_IS_PRECACHED = 0x1,
		MATERIALSUBRECT_VARS_IS_PRECACHED = 0x2,
		MATERIALSUBRECT_IS_MANUALLY_CREATED = 0x4,	
		MATERIALSUBRECT_USES_UNC_FILENAME = 0x20,
		MATERIALSUBRECT_IS_PRELOADED = 0x40,
		MATERIALSUBRECT_ARTIFICIAL_REFCOUNT = 0x80,
	};

	// Fixed-size allocator
	DECLARE_FIXEDSIZE_ALLOCATOR( CMaterialSubRect );

	IMaterialInternal			*m_pMaterialPage;

	int							m_iEnumID;

	CUtlSymbol					m_symName;
	CUtlSymbol					m_symTextureGroupName;

	Vector2D					m_vecOffset;
	Vector2D					m_vecScale;
	Vector2D					m_vecSize;

	short						m_nRefCount;

	unsigned int				m_fLocal;					// Local flags - precached etc...

	CUtlVector<IMaterialVar*>	m_aMaterialVars;

	// Used only by procedural materials; it essentially is an in-memory .VMT file
	KeyValues					*m_pVMTKeyValues;

#ifdef _DEBUG
	// Makes it easier to see what's going on
	char*						m_pDebugName;
#endif

	CMaterial_QueueFriendly		m_QueueFriendlyVersion;
};


// NOTE: This must be the last file included
// Has to exist *after* fixed size allocator declaration
#include "tier0/memdbgon.h"

DEFINE_FIXEDSIZE_ALLOCATOR( CMaterialSubRect, 256, true );

//-----------------------------------------------------------------------------
// Purpose: Static create method for material subrect.
//-----------------------------------------------------------------------------
IMaterialInternal* IMaterialInternal::CreateMaterialSubRect( char const* pMaterialName, const char *pTextureGroupName, 
															 KeyValues *pVMTKeyValues, KeyValues *pPatchKeyValues, bool bAssumeCreateFromFile )
{
	return new CMaterialSubRect( pMaterialName, pTextureGroupName, pVMTKeyValues, pPatchKeyValues, bAssumeCreateFromFile );
}

//-----------------------------------------------------------------------------
// Purpose: Static destroy method for material subrect.
//-----------------------------------------------------------------------------
void IMaterialInternal::DestroyMaterialSubRect( IMaterialInternal* pMaterial )
{
	if ( pMaterial )
	{
		CMaterialSubRect* pMat = static_cast<CMaterialSubRect*>( pMaterial );
		delete pMat;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMaterialSubRect::CMaterialSubRect( const char *pMaterialName, const char *pTextureGroupName,
								    KeyValues *pVMTKeyValues, KeyValues *pPatchKeyValues, bool bAssumeCreateFromFile )
{
	m_QueueFriendlyVersion.SetRealTimeVersion( this );

	// Name with extension stripped off.
	int len = Q_strlen( pMaterialName );
	char* pTemp = ( char* )_alloca( len + 1 );
	Q_strncpy( pTemp, pMaterialName, len + 1 );
	Q_strlower( pTemp );
	pTemp[ len - 4 ] = '\0';
	m_symName = pTemp;

#ifdef _DEBUG
	m_pDebugName = new char[Q_strlen( pTemp ) + 1];
	Q_strncpy( m_pDebugName, pTemp, Q_strlen( pTemp ) + 1 );
#endif

	m_pMaterialPage = NULL;
	m_iEnumID = 0;
	m_symTextureGroupName = pTextureGroupName;
	m_vecOffset.Init();
	m_vecScale.Init();
	m_vecSize.Init();
	m_nRefCount = 0;
	m_fLocal = 0;
	m_aMaterialVars.Purge();

	if ( pTemp[0] == '/' && pTemp[1] == '/' && pTemp[2] != '/' )
	{
		m_fLocal |= MATERIALSUBRECT_USES_UNC_FILENAME;
	}
	if ( !bAssumeCreateFromFile )
	{
		m_pVMTKeyValues = pVMTKeyValues;
		if (m_pVMTKeyValues)
		{
			m_fLocal |= MATERIALSUBRECT_IS_MANUALLY_CREATED; 
		}
		// Precache immediately.  We need the material page immediately.
		Precache();
	}
	else
	{
		m_pVMTKeyValues = NULL;
		PrecacheVars( pVMTKeyValues, pPatchKeyValues );
		Precache();
	}

	Assert( m_pMaterialPage );

	// Increment the material page usage counter.
	m_pMaterialPage->IncrementReferenceCount();
}

//-----------------------------------------------------------------------------
// Purpose: Deconstructor
//-----------------------------------------------------------------------------
CMaterialSubRect::~CMaterialSubRect()
{
	Uncache( );
	if( m_nRefCount != 0 )
	{
		DevWarning( 2, "Reference Count for Material %s (%d) != 0\n", GetName(), m_nRefCount );
	}

	if ( m_pMaterialPage )
	{
		m_pMaterialPage->DecrementReferenceCount();
		m_pMaterialPage = NULL;
	}

	if ( m_pVMTKeyValues )
	{
		m_pVMTKeyValues->deleteThis();
		m_pVMTKeyValues = NULL;
	}

	// m_aMaterialVars is freed, purged, and lit on fire in Uncache() above.

#ifdef _DEBUG
	if ( m_pDebugName )
	{
		delete[] m_pDebugName;
		m_pDebugName = NULL;
	}
#endif
}

//-----------------------------------------------------------------------------
// Sets new VMT shader parameters for the material
//-----------------------------------------------------------------------------
void CMaterialSubRect::SetShaderAndParams( KeyValues *pKeyValues )
{
	Uncache();

	if ( m_pVMTKeyValues )
	{
		m_pVMTKeyValues->deleteThis();
		m_pVMTKeyValues = NULL;
	}

	m_pVMTKeyValues = pKeyValues ? pKeyValues->MakeCopy() : NULL;
	if (m_pVMTKeyValues)
	{
		m_fLocal |= MATERIALSUBRECT_IS_MANUALLY_CREATED; 
	}

	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		Precache();
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CMaterialSubRect::GetName() const
{
	return m_symName.String();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CMaterialSubRect::GetTextureGroupName() const
{
	return m_symTextureGroupName.String();
}

//-----------------------------------------------------------------------------
// Purpose: Return the size of the subrect not the texture page size (width).
//-----------------------------------------------------------------------------
int CMaterialSubRect::GetMappingWidth()
{
	return int( m_vecSize.x );
}

//-----------------------------------------------------------------------------
// Purpose: Return the size of the subrect not the texture page size (height).
//-----------------------------------------------------------------------------
int CMaterialSubRect::GetMappingHeight()
{
	return int( m_vecSize.y );
}

//-----------------------------------------------------------------------------
// Purpose: Return the texture offset into the texture page.
//-----------------------------------------------------------------------------
void CMaterialSubRect::GetMaterialOffset( float *pOffset )
{
	pOffset[0] = m_vecOffset.x;
	pOffset[1] = m_vecOffset.y;
}

//-----------------------------------------------------------------------------
// Purpose: Return the texture scale (size) within the texture page.
//-----------------------------------------------------------------------------
void CMaterialSubRect::GetMaterialScale( float *pScale )
{
	pScale[0] = m_vecScale.x;
	pScale[1] = m_vecScale.y;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterialSubRect::IncrementReferenceCount( void )
{
	++m_nRefCount;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterialSubRect::DecrementReferenceCount( void )
{ 
	--m_nRefCount;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CMaterialSubRect::GetReferenceCount( void )	const
{
	return m_nRefCount;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMaterialSubRect::IsPrecached() const
{
	return ( m_fLocal & MATERIALSUBRECT_IS_PRECACHED ) != 0;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMaterialSubRect::IsPrecachedVars( ) const
{
	return ( m_fLocal & MATERIALSUBRECT_VARS_IS_PRECACHED ) != 0;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMaterialSubRect::IsManuallyCreated() const
{
	return ( m_fLocal & MATERIALSUBRECT_IS_MANUALLY_CREATED ) != 0;
}


//-----------------------------------------------------------------------------
// Do we use a UNC-specified materal name?
//-----------------------------------------------------------------------------
bool CMaterialSubRect::UsesUNCFileName() const
{
	return ( m_fLocal & MATERIALSUBRECT_USES_UNC_FILENAME ) != 0;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterialSubRect::Precache()
{
	// Are we already precached?
	if( IsPrecached() )
		return;

	// Load data from the .vmt file.
	if( !PrecacheVars() )
		return;

	m_QueueFriendlyVersion.UpdateToRealTime();

	// Precached.
	m_fLocal |= MATERIALSUBRECT_IS_PRECACHED;
}
		  

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMaterialSubRect::PrecacheVars( KeyValues * pVMTKeyValues, KeyValues * pPatchKeyValues, CUtlVector<FileNameHandle_t> *pIncludes, int nFindContext )
{
	// FIXME:  Should call through to the parent material for all of this???
	// We should get both parameters or neither
	Assert( ( pVMTKeyValues == NULL ) ? ( pPatchKeyValues == NULL ) : ( pPatchKeyValues != NULL ) );

	// Are we already precached?
	if( IsPrecachedVars() )
		return true;

	// load data from the vmt file
	bool bOk = false;
	KeyValues *vmtKeyValues = NULL;
	KeyValues *patchKeyValues = NULL;
	if ( m_pVMTKeyValues )
	{
		// Use the procedural KeyValues
		vmtKeyValues = m_pVMTKeyValues;
		patchKeyValues = new KeyValues( "vmt_patches" );

		// The caller should not be passing in KeyValues if we have procedural ones
		Assert( ( pVMTKeyValues == NULL ) && ( pPatchKeyValues == NULL ) );
	}
	else if ( pVMTKeyValues )
	{
		// Use the passed-in (already-loaded) KeyValues
		vmtKeyValues = pVMTKeyValues;
		patchKeyValues = pPatchKeyValues;
	}
	else
	{
		// load data from the vmt file
		vmtKeyValues = new KeyValues( "vmt" );
		patchKeyValues = new KeyValues( "vmt_patches" );
		if( !LoadVMTFile( *vmtKeyValues, *patchKeyValues, GetName(), UsesUNCFileName(), NULL ) )
		{
			Warning( "CMaterialSubRect::PrecacheVars: error loading vmt file for %s\n", GetName() );
			goto precacheVarsDone;
		}
	}

	// Get the "Subrect" material vars.
	ParseMaterialVars( *vmtKeyValues );

	// Setup the "Subrect" material vars.
	SetupMaterialVars();

	// Vars are precached.
	m_fLocal |= MATERIALSUBRECT_VARS_IS_PRECACHED;
	bOk = true;

precacheVarsDone:
	// Clean up
	if ( ( vmtKeyValues != m_pVMTKeyValues ) && ( vmtKeyValues != pVMTKeyValues ) )
	{
		vmtKeyValues->deleteThis();
	}
	if ( patchKeyValues != pPatchKeyValues )
	{
		patchKeyValues->deleteThis();
	}

	return bOk;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterialSubRect::ParseMaterialVars( KeyValues &keyValues )
{
	KeyValues *pKeyValues = &keyValues;

	// I'm not quite sure how this can happen, but we'll see... 
	const char *pShaderName = pKeyValues->GetName();
	if ( !pShaderName )
	{
		DevWarning( 1, "CMaterialSubRect::InitializeShader: Shader not specified in material %s.\n", GetName() );
		Assert( 0 );
		pShaderName = IsPC() && !IsEmulatingGL() ? "Wireframe_DX6" : "Wireframe_DX9";
	}

	// Verify we have the correct "shader."  There is only one type.
	// Needs to be case insensitive because we can't guarantee case specified in VMTs
	if ( !Q_stricmp( pShaderName, "Subrect" ) )
	{
		KeyValues *pVar = pKeyValues->GetFirstSubKey();
		while ( pVar )
		{
			if ( !Q_stricmp( pVar->GetName(), "$Pos" ) )
			{
				sscanf( pVar->GetString(), "%f %f", &m_vecOffset.x, &m_vecOffset.y );
			}
			else if ( !Q_stricmp( pVar->GetName(), "$Size" ) )
			{
				sscanf( pVar->GetString(), "%f %f", &m_vecSize.x, &m_vecSize.y );
			}
			else if ( !Q_stricmp( pVar->GetName(), "$Material" ) )
			{
				m_pMaterialPage = static_cast<IMaterialInternal*>( MaterialSystem()->FindMaterial( pVar->GetString(), TEXTURE_GROUP_DECAL ) );
				m_pMaterialPage = m_pMaterialPage->GetRealTimeVersion(); //always work with the realtime material internally
			}

//			else if ( !Q_stricmp( pVar->GetName(), "$decalscale" ) )
//			{
//				m_flDecalScale = pVar->GetFloat();
//			}

			// Add var to list.
			IMaterialVar *pNewVar = CreateMaterialVarFromKeyValue( this, pVar );
			if ( pNewVar )
			{
				m_aMaterialVars.AddToTail( pNewVar );		
			}

			// Continue getting the keys until they are all found.
			pVar = pVar->GetNextKey();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterialSubRect::SetupMaterialVars( void )
{
	if ( !m_pMaterialPage )
	{
		DevWarning( 1, "CMaterialSubRect::SetupMaterialVars: Invalid Material Page!\n" );
		return;
	}

	// Ask the material page for its size.
	int nMaterialPageWidth = m_pMaterialPage->GetMappingWidth();
	int nMaterialPageHeight = m_pMaterialPage->GetMappingHeight();

	// Normalize the offset and scale.
	float flOOWidth = 1.0f / static_cast<float>( nMaterialPageWidth );
	float flOOHeight = 1.0f / static_cast<float>( nMaterialPageHeight );

	// Add 0.5f to push the image "in" by 1/2 a texel, and subtract 1.0f to push it
	// "in" by 1/2 a texel on the other side.
	m_vecOffset.x += 1.0f;
	m_vecOffset.y += 1.0f;
	m_vecOffset.x *= flOOWidth;
	m_vecOffset.y *= flOOHeight;
	m_vecScale.x = ( m_vecSize.x - 2.0f ) * flOOWidth;
	m_vecScale.y = ( m_vecSize.y - 2.0f ) * flOOHeight;
}

//-----------------------------------------------------------------------------
// Purpose: Look through 
//-----------------------------------------------------------------------------
IMaterialVar *CMaterialSubRect::FindVar( char const *varName, bool *found, bool complain ) 
{ 
	// Look for the var in the material page - it has precedence.
	IMaterialVar *pVar = m_pMaterialPage->FindVar( varName, found, false );
	if ( *found )
		return pVar;

	// Look for the var in the local list of vars.
	MaterialVarSym_t symVar = IMaterialVar::FindSymbol( varName );
	if ( symVar != UTL_INVAL_SYMBOL )
	{
		int nVarCount = m_aMaterialVars.Count();
		for ( int iVar = 0; iVar < nVarCount; ++iVar )
		{
			if ( m_aMaterialVars[iVar]->GetNameAsSymbol() == symVar )
			{
				*found = true;
				return m_aMaterialVars[iVar];
			}
		}
	}

	// Not found!
	if( complain )
	{
		static int complainCount = 0;
		if( complainCount < 100 )
		{
			DevWarning( 1, "No such variable \"%s\" for material \"%s\"\n", varName, GetName() );
			complainCount++;
		}
	}

	return GetDummyMaterialVar();
} 

IMaterialVar *CMaterialSubRect::FindVarFast( char const *pVarName, unsigned int *pToken )
{
	// Look for the var in the material page - it has precedence.
	IMaterialVar *pVar = m_pMaterialPage->FindVarFast( pVarName, pToken );
	if ( pVar )
		return pVar;

	if ( *pToken != UTL_INVAL_SYMBOL )
	{
		int nVarCount = m_aMaterialVars.Count();
		for ( int iVar = 0; iVar < nVarCount; ++iVar )
		{
			if ( m_aMaterialVars[iVar]->GetNameAsSymbol() == *pToken )
				return m_aMaterialVars[iVar];
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IMaterialVar *CMaterialSubRect::GetDummyMaterialVar()
{
	static IMaterialVar* pDummyVar = 0;
	if ( !pDummyVar )
		pDummyVar = IMaterialVar::Create( 0, "$dummyVar", 0 );

	return pDummyVar;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CMaterialSubRect::GetEnumerationID() const
{
	return m_iEnumID;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterialSubRect::SetEnumerationID( int id )
{
	m_iEnumID = id;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterialSubRect::Uncache( bool bPreserveVars )
{
	MaterialLock_t hMaterialLock = MaterialSystem()->Lock();

	// Don't bother if we're not cached
	if ( IsPrecached() )
	{
		m_fLocal &= ~MATERIALSUBRECT_IS_PRECACHED;
	}

	if ( !bPreserveVars )
	{
		if ( IsPrecachedVars() )
		{
			for ( int i = 0; i < m_aMaterialVars.Count(); ++i )
			{
				IMaterialVar::Destroy( m_aMaterialVars[i] );
			}
			m_aMaterialVars.Purge();

			m_fLocal &= ~MATERIALSUBRECT_VARS_IS_PRECACHED;
		}
	}

	MaterialSystem()->Unlock( hMaterialLock );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterialSubRect::AddMaterialVar( IMaterialVar *pMaterialVar )
{
	m_aMaterialVars.AddToTail( pMaterialVar );
}

void CMaterialSubRect::MarkAsPreloaded( bool bSet )
{
	if ( bSet )
	{
		m_fLocal |= MATERIALSUBRECT_IS_PRELOADED;
	}
	else
	{
		m_fLocal &= ~MATERIALSUBRECT_IS_PRELOADED;
	}
}

bool CMaterialSubRect::IsPreloaded() const
{
	return ( m_fLocal & MATERIALSUBRECT_IS_PRELOADED ) != 0;
}

void CMaterialSubRect::ArtificialAddRef( void )
{
	if ( m_fLocal & MATERIALSUBRECT_ARTIFICIAL_REFCOUNT )
	{
		// already done
		return;
	}

	m_fLocal |= MATERIALSUBRECT_ARTIFICIAL_REFCOUNT;
	m_nRefCount++;
}

void CMaterialSubRect::ArtificialRelease( void )
{
	if ( !( m_fLocal & MATERIALSUBRECT_ARTIFICIAL_REFCOUNT ) )
	{
		return;
	}

	m_fLocal &= ~MATERIALSUBRECT_ARTIFICIAL_REFCOUNT;
	m_nRefCount--;
}

//-----------------------------------------------------------------------------
// Parser utilities
//-----------------------------------------------------------------------------
static inline bool IsWhitespace( char c )
{
	return c == ' ' || c == '\t';
}

static inline bool IsEndline( char c )
{
	return c == '\n' || c == '\0';
}

static inline bool IsVector( char const* v )
{
	while (IsWhitespace(*v))
	{
		++v;
		if (IsEndline(*v))
			return false;
	}
	return *v == '[' || *v == '{';
}

//-----------------------------------------------------------------------------
// Creates a vector material var
//-----------------------------------------------------------------------------
static IMaterialVar* CreateVectorMaterialVarFromKeyValue( IMaterial* pMaterial, KeyValues* pKeyValue )
{
	float vecVal[4];
	char const* pScan = pKeyValue->GetString();
	bool divideBy255 = false;

	// skip whitespace
	while( IsWhitespace(*pScan) )
	{
		++pScan;
	}

	if( *pScan == '{' )
	{
		divideBy255 = true;
	}
	else
	{
		Assert( *pScan == '[' );
	}
	
	// skip the '['
	++pScan;
	int i;
	for( i = 0; i < 4; i++ )
	{
		// skip whitespace
		while( IsWhitespace(*pScan) )
		{
			++pScan;
		}

		if( IsEndline(*pScan) || *pScan == ']' || *pScan == '}' )
		{
			if (*pScan != ']' && *pScan != '}')
			{
				Warning( "Warning in .VMT file (%s): no ']' or '}' found in vector key \"%s\".\n"
					"Did you forget to surround the vector with \"s?\n", pMaterial->GetName(), pKeyValue->GetName() );
			}

			// allow for vec2's, etc.
			vecVal[i] = 0.0f;
			break;
		}

		char* pEnd;

		vecVal[i] = strtod( pScan, &pEnd );
		if (pScan == pEnd)
		{
			Warning( "Error in .VMT file: error parsing vector element \"%s\" in \"%s\"\n", pKeyValue->GetName(), pMaterial->GetName() );
			return 0;
		}

		pScan = pEnd;
	}

	if( divideBy255 )
	{
		vecVal[0] *= ( 1.0f / 255.0f );
		vecVal[1] *= ( 1.0f / 255.0f );
		vecVal[2] *= ( 1.0f / 255.0f );
		vecVal[3] *= ( 1.0f / 255.0f );
	}
	
	// Create the variable!
	return IMaterialVar::Create( pMaterial, pKeyValue->GetName(), vecVal, i );
}

//-----------------------------------------------------------------------------
// Creates a vector material var
//-----------------------------------------------------------------------------
static IMaterialVar* CreateMatrixMaterialVarFromKeyValue( IMaterial* pMaterial, KeyValues* pKeyValue )
{
	char const* pScan = pKeyValue->GetString();

	// Matrices can be specified one of two ways:
	// [ # # # #  # # # #  # # # #  # # # # ]
	// or
	// center # # scale # # rotate # translate # #

	VMatrix mat;
	int count = sscanf( pScan, " [ %f %f %f %f  %f %f %f %f  %f %f %f %f  %f %f %f %f ]",
		&mat.m[0][0], &mat.m[0][1], &mat.m[0][2], &mat.m[0][3],
		&mat.m[1][0], &mat.m[1][1], &mat.m[1][2], &mat.m[1][3],
		&mat.m[2][0], &mat.m[2][1], &mat.m[2][2], &mat.m[2][3],
		&mat.m[3][0], &mat.m[3][1], &mat.m[3][2], &mat.m[3][3] );
	if (count == 16)
	{
		return IMaterialVar::Create( pMaterial, pKeyValue->GetName(), mat );
	}

	Vector2D scale, center;
	float angle;
	Vector2D translation;
	count = sscanf( pScan, " center %f %f scale %f %f rotate %f translate %f %f",
		&center.x, &center.y, &scale.x, &scale.y, &angle, &translation.x, &translation.y );
	if (count != 7)
		return NULL;

	VMatrix temp;
	MatrixBuildTranslation( mat, -center.x, -center.y, 0.0f );
	MatrixBuildScale( temp, scale.x, scale.y, 1.0f );
	MatrixMultiply( temp, mat, mat );
	MatrixBuildRotateZ( temp, angle );
	MatrixMultiply( temp, mat, mat );
	MatrixBuildTranslation( temp, center.x + translation.x, center.y + translation.y, 0.0f );
	MatrixMultiply( temp, mat, mat );

	// Create the variable!
	return IMaterialVar::Create( pMaterial, pKeyValue->GetName(), mat );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static IMaterialVar *CreateMaterialVarFromKeyValue( IMaterial* pMaterial, KeyValues* pKeyValue )
{
	switch( pKeyValue->GetDataType() )
	{
	case KeyValues::TYPE_INT:
		{
			return IMaterialVar::Create( pMaterial, pKeyValue->GetName(), pKeyValue->GetInt() );
		}

	case KeyValues::TYPE_FLOAT:
		{
			return IMaterialVar::Create( pMaterial, pKeyValue->GetName(), pKeyValue->GetFloat() );
		}

	case KeyValues::TYPE_STRING:
		{
			char const* pString = pKeyValue->GetString();
			if (!pString || !pString[0])
				return 0;

			// Look for matrices
			IMaterialVar *pMatrixVar = CreateMatrixMaterialVarFromKeyValue( pMaterial, pKeyValue );
			if ( pMatrixVar )
				return pMatrixVar;

			// Look for vectors
			if ( !IsVector( pString ) )
				return IMaterialVar::Create( pMaterial, pKeyValue->GetName(), pString );

			// Parse the string as a vector...
			return CreateVectorMaterialVarFromKeyValue( pMaterial, pKeyValue );
		}
	}

	return 0;
}

void CMaterialSubRect::DeleteIfUnreferenced()
{
	if ( m_nRefCount > 0 )
		return;
	MaterialSystem()->RemoveMaterialSubRect( this );
}
