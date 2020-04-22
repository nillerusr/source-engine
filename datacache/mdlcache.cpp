//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// model loading and caching
//
//===========================================================================//

#include <memory.h>
#include "tier0/vprof.h"
#include "tier0/icommandline.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlmap.h"
#include "datacache/imdlcache.h"
#include "istudiorender.h"
#include "filesystem.h"
#include "optimize.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imesh.h"
#include "datacache/idatacache.h"
#include "studio.h"
#include "vcollide.h"
#include "utldict.h"
#include "convar.h"
#include "datacache_common.h"
#include "mempool.h"
#include "vphysics_interface.h"
#include "phyfile.h"
#include "studiobyteswap.h"
#include "tier2/fileutils.h"
#include "filesystem/IQueuedLoader.h"
#include "tier1/lzmaDecoder.h"
#include "functors.h"

// XXX remove this later. (henryg)
#if 0 && defined(_DEBUG) && defined(_WIN32) && !defined(_X360)
typedef struct LARGE_INTEGER { unsigned long long QuadPart; } LARGE_INTEGER;
extern "C" void __stdcall OutputDebugStringA( const char *lpOutputString );
extern "C" long __stdcall QueryPerformanceCounter( LARGE_INTEGER *lpPerformanceCount );
extern "C" long __stdcall QueryPerformanceFrequency( LARGE_INTEGER *lpPerformanceCount );
namespace {
	class CDebugMicroTimer
	{
	public:
		CDebugMicroTimer(const char* n) : name(n) { QueryPerformanceCounter(&start); }
		~CDebugMicroTimer() {
			LARGE_INTEGER end;
			char outbuf[128];
			QueryPerformanceCounter(&end);
			if (!freq) QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
			V_snprintf(outbuf, 128, "%s %6d us\n", name, (int)((end.QuadPart - start.QuadPart) * 1000000 / freq));
			OutputDebugStringA(outbuf);
		}
		LARGE_INTEGER start;
		const char* name;
		static long long freq;
	};
	long long CDebugMicroTimer::freq = 0;
}
#define DEBUG_SCOPE_TIMER(name) CDebugMicroTimer dbgLocalTimer(#name)
#else
#define DEBUG_SCOPE_TIMER(name) (void)0
#endif

#ifdef _RETAIL
#define NO_LOG_MDLCACHE 1
#endif

#ifdef NO_LOG_MDLCACHE
#define LogMdlCache() 0
#else
#define LogMdlCache() mod_trace_load.GetBool()
#endif

#define MdlCacheMsg		if ( !LogMdlCache() ) ; else Msg
#define MdlCacheWarning if ( !LogMdlCache() ) ; else Warning

#if defined( _X360 )
#define AsyncMdlCache() 0	// Explicitly OFF for 360 (incompatible)
#else
#define AsyncMdlCache() 0
#endif

#define ERROR_MODEL		"models/error.mdl"
#define IDSTUDIOHEADER	(('T'<<24)+('S'<<16)+('D'<<8)+'I')

#define MakeCacheID( handle, type )	( ( (uint)(handle) << 16 ) | (uint)(type) )
#define HandleFromCacheID( id)		( (MDLHandle_t)((id) >> 16) )
#define TypeFromCacheID( id )		( (MDLCacheDataType_t)((id) & 0xffff) )

enum
{
	STUDIODATA_FLAGS_STUDIOMESH_LOADED	= 0x0001,
	STUDIODATA_FLAGS_VCOLLISION_LOADED	= 0x0002,
	STUDIODATA_ERROR_MODEL				= 0x0004,
	STUDIODATA_FLAGS_NO_STUDIOMESH		= 0x0008,
	STUDIODATA_FLAGS_NO_VERTEX_DATA		= 0x0010,
	STUDIODATA_FLAGS_VCOLLISION_SHARED	= 0x0020,
	STUDIODATA_FLAGS_LOCKED_MDL			= 0x0040,
};

// only models with type "mod_studio" have this data
struct studiodata_t
{
	// The .mdl file
	DataCacheHandle_t	m_MDLCache;

	// the vphysics.dll collision model
	vcollide_t			m_VCollisionData;

	studiohwdata_t		m_HardwareData;
#if defined( USE_HARDWARE_CACHE )
	DataCacheHandle_t	m_HardwareDataCache;
#endif

	unsigned short		m_nFlags;

	short				m_nRefCount;

	// pointer to the virtual version of the model
	virtualmodel_t		*m_pVirtualModel;

	// array of cache handles to demand loaded virtual model data
	int					m_nAnimBlockCount;
	DataCacheHandle_t	*m_pAnimBlock;
	unsigned long	 	*m_iFakeAnimBlockStall;

	// vertex data is usually compressed to save memory (model decal code only needs some data)
	DataCacheHandle_t	m_VertexCache;
	bool				m_VertexDataIsCompressed;

	int					m_nAutoplaySequenceCount;
	unsigned short		*m_pAutoplaySequenceList;

	void				*m_pUserData;

	DECLARE_FIXEDSIZE_ALLOCATOR_MT( studiodata_t );
};

DEFINE_FIXEDSIZE_ALLOCATOR_MT( studiodata_t, 128, CUtlMemoryPool::GROW_SLOW );

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CTempAllocHelper
{
public:
	CTempAllocHelper()
	{
		m_pData = NULL;
	}

	~CTempAllocHelper()
	{
		Free();
	}

	void *Get()
	{
		return m_pData;
	}

	void Alloc( int nSize )
	{
		m_pData = malloc( nSize );
	}

	void Free()
	{
		if ( m_pData )
		{
			free( m_pData );
			m_pData = NULL;
		}
	}
private:
	void *m_pData;
};

//-----------------------------------------------------------------------------
// ConVars
//-----------------------------------------------------------------------------
static ConVar r_rootlod( "r_rootlod", "0", FCVAR_ARCHIVE );
static ConVar mod_forcedata( "mod_forcedata", ( AsyncMdlCache() ) ? "0" : "1",	0, "Forces all model file data into cache on model load." );
static ConVar mod_test_not_available( "mod_test_not_available", "0", FCVAR_CHEAT );
static ConVar mod_test_mesh_not_available( "mod_test_mesh_not_available", "0", FCVAR_CHEAT );
static ConVar mod_test_verts_not_available( "mod_test_verts_not_available", "0", FCVAR_CHEAT );
static ConVar mod_load_mesh_async( "mod_load_mesh_async", ( AsyncMdlCache() ) ? "1" : "0" );
static ConVar mod_load_anims_async( "mod_load_anims_async", ( IsX360() || AsyncMdlCache() ) ? "1" : "0" );
static ConVar mod_load_vcollide_async( "mod_load_vcollide_async",  ( AsyncMdlCache() ) ? "1" : "0" );
static ConVar mod_trace_load( "mod_trace_load", "0" );
static ConVar mod_lock_mdls_on_load( "mod_lock_mdls_on_load", ( IsX360() ) ? "1" : "0" );
static ConVar mod_load_fakestall( "mod_load_fakestall", "0", 0, "Forces all ANI file loading to stall for specified ms\n");

//-----------------------------------------------------------------------------
// Utility functions
//-----------------------------------------------------------------------------

#if defined( USE_HARDWARE_CACHE )
unsigned ComputeHardwareDataSize( studiohwdata_t *pData )
{
	unsigned size = 0;
	for ( int i = pData->m_RootLOD; i < pData->m_NumLODs; i++ )
	{
		studioloddata_t *pLOD = &pData->m_pLODs[i];
		for ( int j = 0; j < pData->m_NumStudioMeshes; j++ )
		{
			studiomeshdata_t *pMeshData = &pLOD->m_pMeshData[j];
			for ( int k = 0; k < pMeshData->m_NumGroup; k++ )
			{
				size += pMeshData->m_pMeshGroup[k].m_pMesh->ComputeMemoryUsed();
			}
		}
	}
	return size;
}
#endif

//-----------------------------------------------------------------------------
// Async support
//-----------------------------------------------------------------------------

#define MDLCACHE_NONE ((MDLCacheDataType_t)-1)

struct AsyncInfo_t
{
	AsyncInfo_t() : hControl( NULL ), hModel( MDLHANDLE_INVALID ), type( MDLCACHE_NONE ), iAnimBlock( 0 ) {}

	FSAsyncControl_t	hControl;
	MDLHandle_t			hModel;
	MDLCacheDataType_t	type;
	int					iAnimBlock;
};

const int NO_ASYNC = CUtlLinkedList< AsyncInfo_t >::InvalidIndex();

//-------------------------------------

CUtlMap<int, int> g_AsyncInfoMap( DefLessFunc( int ) );
CThreadFastMutex g_AsyncInfoMapMutex;

inline int MakeAsyncInfoKey( MDLHandle_t hModel, MDLCacheDataType_t type, int iAnimBlock )
{
	Assert( type <= 7 && iAnimBlock < 8*1024 );
	return ( ( ( (int)hModel) << 16 ) | ( (int)type << 13 ) | iAnimBlock );
}

inline int GetAsyncInfoIndex( MDLHandle_t hModel, MDLCacheDataType_t type, int iAnimBlock = 0 )
{
	AUTO_LOCK( g_AsyncInfoMapMutex );
	int key = MakeAsyncInfoKey( hModel, type, iAnimBlock );
	int i = g_AsyncInfoMap.Find( key );
	if ( i == g_AsyncInfoMap.InvalidIndex() )
	{
		return NO_ASYNC;
	}
	return g_AsyncInfoMap[i];
}

inline int SetAsyncInfoIndex( MDLHandle_t hModel, MDLCacheDataType_t type, int iAnimBlock, int index )
{
	AUTO_LOCK( g_AsyncInfoMapMutex );
	Assert( index == NO_ASYNC || GetAsyncInfoIndex( hModel, type, iAnimBlock ) == NO_ASYNC );
	int key = MakeAsyncInfoKey( hModel, type, iAnimBlock );
	if ( index == NO_ASYNC )
	{
		g_AsyncInfoMap.Remove( key );
	}
	else
	{
		g_AsyncInfoMap.Insert( key, index );
	}

	return index;
}

inline int SetAsyncInfoIndex( MDLHandle_t hModel, MDLCacheDataType_t type, int index )
{
	return SetAsyncInfoIndex( hModel, type, 0, index );
}

//-----------------------------------------------------------------------------
// QUEUED LOADING
// Populates the cache by pushing expected MDL's (and all of their data).
// The Model cache i/o behavior is unchanged during gameplay, ideally the cache
// should yield miss free behaviour.
//-----------------------------------------------------------------------------

struct ModelParts_t
{
	enum BufferType_t
	{
		BUFFER_MDL = 0,
		BUFFER_VTX = 1,
		BUFFER_VVD = 2,
		BUFFER_PHY = 3,
		BUFFER_MAXPARTS,
	};

	ModelParts_t()
	{
		nLoadedParts = 0;
		nExpectedParts = 0;
		hMDL = MDLHANDLE_INVALID;
		hFileCache = 0;
		bHeaderLoaded = false;
		bMaterialsPending = false;
		bTexturesPending = false;
	}

	// thread safe, only one thread will get a positive result
	bool DoFinalProcessing()
	{
		// indicates that all buffers have arrived
		// when all parts are present, returns true ( guaranteed once ), and marked as completed
		return nLoadedParts.AssignIf( nExpectedParts, nExpectedParts | 0x80000000 );
	}

	CUtlBuffer		Buffers[BUFFER_MAXPARTS];
	MDLHandle_t		hMDL;

	// async material loading on PC
	FileCacheHandle_t hFileCache;
	bool			bHeaderLoaded;
	bool			bMaterialsPending;
	bool			bTexturesPending;
	CUtlVector< IMaterial* > Materials;

	// bit flags
	CInterlockedInt	nLoadedParts;
	int				nExpectedParts;

private:
	ModelParts_t(const ModelParts_t&); // no impl
	ModelParts_t& operator=(const ModelParts_t&); // no impl
};

struct CleanupModelParts_t
{
	FileCacheHandle_t hFileCache;
	CUtlVector< IMaterial* > Materials;
};

//-----------------------------------------------------------------------------
// Implementation of the simple studio data cache (no caching)
//-----------------------------------------------------------------------------
class CMDLCache : public CTier3AppSystem< IMDLCache >, public IStudioDataCache, public CDefaultDataCacheClient
{
	typedef CTier3AppSystem< IMDLCache > BaseClass;

public:
	CMDLCache();

	// Inherited from IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Inherited from IStudioDataCache
	bool VerifyHeaders( studiohdr_t *pStudioHdr );
	vertexFileHeader_t *CacheVertexData( studiohdr_t *pStudioHdr );

	// Inherited from IMDLCache
	virtual MDLHandle_t FindMDL( const char *pMDLRelativePath );
	virtual int AddRef( MDLHandle_t handle );
	virtual int Release( MDLHandle_t handle );
	virtual int GetRef( MDLHandle_t handle );
	virtual void MarkAsLoaded(MDLHandle_t handle);

	virtual studiohdr_t *GetStudioHdr( MDLHandle_t handle );
	virtual studiohwdata_t *GetHardwareData( MDLHandle_t handle );
	virtual vcollide_t *GetVCollide( MDLHandle_t handle ) { return GetVCollideEx( handle, true); }
	virtual vcollide_t *GetVCollideEx( MDLHandle_t handle, bool synchronousLoad = true );
	virtual unsigned char *GetAnimBlock( MDLHandle_t handle, int nBlock );
	virtual virtualmodel_t *GetVirtualModel( MDLHandle_t handle );
	virtual virtualmodel_t *GetVirtualModelFast( const studiohdr_t *pStudioHdr, MDLHandle_t handle );
	virtual int GetAutoplayList( MDLHandle_t handle, unsigned short **pOut );
	virtual void TouchAllData( MDLHandle_t handle );
	virtual void SetUserData( MDLHandle_t handle, void* pData );
	virtual void *GetUserData( MDLHandle_t handle );
	virtual bool IsErrorModel( MDLHandle_t handle );
	virtual void SetCacheNotify( IMDLCacheNotify *pNotify );
	virtual vertexFileHeader_t *GetVertexData( MDLHandle_t handle );
	virtual void Flush( MDLCacheFlush_t nFlushFlags = MDLCACHE_FLUSH_ALL );
	virtual void Flush( MDLHandle_t handle, int nFlushFlags = MDLCACHE_FLUSH_ALL );
	virtual const char *GetModelName( MDLHandle_t handle );

	IDataCacheSection *GetCacheSection( MDLCacheDataType_t type )
	{
		switch ( type )
		{
		case MDLCACHE_STUDIOHWDATA:
		case MDLCACHE_VERTEXES:
			// meshes and vertexes are isolated to their own section
			return m_pMeshCacheSection;

		case MDLCACHE_ANIMBLOCK:
			// anim blocks have their own section
			return m_pAnimBlockCacheSection;

		default:
			// everybody else
			return m_pModelCacheSection;
		}
	}

	void *AllocData( MDLCacheDataType_t type, int size );
	void FreeData( MDLCacheDataType_t type, void *pData );
	void CacheData( DataCacheHandle_t *c, void *pData, int size, const char *name, MDLCacheDataType_t type, DataCacheClientID_t id = (DataCacheClientID_t)-1 );
	void *CheckData( DataCacheHandle_t c, MDLCacheDataType_t type );
	void *CheckDataNoTouch( DataCacheHandle_t c, MDLCacheDataType_t type );
	void UncacheData( DataCacheHandle_t c, MDLCacheDataType_t type, bool bLockedOk = false );

	void DisableAsync() { mod_load_mesh_async.SetValue( 0 ); mod_load_anims_async.SetValue( 0 ); }

	virtual void BeginLock();
	virtual void EndLock();
	virtual int *GetFrameUnlockCounterPtrOLD();
	virtual int *GetFrameUnlockCounterPtr( MDLCacheDataType_t type );

	virtual void FinishPendingLoads();

	// Task switch
	void ReleaseMaterialSystemObjects();
	void RestoreMaterialSystemObjects( int nChangeFlags );
	virtual bool GetVCollideSize( MDLHandle_t handle, int *pVCollideSize );

	virtual void BeginMapLoad();
	virtual void EndMapLoad();

	virtual void InitPreloadData( bool rebuild );
	virtual void ShutdownPreloadData();

	virtual bool IsDataLoaded( MDLHandle_t handle, MDLCacheDataType_t type );

	virtual studiohdr_t *LockStudioHdr( MDLHandle_t handle );
	virtual void UnlockStudioHdr( MDLHandle_t handle );

	virtual bool PreloadModel( MDLHandle_t handle );
	virtual void ResetErrorModelStatus( MDLHandle_t handle );

	virtual void MarkFrame();

	// Queued loading
	void ProcessQueuedData( ModelParts_t *pModelParts, bool bHeaderOnly = false );
	static void	QueuedLoaderCallback_MDL( void *pContext, void  *pContext2, const void *pData, int nSize, LoaderError_t loaderError );
	static void	ProcessDynamicLoad( ModelParts_t *pModelParts );
	static void CleanupDynamicLoad( CleanupModelParts_t *pCleanup );

private:
	// Inits, shuts downs studiodata_t
	void InitStudioData( MDLHandle_t handle );
	void ShutdownStudioData( MDLHandle_t handle );

	// Returns the *actual* name of the model (could be an error model if the requested model didn't load)
	const char *GetActualModelName( MDLHandle_t handle );

	// Constructs a filename based on a model handle
	void MakeFilename( MDLHandle_t handle, const char *pszExtension, char *pszFileName, int nMaxLength );

	// Inform filesystem that we unloaded a particular file
	void NotifyFileUnloaded( MDLHandle_t handle, const char *pszExtension );

	// Attempts to load a MDL file, validates that it's ok.
	bool ReadMDLFile( MDLHandle_t handle, const char *pMDLFileName, CUtlBuffer &buf );

