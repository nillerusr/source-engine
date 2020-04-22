//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Xbox Launch Routines.
//
//=====================================================================================//

#ifndef _XBOX_LAUNCH_H_
#define _XBOX_LAUNCH_H_

#pragma once

#ifndef _CERT
#pragma comment( lib, "xbdm.lib" )
#endif

// id and version are used to tag the data blob, currently only need a singe hardcoded id
// when the version and id don't match, the data blob is not ours
#define VALVE_LAUNCH_ID			(('V'<<24)|('A'<<16)|('L'<<8)|('V'<<0))
#define VALVE_LAUNCH_VERSION	1

// launch flags
#define LF_ISDEBUGGING			0x80000000	// set if session was active prior to launch
#define LF_INTERNALLAUNCH		0x00000001	// set if launch was internal (as opposed to dashboard)
#define LF_EXITFROMINSTALLER	0x00000002	// set if exit was from an installer
#define LF_EXITFROMGAME			0x00000004	// set if exit was from a game
#define LF_EXITFROMCHOOSER		0x00000008	// set if exit was from the chooser
#define LF_GAMERESTART			0x00000010	// set if game wants to restart self (skips appchooser)
#define LF_INVITERESTART		0x00000020	// set if game was invited from another app (launches TF and fires off session connect)

#pragma pack(1)
struct launchHeader_t
{
	unsigned int	id;
	unsigned int	version;
	unsigned int	flags;

	int				nStorageID;
	int				nUserID;
	int				bForceEnglish;
	XNKID			nInviteSessionID;
	
	// for caller defined data, occurs after this header
	// limited to slightly less than MAX_LAUNCH_DATA_SIZE
	unsigned int	nDataSize;
};
#pragma pack()

// per docs, no larger than MAX_LAUNCH_DATA_SIZE
union xboxLaunchData_t
{
	launchHeader_t	header;		
	char			data[MAX_LAUNCH_DATA_SIZE];
};

//--------------------------------------------------------------------------------------
// Simple class to wrap the peristsent launch payload.
//
// Can be used by an application that does not use tier0 (i.e. the launcher). 
// Primarily designed to be anchored in tier0, so multiple systems can easily query and
// set the persistent payload.
//--------------------------------------------------------------------------------------
class CXboxLaunch
{
public:
	CXboxLaunch()
	{
		ResetLaunchData();
	}

	void ResetLaunchData()
	{
		// invalid until established
		// nonzero identifies a valid payload
		m_LaunchDataSize = 0;

		m_Launch.header.id = 0;
		m_Launch.header.version = 0;
		m_Launch.header.flags = 0;

		m_Launch.header.nStorageID = XBX_INVALID_STORAGE_ID;
		m_Launch.header.nUserID = XBX_INVALID_USER_ID;
		m_Launch.header.bForceEnglish = false;

		memset( &m_Launch.header.nInviteSessionID, 0, sizeof( m_Launch.header.nInviteSessionID ) );

		m_Launch.header.nDataSize = 0;
	}

	// Returns how much space can be used by caller
	int MaxPayloadSize()
	{
		return sizeof( xboxLaunchData_t ) - sizeof( launchHeader_t );
	}

	bool SetLaunchData( void *pData, int dataSize, int flags = 0 )
	{
		if ( pData && dataSize && dataSize > MaxPayloadSize() )
		{
			// not enough room
			return false;
		}

		if ( pData && dataSize && dataSize <= MaxPayloadSize() )
		{
			memcpy( m_Launch.data + sizeof( launchHeader_t ), pData, dataSize );
			m_Launch.header.nDataSize = dataSize;
		}
		else
		{
			m_Launch.header.nDataSize = 0;
		}

		flags |= LF_INTERNALLAUNCH;
#if !defined( _CERT )
		if ( DmIsDebuggerPresent() )
		{
			flags |= LF_ISDEBUGGING;
		}
#endif
		m_Launch.header.id = VALVE_LAUNCH_ID;
		m_Launch.header.version = VALVE_LAUNCH_VERSION;
		m_Launch.header.flags = flags;

		XSetLaunchData( &m_Launch, MAX_LAUNCH_DATA_SIZE );

		// assume successful, mark as valid
		m_LaunchDataSize = MAX_LAUNCH_DATA_SIZE;

		return true;
	}

