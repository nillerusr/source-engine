//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "FileChangeWatcher.h"
#include "tier1/utldict.h"
#include "filesystem_tools.h"


CFileChangeWatcher::CFileChangeWatcher()
{
	m_pCallbacks = NULL;
}

CFileChangeWatcher::~CFileChangeWatcher()
{
	Term();
}

void CFileChangeWatcher::Init( ICallbacks *pCallbacks )
{
	Term();
	m_pCallbacks = pCallbacks;
}

bool CFileChangeWatcher::AddDirectory( const char *pSearchPathBase, const char *pDirName, bool bRecursive )
{
	char fullDirName[MAX_PATH];
	V_ComposeFileName( pSearchPathBase, pDirName, fullDirName, sizeof( fullDirName ) );
	
	HANDLE hDir = CreateFile( fullDirName, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, NULL );
	if ( hDir == INVALID_HANDLE_VALUE )
	{
		Warning( "CFileChangeWatcher::AddDirectory - can't get a handle to directory %s.\n", pDirName );
		return false;
	}

	// Call this once to start the ball rolling.. Next time we call it, it'll tell us the changes that
	// have happened since this call.
	CDirWatch *pDirWatch = new CDirWatch;
	V_strncpy( pDirWatch->m_SearchPathBase, pSearchPathBase, sizeof( pDirWatch->m_SearchPathBase ) );
	V_strncpy( pDirWatch->m_DirName, pDirName, sizeof( pDirWatch->m_DirName ) );
	V_strncpy( pDirWatch->m_FullDirName, fullDirName, sizeof( pDirWatch->m_FullDirName ) );
	pDirWatch->m_hDir = hDir;
	pDirWatch->m_hEvent = CreateEvent( NULL, false, false, NULL );
	memset( &pDirWatch->m_Overlapped, 0, sizeof( pDirWatch->m_Overlapped ) );
	pDirWatch->m_Overlapped.hEvent = pDirWatch->m_hEvent;
	if ( !CallReadDirectoryChanges( pDirWatch ) )
	{
		CloseHandle( pDirWatch->m_hEvent );
		CloseHandle( pDirWatch->m_hDir );
		delete pDirWatch;
		return false;
	}

	m_DirWatches.AddToTail( pDirWatch );
	return true;
}

void CFileChangeWatcher::Term()
{
	for ( int i=0; i < m_DirWatches.Count(); i++ )
	{
		CloseHandle( m_DirWatches[i]->m_hDir );
		CloseHandle( m_DirWatches[i]->m_hEvent );
	}
	m_DirWatches.PurgeAndDeleteElements();
	m_pCallbacks = NULL;
}

int CFileChangeWatcher::Update()
{
	CUtlDict< int, int > queuedChanges;
	int nTotalChanges = 0;
	
	// Check each CDirWatch.
	int i = 0;
	while ( i < m_DirWatches.Count() )
	{
		CDirWatch *pDirWatch = m_DirWatches[i];
	
		DWORD dwBytes = 0;
		if ( GetOverlappedResult( pDirWatch->m_hDir, &pDirWatch->m_Overlapped, &dwBytes, FALSE ) )
		{
			// Read through the notifications.
			int nBytesLeft = (int)dwBytes;
			char *pCurPos = pDirWatch->m_Buffer;
			while ( nBytesLeft >= sizeof( FILE_NOTIFY_INFORMATION ) )
			{
				FILE_NOTIFY_INFORMATION *pNotify = (FILE_NOTIFY_INFORMATION*)pCurPos;
			
				if ( m_pCallbacks )
				{
					// Figure out what happened to this file.
					WCHAR nullTerminated[2048];
					int nBytesToCopy = min( (int)pNotify->FileNameLength, 2047 );
					memcpy( nullTerminated, pNotify->FileName, nBytesToCopy );
					nullTerminated[nBytesToCopy/2] = 0;
					char ansiFilename[1024];
					V_UnicodeToUTF8( nullTerminated, ansiFilename, sizeof( ansiFilename ) );
					
					// Now add it to the queue.	We use this queue because sometimes Windows will give us multiple
					// of the same modified notification back to back, and we want to reduce redundant calls.
					int iExisting = queuedChanges.Find( ansiFilename );
					if ( iExisting == queuedChanges.InvalidIndex() )
					{
						iExisting = queuedChanges.Insert( ansiFilename, 0 );
						++nTotalChanges;
					}
				}		
			
				if ( pNotify->NextEntryOffset == 0 )
					break;
					
				pCurPos += pNotify->NextEntryOffset;
				nBytesLeft -= (int)pNotify->NextEntryOffset;
			}
			
			CallReadDirectoryChanges( pDirWatch );
			continue;	// Check again because sometimes it queues up duplicate notifications.
		}

		// Process all the entries in the queue.
		for ( int iQueuedChange=queuedChanges.First(); iQueuedChange != queuedChanges.InvalidIndex(); iQueuedChange=queuedChanges.Next( iQueuedChange ) )
		{
			SendNotification( pDirWatch, queuedChanges.GetElementName( iQueuedChange ) );
		}
		queuedChanges.Purge();
		++i;
	}
	
	return nTotalChanges;
}

void CFileChangeWatcher::SendNotification( CFileChangeWatcher::CDirWatch *pDirWatch, const char *pRelativeFilename )
{
	// Use this for full filenames although you don't strictly need it.. 
	char fullFilename[MAX_PATH];
	V_ComposeFileName( pDirWatch->m_FullDirName, pRelativeFilename, fullFilename, sizeof( fullFilename ) );

	m_pCallbacks->OnFileChange( pRelativeFilename, fullFilename );
}

BOOL CFileChangeWatcher::CallReadDirectoryChanges( CFileChangeWatcher::CDirWatch *pDirWatch )
{
	return ReadDirectoryChangesW( pDirWatch->m_hDir, 
		pDirWatch->m_Buffer, 
		sizeof( pDirWatch->m_Buffer ), 
		true, 
		FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, 
		NULL, 
		&pDirWatch->m_Overlapped, 
		NULL );
}