	// Unserializes the VCollide file associated w/ models (the vphysics representation)
	void UnserializeVCollide( MDLHandle_t handle, bool synchronousLoad );

	// Destroys the VCollide associated w/ models
	void DestroyVCollide( MDLHandle_t handle );

	// Unserializes the MDL
	studiohdr_t *UnserializeMDL( MDLHandle_t handle, void *pData, int nDataSize, bool bDataValid );

	// Unserializes an animation block from disk
	unsigned char *UnserializeAnimBlock( MDLHandle_t handle, int nBlock );

	// Allocates/frees the anim blocks
	void AllocateAnimBlocks( studiodata_t *pStudioData, int nCount );
	void FreeAnimBlocks( MDLHandle_t handle );

	// Allocates/frees the virtual model
	void AllocateVirtualModel( MDLHandle_t handle );
	void FreeVirtualModel( MDLHandle_t handle );

	// Purpose: Pulls all submodels/.ani file models into the cache
	void UnserializeAllVirtualModelsAndAnimBlocks( MDLHandle_t handle );

	// Loads/unloads the static meshes
	bool LoadHardwareData( MDLHandle_t handle ); // returns false if not ready
	void UnloadHardwareData( MDLHandle_t handle, bool bCacheRemove = true, bool bLockedOk = false );

	// Allocates/frees autoplay sequence list
	void AllocateAutoplaySequences( studiodata_t *pStudioData, int nCount );
	void FreeAutoplaySequences( studiodata_t *pStudioData );

	FSAsyncStatus_t LoadData( const char *pszFilename, const char *pszPathID, bool bAsync, FSAsyncControl_t *pControl ) { return LoadData( pszFilename, pszPathID, NULL, 0, 0, bAsync, pControl ); }
	FSAsyncStatus_t LoadData( const char *pszFilename, const char *pszPathID, void *pDest, int nBytes, int nOffset, bool bAsync, FSAsyncControl_t *pControl );
	vertexFileHeader_t *LoadVertexData( studiohdr_t *pStudioHdr );
	vertexFileHeader_t *BuildAndCacheVertexData( studiohdr_t *pStudioHdr, vertexFileHeader_t *pRawVvdHdr  );
	bool BuildHardwareData( MDLHandle_t handle, studiodata_t *pStudioData, studiohdr_t *pStudioHdr, OptimizedModel::FileHeader_t *pVtxHdr );
	void ConvertFlexData( studiohdr_t *pStudioHdr );

	int ProcessPendingAsync( int iAsync );
	void ProcessPendingAsyncs( MDLCacheDataType_t type = MDLCACHE_NONE );
	bool ClearAsync( MDLHandle_t handle, MDLCacheDataType_t type, int iAnimBlock, bool bAbort = false );

	const char *GetVTXExtension();

	virtual bool HandleCacheNotification( const DataCacheNotification_t &notification  );
	virtual bool GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen  );

	virtual bool GetAsyncLoad( MDLCacheDataType_t type );
	virtual bool SetAsyncLoad( MDLCacheDataType_t type, bool bAsync );

	// Creates the 360 file if it doesn't exist or is out of date
	int UpdateOrCreate( studiohdr_t *pHdr, const char *pFilename, char *pX360Filename, int maxLen, const char *pPathID, bool bForce = false );

	// Attempts to read the platform native file - on 360 it can read and swap Win32 file as a fallback
	bool ReadFileNative( char *pFileName, const char *pPath, CUtlBuffer &buf, int nMaxBytes = 0 );

	// Creates a thin cache entry (to be used for model decals) from fat vertex data
	vertexFileHeader_t * CreateThinVertexes( vertexFileHeader_t * originalData, const studiohdr_t * pStudioHdr, int * cacheLength );

	// Processes raw data (from an I/O source) into the cache. Sets the cache state as expected for bad data.
	bool ProcessDataIntoCache( MDLHandle_t handle, MDLCacheDataType_t type, int iAnimBlock, void *pData, int nDataSize, bool bDataValid );

	void BreakFrameLock( bool bModels = true, bool bMesh = true );
	void RestoreFrameLock();

private:
	IDataCacheSection *m_pModelCacheSection;
	IDataCacheSection *m_pMeshCacheSection;
	IDataCacheSection *m_pAnimBlockCacheSection;

	int m_nModelCacheFrameLocks;
	int m_nMeshCacheFrameLocks;

	CUtlDict< studiodata_t*, MDLHandle_t > m_MDLDict;

	IMDLCacheNotify *m_pCacheNotify;

	CUtlFixedLinkedList< AsyncInfo_t > m_PendingAsyncs;

	CThreadFastMutex m_QueuedLoadingMutex;
	CThreadFastMutex m_AsyncMutex;

	bool m_bLostVideoMemory : 1;
	bool m_bConnected : 1;
	bool m_bInitialized : 1;
};

//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
static CMDLCache g_MDLCache;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMDLCache, IMDLCache, MDLCACHE_INTERFACE_VERSION, g_MDLCache );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMDLCache, IStudioDataCache, STUDIO_DATA_CACHE_INTERFACE_VERSION, g_MDLCache );


//-----------------------------------------------------------------------------
// Task switch
//-----------------------------------------------------------------------------
static void ReleaseMaterialSystemObjects( )
{
	g_MDLCache.ReleaseMaterialSystemObjects();
}

static void RestoreMaterialSystemObjects( int nChangeFlags )
{
	g_MDLCache.RestoreMaterialSystemObjects( nChangeFlags );
}



//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMDLCache::CMDLCache() : BaseClass( false )
{
	m_bLostVideoMemory = false;
	m_bConnected = false;
	m_bInitialized = false;
	m_pCacheNotify = NULL;
	m_pModelCacheSection = NULL;
	m_pMeshCacheSection = NULL;
	m_pAnimBlockCacheSection = NULL;
	m_nModelCacheFrameLocks = 0;
	m_nMeshCacheFrameLocks = 0;
}


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CMDLCache::Connect( CreateInterfaceFn factory )
{
	// Connect can be called twice, because this inherits from 2 appsystems.
	if ( m_bConnected )
		return true;

	if ( !BaseClass::Connect( factory ) )
		return false;

	if ( !g_pMaterialSystemHardwareConfig || !g_pPhysicsCollision || !g_pStudioRender || !g_pMaterialSystem )
		return false;

	m_bConnected = true;
	if( g_pMaterialSystem )
	{
		g_pMaterialSystem->AddReleaseFunc( ::ReleaseMaterialSystemObjects );
		g_pMaterialSystem->AddRestoreFunc( ::RestoreMaterialSystemObjects );
	}

	return true;
}

void CMDLCache::Disconnect()
{
	if ( g_pMaterialSystem && m_bConnected )
	{
		g_pMaterialSystem->RemoveReleaseFunc( ::ReleaseMaterialSystemObjects );
		g_pMaterialSystem->RemoveRestoreFunc( ::RestoreMaterialSystemObjects );
		m_bConnected = false;
	}

	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Query Interface
//-----------------------------------------------------------------------------
void *CMDLCache::QueryInterface( const char *pInterfaceName )
{
	if (!Q_strncmp(	pInterfaceName, STUDIO_DATA_CACHE_INTERFACE_VERSION, Q_strlen(STUDIO_DATA_CACHE_INTERFACE_VERSION) + 1))
		return (IStudioDataCache*)this;

	if (!Q_strncmp(	pInterfaceName, MDLCACHE_INTERFACE_VERSION, Q_strlen(MDLCACHE_INTERFACE_VERSION) + 1))
		return (IMDLCache*)this;

	return NULL;
}


//-----------------------------------------------------------------------------
// Init/Shutdown
//-----------------------------------------------------------------------------

#define MODEL_CACHE_MODEL_SECTION_NAME		"ModelData"
#define MODEL_CACHE_MESH_SECTION_NAME		"ModelMesh"
#define MODEL_CACHE_ANIMBLOCK_SECTION_NAME	"AnimBlock"

// #define ENABLE_CACHE_WATCH 1

#if defined( ENABLE_CACHE_WATCH )
static ConVar cache_watch( "cache_watch", "", 0 );

static void CacheLog( const char *fileName, const char *accessType )
{
	if ( Q_stristr( fileName, cache_watch.GetString() ) )
	{
		Msg( "%s access to %s\n", accessType, fileName );
	}
}
#endif

InitReturnVal_t CMDLCache::Init()
{
	// Can be called twice since it inherits from 2 appsystems
	if ( m_bInitialized )
		return INIT_OK;

	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	if ( !m_pModelCacheSection )
	{
		m_pModelCacheSection = g_pDataCache->AddSection( this, MODEL_CACHE_MODEL_SECTION_NAME );
	}

	if ( !m_pMeshCacheSection )
	{
		unsigned int meshLimit = (unsigned)-1;
		DataCacheLimits_t limits( meshLimit, (unsigned)-1, 0, 0 );
		m_pMeshCacheSection = g_pDataCache->AddSection( this, MODEL_CACHE_MESH_SECTION_NAME, limits );
	}

	if ( !m_pAnimBlockCacheSection )
	{
		// 360 tuned to worst case, ep_outland_12a, less than 6 MB is not a viable working set
		unsigned int animBlockLimit = IsX360() ? 6*1024*1024 : (unsigned)-1;
		DataCacheLimits_t limits( animBlockLimit, (unsigned)-1, 0, 0 );
		m_pAnimBlockCacheSection = g_pDataCache->AddSection( this, MODEL_CACHE_ANIMBLOCK_SECTION_NAME, limits );
	}

	if ( IsX360() )
	{
		// By default, source data is assumed to be non-native to the 360.
		StudioByteSwap::ActivateByteSwapping( true );
		StudioByteSwap::SetCollisionInterface( g_pPhysicsCollision );
	}
	m_bLostVideoMemory = false;
	m_bInitialized = true;

#if defined( ENABLE_CACHE_WATCH )
	g_pFullFileSystem->AddLoggingFunc( &CacheLog );
#endif

	return INIT_OK;
}

void CMDLCache::Shutdown()
{
	if ( !m_bInitialized )
		return;
#if defined( ENABLE_CACHE_WATCH )
	g_pFullFileSystem->RemoveLoggingFunc( CacheLog );
#endif
	m_bInitialized = false;

	if ( m_pModelCacheSection || m_pMeshCacheSection )
	{
		// Free all MDLs that haven't been cleaned up
		MDLHandle_t i = m_MDLDict.First();
		while ( i != m_MDLDict.InvalidIndex() )
		{
			ShutdownStudioData( i );
			i = m_MDLDict.Next( i );
		}

		m_MDLDict.Purge();

		if ( m_pModelCacheSection )
		{
			g_pDataCache->RemoveSection( MODEL_CACHE_MODEL_SECTION_NAME );
			m_pModelCacheSection = NULL;
		}
		if ( m_pMeshCacheSection )
		{
			g_pDataCache->RemoveSection( MODEL_CACHE_MESH_SECTION_NAME );
			m_pMeshCacheSection = NULL;
		}
	}

	if ( m_pAnimBlockCacheSection )
	{
		g_pDataCache->RemoveSection( MODEL_CACHE_ANIMBLOCK_SECTION_NAME );
		m_pAnimBlockCacheSection = NULL;
	}

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Flushes an MDLHandle_t
//-----------------------------------------------------------------------------
void CMDLCache::Flush( MDLHandle_t handle, int nFlushFlags )
{
	studiodata_t *pStudioData = m_MDLDict[handle];
	Assert( pStudioData != NULL );

	bool bIgnoreLock = ( nFlushFlags & MDLCACHE_FLUSH_IGNORELOCK ) != 0;

	// release the hardware portion
	if ( nFlushFlags & MDLCACHE_FLUSH_STUDIOHWDATA )
	{
		if ( ClearAsync( handle, MDLCACHE_STUDIOHWDATA, 0, true ) )
		{
			m_pMeshCacheSection->Unlock( pStudioData->m_VertexCache );
		}
		UnloadHardwareData( handle, true, bIgnoreLock );
	}

	// free collision
	if ( nFlushFlags & MDLCACHE_FLUSH_VCOLLIDE )
	{
		DestroyVCollide( handle );
	}

	// Free animations
	if ( nFlushFlags & MDLCACHE_FLUSH_VIRTUALMODEL )
	{
		FreeVirtualModel( handle );
	}

	if ( nFlushFlags & MDLCACHE_FLUSH_ANIMBLOCK )
	{
		FreeAnimBlocks( handle );
	}

	if ( nFlushFlags & MDLCACHE_FLUSH_AUTOPLAY )
	{
		// Free autoplay sequences
		FreeAutoplaySequences( pStudioData );
	}

	if ( nFlushFlags & MDLCACHE_FLUSH_STUDIOHDR )
	{
		MdlCacheMsg( "MDLCache: Free studiohdr %s\n", GetModelName( handle ) );

		if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_LOCKED_MDL )
		{
			GetCacheSection( MDLCACHE_STUDIOHDR )->Unlock( pStudioData->m_MDLCache );
			pStudioData->m_nFlags &= ~STUDIODATA_FLAGS_LOCKED_MDL;
		}
		UncacheData( pStudioData->m_MDLCache, MDLCACHE_STUDIOHDR, bIgnoreLock );
		pStudioData->m_MDLCache = NULL;
	}

	if ( nFlushFlags & MDLCACHE_FLUSH_VERTEXES )
	{
		MdlCacheMsg( "MDLCache: Free VVD %s\n", GetModelName( handle ) );

		ClearAsync( handle, MDLCACHE_VERTEXES, 0, true );

		UncacheData( pStudioData->m_VertexCache, MDLCACHE_VERTEXES, bIgnoreLock );
		pStudioData->m_VertexCache = NULL;
	}

	// Now check whatever files are not loaded, make sure file system knows
	// that we don't have them loaded.
	if ( !IsDataLoaded( handle, MDLCACHE_STUDIOHDR ) )
		NotifyFileUnloaded( handle, ".mdl" );

	if ( !IsDataLoaded( handle, MDLCACHE_STUDIOHWDATA ) )
		NotifyFileUnloaded( handle, GetVTXExtension() );

	if ( !IsDataLoaded( handle, MDLCACHE_VERTEXES ) )
		NotifyFileUnloaded( handle, ".vvd" );

	if ( !IsDataLoaded( handle, MDLCACHE_VCOLLIDE ) )
		NotifyFileUnloaded( handle, ".phy" );
}


//-----------------------------------------------------------------------------
// Inits, shuts downs studiodata_t
//-----------------------------------------------------------------------------
void CMDLCache::InitStudioData( MDLHandle_t handle )
{
	Assert( m_MDLDict[handle] == NULL );

	studiodata_t *pStudioData = new studiodata_t;
	m_MDLDict[handle] = pStudioData;
	memset( pStudioData, 0, sizeof( studiodata_t ) );
}

void CMDLCache::ShutdownStudioData( MDLHandle_t handle )
{
	Flush( handle );

	studiodata_t *pStudioData = m_MDLDict[handle];
	Assert( pStudioData != NULL );
	delete pStudioData;
	m_MDLDict[handle] = NULL;
}


//-----------------------------------------------------------------------------
// Sets the cache notify
//-----------------------------------------------------------------------------
void CMDLCache::SetCacheNotify( IMDLCacheNotify *pNotify )
{
	m_pCacheNotify = pNotify;
}


//-----------------------------------------------------------------------------
// Returns the name of the model
//-----------------------------------------------------------------------------
const char *CMDLCache::GetModelName( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return ERROR_MODEL;

	return m_MDLDict.GetElementName( handle );
}


//-----------------------------------------------------------------------------
// Returns the *actual* name of the model (could be an error model)
//-----------------------------------------------------------------------------
const char *CMDLCache::GetActualModelName( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return ERROR_MODEL;

	if ( m_MDLDict[handle]->m_nFlags & STUDIODATA_ERROR_MODEL )
		return ERROR_MODEL;

	return m_MDLDict.GetElementName( handle );
}


//-----------------------------------------------------------------------------
// Constructs a filename based on a model handle
//-----------------------------------------------------------------------------
void CMDLCache::MakeFilename( MDLHandle_t handle, const char *pszExtension, char *pszFileName, int nMaxLength )
{
	Q_strncpy( pszFileName, GetActualModelName( handle ), nMaxLength );
	Q_SetExtension( pszFileName, pszExtension, nMaxLength );
	Q_FixSlashes( pszFileName );
#ifdef _LINUX
	Q_strlower( pszFileName );
#endif
}

//-----------------------------------------------------------------------------
void CMDLCache::NotifyFileUnloaded( MDLHandle_t handle, const char *pszExtension )
{
	if ( handle == MDLHANDLE_INVALID )
		return;
	if ( !m_MDLDict.IsValidIndex( handle ) )
		return;

	char szFilename[MAX_PATH];
	V_strcpy_safe( szFilename, m_MDLDict.GetElementName( handle ) );
	V_SetExtension( szFilename, pszExtension, sizeof(szFilename) );
	V_FixSlashes( szFilename );
	g_pFullFileSystem->NotifyFileUnloaded( szFilename, "game" );
}


//-----------------------------------------------------------------------------
// Finds an MDL
//-----------------------------------------------------------------------------
MDLHandle_t CMDLCache::FindMDL( const char *pMDLRelativePath )
{
	// can't trust provided path
	// ensure provided path correctly resolves (Dictionary is case-insensitive)
	char szFixedName[MAX_PATH];
	V_strncpy( szFixedName, pMDLRelativePath, sizeof( szFixedName ) );
	V_RemoveDotSlashes( szFixedName, '/' );

	MDLHandle_t handle = m_MDLDict.Find( szFixedName );
	if ( handle == m_MDLDict.InvalidIndex() )
	{
		handle = m_MDLDict.Insert( szFixedName, NULL );
		InitStudioData( handle );
	}

	AddRef( handle );
	return handle;
}

//-----------------------------------------------------------------------------
// Reference counting
//-----------------------------------------------------------------------------
int CMDLCache::AddRef( MDLHandle_t handle )
{
	return ++m_MDLDict[handle]->m_nRefCount;
}

int CMDLCache::Release( MDLHandle_t handle )
{
	// Deal with shutdown order issues (i.e. datamodel shutting down after mdlcache)
	if ( !m_bInitialized )
		return 0;

	// NOTE: It can be null during shutdown because multiple studiomdls
	// could be referencing the same virtual model
	if ( !m_MDLDict[handle] )
		return 0;

	Assert( m_MDLDict[handle]->m_nRefCount > 0 );

	int nRefCount = --m_MDLDict[handle]->m_nRefCount;
	if ( nRefCount <= 0 )
	{
		ShutdownStudioData( handle );
		m_MDLDict.RemoveAt( handle );
	}

	return nRefCount;
}

int CMDLCache::GetRef( MDLHandle_t handle )
{
	if ( !m_bInitialized )
		return 0;

	if ( !m_MDLDict[handle] )
		return 0;

	return m_MDLDict[handle]->m_nRefCount;
}

//-----------------------------------------------------------------------------
// Unserializes the PHY file associated w/ models (the vphysics representation)
//-----------------------------------------------------------------------------
void CMDLCache::UnserializeVCollide( MDLHandle_t handle, bool synchronousLoad )
{
	VPROF( "CMDLCache::UnserializeVCollide" );

	// FIXME: Should the vcollde be played into cacheable memory?
	studiodata_t *pStudioData = m_MDLDict[handle];

	int iAsync = GetAsyncInfoIndex( handle, MDLCACHE_VCOLLIDE );

	if ( iAsync == NO_ASYNC )
	{
		// clear existing data
		pStudioData->m_nFlags &= ~STUDIODATA_FLAGS_VCOLLISION_LOADED;
		memset( &pStudioData->m_VCollisionData, 0, sizeof( pStudioData->m_VCollisionData ) );

#if 0
		// FIXME:  ywb
		// If we don't ask for the virtual model to load, then we can get a hitch later on after startup
		// Should we async load the sub .mdls during startup assuming they'll all be resident by the time the level can actually
		//  start drawing?
		if ( pStudioData->m_pVirtualModel || synchronousLoad )
#endif
		{
			virtualmodel_t *pVirtualModel = GetVirtualModel( handle );
			if ( pVirtualModel )
			{
				for ( int i = 1; i < pVirtualModel->m_group.Count(); i++ )
				{
					MDLHandle_t sharedHandle = (MDLHandle_t) (int)pVirtualModel->m_group[i].cache & 0xffff;
					studiodata_t *pData = m_MDLDict[sharedHandle];
					if ( !(pData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_LOADED) )
					{
						UnserializeVCollide( sharedHandle, synchronousLoad );
					}
					if ( pData->m_VCollisionData.solidCount > 0 )
					{
						pStudioData->m_VCollisionData = pData->m_VCollisionData;
						pStudioData->m_nFlags |= STUDIODATA_FLAGS_VCOLLISION_SHARED;
						return;
					}
				}
			}
		}

		char pFileName[MAX_PATH];
		MakeFilename( handle, ".phy", pFileName, sizeof(pFileName) );
		if ( IsX360() )
		{
			char pX360Filename[MAX_PATH];
			UpdateOrCreate( NULL, pFileName, pX360Filename, sizeof( pX360Filename ), "GAME" );
			Q_strncpy( pFileName, pX360Filename, sizeof(pX360Filename) );
		}

		bool bAsyncLoad = mod_load_vcollide_async.GetBool() && !synchronousLoad;

		MdlCacheMsg( "MDLCache: %s load vcollide %s\n", bAsyncLoad ? "Async" : "Sync", GetModelName( handle ) );

		AsyncInfo_t info;
		if ( IsDebug() )
		{
			memset( &info, 0xdd, sizeof( AsyncInfo_t ) );
		}
		info.hModel = handle;
		info.type = MDLCACHE_VCOLLIDE;
		info.iAnimBlock = 0;
		info.hControl = NULL;
		LoadData( pFileName, "GAME", bAsyncLoad, &info.hControl );
		{
			AUTO_LOCK( m_AsyncMutex );
			iAsync = SetAsyncInfoIndex( handle, MDLCACHE_VCOLLIDE, m_PendingAsyncs.AddToTail( info ) );
		}
	}
	else if ( synchronousLoad )
	{
		AsyncInfo_t *pInfo;
		{
			AUTO_LOCK( m_AsyncMutex );
			pInfo = &m_PendingAsyncs[iAsync];
		}
		if ( pInfo->hControl )
		{
			g_pFullFileSystem->AsyncFinish( pInfo->hControl, true );
		}
	}

	ProcessPendingAsync( iAsync );
}


//-----------------------------------------------------------------------------
// Free model's collision data
//-----------------------------------------------------------------------------
void CMDLCache::DestroyVCollide( MDLHandle_t handle )
{
	studiodata_t *pStudioData = m_MDLDict[handle];

	if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_SHARED )
		return;

	if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_LOADED )
	{
		pStudioData->m_nFlags &= ~STUDIODATA_FLAGS_VCOLLISION_LOADED;
		if ( pStudioData->m_VCollisionData.solidCount )
		{
			if ( m_pCacheNotify )
			{
				m_pCacheNotify->OnDataUnloaded( MDLCACHE_VCOLLIDE, handle );
			}

			MdlCacheMsg("MDLCache: Unload vcollide %s\n", GetModelName( handle ) );

			g_pPhysicsCollision->VCollideUnload( &pStudioData->m_VCollisionData );
		}
	}
}


