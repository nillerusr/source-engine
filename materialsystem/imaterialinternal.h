//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef IMATERIALINTERNAL_H
#define IMATERIALINTERNAL_H

#ifdef _WIN32
#pragma once
#endif

// identifier was truncated to '255' characters in the debug information
#pragma warning(disable: 4786)

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "shaderapi/ishaderapi.h"
#include "filesystem.h"


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
enum MaterialPrimitiveType_t;
class IShader;
class IMesh;
class IVertexBuffer;
class IIndexBuffer;
struct Shader_VertexArrayData_t;
struct ShaderRenderState_t;
class KeyValues;

//-----------------------------------------------------------------------------
// Interface for materials used only within the material system
//-----------------------------------------------------------------------------
abstract_class IMaterialInternal : public IMaterial
{
public:
	// class factory methods
	static IMaterialInternal* CreateMaterial( char const* pMaterialName, const char *pTextureGroupName, KeyValues *pKeyValues = NULL );
	static void DestroyMaterial( IMaterialInternal* pMaterial );

	// If supplied, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	static IMaterialInternal* CreateMaterialSubRect( char const* pMaterialName, const char *pTextureGroupName,
													KeyValues *pKeyValues = NULL, KeyValues *pPatchKeyValues = NULL, bool bAssumeCreateFromFile = false );
	static void DestroyMaterialSubRect( IMaterialInternal* pMaterial );

	// refcount
	virtual int		GetReferenceCount( ) const = 0;

	// enumeration id
	virtual void	SetEnumerationID( int id ) = 0;

	// White lightmap methods
	virtual void	SetNeedsWhiteLightmap( bool val ) = 0;
	virtual bool	GetNeedsWhiteLightmap( ) const = 0;

	// load/unload 
	virtual void	Uncache( bool bPreserveVars = false ) = 0;
	virtual void	Precache() = 0;
	// If supplied, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	virtual bool	PrecacheVars( KeyValues *pKeyValues = NULL, KeyValues *pPatchKeyValues = NULL, CUtlVector<FileNameHandle_t> *pIncludes = NULL, int nFindContext = MATERIAL_FINDCONTEXT_NONE ) = 0;

	// reload all textures used by this materals
	virtual void	ReloadTextures() = 0;
	
	// lightmap pages associated with this material
	virtual void	SetMinLightmapPageID( int pageID ) = 0;
	virtual void	SetMaxLightmapPageID( int pageID ) = 0;;
	virtual int		GetMinLightmapPageID( ) const = 0;
	virtual int		GetMaxLightmapPageID( ) const = 0;

	virtual IShader *GetShader() const = 0;

	// Can we use it?
	virtual bool	IsPrecached( ) const = 0;
	virtual bool	IsPrecachedVars() const = 0;

	// main draw method
	virtual void	DrawMesh( VertexCompressionType_t vertexCompression ) = 0;

	// Gets the vertex format
	virtual VertexFormat_t GetVertexFormat() const = 0;
	virtual VertexFormat_t GetVertexUsage() const = 0;

	// Performs a debug trace on this material
	virtual bool PerformDebugTrace() const = 0;

	// Can we override this material in debug?
	virtual bool NoDebugOverride() const = 0;

	// Should we draw?
	virtual void ToggleSuppression() = 0;

	// Are we suppressed?
	virtual bool IsSuppressed() const = 0;

	// Should we debug?
	virtual void ToggleDebugTrace() = 0;

	// Do we use fog?
	virtual bool UseFog() const = 0;

	// Adds a material variable to the material
	virtual void AddMaterialVar( IMaterialVar *pMaterialVar ) = 0;

	// Gets the renderstate
	virtual ShaderRenderState_t *GetRenderState() = 0;

	// Was this manually created (not read from a file?)
	virtual bool IsManuallyCreated() const = 0;

	virtual bool NeedsFixedFunctionFlashlight() const = 0;

	virtual bool IsUsingVertexID() const = 0;

	// Identifies a material mounted through the preload path
	virtual void MarkAsPreloaded( bool bSet ) = 0;
	virtual bool IsPreloaded() const = 0;

	// Conditonally increments the refcount
	virtual void ArtificialAddRef( void ) = 0;
	virtual void ArtificialRelease( void ) = 0;

	virtual void			ReportVarChanged( IMaterialVar *pVar ) = 0;
	virtual uint32			GetChangeID() const = 0;

	virtual bool			IsTranslucentInternal( float fAlphaModulation ) const = 0;

	//Is this the queue friendly or realtime version of the material?
	virtual bool IsRealTimeVersion( void ) const = 0;

	virtual void ClearContextData( void )
	{
	}

	//easy swapping between the queue friendly and realtime versions of the material
	virtual IMaterialInternal *GetRealTimeVersion( void ) = 0;
	virtual IMaterialInternal *GetQueueFriendlyVersion( void ) = 0;	

	virtual void PrecacheMappingDimensions( void ) = 0;
	virtual void FindRepresentativeTexture( void ) = 0;

	// These are used when a new whitelist is passed in. First materials to be reloaded are flagged, then they are reloaded.
	virtual void DecideShouldReloadFromWhitelist( IFileList *pFileList ) = 0;
	virtual void ReloadFromWhitelistIfMarked() = 0;
};

extern void InsertKeyValues( KeyValues& dst, KeyValues& src, bool bCheckForExistence, bool bRecursive = false );
extern void WriteKeyValuesToFile( const char *pFileName, KeyValues& keyValues );
extern void ExpandPatchFile( KeyValues& keyValues, KeyValues &patchKeyValues );
// patchKeyValues accumulates keys applied by VMT patch files (this is necessary to make $fallbackmaterial
// work properly - the patch keys need to be reapplied when the fallback VMT is loaded). It may contain
// previously accumulated patch keys on entry, and may contain more encountered patch keys on exit.
extern bool LoadVMTFile( KeyValues &vmtKeyValues, KeyValues &patchKeyValues, const char *pMaterialName, bool bUsesUNCFilename, CUtlVector<FileNameHandle_t> *pIncludes  );

#endif // IMATERIALINTERNAL_H
