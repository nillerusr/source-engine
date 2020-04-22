//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	Copies a file using overlapped async IO.
//
//	Stub executeable
//=====================================================================================//
#include "xbox_loader.h"

#define BUFFER_SIZE			(1*1024*1024)
#define NUM_BUFFERS			4
#define ALIGN(x,y)			(((x)+(y)-1) & ~((y)-1))

struct CopyFile_t
{
	// source file
	HANDLE				m_hSrcFile;
	DWORD				m_srcFileSize;
	int					m_readBufferSize;
	unsigned int		m_numReadCycles;

	// target file
	HANDLE				m_hDstFile;
	DWORD				m_dstFileSize;

	// source file gets decompressed
	bool				m_bInflate;
	unsigned char		*m_pInflateBuffer;
	int					m_inflateBufferSize;

	bool				m_bCopyError;
	CopyStats_t			*m_pCopyStats;
};

struct Buffer_t
{
	unsigned char	*pData;
	DWORD			dwSize;
	Buffer_t*		pNext;
	int				id;
};

Buffer_t	*g_pReadBuffers = NULL;
Buffer_t	*g_pWriteBuffers = NULL;

CRITICAL_SECTION	g_criticalSection;
HANDLE				g_hReadEvent;
HANDLE				g_hWriteEvent;
DWORD				*g_pNumReadBuffers;
DWORD				*g_pNumWriteBuffers;

//-----------------------------------------------------------------------------
// CreateFilePath
//
// Create full path to specified file.
//-----------------------------------------------------------------------------
bool CreateFilePath( const char *inPath )
{
	char*	ptr;
	char	dirPath[MAX_PATH];
	BOOL	bSuccess;

	// prime and skip to first seperator after the drive path
	strcpy( dirPath, inPath );
	ptr = strchr( dirPath, '\\' );
	while ( ptr )
	{		
		ptr = strchr( ptr+1, '\\' );
		if ( ptr )
		{
			*ptr = '\0';
			bSuccess = ::CreateDirectory( dirPath, NULL );
			*ptr = '\\';
		}
	}

	// ensure read-only is cleared
	SetFileAttributes( inPath, FILE_ATTRIBUTE_NORMAL );

	return true;
}

//-----------------------------------------------------------------------------
// LockBufferForRead
//
//-----------------------------------------------------------------------------
Buffer_t *LockBufferForRead()
{
	if ( !g_pReadBuffers )
	{
		// out of data, wait for it
		WaitForSingleObject( g_hReadEvent, INFINITE );
	}
	else
	{
		ResetEvent( g_hReadEvent );
	}

	EnterCriticalSection( &g_criticalSection );

	Buffer_t *pBuffer = g_pReadBuffers;
	g_pReadBuffers = pBuffer->pNext;

	(*g_pNumReadBuffers)--;

	LeaveCriticalSection( &g_criticalSection );

	return pBuffer;
}

//-----------------------------------------------------------------------------
// LockBufferForWrite
//
//-----------------------------------------------------------------------------
Buffer_t* LockBufferForWrite()
{
	if ( !g_pWriteBuffers )
	{
		// out of data, wait for more
		WaitForSingleObject( g_hWriteEvent, INFINITE );
	}
	else
	{
		ResetEvent( g_hWriteEvent );
	}

	EnterCriticalSection( &g_criticalSection );

	Buffer_t *pBuffer = g_pWriteBuffers;
	g_pWriteBuffers = pBuffer->pNext;

	(*g_pNumWriteBuffers)--;

	LeaveCriticalSection( &g_criticalSection );

	return pBuffer;
}

//-----------------------------------------------------------------------------
// AddBufferForRead
//
//-----------------------------------------------------------------------------
void AddBufferForRead( Buffer_t *pBuffer )
{
	EnterCriticalSection( &g_criticalSection );

	// add to end of list
	Buffer_t *pCurrent = g_pReadBuffers;
	while ( pCurrent && pCurrent->pNext )
	{
		pCurrent = pCurrent->pNext;
	}
	if ( pCurrent )
	{
		pBuffer->pNext  = pCurrent->pNext;
		pCurrent->pNext = pBuffer;
	}
	else
	{
		pBuffer->pNext = NULL;
		g_pReadBuffers = pBuffer;
	}
	
	(*g_pNumReadBuffers)++;

	LeaveCriticalSection( &g_criticalSection );

	SetEvent( g_hReadEvent );
}