//-----------------------------------------------------------------------------
// Unserializes the PHY file associated w/ models (the vphysics representation)
//-----------------------------------------------------------------------------
vcollide_t *CMDLCache::GetVCollideEx( MDLHandle_t handle, bool synchronousLoad /*= true*/ )
{
	if ( mod_test_not_available.GetBool() )
		return NULL;

	if ( handle == MDLHANDLE_INVALID )
		return NULL;

	studiodata_t *pStudioData = m_MDLDict[handle];

	if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_LOADED ) == 0 )
	{
		UnserializeVCollide( handle, synchronousLoad );
	}

	// We've loaded an empty collision file or no file was found, so return NULL
	if ( !pStudioData->m_VCollisionData.solidCount )
		return NULL;

	return &pStudioData->m_VCollisionData;
}


bool CMDLCache::GetVCollideSize( MDLHandle_t handle, int *pVCollideSize )
{
	*pVCollideSize = 0;

	studiodata_t *pStudioData = m_MDLDict[handle];
	if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_LOADED ) == 0 )
		return false;

	vcollide_t *pCollide = &pStudioData->m_VCollisionData;
	for ( int j = 0; j < pCollide->solidCount; j++ )
	{
		*pVCollideSize += g_pPhysicsCollision->CollideSize( pCollide->solids[j] );
	}
	*pVCollideSize += pCollide->descSize;
	return true;
}

//-----------------------------------------------------------------------------
// Allocates/frees the anim blocks
//-----------------------------------------------------------------------------
void CMDLCache::AllocateAnimBlocks( studiodata_t *pStudioData, int nCount )
{
	Assert( pStudioData->m_pAnimBlock == NULL );

	pStudioData->m_nAnimBlockCount = nCount;
	pStudioData->m_pAnimBlock = new DataCacheHandle_t[pStudioData->m_nAnimBlockCount];

	memset( pStudioData->m_pAnimBlock, 0, sizeof(DataCacheHandle_t) * pStudioData->m_nAnimBlockCount );

	pStudioData->m_iFakeAnimBlockStall = new unsigned long [pStudioData->m_nAnimBlockCount];
	memset( pStudioData->m_iFakeAnimBlockStall, 0, sizeof( unsigned long ) * pStudioData->m_nAnimBlockCount );
}

void CMDLCache::FreeAnimBlocks( MDLHandle_t handle )
{
	studiodata_t *pStudioData = m_MDLDict[handle];

	if ( pStudioData->m_pAnimBlock )
	{
		for (int i = 0; i < pStudioData->m_nAnimBlockCount; ++i )
		{
			MdlCacheMsg( "MDLCache: Free Anim block: %d\n", i );

			ClearAsync( handle, MDLCACHE_ANIMBLOCK, i, true );
			if ( pStudioData->m_pAnimBlock[i] )
			{
				UncacheData( pStudioData->m_pAnimBlock[i], MDLCACHE_ANIMBLOCK, true );
			}
		}

		delete[] pStudioData->m_pAnimBlock;
		pStudioData->m_pAnimBlock = NULL;

		delete[] pStudioData->m_iFakeAnimBlockStall;
		pStudioData->m_iFakeAnimBlockStall = NULL;
	}

	pStudioData->m_nAnimBlockCount = 0;
}


//-----------------------------------------------------------------------------
// Unserializes an animation block from disk
//-----------------------------------------------------------------------------
unsigned char *CMDLCache::UnserializeAnimBlock( MDLHandle_t handle, int nBlock )
{
	VPROF( "CMDLCache::UnserializeAnimBlock" );

	if ( IsX360() && g_pQueuedLoader->IsMapLoading() )
	{
		// anim block i/o is not allowed at this stage
		return NULL;
	}

	// Block 0 is never used!!!
	Assert( nBlock > 0 );

	studiodata_t *pStudioData = m_MDLDict[handle];

	int iAsync = GetAsyncInfoIndex( handle, MDLCACHE_ANIMBLOCK, nBlock );

	if ( iAsync == NO_ASYNC )
	{
		studiohdr_t *pStudioHdr = GetStudioHdr( handle );

		// FIXME: For consistency, the block name maybe shouldn't have 'model' in it.
		char const *pModelName = pStudioHdr->pszAnimBlockName();
		mstudioanimblock_t *pBlock = pStudioHdr->pAnimBlock( nBlock );
		int nSize = pBlock->dataend - pBlock->datastart;
		if ( nSize == 0 )
			return NULL;

		// allocate space in the cache
		pStudioData->m_pAnimBlock[nBlock] = NULL;

		char pFileName[MAX_PATH];
		Q_strncpy( pFileName, pModelName, sizeof(pFileName) );
		Q_FixSlashes( pFileName );
#ifdef _LINUX
		Q_strlower( pFileName );
#endif
		if ( IsX360() )
		{
			char pX360Filename[MAX_PATH];
			UpdateOrCreate( pStudioHdr, pFileName, pX360Filename, sizeof( pX360Filename ), "GAME" );
			Q_strncpy( pFileName, pX360Filename, sizeof(pX360Filename) );
		}

		MdlCacheMsg( "MDLCache: Begin load Anim Block %s (block %i)\n", GetModelName( handle ), nBlock );

		AsyncInfo_t info;
		if ( IsDebug() )
		{
			memset( &info, 0xdd, sizeof( AsyncInfo_t ) );
		}
		info.hModel = handle;
		info.type = MDLCACHE_ANIMBLOCK;
		info.iAnimBlock = nBlock;
		info.hControl = NULL;
		LoadData( pFileName, "GAME", NULL, nSize, pBlock->datastart, mod_load_anims_async.GetBool(), &info.hControl );
		{
			AUTO_LOCK( m_AsyncMutex );
			iAsync = SetAsyncInfoIndex( handle, MDLCACHE_ANIMBLOCK, nBlock, m_PendingAsyncs.AddToTail( info ) );
		}
	}

	ProcessPendingAsync( iAsync );

	return ( unsigned char * )CheckData( pStudioData->m_pAnimBlock[nBlock], MDLCACHE_ANIMBLOCK );
}

//-----------------------------------------------------------------------------
// Gets at an animation block associated with an MDL
//-----------------------------------------------------------------------------
unsigned char *CMDLCache::GetAnimBlock( MDLHandle_t handle, int nBlock )
{
	if ( mod_test_not_available.GetBool() )
		return NULL;

	if ( handle == MDLHANDLE_INVALID )
		return NULL;

	// Allocate animation blocks if we don't have them yet
	studiodata_t *pStudioData = m_MDLDict[handle];
	if ( pStudioData->m_pAnimBlock == NULL )
	{
		studiohdr_t *pStudioHdr = GetStudioHdr( handle );
		AllocateAnimBlocks( pStudioData, pStudioHdr->numanimblocks );
	}

	// check for request being in range
	if ( nBlock < 0 || nBlock >= pStudioData->m_nAnimBlockCount)
		return NULL;

	// Check the cache to see if the animation is in memory
	unsigned char *pData = ( unsigned char * )CheckData( pStudioData->m_pAnimBlock[nBlock], MDLCACHE_ANIMBLOCK );
	if ( !pData )
	{
		pStudioData->m_pAnimBlock[nBlock] = NULL;

		// It's not in memory, read it off of disk
		pData = UnserializeAnimBlock( handle, nBlock );
	}

	if (mod_load_fakestall.GetInt())
	{
		unsigned int t = Plat_MSTime();
		if (pStudioData->m_iFakeAnimBlockStall[nBlock] == 0 || pStudioData->m_iFakeAnimBlockStall[nBlock] > t)
		{
			pStudioData->m_iFakeAnimBlockStall[nBlock] = t;
		}

		if ((int)(t - pStudioData->m_iFakeAnimBlockStall[nBlock]) < mod_load_fakestall.GetInt())
		{
			return NULL;
		}
	}
	return pData;
}


//-----------------------------------------------------------------------------
// Allocates/frees autoplay sequence list
//-----------------------------------------------------------------------------
void CMDLCache::AllocateAutoplaySequences( studiodata_t *pStudioData, int nCount )
{
	FreeAutoplaySequences( pStudioData );

	pStudioData->m_nAutoplaySequenceCount = nCount;
	pStudioData->m_pAutoplaySequenceList = new unsigned short[nCount];
}

void CMDLCache::FreeAutoplaySequences( studiodata_t *pStudioData )
{
	if ( pStudioData->m_pAutoplaySequenceList )
	{
		delete[] pStudioData->m_pAutoplaySequenceList;
		pStudioData->m_pAutoplaySequenceList = NULL;
	}

	pStudioData->m_nAutoplaySequenceCount = 0;
}


//-----------------------------------------------------------------------------
// Gets the autoplay list
//-----------------------------------------------------------------------------
int CMDLCache::GetAutoplayList( MDLHandle_t handle, unsigned short **pAutoplayList )
{
	if ( pAutoplayList )
	{
		*pAutoplayList = NULL;
	}

	if ( handle == MDLHANDLE_INVALID )
		return 0;

	virtualmodel_t *pVirtualModel = GetVirtualModel( handle );
	if ( pVirtualModel )
	{
		if ( pAutoplayList && pVirtualModel->m_autoplaySequences.Count() )
		{
			*pAutoplayList = pVirtualModel->m_autoplaySequences.Base();
		}
		return pVirtualModel->m_autoplaySequences.Count();
	}

	// FIXME: Should we cache autoplay info here on demand instead of in unserializeMDL?
	studiodata_t *pStudioData = m_MDLDict[handle];
	if ( pAutoplayList )
	{
		*pAutoplayList = pStudioData->m_pAutoplaySequenceList;
	}

	return pStudioData->m_nAutoplaySequenceCount;
}


//-----------------------------------------------------------------------------
// Allocates/frees the virtual model
//-----------------------------------------------------------------------------
void CMDLCache::AllocateVirtualModel( MDLHandle_t handle )
{
	studiodata_t *pStudioData = m_MDLDict[handle];
	Assert( pStudioData->m_pVirtualModel == NULL );
	pStudioData->m_pVirtualModel = new virtualmodel_t;

	// FIXME: The old code slammed these; could have leaked memory?
	Assert( pStudioData->m_nAnimBlockCount == 0 );
	Assert( pStudioData->m_pAnimBlock == NULL );
}

void CMDLCache::FreeVirtualModel( MDLHandle_t handle )
{
	studiodata_t *pStudioData = m_MDLDict[handle];
	if ( pStudioData && pStudioData->m_pVirtualModel )
	{
		int nGroupCount = pStudioData->m_pVirtualModel->m_group.Count();
		Assert( (nGroupCount >= 1) && pStudioData->m_pVirtualModel->m_group[0].cache == (void*)(uintp)handle );

		// NOTE: Start at *1* here because the 0th element contains a reference to *this* handle
		for ( int i = 1; i < nGroupCount; ++i )
		{
			MDLHandle_t h = (MDLHandle_t)(int)pStudioData->m_pVirtualModel->m_group[i].cache&0xffff;
			FreeVirtualModel( h );
			Release( h );
		}

		delete pStudioData->m_pVirtualModel;
		pStudioData->m_pVirtualModel = NULL;
	}
}


//-----------------------------------------------------------------------------
// Returns the virtual model
//-----------------------------------------------------------------------------
virtualmodel_t *CMDLCache::GetVirtualModel( MDLHandle_t handle )
{
	if ( mod_test_not_available.GetBool() )
		return NULL;

	if ( handle == MDLHANDLE_INVALID )
		return NULL;

	studiohdr_t *pStudioHdr = GetStudioHdr( handle );

	if ( pStudioHdr == NULL )
		return NULL;

	return GetVirtualModelFast( pStudioHdr, handle );
}

virtualmodel_t *CMDLCache::GetVirtualModelFast( const studiohdr_t *pStudioHdr, MDLHandle_t handle )
{
	if (pStudioHdr->numincludemodels == 0)
		return NULL;

	studiodata_t *pStudioData = m_MDLDict[handle];
	if ( !pStudioData )
		return NULL;
	
	if ( !pStudioData->m_pVirtualModel )
	{
		DevMsg( 2, "Loading virtual model for %s\n", pStudioHdr->pszName() );

		CMDLCacheCriticalSection criticalSection( this );

		AllocateVirtualModel( handle );

		// Group has to be zero to ensure refcounting is correct
		int nGroup = pStudioData->m_pVirtualModel->m_group.AddToTail( );
		Assert( nGroup == 0 );
		pStudioData->m_pVirtualModel->m_group[nGroup].cache = (void *)(uintp)handle;

		// Add all dependent data
		pStudioData->m_pVirtualModel->AppendModels( 0, pStudioHdr );
	}

	return pStudioData->m_pVirtualModel;
}

