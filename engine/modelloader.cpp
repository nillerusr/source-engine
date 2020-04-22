//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Model loading / unloading interface
//
// $NoKeywords: $
//===========================================================================//

#include "render_pch.h"
#include "common.h"
#include "modelloader.h"
#include "sysexternal.h"
#include "cmd.h"
#include "istudiorender.h"
#include "engine/ivmodelinfo.h"
#include "draw.h"
#include "zone.h"
#include "edict.h"
#include "cmodel_engine.h"
#include "cdll_engine_int.h"
#include "iscratchpad3d.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "gl_rsurf.h"
#include "video/ivideoservices.h"
#include "materialsystem/itexture.h"
#include "Overlay.h"
#include "utldict.h"
#include "mempool.h"
#include "r_decal.h"
#include "l_studio.h"
#include "gl_drawlights.h"
#include "tier0/icommandline.h"
#include "MapReslistGenerator.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif
#include "engine/ivmodelrender.h"
#include "host.h"
#include "datacache/idatacache.h"
#include "sys_dll.h"
#include "datacache/imdlcache.h"
#include "gl_cvars.h"
#include "vphysics_interface.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/tier2.h"
#include "lightcache.h"
#include "lumpfiles.h"
#include "tier2/fileutils.h"
#include "UtlSortVector.h"
#include "utlhashtable.h"
#include "tier1/lzmaDecoder.h"
#include "eiface.h"
#include "server.h"
#include "ifilelist.h"
#include "LoadScreenUpdate.h"
#include "optimize.h"
#include "networkstringtable.h"
#include "tier1/callqueue.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar mat_loadtextures( "mat_loadtextures", "1", FCVAR_CHEAT );

// OS X and Linux are blowing up right now due to this.  Benefits vs possible regressions on DX less clear.
#if defined( DX_TO_GL_ABSTRACTION ) || defined( STAGING_ONLY )
	#define CONVAR_DEFAULT_MOD_OFFLINE_HDR_SWITCH "1"
#else
	#define CONVAR_DEFAULT_MOD_OFFLINE_HDR_SWITCH "0"
#endif
static ConVar mod_offline_hdr_switch( "mod_offline_hdr_switch", CONVAR_DEFAULT_MOD_OFFLINE_HDR_SWITCH, FCVAR_INTERNAL_USE,
                                      "Re-order the HDR/LDR mode switch to do most of the material system "
                                      "reloading with the device offline. This reduces unnecessary device "
                                      "resource uploads and may drastically reduce load time and memory pressure "
                                      "on certain drivers, but may trigger bugs in some very old source engine "
                                      "pathways." );
static ConVar mod_touchalldata( "mod_touchalldata", "1", 0, "Touch model data during level startup" );
static ConVar mod_forcetouchdata( "mod_forcetouchdata", "1", 0, "Forces all model file data into cache on model load." );
ConVar mat_excludetextures( "mat_excludetextures", "0", FCVAR_CHEAT );

ConVar r_unloadlightmaps( "r_unloadlightmaps", "0", FCVAR_CHEAT );
ConVar r_hunkalloclightmaps( "r_hunkalloclightmaps", "1" );
extern ConVar r_lightcache_zbuffercache;


static ConVar mod_dynamicunloadtime( "mod_dynamicunloadtime", "150", FCVAR_HIDDEN | FCVAR_DONTRECORD );
static ConVar mod_dynamicunloadtextures( "mod_dynamicunloadtex", "1", FCVAR_HIDDEN | FCVAR_DONTRECORD );
static ConVar mod_dynamicloadpause( "mod_dynamicloadpause", "0", FCVAR_CHEAT | FCVAR_HIDDEN | FCVAR_DONTRECORD );
static ConVar mod_dynamicloadthrottle( "mod_dynamicloadthrottle", "0", FCVAR_CHEAT | FCVAR_HIDDEN | FCVAR_DONTRECORD );
static ConVar mod_dynamicloadspew( "mod_dynamicloadspew", "0", FCVAR_HIDDEN | FCVAR_DONTRECORD );

#define DynamicModelDebugMsg(...) ( mod_dynamicloadspew.GetBool() ? Msg(__VA_ARGS__) : (void)0 )


bool g_bHunkAllocLightmaps;

extern	CGlobalVars g_ServerGlobalVariables;
extern	IMaterial	*g_materialEmpty;
extern	ConVar		r_rootlod;

bool g_bLoadedMapHasBakedPropLighting = false;
bool g_bBakedPropLightingNoSeparateHDR = false;  // Some maps only have HDR lighting on props, contained in the file for non-hdr light data

double g_flAccumulatedModelLoadTime;
double g_flAccumulatedModelLoadTimeStudio;
double g_flAccumulatedModelLoadTimeStaticMesh;
double g_flAccumulatedModelLoadTimeBrush;
double g_flAccumulatedModelLoadTimeSprite;
double g_flAccumulatedModelLoadTimeVCollideSync;
double g_flAccumulatedModelLoadTimeVCollideAsync;
double g_flAccumulatedModelLoadTimeVirtualModel;
double g_flAccumulatedModelLoadTimeMaterialNamesOnly;

//-----------------------------------------------------------------------------
// A dictionary used to store where to find game lump data in the .bsp file
//-----------------------------------------------------------------------------

// Extended from the on-disk struct to include uncompressed size and stop propagation of bogus signed values
struct dgamelump_internal_t
{
	dgamelump_internal_t( dgamelump_t &other, unsigned int nCompressedSize )
		: id( other.id )
		, flags( other.flags )
		, version( other.version )
		, offset( Max( other.fileofs, 0 ) )
		, uncompressedSize( Max( other.filelen, 0 ) )
		, compressedSize( nCompressedSize )
		{}
	GameLumpId_t	id;
	unsigned short	flags;
	unsigned short	version;
	unsigned int	offset;
	unsigned int	uncompressedSize;
	unsigned int	compressedSize;
};

static CUtlVector< dgamelump_internal_t > g_GameLumpDict;
static char g_GameLumpFilename[128] = { 0 };

//void NotifyHunkBeginMapLoad( const char *pszMapName )
//{
//	Hunk_OnMapStart( 32*1024*1024 );
//}


// FIXME/TODO:  Right now Host_FreeToLowMark unloads all models including studio
//  models that have Cache_Alloc data, too.  This needs to be fixed before shipping