//-----------------------------------------------------------------------------
// AddBufferForWrite
//
//-----------------------------------------------------------------------------
void AddBufferForWrite( Buffer_t *pBuffer )
{
	EnterCriticalSection( &g_criticalSection );

	// add to end of list
	Buffer_t* pCurrent = g_pWriteBuffers;
	while ( pCurrent && pCurrent->pNext )
	{
		pCurrent = pCurrent->pNext;
	}
	if ( pCurrent )
	{
		pBuffer->pNext  = pCurrent->pNext;
		pCurrent->pNext = pBuffer;
	}
	else
	{
		pBuffer->pNext = NULL;
		g_pWriteBuffers = pBuffer;
	}

	(*g_pNumWriteBuffers)++;

	LeaveCriticalSection( &g_criticalSection );

	SetEvent( g_hWriteEvent );
}

//-----------------------------------------------------------------------------
// ReadFileThread
//
//-----------------------------------------------------------------------------
DWORD WINAPI ReadFileThread( LPVOID lParam )
{
	CopyFile_t		*pCopyFile;
	OVERLAPPED		overlappedRead = {0};
	DWORD			startTime;
	DWORD			dwBytesRead;
	DWORD			dwError;
	BOOL			bResult;
	Buffer_t		*pBuffer;

	pCopyFile = (CopyFile_t*)lParam;

	// Copy from the buffer to the Hard Drive
	for ( unsigned int readCycle = 0; readCycle < pCopyFile->m_numReadCycles; ++readCycle )
	{
		pBuffer = LockBufferForRead();

		startTime = GetTickCount();
		dwBytesRead = 0;

		int numAttempts = 0;
retry:
		// read file from DVD
		bResult = ReadFile( pCopyFile->m_hSrcFile, pBuffer->pData, pCopyFile->m_readBufferSize, NULL, &overlappedRead );
		dwError = GetLastError();
		if ( !bResult && dwError != ERROR_IO_PENDING )
		{
			if ( dwError == ERROR_HANDLE_EOF )
			{
				// nothing more to read
				break;
			}

			numAttempts++;
			if ( numAttempts == 3 )
			{
				// error
				pCopyFile->m_bCopyError = true;
				break;
			}
			else
			{
				goto retry;
			}
		}
		else
		{
			// Wait for the operation to finish
			GetOverlappedResult( pCopyFile->m_hSrcFile, &overlappedRead, &dwBytesRead, TRUE );
			overlappedRead.Offset += dwBytesRead;
		}

		if ( !dwBytesRead  )
		{
			pCopyFile->m_bCopyError = true;
			break;
		}

		pCopyFile->m_pCopyStats->m_bufferReadSize = dwBytesRead;
		pCopyFile->m_pCopyStats->m_bufferReadTime = GetTickCount() - startTime;
		pCopyFile->m_pCopyStats->m_totalReadSize += pCopyFile->m_pCopyStats->m_bufferReadSize;
		pCopyFile->m_pCopyStats->m_totalReadTime += pCopyFile->m_pCopyStats->m_bufferReadTime;

		pBuffer->dwSize = dwBytesRead;
		AddBufferForWrite( pBuffer );
	}

	return 0;
}