//-----------------------------------------------------------------------------
// Purpose: Pulls all submodels/.ani file models into the cache
// to avoid runtime hitches and load animations at load time, set mod_forcedata to be 1
//-----------------------------------------------------------------------------
void CMDLCache::UnserializeAllVirtualModelsAndAnimBlocks( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return;

	// might be re-loading, discard old virtualmodel to force rebuild
	// unfortunately, the virtualmodel does build data into the cacheable studiohdr
	FreeVirtualModel( handle );

	if ( IsX360() && g_pQueuedLoader->IsMapLoading() )
	{
		// queued loading has to do it
		return;
	}

	// don't load the submodel data
	if ( !mod_forcedata.GetBool() )
		return;

	// if not present, will instance and load the submodels
	GetVirtualModel( handle );

	if ( IsX360() )
	{
		// 360 does not drive the anims into its small cache section
		return;
	}

	// Note that the animblocks start at 1!!!
	studiohdr_t *pStudioHdr = GetStudioHdr( handle );
	for ( int i = 1 ; i < (int)pStudioHdr->numanimblocks; ++i )
	{
		GetAnimBlock( handle, i );
	}

	ProcessPendingAsyncs( MDLCACHE_ANIMBLOCK );
}


//-----------------------------------------------------------------------------
// Loads the static meshes
//-----------------------------------------------------------------------------
bool CMDLCache::LoadHardwareData( MDLHandle_t handle )
{
	Assert( handle != MDLHANDLE_INVALID );

	// Don't try to load VTX files if we don't have focus...
	if ( m_bLostVideoMemory )
		return false;

	studiodata_t *pStudioData = m_MDLDict[handle];

	CMDLCacheCriticalSection criticalSection( this );

	// Load up the model
	studiohdr_t *pStudioHdr = GetStudioHdr( handle );
	if ( !pStudioHdr || !pStudioHdr->numbodyparts )
	{
		pStudioData->m_nFlags |= STUDIODATA_FLAGS_NO_STUDIOMESH;
		return true;
	}

	if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_NO_STUDIOMESH )
	{
		return false;
	}

	if ( LogMdlCache() &&
		 GetAsyncInfoIndex( handle, MDLCACHE_STUDIOHWDATA ) == NO_ASYNC &&
		 GetAsyncInfoIndex( handle, MDLCACHE_VERTEXES ) == NO_ASYNC )
	{
		MdlCacheMsg( "MDLCache: Begin load studiomdl %s\n", GetModelName( handle ) );
	}

	// Vertex data is required to call LoadModel(), so make sure that's ready
	if ( !GetVertexData( handle ) )
	{
		if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_NO_VERTEX_DATA )
		{
			pStudioData->m_nFlags |= STUDIODATA_FLAGS_NO_STUDIOMESH;
		}
		return false;
	}

	int iAsync = GetAsyncInfoIndex( handle, MDLCACHE_STUDIOHWDATA );

	if ( iAsync == NO_ASYNC )
	{
		m_pMeshCacheSection->Lock( pStudioData->m_VertexCache );

		// load and persist the vtx file
		// use model name for correct path
		char pFileName[MAX_PATH];
		MakeFilename( handle, GetVTXExtension(), pFileName, sizeof(pFileName) );
		if ( IsX360() )
		{
			char pX360Filename[MAX_PATH];
			UpdateOrCreate( pStudioHdr, pFileName, pX360Filename, sizeof( pX360Filename ), "GAME" );
			Q_strncpy( pFileName, pX360Filename, sizeof(pX360Filename) );
		}

		MdlCacheMsg("MDLCache: Begin load VTX %s\n", GetModelName( handle ) );

		AsyncInfo_t info;
		if ( IsDebug() )
		{
			memset( &info, 0xdd, sizeof( AsyncInfo_t ) );
		}
		info.hModel = handle;
		info.type = MDLCACHE_STUDIOHWDATA;
		info.iAnimBlock = 0;
		info.hControl = NULL;
		LoadData( pFileName, "GAME", mod_load_mesh_async.GetBool(), &info.hControl );
		{
			AUTO_LOCK( m_AsyncMutex );
			iAsync = SetAsyncInfoIndex( handle, MDLCACHE_STUDIOHWDATA, m_PendingAsyncs.AddToTail( info ) );
		}
	}

	if ( ProcessPendingAsync( iAsync ) > 0 )
	{
		if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_NO_STUDIOMESH )
		{
			return false;
		}

		return ( pStudioData->m_HardwareData.m_NumStudioMeshes != 0 );
	}

	return false;
}

