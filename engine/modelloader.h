//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( MOD_LOADER_H )
#define MOD_LOADER_H
#ifdef _WIN32
#pragma once
#endif

struct model_t;
class IMaterial;
class IFileList;
class IModelLoadCallback;

#include "utlmemory.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
abstract_class IModelLoader
{
public:
	enum REFERENCETYPE
	{
		// The name is allocated, but nothing else is in memory or being referenced
		FMODELLOADER_NOTLOADEDORREFERENCED = 0,
		// The model has been loaded into memory
		FMODELLOADER_LOADED	= (1<<0),

		// The model is being referenced by the server code
		FMODELLOADER_SERVER = (1<<1),
		// The model is being referenced by the client code
		FMODELLOADER_CLIENT = (1<<2),
		// The model is being referenced in the client .dll
		FMODELLOADER_CLIENTDLL = (1<<3),
		// The model is being referenced by static props
		FMODELLOADER_STATICPROP	= (1<<4),
		// The model is a detail prop
		FMODELLOADER_DETAILPROP = (1<<5),
		// The model is dynamically loaded
		FMODELLOADER_DYNSERVER = (1<<6),
		FMODELLOADER_DYNCLIENT = (1<<7),
		FMODELLOADER_DYNAMIC = FMODELLOADER_DYNSERVER | FMODELLOADER_DYNCLIENT,

		FMODELLOADER_REFERENCEMASK = (FMODELLOADER_SERVER | FMODELLOADER_CLIENT | FMODELLOADER_CLIENTDLL | FMODELLOADER_STATICPROP | FMODELLOADER_DETAILPROP | FMODELLOADER_DYNAMIC ),

		// The model was touched by the preload method
		FMODELLOADER_TOUCHED_BY_PRELOAD = (1<<15),
		// The model was loaded by the preload method, a postload fixup is required
		FMODELLOADER_LOADED_BY_PRELOAD = (1<<16),
		// The model touched its materials as part of its load
		FMODELLOADER_TOUCHED_MATERIALS = (1<<17),
	};

	enum ReloadType_t
	{
		RELOAD_LOD_CHANGED = 0,
		RELOAD_EVERYTHING,
		RELOAD_REFRESH_MODELS,
	};

	// Start up modelloader subsystem
	virtual void		Init( void ) = 0;
	virtual void		Shutdown( void ) = 0;

	virtual int			GetCount( void ) = 0;
	virtual model_t		*GetModelForIndex( int i ) = 0;

	// Look up name for model
	virtual const char *GetName( const model_t *model ) = 0;

	// Check for extra data, reload studio model if needed
	virtual void		*GetExtraData( model_t *model ) = 0;

	// Get disk size for model
	virtual int			GetModelFileSize( const char *name ) = 0;

	// Finds the model, and loads it if it isn't already present.  Updates reference flags
	virtual model_t		*GetModelForName( const char *name, REFERENCETYPE referencetype ) = 0;
	virtual model_t		*ReferenceModel( const char *name, REFERENCETYPE referencetype ) = 0;
	// Unmasks the referencetype field for the model
	virtual void		UnreferenceModel( model_t *model, REFERENCETYPE referencetype ) = 0;
	// Unmasks the specified reference type across all models
	virtual void		UnreferenceAllModels( REFERENCETYPE referencetype ) = 0;
	// Set all models to last loaded on server count -1
	virtual void		ResetModelServerCounts() = 0;

	// For any models with referencetype blank, frees all memory associated with the model
	//  and frees up the models slot
	virtual void		UnloadUnreferencedModels( void ) = 0;
	virtual void		PurgeUnusedModels( void ) = 0;
	virtual void		UnloadModel( model_t *pModel ) = 0;

	// On the client only, there is some information that is computed at the time we are just
	//  about to render the map the first time.  If we don't change/unload the map, then we
	//  shouldn't have to recompute it each time we reconnect to the same map
	virtual bool		Map_GetRenderInfoAllocated( void ) = 0;
	virtual void		Map_SetRenderInfoAllocated( bool allocated ) = 0;

	// Load all the displacements for rendering. Set bRestoring to true if we're recovering from an alt+tab.
	virtual void		Map_LoadDisplacements( model_t *model, bool bRestoring ) = 0;

	// Print which models are in the cache/known
	virtual void		Print( void ) = 0;

	// Validate version/header of a .bsp file
	virtual bool		Map_IsValid( char const *mapname, bool bQuiet = false ) = 0;

	// Recomputes surface flags
	virtual void		RecomputeSurfaceFlags( model_t *mod ) = 0;

	// Reloads all models
	virtual void		Studio_ReloadModels( ReloadType_t reloadType ) = 0;

	// Is a model loaded?
	virtual bool		IsLoaded( const model_t *mod ) = 0;

	virtual bool		LastLoadedMapHasHDRLighting( void ) = 0;

	// See CL_HandlePureServerWhitelist for what this is for.
	virtual void ReloadFilesInList( IFileList *pFilesToReload ) = 0;

	virtual const char *GetActiveMapName( void ) = 0;

	// Called by app system once per frame to poll and update dynamic models
	virtual void		UpdateDynamicModels() = 0;

	// Called by server and client engine code to flush unreferenced dynamic models
	virtual void		FlushDynamicModels() = 0;

	// Called by server and client engine code to flush unreferenced dynamic models
	virtual void		ForceUnloadNonClientDynamicModels() = 0;

	// Called by client code to load dynamic models, instead of GetModelForName.
	virtual model_t		*GetDynamicModel( const char *name, bool bClientOnly ) = 0;

	// Called by client code to query dynamic model state
	virtual bool		IsDynamicModelLoading( model_t *pModel, bool bClientOnly ) = 0;

	// Called by client code to refcount dynamic models
	virtual void		AddRefDynamicModel( model_t *pModel, bool bClientSideRef ) = 0;
	virtual void		ReleaseDynamicModel( model_t *pModel, bool bClientSideRef ) = 0;

	// Called by client code
	virtual bool		RegisterModelLoadCallback( model_t *pModel, bool bClientOnly, IModelLoadCallback *pCallback, bool bCallImmediatelyIfLoaded = true ) = 0;

	// Called by client code or IModelLoadCallback destructor
	virtual void		UnregisterModelLoadCallback( model_t *pModel, bool bClientOnly, IModelLoadCallback *pCallback ) = 0;

	virtual void		Client_OnServerModelStateChanged( model_t *pModel, bool bServerLoaded ) = 0;
};