//-----------------------------------------------------------------------------
// WriteFileThread
//
//-----------------------------------------------------------------------------
DWORD WINAPI WriteFileThread( LPVOID lParam )
{
	CopyFile_t		*pCopyFile;
	OVERLAPPED		overlappedWrite = {0};
	DWORD			startTime;
	DWORD			dwBytesWrite;
	DWORD			dwWriteSize;
	DWORD			dwError;
	BOOL			bResult;
	Buffer_t		*pBuffer;
	unsigned char	*pWriteBuffer;

	pCopyFile = (CopyFile_t*)lParam;

	while ( overlappedWrite.Offset < pCopyFile->m_dstFileSize )
	{
		// wait for wake-up event
		pBuffer = LockBufferForWrite();

		if ( pCopyFile->m_bInflate )
		{
			startTime = GetTickCount();

			DWORD dwSkip = overlappedWrite.Offset ? 0 : sizeof( xCompressHeader );
			dwWriteSize = JCALG1_Decompress_Formatted_Buffer( pBuffer->dwSize - dwSkip, pBuffer->pData + dwSkip, pCopyFile->m_inflateBufferSize, pCopyFile->m_pInflateBuffer );
			if ( dwWriteSize == (DWORD)-1 )
			{
				pCopyFile->m_bCopyError = true;
				break;
			}

			pCopyFile->m_pCopyStats->m_inflateSize = dwWriteSize;
			pCopyFile->m_pCopyStats->m_inflateTime = GetTickCount() - startTime;

			pWriteBuffer = pCopyFile->m_pInflateBuffer;
		}
		else
		{
			// straight copy
			dwWriteSize  = pBuffer->dwSize;
			pWriteBuffer = pBuffer->pData;
		}

		if ( overlappedWrite.Offset + dwWriteSize >= pCopyFile->m_dstFileSize )
		{
			// last buffer, ensure all data is written
			dwWriteSize = ALIGN( dwWriteSize, 512 );
		}

		startTime = GetTickCount();
		dwBytesWrite = 0;
	
		int numAttempts = 0;
retry:
		// write file to HDD
		bResult = WriteFile( pCopyFile->m_hDstFile, pWriteBuffer, (dwWriteSize/512) * 512, NULL, &overlappedWrite );
		dwError = GetLastError();
		if ( !bResult && dwError != ERROR_IO_PENDING )
		{
			numAttempts++;
			if ( numAttempts == 3 )
			{
				// error
				pCopyFile->m_bCopyError = true;
				break;
			}
			else
			{
				goto retry;
			}
		}
		else
		{
			// Wait for the operation to finish
			GetOverlappedResult( pCopyFile->m_hDstFile, &overlappedWrite, &dwBytesWrite, TRUE );
			overlappedWrite.Offset += dwBytesWrite;
		}

		if ( dwBytesWrite  )
		{
			// track expected size
			pCopyFile->m_pCopyStats->m_bytesCopied += dwBytesWrite;
			pCopyFile->m_pCopyStats->m_writeSize += dwBytesWrite;
		}
		else
		{
			pCopyFile->m_bCopyError = true;
			break;
		}

		pCopyFile->m_pCopyStats->m_bufferWriteSize = dwBytesWrite;
		pCopyFile->m_pCopyStats->m_bufferWriteTime = GetTickCount() - startTime;
		pCopyFile->m_pCopyStats->m_totalWriteSize += pCopyFile->m_pCopyStats->m_bufferWriteSize;
		pCopyFile->m_pCopyStats->m_totalWriteTime += pCopyFile->m_pCopyStats->m_bufferWriteTime;

		AddBufferForRead( pBuffer );
	}

	return 0;
}

//-----------------------------------------------------------------------------
// CopyFileInit
//
//-----------------------------------------------------------------------------
void CopyFileInit()
{
	static bool init = false;
	if ( !init )
	{
		InitializeCriticalSection( &g_criticalSection );
		g_hReadEvent  = CreateEvent( NULL, FALSE, FALSE, NULL );
		g_hWriteEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		init = true;
	}
	else
	{
		// expected startup state
		ResetEvent( g_hReadEvent );
		ResetEvent( g_hWriteEvent );

		g_pReadBuffers  = NULL;
		g_pWriteBuffers = NULL;
	}
}