	//--------------------------------------------------------------------------------------
	// Returns TRUE if the launch data blob is available. FALSE otherwise.
	// Caller is expected to validate and interpret contents based on ID.
	//--------------------------------------------------------------------------------------
	bool GetLaunchData( unsigned int *pID, void **pData, int *pDataSize )
	{
		if ( !m_LaunchDataSize )
		{
			// purposely not doing this in the constructor (unstable as used by tier0), but on first fetch
			bool bValid = false;
			DWORD dwLaunchDataSize;
			DWORD dwStatus = XGetLaunchDataSize( &dwLaunchDataSize );
			if ( dwStatus == ERROR_SUCCESS && dwLaunchDataSize <= MAX_LAUNCH_DATA_SIZE )
			{
				dwStatus = XGetLaunchData( (void*)&m_Launch, dwLaunchDataSize );
				if ( dwStatus == ERROR_SUCCESS )
				{
					bValid = true;
					m_LaunchDataSize = dwLaunchDataSize;
				}
			}

			if ( !bValid )
			{
				ResetLaunchData();
			}
		}

		// a valid launch payload could be ours (re-launch) or from an alternate booter (demo launcher)
		if ( m_LaunchDataSize == MAX_LAUNCH_DATA_SIZE && m_Launch.header.id == VALVE_LAUNCH_ID && m_Launch.header.version == VALVE_LAUNCH_VERSION )
		{
			// internal recognized format
			if ( pID )
			{
				*pID = m_Launch.header.id;
			}
			if ( pData )
			{
				*pData = m_Launch.data + sizeof( launchHeader_t );
			}
			if ( pDataSize )
			{
				*pDataSize = m_Launch.header.nDataSize;
			}
		}
		else if ( m_LaunchDataSize )
		{
			// not ours, unknown format, caller interprets
			if ( pID )
			{
				// assume payload was packaged with an initial ID
				*pID = *(unsigned int *)m_Launch.data;
			}
			if ( pData )
			{
				*pData = m_Launch.data;
			}
			if ( pDataSize )
			{
				*pDataSize = m_LaunchDataSize;
			}
		}

		// valid when data is available (not necessarily valve's tag)
		return m_LaunchDataSize != 0;
	}

	//--------------------------------------------------------------------------------------
	// Returns TRUE if the launch data blob is available. FALSE otherwise.
	// Data blob could be ours or not.
	//--------------------------------------------------------------------------------------
	bool RestoreLaunchData()
	{
		return GetLaunchData( NULL, NULL, NULL );
	}

	//--------------------------------------------------------------------------------------
	// Restores the data blob. If the data blob is not ours, resets it.
	//--------------------------------------------------------------------------------------
	void RestoreOrResetLaunchData()
	{
		RestoreLaunchData();
		if ( m_Launch.header.id != VALVE_LAUNCH_ID || m_Launch.header.version != VALVE_LAUNCH_VERSION )
		{
			// not interested in somebody else's data
			ResetLaunchData();
		}
	}

	//--------------------------------------------------------------------------------------
	// Returns OUR internal launch flags.
	//--------------------------------------------------------------------------------------
	int GetLaunchFlags()
	{
		// establish the data
		RestoreOrResetLaunchData();
		return m_Launch.header.flags;
	}

	int GetStorageID( void )
	{
		RestoreOrResetLaunchData();
		return m_Launch.header.nStorageID;
	}
	void SetStorageID( int storageID )
	{
		RestoreOrResetLaunchData();
		m_Launch.header.nStorageID = storageID;
	}

	int GetUserID( void )
	{
		RestoreOrResetLaunchData();
		return m_Launch.header.nUserID;
	}
	void SetUserID( int userID )
	{
		RestoreOrResetLaunchData();
		m_Launch.header.nUserID = userID;
	}

	bool GetForceEnglish( void )
	{
		RestoreOrResetLaunchData();
		return m_Launch.header.bForceEnglish ? true : false;
	}
	void SetForceEnglish( bool bForceEnglish )
	{
		RestoreOrResetLaunchData();
		m_Launch.header.bForceEnglish = bForceEnglish;
	}

	void GetInviteSessionID( XNKID *pSessionID )
	{
		RestoreOrResetLaunchData();
		*pSessionID = m_Launch.header.nInviteSessionID;
	}
	void SetInviteSessionID( XNKID *pSessionID )
	{
		RestoreOrResetLaunchData();
		m_Launch.header.nInviteSessionID = *pSessionID;
	}

	void Launch( const char *pNewImageName = NULL )
	{
		if ( !pNewImageName )
		{
			pNewImageName = "default.xex";
		}

		XLaunchNewImage( pNewImageName, 0 );
	}

private:
	xboxLaunchData_t	m_Launch;
	DWORD				m_LaunchDataSize;
};

#if defined( PLATFORM_H )
// For applications that use tier0.dll
PLATFORM_INTERFACE CXboxLaunch *XboxLaunch();
#endif

#endif