extern IModelLoader *modelloader;



//-----------------------------------------------------------------------------
// Purpose: Loads the lump to temporary memory and automatically cleans up the
//  memory when it goes out of scope.
//-----------------------------------------------------------------------------

class CMapLoadHelper
{
public:
						CMapLoadHelper( int lumpToLoad );
						~CMapLoadHelper( void );

	// Get raw memory pointer
	byte				*LumpBase( void );
	int					LumpSize( void );
	int					LumpOffset( void );
	int					LumpVersion() const;
	const char			*GetMapName( void );
	char				*GetLoadName( void );
	struct worldbrushdata_t	*GetMap( void );

	// Global setup/shutdown
	static void			Init( model_t *pMapModel, const char *pLoadname );
	static void			InitFromMemory( model_t *pMapModel, const void *pData, int nDataSize );
	static void			Shutdown( void );
	static int			GetRefCount( void );
	
	// Free the lighting lump (increases free memory during loading on 360)
	static void			FreeLightingLump();

	// Returns the size of a particular lump without loading it
	static int			LumpSize( int lumpId );
	static int			LumpOffset( int lumpId );

	// Loads one element in a lump.
	void				LoadLumpElement( int nElemIndex, int nElemSize, void *pData );
	void				LoadLumpData( int offset, int size, void *pData );

private:
	int					m_nLumpSize;
	int					m_nLumpOffset;
	int					m_nLumpVersion;
	byte				*m_pRawData;
	byte				*m_pData;
	byte				*m_pUncompressedData;

	// Handling for lump files
	int					m_nLumpID;
	char				m_szLumpFilename[MAX_PATH];
};

//-----------------------------------------------------------------------------
// Recomputes translucency for the model...
//-----------------------------------------------------------------------------

void Mod_RecomputeTranslucency( model_t* mod, int nSkin, int nBody, void /*IClientRenderable*/ *pClientRenderable, float fInstanceAlphaModulate );

//-----------------------------------------------------------------------------
// game lumps
//-----------------------------------------------------------------------------

int Mod_GameLumpSize( int lumpId );
int Mod_GameLumpVersion( int lumpId );
bool Mod_LoadGameLump( int lumpId, void* pBuffer, int size );

// returns the material count...
int Mod_GetMaterialCount( model_t* mod );

// returns the first n materials.
int Mod_GetModelMaterials( model_t* mod, int count, IMaterial** ppMaterial );

bool Mod_MarkWaterSurfaces( model_t *pModel );

void Mod_SetMaterialVarFlag( model_t *pModel, unsigned int flag, bool on );

//-----------------------------------------------------------------------------
// Hooks the cache notify into the MDL cache system 
//-----------------------------------------------------------------------------
void ConnectMDLCacheNotify( );
void DisconnectMDLCacheNotify( );

//-----------------------------------------------------------------------------
// Initialize studiomdl state
//-----------------------------------------------------------------------------
void InitStudioModelState( model_t *pModel );

extern bool g_bLoadedMapHasBakedPropLighting;
extern bool g_bBakedPropLightingNoSeparateHDR;

#endif // MOD_LOADER_H