//-----------------------------------------------------------------------------
// CopyFileOverlapped
//
//-----------------------------------------------------------------------------
bool CopyFileOverlapped( const char *pSrcFilename, const char *pDstFilename, xCompressHeader *pxcHeader, CopyStats_t *pCopyStats )
{
	CopyFile_t	copyFile = {0};
	Buffer_t	buffers[NUM_BUFFERS] = {0};
	HANDLE		hReadThread = NULL;
	HANDLE		hWriteThread = NULL;
	bool		bSuccess = false;
	DWORD		startCopyTime;
	DWORD		dwResult;
	int			i;

	startCopyTime = GetTickCount();

	CopyFileInit();

	g_pNumReadBuffers  = &pCopyStats->m_numReadBuffers;
	g_pNumWriteBuffers = &pCopyStats->m_numWriteBuffers;

	strcpy( pCopyStats->m_srcFilename, pSrcFilename );
	strcpy( pCopyStats->m_dstFilename, pDstFilename );

	copyFile.m_hSrcFile   = INVALID_HANDLE_VALUE;
	copyFile.m_hDstFile   = INVALID_HANDLE_VALUE;
	copyFile.m_pCopyStats = pCopyStats;
	copyFile.m_bCopyError = false;

	// validate the source file
	copyFile.m_hSrcFile = CreateFile( pSrcFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING, NULL );
	if ( copyFile.m_hSrcFile == INVALID_HANDLE_VALUE )
	{
		// failure
		goto cleanUp;
	}

	copyFile.m_srcFileSize = GetFileSize( copyFile.m_hSrcFile, NULL );
	if ( copyFile.m_srcFileSize == (DWORD)-1 )
	{
		// failure
		goto cleanUp;
	}

	// ensure the target file path exists
	CreateFilePath( pDstFilename );

	// validate the target file
	copyFile.m_hDstFile = CreateFile( pDstFilename, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING, NULL );
	if ( copyFile.m_hDstFile == INVALID_HANDLE_VALUE )
	{
		// failure
		goto cleanUp;
	}
	
	pCopyStats->m_readSize  = copyFile.m_srcFileSize;
	pCopyStats->m_writeSize = 0;

	if ( pxcHeader )
	{
		// read in chunks of compressed blocks
		copyFile.m_readBufferSize = pxcHeader->nReadBlockSize;
		copyFile.m_dstFileSize = pxcHeader->nUncompressedFileSize;
	}
	else
	{
		// setup for copy
		copyFile.m_readBufferSize = BUFFER_SIZE;
		copyFile.m_dstFileSize = copyFile.m_srcFileSize;
	}

	// setup read buffers
	for ( i=0; i<NUM_BUFFERS; i++)
	{
		buffers[i].pData  = new unsigned char[copyFile.m_readBufferSize];
		buffers[i].dwSize = 0;
		buffers[i].pNext  = NULL;
		AddBufferForRead( &buffers[i] );
	}
	copyFile.m_numReadCycles = (copyFile.m_srcFileSize + copyFile.m_readBufferSize - 1)/copyFile.m_readBufferSize;

	// setup write buffer
	if ( pxcHeader )
	{
		copyFile.m_pInflateBuffer = new unsigned char[pxcHeader->nDecompressionBufferSize];
		copyFile.m_inflateBufferSize = pxcHeader->nDecompressionBufferSize;
		copyFile.m_bInflate = true;
	}
	else
	{
		copyFile.m_bInflate = false;
	}

	// pre-size the target file in aligned buffers
	DWORD dwAligned = ALIGN( copyFile.m_dstFileSize, 512 );
	dwResult = SetFilePointer( copyFile.m_hDstFile, dwAligned, NULL, FILE_BEGIN );
	if ( dwResult == INVALID_SET_FILE_POINTER )
	{
		// failure
		goto cleanUp;
	}
	SetEndOfFile( copyFile.m_hDstFile );

	// start the read thread
	hReadThread = CreateThread( 0, 0, &ReadFileThread, &copyFile, 0, 0 );
	if ( !hReadThread )
	{
		// failure
		goto cleanUp;
	}

	// wait for buffers to populate

	// start the write thread
	hWriteThread = CreateThread( 0, 0, &WriteFileThread, &copyFile, 0, 0 );
	if ( !hWriteThread )
	{
		// failure
		goto cleanUp;
	}

	// wait for write thread to finish
	WaitForSingleObject( hWriteThread, INFINITE );
	WaitForSingleObject( hReadThread, INFINITE );

	if ( copyFile.m_bCopyError )
	{
		goto cleanUp;
	}

	// Fixup the file size
	CloseHandle( copyFile.m_hDstFile );
	copyFile.m_hDstFile = INVALID_HANDLE_VALUE;

	if ( copyFile.m_dstFileSize % 512 )
	{
		// re-open file as non-buffered to adjust to correct file size
		HANDLE hFile = CreateFile( pDstFilename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		SetFilePointer( hFile, copyFile.m_dstFileSize, NULL, FILE_BEGIN );
		SetEndOfFile( hFile );
		CloseHandle( hFile );
	}

	// finished
	bSuccess = true;

cleanUp:
	if ( copyFile.m_hSrcFile != INVALID_HANDLE_VALUE )
	{
		CloseHandle( copyFile.m_hSrcFile );
	}

	if ( copyFile.m_hDstFile != INVALID_HANDLE_VALUE )
	{
		CloseHandle( copyFile.m_hDstFile );
	}

	if ( hReadThread )
	{
		CloseHandle( hReadThread );
	}

	if ( hWriteThread )
	{
		CloseHandle( hWriteThread );
	}

	for ( i=0; i<NUM_BUFFERS; i++ )
	{
		if ( buffers[i].pData )
		{
			delete [] buffers[i].pData;
		}
	}

	if ( copyFile.m_pInflateBuffer )
	{
		delete [] copyFile.m_pInflateBuffer;
	}

	if ( !bSuccess )
	{
		pCopyStats->m_copyErrors++;
	}

	pCopyStats->m_copyTime = GetTickCount() - startCopyTime;

	return bSuccess;
}
