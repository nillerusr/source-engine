//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface to Xbox 360 system functions. Helps deal with the async system and Live
//			functions by either providing a handle for the caller to check results or handling
//			automatic cleanup of the async data when the caller doesn't care about the results.
//
//=====================================================================================//

#include "host.h"
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"
#include "ixboxsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static wchar_t g_szModSaveContainerDisplayName[XCONTENT_MAX_DISPLAYNAME_LENGTH] = L"";
static char g_szModSaveContainerName[XCONTENT_MAX_FILENAME_LENGTH] = "";

//-----------------------------------------------------------------------------
// Implementation of IXboxSystem interface
//-----------------------------------------------------------------------------
class CXboxSystem : public IXboxSystem
{
public:
	CXboxSystem( void );

	virtual	~CXboxSystem( void );

	virtual AsyncHandle_t	CreateAsyncHandle( void );
	virtual void			ReleaseAsyncHandle( AsyncHandle_t handle );
	virtual int				GetOverlappedResult( AsyncHandle_t handle, uint *pResultCode, bool bWait );
	virtual void			CancelOverlappedOperation( AsyncHandle_t handle );

	// Save/Load
	virtual void			GetModSaveContainerNames( const char *pchModName, const wchar_t **ppchDisplayName, const char **ppchName );
	virtual uint			GetContainerRemainingSpace( void );
	virtual bool			DeviceCapacityAdequate( DWORD nDeviceID, const char *pModName );
	virtual DWORD			DiscoverUserData( DWORD nUserID, const char *pModName );

	// XUI
	virtual bool			ShowDeviceSelector( bool bForce, uint *pStorageID, AsyncHandle_t *pHandle );
	virtual void			ShowSigninUI( uint nPanes, uint nFlags );

	// Rich Presence and Matchmaking
	virtual int				UserSetContext( uint nUserIdx, uint nContextID, uint nContextValue, bool bAsync, AsyncHandle_t *pHandle);
	virtual int				UserSetProperty( uint nUserIndex, uint nPropertyId, uint nBytes, const void *pvValue, bool bAsync, AsyncHandle_t *pHandle );