BEGIN_BYTESWAP_DATADESC( lump_t )
	DEFINE_FIELD( fileofs, FIELD_INTEGER ),
	DEFINE_FIELD( filelen, FIELD_INTEGER ),
	DEFINE_FIELD( version, FIELD_INTEGER ),
	DEFINE_FIELD( uncompressedSize, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dheader_t )
	DEFINE_FIELD( ident, FIELD_INTEGER ),
	DEFINE_FIELD( version, FIELD_INTEGER ),
	DEFINE_EMBEDDED_ARRAY( lumps, HEADER_LUMPS ),
	DEFINE_FIELD( mapRevision, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

bool Model_LessFunc( FileNameHandle_t const &a, FileNameHandle_t const &b )
{
	return a < b;
}



//-----------------------------------------------------------------------------
// Purpose: Implements IModelLoader
//-----------------------------------------------------------------------------
class CModelLoader : public IModelLoader
{
// Implement IModelLoader interface
public:
	CModelLoader() : m_ModelPool( sizeof( model_t ), MAX_KNOWN_MODELS, CUtlMemoryPool::GROW_FAST, "CModelLoader::m_ModelPool" ),
					m_Models( 0, 0, Model_LessFunc )
	{
	}

	void			Init( void );
	void			Shutdown( void );

	int				GetCount( void );
	model_t			*GetModelForIndex( int i );

	// Look up name for model
	const char		*GetName( model_t const *model );

	// Check cache for data, reload model if needed
	void			*GetExtraData( model_t *model );

	int				GetModelFileSize( char const *name );

	// Finds the model, and loads it if it isn't already present.  Updates reference flags
	model_t			*GetModelForName( const char *name, REFERENCETYPE referencetype );
	// Mark as referenced by name
	model_t			*ReferenceModel( const char *name, REFERENCETYPE referencetype );

	// Unmasks the referencetype field for the model
	void			UnreferenceModel( model_t *model, REFERENCETYPE referencetype );
	// Unmasks the specified reference type across all models
	void			UnreferenceAllModels( REFERENCETYPE referencetype );
	// Set all models to last loaded on server count -1
	void			ResetModelServerCounts();

	// For any models with referencetype blank, frees all memory associated with the model
	//  and frees up the models slot
	void			UnloadUnreferencedModels( void );
	void			PurgeUnusedModels( void );

	bool			Map_GetRenderInfoAllocated( void );
	void			Map_SetRenderInfoAllocated( bool allocated );

	virtual void	Map_LoadDisplacements( model_t *pModel, bool bRestoring );

	// Validate version/header of a .bsp file
	bool			Map_IsValid( char const *mapname, bool bQuiet = false );

	virtual void	RecomputeSurfaceFlags( model_t *mod );

	virtual void	Studio_ReloadModels( ReloadType_t reloadType );

	void			Print( void );

	// Is a model loaded?
	virtual bool	IsLoaded( const model_t *mod );

	virtual bool	LastLoadedMapHasHDRLighting(void);
	
	void			DumpVCollideStats();

	// Returns the map model, otherwise NULL, no load or create
	model_t			*FindModelNoCreate( const char *pModelName );

	// Finds the model, builds a model entry if not present
	model_t			*FindModel( const char *name );

	modtype_t		GetTypeFromName( const char *pModelName );

	// start with -1, list terminates with -1
	int				FindNext( int iIndex, model_t **ppModel );

	virtual void	UnloadModel( model_t *pModel );

	virtual void	ReloadFilesInList( IFileList *pFilesToReload );

	virtual const char	*GetActiveMapName( void );

	// Called by app system once per frame to poll and update dynamic models
	virtual void	UpdateDynamicModels() { InternalUpdateDynamicModels(false); }

	// Called by server and client engine code to flush unreferenced dynamic models
	virtual void	FlushDynamicModels() { InternalUpdateDynamicModels(true); }

	// Called by server and client to force-unload dynamic models regardless of refcount!
	virtual void	ForceUnloadNonClientDynamicModels();

	// Called by client code to load dynamic models, instead of GetModelForName.
	virtual model_t *GetDynamicModel( const char *name, bool bClientOnly );
	
	// Called by client code to query dynamic model state
	virtual bool	IsDynamicModelLoading( model_t *pModel, bool bClientOnly );

	// Called by client code to refcount dynamic models
	virtual void	AddRefDynamicModel( model_t *pModel, bool bClientSideRef );
	virtual void	ReleaseDynamicModel( model_t *pModel, bool bClientSideRef );

	// Called by client code or GetDynamicModel
	virtual bool	RegisterModelLoadCallback( model_t *pModel, bool bClientOnly, IModelLoadCallback *pCallback, bool bCallImmediatelyIfLoaded );

	// Called by client code or IModelLoadCallback destructor
	virtual void	UnregisterModelLoadCallback( model_t *pModel, bool bClientOnly, IModelLoadCallback *pCallback );

	virtual void	Client_OnServerModelStateChanged( model_t *pModel, bool bServerLoaded );

	void			DebugPrintDynamicModels();

// Internal types
private:
	// TODO, flag these and allow for UnloadUnreferencedModels to check for allocation type
	//  so we don't have to flush all of the studio models when we free the hunk
	enum
	{
		FALLOC_USESHUNKALLOC = (1<<31),
		FALLOC_USESCACHEALLOC = (1<<30),
	};

// Internal methods
private:
	// Set reference flags and load model if it's not present already
	model_t		*LoadModel( model_t	*model, REFERENCETYPE *referencetype );
	// Unload models ( won't unload referenced models if checkreferences is true )
	void		UnloadAllModels( bool checkreference );
	void		SetupSubModels( model_t	*model, CUtlVector<mmodel_t> &list );

	// World/map
	void		Map_LoadModel( model_t *mod );
	void		Map_UnloadModel( model_t *mod );
	void		Map_UnloadCubemapSamples( model_t *mod );

	// World loading helper
	void		SetWorldModel( model_t *mod );
	void		ClearWorldModel( void );
	bool		IsWorldModelSet( void );
	int			GetNumWorldSubmodels( void );

	// Sprites
	void		Sprite_LoadModel( model_t *mod );
	void		Sprite_UnloadModel( model_t *mod );

	// Studio models
	void		Studio_LoadModel( model_t *mod, bool bTouchAllData );
	void		Studio_UnloadModel( model_t *mod );

	// Byteswap
	int			UpdateOrCreate( const char *pSourceName, char *pTargetName, int maxLen, bool bForce );

	// Dynamic load queue
	class CDynamicModelInfo;
	void		QueueDynamicModelLoad( CDynamicModelInfo *dyn, model_t *mod );
	bool		CancelDynamicModelLoad( CDynamicModelInfo *dyn, model_t *mod );
	void		UpdateDynamicModelLoadQueue();

	void		FinishDynamicModelLoadIfReady( CDynamicModelInfo *dyn, model_t *mod );

	void		InternalUpdateDynamicModels( bool bIgnoreUpdateTime );

	// Internal data
private:
	enum 
	{
		MAX_KNOWN_MODELS = 1024,
	};

	struct ModelEntry_t
	{
		model_t *modelpointer;
	};

	CUtlMap< FileNameHandle_t, ModelEntry_t >	m_Models;

	CUtlMemoryPool			m_ModelPool;

	CUtlVector<model_t>	m_InlineModels;

	model_t				*m_pWorldModel;

public: // HACKHACK
	worldbrushdata_t	m_worldBrushData;

private:
	// local name of current loading model
	char				m_szLoadName[64];

	bool				m_bMapRenderInfoLoaded;
	bool				m_bMapHasHDRLighting;

	char				m_szActiveMapName[64];

	// Dynamic model support:
	class CDynamicModelInfo
	{
	public:
		enum { QUEUED = 0x01, LOADING = 0x02, CLIENTREADY = 0x04, SERVERLOADING = 0x08, ALLREADY = 0x10, INVALIDFLAG = 0x20 }; // flags
		CDynamicModelInfo() : m_iRefCount(0), m_iClientRefCount(0), m_nLoadFlags(INVALIDFLAG), m_uLastTouchedMS_Div256(0) { }
		int16 m_iRefCount;
		int16 m_iClientRefCount; // also doublecounted in m_iRefCount
		uint32 m_nLoadFlags : 8;
		uint32 m_uLastTouchedMS_Div256 : 24;
		CUtlVector< uintptr_t > m_Callbacks; // low bit = client only
	};

	CUtlHashtable< model_t * , CDynamicModelInfo > m_DynamicModels;
	CUtlHashtable< uintptr_t , int > m_RegisteredDynamicCallbacks;

	// Dynamic model load queue
	CUtlVector< model_t* > m_DynamicModelLoadQueue;
	bool m_bDynamicLoadQueueHeadActive;
};

// Expose interface
static CModelLoader g_ModelLoader;
IModelLoader *modelloader = ( IModelLoader * )&g_ModelLoader;

//-----------------------------------------------------------------------------
// Globals used by the CMapLoadHelper
//-----------------------------------------------------------------------------
static dheader_t		s_MapHeader;
static FileHandle_t		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
static char				s_szLoadName[128];
static char				s_szMapName[128];
static worldbrushdata_t	*s_pMap = NULL;
static int				s_nMapLoadRecursion = 0;
static CUtlBuffer		s_MapBuffer;

// Lump files are patches for a shipped map
// List of lump files found when map was loaded. Each entry is the lump file index for that lump id.
struct lumpfiles_t
{
	FileHandle_t		file;
	int					lumpfileindex;
	lumpfileheader_t	header;
};
static lumpfiles_t s_MapLumpFiles[ HEADER_LUMPS ];

CON_COMMAND( mem_vcollide, "Dumps the memory used by vcollides" )
{
	g_ModelLoader.DumpVCollideStats();
}

//-----------------------------------------------------------------------------
// Returns the ref count for this bsp
//-----------------------------------------------------------------------------
int CMapLoadHelper::GetRefCount()
{
	return s_nMapLoadRecursion;
}

//-----------------------------------------------------------------------------
// Setup a BSP loading context, maintains a ref count.	
//-----------------------------------------------------------------------------
void CMapLoadHelper::Init( model_t *pMapModel, const char *loadname )
{
	if ( ++s_nMapLoadRecursion > 1 )
	{
		return;
	}

	s_pMap = NULL;
	s_szLoadName[ 0 ] = 0;
	s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
	V_memset( &s_MapHeader, 0, sizeof( s_MapHeader ) );
	V_memset( &s_MapLumpFiles, 0, sizeof( s_MapLumpFiles ) );

	if ( !pMapModel )
	{
		V_strcpy_safe( s_szMapName, loadname );
	}
	else
	{
		V_strcpy_safe( s_szMapName, pMapModel->strName );
	}

	s_MapFileHandle = g_pFileSystem->OpenEx( s_szMapName, "rb", IsX360() ? FSOPEN_NEVERINPACK : 0, IsX360() ? "GAME" : NULL );
	if ( s_MapFileHandle == FILESYSTEM_INVALID_HANDLE )
	{
		Host_Error( "CMapLoadHelper::Init, unable to open %s\n", s_szMapName );
		return;
	}

	g_pFileSystem->Read( &s_MapHeader, sizeof( dheader_t ), s_MapFileHandle );
	if ( s_MapHeader.ident != IDBSPHEADER )
	{
		g_pFileSystem->Close( s_MapFileHandle );
		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
		Host_Error( "CMapLoadHelper::Init, map %s has wrong identifier\n", s_szMapName );
		return;
	}

	if ( s_MapHeader.version < MINBSPVERSION || s_MapHeader.version > BSPVERSION )
	{
		g_pFileSystem->Close( s_MapFileHandle );
		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
		Host_Error( "CMapLoadHelper::Init, map %s has wrong version (%i when expecting %i)\n", s_szMapName,
			s_MapHeader.version, BSPVERSION );
		return;
	}

	V_strcpy_safe( s_szLoadName, loadname );

	// Store map version, but only do it once so that the communication between the engine and Hammer isn't broken. The map version
	// is incremented whenever a Hammer to Engine session is established so resetting the global map version each time causes a problem.
	if ( 0 == g_ServerGlobalVariables.mapversion )
	{
		g_ServerGlobalVariables.mapversion = s_MapHeader.mapRevision;
	}

#ifndef SWDS
	InitDLightGlobals( s_MapHeader.version );
#endif

	s_pMap = &g_ModelLoader.m_worldBrushData;

#if 0
	// XXX(johns): There are security issues with this system currently. sv_pure doesn't handle unexpected/mismatched
	//             lumps, so players can create lumps for maps not using them to wallhack/etc.. Currently unused,
	//             disabling until we have time to make a proper security pass.
	if ( IsPC() )
	{
		// Now find and open our lump files, and create the master list of them.
		for ( int iIndex = 0; iIndex < MAX_LUMPFILES; iIndex++ )
		{
			lumpfileheader_t lumpHeader;
			char lumpfilename[MAX_PATH];

			GenerateLumpFileName( s_szMapName, lumpfilename, MAX_PATH, iIndex );
			if ( !g_pFileSystem->FileExists( lumpfilename ) )
				break;

			// Open the lump file
			FileHandle_t lumpFile = g_pFileSystem->Open( lumpfilename, "rb" );
			if ( lumpFile == FILESYSTEM_INVALID_HANDLE )
			{
				Host_Error( "CMapLoadHelper::Init, failed to load lump file %s\n", lumpfilename );
				return;
			}

			// Read the lump header
			memset( &lumpHeader, 0, sizeof( lumpHeader ) );
			g_pFileSystem->Read( &lumpHeader, sizeof( lumpfileheader_t ), lumpFile );

			if ( lumpHeader.lumpID >= 0 && lumpHeader.lumpID < HEADER_LUMPS )
			{
				// We may find multiple lump files for the same lump ID. If so,
				// close the earlier lump file, because the later one overwrites it.
				if ( s_MapLumpFiles[lumpHeader.lumpID].file != FILESYSTEM_INVALID_HANDLE )
				{
					g_pFileSystem->Close( s_MapLumpFiles[lumpHeader.lumpID].file );
				}

				s_MapLumpFiles[lumpHeader.lumpID].file = lumpFile;
				s_MapLumpFiles[lumpHeader.lumpID].lumpfileindex = iIndex;
				memcpy( &(s_MapLumpFiles[lumpHeader.lumpID].header), &lumpHeader, sizeof(lumpHeader) );
			}
			else
			{
				Warning("Found invalid lump file '%s'. Lump Id: %d\n", lumpfilename, lumpHeader.lumpID );
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Setup a BSP loading context from a supplied buffer
//-----------------------------------------------------------------------------
void CMapLoadHelper::InitFromMemory( model_t *pMapModel, const void *pData, int nDataSize )
{
	// valid for 360 only 
	// 360 has reorganized bsp format and no external lump files
	Assert( IsX360() && pData && nDataSize );

	if ( ++s_nMapLoadRecursion > 1 )
	{
		return;
	}

	s_pMap = NULL;
	s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
	V_memset( &s_MapHeader, 0, sizeof( s_MapHeader ) );
	V_memset( &s_MapLumpFiles, 0, sizeof( s_MapLumpFiles ) );

	V_strcpy_safe( s_szMapName, pMapModel->strName );
	V_FileBase( s_szMapName, s_szLoadName, sizeof( s_szLoadName ) );

	s_MapBuffer.SetExternalBuffer( (void *)pData, nDataSize, nDataSize );

	V_memcpy( &s_MapHeader, pData, sizeof( dheader_t ) );

	if ( s_MapHeader.ident != IDBSPHEADER )
	{
		Host_Error( "CMapLoadHelper::Init, map %s has wrong identifier\n", s_szMapName );
		return;
	}

	if ( s_MapHeader.version < MINBSPVERSION || s_MapHeader.version > BSPVERSION )
	{
		Host_Error( "CMapLoadHelper::Init, map %s has wrong version (%i when expecting %i)\n", s_szMapName, s_MapHeader.version, BSPVERSION );
		return;
	}

	// Store map version
	g_ServerGlobalVariables.mapversion = s_MapHeader.mapRevision;

#ifndef SWDS
	InitDLightGlobals( s_MapHeader.version );
#endif

	s_pMap = &g_ModelLoader.m_worldBrushData;
}

//-----------------------------------------------------------------------------
// Shutdown a BSP loading context.
//-----------------------------------------------------------------------------
void CMapLoadHelper::Shutdown( void )
{
	if ( --s_nMapLoadRecursion > 0 )
	{
		return;
	}

	if ( s_MapFileHandle != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Close( s_MapFileHandle );
		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
	}

	if ( IsPC() )
	{
		// Close our open lump files
		for ( int i = 0; i < HEADER_LUMPS; i++ )
		{
			if ( s_MapLumpFiles[i].file != FILESYSTEM_INVALID_HANDLE )
			{
				g_pFileSystem->Close( s_MapLumpFiles[i].file );
			}
		}
		V_memset( &s_MapLumpFiles, 0, sizeof( s_MapLumpFiles ) );
	}

	s_szLoadName[ 0 ] = 0;
	V_memset( &s_MapHeader, 0, sizeof( s_MapHeader ) );
	s_pMap = NULL;

	// discard from memory
	if ( s_MapBuffer.Base() )
	{
		free( s_MapBuffer.Base() );
		s_MapBuffer.SetExternalBuffer( NULL, 0, 0 );
	}
}


//-----------------------------------------------------------------------------
// Free the lighting lump (increases free memory during loading on 360)
//-----------------------------------------------------------------------------
void CMapLoadHelper::FreeLightingLump( void )
{
	if ( IsX360() && ( s_MapFileHandle == FILESYSTEM_INVALID_HANDLE ) && s_MapBuffer.Base() )
	{
		int lightingLump = LumpSize( LUMP_LIGHTING_HDR ) ? LUMP_LIGHTING_HDR : LUMP_LIGHTING;
		// Should never have both lighting lumps on 360
		Assert( ( lightingLump == LUMP_LIGHTING ) || ( LumpSize( LUMP_LIGHTING ) == 0 ) );

		if ( LumpSize( lightingLump ) )
		{
			// Check that the lighting lump is the last one in the BSP
			int lightingOffset = LumpOffset( lightingLump );
			for ( int i = 0;i < HEADER_LUMPS; i++ )
			{
				if ( ( LumpOffset( i ) > lightingOffset ) && ( i != LUMP_PAKFILE ) )
				{
					Warning( "CMapLoadHelper: Cannot free lighting lump (should be last before the PAK lump). Regenerate the .360.bsp file with the latest version of makegamedata." );
					return;
				}
			}

			// Flag the lighting chunk as gone from the BSP (principally, this sets 'filelen' to 0)
			V_memset( &s_MapHeader.lumps[ lightingLump ], 0, sizeof( lump_t ) );

			// Shrink the buffer to free up the space that was used by the lighting lump
			void * shrunkBuffer = realloc( s_MapBuffer.Base(), lightingOffset );
			Assert( shrunkBuffer == s_MapBuffer.Base() ); // A shrink would surely never move!!!
			s_MapBuffer.SetExternalBuffer( shrunkBuffer, lightingOffset, lightingOffset );
		}
	}
}


//-----------------------------------------------------------------------------
// Returns the size of a particular lump without loading it...
//-----------------------------------------------------------------------------
int CMapLoadHelper::LumpSize( int lumpId )
{
	// If we have a lump file for this lump, return its length instead
	if ( IsPC() && s_MapLumpFiles[lumpId].file != FILESYSTEM_INVALID_HANDLE )
	{
		return s_MapLumpFiles[lumpId].header.lumpLength;
	}

	lump_t *pLump = &s_MapHeader.lumps[ lumpId ];
	Assert( pLump );

	// all knowledge of compression is private, they expect and get the original size
	int originalSize = s_MapHeader.lumps[lumpId].uncompressedSize;
	if ( originalSize != 0 )
	{
		return originalSize;
	}

	return pLump->filelen;
}

//-----------------------------------------------------------------------------
// Returns the offset of a particular lump without loading it...
//-----------------------------------------------------------------------------
int CMapLoadHelper::LumpOffset( int lumpID  )
{
	// If we have a lump file for this lump, return 
	// the offset to move past the lump file header.
	if ( IsPC() && s_MapLumpFiles[lumpID].file != FILESYSTEM_INVALID_HANDLE )
	{
		return s_MapLumpFiles[lumpID].header.lumpOffset;
	}

	lump_t *pLump = &s_MapHeader.lumps[ lumpID ];
	Assert( pLump );

	return pLump->fileofs;
}

//-----------------------------------------------------------------------------
// Loads one element in a lump.
//-----------------------------------------------------------------------------
void CMapLoadHelper::LoadLumpElement( int nElemIndex, int nElemSize, void *pData )
{
	if ( !nElemSize || !m_nLumpSize )
	{
		return;
	}

	// supply from memory
	if ( nElemIndex * nElemSize + nElemSize <= m_nLumpSize )
	{
		V_memcpy( pData, m_pData + nElemIndex * nElemSize, nElemSize );
	}
	else
	{
		// out of range
		Assert( 0 );
	}
}


//-----------------------------------------------------------------------------
// Loads one element in a lump.
//-----------------------------------------------------------------------------
void CMapLoadHelper::LoadLumpData( int offset, int size, void *pData )
{
	if ( !size || !m_nLumpSize )
	{
		return;
	}

	if ( offset + size <= m_nLumpSize )
	{
		V_memcpy( pData, m_pData + offset, size );
	}
	else
	{
		// out of range
		Assert( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mapfile - 
//			lumpToLoad - 
//-----------------------------------------------------------------------------
CMapLoadHelper::CMapLoadHelper( int lumpToLoad )
{
	if ( lumpToLoad < 0 || lumpToLoad >= HEADER_LUMPS )
	{
		Sys_Error( "Can't load lump %i, range is 0 to %i!!!", lumpToLoad, HEADER_LUMPS - 1 );
	}

	m_nLumpID = lumpToLoad;
	m_nLumpSize = 0;
	m_nLumpOffset = -1;
	m_pData = NULL;
	m_pRawData = NULL;
	m_pUncompressedData = NULL;
	
	// Load raw lump from disk
	lump_t *lump = &s_MapHeader.lumps[ lumpToLoad ];
	Assert( lump );

	m_nLumpSize = lump->filelen;
	m_nLumpOffset = lump->fileofs;
	m_nLumpVersion = lump->version;	

	FileHandle_t fileToUse = s_MapFileHandle;

	// If we have a lump file for this lump, use it instead
	if ( IsPC() && s_MapLumpFiles[lumpToLoad].file != FILESYSTEM_INVALID_HANDLE )
	{
		fileToUse = s_MapLumpFiles[lumpToLoad].file;
		m_nLumpSize = s_MapLumpFiles[lumpToLoad].header.lumpLength;
		m_nLumpOffset = s_MapLumpFiles[lumpToLoad].header.lumpOffset;
		m_nLumpVersion = s_MapLumpFiles[lumpToLoad].header.lumpVersion;

		// Store off the lump file name
		GenerateLumpFileName( s_szLoadName, m_szLumpFilename, MAX_PATH, s_MapLumpFiles[lumpToLoad].lumpfileindex );
	}

	if ( !m_nLumpSize )
	{
		// this lump has no data
		return;
	}

	if ( s_MapBuffer.Base() )
	{
		// bsp is in memory
		m_pData = (unsigned char*)s_MapBuffer.Base() + m_nLumpOffset;
	}
	else
	{
		if ( s_MapFileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			Sys_Error( "Can't load map from invalid handle!!!" );
		}

		unsigned nOffsetAlign, nSizeAlign, nBufferAlign;
		g_pFileSystem->GetOptimalIOConstraints( fileToUse, &nOffsetAlign, &nSizeAlign, &nBufferAlign );

		bool bTryOptimal = ( m_nLumpOffset % 4 == 0 ); // Don't return badly aligned data
		unsigned int alignedOffset = m_nLumpOffset;
		unsigned int alignedBytesToRead = ( ( m_nLumpSize ) ? m_nLumpSize : 1 );

		if ( bTryOptimal )
		{
			alignedOffset = AlignValue( ( alignedOffset - nOffsetAlign ) + 1, nOffsetAlign );
			alignedBytesToRead = AlignValue( ( m_nLumpOffset - alignedOffset ) + alignedBytesToRead, nSizeAlign );
		}

		m_pRawData = (byte *)g_pFileSystem->AllocOptimalReadBuffer( fileToUse, alignedBytesToRead, alignedOffset );
		if ( !m_pRawData && m_nLumpSize )
		{
			Sys_Error( "Can't load lump %i, allocation of %i bytes failed!!!", lumpToLoad, m_nLumpSize + 1 );
		}

		if ( m_nLumpSize )
		{
			g_pFileSystem->Seek( fileToUse, alignedOffset, FILESYSTEM_SEEK_HEAD );
			g_pFileSystem->ReadEx( m_pRawData, alignedBytesToRead, alignedBytesToRead, fileToUse );
			m_pData = m_pRawData + ( m_nLumpOffset - alignedOffset );
		}
	}

	if ( lump->uncompressedSize != 0 )
	{
		// Handle compressed lump -- users of the class see the uncompressed data
		AssertMsg( CLZMA::IsCompressed( m_pData ),
		           "Lump claims to be compressed but is not recognized as LZMA" );

		m_nLumpSize = CLZMA::GetActualSize( m_pData );
		AssertMsg( lump->uncompressedSize == m_nLumpSize,
		           "Lump header disagrees with lzma header for compressed lump" );

		m_pUncompressedData = (unsigned char *)malloc( m_nLumpSize );
		CLZMA::Uncompress( m_pData, m_pUncompressedData );

		m_pData = m_pUncompressedData;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapLoadHelper::~CMapLoadHelper( void )
{
	if ( m_pUncompressedData )
	{
		free( m_pUncompressedData );
	}

	if ( m_pRawData )
	{
		g_pFileSystem->FreeOptimalReadBuffer( m_pRawData );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : model_t
//-----------------------------------------------------------------------------
worldbrushdata_t *CMapLoadHelper::GetMap( void )
{
	Assert( s_pMap );
	return s_pMap;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CMapLoadHelper::GetMapName( void )
{
	return s_szMapName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
char *CMapLoadHelper::GetLoadName( void )
{
	// If we have a custom lump file for the lump this helper 
	// is loading, return it instead.
	if ( IsPC() && s_MapLumpFiles[m_nLumpID].file != FILESYSTEM_INVALID_HANDLE )
	{
		return m_szLumpFilename;
	}

	return s_szLoadName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : byte
//-----------------------------------------------------------------------------
byte *CMapLoadHelper::LumpBase( void )
{
	return m_pData;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CMapLoadHelper::LumpSize()
{
	return m_nLumpSize;
}

int CMapLoadHelper::LumpOffset()
{
	return m_nLumpOffset;
}

int	CMapLoadHelper::LumpVersion() const
{
	return m_nLumpVersion;
}

void EnableHDR( bool bEnable )
{
	if ( g_pMaterialSystemHardwareConfig->GetHDREnabled() == bEnable )
		return;

	g_pMaterialSystemHardwareConfig->SetHDREnabled( bEnable );

	if ( IsX360() )
	{
		// cannot do what the pc does and ditch resources, we're loading!
		// can safely do the state update only, knowing that the state change won't affect 360 resources
		((MaterialSystem_Config_t *)g_pMaterialSystemConfig)->SetFlag( MATSYS_VIDCFG_FLAGS_ENABLE_HDR, bEnable );
		return;
	}

	ShutdownWellKnownRenderTargets();
	InitWellKnownRenderTargets();

	/// XXX(JohnS): This works around part of the terribleness the comments below discuss by performing
	///             UpdateMaterialSystemConfig with the device offline, removing its need to do multiple re-uploads of
	///             things.  I am not positive my changes to allow that won't introduce terrible regressions or awaken
	///             ancient bugs, hence the kill switch.
	bool bUpdateOffline = mod_offline_hdr_switch.GetBool();
#ifndef DEDICATED
	extern void V_RenderVGuiOnly();
#endif

	if ( bUpdateOffline )
	{
#ifndef DEDICATED
		V_RenderVGuiOnly();
#endif
		materials->ReleaseResources();
	}

	// Grah. This is terrible. changin mat_hdr_enabled at the commandline
	// will by definition break because the release/restore methods don't call
	// ShutdownWellKnownRenderTargets/InitWellKnownRenderTargets.
	// Also, this forces two alt-tabs, one for InitWellKnownRenderTargets, one
	// for UpdateMaterialSystemConfig.
	UpdateMaterialSystemConfig();

	// Worse, since we need to init+shutdown render targets here, we can't
	// rely on UpdateMaterialSystemConfig to release + reacquire resources
	// because it could be called at any time. We have to precisely control
	// when hdr is changed since this is the only time the code can handle it.
	if ( !bUpdateOffline )
	{
		materials->ReleaseResources();
	}
	materials->ReacquireResources();
#ifndef DEDICATED
	if ( bUpdateOffline )
	{
		V_RenderVGuiOnly();
	}
#endif
}

//-----------------------------------------------------------------------------
// Determine feature flags
//-----------------------------------------------------------------------------
void Map_CheckFeatureFlags()
{
	g_bLoadedMapHasBakedPropLighting = false;
	g_bBakedPropLightingNoSeparateHDR = false;

	if ( CMapLoadHelper::LumpSize( LUMP_MAP_FLAGS ) > 0 )
	{
		CMapLoadHelper lh( LUMP_MAP_FLAGS );
		dflagslump_t flags_lump;
		flags_lump = *( (dflagslump_t *)( lh.LumpBase() ) );

		// check if loaded map has baked static prop lighting
		g_bLoadedMapHasBakedPropLighting = 
			( flags_lump.m_LevelFlags & LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_NONHDR ) != 0 ||
			( flags_lump.m_LevelFlags & LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_HDR ) != 0;
		g_bBakedPropLightingNoSeparateHDR = 
			( flags_lump.m_LevelFlags & LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_HDR ) == 0;
	}
}

//-----------------------------------------------------------------------------
// Parse the map header for HDR ability. Returns the presence of HDR data only,
// not the HDR enable state.
//-----------------------------------------------------------------------------
bool Map_CheckForHDR( model_t *pModel, const char *pLoadName )
{
	// parse the map header only
	CMapLoadHelper::Init( pModel, pLoadName );

	bool bHasHDR = false;
	if ( IsX360() )
	{
		// If this is true, the 360 MUST use HDR, because the LDR data gets stripped out.
		bHasHDR = CMapLoadHelper::LumpSize( LUMP_LIGHTING_HDR ) > 0;
	}
	else
	{
		// might want to also consider the game lumps GAMELUMP_DETAIL_PROP_LIGHTING_HDR
		bHasHDR = CMapLoadHelper::LumpSize( LUMP_LIGHTING_HDR ) > 0 &&
			CMapLoadHelper::LumpSize( LUMP_WORLDLIGHTS_HDR ) > 0;
		//			 Mod_GameLumpSize( GAMELUMP_DETAIL_PROP_LIGHTING_HDR ) > 0  // fixme
	}
	if ( s_MapHeader.version >= 20 && CMapLoadHelper::LumpSize( LUMP_LEAF_AMBIENT_LIGHTING_HDR ) == 0 )
	{
		// This lump only exists in version 20 and greater, so don't bother checking for it on earlier versions.
		bHasHDR = false;
	}
	
	bool bEnableHDR = ( IsX360() && bHasHDR ) ||
		( bHasHDR && ( mat_hdr_level.GetInt() >= 2 ) &&
		( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 90 ) );
	
	EnableHDR( bEnableHDR );

	// this data really should have been in the header, but it isn't
	// establish the features now, before the real bsp load commences
	Map_CheckFeatureFlags();

	CMapLoadHelper::Shutdown();

	return bHasHDR;
}

//-----------------------------------------------------------------------------
// Allocates, frees lighting data
//-----------------------------------------------------------------------------
static void AllocateLightingData( worldbrushdata_t *pBrushData, int nSize )
{
	g_bHunkAllocLightmaps = ( !r_unloadlightmaps.GetBool() && r_hunkalloclightmaps.GetBool() );
	if ( g_bHunkAllocLightmaps )
	{
		pBrushData->lightdata = (ColorRGBExp32 *)Hunk_Alloc( nSize, false );
	}
	else
	{
		// Specifically *not* adding it to the hunk.
		// If this malloc changes, also change the free in CacheAndUnloadLightmapData()
		pBrushData->lightdata = (ColorRGBExp32 *)malloc( nSize );
	}
	pBrushData->unloadedlightmaps = false;
}

static void DeallocateLightingData( worldbrushdata_t *pBrushData )
{
	if ( pBrushData && pBrushData->lightdata )
	{
		if ( !g_bHunkAllocLightmaps )
		{
			free( pBrushData->lightdata );
		}
		pBrushData->lightdata = NULL;
	}
}

static int ComputeLightmapSize( dface_t *pFace, mtexinfo_t *pTexInfo )
{
	bool bNeedsBumpmap = false;
	if( pTexInfo[pFace->texinfo].flags & SURF_BUMPLIGHT )
	{
		bNeedsBumpmap = true;
	}

    int lightstyles;
    for (lightstyles=0; lightstyles < MAXLIGHTMAPS; lightstyles++ )
    {
        if ( pFace->styles[lightstyles] == 255 )
            break;
    }

	int nLuxels = (pFace->m_LightmapTextureSizeInLuxels[0]+1) * (pFace->m_LightmapTextureSizeInLuxels[1]+1);
	if( bNeedsBumpmap )
	{
		return nLuxels * 4 * lightstyles * ( NUM_BUMP_VECTS + 1 );
	}

	return nLuxels * 4 * lightstyles;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadLighting( CMapLoadHelper &lh )
{
	if ( !lh.LumpSize() )
	{
		lh.GetMap()->lightdata = NULL;
		return;
	}

	Assert( lh.LumpSize() % sizeof( ColorRGBExp32 ) == 0 );
	Assert ( lh.LumpVersion() != 0 );

	AllocateLightingData( lh.GetMap(), lh.LumpSize() );
	memcpy( lh.GetMap()->lightdata, lh.LumpBase(), lh.LumpSize());

	if ( IsX360() )
	{
		// Free the lighting lump, to increase the amount of memory free during the rest of loading
		CMapLoadHelper::FreeLightingLump();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadWorldlights( CMapLoadHelper &lh, bool bIsHDR )
{
	lh.GetMap()->shadowzbuffers = NULL;
	if (!lh.LumpSize())
	{
		lh.GetMap()->numworldlights = 0;
		lh.GetMap()->worldlights = NULL;
		return;
	}
	lh.GetMap()->numworldlights = lh.LumpSize() / sizeof( dworldlight_t );
	lh.GetMap()->worldlights = (dworldlight_t *)Hunk_AllocName( lh.LumpSize(), va( "%s [%s]", lh.GetLoadName(), "worldlights" ) );
	memcpy (lh.GetMap()->worldlights, lh.LumpBase(), lh.LumpSize());
#if !defined( SWDS )
	if ( r_lightcache_zbuffercache.GetInt() )
	{
		size_t zbufSize = lh.GetMap()->numworldlights * sizeof( lightzbuffer_t );
		lh.GetMap()->shadowzbuffers = ( lightzbuffer_t *) Hunk_AllocName( zbufSize, va( "%s [%s]", lh.GetLoadName(), "shadowzbuffers" ) );
		memset( lh.GetMap()->shadowzbuffers, 0, zbufSize );		// mark empty
	}
#endif

	// Fixup for backward compatability
	for ( int i = 0; i < lh.GetMap()->numworldlights; i++ )
	{
		if( lh.GetMap()->worldlights[i].type == emit_spotlight)
		{
			if ((lh.GetMap()->worldlights[i].constant_attn == 0.0) && 
				(lh.GetMap()->worldlights[i].linear_attn == 0.0) && 
				(lh.GetMap()->worldlights[i].quadratic_attn == 0.0))
			{
				lh.GetMap()->worldlights[i].quadratic_attn = 1.0;
			}

			if (lh.GetMap()->worldlights[i].exponent == 0.0)
				lh.GetMap()->worldlights[i].exponent = 1.0;
		}
		else if( lh.GetMap()->worldlights[i].type == emit_point)
		{
			// To match earlier lighting, use quadratic...
			if ((lh.GetMap()->worldlights[i].constant_attn == 0.0) && 
				(lh.GetMap()->worldlights[i].linear_attn == 0.0) && 
				(lh.GetMap()->worldlights[i].quadratic_attn == 0.0))
			{
				lh.GetMap()->worldlights[i].quadratic_attn = 1.0;
			}
		}

   		// I replaced the cuttoff_dot field (which took a value from 0 to 1)
		// with a max light radius. Radius of less than 1 will never happen,
		// so I can get away with this. When I set radius to 0, it'll 
		// run the old code which computed a radius
		if (lh.GetMap()->worldlights[i].radius < 1)
		{
			lh.GetMap()->worldlights[i].radius = ComputeLightRadius( &lh.GetMap()->worldlights[i], bIsHDR );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadVertices( void )
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	CMapLoadHelper lh( LUMP_VERTEXES );

	in = (dvertex_t *)lh.LumpBase();
	if ( lh.LumpSize() % sizeof(*in) )
	{
		Host_Error( "Mod_LoadVertices: funny lump size in %s", lh.GetMapName() );
	}
	count = lh.LumpSize() / sizeof(*in);
	out = (mvertex_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "vertexes" ) );

	lh.GetMap()->vertexes = out;
	lh.GetMap()->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = in->point[0];
		out->position[1] = in->point[1];
		out->position[2] = in->point[2];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mins - 
//			maxs - 
// Output : float
//-----------------------------------------------------------------------------
static float RadiusFromBounds (Vector& mins, Vector& maxs)
{
	int		i;
	Vector	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength( corner );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadSubmodels( CUtlVector<mmodel_t> &submodelList )
{
	dmodel_t	*in;
	int			i, j, count;

	CMapLoadHelper lh( LUMP_MODELS );

	in = (dmodel_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error("Mod_LoadSubmodels: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);

	submodelList.SetCount( count );
	lh.GetMap()->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			submodelList[i].mins[j] = in->mins[j] - 1;
			submodelList[i].maxs[j] = in->maxs[j] + 1;
			submodelList[i].origin[j] = in->origin[j];
		}
		submodelList[i].radius = RadiusFromBounds (submodelList[i].mins, submodelList[i].maxs);
		submodelList[i].headnode = in->headnode;
		submodelList[i].firstface = in->firstface;
		submodelList[i].numfaces = in->numfaces;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : medge_t *Mod_LoadEdges
//-----------------------------------------------------------------------------
medge_t *Mod_LoadEdges ( void )
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	CMapLoadHelper lh( LUMP_EDGES );

	in = (dedge_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadEdges: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	medge_t *pedges = new medge_t[count];

	out = pedges;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = in->v[0];
		out->v[1] = in->v[1];
	}

	// delete this in the loader
	return pedges;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadOcclusion( void )
{
	CMapLoadHelper lh( LUMP_OCCLUSION );

	worldbrushdata_t *b = lh.GetMap();
	b->numoccluders = 0;
	b->occluders = NULL;
	b->numoccluderpolys = 0;
	b->occluderpolys = NULL;
	b->numoccludervertindices = 0;
	b->occludervertindices = NULL;

	if ( !lh.LumpSize() )
	{
		return;
	}

	CUtlBuffer buf( lh.LumpBase(), lh.LumpSize(), CUtlBuffer::READ_ONLY );

	switch( lh.LumpVersion() )
	{
	case LUMP_OCCLUSION_VERSION:
		{
			b->numoccluders = buf.GetInt();
			if (b->numoccluders)
			{
				int nSize = b->numoccluders * sizeof(doccluderdata_t);
				b->occluders = (doccluderdata_t*)Hunk_AllocName( nSize, "occluder data" );
				buf.Get( b->occluders, nSize );
			}

			b->numoccluderpolys = buf.GetInt();
			if (b->numoccluderpolys)
			{
				int nSize = b->numoccluderpolys * sizeof(doccluderpolydata_t);
				b->occluderpolys = (doccluderpolydata_t*)Hunk_AllocName( nSize, "occluder poly data" );
				buf.Get( b->occluderpolys, nSize );
			}

			b->numoccludervertindices = buf.GetInt();
			if (b->numoccludervertindices)
			{
				int nSize = b->numoccludervertindices * sizeof(int);
				b->occludervertindices = (int*)Hunk_AllocName( nSize, "occluder vertices" );
				buf.Get( b->occludervertindices, nSize );
			}
		}
		break;

	case 1:
		{
			b->numoccluders = buf.GetInt();
			if (b->numoccluders)
			{
				int nSize = b->numoccluders * sizeof(doccluderdata_t);
				b->occluders = (doccluderdata_t*)Hunk_AllocName( nSize, "occluder data" );

				doccluderdataV1_t temp;
				for ( int i = 0; i < b->numoccluders; ++i )
				{
					buf.Get( &temp, sizeof(doccluderdataV1_t) );
					memcpy( &b->occluders[i], &temp, sizeof(doccluderdataV1_t) );
					b->occluders[i].area = 1;
				}
			}

			b->numoccluderpolys = buf.GetInt();
			if (b->numoccluderpolys)
			{
				int nSize = b->numoccluderpolys * sizeof(doccluderpolydata_t);
				b->occluderpolys = (doccluderpolydata_t*)Hunk_AllocName( nSize, "occluder poly data" );
				buf.Get( b->occluderpolys, nSize );
			}

			b->numoccludervertindices = buf.GetInt();
			if (b->numoccludervertindices)
			{
				int nSize = b->numoccludervertindices * sizeof(int);
				b->occludervertindices = (int*)Hunk_AllocName( nSize, "occluder vertices" );
				buf.Get( b->occludervertindices, nSize );
			}
		}
		break;

	case 0:
		break;

	default:
		Host_Error("Invalid occlusion lump version!\n");
		break;
	}
}



// UNDONE: Really, it's stored 2 times because the texture system keeps a 
// copy of the name too.  I guess we'll get rid of this when we have a material
// system that works without a graphics context.  At that point, everyone can
// reference the name in the material, or just the material itself.
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadTexdata( void )
{
	// Don't bother loading these again; they're already stored in the collision model
	// which is guaranteed to be loaded at this point
	s_pMap->numtexdata = GetCollisionBSPData()->numtextures;
	s_pMap->texdata = GetCollisionBSPData()->map_surfaces.Base();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadTexinfo( void )
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count;
	// UNDONE: Fix this

	CMapLoadHelper lh( LUMP_TEXINFO );

	in = (texinfo_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadTexinfo: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mtexinfo_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "texinfo" ) );

	s_pMap->texinfo = out;
	s_pMap->numtexinfo = count;

	bool loadtextures = mat_loadtextures.GetBool();

	for ( i=0 ; i<count ; ++i, ++in, ++out )
	{
		for (j=0; j<2; ++j)
		{
			for (int k=0 ; k<4 ; ++k)
			{
				out->textureVecsTexelsPerWorldUnits[j][k] = in->textureVecsTexelsPerWorldUnits[j][k];
				out->lightmapVecsLuxelsPerWorldUnits[j][k] = in->lightmapVecsLuxelsPerWorldUnits[j][k] ;
			}
		}

		// assume that the scale is the same on both s and t.
		out->luxelsPerWorldUnit = VectorLength( out->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D() );
		out->worldUnitsPerLuxel = 1.0f / out->luxelsPerWorldUnit;

		out->flags = in->flags;
		out->texinfoFlags = 0;

		if ( loadtextures )
		{
			if ( in->texdata >= 0 )
			{
				out->material = GL_LoadMaterial( lh.GetMap()->texdata[ in->texdata ].name, TEXTURE_GROUP_WORLD );
			}
			else
			{
				DevMsg( "Mod_LoadTexinfo: texdata < 0 (index==%i/%i)\n", i, count );
				out->material = NULL;
			}
			if ( !out->material )
			{
				out->material = g_materialEmpty;
				g_materialEmpty->IncrementReferenceCount();
			}
 		}
		else
		{
			out->material = g_materialEmpty;
			g_materialEmpty->IncrementReferenceCount();
		}
	}
}

// code to scan the lightmaps for empty lightstyles
static void LinearToGamma( unsigned char *pDstRGB, const float *pSrcRGB )
{
	pDstRGB[0] = LinearToScreenGamma( pSrcRGB[0] );
	pDstRGB[1] = LinearToScreenGamma( pSrcRGB[1] );
	pDstRGB[2] = LinearToScreenGamma( pSrcRGB[2] );
}

static void CheckSurfaceLighting( SurfaceHandle_t surfID, worldbrushdata_t *pBrushData )
{
#if !defined( SWDS )
	host_state.worldbrush = pBrushData;
	msurfacelighting_t *pLighting = SurfaceLighting( surfID, pBrushData );

	if( !pLighting->m_pSamples )
		return;

	int smax = ( pLighting->m_LightmapExtents[0] ) + 1;
	int tmax = ( pLighting->m_LightmapExtents[1] ) + 1;
	int offset = smax * tmax;
	if ( SurfHasBumpedLightmaps( surfID ) )
	{
		offset *= ( NUM_BUMP_VECTS + 1 );
	}


	// how many lightmaps does this surface have?
	int maxLightmapIndex = 0;
	for (int maps = 1 ; maps < MAXLIGHTMAPS && pLighting->m_nStyles[maps] != 255 ; ++maps)
	{
		maxLightmapIndex = maps;
	}

	if ( maxLightmapIndex < 1 )
		return;

	// iterate and test each lightmap
	for ( int maps = maxLightmapIndex; maps != 0; maps-- )
	{
		ColorRGBExp32 *pLightmap = pLighting->m_pSamples + (maps * offset);
		float maxLen = -1;
		Vector maxLight;
		maxLight.Init();
		for ( int i = 0; i < offset; i++ )
		{
			Vector c;
			ColorRGBExp32ToVector( pLightmap[i], c );
			if ( c.Length() > maxLen )
			{
				maxLight = c;
				maxLen = c.Length();
			}
		}
		unsigned char color[4];
		LinearToGamma( color, maxLight.Base() );
		const int minLightVal = 1;
		if ( color[0] <= minLightVal && color[1] <= minLightVal && color[2] <= minLightVal )
		{
			// found a lightmap that is too dark, remove it and shift over the subsequent maps/styles
			for ( int i = maps; i < maxLightmapIndex; i++ )
			{
				ColorRGBExp32 *pLightmapOverwrite = pLighting->m_pSamples + (i * offset);
				memcpy( pLightmapOverwrite, pLightmapOverwrite+offset, offset * sizeof(*pLightmapOverwrite) );
				pLighting->m_nStyles[i] = pLighting->m_nStyles[i+1];
			}
			// mark end lightstyle as removed, decrement max index
			pLighting->m_nStyles[maxLightmapIndex] = 255;
			maxLightmapIndex--;
		}
	}
	// we removed all of the lightstyle maps so clear the flag
	if ( maxLightmapIndex == 0 )
	{
		surfID->flags &= ~SURFDRAW_HASLIGHTSYTLES;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*s - 
// Output : void CalcSurfaceExtents
//-----------------------------------------------------------------------------
static void CalcSurfaceExtents ( CMapLoadHelper& lh, SurfaceHandle_t surfID )
{
	float	textureMins[2], textureMaxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	textureMins[0] = textureMins[1] = 999999;
	textureMaxs[0] = textureMaxs[1] = -99999;

	worldbrushdata_t *pBrushData = lh.GetMap();
	tex = MSurf_TexInfo( surfID, pBrushData );
	
	for (i=0 ; i<MSurf_VertCount( surfID ); i++)
	{
		e = pBrushData->vertindices[MSurf_FirstVertIndex( surfID )+i];
		v = &pBrushData->vertexes[e];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->textureVecsTexelsPerWorldUnits[j][0] + 
				  v->position[1] * tex->textureVecsTexelsPerWorldUnits[j][1] +
				  v->position[2] * tex->textureVecsTexelsPerWorldUnits[j][2] +
				  tex->textureVecsTexelsPerWorldUnits[j][3];
			if (val < textureMins[j])
				textureMins[j] = val;
			if (val > textureMaxs[j])
				textureMaxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		if( MSurf_LightmapExtents( surfID, pBrushData )[i] == 0 && !MSurf_Samples( surfID, pBrushData ) )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NOLIGHT;
		}

		bmins[i] = Float2Int( textureMins[i] );
		bmaxs[i] = Ceil2Int( textureMaxs[i] );
		MSurf_TextureMins( surfID, pBrushData )[i] = bmins[i];
		MSurf_TextureExtents( surfID, pBrushData )[i] = ( bmaxs[i] - bmins[i] );

		if ( !(tex->flags & SURF_NOLIGHT) && MSurf_LightmapExtents( surfID, pBrushData )[i] > MSurf_MaxLightmapSizeWithBorder( surfID ) )
		{
			Sys_Error ("Bad surface extents on texture %s", tex->material->GetName() );
		}
	}
	CheckSurfaceLighting( surfID, pBrushData );
}

//-----------------------------------------------------------------------------
// Input  : *pModel - 
//			*pLump - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadVertNormals( void )
{
	CMapLoadHelper lh( LUMP_VERTNORMALS );

    // get a pointer to the vertex normal data.
	Vector *pVertNormals = ( Vector * )lh.LumpBase();

    //
    // verify vertnormals data size
    //
    if( lh.LumpSize() % sizeof( *pVertNormals ) )
        Host_Error( "Mod_LoadVertNormals: funny lump size in %s!\n", lh.GetMapName() );

	int count = lh.LumpSize() / sizeof(*pVertNormals);
	Vector *out = (Vector *)Hunk_AllocName( lh.LumpSize(), va( "%s [%s]", lh.GetLoadName(), "vertnormals" ) );
	memcpy( out, pVertNormals, lh.LumpSize() );
	
	lh.GetMap()->vertnormals = out;
	lh.GetMap()->numvertnormals = count;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadVertNormalIndices( void )
{
	CMapLoadHelper lh( LUMP_VERTNORMALINDICES );

    // get a pointer to the vertex normal data.
	unsigned short *pIndices = ( unsigned short * )lh.LumpBase();

	int count = lh.LumpSize() / sizeof(*pIndices);
	unsigned short *out = (unsigned short *)Hunk_AllocName( lh.LumpSize(), va( "%s [%s]", lh.GetLoadName(), "vertnormalindices" ) );
	memcpy( out, pIndices, lh.LumpSize() );
	
	lh.GetMap()->vertnormalindices = out;
	lh.GetMap()->numvertnormalindices = count;

	// OPTIMIZE: Water surfaces don't need vertex normals?
	int normalIndex = 0;
	for( int i = 0; i < lh.GetMap()->numsurfaces; i++ )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( i, lh.GetMap() );
		MSurf_FirstVertNormal( surfID, lh.GetMap() ) = normalIndex;
		normalIndex += MSurf_VertCount( surfID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadPrimitives( void )
{
	dprimitive_t	*in;
	mprimitive_t	*out;
	int				i, count;

	CMapLoadHelper lh( LUMP_PRIMITIVES );

	in = (dprimitive_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadPrimitives: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mprimitive_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "primitives" ) );
	memset( out, 0, count * sizeof( mprimitive_t ) );

	lh.GetMap()->primitives = out;
	lh.GetMap()->numprimitives = count;
	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->firstIndex		= in->firstIndex;
		out->firstVert		= in->firstVert;
		out->indexCount		= in->indexCount;
		out->type			= in->type;
		out->vertCount		= in->vertCount;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadPrimVerts( void )
{
	dprimvert_t		*in;
	mprimvert_t		*out;
	int				i, count;

	CMapLoadHelper lh( LUMP_PRIMVERTS );

	in = (dprimvert_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadPrimVerts: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mprimvert_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "primverts" ) );
	memset( out, 0, count * sizeof( mprimvert_t ) );

	lh.GetMap()->primverts = out;
	lh.GetMap()->numprimverts = count;
	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->pos = in->pos;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadPrimIndices( void )
{
	unsigned short	*in;
	unsigned short	*out;
	int				count;

	CMapLoadHelper lh( LUMP_PRIMINDICES );

	in = (unsigned short *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadPrimIndices: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	out = (unsigned short *)Hunk_AllocName( count*sizeof(*out), va("%s [%s]", lh.GetLoadName(), "primindices" ) );
	memset( out, 0, count * sizeof( unsigned short ) );

	lh.GetMap()->primindices = out;
	lh.GetMap()->numprimindices = count;

	memcpy( out, in, count * sizeof( unsigned short ) );
}


// This allocates memory for a lump and copies the lump data in.
void Mod_LoadLump( 
	model_t *loadmodel, 
	int iLump,
	char *loadname, 
	int elementSize,
	void **ppData, 
	int *nElements )
{
	CMapLoadHelper lh( iLump );

	if ( lh.LumpSize() % elementSize )
	{
		Host_Error( "Mod_LoadLump: funny lump size in %s", loadmodel->strName.String() );
	}

	// How many elements?
	*nElements = lh.LumpSize() / elementSize;

	// Make room for the data and copy the data in.
	*ppData = Hunk_AllocName( lh.LumpSize(), loadname );
	memcpy( *ppData, lh.LumpBase(), lh.LumpSize() );
}


//-----------------------------------------------------------------------------
// Sets up the msurfacelighting_t structure
//-----------------------------------------------------------------------------
bool Mod_LoadSurfaceLightingV1( msurfacelighting_t *pLighting, dface_t *in, ColorRGBExp32 *pBaseLightData )
{
	// Get lightmap extents from the file.
	pLighting->m_LightmapExtents[0] = in->m_LightmapTextureSizeInLuxels[0];
	pLighting->m_LightmapExtents[1] = in->m_LightmapTextureSizeInLuxels[1];
	pLighting->m_LightmapMins[0] = in->m_LightmapTextureMinsInLuxels[0];
	pLighting->m_LightmapMins[1] = in->m_LightmapTextureMinsInLuxels[1];

	int i = in->lightofs;
	if ( (i == -1) || (!pBaseLightData) )
	{
		pLighting->m_pSamples = NULL;

		// Can't have *any* lightstyles if we have no samples....
		for ( i=0; i<MAXLIGHTMAPS; ++i)
		{
			pLighting->m_nStyles[i] = 255;
		}
	}
	else
	{
		pLighting->m_pSamples = (ColorRGBExp32 *)( ((byte *)pBaseLightData) + i );

		for (i=0 ; i<MAXLIGHTMAPS; ++i)
		{
			pLighting->m_nStyles[i] = in->styles[i];
		}
	}

	return ((pLighting->m_nStyles[0] != 0) && (pLighting->m_nStyles[0] != 255)) || (pLighting->m_nStyles[1] != 255);
}

void *Hunk_AllocNameAlignedClear_( int size, int alignment, const char *pHunkName )
{
	Assert(IsPowerOfTwo(alignment));
	void *pMem = Hunk_AllocName( alignment + size, pHunkName );
	memset( pMem, 0, size + alignment );
	pMem = (void *)( ( ( ( unsigned long )pMem ) + (alignment-1) ) & ~(alignment-1) );

	return pMem;
}

// Allocates a block of T from the hunk.  Aligns as specified and clears the memory
template< typename T > 
T *Hunk_AllocNameAlignedClear( int count, int alignment, const char *pHunkName )
{
	return (T *)Hunk_AllocNameAlignedClear_( alignment + count * sizeof(T), alignment, pHunkName );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadFaces( void )
{
	dface_t		*in;
	int			count, surfnum;
	int			planenum;
	int			ti, di;

	int face_lump_to_load = LUMP_FACES;
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE &&
		CMapLoadHelper::LumpSize( LUMP_FACES_HDR ) > 0 )
	{
		face_lump_to_load = LUMP_FACES_HDR;
	}
	CMapLoadHelper lh( face_lump_to_load );
	
	in = (dface_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadFaces: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);

	// align these allocations
	// If you trip one of these, you need to rethink the alignment of the struct
	Assert( sizeof(msurface1_t) == 16 );
	Assert( sizeof(msurface2_t) == 32 );
	Assert( sizeof(msurfacelighting_t) == 32 );

	msurface1_t *out1 = Hunk_AllocNameAlignedClear< msurface1_t >( count, 16, va( "%s [%s]", lh.GetLoadName(), "surface1" ) );
	msurface2_t *out2 = Hunk_AllocNameAlignedClear< msurface2_t >( count, 32, va( "%s [%s]", lh.GetLoadName(), "surface2" ) );

	msurfacelighting_t *pLighting = Hunk_AllocNameAlignedClear< msurfacelighting_t >( count, 32, va( "%s [%s]", lh.GetLoadName(), "surfacelighting" ) );

	lh.GetMap()->surfaces1 = out1;
	lh.GetMap()->surfaces2 = out2;
	lh.GetMap()->surfacelighting = pLighting;
	lh.GetMap()->surfacenormals = Hunk_AllocNameAlignedClear< msurfacenormal_t >( count, 2, va( "%s [%s]", lh.GetLoadName(), "surfacenormal" ) );
	lh.GetMap()->numsurfaces = count;

	worldbrushdata_t *pBrushData = lh.GetMap();

	for ( surfnum=0 ; surfnum<count ; ++surfnum, ++in, ++out1, ++out2, ++pLighting )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( surfnum, pBrushData );
		MSurf_FirstVertIndex( surfID )  = in->firstedge;
		
		int vertCount = in->numedges;
		MSurf_Flags( surfID ) = 0;
		Assert( vertCount <= 255 );
		MSurf_SetVertCount( surfID, vertCount );

		planenum = in->planenum;
		
		if ( in->onNode )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NODE;
		}
		if ( in->side )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_PLANEBACK;
		}

		out2->plane = lh.GetMap()->planes + planenum;

		ti = in->texinfo;
		if (ti < 0 || ti >= lh.GetMap()->numtexinfo)
		{
			Host_Error( "Mod_LoadFaces: bad texinfo number" );
		}
		surfID->texinfo = ti;
		surfID->m_bDynamicShadowsEnabled = in->AreDynamicShadowsEnabled();
		mtexinfo_t *pTex = lh.GetMap()->texinfo + ti;

		// big hack!
		if ( !pTex->material )
		{
			pTex->material = g_materialEmpty;
			g_materialEmpty->IncrementReferenceCount();
		}

		// lighting info
		if ( Mod_LoadSurfaceLightingV1( pLighting, in, lh.GetMap()->lightdata ) )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_HASLIGHTSYTLES;
		}

		// set the drawing flags flag
		if ( pTex->flags & SURF_NOLIGHT )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NOLIGHT;
		}
		
		if ( pTex->flags & SURF_NOSHADOWS )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NOSHADOWS;
		}

		if ( pTex->flags & SURF_WARP )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_WATERSURFACE;
		}

		if ( pTex->flags & SURF_SKY )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_SKY;
		}

        di = in->dispinfo;
		out2->pDispInfo = NULL;
        if( di != -1 )
        {
//			out->origSurfaceID = in->origFace;
			MSurf_Flags( surfID ) |= SURFDRAW_HAS_DISP;
        }
		else
		{
			// non-displacement faces shouldn't come out of VBSP if they have nodraw.
			Assert( !(pTex->flags & SURF_NODRAW) );

			out1->prims.numPrims = in->GetNumPrims();
			out1->prims.firstPrimID = in->firstPrimID;
			if ( in->GetNumPrims() )
			{
				MSurf_Flags( surfID ) |= SURFDRAW_HAS_PRIMS;
				mprimitive_t *pPrim = &pBrushData->primitives[in->firstPrimID];
				if ( pPrim->vertCount > 0 )
				{
					MSurf_Flags( surfID ) |= SURFDRAW_DYNAMIC;
				}
			}
		}
		
		// No shadows on the surface to start with
		out2->m_ShadowDecals = SHADOW_DECAL_HANDLE_INVALID;
		out2->decals = WORLD_DECAL_HANDLE_INVALID;

		// No overlays on the surface to start with
		out2->m_nFirstOverlayFragment = OVERLAY_FRAGMENT_INVALID;

		CalcSurfaceExtents( lh, surfID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *node - 
//			*parent - 
// Output : void Mod_SetParent
//-----------------------------------------------------------------------------
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents >= 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}


//-----------------------------------------------------------------------------
// Mark an entire subtree as being too small to bother with
//-----------------------------------------------------------------------------
static void MarkSmallNode( mnode_t *node )
{
	if (node->contents >= 0)
		return;
	node->contents = -2;
	MarkSmallNode (node->children[0]);
	MarkSmallNode (node->children[1]);
}

static void CheckSmallVolumeDifferences( mnode_t *pNode, const Vector &parentSize )
{
	if (pNode->contents >= 0)
		return;

	Vector delta;
	VectorSubtract( parentSize, pNode->m_vecHalfDiagonal, delta );

	if ((delta.x < 5) && (delta.y < 5) && (delta.z < 5))
	{
		pNode->contents = -3;
		CheckSmallVolumeDifferences( pNode->children[0], parentSize );
		CheckSmallVolumeDifferences( pNode->children[1], parentSize );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadNodes( void )
{
	Vector mins( 0, 0, 0 ), maxs( 0, 0, 0 );
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	CMapLoadHelper lh( LUMP_NODES );

	in = (dnode_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadNodes: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mnode_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "nodes" ) );

	lh.GetMap()->nodes = out;
	lh.GetMap()->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			mins[j] = in->mins[j];
			maxs[j] = in->maxs[j];
		}
	
		VectorAdd( mins, maxs, out->m_vecCenter );
		out->m_vecCenter *= 0.5f;
		VectorSubtract( maxs, out->m_vecCenter, out->m_vecHalfDiagonal );

		p = in->planenum;
		out->plane = lh.GetMap()->planes + p;

		out->firstsurface = in->firstface;
		out->numsurfaces = in->numfaces;
		out->area = in->area;
		out->contents = -1;	// differentiate from leafs

		for (j=0 ; j<2 ; j++)
		{
			p = in->children[j];
			if (p >= 0)
				out->children[j] = lh.GetMap()->nodes + p;
			else
				out->children[j] = (mnode_t *)(lh.GetMap()->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent (lh.GetMap()->nodes, NULL);	// sets nodes and leafs

	// Check for small-area parents... no culling below them...
	mnode_t *pNode = lh.GetMap()->nodes;
	for ( i=0 ; i<count ; ++i, ++pNode)
	{
		if (pNode->contents == -1)
		{
			if ((pNode->m_vecHalfDiagonal.x <= 50) && (pNode->m_vecHalfDiagonal.y <= 50) && 
				(pNode->m_vecHalfDiagonal.z <= 50))
			{
				// Mark all children as being too small to bother with...
				MarkSmallNode( pNode->children[0] );
				MarkSmallNode( pNode->children[1] );
			}
			else
			{
				CheckSmallVolumeDifferences( pNode->children[0], pNode->m_vecHalfDiagonal );
				CheckSmallVolumeDifferences( pNode->children[1], pNode->m_vecHalfDiagonal );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadLeafs_Version_0( CMapLoadHelper &lh )
{
	Vector mins( 0, 0, 0 ), maxs( 0, 0, 0 );
	dleaf_version_0_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (dleaf_version_0_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mleaf_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "leafs" ) );

	lh.GetMap()->leafs = out;
	lh.GetMap()->numleafs = count;

	// one sample per leaf
	lh.GetMap()->m_pLeafAmbient = (mleafambientindex_t *)Hunk_AllocName( count * sizeof(*lh.GetMap()->m_pLeafAmbient), "LeafAmbient" );
	lh.GetMap()->m_pAmbientSamples = (mleafambientlighting_t *)Hunk_AllocName( count * sizeof(*lh.GetMap()->m_pAmbientSamples), "LeafAmbientSamples" );
	mleafambientindex_t *pTable = lh.GetMap()->m_pLeafAmbient;
	mleafambientlighting_t *pSamples = lh.GetMap()->m_pAmbientSamples;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			mins[j] = in->mins[j];
			maxs[j] = in->maxs[j];
		}

		VectorAdd( mins, maxs, out->m_vecCenter );
		out->m_vecCenter *= 0.5f;
		VectorSubtract( maxs, out->m_vecCenter, out->m_vecHalfDiagonal );

		pTable[i].ambientSampleCount = 1;
		pTable[i].firstAmbientSample = i;
		pSamples[i].x = pSamples[i].y = pSamples[i].z = 128;
		pSamples[i].pad = 0;
		Q_memcpy( &pSamples[i].cube, &in->m_AmbientLighting, sizeof(pSamples[i].cube) );


		p = in->contents;
		out->contents = p;

		out->cluster = in->cluster;
		out->area = in->area;
		out->flags = in->flags;
/*
		out->firstmarksurface = lh.GetMap()->marksurfaces + in->firstleafface;
*/
		out->firstmarksurface = in->firstleafface;
		out->nummarksurfaces = in->numleaffaces;
		out->parent = NULL;
		
		out->dispCount = 0;

		out->leafWaterDataID = in->leafWaterDataID;
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadLeafs_Version_1( CMapLoadHelper &lh, CMapLoadHelper &ambientLightingLump, CMapLoadHelper &ambientLightingTable )
{
	Vector mins( 0, 0, 0 ), maxs( 0, 0, 0 );
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (dleaf_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mleaf_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "leafs" ) );

	lh.GetMap()->leafs = out;
	lh.GetMap()->numleafs = count;

	if ( ambientLightingLump.LumpVersion() != LUMP_LEAF_AMBIENT_LIGHTING_VERSION || ambientLightingTable.LumpSize() == 0 )
	{
		// convert from previous version
		CompressedLightCube *inLightCubes = NULL;
		if ( ambientLightingLump.LumpSize() )
		{
			inLightCubes = ( CompressedLightCube * )ambientLightingLump.LumpBase();
			Assert( ambientLightingLump.LumpSize() % sizeof( CompressedLightCube ) == 0 );
			Assert( ambientLightingLump.LumpSize() / sizeof( CompressedLightCube ) == lh.LumpSize() / sizeof( dleaf_t ) );
		}
		lh.GetMap()->m_pLeafAmbient = (mleafambientindex_t *)Hunk_AllocName( count * sizeof(*lh.GetMap()->m_pLeafAmbient), "LeafAmbient" );
		lh.GetMap()->m_pAmbientSamples = (mleafambientlighting_t *)Hunk_AllocName( count * sizeof(*lh.GetMap()->m_pAmbientSamples), "LeafAmbientSamples" );
		mleafambientindex_t *pTable = lh.GetMap()->m_pLeafAmbient;
		mleafambientlighting_t *pSamples = lh.GetMap()->m_pAmbientSamples;
		Vector gray(0.5, 0.5, 0.5);
		ColorRGBExp32 grayColor;
		VectorToColorRGBExp32( gray, grayColor );
		for ( i = 0; i < count; i++ )
		{
			pTable[i].ambientSampleCount = 1;
			pTable[i].firstAmbientSample = i;
			pSamples[i].x = pSamples[i].y = pSamples[i].z = 128;
			pSamples[i].pad = 0;
			if ( inLightCubes )
			{
				Q_memcpy( &pSamples[i].cube, &inLightCubes[i], sizeof(pSamples[i].cube) );
			}
			else
			{
				for ( j = 0; j < 6; j++ )
				{
					pSamples[i].cube.m_Color[j] = grayColor;
				}
			}
		}
	}
	else
	{
		Assert( ambientLightingLump.LumpSize() % sizeof( dleafambientlighting_t ) == 0 );
		Assert( ambientLightingTable.LumpSize() % sizeof( dleafambientindex_t ) == 0 );
		Assert((ambientLightingTable.LumpSize() / sizeof(dleafambientindex_t)) == (unsigned)count);	// should have one of these per leaf
		lh.GetMap()->m_pLeafAmbient = (mleafambientindex_t *)Hunk_AllocName( ambientLightingTable.LumpSize(), "LeafAmbient" );
		lh.GetMap()->m_pAmbientSamples = (mleafambientlighting_t *)Hunk_AllocName( ambientLightingLump.LumpSize(), "LeafAmbientSamples" );
		Q_memcpy( lh.GetMap()->m_pLeafAmbient, ambientLightingTable.LumpBase(), ambientLightingTable.LumpSize() );
		Q_memcpy( lh.GetMap()->m_pAmbientSamples, ambientLightingLump.LumpBase(), ambientLightingLump.LumpSize() );
	}


	for ( i=0 ; i<count ; i++, in++, out++ )
	{
		for (j=0 ; j<3 ; j++)
		{
			mins[j] = in->mins[j];
			maxs[j] = in->maxs[j];
		}

		VectorAdd( mins, maxs, out->m_vecCenter );
		out->m_vecCenter *= 0.5f;
		VectorSubtract( maxs, out->m_vecCenter, out->m_vecHalfDiagonal );

		p = in->contents;
		out->contents = p;

		out->cluster = in->cluster;
		out->area = in->area;
		out->flags = in->flags;
/*
		out->firstmarksurface = lh.GetMap()->marksurfaces + in->firstleafface;
*/
		out->firstmarksurface = in->firstleafface;
		out->nummarksurfaces = in->numleaffaces;
		out->parent = NULL;
		
		out->dispCount = 0;

		out->leafWaterDataID = in->leafWaterDataID;
	}	
}

void Mod_LoadLeafs( void )
{
	CMapLoadHelper lh( LUMP_LEAFS );

	switch( lh.LumpVersion() )
	{
	case 0:
		Mod_LoadLeafs_Version_0( lh );
		break;
	case 1:
		if( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE &&
	  		  CMapLoadHelper::LumpSize( LUMP_LEAF_AMBIENT_LIGHTING_HDR ) > 0 )
		{
			CMapLoadHelper mlh( LUMP_LEAF_AMBIENT_LIGHTING_HDR );
			CMapLoadHelper mlhTable( LUMP_LEAF_AMBIENT_INDEX_HDR );
			Mod_LoadLeafs_Version_1( lh, mlh, mlhTable );
		}
		else
		{
			CMapLoadHelper mlh( LUMP_LEAF_AMBIENT_LIGHTING );
			CMapLoadHelper mlhTable( LUMP_LEAF_AMBIENT_INDEX );
			Mod_LoadLeafs_Version_1( lh, mlh, mlhTable ); 
		}
		break;
	default:
		Assert( 0 );
		Error( "Unknown LUMP_LEAFS version\n" );
		break;
	}

	worldbrushdata_t *pMap = lh.GetMap();
	cleaf_t *pCLeaf = GetCollisionBSPData()->map_leafs.Base();
	for ( int i = 0; i < pMap->numleafs; i++ )
	{
		pMap->leafs[i].dispCount = pCLeaf[i].dispCount;
		pMap->leafs[i].dispListStart = pCLeaf[i].dispListStart;
	}
	// HACKHACK: Copy over the shared global list here.  Hunk_Alloc a copy?
	pMap->m_pDispInfoReferences = GetCollisionBSPData()->map_dispList.Base();
	pMap->m_nDispInfoReferences = GetCollisionBSPData()->numdisplist;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadLeafWaterData( void )
{
	dleafwaterdata_t *in;
	mleafwaterdata_t *out;
	int count, i;

	CMapLoadHelper lh( LUMP_LEAFWATERDATA );

	in = (dleafwaterdata_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mleafwaterdata_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "leafwaterdata" ) );

	lh.GetMap()->leafwaterdata = out;
	lh.GetMap()->numleafwaterdata = count;
	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->minZ = in->minZ;
		out->surfaceTexInfoID = in->surfaceTexInfoID;
		out->surfaceZ = in->surfaceZ;
		out->firstLeafIndex = -1;
	}
	if ( count == 1 )
	{
		worldbrushdata_t *brush = lh.GetMap();
		for ( i = 0; i < brush->numleafs; i++ )
		{
			if ( brush->leafs[i].leafWaterDataID >= 0 )
			{
				brush->leafwaterdata[0].firstLeafIndex = i;
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadCubemapSamples( void )
{
	char textureName[512];
	char loadName[ MAX_PATH ];
	dcubemapsample_t *in;
	mcubemapsample_t *out;
	int count, i;

	CMapLoadHelper lh( LUMP_CUBEMAPS );

	V_strcpy_safe( loadName, lh.GetLoadName() );

	in = (dcubemapsample_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadCubemapSamples: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mcubemapsample_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "cubemapsample" ) );

	lh.GetMap()->m_pCubemapSamples = out;
	lh.GetMap()->m_nCubemapSamples = count;

	bool bHDR = g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE;
	int nCreateFlags = bHDR ? 0 : TEXTUREFLAGS_SRGB;

	// We have separate HDR versions of the textures.  In order to deal with this,
	// we have blahenvmap.hdr.vtf and blahenvmap.vtf.
	char *pHDRExtension = "";
	if( bHDR )
	{
		pHDRExtension = ".hdr";
	}

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->origin.Init( ( float )in->origin[0], ( float )in->origin[1], ( float )in->origin[2] );
		out->size = in->size;
		Q_snprintf( textureName, sizeof( textureName ), "maps/%s/c%d_%d_%d%s", loadName, ( int )in->origin[0], 
			( int )in->origin[1], ( int )in->origin[2], pHDRExtension );
		out->pTexture = materials->FindTexture( textureName, TEXTURE_GROUP_CUBE_MAP, true, nCreateFlags );
		if ( IsErrorTexture( out->pTexture ) )
		{
			if ( bHDR )
			{
				Warning( "Couldn't get HDR '%s' -- ", textureName );
				// try non hdr version
				Q_snprintf( textureName, sizeof( textureName ), "maps/%s/c%d_%d_%d", loadName, ( int )in->origin[0], 
							( int )in->origin[1], ( int )in->origin[2]);
				Warning( "Trying non HDR '%s'\n", textureName);
				out->pTexture = materials->FindTexture( textureName, TEXTURE_GROUP_CUBE_MAP, true );
			}
			if ( IsErrorTexture( out->pTexture ) )
			{
				Q_snprintf( textureName, sizeof( textureName ), "maps/%s/cubemapdefault", loadName );
				out->pTexture = materials->FindTexture( textureName, TEXTURE_GROUP_CUBE_MAP, true, nCreateFlags );
				if ( IsErrorTexture( out->pTexture ) )
				{
					out->pTexture = materials->FindTexture( "engine/defaultcubemap", TEXTURE_GROUP_CUBE_MAP, true, nCreateFlags );
				}
				Warning( "Failed, using default cubemap '%s'\n", out->pTexture->GetName() );
			}
		}
		out->pTexture->IncrementReferenceCount();
	}

	CMatRenderContextPtr pRenderContext( materials );

	if ( count )
	{
		pRenderContext->BindLocalCubemap( lh.GetMap()->m_pCubemapSamples[0].pTexture );
	}
	else
	{
		if ( CommandLine()->CheckParm( "-requirecubemaps" ) )
		{
			Sys_Error( "Map \"%s\" does not have cubemaps!", lh.GetMapName() );
		}

		ITexture *pTexture;
		Q_snprintf( textureName, sizeof( textureName ), "maps/%s/cubemapdefault", loadName );
		pTexture = materials->FindTexture( textureName, TEXTURE_GROUP_CUBE_MAP, true, nCreateFlags );
		if ( IsErrorTexture( pTexture ) )
		{
			pTexture = materials->FindTexture( "engine/defaultcubemap", TEXTURE_GROUP_CUBE_MAP, true, nCreateFlags );
		}
		pTexture->IncrementReferenceCount();
		pRenderContext->BindLocalCubemap( pTexture );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadLeafMinDistToWater( void )
{
	CMapLoadHelper lh( LUMP_LEAFMINDISTTOWATER );

	unsigned short *pTmp = ( unsigned short * )lh.LumpBase();

	int i;
	bool foundOne = false;
	for( i = 0; i < ( int )( lh.LumpSize() / sizeof( *pTmp ) ); i++ )
	{
		if( pTmp[i] != 65535 ) // FIXME: make a marcro for this.
		{
			foundOne = true;
			break;
		}
	}
	
	if( !foundOne || lh.LumpSize() == 0 || !g_pMaterialSystemHardwareConfig || !g_pMaterialSystemHardwareConfig->SupportsVertexAndPixelShaders())
	{
		// We don't bother keeping this if:
		// 1) there is no water in the map
		// 2) we don't have this lump in the bsp file (old bsp file)
		// 3) we aren't going to use it because we are on old hardware.
		lh.GetMap()->m_LeafMinDistToWater = NULL;
	}
	else
	{
		int		count;
		unsigned short	*in;
		unsigned short	*out;

		in = (unsigned short *)lh.LumpBase();
		if (lh.LumpSize() % sizeof(*in))
			Host_Error ("Mod_LoadLeafMinDistToWater: funny lump size in %s",lh.GetMapName());
		count = lh.LumpSize() / sizeof(*in);
		out = (unsigned short *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "leafmindisttowater" ) );

		memcpy( out, in, sizeof( out[0] ) * count );
		lh.GetMap()->m_LeafMinDistToWater = out;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadMarksurfaces( void )
{	
	int		i, j, count;
	unsigned short	*in;

	CMapLoadHelper lh( LUMP_LEAFFACES );
	
	in = (unsigned short *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadMarksurfaces: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	SurfaceHandle_t	*tempDiskData = new SurfaceHandle_t[count];

	worldbrushdata_t *pBrushData = lh.GetMap();
	pBrushData->marksurfaces = tempDiskData;
	pBrushData->nummarksurfaces = count;

	// read in the mark surfaces, count out how many we'll actually need to store
	int realCount = 0;
	for ( i=0 ; i<count ; i++)
	{
		j = in[i];
		if (j >= lh.GetMap()->numsurfaces)
			Host_Error ("Mod_LoadMarksurfaces: bad surface number");
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( j, pBrushData );
		tempDiskData[i] = surfID;
		if ( !SurfaceHasDispInfo( surfID ) && !(MSurf_Flags(surfID) & SURFDRAW_NODRAW) )
		{
			realCount++;
		}
	}

	// now allocate the permanent list, and copy the non-terrain, non-nodraw surfs into it
	SurfaceHandle_t *surfList = (SurfaceHandle_t *)Hunk_AllocName( realCount*sizeof(SurfaceHandle_t), va( "%s [%s]", lh.GetLoadName(), "surfacehandle" ) );

	int outCount = 0;
	mleaf_t *pLeaf = pBrushData->leafs;
	for ( i = 0; i < pBrushData->numleafs; i++ )
	{
		int firstMark = outCount;
		int numMark = 0;
		bool foundDetail = false;
		int numMarkNode = 0;
		for ( j = 0; j < pLeaf[i].nummarksurfaces; j++ )
		{
			// write a new copy of the mark surfaces for this leaf, strip out the nodraw & terrain
			SurfaceHandle_t surfID = tempDiskData[pLeaf[i].firstmarksurface+j];
			if ( !SurfaceHasDispInfo( surfID ) && !(MSurf_Flags(surfID) & SURFDRAW_NODRAW) )
			{
				surfList[outCount++] = surfID;
				numMark++;
				Assert(outCount<=realCount);
				if ( MSurf_Flags(surfID) & SURFDRAW_NODE )
				{
					// this assert assures that all SURFDRAW_NODE surfs appear coherently
					Assert( !foundDetail );
					numMarkNode++;
				}
				else
				{
					foundDetail = true;
				}
			}
		}
		// update the leaf count
		pLeaf[i].nummarksurfaces = numMark;
		pLeaf[i].firstmarksurface = firstMark;
		pLeaf[i].nummarknodesurfaces = numMarkNode;
	}

	// write out the compacted array
	pBrushData->marksurfaces = surfList;
	pBrushData->nummarksurfaces = realCount;
	
	// remove the temp copy of the disk data
	delete[] tempDiskData;

	//Msg("Must check %d / %d faces\n", checkCount, pModel->brush.numsurfaces );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pedges - 
//			*loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadSurfedges( medge_t *pedges )
{	
	int		i, count;
	int		*in;
	unsigned short *out;
	
	CMapLoadHelper lh( LUMP_SURFEDGES );

	in = (int *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadSurfedges: funny lump size in %s",lh.GetMapName());
	count = lh.LumpSize() / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		Host_Error ("Mod_LoadSurfedges: bad surfedges count in %s: %i",
		lh.GetMapName(), count);
	out = (unsigned short *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "surfedges" ) );

	lh.GetMap()->vertindices = out;
	lh.GetMap()->numvertindices = count;

	for ( i=0 ; i<count ; i++)
	{
		int edge = in[i];
		int index = 0;
		if ( edge < 0 )
		{
			edge = -edge;
			index = 1;
		}
		out[i] = pedges[edge].v[index];
	}

	delete[] pedges;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadPlanes( void )
{
	// Don't bother loading them, they're already stored
	s_pMap->planes = GetCollisionBSPData()->map_planes.Base();
	s_pMap->numplanes = GetCollisionBSPData()->numplanes;
}


//-----------------------------------------------------------------------------
// Returns game lump version
//-----------------------------------------------------------------------------
int Mod_GameLumpVersion( int lumpId )
{
	for ( int i = g_GameLumpDict.Size(); --i >= 0; )
	{
		if ( g_GameLumpDict[i].id == lumpId )
		{
			return g_GameLumpDict[i].version;
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Returns game lump size
//-----------------------------------------------------------------------------
int Mod_GameLumpSize( int lumpId )
{
	for ( int i = g_GameLumpDict.Size(); --i >= 0; )
	{
		if ( g_GameLumpDict[i].id == lumpId )
		{
			return g_GameLumpDict[i].uncompressedSize;
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Loads game lumps
//-----------------------------------------------------------------------------
bool Mod_LoadGameLump( int lumpId, void *pOutBuffer, int size )
{
	int i;
	for ( i = g_GameLumpDict.Size(); --i >= 0; )
	{
		if ( g_GameLumpDict[i].id == lumpId )
		{
			break;
		}
	}
	if ( i < 0 )
	{
		// unknown
		return false;
	}

	byte *pData;
	bool bIsCompressed = ( g_GameLumpDict[i].flags & GAMELUMPFLAG_COMPRESSED );
	int dataLength;
	int outSize;
	if ( bIsCompressed )
	{
		// lump data length is always original uncompressed size
		// compressed lump data length is determined from next dictionary entry offset
		dataLength = g_GameLumpDict[i].compressedSize;
		outSize = g_GameLumpDict[i].uncompressedSize;
	}
	else
	{
		dataLength = outSize = g_GameLumpDict[i].uncompressedSize;
	}

	if ( size < 0 || size < outSize )
	{
		// caller must supply a buffer that is large enough to hold the data
		return false;
	}

	if ( s_MapBuffer.Base() )
	{
		// data is in memory
		Assert( CMapLoadHelper::GetRefCount() );

		if ( g_GameLumpDict[i].offset + dataLength > (unsigned int)s_MapBuffer.TellMaxPut() )
		{
			// out of range
			Assert( 0 );
			return false;
		}

		pData = (unsigned char *)s_MapBuffer.Base() + g_GameLumpDict[i].offset;
		if ( !bIsCompressed )
		{
			V_memcpy( pOutBuffer, pData, outSize );
			return true;
		}
	}
	else
	{
		// Load file into buffer
		FileHandle_t fileHandle = g_pFileSystem->Open( g_GameLumpFilename, "rb" );
		if ( fileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			return false;
		}

		g_pFileSystem->Seek( fileHandle, g_GameLumpDict[i].offset, FILESYSTEM_SEEK_HEAD );

		if ( !bIsCompressed )
		{
			// read directly into user's buffer
			bool bOK = ( g_pFileSystem->Read( pOutBuffer, outSize, fileHandle ) > 0 );
			g_pFileSystem->Close( fileHandle );
			return bOK;
		}
		else
		{
			// data is compressed, read into temporary
			pData = (byte *)malloc( dataLength );
			bool bOK = ( g_pFileSystem->Read( pData, dataLength, fileHandle ) > 0 );
			g_pFileSystem->Close( fileHandle );
			if ( !bOK )
			{
				free( pData );
				return false;
			}
		}
	}

	// We'll fall though to here through here if we're compressed
	bool bResult = false;
	if ( !CLZMA::IsCompressed( pData ) || CLZMA::GetActualSize( (unsigned char *)pData ) != g_GameLumpDict[i].uncompressedSize )
	{
		Warning( "Failed loading game lump %i: lump claims to be compressed but metadata does not match\n", lumpId );
	}
	else
	{
		// uncompress directly into caller's buffer
		int outputLength = CLZMA::Uncompress( pData, (unsigned char *)pOutBuffer );
		bResult = ( outputLength > 0 && (unsigned int)outputLength == g_GameLumpDict[i].uncompressedSize );
	}

	if ( !s_MapBuffer.Base() )
	{
		// done with temporary buffer
		free( pData );
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Loads game lump dictionary
//-----------------------------------------------------------------------------
void Mod_LoadGameLumpDict( void )
{
	CMapLoadHelper lh( LUMP_GAME_LUMP );

	// FIXME: This is brittle. If we ever try to load two game lumps
	// (say, in multiple BSP files), the dictionary info I store here will get whacked

	g_GameLumpDict.RemoveAll();
	V_strcpy_safe( g_GameLumpFilename, lh.GetMapName() );

	unsigned int lhSize = (unsigned int)Max( lh.LumpSize(), 0 );
	if ( lhSize >= sizeof( dgamelumpheader_t ) )
	{
		dgamelumpheader_t* pGameLumpHeader = (dgamelumpheader_t*)lh.LumpBase();

		// Ensure (lumpsize * numlumps + headersize) doesn't overflow
		const int nMaxGameLumps = ( INT_MAX - sizeof( dgamelumpheader_t ) ) / sizeof( dgamelump_t );
		if ( pGameLumpHeader->lumpCount < 0 ||
		     pGameLumpHeader->lumpCount > nMaxGameLumps ||
		     sizeof( dgamelumpheader_t ) + sizeof( dgamelump_t ) * pGameLumpHeader->lumpCount > lhSize )
		{
			Warning( "Bogus gamelump header in map, rejecting\n" );
		}
		else
		{
			// Load in lumps
			dgamelump_t* pGameLump = (dgamelump_t*)(pGameLumpHeader + 1);
			for (int i = 0; i < pGameLumpHeader->lumpCount; ++i )
			{
				if ( pGameLump[i].fileofs >= 0 &&
				     (unsigned int)pGameLump[i].fileofs >= (unsigned int)lh.LumpOffset() &&
				     (unsigned int)pGameLump[i].fileofs < (unsigned int)lh.LumpOffset() + lhSize &&
				     pGameLump[i].filelen > 0 )
				{
					unsigned int compressedSize = 0;
					if ( i + 1 < pGameLumpHeader->lumpCount &&
					     pGameLump[i+1].fileofs > pGameLump[i].fileofs &&
					     pGameLump[i+1].fileofs >= 0 &&
					     (unsigned int)pGameLump[i+1].fileofs <= (unsigned int)lh.LumpOffset() + lhSize )
					{
						compressedSize = (unsigned int)pGameLump[i+1].fileofs - (unsigned int)pGameLump[i].fileofs;
					}
					else
					{
						compressedSize = (unsigned int)lh.LumpOffset() + lhSize - (unsigned int)pGameLump[i].fileofs;
					}
					g_GameLumpDict.AddToTail( { pGameLump[i], compressedSize } );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Re-Loads all of a model's peer data
//-----------------------------------------------------------------------------
void Mod_TouchAllData( model_t *pModel, int nServerCount )
{
	double t1 = Plat_FloatTime();

	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	virtualmodel_t *pVirtualModel = g_pMDLCache->GetVirtualModel( pModel->studio );

	double t2 = Plat_FloatTime();
	g_flAccumulatedModelLoadTimeVirtualModel += ( t2 - t1 );

	if ( pVirtualModel && nServerCount >= 1 )
	{
		// ensure all sub models get current count to avoid purge
		// mark first to prevent re-entrant issues during possible reload
		// skip self, start at children
		for ( int i=1; i<pVirtualModel->m_group.Count(); ++i )
		{
			MDLHandle_t childHandle = (MDLHandle_t)(int)pVirtualModel->m_group[i].cache&0xffff;
			model_t *pChildModel = (model_t *)g_pMDLCache->GetUserData( childHandle );
			if ( pChildModel )
			{
				// child inherits parent reference
				pChildModel->nLoadFlags |= ( pModel->nLoadFlags & IModelLoader::FMODELLOADER_REFERENCEMASK );
				pChildModel->nLoadFlags |= IModelLoader::FMODELLOADER_LOADED;
				pChildModel->nLoadFlags &= ~IModelLoader::FMODELLOADER_LOADED_BY_PRELOAD;
				pChildModel->nServerCount = nServerCount;
			}
		}
	}

	// don't touch all the data
	if ( !mod_forcetouchdata.GetBool() )
		return;

	g_pMDLCache->TouchAllData( pModel->studio );
}

//-----------------------------------------------------------------------------
// Callbacks to get called when various data is loaded or unloaded 
//-----------------------------------------------------------------------------
class CMDLCacheNotify : public IMDLCacheNotify
{
public:
	virtual void OnDataLoaded( MDLCacheDataType_t type, MDLHandle_t handle );
	virtual void OnDataUnloaded( MDLCacheDataType_t type, MDLHandle_t handle );

private:
	void ComputeModelFlags( model_t* mod, MDLHandle_t handle );

	// Sets the bounds from the studiohdr 
	void SetBoundsFromStudioHdr( model_t *pModel, MDLHandle_t handle );
};
static CMDLCacheNotify s_MDLCacheNotify;

//-----------------------------------------------------------------------------
// Computes model flags
//-----------------------------------------------------------------------------
void CMDLCacheNotify::ComputeModelFlags( model_t* pModel, MDLHandle_t handle )
{
	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( handle );

	// Clear out those flags we set...
	pModel->flags &= ~(MODELFLAG_TRANSLUCENT_TWOPASS | MODELFLAG_VERTEXLIT | 
		MODELFLAG_TRANSLUCENT | MODELFLAG_MATERIALPROXY | MODELFLAG_FRAMEBUFFER_TEXTURE |
		MODELFLAG_STUDIOHDR_USES_FB_TEXTURE | MODELFLAG_STUDIOHDR_USES_BUMPMAPPING | MODELFLAG_STUDIOHDR_USES_ENV_CUBEMAP );

	bool bForceOpaque = (pStudioHdr->flags & STUDIOHDR_FLAGS_FORCE_OPAQUE) != 0;

	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_TRANSLUCENT_TWOPASS )
	{
		pModel->flags |= MODELFLAG_TRANSLUCENT_TWOPASS;
	}
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_USES_FB_TEXTURE )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_USES_FB_TEXTURE;
	}
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_USES_BUMPMAPPING )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_USES_BUMPMAPPING;
	}
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_USES_ENV_CUBEMAP )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_USES_ENV_CUBEMAP;
	}
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_AMBIENT_BOOST )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_AMBIENT_BOOST;
	}
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_DO_NOT_CAST_SHADOWS )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_DO_NOT_CAST_SHADOWS;
	}

	IMaterial *pMaterials[ 128 ];
	int materialCount = Mod_GetModelMaterials( pModel, ARRAYSIZE( pMaterials ), pMaterials );

	for ( int i = 0; i < materialCount; ++i )
	{
		IMaterial *pMaterial = pMaterials[ i ];
		if ( !pMaterial )
			continue;

		if ( pMaterial->IsVertexLit() )
		{
			pModel->flags |= MODELFLAG_VERTEXLIT;
		}

		if ( !bForceOpaque && pMaterial->IsTranslucent() )
		{
			//Msg("Translucent material %s for model %s\n", pLODData->ppMaterials[i]->GetName(), pModel->name );
			pModel->flags |= MODELFLAG_TRANSLUCENT;
		}

		if ( pMaterial->HasProxy() )
		{
			pModel->flags |= MODELFLAG_MATERIALPROXY;
		}

		if ( pMaterial->NeedsPowerOfTwoFrameBufferTexture( false ) ) // The false checks if it will ever need the frame buffer, not just this frame
		{
			pModel->flags |= MODELFLAG_FRAMEBUFFER_TEXTURE;
		}
	}
}

//-----------------------------------------------------------------------------
// Sets the bounds from the studiohdr 
//-----------------------------------------------------------------------------
void CMDLCacheNotify::SetBoundsFromStudioHdr( model_t *pModel, MDLHandle_t handle )
{
	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( handle );
	VectorCopy( pStudioHdr->hull_min, pModel->mins );
	VectorCopy( pStudioHdr->hull_max, pModel->maxs );
	pModel->radius = 0.0f;
	for ( int i = 0; i < 3; i++ )
	{
		if ( fabs(pModel->mins[i]) > pModel->radius )
		{
			pModel->radius = fabs(pModel->mins[i]);
		}

		if ( fabs(pModel->maxs[i]) > pModel->radius )
		{
			pModel->radius = fabs(pModel->maxs[i]);
		}
	}
}

//-----------------------------------------------------------------------------
// Callbacks to get called when various data is loaded or unloaded 
//-----------------------------------------------------------------------------
void CMDLCacheNotify::OnDataLoaded( MDLCacheDataType_t type, MDLHandle_t handle )
{
	model_t *pModel = (model_t*)g_pMDLCache->GetUserData( handle );

	// NOTE: A NULL model can occur for dependent MDLHandle_ts (like .ani files)
	if ( !pModel )
		return;

	switch( type )
	{
	case MDLCACHE_STUDIOHDR:
		{
			// FIXME: This code only works because it assumes StudioHdr
			// is loaded before VCollide.
			SetBoundsFromStudioHdr( pModel, handle );
		}
		break;

	case MDLCACHE_VCOLLIDE:
		{
			SetBoundsFromStudioHdr( pModel, handle );

			// Expand the model bounds to enclose the collision model (should be done in studiomdl)
			vcollide_t *pCollide = g_pMDLCache->GetVCollide( handle );
			if ( pCollide )
			{
				Vector mins, maxs;
				physcollision->CollideGetAABB( &mins, &maxs, pCollide->solids[0], vec3_origin, vec3_angle );
				AddPointToBounds( mins, pModel->mins, pModel->maxs );
				AddPointToBounds( maxs, pModel->mins, pModel->maxs );
			}
		}
		break;

	case MDLCACHE_STUDIOHWDATA:
		ComputeModelFlags( pModel, handle );
		break;
	}
}

void CMDLCacheNotify::OnDataUnloaded( MDLCacheDataType_t type, MDLHandle_t handle )
{
}

//-----------------------------------------------------------------------------
// Hooks the cache notify into the MDL cache system 
//-----------------------------------------------------------------------------
void ConnectMDLCacheNotify( )
{
	g_pMDLCache->SetCacheNotify( &s_MDLCacheNotify );
}

void DisconnectMDLCacheNotify( )
{
	g_pMDLCache->SetCacheNotify( NULL );
}

//-----------------------------------------------------------------------------
// Initialize studiomdl state
//-----------------------------------------------------------------------------
void InitStudioModelState( model_t *pModel )
{
	Assert( pModel->type == mod_studio );

	if ( g_pMDLCache->IsDataLoaded( pModel->studio, MDLCACHE_STUDIOHDR ) )
	{
		s_MDLCacheNotify.OnDataLoaded( MDLCACHE_STUDIOHDR, pModel->studio );
	}
	if ( g_pMDLCache->IsDataLoaded( pModel->studio, MDLCACHE_STUDIOHWDATA ) )
	{
		s_MDLCacheNotify.OnDataLoaded( MDLCACHE_STUDIOHWDATA, pModel->studio );
	}
	if ( g_pMDLCache->IsDataLoaded( pModel->studio, MDLCACHE_VCOLLIDE ) )
	{
		s_MDLCacheNotify.OnDataLoaded( MDLCACHE_VCOLLIDE, pModel->studio );
	}
}

//-----------------------------------------------------------------------------
// Resource loading for models
//-----------------------------------------------------------------------------
class CResourcePreloadModel : public CResourcePreload
{
	static void QueuedLoaderMapCallback( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError )
	{
		if ( loaderError == LOADERERROR_NONE )
		{
			// 360 mounts its bsp entirely into memory
			// this data is discarded at the conclusion of the entire load process
			Assert( CMapLoadHelper::GetRefCount() == 0 );
			CMapLoadHelper::InitFromMemory( (model_t *)pContext, pData, nSize );
		}
	}

	virtual bool CreateResource( const char *pName )
	{
		modtype_t modType = g_ModelLoader.GetTypeFromName( pName );

		// each model type resource has entirely differnt schemes for loading/creating
		if ( modType == mod_brush )
		{
			// expect to be the map bsp model
			MEM_ALLOC_CREDIT_( "CResourcePreloadModel(BSP)" );
			model_t *pMapModel = g_ModelLoader.FindModelNoCreate( pName );
			if ( pMapModel )
			{
				Assert( CMapLoadHelper::GetRefCount() == 0 );

				// 360 reads its specialized bsp into memory,
				// up to the pack lump, which is guranateed last
				char szLoadName[MAX_PATH];
				V_FileBase( pMapModel->strName, szLoadName, sizeof( szLoadName ) );
				CMapLoadHelper::Init( pMapModel, szLoadName );
				int nBytesToRead = CMapLoadHelper::LumpOffset( LUMP_PAKFILE );
				CMapLoadHelper::Shutdown();

				// create a loader job to perform i/o operation to mount the .bsp
				LoaderJob_t loaderJobBSP;
				loaderJobBSP.m_pFilename = pMapModel->strName;
				loaderJobBSP.m_pPathID = "GAME";
				loaderJobBSP.m_pCallback = QueuedLoaderMapCallback;
				loaderJobBSP.m_pContext = (void *)pMapModel;
				loaderJobBSP.m_pTargetData = malloc( nBytesToRead );
				loaderJobBSP.m_nBytesToRead = nBytesToRead;
				loaderJobBSP.m_Priority = LOADERPRIORITY_DURINGPRELOAD;
				g_pQueuedLoader->AddJob( &loaderJobBSP );

				// create an anonymous job to perform i/o operation to mount the .ain
				// the .ain gets claimed later
				char szAINName[MAX_PATH] = { 0 };
				V_snprintf( szAINName, sizeof( szAINName ), "maps/graphs/%s.360.ain", szLoadName );
				LoaderJob_t loaderJobAIN;
				loaderJobAIN.m_pFilename = szAINName;
				loaderJobAIN.m_pPathID = "GAME";
				loaderJobAIN.m_Priority = LOADERPRIORITY_DURINGPRELOAD;
				g_pQueuedLoader->AddJob( &loaderJobAIN );

				return true;
			}
		}
		else if ( modType == mod_studio )
		{
			MEM_ALLOC_CREDIT_( "CResourcePreloadModel(MDL)" );

			char szFilename[MAX_PATH];
			V_ComposeFileName( "models", pName, szFilename, sizeof( szFilename ) );			
	
			// find model or create empty entry
			model_t *pModel = g_ModelLoader.FindModel( szFilename );

			// mark as touched
			pModel->nLoadFlags |= IModelLoader::FMODELLOADER_TOUCHED_BY_PRELOAD;

			if ( pModel->nLoadFlags & ( IModelLoader::FMODELLOADER_LOADED|IModelLoader::FMODELLOADER_LOADED_BY_PRELOAD ) )
			{
				// already loaded or preloaded
				return true;
			}
			
			// the model in not supposed to be in memory
			Assert( pModel->type == mod_bad );

			// set its type
			pModel->type = mod_studio;

			// mark the model so that the normal studio load path can perform a final fixup
			pModel->nLoadFlags |= IModelLoader::FMODELLOADER_LOADED_BY_PRELOAD;

			// setup the new entry for preload to operate
			pModel->studio = g_pMDLCache->FindMDL( pModel->strName );

			// the model is not supposed to be in memory
			// if this hits, the mdlcache is out of sync with the modelloder
			// if this hits, the mdlcache has the model, but the modelloader doesn't think so
			// if the refcounts go haywire, bad evil bugs will occur
			Assert( g_pMDLCache->GetRef( pModel->studio ) == 1 );

			g_pMDLCache->SetUserData( pModel->studio, pModel );

			// get it into the cache
			g_pMDLCache->PreloadModel( pModel->studio );
			
			return true;
		}

		// unknown
		return false;
	}

	//-----------------------------------------------------------------------------
	// Called before queued loader i/o jobs are actually performed. Must free up memory
	// to ensure i/o requests have enough memory to succeed. The models that were
	// touched by the CreateResource() are the ones to keep, all others get purged.
	//-----------------------------------------------------------------------------
	virtual void PurgeUnreferencedResources()
	{
		bool bSpew = ( g_pQueuedLoader->GetSpewDetail() & LOADER_DETAIL_PURGES ) != 0;

		// purge any model that was not touched by the preload process
		int iIndex = -1;
		CUtlVector< model_t* > firstList;
		CUtlVector< model_t* > otherList;
		for ( ;; )
		{
			model_t *pModel;
			iIndex = g_ModelLoader.FindNext( iIndex, &pModel );
			if ( iIndex == -1 || !pModel )
			{
				// end of list
				break;
			}
			if ( pModel->type == mod_studio )
			{
				// models that were touched during the preload stay, otherwise purged
				if ( pModel->nLoadFlags & IModelLoader::FMODELLOADER_TOUCHED_BY_PRELOAD )
				{
					pModel->nLoadFlags &= ~IModelLoader::FMODELLOADER_TOUCHED_BY_PRELOAD;
				}
				else
				{
					if ( bSpew )
					{
						Msg( "CResourcePreloadModel: Purging: %s\n", pModel->strName.String() );
					}

					// Models that have virtual models have to unload first to
					// ensure they properly unreference their virtual models.
					if ( g_pMDLCache->IsDataLoaded( pModel->studio, MDLCACHE_VIRTUALMODEL ) )
					{
						firstList.AddToTail( pModel );
					}
					else
					{
						otherList.AddToTail( pModel );
					}
				}
			}
		}

		for ( int i=0; i<firstList.Count(); i++ )
		{
			g_ModelLoader.UnloadModel( firstList[i] );
		}
		for ( int i=0; i<otherList.Count(); i++ )
		{
			g_ModelLoader.UnloadModel( otherList[i] );
		}

		if ( !g_pQueuedLoader->IsSameMapLoading() )
		{
			g_pMDLCache->Flush( MDLCACHE_FLUSH_ANIMBLOCK );
		}
	}

	virtual void PurgeAll()
	{
		bool bSpew = ( g_pQueuedLoader->GetSpewDetail() & LOADER_DETAIL_PURGES ) != 0;

		// purge any model that was not touched by the preload process
		int iIndex = -1;
		CUtlVector< model_t* > firstList;
		CUtlVector< model_t* > otherList;
		for ( ;; )
		{
			model_t *pModel;
			iIndex = g_ModelLoader.FindNext( iIndex, &pModel );
			if ( iIndex == -1 || !pModel )
			{
				// end of list
				break;
			}
			if ( pModel->type == mod_studio )
			{
				pModel->nLoadFlags &= ~IModelLoader::FMODELLOADER_TOUCHED_BY_PRELOAD;
				if ( bSpew )
				{
					Msg( "CResourcePreloadModel: Purging: %s\n", pModel->strName.String() );
				}

				// Models that have virtual models have to unload first to
				// ensure they properly unreference their virtual models.
				if ( g_pMDLCache->IsDataLoaded( pModel->studio, MDLCACHE_VIRTUALMODEL ) )
				{
					firstList.AddToTail( pModel );
				}
				else
				{
					otherList.AddToTail( pModel );
				}
			}
		}

		for ( int i=0; i<firstList.Count(); i++ )
		{
			g_ModelLoader.UnloadModel( firstList[i] );
		}
		for ( int i=0; i<otherList.Count(); i++ )
		{
			g_ModelLoader.UnloadModel( otherList[i] );
		}

		g_pMDLCache->Flush( MDLCACHE_FLUSH_ANIMBLOCK );
	}

	virtual void OnEndMapLoading( bool bAbort )
	{
		// discard the memory mounted bsp
		CMapLoadHelper::Shutdown();
		Assert( CMapLoadHelper::GetRefCount() == 0 );
	}
};
static CResourcePreloadModel s_ResourcePreloadModel;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::Init( void )
{
	m_Models.RemoveAll();
	m_InlineModels.Purge();

	m_pWorldModel = NULL;
	m_bMapRenderInfoLoaded = false;
	m_bMapHasHDRLighting = false;
	g_bLoadedMapHasBakedPropLighting = false;
	
	// Make sure we have physcollision and physprop interfaces
	CollisionBSPData_LinkPhysics();

	m_szActiveMapName[0] = '\0';

	g_pQueuedLoader->InstallLoader( RESOURCEPRELOAD_MODEL, &s_ResourcePreloadModel );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::Shutdown( void )
{
	m_pWorldModel = NULL;

	ForceUnloadNonClientDynamicModels();

	UnloadAllModels( false );

	m_ModelPool.Clear();
}

int CModelLoader::GetCount( void )
{
	Assert( m_Models.Count() == m_Models.MaxElement() );
	return m_Models.Count();
}

model_t *CModelLoader::GetModelForIndex( int i )
{
	if ( i < 0 || (unsigned)i >= m_Models.Count() )
	{
		Assert( !m_Models.IsValidIndex( i ) );
		return NULL;
	}

	Assert( m_Models.IsValidIndex( i ) );
	return m_Models[i].modelpointer;
}

//-----------------------------------------------------------------------------
// Purpose: Look up name for model
// Input  : *model - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CModelLoader::GetName( const model_t *pModel )
{
	if ( pModel )
	{
		return pModel->strName;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the model, builds entry if not present, always returns a model
// Input  : *name - 
//			referencetype - 
// Output : model_t
//-----------------------------------------------------------------------------
model_t *CModelLoader::FindModel( const char *pName )
{
	if ( !pName || !pName[0] )
	{
		Sys_Error( "CModelLoader::FindModel: NULL name" );
	}

	// inline models are grabbed only from worldmodel
	if ( pName[0] == '*' )
	{
		int modelNum = atoi( pName + 1 );
		if ( !IsWorldModelSet() )
		{
			Sys_Error( "bad inline model number %i, worldmodel not yet setup", modelNum );
		}

		if ( modelNum < 1 || modelNum >= GetNumWorldSubmodels() )
		{
			Sys_Error( "bad inline model number %i", modelNum );
		}
		return &m_InlineModels[modelNum];
	}

	model_t *pModel = NULL;

	// get a handle suitable to use as the model key
	// handles are insensitive to case and slashes
	FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( pName );

	int i = m_Models.Find( fnHandle );
	if ( i == m_Models.InvalidIndex() )
	{
		pModel = (model_t *)m_ModelPool.Alloc();
		Assert( pModel );
		memset( pModel, 0, sizeof( model_t ) );

		pModel->fnHandle = fnHandle;

		// Mark that we should load from disk
		pModel->nLoadFlags = FMODELLOADER_NOTLOADEDORREFERENCED;

		// Copy in name and normalize!
		// Various other subsystems fetch this 'object' name to do dictionary lookups, 
		// which are usually case insensitive, but not to slashes or dotslashes.
		pModel->strName = pName;
		V_RemoveDotSlashes( pModel->strName.GetForModify(), '/' );

		ModelEntry_t entry;
		entry.modelpointer = pModel;
		m_Models.Insert( fnHandle, entry );
	}
	else
	{
		pModel = m_Models[i].modelpointer;
	}

	// notify the reslist generator that this model may be referenced later in the level 
	// (does nothing if reslist generation is not enabled)
	MapReslistGenerator().OnModelPrecached( pName );

	Assert( pModel );

	return pModel;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the model, and loads it if it isn't already present.  Updates reference flags
// Input  : *name - 
//			referencetype - 
// Output : model_t
//-----------------------------------------------------------------------------
model_t *CModelLoader::GetModelForName( const char *name, REFERENCETYPE referencetype )
{
	AssertMsg( !(referencetype & FMODELLOADER_DYNAMIC), "GetModelForName: dynamic models must use GetDynamicModel" );

	// find or build new entry
	model_t *model = FindModel( name );

	// touch and load if not present
	model_t *retval = LoadModel( model, &referencetype );

	return retval;
}


//-----------------------------------------------------------------------------
// Purpose: Add a reference to the model in question
// Input  : *name - 
//			referencetype - 
//-----------------------------------------------------------------------------
model_t *CModelLoader::ReferenceModel( const char *name, REFERENCETYPE referencetype )
{
	AssertMsg( !(referencetype & FMODELLOADER_DYNAMIC), "ReferenceModel: do not use for dynamic models" );

	model_t *model = FindModel( name );

	model->nLoadFlags |= referencetype;

	return model;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *entry - 
//			referencetype - 
//-----------------------------------------------------------------------------
model_t	*CModelLoader::LoadModel( model_t *mod, REFERENCETYPE *pReferencetype )
{
	if ( pReferencetype )
	{
		mod->nLoadFlags |= *pReferencetype;
	}

	// during initial load mark the model with an unique session ticket
	// at load end, models that have a mismatch count are considered candidates for purge
	// models that get marked, touch *all* their sub data to ensure the cache is pre-populated
	// and hitches less during gameplay
	bool bTouchAllData = false;
	int nServerCount = Host_GetServerCount();
	if ( mod->nServerCount != nServerCount )
	{
		// server has changed
		mod->nServerCount = nServerCount;
		bTouchAllData = true;
	}

	// Check if the studio model is in cache.
	// The model type will not be set for first time models that need to fall through to the load path.
	// A model that needs a post precache fixup will fall through to the load path.
	if ( mod->type == mod_studio && !( mod->nLoadFlags & FMODELLOADER_LOADED_BY_PRELOAD ) )
	{
		// in cache
		Verify( g_pMDLCache->GetStudioHdr( mod->studio ) != 0 );
		Assert( FMODELLOADER_LOADED & mod->nLoadFlags );

		if ( bTouchAllData )
		{
			// Touch all related .ani files and sub/dependent models
			// only touches once, when server changes
			Mod_TouchAllData( mod, nServerCount );
		}

		return mod;
	}

	// Check if brushes or sprites are loaded
	if ( FMODELLOADER_LOADED & mod->nLoadFlags ) 
	{
		return mod;
	}

	// model needs to be loaded
	double st = Plat_FloatTime();

	// Set the name of the current model we are loading
	Q_FileBase( mod->strName, m_szLoadName, sizeof( m_szLoadName ) );

	// load the file
	if ( developer.GetInt() > 1 )
	{
		DevMsg( "Loading: %s\n", mod->strName.String() );
	}

	mod->type = GetTypeFromName( mod->strName );
	if ( mod->type == mod_bad )
	{
		mod->type = mod_studio;
	}

	// finalize the model data
	switch ( mod->type )
	{
	case mod_sprite:
		{
			MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

			double t1 = Plat_FloatTime();
			Sprite_LoadModel( mod );
			double t2 = Plat_FloatTime();
			g_flAccumulatedModelLoadTimeSprite += ( t2 - t1 );
		}
		break;

	case mod_studio:
		{
			MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

			double t1 = Plat_FloatTime();
			Studio_LoadModel( mod, bTouchAllData );
			double t2 = Plat_FloatTime();
			g_flAccumulatedModelLoadTimeStudio += ( t2 - t1 );
		}
		break;

	case mod_brush:
		{
			double t1 = Plat_FloatTime();
			
			// This is necessary on dedicated clients. On listen + dedicated servers, it's called twice.
			// The second invocation is harmless.
			// Add to file system before loading so referenced objects in map can use the filename.
			g_pFileSystem->AddSearchPath( mod->strName, "GAME", PATH_ADD_TO_HEAD );

			// the map may have explicit texture exclusion
			// the texture state needs to be established before any loading work
			if ( IsX360() || mat_excludetextures.GetBool() )
			{
				char szExcludePath[MAX_PATH];
				sprintf( szExcludePath, "//MOD/maps/%s_exclude.lst", m_szLoadName );
				g_pMaterialSystem->SetExcludedTextures( szExcludePath );
			}

			// need this before queued loader starts, various systems use this as a cheap map changed state
			V_strncpy( m_szActiveMapName, mod->strName, sizeof( m_szActiveMapName ) );

			//NotifyHunkBeginMapLoad( m_szActiveMapName );

			bool bQueuedLoader = false;
			if ( IsX360() )
			{
				// must establish the bsp feature set first to ensure proper state during queued loading
				Map_CheckForHDR( mod, m_szLoadName );

				// Do not optimize map-to-same-map loading in TF
				// FIXME/HACK: this fixes a bug (when shipping Orange Box) where static props would sometimes
				//             disappear when a client disconnects and reconnects to the same map+server
				//             (static prop lighting data persists when loading map A after map A)
				bool bIsTF = !V_stricmp( COM_GetModDirectory(), "tf" );
				bool bOptimizeMapReload = !bIsTF;

				// start the queued loading process
				bQueuedLoader = g_pQueuedLoader->BeginMapLoading( mod->strName, g_pMaterialSystemHardwareConfig->GetHDREnabled(), bOptimizeMapReload );
			}

			// the queued loader process needs to own the actual texture update
			if ( !bQueuedLoader && ( IsX360() || mat_excludetextures.GetBool() ) )
			{
				g_pMaterialSystem->UpdateExcludedTextures();
			}

			BeginLoadingUpdates( MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD );
			g_pFileSystem->BeginMapAccess();
			Map_LoadModel( mod );
			g_pFileSystem->EndMapAccess();
	
			double t2 = Plat_FloatTime();
			g_flAccumulatedModelLoadTimeBrush += (t2 - t1);
		}
		break;

	default:
		Assert( 0 );
		break;
	};

	float dt = ( Plat_FloatTime() - st );
	COM_TimestampedLog( "Load of %s took %.3f msec", mod->strName.String(), 1000.0f * dt );
	g_flAccumulatedModelLoadTime += dt;

	return mod;
}

//-----------------------------------------------------------------------------
// Purpose: Creates the name of the sprite
//-----------------------------------------------------------------------------
//static void BuildSpriteLoadName( const char *pName, char *pOut, int outLen, bool &bIsAVI, bool &bIsBIK )
static void BuildSpriteLoadName( const char *pName, char *pOut, int outLen, bool &bIsVideo )
{
	// If it's a .vmt and they put a path in there, then use the path.
	// Otherwise, use the old method of prepending the sprites directory.
	Assert( pName != NULL && pOut != NULL );
	
	bIsVideo = false;
	bool bIsVMT = false;
	const char *pExt = V_GetFileExtension( pName );
	if ( pExt != NULL )
	{
		bIsVMT = !Q_stricmp( pExt, "vmt" );
		if ( !bIsVMT )
		{
			if ( g_pVideo )
			{
				bIsVideo = ( g_pVideo->LocateVideoSystemForPlayingFile( pName ) != VideoSystem::NONE );
			}
		}
	}
	
	if ( ( bIsVideo || bIsVMT ) && ( strchr( pName, '/' ) || strchr( pName, '\\' )  ) )
	{
		// The material system cannot handle a prepended "materials" dir
		// Keep .avi extensions on the material to load avi-based materials
		if ( bIsVMT )
		{
			const char *pNameStart = pName;
			if ( Q_stristr( pName, "materials/" ) == pName ||
				Q_stristr( pName, "materials\\" ) == pName )
			{
				// skip past materials/
				pNameStart = &pName[10];
			}
			Q_StripExtension( pNameStart, pOut, outLen );
		}
		else
		{
			// name is good as is
			Q_strncpy( pOut, pName, outLen );
		}
	}
	else
	{
		char szBase[MAX_PATH];
		Q_FileBase( pName, szBase, sizeof( szBase ) );
		Q_snprintf( pOut, outLen, "sprites/%s", szBase );
	}
	
	return;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int CModelLoader::GetModelFileSize( char const *name )
{
	if ( !name || !name[ 0 ] )
		return -1;

	model_t *model = FindModel( name );

	int size = -1;
	if ( Q_stristr( model->strName, ".spr" ) || Q_stristr( model->strName, ".vmt" ) )
	{
		char spritename[ MAX_PATH ];
		Q_StripExtension( va( "materials/%s", model->strName.String() ), spritename, MAX_PATH );
		Q_DefaultExtension( spritename, ".vmt", sizeof( spritename ) );

		size = COM_FileSize( spritename );
	}
	else
	{
		size = COM_FileSize( name );
	}

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Unmasks the referencetype field for the model
// Input  : *model - 
//			referencetype - 
//-----------------------------------------------------------------------------
void CModelLoader::UnreferenceModel( model_t *model, REFERENCETYPE referencetype )
{
	AssertMsg( !(referencetype & FMODELLOADER_DYNAMIC), "UnreferenceModel: do not use for dynamic models" );
	model->nLoadFlags &= ~referencetype;
}

//-----------------------------------------------------------------------------
// Purpose: Unmasks the specified reference type across all models
// Input  : referencetype - 
//-----------------------------------------------------------------------------
void CModelLoader::UnreferenceAllModels( REFERENCETYPE referencetype )
{
	AssertMsg( !(referencetype & FMODELLOADER_DYNAMIC), "UnreferenceAllModels: do not use for dynamic models" );

	// UNDONE: If we ever free a studio model, write code to free the collision data
	// UNDONE: Reference count collision data?

	FOR_EACH_MAP_FAST( m_Models, i )
	{
		m_Models[ i ].modelpointer->nLoadFlags &= ~referencetype;
	}
}

//-----------------------------------------------------------------------------
// Purpose: When changing servers the old servercount number is bogus. This
//          marks all models as loaded from -1 (e.g. a server count from the
//          before time.)
//-----------------------------------------------------------------------------
void CModelLoader::ResetModelServerCounts()
{
	FOR_EACH_MAP_FAST( m_Models, i )
	{
		model_t *pModel = m_Models[i].modelpointer;
		pModel->nServerCount = -1;
	}
}


void CModelLoader::ReloadFilesInList( IFileList *pFilesToReload )
{
	FOR_EACH_MAP_FAST( m_Models, i )
	{
		model_t	*pModel = m_Models[i].modelpointer;
		
		if ( pModel->type != mod_studio )
			continue;
		
		if ( !IsLoaded( pModel ) )
			continue;
		
		if ( pModel->type != mod_studio )
			continue;
		
		if ( pFilesToReload->IsFileInList( pModel->strName ) )
		{
			#ifdef PURE_SERVER_DEBUG_SPEW
				Msg( "Reloading model %s\n", pModel->strName.String() );
			#endif

			// Flush out the model cache
			// Don't flush vcollides since the vphysics system currently
			// has no way of indicating they refer to vcollides
			g_pMDLCache->Flush( pModel->studio, (int)(MDLCACHE_FLUSH_ALL & (~MDLCACHE_FLUSH_VCOLLIDE)) );

			MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

			// Get the studiohdr into the cache
			g_pMDLCache->GetStudioHdr( pModel->studio );

#ifndef _XBOX
			// force the collision to load
			g_pMDLCache->GetVCollide( pModel->studio );
#endif
		}
		else
		{
			if ( g_pMDLCache->IsDataLoaded( pModel->studio, MDLCACHE_STUDIOHWDATA ) )
			{
				studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( pModel->studio );
				if ( pStudioHdr )
				{
					// Ok, we didn't have to do a full reload, but if any of our materials changed, flush out the studiohwdata because the
					// vertex format may have changed.
					IMaterial *pMaterials[128];
					int nMaterials = g_pStudioRender->GetMaterialList( pStudioHdr, ARRAYSIZE( pMaterials ), &pMaterials[0] );

					for ( int iMat=0; iMat < nMaterials; iMat++ )
					{
						if ( pMaterials[iMat] && pMaterials[iMat]->WasReloadedFromWhitelist() )
						{
							#ifdef PURE_SERVER_DEBUG_SPEW
								Msg( "Reloading model %s because material %s was reloaded\n", pModel->strName.String(), pMaterials[iMat]->GetName() );
							#endif
							g_pMDLCache->Flush( pModel->studio, MDLCACHE_FLUSH_STUDIOHWDATA );
							break;
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: For any models with referencetype blank (if checking), frees all memory associated with the model
// and frees up the models slot
//-----------------------------------------------------------------------------
void CModelLoader::UnloadAllModels( bool bCheckReference )
{
	model_t			*model;

	FOR_EACH_MAP_FAST( m_Models, i )
	{
		model = m_Models[ i ].modelpointer;
		if ( bCheckReference )
		{
			if ( model->nLoadFlags & FMODELLOADER_REFERENCEMASK )
			{
				if ( model->type == mod_studio )
				{
					g_pMDLCache->MarkAsLoaded(model->studio);
				}
				continue;
			}
		}
		else
		{
			// Wipe current flags
			model->nLoadFlags &= ~FMODELLOADER_REFERENCEMASK;
		}

		if ( IsX360() && g_pQueuedLoader->IsMapLoading() && ( model->nLoadFlags & FMODELLOADER_LOADED_BY_PRELOAD ) )
		{
			// models preloaded by the queued loader are not initially claimed and MUST remain until the end of the load process
			// unclaimed models get unloaded during the post load purge
			continue;
		}

		if ( model->nLoadFlags & ( FMODELLOADER_LOADED | FMODELLOADER_LOADED_BY_PRELOAD ) )
		{		
			UnloadModel( model );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: For any models with referencetype blank (if checking), frees all memory associated with the model
//  and frees up the models slot
//-----------------------------------------------------------------------------
void CModelLoader::UnloadUnreferencedModels( void )
{
	// unload all unreferenced models
	UnloadAllModels( true );
}


//-----------------------------------------------------------------------------
// Called at the conclusion of loading.
// Frees all memory associated with models (and their materials) that are not
// marked with the current session.
//-----------------------------------------------------------------------------
void CModelLoader::PurgeUnusedModels( void )
{
	int nServerCount = Host_GetServerCount();
	FOR_EACH_MAP_FAST( m_Models, i )
	{
		model_t *pModel = m_Models[i].modelpointer;
		if ( ( pModel->nLoadFlags & FMODELLOADER_LOADED ) && ( pModel->nServerCount != nServerCount ) )
		{
			// mark as unreferenced
			// do not unload dynamic models
			pModel->nLoadFlags &= (~FMODELLOADER_REFERENCEMASK) | FMODELLOADER_DYNAMIC;
		}
	}

	// flush dynamic models that have no refcount
	FlushDynamicModels();

	// unload unreferenced models only
	UnloadAllModels( true );

	// now purge unreferenced materials
	materials->UncacheUnusedMaterials( true );
}

//-----------------------------------------------------------------------------
// Compute whether this submodel uses material proxies or not
//-----------------------------------------------------------------------------
static void Mod_ComputeBrushModelFlags( model_t *mod )
{
	Assert( mod );

	worldbrushdata_t *pBrushData = mod->brush.pShared;
	// Clear out flags we're going to set
	mod->flags &= ~(MODELFLAG_MATERIALPROXY | MODELFLAG_TRANSLUCENT | MODELFLAG_FRAMEBUFFER_TEXTURE | MODELFLAG_TRANSLUCENT_TWOPASS );
	mod->flags = MODELFLAG_HAS_DLIGHT; // force this check the first time

	int i;
	int scount = mod->brush.nummodelsurfaces;
	bool bHasOpaqueSurfaces = false;
	bool bHasTranslucentSurfaces = false;
	for ( i = 0; i < scount; ++i )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( mod->brush.firstmodelsurface + i, pBrushData );

		// Clear out flags we're going to set
		MSurf_Flags( surfID ) &= ~(SURFDRAW_NOCULL | SURFDRAW_TRANS | SURFDRAW_ALPHATEST | SURFDRAW_NODECALS);

		mtexinfo_t *pTex = MSurf_TexInfo( surfID, pBrushData );
		IMaterial* pMaterial = pTex->material;

		if ( pMaterial->HasProxy() )
		{
			mod->flags |= MODELFLAG_MATERIALPROXY;
		}

		if ( pMaterial->NeedsPowerOfTwoFrameBufferTexture( false ) ) // The false checks if it will ever need the frame buffer, not just this frame
		{
			mod->flags |= MODELFLAG_FRAMEBUFFER_TEXTURE;
		}

		// Deactivate culling if the material is two sided
		if ( pMaterial->IsTwoSided() )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NOCULL;
		}

		if ( (pTex->flags & SURF_TRANS) || pMaterial->IsTranslucent() )
		{
			mod->flags |= MODELFLAG_TRANSLUCENT;
			MSurf_Flags( surfID ) |= SURFDRAW_TRANS;
			bHasTranslucentSurfaces = true;
		}
		else
		{
			bHasOpaqueSurfaces = true;
		}

		// Certain surfaces don't want decals at all
		if ( (pTex->flags & SURF_NODECALS) || pMaterial->GetMaterialVarFlag( MATERIAL_VAR_SUPPRESS_DECALS ) || pMaterial->IsAlphaTested() )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NODECALS;
		}

		if ( pMaterial->IsAlphaTested() )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_ALPHATEST;
		}
	}

	if ( bHasOpaqueSurfaces && bHasTranslucentSurfaces )
	{
		mod->flags |= MODELFLAG_TRANSLUCENT_TWOPASS;
	}
}


//-----------------------------------------------------------------------------
// Recomputes translucency for the model...
//-----------------------------------------------------------------------------
void Mod_RecomputeTranslucency( model_t* mod, int nSkin, int nBody, void /*IClientRenderable*/ *pClientRenderable, float fInstanceAlphaModulate )
{
	if (fInstanceAlphaModulate < 1.0f)
	{
		mod->flags |= MODELFLAG_TRANSLUCENT;
		return;
	}

	mod->flags &= ~MODELFLAG_TRANSLUCENT;

	switch( mod->type )
	{
	case mod_brush:
		{
			for (int i = 0; i < mod->brush.nummodelsurfaces; ++i)
			{
				SurfaceHandle_t surfID = SurfaceHandleFromIndex( mod->brush.firstmodelsurface+i, mod->brush.pShared );
				if ( MSurf_Flags( surfID ) & SURFDRAW_NODRAW )
					continue;

				IMaterial* material = MSurf_TexInfo( surfID, mod->brush.pShared )->material;
				if ( material->IsTranslucent() )
				{
					mod->flags |= MODELFLAG_TRANSLUCENT;
					break;
				}
			}
		}
		break;

	case mod_studio:
		{
			studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( mod->studio );
			if ( pStudioHdr->flags & STUDIOHDR_FLAGS_FORCE_OPAQUE )
				return;

			IMaterial *pMaterials[ 128 ];
			int materialCount = g_pStudioRender->GetMaterialListFromBodyAndSkin( mod->studio, nSkin, nBody, ARRAYSIZE( pMaterials ), pMaterials );
			for ( int i = 0; i < materialCount; i++ )
			{
				if ( pMaterials[i] != NULL )
				{
					// Bind material first so all material proxies execute
					CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
					pRenderContext->Bind( pMaterials[i], pClientRenderable );
					bool bIsTranslucent = pMaterials[i]->IsTranslucent();

					if ( bIsTranslucent )
					{
						mod->flags |= MODELFLAG_TRANSLUCENT;
						break;
					}
				}
			}
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// returns the material count...
//-----------------------------------------------------------------------------
int Mod_GetMaterialCount( model_t* mod )
{
	switch( mod->type )
	{
	case mod_brush:
		{
			CUtlVector<IMaterial*> uniqueMaterials( 0, 32 );

			for (int i = 0; i < mod->brush.nummodelsurfaces; ++i)
			{
				SurfaceHandle_t surfID = SurfaceHandleFromIndex( mod->brush.firstmodelsurface + i, mod->brush.pShared );

				if ( MSurf_Flags( surfID ) & SURFDRAW_NODRAW )
					continue;

				IMaterial* pMaterial = MSurf_TexInfo( surfID, mod->brush.pShared )->material;

				// Try to find the material in the unique list of materials
				// if it's not there, then add it
				if (uniqueMaterials.Find(pMaterial) < 0)
					uniqueMaterials.AddToTail(pMaterial);
			}

			return uniqueMaterials.Size();
		}
		break;

	case mod_studio:
		{
			// FIXME: This should return the list of all materials
			// across all LODs if we every decide to implement this
			Assert(0);
		}
		break;

	default:
		// unimplemented
		Assert(0);
		break;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// returns the first n materials.
//-----------------------------------------------------------------------------
int Mod_GetModelMaterials( model_t* pModel, int count, IMaterial** ppMaterials )
{
	studiohdr_t *pStudioHdr;
	int found = 0; 
	int	i;

	switch( pModel->type )
	{
	case mod_brush:
		{
			for ( i = 0; i < pModel->brush.nummodelsurfaces; ++i)
			{
				SurfaceHandle_t surfID = SurfaceHandleFromIndex( pModel->brush.firstmodelsurface + i, pModel->brush.pShared );
				if ( MSurf_Flags( surfID ) & SURFDRAW_NODRAW )
					continue;

				IMaterial* pMaterial = MSurf_TexInfo( surfID, pModel->brush.pShared )->material;

				// Try to find the material in the unique list of materials
				// if it's not there, then add it
				int j = found;
				while ( --j >= 0 )
				{
					if ( ppMaterials[j] == pMaterial )
						break;
				}
				if (j < 0)
					ppMaterials[found++] = pMaterial;

				// Stop when we've gotten count materials
				if ( found >= count )
					return found;
			}
		}
		break;

	case mod_studio:
		if ( pModel->ppMaterials )
		{
			int nMaterials = ((intptr_t*)(pModel->ppMaterials))[-1];
			found = MIN( count, nMaterials );
			memcpy( ppMaterials, pModel->ppMaterials, found * sizeof( IMaterial* ) );
		}
		else
		{
			// Get the studiohdr into the cache
			pStudioHdr = g_pMDLCache->GetStudioHdr( pModel->studio );
			// Get the list of materials
			found = g_pStudioRender->GetMaterialList( pStudioHdr, count, ppMaterials );
		}
		break;

	default:
		// unimplemented
		Assert( 0 );
		break;
	}

	return found;
}


void Mod_SetMaterialVarFlag( model_t *pModel, unsigned int uiFlag, bool on )
{
	MaterialVarFlags_t flag = (MaterialVarFlags_t)uiFlag;
	IMaterial *pMaterials[ 128 ];
	if ( pModel )
	{
		int materialCount = Mod_GetModelMaterials( pModel, ARRAYSIZE( pMaterials ), pMaterials );

		for ( int i = 0; i < materialCount; ++i )
		{
			IMaterial *pMaterial = pMaterials[ i ];
			if ( pMaterial )
			{
				pMaterial->SetMaterialVarFlag( flag, on );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Used to compute which surfaces are in water or not
//-----------------------------------------------------------------------------

static void MarkWaterSurfaces_ProcessLeafNode( mleaf_t *pLeaf )
{
	int i;

	int flags = ( pLeaf->leafWaterDataID == -1 ) ? SURFDRAW_ABOVEWATER : SURFDRAW_UNDERWATER;

	SurfaceHandle_t *pHandle = &host_state.worldbrush->marksurfaces[pLeaf->firstmarksurface];

	for( i = 0; i < pLeaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pHandle[i];
		ASSERT_SURF_VALID( surfID );
		if( MSurf_Flags( surfID ) & SURFDRAW_WATERSURFACE )
			continue;

		if (SurfaceHasDispInfo( surfID ))
			continue;

		MSurf_Flags( surfID ) |= flags;
	}

	// FIXME: This is somewhat bogus, but I can do it quickly, and it's
	// not clear I need to solve the harder problem.

	// If any portion of a displacement surface hits a water surface,
	// I'm going to mark it as being in water, and vice versa.
	for ( i = 0; i < pLeaf->dispCount; i++ )
	{
		IDispInfo *pDispInfo = MLeaf_Disaplcement( pLeaf, i );

		if ( pDispInfo )
		{
			SurfaceHandle_t parentSurfID = pDispInfo->GetParent();
			MSurf_Flags( parentSurfID ) |= flags;
		}
	}
}


void MarkWaterSurfaces_r( mnode_t *node )
{
	// no polygons in solid nodes
	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	// if a leaf node, . .mark all the polys as to whether or not they are in water.
	if (node->contents >= 0)
	{
		MarkWaterSurfaces_ProcessLeafNode( (mleaf_t *)node );
		return;
	}

	MarkWaterSurfaces_r( node->children[0] );
	MarkWaterSurfaces_r( node->children[1] );
}


//-----------------------------------------------------------------------------
// Computes the sort group for a particular face
//-----------------------------------------------------------------------------
static int SurfFlagsToSortGroup( SurfaceHandle_t surfID, int flags )
{
	// If we're on the low end, stick everything into the same sort group
	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 80 )
		return MAT_SORT_GROUP_STRICTLY_ABOVEWATER;

	if( flags & SURFDRAW_WATERSURFACE )
		return MAT_SORT_GROUP_WATERSURFACE;

	if( ( flags & ( SURFDRAW_UNDERWATER | SURFDRAW_ABOVEWATER ) ) == ( SURFDRAW_UNDERWATER | SURFDRAW_ABOVEWATER ) )
		return MAT_SORT_GROUP_INTERSECTS_WATER_SURFACE;
	
	if( flags & SURFDRAW_UNDERWATER )
		return MAT_SORT_GROUP_STRICTLY_UNDERWATER;

	if( flags & SURFDRAW_ABOVEWATER )
		return MAT_SORT_GROUP_STRICTLY_ABOVEWATER;

	static int warningcount = 0;
	if ( ++warningcount < 10 )
	{
		Vector vecCenter;
		Surf_ComputeCentroid( surfID, &vecCenter );
		DevWarning( "SurfFlagsToSortGroup:  unhandled flags (%X) (%s)!\n", flags, MSurf_TexInfo(surfID)->material->GetName() );	
		DevWarning( "- This implies you have a surface (usually a displacement) embedded in solid.\n" );	
		DevWarning( "- Look near (%.1f, %.1f, %.1f)\n", vecCenter.x, vecCenter.y, vecCenter.z );	
	}
	//Assert( 0 );
	return MAT_SORT_GROUP_STRICTLY_ABOVEWATER;
}



//-----------------------------------------------------------------------------
// Computes sort group
//-----------------------------------------------------------------------------
bool Mod_MarkWaterSurfaces( model_t *pModel )
{
	bool bHasWaterSurfaces = false;
	model_t *pSaveModel = host_state.worldmodel;

	// garymcthack!!!!!!!!
	// host_state.worldmodel isn't set at this point, so. . . . 
	host_state.SetWorldModel( pModel );
	MarkWaterSurfaces_r( pModel->brush.pShared->nodes );
	for ( int i = 0; i < pModel->brush.pShared->numsurfaces; i++ )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( i, pModel->brush.pShared );
		
		int sortGroup = SurfFlagsToSortGroup( surfID, MSurf_Flags( surfID ) );
		if ( sortGroup == MAT_SORT_GROUP_WATERSURFACE )
		{
			bHasWaterSurfaces = true;
		}
		MSurf_SetSortGroup( surfID, sortGroup );
	}
	host_state.SetWorldModel( pSaveModel );

	return bHasWaterSurfaces;
}


//-----------------------------------------------------------------------------
// Marks identity brushes as being in fog volumes or not
//-----------------------------------------------------------------------------
class CBrushBSPIterator : public ISpatialLeafEnumerator
{
public:
	CBrushBSPIterator( model_t *pWorld, model_t *pBrush )
	{
		m_pWorld = pWorld;
		m_pBrush = pBrush;
		m_pShared = pBrush->brush.pShared;
		m_count = 0;
	}
	bool EnumerateLeaf( int leaf, int )
	{
		// garymcthack - need to test identity brush models
		int flags = ( m_pShared->leafs[leaf].leafWaterDataID == -1 ) ? SURFDRAW_ABOVEWATER : SURFDRAW_UNDERWATER;
		MarkModelSurfaces( flags );
		m_count++;
		return true;
	}

	void MarkModelSurfaces( int flags )
	{
		// Iterate over all this models surfaces
		int surfaceCount = m_pBrush->brush.nummodelsurfaces;
		for (int i = 0; i < surfaceCount; ++i)
		{
			SurfaceHandle_t surfID = SurfaceHandleFromIndex( m_pBrush->brush.firstmodelsurface + i, m_pShared ); 
			MSurf_Flags( surfID ) &= ~(SURFDRAW_ABOVEWATER | SURFDRAW_UNDERWATER);
			MSurf_Flags( surfID ) |= flags;
		}
	}

	void CheckSurfaces()
	{
		if ( !m_count )
		{
			MarkModelSurfaces( SURFDRAW_ABOVEWATER );
		}
	}

	model_t* m_pWorld;
	model_t* m_pBrush;
	worldbrushdata_t *m_pShared;
	int m_count;
};

static void MarkBrushModelWaterSurfaces( model_t* world,
	Vector const& mins, Vector const& maxs, model_t* brush )
{
	// HACK: This is a totally brutal hack dealing with initialization order issues.
	// I want to use the same box enumeration code so I don't have multiple
	// copies, but I want to use it from modelloader. host_state.worldmodel isn't
	// set up at that time however, so I have to fly through these crazy hoops.
	// Massive suckage.

	model_t* pTemp = host_state.worldmodel;
	CBrushBSPIterator brushIterator( world, brush );
	host_state.SetWorldModel( world );
	g_pToolBSPTree->EnumerateLeavesInBox( mins, maxs, &brushIterator, (int)brush );
	brushIterator.CheckSurfaces();
	host_state.SetWorldModel( pTemp );
}

int g_nMapLoadCount = 0;
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mod - 
//			*buffer - 
//-----------------------------------------------------------------------------
void CModelLoader::Map_LoadModel( model_t *mod )
{
	++g_nMapLoadCount;

	MEM_ALLOC_CREDIT();

#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	Assert( !( mod->nLoadFlags & FMODELLOADER_LOADED ) );

	COM_TimestampedLog( "Map_LoadModel: Start" );

	double startTime = Plat_FloatTime();

	SetWorldModel( mod );

	// point at the shared world/brush data
	mod->brush.pShared = &m_worldBrushData;
	mod->brush.renderHandle = 0;

	// HDR and features must be established first
	COM_TimestampedLog( "  Map_CheckForHDR" );
	m_bMapHasHDRLighting = Map_CheckForHDR( mod, m_szLoadName );
	if ( IsX360() && !m_bMapHasHDRLighting )
	{
		Warning( "Map '%s' lacks exepected HDR data! 360 does not support accurate LDR visuals.", m_szLoadName );
	}

	// Load the collision model
	COM_TimestampedLog( "  CM_LoadMap" );
	unsigned int checksum;
	CM_LoadMap( mod->strName, false, &checksum );

	// Load the map
	mod->type = mod_brush;
	mod->nLoadFlags |= FMODELLOADER_LOADED;
	CMapLoadHelper::Init( mod, m_szLoadName );

	COM_TimestampedLog( "  Mod_LoadVertices" );
	Mod_LoadVertices();
	
	COM_TimestampedLog( "  Mod_LoadEdges" );
	medge_t *pedges = Mod_LoadEdges();

	COM_TimestampedLog( "  Mod_LoadSurfedges" );
	Mod_LoadSurfedges( pedges );

	COM_TimestampedLog( "  Mod_LoadPlanes" );
	Mod_LoadPlanes();

	COM_TimestampedLog( "  Mod_LoadOcclusion" );
	Mod_LoadOcclusion();

	// texdata needs to load before texinfo
	COM_TimestampedLog( "  Mod_LoadTexdata" );
	Mod_LoadTexdata();

	COM_TimestampedLog( "  Mod_LoadTexinfo" );
	Mod_LoadTexinfo();

#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	// Until BSP version 19, this must occur after loading texinfo
	COM_TimestampedLog( "  Mod_LoadLighting" );
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE &&
		CMapLoadHelper::LumpSize( LUMP_LIGHTING_HDR ) > 0 )
	{
		CMapLoadHelper mlh( LUMP_LIGHTING_HDR );
		Mod_LoadLighting( mlh );
	}
	else
	{
		CMapLoadHelper mlh( LUMP_LIGHTING );
		Mod_LoadLighting( mlh );
	}

	COM_TimestampedLog( "  Mod_LoadPrimitives" );
	Mod_LoadPrimitives();

	COM_TimestampedLog( "  Mod_LoadPrimVerts" );
	Mod_LoadPrimVerts();

	COM_TimestampedLog( "  Mod_LoadPrimIndices" );
	Mod_LoadPrimIndices();

#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	// faces need to be loaded before vertnormals
	COM_TimestampedLog( "  Mod_LoadFaces" );
	Mod_LoadFaces();

	COM_TimestampedLog( "  Mod_LoadVertNormals" );
	Mod_LoadVertNormals();

	COM_TimestampedLog( "  Mod_LoadVertNormalIndices" );
	Mod_LoadVertNormalIndices();

#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	// note leafs must load befor marksurfaces
	COM_TimestampedLog( "  Mod_LoadLeafs" );
	Mod_LoadLeafs();

	COM_TimestampedLog( "  Mod_LoadMarksurfaces" );
    Mod_LoadMarksurfaces();

	COM_TimestampedLog( "  Mod_LoadNodes" );
	Mod_LoadNodes();

	COM_TimestampedLog( "  Mod_LoadLeafWaterData" );
	Mod_LoadLeafWaterData();

	COM_TimestampedLog( "  Mod_LoadCubemapSamples" );
	Mod_LoadCubemapSamples();

#ifndef SWDS
	// UNDONE: Does the cmodel need worldlights?
	COM_TimestampedLog( "  OverlayMgr()->LoadOverlays" );
	OverlayMgr()->LoadOverlays();	
#endif

	COM_TimestampedLog( "  Mod_LoadLeafMinDistToWater" );
	Mod_LoadLeafMinDistToWater();

#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	COM_TimestampedLog( "  LUMP_CLIPPORTALVERTS" );
	Mod_LoadLump( mod, 
		LUMP_CLIPPORTALVERTS, 
		va( "%s [%s]", m_szLoadName, "clipportalverts" ),
		sizeof(m_worldBrushData.m_pClipPortalVerts[0]), 
		(void**)&m_worldBrushData.m_pClipPortalVerts,
		&m_worldBrushData.m_nClipPortalVerts );

	COM_TimestampedLog( "  LUMP_AREAPORTALS" );
	Mod_LoadLump( mod, 
		LUMP_AREAPORTALS, 
		va( "%s [%s]", m_szLoadName, "areaportals" ),
		sizeof(m_worldBrushData.m_pAreaPortals[0]), 
		(void**)&m_worldBrushData.m_pAreaPortals,
		&m_worldBrushData.m_nAreaPortals );
	
	COM_TimestampedLog( "  LUMP_AREAS" );
	Mod_LoadLump( mod, 
		LUMP_AREAS, 
		va( "%s [%s]", m_szLoadName, "areas" ),
		sizeof(m_worldBrushData.m_pAreas[0]), 
		(void**)&m_worldBrushData.m_pAreas,
		&m_worldBrushData.m_nAreas );

	COM_TimestampedLog( "  Mod_LoadWorldlights" );
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE &&
  		  CMapLoadHelper::LumpSize( LUMP_WORLDLIGHTS_HDR ) > 0 )
	{
		CMapLoadHelper mlh( LUMP_WORLDLIGHTS_HDR );
		Mod_LoadWorldlights( mlh, true );
	}
	else
	{
		CMapLoadHelper mlh( LUMP_WORLDLIGHTS );
		Mod_LoadWorldlights( mlh, false );
	}

	COM_TimestampedLog( "  Mod_LoadGameLumpDict" );
	Mod_LoadGameLumpDict();

	// load the portal information
	// JAY: Disabled until we need this information.
#if 0
	Mod_LoadPortalVerts();
	Mod_LoadClusterPortals();
	Mod_LoadClusters();
	Mod_LoadPortals();
#endif

#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	COM_TimestampedLog( "  Mod_LoadSubmodels" );
	CUtlVector<mmodel_t> submodelList;
	Mod_LoadSubmodels( submodelList );

#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	COM_TimestampedLog( "  SetupSubModels" );
	SetupSubModels( mod, submodelList );

	COM_TimestampedLog( "  RecomputeSurfaceFlags" );
	RecomputeSurfaceFlags( mod );

#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	COM_TimestampedLog( "  Map_VisClear" );
	Map_VisClear();

	COM_TimestampedLog( "  Map_SetRenderInfoAllocated" );
	Map_SetRenderInfoAllocated( false );

	// Close map file, etc.
	CMapLoadHelper::Shutdown();

	double elapsed = Plat_FloatTime() - startTime;
	COM_TimestampedLog( "Map_LoadModel: Finish - loading took %.4f seconds", elapsed );
}

void CModelLoader::Map_UnloadCubemapSamples( model_t *mod )
{
	int i;
	for ( i=0 ; i < mod->brush.pShared->m_nCubemapSamples ; i++ )
	{
		mcubemapsample_t *pSample = &mod->brush.pShared->m_pCubemapSamples[i];
		pSample->pTexture->DecrementReferenceCount();
	}
}


//-----------------------------------------------------------------------------
// Recomputes surface flags
//-----------------------------------------------------------------------------
void CModelLoader::RecomputeSurfaceFlags( model_t *mod )
{
	for (int i=0 ; i<mod->brush.pShared->numsubmodels ; i++)
	{
		model_t *pSubModel = &m_InlineModels[i];

		// Compute whether this submodel uses material proxies or not
		Mod_ComputeBrushModelFlags( pSubModel );

		// Mark if brush models are in water or not; we'll use this
		// for identity brushes. If the brush is not an identity brush,
		// then we'll not have to worry.
		if ( i != 0 )
		{
			MarkBrushModelWaterSurfaces( mod, pSubModel->mins, pSubModel->maxs, pSubModel );
		}
	}
}

//-----------------------------------------------------------------------------
// Setup sub models
//-----------------------------------------------------------------------------
void CModelLoader::SetupSubModels( model_t *mod, CUtlVector<mmodel_t> &list )
{
	int	i;

	m_InlineModels.SetCount( m_worldBrushData.numsubmodels );

	for (i=0 ; i<m_worldBrushData.numsubmodels ; i++)
	{
		model_t		*starmod;
		mmodel_t	*bm;

		bm = &list[i];
		starmod = &m_InlineModels[i];

		*starmod = *mod;
		
		starmod->brush.firstmodelsurface = bm->firstface;
		starmod->brush.nummodelsurfaces = bm->numfaces;
		starmod->brush.firstnode = bm->headnode;
		if ( starmod->brush.firstnode >= m_worldBrushData.numnodes )
		{
			Sys_Error( "Inline model %i has bad firstnode", i );
		}

		VectorCopy(bm->maxs, starmod->maxs);
		VectorCopy(bm->mins, starmod->mins);
		starmod->radius = bm->radius;
	
		if (i == 0)
		{
			*mod = *starmod;
		}
		else
		{
			starmod->strName.Format( "*%d", i );
			starmod->fnHandle = g_pFileSystem->FindOrAddFileName( starmod->strName );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mod - 
//-----------------------------------------------------------------------------
void CModelLoader::Map_UnloadModel( model_t *mod )
{
	Assert( !( mod->nLoadFlags & FMODELLOADER_REFERENCEMASK ) );
	mod->nLoadFlags &= ~FMODELLOADER_LOADED;
	
#ifndef SWDS
	OverlayMgr()->UnloadOverlays();
#endif

	DeallocateLightingData( &m_worldBrushData );

#ifndef SWDS
	DispInfo_ReleaseMaterialSystemObjects( mod );
#endif

	Map_UnloadCubemapSamples( mod );
	
#ifndef SWDS
	// Free decals in displacements.
	R_DecalTerm( &m_worldBrushData, true );
#endif

	if ( m_worldBrushData.hDispInfos )
	{
		DispInfo_DeleteArray( m_worldBrushData.hDispInfos );
		m_worldBrushData.hDispInfos = NULL;
	}

	// Model loader loads world model materials, unload them here
	for( int texinfoID = 0; texinfoID < m_worldBrushData.numtexinfo; texinfoID++ )
	{
		mtexinfo_t *pTexinfo = &m_worldBrushData.texinfo[texinfoID];
		if ( pTexinfo )
		{
			GL_UnloadMaterial( pTexinfo->material );
		}
	}

	MaterialSystem_DestroySortinfo();

	// Don't store any reference to it here
	ClearWorldModel();
	Map_SetRenderInfoAllocated( false );
}


//-----------------------------------------------------------------------------
// Computes dimensions + frame count of a material 
//-----------------------------------------------------------------------------
static void GetSpriteInfo( const char *pName, bool bIsVideo, int &nWidth, int &nHeight, int &nFrameCount )
{
	nFrameCount = 1;
	nWidth = nHeight = 1;

	// FIXME: The reason we are putting logic related to AVIs here,
	// logic which is duplicated in the client DLL related to loading sprites,
	// is that this code gets run on dedicated servers also.
	IMaterial *pMaterial = NULL;
	IVideoMaterial *pVideoMaterial = NULL;
	if ( bIsVideo && g_pVideo != NULL )
	{
		pVideoMaterial = g_pVideo->CreateVideoMaterial( pName, pName, "GAME", VideoPlaybackFlags::DEFAULT_MATERIAL_OPTIONS, VideoSystem::DETERMINE_FROM_FILE_EXTENSION, false );
		if ( pVideoMaterial )
		{
			pVideoMaterial->GetVideoImageSize( &nWidth, &nHeight );
			nFrameCount = pVideoMaterial->GetFrameCount();
			pMaterial = pVideoMaterial->GetMaterial();
			
			g_pVideo->DestroyVideoMaterial( pVideoMaterial );
		}
	}
	else
	{
		pMaterial = GL_LoadMaterial( pName, TEXTURE_GROUP_OTHER );
		if ( pMaterial )
		{
			// Store off our source height, width, frame count
			nWidth = pMaterial->GetMappingWidth();
			nHeight = pMaterial->GetMappingHeight();
			nFrameCount = pMaterial->GetNumAnimationFrames();
		}
	}

	if ( pMaterial == g_materialEmpty )
	{
		DevMsg( "Missing sprite material %s\n", pName );
	}

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::Sprite_LoadModel( model_t *mod )
{
	Assert( !( mod->nLoadFlags & FMODELLOADER_LOADED ) );

	mod->nLoadFlags |= FMODELLOADER_LOADED;

	// The hunk data is not used on the server
	byte* pSprite = NULL;

#ifndef SWDS
	if ( g_ClientDLL )
	{
		int nSize = g_ClientDLL->GetSpriteSize();
		if ( nSize )
		{
			pSprite = ( byte * )new byte[ nSize ];
		}
	}
#endif

	mod->type = mod_sprite;
	mod->sprite.sprite = (CEngineSprite *)pSprite;

	// Fake the bounding box. We need it for PVS culling, and we don't
	// know the scale at which the sprite is going to be rendered at
	// when we load it
	mod->mins = mod->maxs = Vector(0,0,0);

	// Figure out the real load name..
	char loadName[MAX_PATH];
	bool bIsVideo;
	BuildSpriteLoadName( mod->strName, loadName, MAX_PATH, bIsVideo );
	GetSpriteInfo( loadName, bIsVideo, mod->sprite.width, mod->sprite.height, mod->sprite.numframes );

#ifndef SWDS
	if ( g_ClientDLL && mod->sprite.sprite )
	{
		g_ClientDLL->InitSprite( mod->sprite.sprite, loadName );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::Sprite_UnloadModel( model_t *mod )
{
	Assert( !( mod->nLoadFlags & FMODELLOADER_REFERENCEMASK ) );
	mod->nLoadFlags &= ~FMODELLOADER_LOADED;

	char loadName[MAX_PATH];
	bool bIsVideo;
	BuildSpriteLoadName( mod->strName, loadName, sizeof( loadName ), bIsVideo );

	IMaterial *mat = materials->FindMaterial( loadName, TEXTURE_GROUP_OTHER );
	if ( !IsErrorMaterial( mat ) )
	{
		GL_UnloadMaterial( mat );
	}

#ifndef SWDS
	if ( g_ClientDLL && mod->sprite.sprite )
	{
		g_ClientDLL->ShutdownSprite( mod->sprite.sprite );
	}
#endif

	delete[] (byte *)mod->sprite.sprite;
	mod->sprite.sprite = 0;
	mod->sprite.numframes = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Flush and reload models.  Intended for use when lod changes.
//-----------------------------------------------------------------------------
void CModelLoader::Studio_ReloadModels( CModelLoader::ReloadType_t reloadType )
{
#if !defined( SWDS )
	if ( g_ClientDLL )
		g_ClientDLL->InvalidateMdlCache();
#endif // SWDS
	if ( serverGameDLL )
		serverGameDLL->InvalidateMdlCache();

	// ensure decals have no stale references to invalid lods
	modelrender->RemoveAllDecalsFromAllModels();

	// ensure static props have no stale references to invalid lods
	modelrender->ReleaseAllStaticPropColorData();

	// Flush out the model cache
	// Don't flush vcollides since the vphysics system currently
	// has no way of indicating they refer to vcollides
	g_pMDLCache->Flush( (MDLCacheFlush_t) (MDLCACHE_FLUSH_ALL & (~MDLCACHE_FLUSH_VCOLLIDE)) );

	// Load the critical pieces now
	// The model cache will re-populate as models render
	FOR_EACH_MAP_FAST( m_Models, i )
	{
		model_t *pModel = m_Models[ i ].modelpointer;
		if ( !IsLoaded( pModel ) )
			continue;

		if ( pModel->type != mod_studio )
			continue;

		MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

		// Get the studiohdr into the cache
		g_pMDLCache->GetStudioHdr( pModel->studio );

		// force the collision to load
		g_pMDLCache->GetVCollide( pModel->studio );
	}
}

struct modelsize_t
{
	const char *pName;
	int			size;
};

class CModelsize_Less
{
public:
	bool Less( const modelsize_t& src1, const modelsize_t& src2, void *pCtx )
	{
		return ( src1.size < src2.size );
	}
};

void CModelLoader::DumpVCollideStats()
{
	int i;
	CUtlSortVector< modelsize_t, CModelsize_Less > list;
	for ( i = 0; (m_Models).IsUtlMap && i < (m_Models).MaxElement(); ++i ) if ( !(m_Models).IsValidIndex( i ) ) continue; else
	{
		model_t *pModel = m_Models[ i ].modelpointer;
		if ( pModel && pModel->type == mod_studio )
		{
			int size = 0;
			bool loaded = g_pMDLCache->GetVCollideSize( pModel->studio, &size );
			if ( loaded && size )
			{
				modelsize_t elem;
				elem.pName = pModel->strName;
				elem.size = size;
				list.Insert( elem );
			}
		}
	}
	for ( i = m_InlineModels.Count(); --i >= 0; )
	{
		vcollide_t *pCollide = CM_VCollideForModel( i+1, &m_InlineModels[i] );
		if ( pCollide )
		{
			int size = 0;
			for ( int j = 0; j < pCollide->solidCount; j++ )
			{
				size += physcollision->CollideSize( pCollide->solids[j] );
			}
			size += pCollide->descSize;
			if ( size )
			{
				modelsize_t elem;
				elem.pName = m_InlineModels[i].strName;
				elem.size = size;
				list.Insert( elem );
			}
		}
	}

	Msg("VCollides loaded: %d\n", list.Count() );
	int totalVCollideMemory = 0;
	for ( i = 0; i < list.Count(); i++ )
	{
		Msg("%8d bytes:%s\n", list[i].size, list[i].pName);
		totalVCollideMemory += list[i].size;
	}
	int bboxCount, bboxSize;
	physcollision->GetBBoxCacheSize( &bboxSize, &bboxCount );
	Msg( "%8d bytes BBox physics: %d boxes\n", bboxSize, bboxCount );
	totalVCollideMemory += bboxSize;
	Msg( "--------------\n%8d bytes total VCollide Memory\n", totalVCollideMemory );
}


//-----------------------------------------------------------------------------
// Is the model loaded?
//-----------------------------------------------------------------------------
bool CModelLoader::IsLoaded( const model_t *mod )
{
	return (mod->nLoadFlags & FMODELLOADER_LOADED) != 0;
}

bool CModelLoader::LastLoadedMapHasHDRLighting(void)
{
	return m_bMapHasHDRLighting;
}

//-----------------------------------------------------------------------------
// Loads a studio model
//-----------------------------------------------------------------------------
void CModelLoader::Studio_LoadModel( model_t *pModel, bool bTouchAllData )
{
	if ( !mod_touchalldata.GetBool() )
	{
		bTouchAllData = false;
	}

	// a preloaded model requires specific fixup behavior
	bool bPreLoaded = ( pModel->nLoadFlags & FMODELLOADER_LOADED_BY_PRELOAD ) != 0;

	bool bLoadPhysics = true;
	if ( pModel->nLoadFlags == FMODELLOADER_STATICPROP )
	{
		// this is the first call in loading as a static prop (load bit not set), don't load physics yet
		// the next call in causes the physics to load
		bLoadPhysics = false;
	}

	// mark as loaded and fixed up
	pModel->nLoadFlags |= FMODELLOADER_LOADED;
	pModel->nLoadFlags &= ~FMODELLOADER_LOADED_BY_PRELOAD;

	if ( !bPreLoaded )
	{
		pModel->studio = g_pMDLCache->FindMDL( pModel->strName );		
		g_pMDLCache->SetUserData( pModel->studio, pModel );

		InitStudioModelState( pModel );
	}

	// Get the studiohdr into the cache
	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( pModel->studio );
	(void) pStudioHdr;

	// a preloaded model alrady has its physics data resident
	if ( bLoadPhysics && !bPreLoaded )
	{
		// load the collision data now
		bool bSynchronous = bTouchAllData;
		double t1 = Plat_FloatTime();
		g_pMDLCache->GetVCollideEx( pModel->studio, bSynchronous );

		double t2 = Plat_FloatTime();
		if ( bSynchronous )
		{
			g_flAccumulatedModelLoadTimeVCollideSync += ( t2 - t1 );
		}
		else
		{
			g_flAccumulatedModelLoadTimeVCollideAsync += ( t2 - t1 );
		}
	}

	// this forces sync setup operations (materials/shaders) to build out now during load and not at runtime
	double t1 = Plat_FloatTime();

	// should already be NULL, but better safe than sorry
	if ( pModel->ppMaterials )
	{
		free( pModel->ppMaterials - 1 );
		pModel->ppMaterials = NULL;
	}

	IMaterial *pMaterials[128];
	int nMaterials = Mod_GetModelMaterials( pModel, ARRAYSIZE( pMaterials ), pMaterials );

	if ( pModel->nLoadFlags & FMODELLOADER_DYNAMIC )
	{
		// Cache the material pointers so that we don't re-scan all the VMTs on dynamic unload
		COMPILE_TIME_ASSERT( sizeof( intptr_t ) == sizeof( IMaterial * ) );
		IMaterial **pMem = (IMaterial**) malloc( (1 + nMaterials) * sizeof( IMaterial* ) );
		*(intptr_t*)pMem = nMaterials;
		pModel->ppMaterials = pMem + 1;
		for ( int i=0; i<nMaterials; i++ )
		{
			pModel->ppMaterials[i] = pMaterials[i];
		}
	}

	if ( nMaterials )
	{
		for ( int i=0; i<nMaterials; i++ )
		{
			pMaterials[i]->IncrementReferenceCount();
		}
		// track the refcount bump
		pModel->nLoadFlags |= FMODELLOADER_TOUCHED_MATERIALS;
	}

	double t2 = Plat_FloatTime();
	g_flAccumulatedModelLoadTimeMaterialNamesOnly += ( t2 - t1 );

	// a preloaded model must touch its children
	if ( bTouchAllData || bPreLoaded )
	{
		Mod_TouchAllData( pModel, Host_GetServerCount() );
	}
}

	
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mod - 
//-----------------------------------------------------------------------------
void CModelLoader::Studio_UnloadModel( model_t *pModel )
{
	// Do not unload models that are still referenced by the dynamic system
	if ( pModel->nLoadFlags & FMODELLOADER_DYNAMIC )
	{
		return;
	}

	if ( pModel->nLoadFlags & FMODELLOADER_TOUCHED_MATERIALS )
	{
		IMaterial *pMaterials[128];
		int nMaterials = Mod_GetModelMaterials( pModel, ARRAYSIZE( pMaterials ), &pMaterials[0] );
		for ( int j=0; j<nMaterials; j++ )
		{
			pMaterials[j]->DecrementReferenceCount();
		}
		pModel->nLoadFlags &= ~FMODELLOADER_TOUCHED_MATERIALS;
	}

	// leave these flags alone since we are going to return from alt-tab at some point.
	//	Assert( !( mod->needload & FMODELLOADER_REFERENCEMASK ) );
	pModel->nLoadFlags &= ~( FMODELLOADER_LOADED | FMODELLOADER_LOADED_BY_PRELOAD );
	if ( IsX360() )
	{
		// 360 doesn't need to keep the reference flags, but the PC does
		pModel->nLoadFlags &= ~FMODELLOADER_REFERENCEMASK;
	}

#ifdef DBGFLAG_ASSERT
	int nRef = 
#endif
		g_pMDLCache->Release( pModel->studio );

	// the refcounts must be as expected, or evil latent bugs will occur
	Assert( InEditMode() || ( nRef == 0 ) );

	if ( pModel->ppMaterials )
	{
		free( pModel->ppMaterials - 1 );
		pModel->ppMaterials = NULL;
	}

	pModel->studio = MDLHANDLE_INVALID;
	pModel->type = mod_bad;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mod - 
//-----------------------------------------------------------------------------
void CModelLoader::SetWorldModel( model_t *mod )
{
	Assert( mod );
	m_pWorldModel = mod;
//	host_state.SetWorldModel( mod ); // garymcthack
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::ClearWorldModel( void )
{
	m_pWorldModel = NULL;
	memset( &m_worldBrushData, 0, sizeof(m_worldBrushData) );
	m_InlineModels.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CModelLoader::IsWorldModelSet( void )
{
	return m_pWorldModel ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CModelLoader::GetNumWorldSubmodels( void )
{
	if ( !IsWorldModelSet() )
		return 0;

	return m_worldBrushData.numsubmodels;
}

//-----------------------------------------------------------------------------
// Purpose: Check cache or union data for info, reload studio model if needed 
// Input  : *model - 
//-----------------------------------------------------------------------------
void *CModelLoader::GetExtraData( model_t *model )
{
	if ( !model )
	{
		return NULL;
	}

	switch ( model->type )
	{
	case mod_sprite:
		{
			// sprites don't use the real cache yet
			if ( model->type == mod_sprite )
			{
				// The sprite got unloaded.
				if ( !( FMODELLOADER_LOADED & model->nLoadFlags ) )
				{
					return NULL;
				}

				return model->sprite.sprite;
			}
		}
		break;

	case mod_studio:
		return g_pMDLCache->GetStudioHdr( model->studio );

	default:
	case mod_brush:
		// Should never happen
		Assert( 0 );
		break;
	};

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CModelLoader::Map_GetRenderInfoAllocated( void )
{
	return m_bMapRenderInfoLoaded;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::Map_SetRenderInfoAllocated( bool allocated )
{
	m_bMapRenderInfoLoaded = allocated;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mod - 
//-----------------------------------------------------------------------------
void CModelLoader::Map_LoadDisplacements( model_t *pModel, bool bRestoring )
{
	if ( !pModel )
	{
		Assert( false );
		return;
	}
	
	Q_FileBase( pModel->strName, m_szLoadName, sizeof( m_szLoadName ) );
	CMapLoadHelper::Init( pModel, m_szLoadName );

    DispInfo_LoadDisplacements( pModel, bRestoring );

	CMapLoadHelper::Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: List the model dictionary
//-----------------------------------------------------------------------------
void CModelLoader::Print( void )
{
	ConMsg( "Models:\n" );
	FOR_EACH_MAP_FAST( m_Models, i )
	{
		model_t *pModel = m_Models[i].modelpointer;
		if ( pModel->type == mod_studio || pModel->type == mod_bad )
		{
			// studio models have ref counts
			// bad models are unloaded models which need to be listed
			int refCount = ( pModel->type == mod_studio ) ? g_pMDLCache->GetRef( pModel->studio ) : 0;
			ConMsg( "%4d: Flags:0x%8.8x RefCount:%2d %s\n", i, pModel->nLoadFlags, refCount, pModel->strName.String() );
		}
		else
		{
			ConMsg( "%4d: Flags:0x%8.8x %s\n", i, pModel->nLoadFlags, pModel->strName.String() );
		}
	}
}

//-----------------------------------------------------------------------------
// Callback for UpdateOrCreate utility function - swaps a bsp.
//-----------------------------------------------------------------------------
#if defined( _X360 )
static bool BSPCreateCallback( const char *pSourceName, const char *pTargetName, const char *pPathID, void *pExtraData )
{
	// load the bsppack dll
	IBSPPack *iBSPPack = NULL;
	CSysModule *pmodule = g_pFullFileSystem->LoadModule( "bsppack" );
	if ( pmodule )
	{
		CreateInterfaceFn factory = Sys_GetFactory( pmodule );
		if ( factory )
		{
			iBSPPack = ( IBSPPack * )factory( IBSPPACK_VERSION_STRING, NULL );
		}
	}
	if( !iBSPPack )
	{
		Warning( "Can't load bsppack.dll - unable to swap bsp.\n" );
		return false;
	}

	bool bOk = true;
	if ( !iBSPPack->SwapBSPFile( g_pFileSystem, pSourceName, pTargetName, IsX360(), ConvertVTFTo360Format, NULL, NULL ) )
	{
		bOk = false;
		Warning( "Failed to create %s\n", pTargetName );
	}

	Sys_UnloadModule( pmodule );

	return bOk;
}
#endif

//-----------------------------------------------------------------------------
// Calls utility function to create .360 version of a file.
//-----------------------------------------------------------------------------
int CModelLoader::UpdateOrCreate( const char *pSourceName, char *pTargetName, int targetLen, bool bForce )
{
#if defined( _X360 )
	return ::UpdateOrCreate( pSourceName, pTargetName, targetLen, NULL, BSPCreateCallback, bForce );
#else
	return UOC_NOT_CREATED;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Determine if specified .bsp is valid
// Input  : *mapname - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CModelLoader::Map_IsValid( char const *pMapFile, bool bQuiet /* = false */ )
{
	static char	s_szLastMapFile[MAX_PATH] = { 0 };

	if ( !pMapFile || !pMapFile[0] )
	{
		if ( !bQuiet )
		{
			ConMsg( "CModelLoader::Map_IsValid:  Empty mapname!!!\n" );
		}
		return false;
	}

	char szMapFile[MAX_PATH] = { 0 };
	V_strncpy( szMapFile, pMapFile, sizeof( szMapFile ) );

	if ( IsX360() && !V_stricmp( szMapFile, s_szLastMapFile ) )
	{
		// already been checked, no reason to do multiple i/o validations
		return true;
	}

	// Blacklist some characters
	// - Don't allow characters not allowed on all supported platforms for consistency
	// - Don't allow quotes or ;"' as defense-in-depth against script abuses (and, no real reason for mapnames to use these)
	const char *pBaseFileName = V_UnqualifiedFileName( pMapFile );
	bool bIllegalChar = false;
	for (; pBaseFileName && *pBaseFileName; pBaseFileName++ )
	{
		// ASCII control characters (codepoints <= 31) illegal in windows filenames
		if ( *pBaseFileName <= (char)31 )
			bIllegalChar = true;

		switch ( *pBaseFileName )
		{
			// Illegal in windows filenames, don't allow on any platform
			case '<': case '>': case ':': case '"': case '/': case '\\':
			case '|': case '?': case '*':
				bIllegalChar = true;
			// Additional special characters in source engine commands, defense-in-depth against things that might be
			// composing commands with map names (though they really shouldn't be)
			case ';': case '\'':
				bIllegalChar = true;
			default: break;
		}
	}

	if ( bIllegalChar )
	{
		Assert( !"Map with illegal characters in filename" );
		Warning( "Map with illegal characters in filename\n" );
		return false;
	}

	FileHandle_t mapfile;

	if ( IsX360() )
	{
		char szMapName360[MAX_PATH];
		UpdateOrCreate( szMapFile, szMapName360, sizeof( szMapName360 ), false );
		V_strcpy_safe( szMapFile, szMapName360 );
	}

	mapfile = g_pFileSystem->OpenEx( szMapFile, "rb", IsX360() ? FSOPEN_NEVERINPACK : 0, "GAME" );
	if ( mapfile != FILESYSTEM_INVALID_HANDLE )
	{
		dheader_t header;
		memset( &header, 0, sizeof( header ) );
		g_pFileSystem->Read( &header, sizeof( dheader_t ), mapfile );
		g_pFileSystem->Close( mapfile );

		if ( header.ident == IDBSPHEADER )
		{
			if ( header.version >= MINBSPVERSION && header.version <= BSPVERSION )
			{
				V_strncpy( s_szLastMapFile, szMapFile, sizeof( s_szLastMapFile ) );
				return true;
			}
			else
			{
				if ( !bQuiet )
				{
					Warning( "CModelLoader::Map_IsValid:  Map '%s' bsp version %i, expecting %i\n", szMapFile, header.version, BSPVERSION );
				}

			}
		}
		else
		{
			if ( !bQuiet )
			{
				Warning( "CModelLoader::Map_IsValid: '%s' is not a valid BSP file\n", szMapFile );
			}
		}
	}
	else
	{
		if ( !bQuiet )
		{
			Warning( "CModelLoader::Map_IsValid:  No such map '%s'\n", szMapFile );
		}
	}

	// Get outta here if we are checking vidmemstats.
	if ( CommandLine()->CheckParm( "-dumpvidmemstats" ) )
	{
		Cbuf_AddText( "quit\n" );
	}

	return false;
}

model_t *CModelLoader::FindModelNoCreate( const char *pModelName )
{
	FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( pModelName );
	int i = m_Models.Find( fnHandle );
	if ( i != m_Models.InvalidIndex() )
	{
		return m_Models[i].modelpointer;
	}

	// not found
	return NULL;
}

modtype_t CModelLoader::GetTypeFromName( const char *pModelName )
{
	// HACK HACK, force sprites to correctly
	const char *pExt = V_GetFileExtension( pModelName );
	if ( pExt )
	{
		if ( !V_stricmp( pExt, "spr" ) || !V_stricmp( pExt, "vmt" )  )
		{
			return mod_sprite;
		}
		else if ( !V_stricmp( pExt, "bsp" ) )
		{
			return mod_brush;
		}
		else if ( !V_stricmp( pExt, "mdl" ) )
		{
			return mod_studio;
		}
		else if ( g_pVideo != NULL && g_pVideo->LocateVideoSystemForPlayingFile( pModelName) != VideoSystem::NONE )		// video sprite
		{
			return mod_sprite;
		}
	}

	return mod_bad;
}

int	CModelLoader::FindNext( int iIndex, model_t **ppModel )
{
	if ( iIndex == -1 && m_Models.Count() )
	{
		iIndex = m_Models.FirstInorder();
	}
	else if ( !m_Models.Count() || !m_Models.IsValidIndex( iIndex ) )
	{
		*ppModel = NULL;
		return -1;
	}

	*ppModel = m_Models[iIndex].modelpointer;
	
	iIndex = m_Models.NextInorder( iIndex );
	if ( iIndex == m_Models.InvalidIndex() )
	{
		// end of list
		iIndex = -1;
	}

	return iIndex;
}

void CModelLoader::UnloadModel( model_t *pModel )
{
	switch ( pModel->type )
	{
	case mod_brush:
		// Let it free data or call destructors..
		Map_UnloadModel( pModel );

		// Remove from file system
		g_pFileSystem->RemoveSearchPath( pModel->strName, "GAME" );

		m_szActiveMapName[0] = '\0';
		break;

	case mod_studio:
		Studio_UnloadModel( pModel );
		break;

	case mod_sprite:
		Sprite_UnloadModel( pModel );
		break;
	}
}

const char *CModelLoader::GetActiveMapName( void )
{
	return m_szActiveMapName;
}

model_t *CModelLoader::GetDynamicModel( const char *name, bool bClientOnly )
{
	if ( !name || !name[0] )
	{
		name = "models/empty.mdl";
	}

	Assert( V_strnicmp( name, "models/", 7 ) == 0 && V_strstr( name, ".mdl" ) != NULL );

	model_t *pModel = FindModel( name );
	Assert( pModel );

	CDynamicModelInfo &dyn = m_DynamicModels[ m_DynamicModels.Insert( pModel ) ]; // Insert returns existing if key is already set
	if ( dyn.m_nLoadFlags == CDynamicModelInfo::INVALIDFLAG )
	{
		dyn.m_nLoadFlags = 0;
		DynamicModelDebugMsg( "model %p [%s] registered\n", pModel, pModel->strName.String() );
	}
	dyn.m_uLastTouchedMS_Div256 = Plat_MSTime() >> 8;

	return pModel;
}

void CModelLoader::UpdateDynamicModelLoadQueue()
{
	if ( mod_dynamicloadpause.GetBool() )
		return;

	static double s_LastDynamicLoadTime = 0.0;
	if ( mod_dynamicloadthrottle.GetFloat() > 0 && Plat_FloatTime() < s_LastDynamicLoadTime + mod_dynamicloadthrottle.GetFloat() )
		return;

	if ( m_bDynamicLoadQueueHeadActive )
	{
		Assert( m_DynamicModelLoadQueue.Count() >= 1 );
		MaterialLock_t matLock = g_pMaterialSystem->Lock(); // ASDFADFASFASEGAafliejsfjaslaslgsaigas
		bool bComplete = g_pQueuedLoader->CompleteDynamicLoad();
		g_pMaterialSystem->Unlock(matLock);

		if ( bComplete )
		{
			model_t *pModel = m_DynamicModelLoadQueue[0];
			m_DynamicModelLoadQueue.Remove(0);
			m_bDynamicLoadQueueHeadActive = false;

			Assert( pModel->nLoadFlags & FMODELLOADER_DYNAMIC );
			Assert( pModel->type == mod_bad || ( pModel->nLoadFlags & (FMODELLOADER_LOADED | FMODELLOADER_LOADED_BY_PRELOAD) ) );
			(void) LoadModel( pModel, NULL );
			Assert( pModel->type == mod_studio );

			UtlHashHandle_t hDyn = m_DynamicModels.Find( pModel );
			Assert( hDyn != m_DynamicModels.InvalidHandle() );
			if ( hDyn != m_DynamicModels.InvalidHandle() )
			{
				CDynamicModelInfo &dyn = m_DynamicModels[hDyn];
				Assert( dyn.m_nLoadFlags & CDynamicModelInfo::QUEUED );
				Assert( dyn.m_nLoadFlags & CDynamicModelInfo::LOADING );

				dyn.m_nLoadFlags &= ~( CDynamicModelInfo::QUEUED | CDynamicModelInfo::LOADING );

				g_pMDLCache->LockStudioHdr( pModel->studio );
				dyn.m_nLoadFlags |= CDynamicModelInfo::CLIENTREADY;

				dyn.m_uLastTouchedMS_Div256 = Plat_MSTime() >> 8;

				FinishDynamicModelLoadIfReady( &dyn, pModel );
			}

			// do the clean up after we're actually done
			// we keep some file cache around to make sure that LoadModel doesn't do blocking load
			g_pQueuedLoader->CleanupDynamicLoad();
			
			s_LastDynamicLoadTime = Plat_FloatTime();
		}
	}

	// If we're not working, and we have work to do, and the queued loader is open for business...
	if ( !m_bDynamicLoadQueueHeadActive && m_DynamicModelLoadQueue.Count() > 0 && g_pQueuedLoader->IsFinished() )
	{
		model_t *pModel = m_DynamicModelLoadQueue[0];
		UtlHashHandle_t hDyn = m_DynamicModels.Find( pModel );
		Assert( hDyn != m_DynamicModels.InvalidHandle() );
		if ( hDyn != m_DynamicModels.InvalidHandle() )
		{
			m_bDynamicLoadQueueHeadActive = true;

			CDynamicModelInfo &dyn = m_DynamicModels[hDyn];
			Assert( dyn.m_nLoadFlags & CDynamicModelInfo::QUEUED );
			Assert( !(dyn.m_nLoadFlags & CDynamicModelInfo::LOADING) );
			Assert( !(dyn.m_nLoadFlags & CDynamicModelInfo::CLIENTREADY) );
			dyn.m_nLoadFlags |= CDynamicModelInfo::LOADING;

			// the queued loader is very ... particular about path names. it doesn't like leading "models/"
			const char* pName = pModel->strName;
			if ( V_strnicmp( pName, "models", 6 ) == 0 && ( pName[6] == '/' || pName[6] == '\\' ) )
			{
				pName += 7;
			}

			MaterialLock_t matLock = g_pMaterialSystem->Lock();
			g_pQueuedLoader->DynamicLoadMapResource( pName, NULL, NULL, NULL );
			g_pMaterialSystem->Unlock(matLock);
		}
		else
		{
			m_DynamicModelLoadQueue.Remove(0);
		}
	}
}

void CModelLoader::FinishDynamicModelLoadIfReady( CDynamicModelInfo *pDyn, model_t *pModel )
{
	CDynamicModelInfo &dyn = *pDyn;
	if ( ( dyn.m_nLoadFlags & CDynamicModelInfo::CLIENTREADY ) )
	{
		if ( !( dyn.m_nLoadFlags & CDynamicModelInfo::SERVERLOADING ) )
		{
			// There ought to be a better way to plumb this through, but this should be ok...
			if ( sv.GetDynamicModelsTable() )
			{
				int netidx = sv.GetDynamicModelsTable()->FindStringIndex( pModel->strName );
				if ( netidx != INVALID_STRING_INDEX )
				{
					char nIsLoaded = 1;
					sv.GetDynamicModelsTable()->SetStringUserData( netidx, 1, &nIsLoaded );
				}
			}

			DynamicModelDebugMsg( "model %p [%s] loaded\n", pModel, pModel->strName.String() );

			dyn.m_nLoadFlags |= CDynamicModelInfo::ALLREADY;

			// Reverse order; UnregisterModelLoadCallback does a FastRemove that swaps from back
			for ( int i = dyn.m_Callbacks.Count()-1; i >= 0; --i )
			{
				uintptr_t callbackID = dyn.m_Callbacks[ i ];
				bool bClientOnly = (bool)(callbackID & 1);
				IModelLoadCallback* pCallback = ( IModelLoadCallback* )( callbackID & ~1 );
				UnregisterModelLoadCallback( pModel, bClientOnly, pCallback );
				pCallback->OnModelLoadComplete( pModel );
			}
		}
		else
		{
			// Reverse order; UnregisterModelLoadCallback does a FastRemove that swaps from back
			for ( int i = dyn.m_Callbacks.Count()-1; i >= 0; --i )
			{
				uintptr_t callbackID = dyn.m_Callbacks[ i ];
				bool bClientOnly = (bool)(callbackID & 1);
				IModelLoadCallback* pCallback = ( IModelLoadCallback* )( callbackID & ~1 );
				if ( bClientOnly )
				{
					UnregisterModelLoadCallback( pModel, true, pCallback );
					pCallback->OnModelLoadComplete( pModel );
				}
			}
		}
	}
}

bool CModelLoader::RegisterModelLoadCallback( model_t *pModel, bool bClientOnly, IModelLoadCallback *pCallback, bool bCallImmediatelyIfLoaded )
{
	UtlHashHandle_t hDyn = m_DynamicModels.Find( pModel );
	Assert( hDyn != m_DynamicModels.InvalidHandle() );
	if ( hDyn == m_DynamicModels.InvalidHandle() )
		return false;

	Assert( ((uintptr_t)pCallback & 1) == 0 );
	uintptr_t callbackID = (uintptr_t)pCallback | (uintptr_t)bClientOnly;

	int readyFlag = bClientOnly ? CDynamicModelInfo::CLIENTREADY : CDynamicModelInfo::ALLREADY;
	CDynamicModelInfo &dyn = m_DynamicModels[ hDyn ];
	AssertMsg( dyn.m_iRefCount > 0, "RegisterModelLoadCallback requires non-zero model refcount" );
	if ( dyn.m_nLoadFlags & readyFlag )
	{
		if ( !bCallImmediatelyIfLoaded )
			return false;

		pCallback->OnModelLoadComplete( pModel );
	}
	else
	{
		if ( !dyn.m_Callbacks.HasElement( callbackID ) )
		{
			dyn.m_Callbacks.AddToTail( callbackID );
			// Set registration count for callback pointer
			m_RegisteredDynamicCallbacks[ m_RegisteredDynamicCallbacks.Insert( callbackID, 0 ) ]++;
		}
	}

	return true;
}

bool CModelLoader::IsDynamicModelLoading( model_t *pModel, bool bClientOnly )
{
	Assert( pModel->nLoadFlags & FMODELLOADER_DYNAMIC );
	UtlHashHandle_t hDyn = m_DynamicModels.Find( pModel );
	Assert( hDyn != m_DynamicModels.InvalidHandle() );
	if ( hDyn != m_DynamicModels.InvalidHandle() )
	{
		CDynamicModelInfo &dyn = m_DynamicModels[ hDyn ];
		AssertMsg( dyn.m_iRefCount > 0, "dynamic model state cannot be queried with zero refcount" );
		if ( dyn.m_iRefCount > 0 )
		{
			int readyFlag = bClientOnly ? CDynamicModelInfo::CLIENTREADY : CDynamicModelInfo::ALLREADY;
			return !( dyn.m_nLoadFlags & readyFlag );
		}
	}
	return false;
}

void CModelLoader::AddRefDynamicModel( model_t *pModel, bool bClientSideRef  )
{
	extern IVModelInfo* modelinfo;

	UtlHashHandle_t hDyn = m_DynamicModels.Insert( pModel );
	CDynamicModelInfo& dyn = m_DynamicModels[ hDyn ];
	dyn.m_iRefCount++;
	dyn.m_iClientRefCount += ( bClientSideRef ? 1 : 0 );
	Assert( dyn.m_iRefCount > 0 );

	DynamicModelDebugMsg( "model %p [%s] addref %d (%d)\n", pModel, pModel->strName.String(), dyn.m_iRefCount, dyn.m_iClientRefCount );

	if ( !( dyn.m_nLoadFlags & ( CDynamicModelInfo::QUEUED | CDynamicModelInfo::CLIENTREADY ) ) )
	{
		QueueDynamicModelLoad( &dyn, pModel );

		// Try to kick it off asap if we aren't already busy.
		if ( !m_bDynamicLoadQueueHeadActive )
		{
			UpdateDynamicModelLoadQueue();
		}
	}
}

void CModelLoader::ReleaseDynamicModel( model_t *pModel, bool bClientSideRef )
{
	Assert( pModel->nLoadFlags & FMODELLOADER_DYNAMIC );
	UtlHashHandle_t hDyn = m_DynamicModels.Find( pModel );
	Assert( hDyn != m_DynamicModels.InvalidHandle() );
	if ( hDyn != m_DynamicModels.InvalidHandle() )
	{
		CDynamicModelInfo &dyn = m_DynamicModels[ hDyn ];
		Assert( dyn.m_iRefCount > 0 );
		if ( dyn.m_iRefCount > 0 )
		{
			DynamicModelDebugMsg( "model %p [%s] release %d (%dc)\n", pModel, pModel->strName.String(), dyn.m_iRefCount, dyn.m_iClientRefCount );
			dyn.m_iRefCount--;
			dyn.m_iClientRefCount -= ( bClientSideRef ? 1 : 0 );
			Assert( dyn.m_iClientRefCount >= 0 );
			if ( dyn.m_iClientRefCount < 0 )
				dyn.m_iClientRefCount = 0;
			dyn.m_uLastTouchedMS_Div256 = Plat_MSTime() >> 8;
		}
	}
}

void CModelLoader::UnregisterModelLoadCallback( model_t *pModel, bool bClientOnly, IModelLoadCallback *pCallback )
{
	Assert( ((uintptr_t)pCallback & 1) == 0 );
	uintptr_t callbackID = (uintptr_t)pCallback | (uintptr_t)bClientOnly;
	if ( int *pCallbackRegistrationCount = m_RegisteredDynamicCallbacks.GetPtr( callbackID ) )
	{
		if ( pModel )
		{
			UtlHashHandle_t i = m_DynamicModels.Find( pModel );
			if ( i != m_DynamicModels.InvalidHandle() )
			{
				CDynamicModelInfo &dyn = m_DynamicModels[ i ];
				if ( dyn.m_Callbacks.FindAndFastRemove( callbackID ) )
				{
					if ( dyn.m_Callbacks.Count() == 0 )
					{
						dyn.m_Callbacks.Purge();
					}
					if ( --(*pCallbackRegistrationCount) == 0 )
					{
						m_RegisteredDynamicCallbacks.Remove( callbackID );
						return;
					}
				}
			}
		}
		else
		{
			for ( UtlHashHandle_t i = m_DynamicModels.FirstHandle(); i != m_DynamicModels.InvalidHandle(); i = m_DynamicModels.NextHandle(i) )
			{
				CDynamicModelInfo &dyn = m_DynamicModels[ i ];
				if ( dyn.m_Callbacks.FindAndFastRemove( callbackID ) )
				{
					if ( dyn.m_Callbacks.Count() == 0 )
					{
						dyn.m_Callbacks.Purge();
					}
					if ( --(*pCallbackRegistrationCount) == 0 )
					{
						m_RegisteredDynamicCallbacks.Remove( callbackID );
						return;
					}
				}
			}
		}
	}
}

void CModelLoader::QueueDynamicModelLoad( CDynamicModelInfo *dyn, model_t *mod )
{
	Assert( !(dyn->m_nLoadFlags & CDynamicModelInfo::QUEUED) );
	// Client-side entities have priority over server-side entities
	// because they are more likely to be used in UI elements. --henryg
	if ( dyn->m_iClientRefCount > 0 && m_DynamicModelLoadQueue.Count() > 1 )
	{
		m_DynamicModelLoadQueue.InsertAfter( 0, mod );
	}
	else
	{
		m_DynamicModelLoadQueue.AddToTail( mod );
	}
	dyn->m_nLoadFlags |= CDynamicModelInfo::QUEUED;
	mod->nLoadFlags |= ( dyn->m_iClientRefCount > 0 ? FMODELLOADER_DYNCLIENT : FMODELLOADER_DYNSERVER );
}

bool CModelLoader::CancelDynamicModelLoad( CDynamicModelInfo *dyn, model_t *mod )
{
	int i = m_DynamicModelLoadQueue.Find( mod );
	Assert( (i < 0) == !(dyn->m_nLoadFlags & CDynamicModelInfo::QUEUED) );
	if ( i >= 0 )
	{
		if ( i == 0 && m_bDynamicLoadQueueHeadActive )
		{
			Assert( dyn->m_nLoadFlags & CDynamicModelInfo::LOADING );
			// can't remove head of queue
			return false;
		}
		else
		{
			Assert( dyn->m_nLoadFlags & CDynamicModelInfo::QUEUED );
			Assert( !(dyn->m_nLoadFlags & CDynamicModelInfo::LOADING) );
			m_DynamicModelLoadQueue.Remove( i );
			dyn->m_nLoadFlags &= ~CDynamicModelInfo::QUEUED;
			mod->nLoadFlags &= ~FMODELLOADER_DYNAMIC;
			return true;
		}
	}
	return false;
}

void CModelLoader::InternalUpdateDynamicModels( bool bIgnoreTime )
{
	const uint now = Plat_MSTime();
	const uint delay = bIgnoreTime ? 0 : (int)( clamp( mod_dynamicunloadtime.GetFloat(), 1.f, 600.f ) * 1000 );

	UpdateDynamicModelLoadQueue();

#ifdef _DEBUG
	extern CNetworkStringTableContainer *networkStringTableContainerServer;
	bool bPrevStringTableLockState = networkStringTableContainerServer->Lock( false );
#endif

	// Scan for models to unload. TODO: accelerate with a "models to potentially unload" list?
	UtlHashHandle_t i = m_DynamicModels.FirstHandle();
	while ( i != m_DynamicModels.InvalidHandle() )
	{
		model_t *pModel = m_DynamicModels.Key( i );
		CDynamicModelInfo& dyn = m_DynamicModels[ i ];

		// UNLOAD THIS MODEL if zero refcount and not currently loading, and either timed out or never loaded
		if ( dyn.m_iRefCount <= 0 && !(dyn.m_nLoadFlags & CDynamicModelInfo::LOADING) &&
			 ( ( now - (dyn.m_uLastTouchedMS_Div256 << 8) ) >= delay || !( dyn.m_nLoadFlags & CDynamicModelInfo::CLIENTREADY ) ) )
		{
			// Remove from load queue
			if ( dyn.m_nLoadFlags & CDynamicModelInfo::QUEUED )
			{
				if ( !CancelDynamicModelLoad( &dyn, pModel ) )
				{
					// Couldn't remove from queue, advance to next entry and do not remove
					i = m_DynamicModels.NextHandle(i);
					continue;
				}
			}

			// Unlock studiohdr_t
			if ( dyn.m_nLoadFlags & CDynamicModelInfo::CLIENTREADY )
			{
				g_pMDLCache->UnlockStudioHdr( pModel->studio );
			}

			// There ought to be a better way to plumb this through, but this should be ok...
			if ( sv.GetDynamicModelsTable() )
			{
				int netidx = sv.GetDynamicModelsTable()->FindStringIndex( pModel->strName );
				if ( netidx != INVALID_STRING_INDEX )
				{
					char nIsLoaded = 0;
					sv.GetDynamicModelsTable()->SetStringUserData( netidx, 1, &nIsLoaded );
				}
			}

			if ( pModel->nLoadFlags & FMODELLOADER_DYNAMIC )
			{
				pModel->nLoadFlags &= ~FMODELLOADER_DYNAMIC;
				// Actually unload the model if all system references are gone
				if ( pModel->nLoadFlags & FMODELLOADER_REFERENCEMASK )
				{
					DynamicModelDebugMsg( "model %p [%s] unload - deferred: non-dynamic reference\n", pModel, pModel->strName.String() );
				}
				else
				{
					DynamicModelDebugMsg( "model %p [%s] unload\n", pModel, pModel->strName.String() );
					
					Studio_UnloadModel( pModel );

					if ( mod_dynamicunloadtextures.GetBool() )
					{
						materials->UncacheUnusedMaterials( false );
					}
				}
			}

			// Remove from table, advance to next entry
			i = m_DynamicModels.RemoveAndAdvance(i);
			continue;
		}

		// Advance to next entry in table
		i = m_DynamicModels.NextHandle(i);
	}

#ifdef _DEBUG
	networkStringTableContainerServer->Lock( bPrevStringTableLockState );
#endif
}

void CModelLoader::Client_OnServerModelStateChanged( model_t *pModel, bool bServerLoaded )
{
#ifndef SWDS
	// Listen server don't distinguish between server and client ready, never use SERVERLOADING flag
	if ( sv.IsActive() ) 
		return;

	UtlHashHandle_t i = m_DynamicModels.Find( pModel );
	if ( i != m_DynamicModels.InvalidHandle() )
	{
		CDynamicModelInfo &dyn = m_DynamicModels[i];
		if ( !bServerLoaded )
		{
			if ( dyn.m_nLoadFlags & CDynamicModelInfo::ALLREADY )
				DynamicModelDebugMsg( "dynamic model [%s] loaded on client but not server! is this bad? unknown...", pModel->strName.String() );
			dyn.m_nLoadFlags &= ~CDynamicModelInfo::ALLREADY;
			dyn.m_nLoadFlags |= CDynamicModelInfo::SERVERLOADING;
		}
		else
		{
			dyn.m_nLoadFlags &= ~CDynamicModelInfo::SERVERLOADING;
			FinishDynamicModelLoadIfReady( &dyn, pModel );
		}
	}
#endif
}

void CModelLoader::ForceUnloadNonClientDynamicModels()
{
	UtlHashHandle_t i = m_DynamicModels.FirstHandle();
	while ( i != m_DynamicModels.InvalidHandle() )
	{
		CDynamicModelInfo &dyn = m_DynamicModels[i];
		dyn.m_iRefCount = dyn.m_iClientRefCount;
		i = m_DynamicModels.NextHandle( i );
	}

	// Flush everything
	InternalUpdateDynamicModels( true );
}


// reconstruct the ambient lighting for a leaf at the given position in worldspace
void Mod_LeafAmbientColorAtPos( Vector *pOut, const Vector &pos, int leafIndex )
{
	for ( int i = 0; i < 6; i++ )
	{
		pOut[i].Init();
	}
	mleafambientindex_t *pAmbient = &host_state.worldbrush->m_pLeafAmbient[leafIndex];
	if ( !pAmbient->ambientSampleCount && pAmbient->firstAmbientSample )
	{
		// this leaf references another leaf, move there (this leaf is a solid leaf so it borrows samples from a neighbor)
		leafIndex = pAmbient->firstAmbientSample;
		pAmbient = &host_state.worldbrush->m_pLeafAmbient[leafIndex];
	}
	int count = pAmbient->ambientSampleCount;
	if ( count > 0 )
	{
		int start = host_state.worldbrush->m_pLeafAmbient[leafIndex].firstAmbientSample;
		mleafambientlighting_t *pSamples = host_state.worldbrush->m_pAmbientSamples + start;
		mleaf_t *pLeaf = &host_state.worldbrush->leafs[leafIndex];
		float totalFactor = 0;
		for ( int i = 0; i < count; i++ )
		{
			// do an inverse squared distance weighted average of the samples to reconstruct 
			// the original function

			// the sample positions are packed as leaf bounds fractions, compute
			Vector samplePos = pLeaf->m_vecCenter - pLeaf->m_vecHalfDiagonal;
			samplePos.x += float(pSamples[i].x) * pLeaf->m_vecHalfDiagonal.x * (2.0f / 255.0f);
			samplePos.y += float(pSamples[i].y) * pLeaf->m_vecHalfDiagonal.y * (2.0f / 255.0f);
			samplePos.z += float(pSamples[i].z) * pLeaf->m_vecHalfDiagonal.z * (2.0f / 255.0f);

			float dist = (samplePos - pos).LengthSqr();
			float factor = 1.0f / (dist + 1.0f);
			totalFactor += factor;
			for ( int j = 0; j < 6; j++ )
			{
				Vector v;
				ColorRGBExp32ToVector( pSamples[i].cube.m_Color[j], v );
				pOut[j] += v * factor;
			}
		}
		for ( int i = 0; i < 6; i++ )
		{
			pOut[i] *= (1.0f / totalFactor);
		}
	}
}

#if defined( WIN32 )
int ComputeSize( studiohwdata_t *hwData, int *numVerts, int *pTriCount, bool onlyTopLod = false )
{
	unsigned size = 0;
	Assert(hwData && numVerts);
	int max_lod = (onlyTopLod ? 1 : hwData->m_NumLODs);
	*pTriCount = 0;
	for ( int i=0; i < max_lod; i++ )
	{
		studioloddata_t *pLOD = &hwData->m_pLODs[i];
		for ( int j = 0; j < hwData->m_NumStudioMeshes; j++ )
		{
			studiomeshdata_t *pMeshData = &pLOD->m_pMeshData[j];
			for ( int k = 0; k < pMeshData->m_NumGroup; k++ )
			{
				studiomeshgroup_t *pMeshGroup = &pMeshData->m_pMeshGroup[k];
				IMesh* mesh = pMeshGroup->m_pMesh;
				size += mesh->ComputeMemoryUsed();		// Size of VB and IB
				size += 2*pMeshGroup->m_NumVertices;	// Size of m_pGroupIndexToMeshIndex[] array
				*numVerts += mesh->VertexCount();
				Assert( mesh->VertexCount() == pMeshGroup->m_NumVertices );
				for ( int l = 0; l < pMeshGroup->m_NumStrips; ++l )
				{
					OptimizedModel::StripHeader_t *pStripData = &pMeshGroup->m_pStripData[l];
					*pTriCount += pStripData->numIndices / 3;
				}
			}
		}
	}
	return size;
}

// APSFIXME: needs to only do models that are resident, sizes might be wrong, i.e lacking compressed vert state?
CON_COMMAND_F( model_list, "Dump model list to file", FCVAR_CHEAT | FCVAR_DONTRECORD )
{
	// don't run this on dedicated servers
	if ( sv.IsDedicated() )
		return;

	if ( g_pFileSystem )
	{
		FileHandle_t fileHandle = g_pFileSystem->Open( "model_list.csv", "wt", "GAME" );

		if ( fileHandle )
		{
			const char *substring = NULL;
			if ( args.ArgC() > 1 )
			{
				substring = args[1];
			}

			g_pFileSystem->FPrintf( fileHandle, "name,dataSize,numVerts,nTriCount,dataSizeLod0,numVertsLod0,nTriCountLod0,numBones,numParts,numLODs,numMeshes\n" );
	
			for ( int i = 0; i < modelloader->GetCount(); i++ )
			{
				const char* name = "Unknown";
				int dataSizeLod0 = 0;
				int dataSize = 0;
				int numParts = 0;
				int numBones = 0;
				int numVertsLod0 = 0;
				int numVerts = 0;
				int numLODs = 0;
				int numMeshes = 0;
				int nTriCount = 0;
				int nTriCountLod0 = 0;

				model_t* model = modelloader->GetModelForIndex( i );
				if ( model )
				{
					// other model types are not interesting
					if ( model->type != mod_studio )
						continue;

					name = model->strName;

					if ( substring && substring[0] )
					{
						if ( Q_stristr( name, substring ) == NULL )
							continue;
					}

					studiohwdata_t *hwData = g_pMDLCache->GetHardwareData( model->studio );
					if ( hwData )
					{
						numMeshes = hwData->m_NumStudioMeshes;
						numLODs = hwData->m_NumLODs;
						dataSize = ComputeSize( hwData, &numVerts, &nTriCount, false ); // Size of vertex data
						dataSizeLod0 = ComputeSize( hwData, &numVertsLod0, &nTriCountLod0, true );
					}

					studiohdr_t *pStudioHdr = (studiohdr_t *)modelloader->GetExtraData( model );
					dataSize += pStudioHdr->length; // Size of MDL file
					numBones = pStudioHdr->numbones;
					numParts = pStudioHdr->numbodyparts;

					g_pFileSystem->FPrintf( fileHandle, "%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
						name, dataSize, numVerts, nTriCount, dataSizeLod0, numVertsLod0, nTriCountLod0, numBones, numParts, numLODs, numMeshes );
				}
			}

			g_pFileSystem->Close( fileHandle );
			Msg( "Created \"model_list.csv\" in the game folder.\n" );
		}
	}
}
#endif // WIN32



CON_COMMAND_F( mod_dynamicmodeldebug, "debug spew for dynamic model loading", FCVAR_HIDDEN | FCVAR_DONTRECORD )
{
	((CModelLoader*)modelloader)->DebugPrintDynamicModels();
}

#include "server.h"
#ifndef SWDS
#include "client.h"
#endif
void CModelLoader::DebugPrintDynamicModels()
{
	Msg( "network table (server):\n" );
	if ( sv.GetDynamicModelsTable() )
	{
		for ( int i = 0; i < sv.GetDynamicModelsTable()->GetNumStrings(); ++i )
		{
			int dummy = 0;
			char* data = (char*) sv.GetDynamicModelsTable()->GetStringUserData( i, &dummy );
			bool bLoadedOnServer = !(data && dummy && data[0] == 0);
			Msg( "%3i: %c %s\n", i, bLoadedOnServer ? '*' : ' ', sv.GetDynamicModelsTable()->GetString(i) );
		}
	}

#ifndef SWDS
	Msg( "\nnetwork table (client):\n" );
	if ( cl.m_pDynamicModelsTable )
	{
		for ( int i = 0; i < cl.m_pDynamicModelsTable->GetNumStrings(); ++i )
		{
			int dummy = 0;
			char* data = (char*) cl.m_pDynamicModelsTable->GetStringUserData( i, &dummy );
			bool bLoadedOnServer = !(data && dummy && data[0] == 0);
			Msg( "%3i: %c %s\n", i, bLoadedOnServer ? '*' : ' ', cl.m_pDynamicModelsTable->GetString(i) );
		}
	}
#endif

	extern IVModelInfo *modelinfo;
	extern IVModelInfoClient *modelinfoclient;
	Msg( "\ndynamic models:\n" );
	for ( UtlHashHandle_t h = m_DynamicModels.FirstHandle(); h != m_DynamicModels.InvalidHandle(); h = m_DynamicModels.NextHandle(h) )
	{
		CDynamicModelInfo &dyn = m_DynamicModels[h];
		int idx = modelinfo->GetModelIndex( m_DynamicModels.Key(h)->strName );
#ifndef SWDS
		if ( idx == -1 ) idx = modelinfoclient->GetModelIndex( m_DynamicModels.Key(h)->strName );
#endif
		Msg( "%d (%d%c): %s [ref: %d (%dc)] %s%s%s%s\n", idx, ((-2 - idx) >> 1), (idx & 1) ? 'c' : 's',
			m_DynamicModels.Key(h)->strName.String(), dyn.m_iRefCount, dyn.m_iClientRefCount,
			(dyn.m_nLoadFlags & CDynamicModelInfo::QUEUED) ? " QUEUED" : "",
			(dyn.m_nLoadFlags & CDynamicModelInfo::LOADING) ? " LOADING" : "",
			(dyn.m_nLoadFlags & CDynamicModelInfo::CLIENTREADY) ? " CLIENTREADY" : "",
			(dyn.m_nLoadFlags & CDynamicModelInfo::ALLREADY) ? " ALLREADY" : "" );
	}
}