void CMDLCache::ConvertFlexData( studiohdr_t *pStudioHdr )
{
	float flVertAnimFixedPointScale = pStudioHdr->VertAnimFixedPointScale();

	for ( int i = 0; i < pStudioHdr->numbodyparts; i++ )
	{
		mstudiobodyparts_t *pBody = pStudioHdr->pBodypart( i );
		for ( int j = 0; j < pBody->nummodels; j++ )
		{
			mstudiomodel_t *pModel = pBody->pModel( j );
			for ( int k = 0; k < pModel->nummeshes; k++ )
			{
				mstudiomesh_t *pMesh = pModel->pMesh( k );
				for ( int l = 0; l < pMesh->numflexes; l++ )
				{
					mstudioflex_t *pFlex = pMesh->pFlex( l );
					bool bIsWrinkleAnim = ( pFlex->vertanimtype == STUDIO_VERT_ANIM_WRINKLE );
					for ( int m = 0; m < pFlex->numverts; m++ )
					{
						mstudiovertanim_t *pVAnim = bIsWrinkleAnim ?
							pFlex->pVertanimWrinkle( m ) : pFlex->pVertanim( m );
						pVAnim->ConvertToFixed( flVertAnimFixedPointScale );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMDLCache::BuildHardwareData( MDLHandle_t handle, studiodata_t *pStudioData, studiohdr_t *pStudioHdr, OptimizedModel::FileHeader_t *pVtxHdr )
{
	if ( pVtxHdr )
	{
		MdlCacheMsg("MDLCache: Alloc VTX %s\n", pStudioHdr->pszName() );

		// check header
		if ( pVtxHdr->version != OPTIMIZED_MODEL_FILE_VERSION )
		{
			Warning( "Error Index File for '%s' version %d should be %d\n", pStudioHdr->pszName(), pVtxHdr->version, OPTIMIZED_MODEL_FILE_VERSION );
			pVtxHdr = NULL;
		}
		else if ( pVtxHdr->checkSum != pStudioHdr->checksum )
		{
			Warning( "Error Index File for '%s' checksum %d should be %d\n", pStudioHdr->pszName(), pVtxHdr->checkSum, pStudioHdr->checksum );
			pVtxHdr = NULL;
		}
	}

	if ( !pVtxHdr )
	{
		pStudioData->m_nFlags |= STUDIODATA_FLAGS_NO_STUDIOMESH;
		return false;
	}

	CTempAllocHelper pOriginalData;
	if ( IsX360() )
	{
		unsigned char *pInputData = (unsigned char *)pVtxHdr + sizeof( OptimizedModel::FileHeader_t );
		if ( CLZMA::IsCompressed( pInputData ) )
		{
			// vtx arrives compressed, decode and cache the results
			unsigned int nOriginalSize = CLZMA::GetActualSize( pInputData );
			pOriginalData.Alloc( sizeof( OptimizedModel::FileHeader_t ) + nOriginalSize );
			V_memcpy( pOriginalData.Get(), pVtxHdr, sizeof( OptimizedModel::FileHeader_t ) );
			unsigned int nOutputSize = CLZMA::Uncompress( pInputData, sizeof( OptimizedModel::FileHeader_t ) + (unsigned char *)pOriginalData.Get() );
			if ( nOutputSize != nOriginalSize )
			{
				// decoder failure
				return false;
			}

			pVtxHdr = (OptimizedModel::FileHeader_t *)pOriginalData.Get();
		}
	}

	MdlCacheMsg( "MDLCache: Load studiomdl %s\n", pStudioHdr->pszName() );

	Assert( GetVertexData( handle ) );

	BeginLock();
	bool bLoaded = g_pStudioRender->LoadModel( pStudioHdr, pVtxHdr, &pStudioData->m_HardwareData );
	EndLock();

	if ( bLoaded )
	{
		pStudioData->m_nFlags |= STUDIODATA_FLAGS_STUDIOMESH_LOADED;
	}
	else
	{
		pStudioData->m_nFlags |= STUDIODATA_FLAGS_NO_STUDIOMESH;
	}

	if ( m_pCacheNotify )
	{
		m_pCacheNotify->OnDataLoaded( MDLCACHE_STUDIOHWDATA, handle );
	}

#if defined( USE_HARDWARE_CACHE )
	GetCacheSection( MDLCACHE_STUDIOHWDATA )->Add( MakeCacheID( handle, MDLCACHE_STUDIOHWDATA ), &pStudioData->m_HardwareData, ComputeHardwareDataSize( &pStudioData->m_HardwareData ), &pStudioData->m_HardwareDataCache );
#endif
	return true;
}


//-----------------------------------------------------------------------------
// Loads the static meshes
//-----------------------------------------------------------------------------
void CMDLCache::UnloadHardwareData( MDLHandle_t handle, bool bCacheRemove, bool bLockedOk )
{
	if ( handle == MDLHANDLE_INVALID )
		return;

	// Don't load it if it's loaded
	studiodata_t *pStudioData = m_MDLDict[handle];
	if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_STUDIOMESH_LOADED )
	{
#if defined( USE_HARDWARE_CACHE )
		if ( bCacheRemove )
		{
			if ( GetCacheSection( MDLCACHE_STUDIOHWDATA )->BreakLock( pStudioData->m_HardwareDataCache ) && !bLockedOk )
			{
				DevMsg( "Warning: freed a locked resource\n" );
				Assert( 0 );
			}

			GetCacheSection( MDLCACHE_STUDIOHWDATA )->Remove( pStudioData->m_HardwareDataCache );
		}
#endif

		if ( m_pCacheNotify )
		{
			m_pCacheNotify->OnDataUnloaded( MDLCACHE_STUDIOHWDATA, handle );
		}

		MdlCacheMsg("MDLCache: Unload studiomdl %s\n", GetModelName( handle ) );

		g_pStudioRender->UnloadModel( &pStudioData->m_HardwareData );
		memset( &pStudioData->m_HardwareData, 0, sizeof( pStudioData->m_HardwareData ) );
		pStudioData->m_nFlags &= ~STUDIODATA_FLAGS_STUDIOMESH_LOADED;

		NotifyFileUnloaded( handle, ".mdl" );

	}
}


//-----------------------------------------------------------------------------
// Returns the hardware data associated with an MDL
//-----------------------------------------------------------------------------
studiohwdata_t *CMDLCache::GetHardwareData( MDLHandle_t handle )
{
	if ( mod_test_not_available.GetBool() )
		return NULL;

	if ( mod_test_mesh_not_available.GetBool() )
		return NULL;

	studiodata_t *pStudioData = m_MDLDict[handle];
	m_pMeshCacheSection->LockMutex();
	if ( ( pStudioData->m_nFlags & (STUDIODATA_FLAGS_STUDIOMESH_LOADED | STUDIODATA_FLAGS_NO_STUDIOMESH) ) == 0 )
	{
		m_pMeshCacheSection->UnlockMutex();
		if ( !LoadHardwareData( handle ) )
		{
			return NULL;
		}
	}
	else
	{
#if defined( USE_HARDWARE_CACHE )
		CheckData( pStudioData->m_HardwareDataCache, MDLCACHE_STUDIOHWDATA );
#endif
		m_pMeshCacheSection->UnlockMutex();
	}

	// didn't load, don't return an empty pointer
	if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_NO_STUDIOMESH )
		return NULL;

	return &pStudioData->m_HardwareData;
}


//-----------------------------------------------------------------------------
// Task switch
//-----------------------------------------------------------------------------
void CMDLCache::ReleaseMaterialSystemObjects()
{
	Assert( !m_bLostVideoMemory );
	m_bLostVideoMemory = true;

	BreakFrameLock( false );

	// Free all hardware data
	MDLHandle_t i = m_MDLDict.First();
	while ( i != m_MDLDict.InvalidIndex() )
	{
		UnloadHardwareData( i );
		i = m_MDLDict.Next( i );
	}

	RestoreFrameLock();
}

void CMDLCache::RestoreMaterialSystemObjects( int nChangeFlags )
{
	Assert( m_bLostVideoMemory );
	m_bLostVideoMemory = false;

	BreakFrameLock( false );

	// Restore all hardware data
	MDLHandle_t i = m_MDLDict.First();
	while ( i != m_MDLDict.InvalidIndex() )
	{
		studiodata_t *pStudioData = m_MDLDict[i];

		bool bIsMDLInMemory = GetCacheSection( MDLCACHE_STUDIOHDR )->IsPresent( pStudioData->m_MDLCache );

		// If the vertex format changed, we have to free the data because we may be using different .vtx files.
		if ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED )
		{
			MdlCacheMsg( "MDLCache: Free studiohdr\n" );
			MdlCacheMsg( "MDLCache: Free VVD\n" );
			MdlCacheMsg( "MDLCache: Free VTX\n" );

			// FIXME: Do we have to free m_MDLCache + m_VertexCache?
			// Certainly we have to free m_IndexCache, cause that's a dx-level specific vtx file.
			ClearAsync( i, MDLCACHE_STUDIOHWDATA, 0, true );

			Flush( i, MDLCACHE_FLUSH_VERTEXES );
		}

		// Only restore the hardware data of those studiohdrs which are currently in memory
		if ( bIsMDLInMemory )
		{
			GetHardwareData( i );
		}

		i = m_MDLDict.Next( i );
	}

	RestoreFrameLock();
}


void CMDLCache::MarkAsLoaded(MDLHandle_t handle)
{
	if ( mod_lock_mdls_on_load.GetBool() )
	{
		g_MDLCache.GetStudioHdr(handle);
		if ( !( m_MDLDict[handle]->m_nFlags & STUDIODATA_FLAGS_LOCKED_MDL ) )
		{
			m_MDLDict[handle]->m_nFlags |= STUDIODATA_FLAGS_LOCKED_MDL;
			GetCacheSection( MDLCACHE_STUDIOHDR )->Lock( m_MDLDict[handle]->m_MDLCache );
		}
	}
}


//-----------------------------------------------------------------------------
// Callback for UpdateOrCreate utility function - swaps any studiomdl file type.
//-----------------------------------------------------------------------------
static bool MdlcacheCreateCallback( const char *pSourceName, const char *pTargetName, const char *pPathID, void *pHdr )
{
	// Missing studio files are permissible and not spewed as errors
	bool retval = false;
	CUtlBuffer sourceBuf;
	bool bOk = g_pFullFileSystem->ReadFile( pSourceName, NULL, sourceBuf );
	if ( bOk )
	{
		CUtlBuffer targetBuf;
		targetBuf.EnsureCapacity( sourceBuf.TellPut() + BYTESWAP_ALIGNMENT_PADDING );

		int bytes = StudioByteSwap::ByteswapStudioFile( pTargetName, targetBuf.Base(), sourceBuf.Base(), sourceBuf.TellPut(), (studiohdr_t*)pHdr );
		if ( bytes )
		{
			// If the file was an .mdl, attempt to swap the .ani as well
			if ( Q_stristr( pSourceName, ".mdl" ) )
			{
				char szANISourceName[ MAX_PATH ];
				Q_StripExtension( pSourceName, szANISourceName, sizeof( szANISourceName ) );
				Q_strncat( szANISourceName, ".ani", sizeof( szANISourceName ), COPY_ALL_CHARACTERS );
				UpdateOrCreate( szANISourceName, NULL, 0, pPathID, MdlcacheCreateCallback, true, targetBuf.Base() );
			}

			targetBuf.SeekPut( CUtlBuffer::SEEK_HEAD, bytes );
			g_pFullFileSystem->WriteFile( pTargetName, pPathID, targetBuf );
			retval = true;
		}
		else
		{
			Warning( "Failed to create %s\n", pTargetName );
		}
	}
	return retval;
}

//-----------------------------------------------------------------------------
// Calls utility function to create .360 version of a file.
//-----------------------------------------------------------------------------
int CMDLCache::UpdateOrCreate( studiohdr_t *pHdr, const char *pSourceName, char *pTargetName, int targetLen, const char *pPathID, bool bForce )
{
	return ::UpdateOrCreate( pSourceName, pTargetName, targetLen, pPathID, MdlcacheCreateCallback, bForce, pHdr );
}

//-----------------------------------------------------------------------------
// Purpose: Attempts to read a file native to the current platform
//-----------------------------------------------------------------------------
bool CMDLCache::ReadFileNative( char *pFileName, const char *pPath, CUtlBuffer &buf, int nMaxBytes )
{
	bool bOk = false;

	if ( IsX360() )
	{
		// Read the 360 version
		char pX360Filename[ MAX_PATH ];
		UpdateOrCreate( NULL, pFileName, pX360Filename, sizeof( pX360Filename ), pPath );
		bOk = g_pFullFileSystem->ReadFile( pX360Filename, pPath, buf, nMaxBytes );
	}
	else
	{
		// Read the PC version
		bOk = g_pFullFileSystem->ReadFile( pFileName, pPath, buf, nMaxBytes );
	}

	return bOk;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
studiohdr_t *CMDLCache::UnserializeMDL( MDLHandle_t handle, void *pData, int nDataSize, bool bDataValid )
{
	if ( !bDataValid || nDataSize <= 0 || pData == NULL)
	{
		return NULL;
	}

	CTempAllocHelper pOriginalData;
	if ( IsX360() )
	{
		if ( CLZMA::IsCompressed( (unsigned char *)pData ) )
		{
			// mdl arrives compressed, decode and cache the results
			unsigned int nOriginalSize = CLZMA::GetActualSize( (unsigned char *)pData );
			pOriginalData.Alloc( nOriginalSize );
			unsigned int nOutputSize = CLZMA::Uncompress( (unsigned char *)pData, (unsigned char *)pOriginalData.Get() );
			if ( nOutputSize != nOriginalSize )
			{
				// decoder failure
				return NULL;
			}

			pData = pOriginalData.Get();
			nDataSize = nOriginalSize;
		}
	}

	studiohdr_t	*pStudioHdrIn = (studiohdr_t *)pData;

	if ( r_rootlod.GetInt() > 0 )
	{
		// raw data is already setup for lod 0, override otherwise
		Studio_SetRootLOD( pStudioHdrIn, r_rootlod.GetInt() );
	}

	// critical! store a back link to our data
	// this is fetched when re-establishing dependent cached data (vtx/vvd)
	pStudioHdrIn->virtualModel = (void *)(uintp)handle;

	MdlCacheMsg( "MDLCache: Alloc studiohdr %s\n", GetModelName( handle ) );

	// allocate cache space
	MemAlloc_PushAllocDbgInfo( "Models:StudioHdr", 0);
	studiohdr_t *pHdr = (studiohdr_t *)AllocData( MDLCACHE_STUDIOHDR, pStudioHdrIn->length );
	MemAlloc_PopAllocDbgInfo();
	if ( !pHdr )
		return NULL;

	CacheData( &m_MDLDict[handle]->m_MDLCache, pHdr, pStudioHdrIn->length, GetModelName( handle ), MDLCACHE_STUDIOHDR, MakeCacheID( handle, MDLCACHE_STUDIOHDR) );

	if ( mod_lock_mdls_on_load.GetBool() )
	{
		GetCacheSection( MDLCACHE_STUDIOHDR )->Lock( m_MDLDict[handle]->m_MDLCache );
		m_MDLDict[handle]->m_nFlags |= STUDIODATA_FLAGS_LOCKED_MDL;
	}

	// FIXME: Is there any way we can compute the size to load *before* loading in
	// and read directly into cache memory? It would be nice to reduce cache overhead here.
	// move the complete, relocatable model to the cache
	memcpy( pHdr, pStudioHdrIn, pStudioHdrIn->length );

	// On first load, convert the flex deltas from fp16 to 16-bit fixed-point
	if ( (pHdr->flags & STUDIOHDR_FLAGS_FLEXES_CONVERTED) == 0 )
	{
		ConvertFlexData( pHdr );

		// Mark as converted so it only happens once
		pHdr->flags |= STUDIOHDR_FLAGS_FLEXES_CONVERTED;
	}

	if ( m_pCacheNotify )
	{
		m_pCacheNotify->OnDataLoaded( MDLCACHE_STUDIOHDR, handle );
	}

	return pHdr;
}


//-----------------------------------------------------------------------------
// Attempts to load a MDL file, validates that it's ok.
//-----------------------------------------------------------------------------
bool CMDLCache::ReadMDLFile( MDLHandle_t handle, const char *pMDLFileName, CUtlBuffer &buf )
{
	VPROF( "CMDLCache::ReadMDLFile" );

	char pFileName[ MAX_PATH ];
	Q_strncpy( pFileName, pMDLFileName, sizeof( pFileName ) );
	Q_FixSlashes( pFileName );
#ifdef _LINUX
	Q_strlower( pFileName );
#endif

	MdlCacheMsg( "MDLCache: Load studiohdr %s\n", pFileName );

	MEM_ALLOC_CREDIT();

	bool bOk = ReadFileNative( pFileName, "GAME", buf );
	if ( !bOk )
	{
		DevWarning( "Failed to load %s!\n", pMDLFileName );
		return false;
	}

	if ( IsX360() )
	{
		if ( CLZMA::IsCompressed( (unsigned char *)buf.PeekGet() ) )
		{
			// mdl arrives compressed, decode and cache the results
			unsigned int nOriginalSize = CLZMA::GetActualSize( (unsigned char *)buf.PeekGet() );
			void *pOriginalData = malloc( nOriginalSize );
			unsigned int nOutputSize = CLZMA::Uncompress( (unsigned char *)buf.PeekGet(), (unsigned char *)pOriginalData );
			if ( nOutputSize != nOriginalSize )
			{
				// decoder failure
				free( pOriginalData );
				return false;
			}

			// replace caller's buffer
			buf.Purge();
			buf.Put( pOriginalData, nOriginalSize );
			free( pOriginalData );
		}
	}

	studiohdr_t *pStudioHdr = (studiohdr_t*)buf.PeekGet();
	if ( !pStudioHdr )
	{
		DevWarning( "Failed to read model %s from buffer!\n", pMDLFileName );
		return false;
	}
	if ( pStudioHdr->id != IDSTUDIOHEADER )
	{
		DevWarning( "Model %s not a .MDL format file!\n", pMDLFileName );
		return false;
	}

	// critical! store a back link to our data
	// this is fetched when re-establishing dependent cached data (vtx/vvd)
	pStudioHdr->virtualModel = (void*)(uintp)handle;

	// Make sure all dependent files are valid
	if ( !VerifyHeaders( pStudioHdr ) )
	{
		DevWarning( "Model %s has mismatched .vvd + .vtx files!\n", pMDLFileName );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
studiohdr_t *CMDLCache::LockStudioHdr( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
	{
		return NULL;
	}

	CMDLCacheCriticalSection cacheCriticalSection( this );
	studiohdr_t *pStdioHdr = GetStudioHdr( handle );
	// @TODO (toml 9/12/2006) need this?: AddRef( handle );
	if ( !pStdioHdr )
	{
		return NULL;
	}

	GetCacheSection( MDLCACHE_STUDIOHDR )->Lock( m_MDLDict[handle]->m_MDLCache );
	return pStdioHdr;
}

void CMDLCache::UnlockStudioHdr( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
	{
		return;
	}

	CMDLCacheCriticalSection cacheCriticalSection( this );
	studiohdr_t *pStdioHdr = GetStudioHdr( handle );
	if ( pStdioHdr )
	{
		GetCacheSection( MDLCACHE_STUDIOHDR )->Unlock( m_MDLDict[handle]->m_MDLCache );
	}
	// @TODO (toml 9/12/2006) need this?: Release( handle );
}

//-----------------------------------------------------------------------------
// Loading the data in
//-----------------------------------------------------------------------------
studiohdr_t *CMDLCache::GetStudioHdr( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return NULL;

	// Returning a pointer to data inside the cache when it's unlocked is just a bad idea.
	// It's technically legal, but the pointer can get invalidated if anything else looks at the cache.
	// Don't do that.
	// Assert( m_pModelCacheSection->IsFrameLocking() );
	// Assert( m_pMeshCacheSection->IsFrameLocking() );

#if _DEBUG
	VPROF_INCREMENT_COUNTER( "GetStudioHdr", 1 );
#endif
	studiohdr_t *pHdr = (studiohdr_t*)CheckData( m_MDLDict[handle]->m_MDLCache, MDLCACHE_STUDIOHDR );
	if ( !pHdr )
	{
		m_MDLDict[handle]->m_MDLCache = NULL;

		CMDLCacheCriticalSection cacheCriticalSection( this );

		// load the file
		const char *pModelName = GetActualModelName( handle );
		if ( developer.GetInt() > 1 )
		{
			DevMsg( "Loading %s\n", pModelName );
		}

		// Load file to temporary space
		CUtlBuffer buf;
		if ( !ReadMDLFile( handle, pModelName, buf ) )
		{
			bool bOk = false;
			if ( ( m_MDLDict[handle]->m_nFlags & STUDIODATA_ERROR_MODEL ) == 0 )
			{
				buf.Clear(); // clear buffer for next file read

				m_MDLDict[handle]->m_nFlags |= STUDIODATA_ERROR_MODEL;
				bOk = ReadMDLFile( handle, ERROR_MODEL, buf );
			}

			if ( !bOk )
			{
				if (IsOSX())
				{
					// rbarris wants this to go somewhere like the console.log prior to crashing, which is what the Error call will do next
					printf("\n ##### Model %s not found and %s couldn't be loaded", pModelName, ERROR_MODEL );
					fflush( stdout );
				}
				Error( "Model %s not found and %s couldn't be loaded", pModelName, ERROR_MODEL );
				return NULL;
			}
		}

		// put it in the cache
		if ( ProcessDataIntoCache( handle, MDLCACHE_STUDIOHDR, 0, buf.Base(), buf.TellMaxPut(), true ) )
		{
			pHdr = (studiohdr_t*)CheckData( m_MDLDict[handle]->m_MDLCache, MDLCACHE_STUDIOHDR );
		}
	}

	return pHdr;
}


//-----------------------------------------------------------------------------
// Gets/sets user data associated with the MDL
//-----------------------------------------------------------------------------
void CMDLCache::SetUserData( MDLHandle_t handle, void* pData )
{
	if ( handle == MDLHANDLE_INVALID )
		return;

	m_MDLDict[handle]->m_pUserData = pData;
}

void *CMDLCache::GetUserData( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return NULL;
	return m_MDLDict[handle]->m_pUserData;
}


//-----------------------------------------------------------------------------
// Polls information about a particular mdl
//-----------------------------------------------------------------------------
bool CMDLCache::IsErrorModel( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return false;

	return (m_MDLDict[handle]->m_nFlags & STUDIODATA_ERROR_MODEL) != 0;
}


//-----------------------------------------------------------------------------
// Brings all data associated with an MDL into memory
//-----------------------------------------------------------------------------
void CMDLCache::TouchAllData( MDLHandle_t handle )
{
	studiohdr_t *pStudioHdr = GetStudioHdr( handle );
	virtualmodel_t *pVModel = GetVirtualModel( handle );
	if ( pVModel )
	{
		// skip self, start at children
		// ensure all sub models are cached
		for ( int i=1; i<pVModel->m_group.Count(); ++i )
		{
			MDLHandle_t childHandle = (MDLHandle_t)(int)pVModel->m_group[i].cache&0xffff;
			if ( childHandle != MDLHANDLE_INVALID )
			{
				// FIXME: Should this be calling TouchAllData on the child?
				GetStudioHdr( childHandle );
			}
		}
	}

	if ( !IsX360() )
	{
		// cache the anims
		// Note that the animblocks start at 1!!!
		for ( int i=1; i< (int)pStudioHdr->numanimblocks; ++i )
		{
			pStudioHdr->GetAnimBlock( i );
		}
	}

	// cache the vertexes
	if ( pStudioHdr->numbodyparts )
	{
		CacheVertexData( pStudioHdr );
		GetHardwareData( handle );
	}
}


//-----------------------------------------------------------------------------
// Flushes all data
//-----------------------------------------------------------------------------
void CMDLCache::Flush( MDLCacheFlush_t nFlushFlags )
{
	// Free all MDLs that haven't been cleaned up
	MDLHandle_t i = m_MDLDict.First();
	while ( i != m_MDLDict.InvalidIndex() )
	{
		Flush( i, nFlushFlags );
		i = m_MDLDict.Next( i );
	}
}

//-----------------------------------------------------------------------------
// Cache handlers
//-----------------------------------------------------------------------------
static const char *g_ppszTypes[] =
{
	"studiohdr",	// MDLCACHE_STUDIOHDR
	"studiohwdata", // MDLCACHE_STUDIOHWDATA
	"vcollide",		// MDLCACHE_VCOLLIDE
	"animblock",	// MDLCACHE_ANIMBLOCK
	"virtualmodel", // MDLCACHE_VIRTUALMODEL
	"vertexes",		// MDLCACHE_VERTEXES
};

bool CMDLCache::HandleCacheNotification( const DataCacheNotification_t &notification  )
{
	switch ( notification.type )
	{
	case DC_AGE_DISCARD:
	case DC_FLUSH_DISCARD:
	case DC_REMOVED:
		{
			MdlCacheMsg( "MDLCache: Data cache discard %s %s\n", g_ppszTypes[TypeFromCacheID( notification.clientId )], GetModelName( HandleFromCacheID( notification.clientId ) ) );

			if ( (DataCacheClientID_t)notification.pItemData == notification.clientId ||
				 TypeFromCacheID(notification.clientId) != MDLCACHE_STUDIOHWDATA )
			{
				Assert( notification.pItemData );
				FreeData( TypeFromCacheID(notification.clientId), (void *)notification.pItemData );
			}
			else
			{
				UnloadHardwareData( HandleFromCacheID( notification.clientId ), false );
			}
			return true;
		}
	}

	return CDefaultDataCacheClient::HandleCacheNotification( notification );
}

bool CMDLCache::GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen  )
{
	if ( (DataCacheClientID_t)pItem == clientId )
	{
		return false;
	}

	MDLHandle_t handle = HandleFromCacheID( clientId );
	MDLCacheDataType_t type = TypeFromCacheID( clientId );

	Q_snprintf( pDest, nMaxLen, "%s - %s", g_ppszTypes[type], GetModelName( handle ) );

	return false;
}

//-----------------------------------------------------------------------------
// Flushes all data
//-----------------------------------------------------------------------------
void CMDLCache::BeginLock()
{
	m_pModelCacheSection->BeginFrameLocking();
	m_pMeshCacheSection->BeginFrameLocking();
}

//-----------------------------------------------------------------------------
// Flushes all data
//-----------------------------------------------------------------------------
void CMDLCache::EndLock()
{
	m_pModelCacheSection->EndFrameLocking();
	m_pMeshCacheSection->EndFrameLocking();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMDLCache::BreakFrameLock( bool bModels, bool bMesh )
{
	if ( bModels )
	{
		if ( m_pModelCacheSection->IsFrameLocking() )
		{
			Assert( !m_nModelCacheFrameLocks );
			m_nModelCacheFrameLocks = 0;
			do
			{
				m_nModelCacheFrameLocks++;
			} while ( m_pModelCacheSection->EndFrameLocking() );
		}

	}

	if ( bMesh )
	{
		if ( m_pMeshCacheSection->IsFrameLocking() )
		{
			Assert( !m_nMeshCacheFrameLocks );
			m_nMeshCacheFrameLocks = 0;
			do
			{
				m_nMeshCacheFrameLocks++;
			} while ( m_pMeshCacheSection->EndFrameLocking() );
		}
	}

}

void CMDLCache::RestoreFrameLock()
{
	while ( m_nModelCacheFrameLocks )
	{
		m_pModelCacheSection->BeginFrameLocking();
		m_nModelCacheFrameLocks--;
	}
	while ( m_nMeshCacheFrameLocks )
	{
		m_pMeshCacheSection->BeginFrameLocking();
		m_nMeshCacheFrameLocks--;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int *CMDLCache::GetFrameUnlockCounterPtrOLD()
{
	return GetCacheSection( MDLCACHE_STUDIOHDR )->GetFrameUnlockCounterPtr();
}

int *CMDLCache::GetFrameUnlockCounterPtr( MDLCacheDataType_t type )
{
	return GetCacheSection( type )->GetFrameUnlockCounterPtr();
}

//-----------------------------------------------------------------------------
// Completes all pending async operations
//-----------------------------------------------------------------------------
void CMDLCache::FinishPendingLoads()
{
	if ( !ThreadInMainThread() )
	{
		return;
	}

	AUTO_LOCK( m_AsyncMutex );

	// finish just our known jobs
	int iAsync = m_PendingAsyncs.Head();
	while ( iAsync != m_PendingAsyncs.InvalidIndex() )
	{
		AsyncInfo_t &info = m_PendingAsyncs[iAsync];
		if ( info.hControl )
		{
			g_pFullFileSystem->AsyncFinish( info.hControl, true );
		}
		iAsync = m_PendingAsyncs.Next( iAsync );
	}

	ProcessPendingAsyncs();
}

//-----------------------------------------------------------------------------
// Notify map load has started
//-----------------------------------------------------------------------------
void CMDLCache::BeginMapLoad()
{
	BreakFrameLock();

	studiodata_t *pStudioData;

	// Unlock prior map MDLs prior to load
	MDLHandle_t i = m_MDLDict.First();
	while ( i != m_MDLDict.InvalidIndex() )
	{
		pStudioData = m_MDLDict[i];
		if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_LOCKED_MDL )
		{
			GetCacheSection( MDLCACHE_STUDIOHDR )->Unlock( pStudioData->m_MDLCache );
			pStudioData->m_nFlags &= ~STUDIODATA_FLAGS_LOCKED_MDL;
		}
		i = m_MDLDict.Next( i );
	}
}

//-----------------------------------------------------------------------------
// Notify map load is complete
//-----------------------------------------------------------------------------
void CMDLCache::EndMapLoad()
{
	FinishPendingLoads();

	// Remove all stray MDLs not referenced during load
	if ( mod_lock_mdls_on_load.GetBool() )
	{
		studiodata_t *pStudioData;
		MDLHandle_t i = m_MDLDict.First();
		while ( i != m_MDLDict.InvalidIndex() )
		{
			pStudioData = m_MDLDict[i];
			if ( !(pStudioData->m_nFlags & STUDIODATA_FLAGS_LOCKED_MDL) )
			{
				Flush( i, MDLCACHE_FLUSH_STUDIOHDR );
			}
			i = m_MDLDict.Next( i );
		}
	}

	RestoreFrameLock();
}


//-----------------------------------------------------------------------------
// Is a particular part of the model data loaded?
//-----------------------------------------------------------------------------
bool CMDLCache::IsDataLoaded( MDLHandle_t handle, MDLCacheDataType_t type )
{
	if ( handle == MDLHANDLE_INVALID || !m_MDLDict.IsValidIndex( handle ) )
		return false;

	studiodata_t *pData = m_MDLDict[ handle ];
	switch( type )
	{
	case MDLCACHE_STUDIOHDR:
		return GetCacheSection( MDLCACHE_STUDIOHDR )->IsPresent( pData->m_MDLCache );

	case MDLCACHE_STUDIOHWDATA:
		return ( pData->m_nFlags & STUDIODATA_FLAGS_STUDIOMESH_LOADED ) != 0;

	case MDLCACHE_VCOLLIDE:
		return ( pData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_LOADED ) != 0;

	case MDLCACHE_ANIMBLOCK:
		{
			if ( !pData->m_pAnimBlock )
				return false;

			for (int i = 0; i < pData->m_nAnimBlockCount; ++i )
			{
				if ( !pData->m_pAnimBlock[i] )
					return false;

				if ( !GetCacheSection( type )->IsPresent( pData->m_pAnimBlock[i] ) )
					return false;
			}
			return true;
		}

	case MDLCACHE_VIRTUALMODEL:
		return ( pData->m_pVirtualModel != 0 );

	case MDLCACHE_VERTEXES:
		return m_pMeshCacheSection->IsPresent( pData->m_VertexCache );
	}
	return false;
}


//-----------------------------------------------------------------------------
// Get the correct extension for our dx
//-----------------------------------------------------------------------------
const char *CMDLCache::GetVTXExtension()
{
	if ( IsPC() )
	{
		if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 90 )
		{
			return ".dx90.vtx";
		}
		else if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80 )
		{
			return ".dx80.vtx";
		}
		else
		{
			return ".sw.vtx";
		}
	}

	return ".dx90.vtx";
}

//-----------------------------------------------------------------------------
// Minimal presence and header validation, no data loads
// Return true if successful, false otherwise.
//-----------------------------------------------------------------------------
bool CMDLCache::VerifyHeaders( studiohdr_t *pStudioHdr )
{
	VPROF( "CMDLCache::VerifyHeaders" );

	if ( developer.GetInt() < 2 )
	{
		return true;
	}

	// model has no vertex data
	if ( !pStudioHdr->numbodyparts )
	{
		// valid
		return true;
	}

	char pFileName[ MAX_PATH ];
	MDLHandle_t handle = (MDLHandle_t)(int)pStudioHdr->virtualModel&0xffff;

	MakeFilename( handle, ".vvd", pFileName, sizeof(pFileName) );

	MdlCacheMsg("MDLCache: Load VVD (verify) %s\n", pFileName );

	// vvd header only
	CUtlBuffer vvdHeader( 0, sizeof(vertexFileHeader_t) );
	if ( !ReadFileNative( pFileName, "GAME", vvdHeader, sizeof(vertexFileHeader_t) ) )
	{
		return false;
	}

	vertexFileHeader_t *pVertexHdr = (vertexFileHeader_t*)vvdHeader.PeekGet();

	// check
	if (( pVertexHdr->id != MODEL_VERTEX_FILE_ID ) ||
		( pVertexHdr->version != MODEL_VERTEX_FILE_VERSION ) ||
		( pVertexHdr->checksum != pStudioHdr->checksum ))
	{
		return false;
	}

	// load the VTX file
	// use model name for correct path
	MakeFilename( handle, GetVTXExtension(), pFileName, sizeof(pFileName) );

	MdlCacheMsg("MDLCache: Load VTX (verify) %s\n", pFileName );

	// vtx header only
	CUtlBuffer vtxHeader( 0, sizeof(OptimizedModel::FileHeader_t) );
	if ( !ReadFileNative( pFileName, "GAME", vtxHeader, sizeof(OptimizedModel::FileHeader_t) ) )
	{
		return false;
	}

	// check
	OptimizedModel::FileHeader_t *pVtxHdr = (OptimizedModel::FileHeader_t*)vtxHeader.PeekGet();
	if (( pVtxHdr->version != OPTIMIZED_MODEL_FILE_VERSION ) ||
		( pVtxHdr->checkSum != pStudioHdr->checksum ))
	{
		return false;
	}

	// valid
	return true;
}


//-----------------------------------------------------------------------------
// Cache model's specified dynamic data
//-----------------------------------------------------------------------------
vertexFileHeader_t *CMDLCache::CacheVertexData( studiohdr_t *pStudioHdr )
{
	VPROF( "CMDLCache::CacheVertexData" );

	vertexFileHeader_t	*pVvdHdr;
	MDLHandle_t			handle;

	Assert( pStudioHdr );

	handle = (MDLHandle_t)(int)pStudioHdr->virtualModel&0xffff;
	Assert( handle != MDLHANDLE_INVALID );

	pVvdHdr = (vertexFileHeader_t *)CheckData( m_MDLDict[handle]->m_VertexCache, MDLCACHE_VERTEXES );
	if ( pVvdHdr )
	{
		return pVvdHdr;
	}

	m_MDLDict[handle]->m_VertexCache = NULL;

	return LoadVertexData( pStudioHdr );
}

//-----------------------------------------------------------------------------
// Start an async transfer
//-----------------------------------------------------------------------------
FSAsyncStatus_t CMDLCache::LoadData( const char *pszFilename, const char *pszPathID, void *pDest, int nBytes, int nOffset, bool bAsync, FSAsyncControl_t *pControl )
{
	if ( !*pControl )
	{
		if ( IsX360() && g_pQueuedLoader->IsMapLoading() )
		{
			DevWarning( "CMDLCache: Non-Optimal loading path for %s\n", pszFilename );
		}

		FileAsyncRequest_t asyncRequest;
		asyncRequest.pszFilename = pszFilename;
		asyncRequest.pszPathID = pszPathID;
		asyncRequest.pData = pDest;
		asyncRequest.nBytes = nBytes;
		asyncRequest.nOffset = nOffset;

		if ( !pDest )
		{
			asyncRequest.flags = FSASYNC_FLAGS_ALLOCNOFREE;
		}

		if ( !bAsync )
		{
			asyncRequest.flags |= FSASYNC_FLAGS_SYNC;
		}

		MEM_ALLOC_CREDIT();
		return g_pFullFileSystem->AsyncRead( asyncRequest, pControl );
	}

	return FSASYNC_ERR_FAILURE;
}

//-----------------------------------------------------------------------------
// Determine the maximum number of 'real' bone influences used by any vertex in a model
// (100% binding to bone zero doesn't count)
//-----------------------------------------------------------------------------
int ComputeMaxRealBoneInfluences( vertexFileHeader_t * vertexFile, int lod )
{
	const mstudiovertex_t * verts = vertexFile->GetVertexData();
	int numVerts = vertexFile->numLODVertexes[ lod ];
	Assert(verts);

	int maxWeights = 0;
	for (int i = 0;i < numVerts;i++)
	{
		if ( verts[i].m_BoneWeights.numbones > 0 )
		{
			int numWeights = 0;
			for (int j = 0;j < MAX_NUM_BONES_PER_VERT;j++)
			{
				if ( verts[i].m_BoneWeights.weight[j] > 0 )
					numWeights = j + 1;
			}
			if ( ( numWeights == 1 ) && ( verts[i].m_BoneWeights.bone[0] == 0 ) )
			{
				// 100% binding to first bone - not really skinned (the first bone is just the model transform)
				numWeights = 0;
			}
			maxWeights = max( numWeights, maxWeights );
		}
	}
	return maxWeights;
}

//-----------------------------------------------------------------------------
// Generate thin vertices (containing just the data needed to do model decals)
//-----------------------------------------------------------------------------
vertexFileHeader_t * CMDLCache::CreateThinVertexes( vertexFileHeader_t * originalData, const studiohdr_t * pStudioHdr, int * cacheLength )
{
	int rootLod = min( (int)pStudioHdr->rootLOD, ( originalData->numLODs - 1 ) );
	int numVerts = originalData->numLODVertexes[ rootLod ] + 1; // Add 1 vert to support prefetch during array access

	int numBoneInfluences = ComputeMaxRealBoneInfluences( originalData, rootLod );
	// Only store (N-1) weights (all N weights sum to 1, so we can re-compute the Nth weight later)
	int numStoredWeights = max( 0, ( numBoneInfluences - 1 ) );

	int vertexSize = 2*sizeof( Vector ) + numBoneInfluences*sizeof( unsigned char ) + numStoredWeights*sizeof( float );
	*cacheLength = sizeof( vertexFileHeader_t ) + sizeof( thinModelVertices_t ) + numVerts*vertexSize;

	// Allocate cache space for the thin data
	MemAlloc_PushAllocDbgInfo( "Models:Vertex data", 0);
	vertexFileHeader_t * pNewVvdHdr = (vertexFileHeader_t *)AllocData( MDLCACHE_VERTEXES, *cacheLength );
	MemAlloc_PopAllocDbgInfo();

	Assert( pNewVvdHdr );
	if ( pNewVvdHdr )
	{
		// Copy the header and set it up to hold thin vertex data
		memcpy( (void *)pNewVvdHdr, (void *)originalData, sizeof( vertexFileHeader_t ) );
		pNewVvdHdr->id					= MODEL_VERTEX_FILE_THIN_ID;
		pNewVvdHdr->numFixups			= 0;
		pNewVvdHdr->fixupTableStart		= 0;
		pNewVvdHdr->tangentDataStart	= 0;
		pNewVvdHdr->vertexDataStart		= sizeof( vertexFileHeader_t );

		// Set up the thin vertex structure
 		thinModelVertices_t	* pNewThinVerts = (thinModelVertices_t	*)( pNewVvdHdr		+ 1 );
		Vector				* pPositions	= (Vector				*)( pNewThinVerts	+ 1 );
		float				* pBoneWeights	= (float				*)( pPositions		+ numVerts );
		// Alloc the (short) normals here to avoid mis-aligning the float data
		unsigned short		* pNormals		= (unsigned short		*)( pBoneWeights	+ numVerts*numStoredWeights );
		// Alloc the (char) indices here to avoid mis-aligning the float/short data
		char				* pBoneIndices	= (char					*)( pNormals		+ numVerts );
		if ( numStoredWeights == 0 )
			pBoneWeights = NULL;
		if ( numBoneInfluences == 0 )
			pBoneIndices = NULL;
		pNewThinVerts->Init( numBoneInfluences, pPositions, pNormals, pBoneWeights, pBoneIndices );

		// Copy over the original data
		const mstudiovertex_t * srcVertexData = originalData->GetVertexData();
		for ( int i = 0; i < numVerts; i++ )
		{
			pNewThinVerts->SetPosition( i, srcVertexData[ i ].m_vecPosition );
			pNewThinVerts->SetNormal(   i, srcVertexData[ i ].m_vecNormal );
			if ( numBoneInfluences > 0 )
			{
				mstudioboneweight_t boneWeights;
				boneWeights.numbones = numBoneInfluences;
				for ( int j = 0; j < numStoredWeights; j++ )
				{
					boneWeights.weight[ j ] = srcVertexData[ i ].m_BoneWeights.weight[ j ];
				}
				for ( int j = 0; j < numBoneInfluences; j++ )
				{
					boneWeights.bone[ j ] = srcVertexData[ i ].m_BoneWeights.bone[ j ];
				}
				pNewThinVerts->SetBoneWeights( i, boneWeights );
			}
		}
	}

	return pNewVvdHdr;
}

//-----------------------------------------------------------------------------
// Process the provided raw data into the cache. Distributes to low level
// unserialization or build methods.
//-----------------------------------------------------------------------------
bool CMDLCache::ProcessDataIntoCache( MDLHandle_t handle, MDLCacheDataType_t type, int iAnimBlock, void *pData, int nDataSize, bool bDataValid )
{
	studiohdr_t *pStudioHdrCurrent = NULL;
	if ( type != MDLCACHE_STUDIOHDR )
	{
		// can only get the studiohdr once the header has been processed successfully into the cache
		// causes a ProcessDataIntoCache() with the studiohdr data
		pStudioHdrCurrent = GetStudioHdr( handle );
		if ( !pStudioHdrCurrent )
		{
			return false;
		}
	}

	studiodata_t *pStudioDataCurrent = m_MDLDict[handle];

	if ( !pStudioDataCurrent )
	{
		return false;
	}

	switch ( type )
	{
	case MDLCACHE_STUDIOHDR:
		{
			pStudioHdrCurrent = UnserializeMDL( handle, pData, nDataSize, bDataValid );
			if ( !pStudioHdrCurrent )
			{
				return false;
			}

			if (!Studio_ConvertStudioHdrToNewVersion( pStudioHdrCurrent ))
			{
				Warning( "MDLCache: %s needs to be recompiled\n", pStudioHdrCurrent->pszName() );
			}

			if ( pStudioHdrCurrent->numincludemodels == 0 )
			{
				// perf optimization, calculate once and cache off the autoplay sequences
				int nCount = pStudioHdrCurrent->CountAutoplaySequences();
				if ( nCount )
				{
					AllocateAutoplaySequences( m_MDLDict[handle], nCount );
					pStudioHdrCurrent->CopyAutoplaySequences( m_MDLDict[handle]->m_pAutoplaySequenceList, nCount );
				}
			}

			// Load animations
			UnserializeAllVirtualModelsAndAnimBlocks( handle );
			break;
		}

	case MDLCACHE_VERTEXES:
		{
			if ( bDataValid )
			{
				BuildAndCacheVertexData( pStudioHdrCurrent, (vertexFileHeader_t *)pData );
			}
			else
			{
				pStudioDataCurrent->m_nFlags |= STUDIODATA_FLAGS_NO_VERTEX_DATA;
				if ( pStudioHdrCurrent->numbodyparts )
				{
					// expected data not valid
					Warning( "MDLCache: Failed load of .VVD data for %s\n", pStudioHdrCurrent->pszName() );
					return false;
				}
			}
			break;
		}

	case MDLCACHE_STUDIOHWDATA:
		{
			if ( bDataValid )
			{
				BuildHardwareData( handle, pStudioDataCurrent, pStudioHdrCurrent, (OptimizedModel::FileHeader_t *)pData );
			}
			else
			{
				pStudioDataCurrent->m_nFlags |= STUDIODATA_FLAGS_NO_STUDIOMESH;
				if ( pStudioHdrCurrent->numbodyparts )
				{
					// expected data not valid
					Warning( "MDLCache: Failed load of .VTX data for %s\n", pStudioHdrCurrent->pszName() );
					return false;
				}
			}

			m_pMeshCacheSection->Unlock( pStudioDataCurrent->m_VertexCache );
			m_pMeshCacheSection->Age( pStudioDataCurrent->m_VertexCache );

			// FIXME: thin VVD data on PC too (have to address alt-tab, various DX8/DX7/debug software paths in studiorender, tools, etc)
			static bool bCompressedVVDs = CommandLine()->CheckParm( "-no_compressed_vvds" ) == NULL;
			if ( IsX360() && !( pStudioDataCurrent->m_nFlags & STUDIODATA_FLAGS_NO_STUDIOMESH ) && bCompressedVVDs )
			{
				// Replace the cached vertex data with a thin version (used for model decals).
				// Flexed meshes require the fat data to remain, for CPU mesh anim.
				if ( pStudioHdrCurrent->numflexdesc == 0 )
				{
					vertexFileHeader_t *originalVertexData = GetVertexData( handle );
					Assert( originalVertexData );
					if ( originalVertexData )
					{
						int thinVertexDataSize = 0;
						vertexFileHeader_t *thinVertexData = CreateThinVertexes( originalVertexData, pStudioHdrCurrent, &thinVertexDataSize );
						Assert( thinVertexData && ( thinVertexDataSize > 0 ) );
						if ( thinVertexData && ( thinVertexDataSize > 0 ) )
						{
							// Remove the original cache entry (and free it)
							Flush( handle, MDLCACHE_FLUSH_VERTEXES | MDLCACHE_FLUSH_IGNORELOCK );
							// Add the new one
							CacheData( &pStudioDataCurrent->m_VertexCache, thinVertexData, thinVertexDataSize, pStudioHdrCurrent->pszName(), MDLCACHE_VERTEXES, MakeCacheID( handle, MDLCACHE_VERTEXES) );
						}
					}
				}
			}

			break;
		}

	case MDLCACHE_ANIMBLOCK:
		{
			MEM_ALLOC_CREDIT_( __FILE__ ": Anim Blocks" );

			if ( bDataValid )
			{
				MdlCacheMsg( "MDLCache: Finish load anim block %s (block %i)\n", pStudioHdrCurrent->pszName(), iAnimBlock );

				char pCacheName[MAX_PATH];
				Q_snprintf( pCacheName, MAX_PATH, "%s (block %i)", pStudioHdrCurrent->pszName(), iAnimBlock );

				if ( IsX360() )
				{
					if ( CLZMA::IsCompressed( (unsigned char *)pData ) )
					{
						// anim block arrives compressed, decode and cache the results
						unsigned int nOriginalSize = CLZMA::GetActualSize( (unsigned char *)pData );

						// get a "fake" (not really aligned) optimal read buffer, as expected by the free logic
						void *pOriginalData = g_pFullFileSystem->AllocOptimalReadBuffer( FILESYSTEM_INVALID_HANDLE, nOriginalSize, 0 );
						unsigned int nOutputSize = CLZMA::Uncompress( (unsigned char *)pData, (unsigned char *)pOriginalData );
						if ( nOutputSize != nOriginalSize )
						{
							// decoder failure
							g_pFullFileSystem->FreeOptimalReadBuffer( pOriginalData );
							return false;
						}

						// input i/o buffer is now unused
						g_pFullFileSystem->FreeOptimalReadBuffer( pData );

						// datacache will now own the data
						pData = pOriginalData;
						nDataSize = nOriginalSize;
					}
				}

				CacheData( &pStudioDataCurrent->m_pAnimBlock[iAnimBlock], pData, nDataSize, pCacheName, MDLCACHE_ANIMBLOCK, MakeCacheID( handle, MDLCACHE_ANIMBLOCK) );
			}
			else
			{
				MdlCacheMsg( "MDLCache: Failed load anim block %s (block %i)\n", pStudioHdrCurrent->pszName(), iAnimBlock );
				if ( pStudioDataCurrent->m_pAnimBlock )
				{
					pStudioDataCurrent->m_pAnimBlock[iAnimBlock] = NULL;
				}
				return false;
			}
			break;
		}

	case MDLCACHE_VCOLLIDE:
		{
			// always marked as loaded, vcollides are not present for every model
			pStudioDataCurrent->m_nFlags |= STUDIODATA_FLAGS_VCOLLISION_LOADED;

			if ( bDataValid )
			{
				MdlCacheMsg( "MDLCache: Finish load vcollide for %s\n", pStudioHdrCurrent->pszName() );

				CTempAllocHelper pOriginalData;
				if ( IsX360() )
				{
					if ( CLZMA::IsCompressed( (unsigned char *)pData ) )
					{
						// phy arrives compressed, decode and cache the results
						unsigned int nOriginalSize = CLZMA::GetActualSize( (unsigned char *)pData );
						pOriginalData.Alloc( nOriginalSize );
						unsigned int nOutputSize = CLZMA::Uncompress( (unsigned char *)pData, (unsigned char *)pOriginalData.Get() );
						if ( nOutputSize != nOriginalSize )
						{
							// decoder failure
							return NULL;
						}

						pData = pOriginalData.Get();
						nDataSize = nOriginalSize;
					}
				}

				CUtlBuffer buf( pData, nDataSize, CUtlBuffer::READ_ONLY );
				buf.SeekPut( CUtlBuffer::SEEK_HEAD, nDataSize );

				phyheader_t header;
				buf.Get( &header, sizeof( phyheader_t ) );
				if ( ( header.size == sizeof( header ) ) && header.solidCount > 0 )
				{
					int nBufSize = buf.TellMaxPut() - buf.TellGet();
					vcollide_t *pCollide = &pStudioDataCurrent->m_VCollisionData;
					g_pPhysicsCollision->VCollideLoad( pCollide, header.solidCount, (const char*)buf.PeekGet(), nBufSize );
					if ( m_pCacheNotify )
					{
						m_pCacheNotify->OnDataLoaded( MDLCACHE_VCOLLIDE, handle );
					}
				}
			}
			else
			{
				MdlCacheWarning( "MDLCache: Failed load of .PHY data for %s\n", pStudioHdrCurrent->pszName() );
				return false;
			}
			break;
		}

	default:
		Assert( 0 );
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Returns:
//	<0: indeterminate at this time
//	=0: pending
//	>0:	completed
//-----------------------------------------------------------------------------
int CMDLCache::ProcessPendingAsync( int iAsync )
{
	if ( !ThreadInMainThread() )
	{
		return -1;
	}

	ASSERT_NO_REENTRY();

	void *pData = NULL;
	int nBytesRead = 0;

	AsyncInfo_t *pInfo;
	{
		AUTO_LOCK( m_AsyncMutex );
		pInfo = &m_PendingAsyncs[iAsync];
	}
	Assert( pInfo->hControl );

	FSAsyncStatus_t status = g_pFullFileSystem->AsyncGetResult( pInfo->hControl, &pData, &nBytesRead );
	if ( status == FSASYNC_STATUS_PENDING )
	{
		return 0;
	}

	AsyncInfo_t info = *pInfo;
	pInfo = &info;
	ClearAsync( pInfo->hModel, pInfo->type, pInfo->iAnimBlock );

	switch ( pInfo->type )
	{
	case MDLCACHE_VERTEXES:
	case MDLCACHE_STUDIOHWDATA:
	case MDLCACHE_VCOLLIDE:
		{
			ProcessDataIntoCache( pInfo->hModel, pInfo->type, 0, pData, nBytesRead, status == FSASYNC_OK );
			g_pFullFileSystem->FreeOptimalReadBuffer( pData );
			break;
		}

	case MDLCACHE_ANIMBLOCK:
		{
			// cache assumes ownership of valid async'd data
			if ( !ProcessDataIntoCache( pInfo->hModel, MDLCACHE_ANIMBLOCK, pInfo->iAnimBlock, pData, nBytesRead, status == FSASYNC_OK ) )
			{
				g_pFullFileSystem->FreeOptimalReadBuffer( pData );
			}
			break;
		}

	default:
		Assert( 0 );
	}

	return 1;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMDLCache::ProcessPendingAsyncs( MDLCacheDataType_t type )
{
	if ( !ThreadInMainThread() )
	{
		return;
	}

	if ( !m_PendingAsyncs.Count() )
	{
		return;
	}

	static bool bReentering;
	if ( bReentering )
	{
		return;
	}
	bReentering = true;

	AUTO_LOCK( m_AsyncMutex );

	// Process all of the completed loads that were requested before a new one. This ensures two
	// things -- the LRU is in correct order, and it catches precached items lurking
	// in the async queue that have only been requested once (thus aren't being cached
	// and might lurk forever, e.g., wood gibs in the citadel)
	int current = m_PendingAsyncs.Head();
	while ( current != m_PendingAsyncs.InvalidIndex() )
	{
		int next = m_PendingAsyncs.Next( current );

		if ( type == MDLCACHE_NONE || m_PendingAsyncs[current].type == type )
		{
			// process, also removes from list
			if ( ProcessPendingAsync( current ) <= 0 )
			{
				// indeterminate or pending
				break;
			}
		}

		current = next;
	}

	bReentering = false;
}

//-----------------------------------------------------------------------------
// Cache model's specified dynamic data
//-----------------------------------------------------------------------------
bool CMDLCache::ClearAsync( MDLHandle_t handle, MDLCacheDataType_t type, int iAnimBlock, bool bAbort )
{
	int iAsyncInfo = GetAsyncInfoIndex( handle, type, iAnimBlock );
	if ( iAsyncInfo != NO_ASYNC )
	{
		AsyncInfo_t *pInfo;
		{
			AUTO_LOCK( m_AsyncMutex );
			pInfo = &m_PendingAsyncs[iAsyncInfo];
		}
		if ( pInfo->hControl )
		{
			if ( bAbort )
			{
				g_pFullFileSystem->AsyncAbort(  pInfo->hControl );
				void *pData;
				int ignored;
				if ( g_pFullFileSystem->AsyncGetResult(  pInfo->hControl, &pData, &ignored ) == FSASYNC_OK )
				{
					g_pFullFileSystem->FreeOptimalReadBuffer( pData );
				}
			}
			g_pFullFileSystem->AsyncRelease(  pInfo->hControl );
			pInfo->hControl = NULL;
		}

		SetAsyncInfoIndex( handle, type, iAnimBlock, NO_ASYNC );
		{
			AUTO_LOCK( m_AsyncMutex );
			m_PendingAsyncs.Remove( iAsyncInfo );
		}

		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMDLCache::GetAsyncLoad( MDLCacheDataType_t type )
{
	switch ( type )
	{
	case MDLCACHE_STUDIOHDR:
		return false;
	case MDLCACHE_STUDIOHWDATA:
		return mod_load_mesh_async.GetBool();
	case MDLCACHE_VCOLLIDE:
		return mod_load_vcollide_async.GetBool();
	case MDLCACHE_ANIMBLOCK:
		return mod_load_anims_async.GetBool();
	case MDLCACHE_VIRTUALMODEL:
		return false;
	case MDLCACHE_VERTEXES:
		return mod_load_mesh_async.GetBool();
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMDLCache::SetAsyncLoad( MDLCacheDataType_t type, bool bAsync )
{
	bool bRetVal = false;
	switch ( type )
	{
	case MDLCACHE_STUDIOHDR:
		break;
	case MDLCACHE_STUDIOHWDATA:
		bRetVal = mod_load_mesh_async.GetBool();
		mod_load_mesh_async.SetValue( bAsync );
		break;
	case MDLCACHE_VCOLLIDE:
		bRetVal = mod_load_vcollide_async.GetBool();
		mod_load_vcollide_async.SetValue( bAsync );
		break;
	case MDLCACHE_ANIMBLOCK:
		bRetVal = mod_load_anims_async.GetBool();
		mod_load_anims_async.SetValue( bAsync );
		break;
	case MDLCACHE_VIRTUALMODEL:
		return false;
		break;
	case MDLCACHE_VERTEXES:
		bRetVal = mod_load_mesh_async.GetBool();
		mod_load_mesh_async.SetValue( bAsync );
		break;
	}
	return bRetVal;
}

//-----------------------------------------------------------------------------
// Cache model's specified dynamic data
//-----------------------------------------------------------------------------
vertexFileHeader_t *CMDLCache::BuildAndCacheVertexData( studiohdr_t *pStudioHdr, vertexFileHeader_t *pRawVvdHdr  )
{
	MDLHandle_t	handle = (MDLHandle_t)(int)pStudioHdr->virtualModel&0xffff;
	vertexFileHeader_t *pVvdHdr;

	MdlCacheMsg( "MDLCache: Load VVD for %s\n", pStudioHdr->pszName() );

	Assert( pRawVvdHdr );

	// check header
	if ( pRawVvdHdr->id != MODEL_VERTEX_FILE_ID )
	{
		Warning( "Error Vertex File for '%s' id %d should be %d\n", pStudioHdr->pszName(), pRawVvdHdr->id, MODEL_VERTEX_FILE_ID );
		return NULL;
	}
	if ( pRawVvdHdr->version != MODEL_VERTEX_FILE_VERSION )
	{
		Warning( "Error Vertex File for '%s' version %d should be %d\n", pStudioHdr->pszName(), pRawVvdHdr->version, MODEL_VERTEX_FILE_VERSION );
		return NULL;
	}
	if ( pRawVvdHdr->checksum != pStudioHdr->checksum )
	{
		Warning( "Error Vertex File for '%s' checksum %d should be %d\n", pStudioHdr->pszName(), pRawVvdHdr->checksum, pStudioHdr->checksum );
		return NULL;
	}

	Assert( pRawVvdHdr->numLODs );
	if ( !pRawVvdHdr->numLODs )
	{
		return NULL;
	}

	CTempAllocHelper pOriginalData;
	if ( IsX360() )
	{
		unsigned char *pInput = (unsigned char *)pRawVvdHdr + sizeof( vertexFileHeader_t );
		if ( CLZMA::IsCompressed( pInput ) )
		{
			// vvd arrives compressed, decode and cache the results
			unsigned int nOriginalSize = CLZMA::GetActualSize( pInput );
			pOriginalData.Alloc( sizeof( vertexFileHeader_t ) + nOriginalSize );
			V_memcpy( pOriginalData.Get(), pRawVvdHdr, sizeof( vertexFileHeader_t ) );
			unsigned int nOutputSize = CLZMA::Uncompress( pInput, sizeof( vertexFileHeader_t ) + (unsigned char *)pOriginalData.Get() );
			if ( nOutputSize != nOriginalSize )
			{
				// decoder failure
				return NULL;
			}

			pRawVvdHdr = (vertexFileHeader_t *)pOriginalData.Get();
		}
	}

	bool bNeedsTangentS = IsX360() || (g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80);
	int rootLOD = min( (int)pStudioHdr->rootLOD, pRawVvdHdr->numLODs - 1 );

	// determine final cache footprint, possibly truncated due to lod
	int cacheLength = Studio_VertexDataSize( pRawVvdHdr, rootLOD, bNeedsTangentS );

	MdlCacheMsg("MDLCache: Alloc VVD %s\n", GetModelName( handle ) );

	// allocate cache space
	MemAlloc_PushAllocDbgInfo( "Models:Vertex data", 0);
	pVvdHdr = (vertexFileHeader_t *)AllocData( MDLCACHE_VERTEXES, cacheLength );
	MemAlloc_PopAllocDbgInfo();

	GetCacheSection( MDLCACHE_VERTEXES )->BeginFrameLocking();

	CacheData( &m_MDLDict[handle]->m_VertexCache, pVvdHdr, cacheLength, pStudioHdr->pszName(), MDLCACHE_VERTEXES, MakeCacheID( handle, MDLCACHE_VERTEXES) );

	// expected 32 byte alignment
	Assert( ((int64)pVvdHdr & 0x1F) == 0 );

	// load minimum vertexes and fixup
	Studio_LoadVertexes( pRawVvdHdr, pVvdHdr, rootLOD, bNeedsTangentS );

	GetCacheSection( MDLCACHE_VERTEXES )->EndFrameLocking();

	return pVvdHdr;
}

//-----------------------------------------------------------------------------
// Load and cache model's specified dynamic data
//-----------------------------------------------------------------------------
vertexFileHeader_t *CMDLCache::LoadVertexData( studiohdr_t *pStudioHdr )
{
	char				pFileName[MAX_PATH];
	MDLHandle_t			handle;

	Assert( pStudioHdr );
	handle = (MDLHandle_t)(int)pStudioHdr->virtualModel&0xffff;
	Assert( !m_MDLDict[handle]->m_VertexCache );

	studiodata_t *pStudioData = m_MDLDict[handle];

	if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_NO_VERTEX_DATA )
	{
		return NULL;
	}

	int iAsync = GetAsyncInfoIndex( handle, MDLCACHE_VERTEXES );

	if ( iAsync == NO_ASYNC )
	{
		// load the VVD file
		// use model name for correct path
		MakeFilename( handle, ".vvd", pFileName, sizeof(pFileName) );
		if ( IsX360() )
		{
			char pX360Filename[MAX_PATH];
			UpdateOrCreate( pStudioHdr, pFileName, pX360Filename, sizeof( pX360Filename ), "GAME" );
			Q_strncpy( pFileName, pX360Filename, sizeof(pX360Filename) );
		}

		MdlCacheMsg( "MDLCache: Begin load VVD %s\n", pFileName );

		AsyncInfo_t info;
		if ( IsDebug() )
		{
			memset( &info, 0xdd, sizeof( AsyncInfo_t ) );
		}
		info.hModel = handle;
		info.type = MDLCACHE_VERTEXES;
		info.iAnimBlock = 0;
		info.hControl = NULL;
		LoadData( pFileName, "GAME", mod_load_mesh_async.GetBool(), &info.hControl );
		{
			AUTO_LOCK( m_AsyncMutex );
			iAsync = SetAsyncInfoIndex( handle, MDLCACHE_VERTEXES, m_PendingAsyncs.AddToTail( info ) );
		}
	}

	ProcessPendingAsync( iAsync );

	return (vertexFileHeader_t *)CheckData( m_MDLDict[handle]->m_VertexCache, MDLCACHE_VERTEXES );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
vertexFileHeader_t *CMDLCache::GetVertexData( MDLHandle_t handle )
{
	if ( mod_test_not_available.GetBool() )
		return NULL;

	if ( mod_test_verts_not_available.GetBool() )
		return NULL;

	return CacheVertexData( GetStudioHdr( handle ) );
}


//-----------------------------------------------------------------------------
// Allocates a cacheable item
//-----------------------------------------------------------------------------
void *CMDLCache::AllocData( MDLCacheDataType_t type, int size )
{
	void *pData = _aligned_malloc( size, 32 );

	if ( !pData )
	{
		Error( "CMDLCache:: Out of memory" );
		return NULL;
	}

	return pData;
}


//-----------------------------------------------------------------------------
// Caches an item
//-----------------------------------------------------------------------------
void CMDLCache::CacheData( DataCacheHandle_t *c, void *pData, int size, const char *name, MDLCacheDataType_t type, DataCacheClientID_t id )
{
	if ( !pData )
	{
		return;
	}

	if ( id == (DataCacheClientID_t)-1 )
		id = (DataCacheClientID_t)pData;

	GetCacheSection( type )->Add(id, pData, size, c );
}

//-----------------------------------------------------------------------------
// returns the cached data, and moves to the head of the LRU list
// if present, otherwise returns NULL
//-----------------------------------------------------------------------------
void *CMDLCache::CheckData( DataCacheHandle_t c, MDLCacheDataType_t type )
{
	return GetCacheSection( type )->Get( c, true );
}

//-----------------------------------------------------------------------------
// returns the cached data, if present, otherwise returns NULL
//-----------------------------------------------------------------------------
void *CMDLCache::CheckDataNoTouch( DataCacheHandle_t c, MDLCacheDataType_t type )
{
	return GetCacheSection( type )->GetNoTouch( c, true );
}

//-----------------------------------------------------------------------------
// Frees a cache item
//-----------------------------------------------------------------------------
void CMDLCache::UncacheData( DataCacheHandle_t c, MDLCacheDataType_t type, bool bLockedOk )
{
	if ( c == DC_INVALID_HANDLE )
		return;

	if ( !GetCacheSection( type )->IsPresent( c ) )
		return;

	if ( GetCacheSection( type )->BreakLock( c )  && !bLockedOk )
	{
		DevMsg( "Warning: freed a locked resource\n" );
		Assert( 0 );
	}

	const void *pItemData;
	GetCacheSection( type )->Remove( c, &pItemData );

	FreeData( type, (void *)pItemData );
}


//-----------------------------------------------------------------------------
// Frees memory for an item
//-----------------------------------------------------------------------------
void CMDLCache::FreeData( MDLCacheDataType_t type, void *pData )
{
	if ( type != MDLCACHE_ANIMBLOCK )
	{
		_aligned_free( (void *)pData );
	}
	else
	{
		g_pFullFileSystem->FreeOptimalReadBuffer( pData );
	}
}


void CMDLCache::InitPreloadData( bool rebuild )
{
}

void CMDLCache::ShutdownPreloadData()
{
}

//-----------------------------------------------------------------------------
// Work function for processing a model delivered by the queued loader.
// ProcessDataIntoCache() is invoked for each MDL datum.
//-----------------------------------------------------------------------------
void CMDLCache::ProcessQueuedData( ModelParts_t *pModelParts, bool bHeaderOnly )
{
	void *pData;
	int nSize;

	// the studiohdr is critical, ensure it's setup as expected
	MDLHandle_t handle = pModelParts->hMDL;
	studiohdr_t *pStudioHdr = NULL;
	if ( !pModelParts->bHeaderLoaded && ( pModelParts->nLoadedParts & ( 1 << ModelParts_t::BUFFER_MDL ) ) )
	{
		DEBUG_SCOPE_TIMER(mdl);
		pData = pModelParts->Buffers[ModelParts_t::BUFFER_MDL].Base();
		nSize = pModelParts->Buffers[ModelParts_t::BUFFER_MDL].TellMaxPut();
		ProcessDataIntoCache( handle, MDLCACHE_STUDIOHDR, 0, pData, nSize, nSize != 0 );
		LockStudioHdr( handle );
		g_pFullFileSystem->FreeOptimalReadBuffer( pData );
		pModelParts->bHeaderLoaded = true;
	}

	if ( bHeaderOnly )
	{
		return;
	}

	bool bAbort = false;
	pStudioHdr = (studiohdr_t *)CheckDataNoTouch( m_MDLDict[handle]->m_MDLCache, MDLCACHE_STUDIOHDR );
	if ( !pStudioHdr )
	{
		// The header is expected to be loaded and locked, everything depends on it!
		// but if the async read fails, we might not have it
		//Assert( 0 );
		DevWarning( "CMDLCache:: Error MDLCACHE_STUDIOHDR not present for '%s'\n", GetModelName( handle ) );

		// cannot unravel any of this model's dependant data, abort any further processing
		bAbort = true;
	}

	if ( pModelParts->nLoadedParts & ( 1 << ModelParts_t::BUFFER_PHY ) )
	{
		DEBUG_SCOPE_TIMER(phy);
		// regardless of error, call job callback so caller can do cleanup of their context
		pData = pModelParts->Buffers[ModelParts_t::BUFFER_PHY].Base();
		nSize = bAbort ? 0 : pModelParts->Buffers[ModelParts_t::BUFFER_PHY].TellMaxPut();
		ProcessDataIntoCache( handle, MDLCACHE_VCOLLIDE, 0, pData, nSize, nSize != 0 );
		g_pFullFileSystem->FreeOptimalReadBuffer( pData );
	}

	// vvd vertexes before vtx
	if ( pModelParts->nLoadedParts & ( 1 << ModelParts_t::BUFFER_VVD ) )
	{
		DEBUG_SCOPE_TIMER(vvd);
		pData = pModelParts->Buffers[ModelParts_t::BUFFER_VVD].Base();
		nSize = bAbort ? 0 : pModelParts->Buffers[ModelParts_t::BUFFER_VVD].TellMaxPut();
		ProcessDataIntoCache( handle, MDLCACHE_VERTEXES, 0, pData, nSize, nSize != 0 );
		g_pFullFileSystem->FreeOptimalReadBuffer( pData );
	}

	// can construct meshes after vvd and vtx vertexes arrive
	if ( pModelParts->nLoadedParts & ( 1 << ModelParts_t::BUFFER_VTX ) )
	{
		DEBUG_SCOPE_TIMER(vtx);
		pData = pModelParts->Buffers[ModelParts_t::BUFFER_VTX].Base();
		nSize = bAbort ?  0 : pModelParts->Buffers[ModelParts_t::BUFFER_VTX].TellMaxPut();

		// ProcessDataIntoCache() will do an unlock, so lock
		studiodata_t *pStudioData = m_MDLDict[handle];
		GetCacheSection( MDLCACHE_STUDIOHWDATA )->Lock( pStudioData->m_VertexCache );
		{
			// constructing the static meshes isn't thread safe
			AUTO_LOCK( m_QueuedLoadingMutex );
			ProcessDataIntoCache( handle, MDLCACHE_STUDIOHWDATA, 0, pData, nSize, nSize != 0 );
		}
		g_pFullFileSystem->FreeOptimalReadBuffer( pData );
	}

	UnlockStudioHdr( handle );
	delete pModelParts;
}

//-----------------------------------------------------------------------------
// Journals each of the incoming MDL components until all arrive (or error).
// Not all components exist, but that information is not known at job submission.
//-----------------------------------------------------------------------------
void CMDLCache::QueuedLoaderCallback_MDL( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError )
{
	// validity is denoted by a nonzero buffer
	nSize = ( loaderError == LOADERERROR_NONE ) ? nSize : 0;

	// journal each incoming buffer
	ModelParts_t *pModelParts = (ModelParts_t *)pContext;
	ModelParts_t::BufferType_t bufferType = static_cast< ModelParts_t::BufferType_t >((int)pContext2);
	pModelParts->Buffers[bufferType].SetExternalBuffer( (void *)pData, nSize, nSize, CUtlBuffer::READ_ONLY );
	pModelParts->nLoadedParts += (1 << bufferType);

	// wait for all components
	if ( pModelParts->DoFinalProcessing() )
	{
		if ( !IsPC() )
		{
			// now have all components, process the raw data into the cache
			g_MDLCache.ProcessQueuedData( pModelParts );
		}
		else
		{
			// PC background load path. pull in material dependencies on the fly.
			Assert( ThreadInMainThread() );

			g_MDLCache.ProcessQueuedData( pModelParts, true );

			// preload all possible paths to VMTs
			{
				DEBUG_SCOPE_TIMER(findvmt);
				MaterialLock_t hMatLock = materials->Lock();
				if ( studiohdr_t * pHdr = g_MDLCache.GetStudioHdr( pModelParts->hMDL ) )
				{
					if ( !(pHdr->flags & STUDIOHDR_FLAGS_OBSOLETE) )
					{
						char buf[MAX_PATH];
						V_strcpy( buf, "materials/" );
						int prefixLen = V_strlen( buf );
						
						for ( int t = 0; t < pHdr->numtextures; ++t )
						{
							// XXX this does not take remaps from vtxdata into account;
							// right now i am not caring about that. we will hitch if any
							// LODs remap to materials that are not in the header. (henryg)
							const char *pTexture = pHdr->pTexture(t)->pszName();
							pTexture += ( pTexture[0] == CORRECT_PATH_SEPARATOR || pTexture[0] == INCORRECT_PATH_SEPARATOR );
							for ( int cd = 0; cd < pHdr->numcdtextures; ++cd )
							{
								const char *pCdTexture = pHdr->pCdtexture( cd );
								pCdTexture += ( pCdTexture[0] == CORRECT_PATH_SEPARATOR || pCdTexture[0] == INCORRECT_PATH_SEPARATOR );
								V_ComposeFileName( pCdTexture, pTexture, buf + prefixLen, MAX_PATH - prefixLen );
								V_strncat( buf, ".vmt", MAX_PATH, COPY_ALL_CHARACTERS );
								pModelParts->bMaterialsPending = true;
								const char *pbuf = buf;
								g_pFullFileSystem->AddFilesToFileCache( pModelParts->hFileCache, &pbuf, 1, "GAME" );
								if ( materials->IsMaterialLoaded( buf + prefixLen ) )
								{
									// found a loaded one. still cache it in case it unloads,
									// but we can stop adding more potential paths to the cache
									// since this one is known to be valid.
									break;
								}
							}
						}
					}
				}
				materials->Unlock(hMatLock);
			}

			// queue functor which will start polling every frame by re-queuing itself
			g_pQueuedLoader->QueueDynamicLoadFunctor( CreateFunctor( ProcessDynamicLoad, pModelParts ) );
		}
	}
}

void CMDLCache::ProcessDynamicLoad( ModelParts_t *pModelParts )
{
	Assert( IsPC() && ThreadInMainThread() );

	if ( !g_pFullFileSystem->IsFileCacheLoaded( pModelParts->hFileCache ) )
	{
		// poll again next frame...
		g_pQueuedLoader->QueueDynamicLoadFunctor( CreateFunctor( ProcessDynamicLoad, pModelParts ) );
		return;
	}

	if ( pModelParts->bMaterialsPending )
	{
		DEBUG_SCOPE_TIMER(processvmt);
		pModelParts->bMaterialsPending = false;
		pModelParts->bTexturesPending = true;

		MaterialLock_t hMatLock = materials->Lock();
		materials->SetAsyncTextureLoadCache( pModelParts->hFileCache );

		// Load all the materials
		studiohdr_t * pHdr = g_MDLCache.GetStudioHdr( pModelParts->hMDL );
		if ( pHdr && !(pHdr->flags & STUDIOHDR_FLAGS_OBSOLETE) )
		{
			// build strings inside a buffer that already contains a materials/ prefix
			char buf[MAX_PATH];
			V_strcpy( buf, "materials/" );
			int prefixLen = V_strlen( buf );

			// XXX this does not take remaps from vtxdata into account;
			// right now i am not caring about that. we will hitch if any
			// LODs remap to materials that are not in the header. (henryg)
			for ( int t = 0; t < pHdr->numtextures; ++t )
			{
				const char *pTexture = pHdr->pTexture(t)->pszName();
				pTexture += ( pTexture[0] == CORRECT_PATH_SEPARATOR || pTexture[0] == INCORRECT_PATH_SEPARATOR );
				for ( int cd = 0; cd < pHdr->numcdtextures; ++cd )
				{
					const char *pCdTexture = pHdr->pCdtexture( cd );
					pCdTexture += ( pCdTexture[0] == CORRECT_PATH_SEPARATOR || pCdTexture[0] == INCORRECT_PATH_SEPARATOR );
					V_ComposeFileName( pCdTexture, pTexture, buf + prefixLen, MAX_PATH - prefixLen );
					IMaterial* pMaterial = materials->FindMaterial( buf + prefixLen, TEXTURE_GROUP_MODEL, false );
					if ( !IsErrorMaterial( pMaterial ) && !pMaterial->IsPrecached() )
					{
						pModelParts->Materials.AddToTail( pMaterial );
						pMaterial->IncrementReferenceCount();
						// Force texture loads while material system is set to capture
						// them and redirect to an error texture... this will populate
						// the file cache with all the requested textures
						pMaterial->RefreshPreservingMaterialVars();
						break;
					}
				}
			}
		}

		materials->SetAsyncTextureLoadCache( NULL );
		materials->Unlock( hMatLock );

		// poll again next frame... dont want to do too much work right now
		g_pQueuedLoader->QueueDynamicLoadFunctor( CreateFunctor( ProcessDynamicLoad, pModelParts ) );
		return;
	}
	
	if ( pModelParts->bTexturesPending )
	{
		DEBUG_SCOPE_TIMER(matrefresh);
		pModelParts->bTexturesPending = false;

		// Perform the real material loads now while raw texture files are cached.
		FOR_EACH_VEC( pModelParts->Materials, i )
		{
			IMaterial* pMaterial = pModelParts->Materials[i];
			if ( !IsErrorMaterial( pMaterial ) && pMaterial->IsPrecached() )
			{
				// Do a full reload to get the correct textures and computed flags
				pMaterial->Refresh();
			}
		}

		// poll again next frame... dont want to do too much work right now
		g_pQueuedLoader->QueueDynamicLoadFunctor( CreateFunctor( ProcessDynamicLoad, pModelParts ) );
		return;
	}

	// done. finish and clean up.
	Assert( !pModelParts->bTexturesPending && !pModelParts->bMaterialsPending );

	// pull out cached items we want to overlap with final processing
	CleanupModelParts_t *pCleanup = new CleanupModelParts_t;
	pCleanup->hFileCache = pModelParts->hFileCache;
	pCleanup->Materials.Swap( pModelParts->Materials );
	g_pQueuedLoader->QueueCleanupDynamicLoadFunctor( CreateFunctor( CleanupDynamicLoad, pCleanup ) );

	{
		DEBUG_SCOPE_TIMER(processall);
		g_MDLCache.ProcessQueuedData( pModelParts ); // pModelParts is deleted here
	}
}

void CMDLCache::CleanupDynamicLoad( CleanupModelParts_t *pCleanup )
{
	Assert( IsPC() && ThreadInMainThread() );

	// remove extra material refs, unload cached files
	FOR_EACH_VEC( pCleanup->Materials, i )
	{
		pCleanup->Materials[i]->DecrementReferenceCount();
	}

	g_pFullFileSystem->DestroyFileCache( pCleanup->hFileCache );

	delete pCleanup;
}

//-----------------------------------------------------------------------------
// Build a queued loader job to get the MDL ant all of its components into the cache.
//-----------------------------------------------------------------------------
bool CMDLCache::PreloadModel( MDLHandle_t handle )
{
	if ( g_pQueuedLoader->IsDynamic() == false )
	{
		if ( !IsX360() )
		{
			return false;
		}

		if ( !g_pQueuedLoader->IsMapLoading() || handle == MDLHANDLE_INVALID )
		{
			return false;
		}
	}

	if ( !g_pQueuedLoader->IsBatching() )
	{
		// batching must be active, following code depends on its behavior
		DevWarning( "CMDLCache:: Late preload of model '%s'\n", GetModelName( handle ) );
		return false;
	}

	// determine existing presence
	// actual necessity is not established here, allowable absent files need their i/o error to occur
	bool bNeedsMDL = !IsDataLoaded( handle, MDLCACHE_STUDIOHDR );
	bool bNeedsVTX = !IsDataLoaded( handle, MDLCACHE_STUDIOHWDATA );
	bool bNeedsVVD = !IsDataLoaded( handle, MDLCACHE_VERTEXES );
	bool bNeedsPHY = !IsDataLoaded( handle, MDLCACHE_VCOLLIDE );
	if ( !bNeedsMDL && !bNeedsVTX && !bNeedsVVD && !bNeedsPHY )
	{
		// nothing to do
		return true;
	}

	char szFilename[MAX_PATH];
	char szNameOnDisk[MAX_PATH];
	V_strncpy( szFilename, GetActualModelName( handle ), sizeof( szFilename ) );
	V_StripExtension( szFilename, szFilename, sizeof( szFilename ) );

	// need to gather all model parts (mdl, vtx, vvd, phy, ani)
	ModelParts_t *pModelParts = new ModelParts_t;
	pModelParts->hMDL = handle;
	pModelParts->hFileCache = g_pFullFileSystem->CreateFileCache();

	// create multiple loader jobs to perform gathering i/o operations
	LoaderJob_t loaderJob;
	loaderJob.m_pPathID = "GAME";
	loaderJob.m_pCallback = QueuedLoaderCallback_MDL;
	loaderJob.m_pContext = (void *)pModelParts;
	loaderJob.m_Priority = LOADERPRIORITY_DURINGPRELOAD;
	loaderJob.m_bPersistTargetData = true;

	if ( bNeedsMDL )
	{
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s.mdl", szFilename, GetPlatformExt() );
		loaderJob.m_pFilename = szNameOnDisk;
		loaderJob.m_pContext2 = (void *)ModelParts_t::BUFFER_MDL;
		g_pQueuedLoader->AddJob( &loaderJob );
		pModelParts->nExpectedParts |= 1 << ModelParts_t::BUFFER_MDL;
	}

	if ( bNeedsVTX )
	{
		// vtx extensions are .xxx.vtx, need to re-form as, ???.xxx.yyy.vtx
		char szTempName[MAX_PATH];
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s", szFilename, GetVTXExtension() );
		V_StripExtension( szNameOnDisk, szTempName, sizeof( szTempName ) );
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s.vtx", szTempName, GetPlatformExt() );
		loaderJob.m_pFilename = szNameOnDisk;
		loaderJob.m_pContext2 = (void *)ModelParts_t::BUFFER_VTX;
		g_pQueuedLoader->AddJob( &loaderJob );
		pModelParts->nExpectedParts |= 1 << ModelParts_t::BUFFER_VTX;
	}

	if ( bNeedsVVD )
	{
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s.vvd", szFilename, GetPlatformExt() );
		loaderJob.m_pFilename = szNameOnDisk;
		loaderJob.m_pContext2 = (void *)ModelParts_t::BUFFER_VVD;
		g_pQueuedLoader->AddJob( &loaderJob );
		pModelParts->nExpectedParts |= 1 << ModelParts_t::BUFFER_VVD;
	}

	if ( bNeedsPHY )
	{
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s.phy", szFilename, GetPlatformExt() );
		loaderJob.m_pFilename = szNameOnDisk;
		loaderJob.m_pContext2 = (void *)ModelParts_t::BUFFER_PHY;
		g_pQueuedLoader->AddJob( &loaderJob );
		pModelParts->nExpectedParts |= 1 << ModelParts_t::BUFFER_PHY;
	}

	if ( !pModelParts->nExpectedParts )
	{
		g_pFullFileSystem->DestroyFileCache( pModelParts->hFileCache );
		delete pModelParts;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Clear the STUDIODATA_ERROR_MODEL flag.
//-----------------------------------------------------------------------------
void CMDLCache::ResetErrorModelStatus( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return;

	m_MDLDict[handle]->m_nFlags &= ~STUDIODATA_ERROR_MODEL;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMDLCache::MarkFrame()
{
	ProcessPendingAsyncs();
}

//-----------------------------------------------------------------------------
// Purpose: bind studiohdr_t support functions to the mdlcacher
//-----------------------------------------------------------------------------
const studiohdr_t *studiohdr_t::FindModel( void **cache, char const *pModelName ) const
{
	MDLHandle_t handle = g_MDLCache.FindMDL( pModelName );
	*cache = (void*)(uintp)handle;
	return g_MDLCache.GetStudioHdr( handle );
}

virtualmodel_t *studiohdr_t::GetVirtualModel( void ) const
{
	if (numincludemodels == 0)
		return NULL;

	return g_MDLCache.GetVirtualModelFast( this, (MDLHandle_t)(int)virtualModel&0xffff );
}

byte *studiohdr_t::GetAnimBlock( int i ) const
{
	return g_MDLCache.GetAnimBlock( (MDLHandle_t)(int)virtualModel&0xffff, i );
}

int studiohdr_t::GetAutoplayList( unsigned short **pOut ) const
{
	return g_MDLCache.GetAutoplayList( (MDLHandle_t)(int)virtualModel&0xffff, pOut );
}

const studiohdr_t *virtualgroup_t::GetStudioHdr( void ) const
{
	return g_MDLCache.GetStudioHdr( (MDLHandle_t)(int)cache&0xffff );
}