	// Matchmaking
	virtual int				CreateSession( uint nFlags, uint nUserIdx, uint nMaxPublicSlots, uint nMaxPrivateSlots, uint64 *pNonce, void *pSessionInfo, XboxHandle_t *pSessionHandle, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual uint			DeleteSession( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL );
	virtual uint			SessionSearch( uint nProcedureIndex, uint nUserIndex, uint nNumResults, uint nNumUsers, uint nNumProperties, uint nNumContexts, XUSER_PROPERTY *pSearchProperties, XUSER_CONTEXT *pSearchContexts, uint *pcbResultsBuffer, XSESSION_SEARCHRESULT_HEADER *pSearchResults, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual uint			SessionStart( XboxHandle_t hSession, uint nFlags, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual uint			SessionEnd( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionJoinLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, const bool *pPrivateSlots, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionJoinRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, const bool *pPrivateSlot, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionLeaveLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionLeaveRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionMigrate( XboxHandle_t hSession, uint nUserIndex, void *pSessionInfo, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionArbitrationRegister( XboxHandle_t hSession, uint nFlags, uint64 nonce, uint *pBytes, void *pBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle );

	// Stats
	virtual int				WriteStats( XboxHandle_t hSession, XUID xuid, uint nViews, void* pViews, bool bAsync, AsyncHandle_t *pAsyncHandle );

	// Achievements
	virtual int				EnumerateAchievements( uint nUserIdx, uint64 xuid, uint nStartingIdx, uint nCount, void *pBuffer, uint nBufferBytes, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual void			AwardAchievement( uint nUserIdx, uint nAchievementId );
	
	virtual void			FinishContainerWrites( void );
	virtual uint			GetContainerOpenResult( void );
	virtual uint			OpenContainers( void );
	virtual void			CloseContainers( void );

private:
	virtual uint			CreateSavegameContainer( uint nCreationFlags );
	virtual uint			CreateUserSettingsContainer( uint nCreationFlags );

	uint					m_OpenContainerResult;
};

static CXboxSystem s_XboxSystem;
IXboxSystem *g_pXboxSystem = &s_XboxSystem;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CXboxSystem, IXboxSystem, XBOXSYSTEM_INTERFACE_VERSION, s_XboxSystem );

#define ASYNC_RESULT(ph) 	((AsyncResult_t*)*ph);

#if defined( _X360 )
//-----------------------------------------------------------------------------
// Holds the overlapped object and any persistent data for async system calls
//-----------------------------------------------------------------------------
typedef struct AsyncResult_s
{
	XOVERLAPPED		overlapped;
	bool			bAutoRelease;
	void			*pInputData;
	AsyncResult_s	*pNext;
} AsyncResult_t;

static AsyncResult_t * g_pAsyncResultHead = NULL;

//-----------------------------------------------------------------------------
// Purpose: Remove an AsyncResult_t from the list
//-----------------------------------------------------------------------------
static void ReleaseAsyncResult( AsyncResult_t *pAsyncResult )
{
	if ( pAsyncResult == g_pAsyncResultHead )
	{
		g_pAsyncResultHead = pAsyncResult->pNext;
		free( pAsyncResult->pInputData );
		delete pAsyncResult;
		return;
	}

	AsyncResult_t *pNode = g_pAsyncResultHead;
	while ( pNode->pNext )
	{
		if ( pNode->pNext == pAsyncResult )
		{
			pNode->pNext = pAsyncResult->pNext;
			free( pAsyncResult->pInputData );
			delete pAsyncResult;
			return;
		}
		pNode = pNode->pNext;
	}
	Warning( "AsyncResult_t not found in ReleaseAsyncResult.\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Remove an AsyncResult_t from the list
//-----------------------------------------------------------------------------
static void ReleaseAsyncResult( XOVERLAPPED *pOverlapped )
{
	AsyncResult_t *pResult = g_pAsyncResultHead;
	while ( pResult )
	{
		if ( &pResult->overlapped == pOverlapped )
		{
			ReleaseAsyncResult( pResult );
			return;
		}
	}
	Warning( "XOVERLAPPED couldn't be found in ReleaseAsyncResult.\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Release async results that were marked for auto-release.
//-----------------------------------------------------------------------------
static void CleanupFinishedAsyncResults()
{
	AsyncResult_t *pResult = g_pAsyncResultHead;
	AsyncResult_t *pNext;
	while( pResult )
	{
		pNext = pResult->pNext;
		if ( pResult->bAutoRelease )
		{
			if ( XHasOverlappedIoCompleted( &pResult->overlapped ) )
			{
				ReleaseAsyncResult( pResult );
			}
		}
		pResult = pNext;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add a new AsyncResult_t object to the list
//-----------------------------------------------------------------------------
static AsyncResult_t *CreateAsyncResult( bool bAutoRelease )
{
	// Take this opportunity to clean up finished operations
	CleanupFinishedAsyncResults();

	AsyncResult_t *pAsyncResult = new AsyncResult_t;
	memset( pAsyncResult, 0, sizeof( AsyncResult_t ) );

	pAsyncResult->pNext = g_pAsyncResultHead;
	g_pAsyncResultHead = pAsyncResult;

	if ( bAutoRelease )
	{
		pAsyncResult->bAutoRelease = true;
	}

	return pAsyncResult;
}

//-----------------------------------------------------------------------------
// Purpose: Return an AsyncResult_t object to the pool
//-----------------------------------------------------------------------------
static void InitializeAsyncHandle( AsyncHandle_t *pHandle )
{
	XOVERLAPPED *pOverlapped = &((AsyncResult_t *)*pHandle)->overlapped;
	memset( pOverlapped, 0, sizeof( XOVERLAPPED ) );
}

//-----------------------------------------------------------------------------
// Purpose: Initialize or create and async handle
//-----------------------------------------------------------------------------
static AsyncResult_t *InitializeAsyncResult( AsyncHandle_t **ppAsyncHandle )
{
	AsyncResult_t *pResult = NULL;
	if ( *ppAsyncHandle )
	{
		InitializeAsyncHandle( *ppAsyncHandle );
		pResult = ASYNC_RESULT( *ppAsyncHandle );
	}
	else
	{
		// No handle provided, create one
		pResult = CreateAsyncResult( true );
	}
	return pResult;
}

CXboxSystem::CXboxSystem( void ) : m_OpenContainerResult( ERROR_SUCCESS )
{
}

//-----------------------------------------------------------------------------
// Purpose: Force overlapped operations to finish and clean up
//-----------------------------------------------------------------------------
CXboxSystem::~CXboxSystem()
{
	// Force async operations to finish.
	AsyncResult_t *pResult = g_pAsyncResultHead;
	while ( pResult )
	{
		AsyncResult_t *pNext = pResult->pNext;
		GetOverlappedResult( (AsyncHandle_t)pResult, NULL, true );
		pResult = pNext;
	}

	// Release any remaining handles - should have been released by the client that created them.
	int ct = 0;
	while ( g_pAsyncResultHead )
	{
		ReleaseAsyncResult( g_pAsyncResultHead );
		++ct;
	}

	if ( ct )
	{
		Warning( "Released %d async handles\n", ct );
	}
}

//-----------------------------------------------------------------------------
//	Purpose: Check on the result of an overlapped operation
//-----------------------------------------------------------------------------
int CXboxSystem::GetOverlappedResult( AsyncHandle_t handle, uint *pResultCode, bool bWait )
{
	if ( !handle )
		return ERROR_INVALID_HANDLE;

	return XGetOverlappedResult( &((AsyncResult_t*)handle)->overlapped, (DWORD*)pResultCode, bWait );
}

//-----------------------------------------------------------------------------
//	Purpose: Cancel an overlapped operation
//-----------------------------------------------------------------------------
void CXboxSystem::CancelOverlappedOperation( AsyncHandle_t handle )
{
	XCancelOverlapped( &((AsyncResult_t*)handle)->overlapped );
}

//-----------------------------------------------------------------------------
// Purpose: Create a new AsyncHandle_t
//-----------------------------------------------------------------------------
AsyncHandle_t CXboxSystem::CreateAsyncHandle( void )
{
	return (AsyncHandle_t)CreateAsyncResult( false );
}

//-----------------------------------------------------------------------------
// Purpose: Delete an AsyncHandle_t
//-----------------------------------------------------------------------------
void CXboxSystem::ReleaseAsyncHandle( AsyncHandle_t handle )
{
	ReleaseAsyncResult( (AsyncResult_t*)handle );
}

//-----------------------------------------------------------------------------
// Purpose: Close the open containers
//-----------------------------------------------------------------------------
void CXboxSystem::CloseContainers( void )
{
	XContentClose( GetCurrentMod(), NULL );
	XContentClose( XBX_USER_SETTINGS_CONTAINER_DRIVE, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
uint CXboxSystem::OpenContainers( void )
{
	// Close the containers (force dismount)
	CloseContainers();

	m_OpenContainerResult = ERROR_SUCCESS;

	// Open the save games
	if ( ( m_OpenContainerResult = CreateUserSettingsContainer( XCONTENTFLAG_OPENALWAYS ) ) != ERROR_SUCCESS )
		return m_OpenContainerResult;

	// If we're TF, we don't care about save game space
	if ( !Q_stricmp( GetCurrentMod(), "tf" ) )
		return m_OpenContainerResult;

	// Open the user settings
	if ( ( m_OpenContainerResult = CreateSavegameContainer( XCONTENTFLAG_OPENALWAYS ) ) != ERROR_SUCCESS )
	{
		CloseContainers();
		return m_OpenContainerResult;
	}
	
	return m_OpenContainerResult;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the results from the last container opening
//-----------------------------------------------------------------------------
uint CXboxSystem::GetContainerOpenResult( void )
{
	return m_OpenContainerResult;
}

//-----------------------------------------------------------------------------
//	Purpose: Open the save game container for the current mod
//-----------------------------------------------------------------------------
uint CXboxSystem::CreateSavegameContainer( uint nCreationFlags )
{
	if ( XBX_GetStorageDeviceId() == XBX_INVALID_STORAGE_ID || XBX_GetStorageDeviceId() == XBX_STORAGE_DECLINED )
		return ERROR_INVALID_HANDLE;

	// Don't allow any of our saves or user data to be transferred to another user
	nCreationFlags |= XCONTENTFLAG_NOPROFILE_TRANSFER;

	const wchar_t *pchContainerDisplayName;
	const char *pchContainerName;
	g_pXboxSystem->GetModSaveContainerNames( GetCurrentMod(), &pchContainerDisplayName, &pchContainerName );

	XCONTENT_DATA contentData;
	contentData.DeviceID = XBX_GetStorageDeviceId();
	contentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
	Q_wcsncpy( contentData.szDisplayName, pchContainerDisplayName, sizeof ( contentData.szDisplayName ) );
	Q_snprintf( contentData.szFileName, sizeof( contentData.szFileName ), pchContainerName );

	SIZE_T dwFileCacheSize = 0; // Use the smallest size (default)
	ULARGE_INTEGER ulSize;
	ulSize.QuadPart = XBX_PERSISTENT_BYTES_NEEDED;

	int nRet = XContentCreateEx( XBX_GetPrimaryUserId(), GetCurrentMod(), &contentData, nCreationFlags, NULL, NULL, dwFileCacheSize, ulSize, NULL );
	if ( nRet == ERROR_SUCCESS )
	{
		BOOL bUserIsCreator = false;
		XContentGetCreator( XBX_GetPrimaryUserId(), &contentData, &bUserIsCreator, NULL, NULL );
		if( bUserIsCreator == false )
		{
			XContentClose( GetCurrentMod(), NULL );
			return ERROR_ACCESS_DENIED;
		}
	}

	return nRet;
}

//-----------------------------------------------------------------------------
//	Purpose: Open the user settings container for the current mod
//-----------------------------------------------------------------------------
uint CXboxSystem::CreateUserSettingsContainer( uint nCreationFlags )
{
	if ( XBX_GetStorageDeviceId() == XBX_INVALID_STORAGE_ID || XBX_GetStorageDeviceId() == XBX_STORAGE_DECLINED )
		return ERROR_INVALID_HANDLE;

	// Don't allow any of our saves or user data to be transferred to another user
	nCreationFlags |= XCONTENTFLAG_NOPROFILE_TRANSFER;

	XCONTENT_DATA contentData;
	contentData.DeviceID = XBX_GetStorageDeviceId();
	contentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
	Q_wcsncpy( contentData.szDisplayName, g_pVGuiLocalize->Find( "#GameUI_Console_UserSettings" ), sizeof( contentData.szDisplayName ) );
	Q_snprintf( contentData.szFileName, sizeof( contentData.szFileName ), "UserSettings" );

	SIZE_T dwFileCacheSize = 0; // Use the smallest size (default)
	ULARGE_INTEGER ulSize;
	ulSize.QuadPart = XBX_USER_SETTINGS_BYTES;

	int nRet = XContentCreateEx( XBX_GetPrimaryUserId(), XBX_USER_SETTINGS_CONTAINER_DRIVE, &contentData, nCreationFlags, NULL, NULL, dwFileCacheSize, ulSize, NULL );
	if ( nRet == ERROR_SUCCESS )
	{
		BOOL bUserIsCreator = false;
		XContentGetCreator( XBX_GetPrimaryUserId(), &contentData, &bUserIsCreator, NULL, NULL );
		if( bUserIsCreator == false )
		{
			XContentClose( XBX_USER_SETTINGS_CONTAINER_DRIVE, NULL );
			return ERROR_ACCESS_DENIED;
		}
	}

	return nRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CXboxSystem::FinishContainerWrites( void )
{
	// Finish all writes
	XContentFlush( GetCurrentMod(), NULL );
	XContentFlush( XBX_USER_SETTINGS_CONTAINER_DRIVE, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve the names used for our save game container
// Input  : *pchModName - Name of the mod we're running (tf, hl2, etc)
//			**ppchDisplayName - Display name that will be presented to users by the console
//			**ppchName - Filename of the container
//-----------------------------------------------------------------------------
void CXboxSystem::GetModSaveContainerNames( const char *pchModName, const wchar_t **ppchDisplayName, const char **ppchName )
{
	// If the strings haven't been setup
	if ( g_szModSaveContainerDisplayName[ 0 ] == '\0' )
	{
		if ( Q_stricmp( pchModName, "episodic" ) == 0 )
		{
			Q_wcsncpy( g_szModSaveContainerDisplayName, g_pVGuiLocalize->Find( "#GameUI_Console_Ep1_Saves" ), sizeof( g_szModSaveContainerDisplayName ) );
		}
		else if ( Q_stricmp( pchModName, "ep2" ) == 0 )
		{
			Q_wcsncpy( g_szModSaveContainerDisplayName, g_pVGuiLocalize->Find( "#GameUI_Console_Ep2_Saves" ), sizeof( g_szModSaveContainerDisplayName ) );
		}
		else if ( Q_stricmp( pchModName, "portal" ) == 0 )
		{
			Q_wcsncpy( g_szModSaveContainerDisplayName, g_pVGuiLocalize->Find( "#GameUI_Console_Portal_Saves" ), sizeof( g_szModSaveContainerDisplayName ) );
		}
		else if ( Q_stricmp( pchModName, "tf" ) == 0 )
		{
			Q_wcsncpy( g_szModSaveContainerDisplayName, g_pVGuiLocalize->Find( "#GameUI_Console_TF2_Saves" ), sizeof( g_szModSaveContainerDisplayName ) );
		}
		else
		{
			Q_wcsncpy( g_szModSaveContainerDisplayName, g_pVGuiLocalize->Find( "#GameUI_Console_HL2_Saves" ), sizeof( g_szModSaveContainerDisplayName ) );
		}

		// Create a filename with the format "mod_saves"
		Q_snprintf( g_szModSaveContainerName, sizeof( g_szModSaveContainerName ), "%s_saves", pchModName );
	}

	// Return pointers to these internally kept strings
	*ppchDisplayName = g_szModSaveContainerDisplayName;
	*ppchName = g_szModSaveContainerName;
}

//-----------------------------------------------------------------------------
// Purpose: Search the device and find out if we have adequate space to start a game
// Input  : nStorageID - Device to check
//			*pModName - Name of the mod we want to check for
//-----------------------------------------------------------------------------
bool CXboxSystem::DeviceCapacityAdequate( DWORD nStorageID, const char *pModName )
{
	// If we don't have a valid user id, we can't poll the device
	if ( XBX_GetPrimaryUserId() == XBX_INVALID_USER_ID ) 
		return false;

	// Must be a valid storage device to poll
	if ( nStorageID == XBX_INVALID_STORAGE_ID  )
		return false;

	// Get the actual amount on the drive
	XDEVICE_DATA deviceData;
	if ( XContentGetDeviceData( nStorageID, &deviceData ) != ERROR_SUCCESS )
		return false;

	const ULONGLONG nSaveGameSize = XContentCalculateSize( XBX_PERSISTENT_BYTES_NEEDED, 1 );
	const ULONGLONG nUserSettingsSize = XContentCalculateSize( XBX_USER_SETTINGS_BYTES, 1 );
	bool bIsTF2 = ( !Q_stricmp( pModName, "tf" ) );
	ULONGLONG nTotalSpaceNeeded = ( bIsTF2 ) ? nUserSettingsSize : ( nSaveGameSize + nUserSettingsSize );
	ULONGLONG nAvailableSpace = deviceData.ulDeviceFreeBytes; // Take the first device's free space to compare this against
	
	// If they've already got enough space, early out
	if ( nAvailableSpace >= nTotalSpaceNeeded )
		return true;

	const int nNumItemsToRetrieve = 1;
	const int fContentFlags = XCONTENTFLAG_ENUM_EXCLUDECOMMON;

	// Save for queries against the storage devices
	const wchar_t *pchContainerDisplayName;
	const char *pchContainerName;
	GetModSaveContainerNames( pModName, &pchContainerDisplayName, &pchContainerName );

	// Look for a user settings block for all products
	DWORD nBufferSize;
	HANDLE hEnumerator;
	if ( XContentCreateEnumerator(	XBX_GetPrimaryUserId(), 
									nStorageID, 
									XCONTENTTYPE_SAVEDGAME, 
									fContentFlags, 
									nNumItemsToRetrieve, 
									&nBufferSize, 
									&hEnumerator ) == ERROR_SUCCESS )
	{
		// Allocate a buffer of the correct size
		BYTE *pBuffer = new BYTE[nBufferSize];
		if ( pBuffer == NULL )
			return XBX_INVALID_STORAGE_ID;

		char szFilename[XCONTENT_MAX_FILENAME_LENGTH+1];
		szFilename[XCONTENT_MAX_FILENAME_LENGTH] = 0;
		XCONTENT_DATA *pData = NULL;

		// Step through all items, looking for ones we care about
		DWORD nNumItems;
		while ( XEnumerate( hEnumerator, pBuffer, nBufferSize, &nNumItems, NULL ) == ERROR_SUCCESS )
		{
			// Grab the item in question
			pData = (XCONTENT_DATA *) pBuffer;

			// Safely store this away (null-termination is not guaranteed by the API!)
			memcpy( szFilename, pData->szFileName, XCONTENT_MAX_FILENAME_LENGTH );

			// See if this is our user settings file
			if ( !Q_stricmp( szFilename, "UserSettings" ) )
			{
				nTotalSpaceNeeded -= nUserSettingsSize;
			}
			else if ( bIsTF2 == false && !Q_stricmp( szFilename, pchContainerName ) )
			{
				nTotalSpaceNeeded -= nSaveGameSize;
			}
		}

		// Clean up
		delete[] pBuffer;
		CloseHandle( hEnumerator );
	}

	// Finally, check its complete size
	if ( nTotalSpaceNeeded <= nAvailableSpace )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Enumerate all devices and search for game data already present.  If only one device has it, we return it
// Input  : nUserID - User whose data we're searching for
//			*pModName - Name of the mod we're searching for
// Output : Device ID which contains our data (-1 if no data was found, or data resided on multiple devices)
//-----------------------------------------------------------------------------
DWORD CXboxSystem::DiscoverUserData( DWORD nUserID, const char *pModName )
{
	// If we're entering this function without a storage device, then we must pop the UI anyway to choose it!
	Assert( nUserID != XBX_INVALID_USER_ID );
	if ( nUserID == XBX_INVALID_USER_ID )
		return XBX_INVALID_STORAGE_ID;

	const int nNumItemsToRetrieve = 1;
	const int fContentFlags = XCONTENTFLAG_ENUM_EXCLUDECOMMON;
	DWORD nFoundDevice = XBX_INVALID_STORAGE_ID;

	// Save for queries against the storage devices
	const wchar_t *pchContainerDisplayName;
	const char *pchContainerName;
	GetModSaveContainerNames( pModName, &pchContainerDisplayName, &pchContainerName );

	const ULONGLONG nSaveGameSize = XContentCalculateSize( XBX_PERSISTENT_BYTES_NEEDED, 1 );
	const ULONGLONG nUserSettingsSize = XContentCalculateSize( XBX_USER_SETTINGS_BYTES, 1 );
	ULONGLONG nTotalSpaceNeeded = ( nSaveGameSize + nUserSettingsSize );
	ULONGLONG nAvailableSpace = 0; // Take the first device's free space to compare this against

	// Look for a user settings block for all products
	DWORD nBufferSize;
	HANDLE hEnumerator;
	if ( XContentCreateEnumerator(	nUserID, 
									XCONTENTDEVICE_ANY, // All devices we know about
									XCONTENTTYPE_SAVEDGAME, 
									fContentFlags, 
									nNumItemsToRetrieve, 
									&nBufferSize, 
									&hEnumerator ) == ERROR_SUCCESS )
	{
		// Allocate a buffer of the correct size
		BYTE *pBuffer = new BYTE[nBufferSize];
		if ( pBuffer == NULL )
			return XBX_INVALID_STORAGE_ID;

		char szFilename[XCONTENT_MAX_FILENAME_LENGTH+1];
		szFilename[XCONTENT_MAX_FILENAME_LENGTH] = 0;
		XCONTENT_DATA *pData = NULL;

		// Step through all items, looking for ones we care about
		DWORD nNumItems;
		while ( XEnumerate( hEnumerator, pBuffer, nBufferSize, &nNumItems, NULL ) == ERROR_SUCCESS )
		{
			// Grab the item in question
			pData = (XCONTENT_DATA *) pBuffer;

			// If they have multiple devices installed, then we must ask
			if ( nFoundDevice != XBX_INVALID_STORAGE_ID && nFoundDevice != pData->DeviceID )
			{
				// Clean up
				delete[] pBuffer;
				CloseHandle( hEnumerator );

				return XBX_INVALID_STORAGE_ID;
			}

			// Hold on to this device ID
			if ( nFoundDevice != pData->DeviceID )
			{
				nFoundDevice = pData->DeviceID;

				XDEVICE_DATA deviceData;
				if ( XContentGetDeviceData( nFoundDevice, &deviceData ) != ERROR_SUCCESS )
					continue;

				nAvailableSpace = deviceData.ulDeviceFreeBytes;
			}

			// Safely store this away (null-termination is not guaranteed by the API!)
			memcpy( szFilename, pData->szFileName, XCONTENT_MAX_FILENAME_LENGTH );

			// See if this is our user settings file
			if ( !Q_stricmp( szFilename, "UserSettings" ) )
			{
				nTotalSpaceNeeded -= nUserSettingsSize;
			}
			else if ( !Q_stricmp( szFilename, pchContainerName ) )
			{
				nTotalSpaceNeeded -= nSaveGameSize;
			}
		}

		// Clean up
		delete[] pBuffer;
		CloseHandle( hEnumerator );
	}

	// If we found nothing, then give up
	if ( nFoundDevice == XBX_INVALID_STORAGE_ID )
		return nFoundDevice;

	// Finally, check its complete size
	if ( nTotalSpaceNeeded <= nAvailableSpace )
		return nFoundDevice;

	return XBX_INVALID_STORAGE_ID;
}

//-----------------------------------------------------------------------------
// Purpose: Space free on the current device
//-----------------------------------------------------------------------------
uint CXboxSystem::GetContainerRemainingSpace( void )
{
	XDEVICE_DATA deviceData;
	if ( XContentGetDeviceData( XBX_GetStorageDeviceId(), &deviceData ) != ERROR_SUCCESS )
		return 0;

	return deviceData.ulDeviceFreeBytes;
}

//-----------------------------------------------------------------------------
//	Purpose: Show the storage device selector
//-----------------------------------------------------------------------------
bool CXboxSystem::ShowDeviceSelector( bool bForce, uint *pStorageID, AsyncHandle_t *pAsyncHandle  )
{
	AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );

	// We validate the size outside of this because we want to look inside our packages to see what's really free
	ULARGE_INTEGER bytes;
	bytes.QuadPart = XContentCalculateSize( XBX_PERSISTENT_BYTES_NEEDED + XBX_USER_SETTINGS_BYTES, 1 );

	DWORD showFlags = bForce ? XCONTENTFLAG_FORCE_SHOW_UI : 0;
	showFlags |= XCONTENTFLAG_MANAGESTORAGE;

	DWORD ret = XShowDeviceSelectorUI(	XBX_GetPrimaryUserId(), 
										XCONTENTTYPE_SAVEDGAME, 
										showFlags, 
										bytes, 
										(DWORD*) pStorageID, 
										&pResult->overlapped 
										);

	if ( ret != ERROR_IO_PENDING )
	{
		Msg( "Error showing device Selector UI\n" );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Show the user sign in screen
//-----------------------------------------------------------------------------
void CXboxSystem::ShowSigninUI( uint nPanes, uint nFlags )
{
	XShowSigninUI( nPanes, nFlags );
}

//-----------------------------------------------------------------------------
//	Purpose: Set a user context
//-----------------------------------------------------------------------------
int CXboxSystem::UserSetContext( uint nUserIdx, uint nContextID, uint nContextValue, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;
	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XUserSetContextEx( nUserIdx, nContextID, nContextValue, pOverlapped );
}

//-----------------------------------------------------------------------------
//	Purpose: Set a user property
//-----------------------------------------------------------------------------
int CXboxSystem::UserSetProperty( uint nUserIndex, uint nPropertyId, uint nBytes, const void *pvValue, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;
	const void *pData = pvValue;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );

		if ( nBytes && pvValue )
		{
			pResult->pInputData = malloc( nBytes );
			memcpy( pResult->pInputData, pvValue, nBytes );
		}
		else
		{
			nBytes = 0;
		}

		pOverlapped = &pResult->overlapped;
		pData = pResult->pInputData;
	}

	return XUserSetPropertyEx( nUserIndex, nPropertyId, nBytes, pData, pOverlapped );
}

//-----------------------------------------------------------------------------
//	Purpose: Create a matchmaking session
//-----------------------------------------------------------------------------
int CXboxSystem::CreateSession( uint nFlags, 
							    uint nUserIdx, 
								uint nMaxPublicSlots, 
								uint nMaxPrivateSlots, 
								uint64 *pNonce,  
								void *pSessionInfo,
								XboxHandle_t *pSessionHandle,
								bool bAsync,
								AsyncHandle_t *pAsyncHandle 
								)
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	// Create the session
	return XSessionCreate( nFlags, nUserIdx, nMaxPublicSlots, nMaxPrivateSlots, pNonce, (XSESSION_INFO*)pSessionInfo, pOverlapped, pSessionHandle );
}

//-----------------------------------------------------------------------------
//	Purpose: Destroy a matchmaking session
//-----------------------------------------------------------------------------
uint CXboxSystem::DeleteSession( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	// Delete the session
	uint ret = XSessionDelete( hSession, pOverlapped );
	CloseHandle( hSession );

	return ret;
}

//-----------------------------------------------------------------------------
//	Purpose: Create a matchmaking session
//-----------------------------------------------------------------------------
uint CXboxSystem::SessionSearch( uint nProcedureIndex,
								 uint nUserIndex,
								 uint nNumResults,
								 uint nNumUsers,
								 uint nNumProperties,
								 uint nNumContexts,
								 XUSER_PROPERTY *pSearchProperties,
								 XUSER_CONTEXT *pSearchContexts,
								 uint *pcbResultsBuffer,
								 XSESSION_SEARCHRESULT_HEADER *pSearchResults,
								 bool	bAsync,
								 AsyncHandle_t *pAsyncHandle
								 )
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	// Search for the session
	return XSessionSearchEx( nProcedureIndex, nUserIndex, nNumResults, nNumUsers, nNumProperties, nNumContexts, pSearchProperties, pSearchContexts, (DWORD*)pcbResultsBuffer, pSearchResults, pOverlapped );
}

//-----------------------------------------------------------------------------
//	Purpose: Starting a multiplayer game
//-----------------------------------------------------------------------------
uint CXboxSystem::SessionStart( XboxHandle_t hSession, uint nFlags, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionStart( hSession, nFlags, pOverlapped );
}

//-----------------------------------------------------------------------------
//	Purpose: Finished a multiplayer game
//-----------------------------------------------------------------------------
uint CXboxSystem::SessionEnd( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionEnd( hSession, pOverlapped );
}

//-----------------------------------------------------------------------------
//	Purpose: Join local users to a session
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionJoinLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, const bool *pPrivateSlots, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionJoinLocal( hSession, nUserCount, (DWORD*)pUserIndexes, (BOOL*)pPrivateSlots, pOverlapped );
}

//-----------------------------------------------------------------------------
//	Purpose: Join remote users to a session
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionJoinRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, const bool *pPrivateSlots, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionJoinRemote( hSession, nUserCount, pXuids, (BOOL*)pPrivateSlots, pOverlapped );
}

//-----------------------------------------------------------------------------
//	Purpose: Remove local users from a session
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionLeaveLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionLeaveLocal( hSession, nUserCount, (DWORD*)pUserIndexes, pOverlapped );
}

//-----------------------------------------------------------------------------
//	Purpose: Remove remote users from a session
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionLeaveRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionLeaveRemote( hSession, nUserCount, pXuids, pOverlapped );
}

//-----------------------------------------------------------------------------
//	Purpose: Migrate a session to a new host
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionMigrate( XboxHandle_t hSession, uint nUserIndex, void *pSessionInfo, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionMigrateHost( hSession, nUserIndex, (XSESSION_INFO*)pSessionInfo, pOverlapped );
}

//-----------------------------------------------------------------------------
//	Purpose: Register for arbitration
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionArbitrationRegister( XboxHandle_t hSession, uint nFlags, uint64 nonce, uint *pBytes, void *pBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionArbitrationRegister( hSession, nFlags, nonce, (DWORD*)pBytes, (XSESSION_REGISTRATION_RESULTS*)pBuffer, pOverlapped );
}

//-----------------------------------------------------------------------------
//	Purpose: Upload player stats to Xbox Live
//-----------------------------------------------------------------------------
int	CXboxSystem::WriteStats( XboxHandle_t hSession, XUID xuid, uint nViews, void* pViews, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionWriteStats( hSession, xuid, nViews, (XSESSION_VIEW_PROPERTIES*)pViews, pOverlapped );
}


//-----------------------------------------------------------------------------
//	Purpose: Enumerate a player's achievements
//-----------------------------------------------------------------------------
int CXboxSystem::EnumerateAchievements( uint nUserIdx, 
									    uint64 xuid, 
										uint nStartingIdx, 
										uint nCount, 
										void *pBuffer, 
										uint nBufferBytes,
										bool bAsync,
										AsyncHandle_t *pAsyncHandle 
										)
{
	HANDLE hEnumerator = INVALID_HANDLE_VALUE;
	DWORD ret = XUserCreateAchievementEnumerator( 0, nUserIdx, xuid, XACHIEVEMENT_DETAILS_ALL, nStartingIdx, nCount, (DWORD*)pBuffer, &hEnumerator );
	
	// Just looking for the buffer size needed
	if ( ret != ERROR_SUCCESS || nBufferBytes == 0 )
	{
		CloseHandle( hEnumerator );
		return ret;
	}

	if ( nBufferBytes < *(uint*)pBuffer )
	{
		Warning( "EnumerateAchievements: Buffer provided not large enough to hold results" );
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	DWORD items;
	ret = XEnumerate( hEnumerator, pBuffer, nBufferBytes, &items, pOverlapped );
	if ( ret != ERROR_SUCCESS )
	{
		Warning( "XEnumerate failed in EnumerateAchievements.\n" );
	}
	CloseHandle( hEnumerator );

	return items;
}

//-----------------------------------------------------------------------------
//	Purpose: Award an achievement to the current user
//-----------------------------------------------------------------------------
void CXboxSystem::AwardAchievement( uint nUserIdx, uint nAchievementId )
{
	AsyncResult_t *pResult = CreateAsyncResult( true );

	XUSER_ACHIEVEMENT ach;
	ach.dwUserIndex = nUserIdx;
	ach.dwAchievementId = nAchievementId;

	pResult->pInputData = malloc( sizeof( ach ) );
	Q_memcpy( pResult->pInputData, &ach, sizeof( ach ) );

	DWORD ret = XUserWriteAchievements( 1, (XUSER_ACHIEVEMENT*)pResult->pInputData, &pResult->overlapped );
	if ( ret != ERROR_IO_PENDING )
	{
		Warning( "XUserWriteAchievments failed.\n" );
	}
}

#else

// Stubbed interface for win32
CXboxSystem::~CXboxSystem( void ) {}
CXboxSystem::CXboxSystem( void ) {}
AsyncHandle_t	CXboxSystem::CreateAsyncHandle( void ) { return NULL; }
void			CXboxSystem::ReleaseAsyncHandle( AsyncHandle_t handle ) {}
int				CXboxSystem::GetOverlappedResult( AsyncHandle_t handle, uint *pResultCode, bool bWait ) { return 0; }
void			CXboxSystem::CancelOverlappedOperation( AsyncHandle_t handle ) {};
void			CXboxSystem::GetModSaveContainerNames( const char *pchModName, const wchar_t **ppchDisplayName, const char **ppchName ) 
{
	*ppchDisplayName = g_szModSaveContainerDisplayName;
	*ppchName = g_szModSaveContainerName;
}
DWORD			CXboxSystem::DiscoverUserData( DWORD nUserID, const char *pModName ) { return ((DWORD)-1); }
bool			CXboxSystem::DeviceCapacityAdequate( DWORD nDeviceID, const char *pModName ) { return true; }
uint			CXboxSystem::GetContainerRemainingSpace( void ) { return 0; }
bool			CXboxSystem::ShowDeviceSelector( bool bForce, uint *pStorageID, AsyncHandle_t *pHandle  ) { return false; }
void			CXboxSystem::ShowSigninUI( uint nPanes, uint nFlags ) {}
int				CXboxSystem::UserSetContext( uint nUserIdx, uint nContextID, uint nContextValue, bool bAsync, AsyncHandle_t *pHandle) { return 0; }
int				CXboxSystem::UserSetProperty( uint nUserIndex, uint nPropertyId, uint nBytes, const void *pvValue, bool bAsync, AsyncHandle_t *pHandle ) { return 0; }
int				CXboxSystem::CreateSession( uint nFlags, uint nUserIdx, uint nMaxPublicSlots, uint nMaxPrivateSlots, uint64 *pNonce, void *pSessionInfo, XboxHandle_t *pSessionHandle, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
uint			CXboxSystem::DeleteSession( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
uint			CXboxSystem::SessionSearch( uint nProcedureIndex, uint nUserIndex, uint nNumResults, uint nNumUsers, uint nNumProperties, uint nNumContexts, XUSER_PROPERTY *pSearchProperties, XUSER_CONTEXT *pSearchContexts, uint *pcbResultsBuffer, XSESSION_SEARCHRESULT_HEADER *pSearchResults, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
uint			CXboxSystem::SessionStart( XboxHandle_t hSession, uint nFlags, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; };
uint			CXboxSystem::SessionEnd( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; };
int				CXboxSystem::SessionJoinLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, const bool *pPrivateSlots, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::SessionJoinRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, const bool *pPrivateSlot, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::SessionLeaveLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::SessionLeaveRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::SessionMigrate( XboxHandle_t hSession, uint nUserIndex, void *pSessionInfo, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::SessionArbitrationRegister( XboxHandle_t hSession, uint nFlags, uint64 nonce, uint *pBytes, void *pBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::WriteStats( XboxHandle_t hSession, XUID xuid, uint nViews, void* pViews, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::EnumerateAchievements( uint nUserIdx, uint64 xuid, uint nStartingIdx, uint nCount, void *pBuffer, uint nBufferBytes, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
void			CXboxSystem::AwardAchievement( uint nUserIdx, uint nAchievementId ) {}
void			CXboxSystem::FinishContainerWrites( void ) {}
uint			CXboxSystem::GetContainerOpenResult( void ) { return 0; }
uint			CXboxSystem::OpenContainers( void ) { return 0; }
void			CXboxSystem::CloseContainers( void ) {}
uint			CXboxSystem::CreateSavegameContainer( uint nCreationFlags ) { return 0; }
uint			CXboxSystem::CreateUserSettingsContainer( uint nCreationFlags ) { return 0; }

#endif // defined _X360
