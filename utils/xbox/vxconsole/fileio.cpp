//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	FILEIO.CPP
//
//	File I/O Service Routines
//=====================================================================================//
#include "vxconsole.h"

//-----------------------------------------------------------------------------
// SystemTimeToString
//
// mm/dd/yyyy hh:mm:ss am
//-----------------------------------------------------------------------------
char *SystemTimeToString( SYSTEMTIME *systemTime, char *buffer, int bufferSize )
{
	char	timeString[256];
	char	dateString[256];
	int		length;

	GetDateFormat( LOCALE_SYSTEM_DEFAULT, 0, systemTime, "MM'/'dd'/'yyyy", dateString, sizeof( dateString ) );
	GetTimeFormat( LOCALE_SYSTEM_DEFAULT, 0, systemTime, "hh':'mm':'ss tt", timeString, sizeof( timeString ) );
	length = _snprintf( buffer, bufferSize, "%s  %s", dateString, timeString );
	if ( length == -1 )
		buffer[bufferSize-1] = '\0';

	return buffer;
}

//-----------------------------------------------------------------------------
// CompareFileTimes_NTFStoFATX
//
//-----------------------------------------------------------------------------
int CompareFileTimes_NTFStoFATX( FILETIME* ntfsFileTime, char *ntfsTimeString, int ntfsStringSize, FILETIME* fatxFileTime, char *fatxTimeString, int fatxStringSize )
{
	SYSTEMTIME	ntfsSystemTime;
	SYSTEMTIME	fatxSystemTime;
	int			diff;
	int			ntfsSeconds;
	int			fatxSeconds;

	TIME_ZONE_INFORMATION tzInfo;
	GetTimeZoneInformation( &tzInfo );

	// cannot compare UTC file times directly
	// disjoint filesystems - xbox has a +/- 2s error
	// daylight savings time handling on each file system may cause problems
	FileTimeToSystemTime( ntfsFileTime, &ntfsSystemTime );
	FileTimeToSystemTime( fatxFileTime, &fatxSystemTime );

	// operate on local times, assumes xbox and pc are both set for same time zone and daylight saving
	SYSTEMTIME	ntfsLocalTime;
	SYSTEMTIME	fatxLocalTime;
	SystemTimeToTzSpecificLocalTime( &tzInfo, &ntfsSystemTime, &ntfsLocalTime );
	SystemTimeToTzSpecificLocalTime( &tzInfo, &fatxSystemTime, &fatxLocalTime );

	if ( ntfsTimeString )
	{
		SystemTimeToString( &ntfsLocalTime, ntfsTimeString, ntfsStringSize );
	}

	if ( fatxTimeString )
	{
		SystemTimeToString( &fatxLocalTime, fatxTimeString, fatxStringSize );
	}

	diff = ntfsLocalTime.wYear-fatxLocalTime.wYear;
	if ( diff )
		return diff;

	diff = ntfsLocalTime.wMonth-fatxLocalTime.wMonth;
	if ( diff )
		return diff;

	diff = ntfsLocalTime.wDay-fatxLocalTime.wDay;
	if ( diff )
		return diff;

	diff = ntfsLocalTime.wHour-fatxLocalTime.wHour;
	if ( diff )
		return diff;

	// allow for +/- 3s error
	ntfsSeconds = ntfsLocalTime.wHour*60*60 + ntfsLocalTime.wMinute*60 + ntfsLocalTime.wSecond;
	fatxSeconds = fatxLocalTime.wHour*60*60 + fatxLocalTime.wMinute*60 + fatxLocalTime.wSecond;

	diff = ntfsSeconds-fatxSeconds;
	if ( diff > 3 || diff < -3 )
		return diff;
	
	// times are considered equal
	return 0;
}

//-----------------------------------------------------------------------------
//	FreeTargetFileList
//
//-----------------------------------------------------------------------------
void FreeTargetFileList( fileNode_t* pFileList )
{
	fileNode_t	*nodePtr;
	fileNode_t	*nextPtr;

	if ( !pFileList )
		return;

	nodePtr = pFileList;
	while ( nodePtr )
	{
		nextPtr = nodePtr->nextPtr;

		Sys_Free( nodePtr->filename );
		Sys_Free( nodePtr );

		nodePtr = nextPtr;
	}
}

//-----------------------------------------------------------------------------
//	GetTargetFileList_r
//
//-----------------------------------------------------------------------------
bool GetTargetFileList_r( char* targetPath, bool recurse, int attributes, int level, fileNode_t** pFileList )
{
	HRESULT				hr;
	PDM_WALK_DIR		pWalkDir = NULL;
	DM_FILE_ATTRIBUTES	fileAttr;
	bool				valid;
	char				filename[MAX_PATH];
	fileNode_t*			nodePtr;
	bool				bGetNormal;
	int					fixedAttributes;

	if ( !level )
		*pFileList = NULL;

	fixedAttributes = attributes;
	if ( fixedAttributes & FILE_ATTRIBUTE_NORMAL )
	{
		fixedAttributes &= ~FILE_ATTRIBUTE_NORMAL;
		bGetNormal = true;
	}
	else
		bGetNormal = false;

	while ( 1 )
	{
		hr = DmWalkDir( &pWalkDir, targetPath, &fileAttr );
		if ( hr != XBDM_NOERR )
			break;

		strcpy( filename, targetPath );
		Sys_AddFileSeperator( filename, sizeof( filename ) );
		strcat( filename, fileAttr.Name );

		// restrict to desired attributes
		if ( ( bGetNormal && !fileAttr.Attributes ) ||	( fileAttr.Attributes & fixedAttributes ) )
		{
			Sys_NormalizePath( filename, false );

			// create a new file node
			nodePtr = ( fileNode_t* )Sys_Alloc( sizeof( fileNode_t ) );

			// link it in
			nodePtr->filename     = Sys_CopyString( filename );
			nodePtr->changeTime   = fileAttr.ChangeTime;
			nodePtr->creationTime = fileAttr.CreationTime;
			nodePtr->sizeHigh     = fileAttr.SizeHigh;
			nodePtr->sizeLow      = fileAttr.SizeLow;
			nodePtr->attributes   = fileAttr.Attributes;
			nodePtr->level        = level;
			nodePtr->nextPtr      = *pFileList;
			*pFileList            = nodePtr;
		}

		if ( fileAttr.Attributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			if ( recurse )
			{
				// descend into directory
				valid = GetTargetFileList_r( filename, recurse, attributes, level+1, pFileList );
				if ( !valid )
					return false;
			}
		}
	}
	DmCloseDir( pWalkDir );

	if ( hr != XBDM_ENDOFLIST )
	{
		// failure
		return false;
	}

	// ok
	return true;
}

//-----------------------------------------------------------------------------
// FileSyncEx
//
// -1: failure, 0: nothing, 1: synced
//-----------------------------------------------------------------------------
int FileSyncEx( const char* localFilename, const char* targetFilename, int fileSyncMode, bool bVerbose, bool bNoWrite )
{
	bool						copy;
	bool						pathExist;
	WIN32_FILE_ATTRIBUTE_DATA	localAttributes;
	DM_FILE_ATTRIBUTES			targetAttributes;
	HRESULT						hr;
	int							errCode;
	int							deltaTime;
	char						localTimeString[256];
	char						targetTimeString[256];

	if ( ( fileSyncMode & FSYNC_TYPEMASK ) == FSYNC_OFF )
	{
		return 0;
	}

	if ( !GetFileAttributesEx( localFilename, GetFileExInfoStandard, &localAttributes ) )
	{
		// failed to get the local file's attributes
		if ( bVerbose )
		{
			ConsoleWindowPrintf( XBX_CLR_RED, "Sync Failure: Local file %s not available\n", localFilename );
		}
		return -1;
	}

	if ( localAttributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
	{
		// ignore directory
		return 0;
	}

	if ( fileSyncMode & FSYNC_ANDEXISTSONTARGET )
	{
		hr = DmGetFileAttributes( targetFilename, &targetAttributes );
		if ( hr != XBDM_NOERR )
		{
			// target doesn't exist, no sync operation should commence
			if ( bVerbose )
			{
				ConsoleWindowPrintf( XBX_CLR_DEFAULT, "No Update, Target file %s not available\n", targetFilename );
			}
			return 0;
		}
	}

	// default success, no operation
	errCode = 0;

	// default, create path and copy
	copy      = true;
	pathExist = false;

	if ( ( fileSyncMode & FSYNC_TYPEMASK ) == FSYNC_IFNEWER )
	{
		hr = DmGetFileAttributes( targetFilename, &targetAttributes );
		if ( hr == XBDM_NOERR )
		{
			// target path to file exists
			pathExist = true;

			// compare times
			deltaTime = CompareFileTimes_NTFStoFATX( &localAttributes.ftLastWriteTime, localTimeString, sizeof( localTimeString), &targetAttributes.ChangeTime, targetTimeString, sizeof( targetTimeString ) );
			if ( deltaTime < 0 )
			{
				// ntfs is older, fatx is newer, no update
				if ( bVerbose )
				{
					ConsoleWindowPrintf( XBX_CLR_DEFAULT, "No Update, %s [%s] is newer than %s [%s]\n", targetFilename, targetTimeString, localFilename, localTimeString );
				}
				copy = false;
			}
			else if ( !deltaTime )
			{
				// equal times, compare sizes
				if ( localAttributes.nFileSizeLow == targetAttributes.SizeLow &&
					localAttributes.nFileSizeHigh == targetAttributes.SizeHigh )
				{
					// file appears synced
					copy = false;
				}
				if ( bVerbose )
				{
					if ( copy )
					{
						ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Update, %s [%s] [%d] has different size than %s [%s] [%d]\n", targetFilename, targetTimeString, targetAttributes.SizeLow, localFilename, localTimeString, localAttributes.nFileSizeLow );
					}
					else
					{
						ConsoleWindowPrintf( XBX_CLR_DEFAULT, "No Update, %s [%s] [%d] has same time and file size as %s [%s] [%d]\n", targetFilename, targetTimeString, targetAttributes.SizeLow, localFilename, localTimeString, localAttributes.nFileSizeLow );
					}
				}
			}
			else
			{
				// ntfs is newer, fatx is older, update
				if ( bVerbose )
				{
					ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Update, %s [%s] is older than %s [%s]\n", targetFilename, targetTimeString, localFilename, localTimeString );
				}
			}
		}
	}
	else if ( ( fileSyncMode & FSYNC_TYPEMASK ) == FSYNC_ALWAYS )
	{
		if ( bVerbose )
		{
			ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Force Update, %s\n", targetFilename );
		}
	}

	if ( copy && !bNoWrite )
	{
		if ( !pathExist )
		{
			CreateTargetPath( targetFilename );
		}

		hr = DmSendFile( localFilename, targetFilename );
		if ( hr == XBDM_NOERR )
		{
			// force the target to match the local attributes
			// to ensure sync
			memset( &targetAttributes, 0, sizeof( targetAttributes ) );
			targetAttributes.SizeHigh     = localAttributes.nFileSizeHigh;
			targetAttributes.SizeLow      = localAttributes.nFileSizeLow;
			targetAttributes.CreationTime = localAttributes.ftCreationTime;
			targetAttributes.ChangeTime   = localAttributes.ftLastWriteTime;
			DmSetFileAttributes( targetFilename, &targetAttributes );

			// success, file copied
			errCode = 1;
		}
		else
		{
			// failure
			if ( bVerbose )
			{
				ConsoleWindowPrintf( XBX_CLR_RED, "Sync Failed!\n" );
			}
			errCode = -1;
		}

		DebugCommand( "0x%8.8x = FileSyncEx( %s, %s )\n", hr, localFilename, targetFilename );
	}

	return errCode;
}

//-----------------------------------------------------------------------------
//	LoadTargetFile
//
//-----------------------------------------------------------------------------
bool LoadTargetFile( const char *pTargetPath, int *pFileSize, void **pData )
{
	DM_FILE_ATTRIBUTES	fileAttributes;
	HRESULT				hr;
	DWORD				bytesRead;
	char				*pBuffer;

	*pFileSize = 0;
	*pData     = (void *)NULL;

	hr = DmGetFileAttributes( pTargetPath, &fileAttributes );
	if ( hr != XBDM_NOERR || !fileAttributes.SizeLow )
		return false;

	// allocate for size and terminating null
	pBuffer = (char *)Sys_Alloc( fileAttributes.SizeLow+1 );

	hr = DmReadFilePartial( pTargetPath, 0, (LPBYTE)pBuffer, fileAttributes.SizeLow, &bytesRead );
  	if ( hr != XBDM_NOERR || ( bytesRead != fileAttributes.SizeLow ) )
	{
		Sys_Free( pBuffer );
		return false;
	}

	// add a terminating null
	pBuffer[fileAttributes.SizeLow] = '\0';

	*pFileSize = fileAttributes.SizeLow;
	*pData     = pBuffer;

	// success
	return true;
}

//-----------------------------------------------------------------------------
//	CreateTargetPath
//
//-----------------------------------------------------------------------------
bool CreateTargetPath( const char *pTargetFilename )
{
	// create path chain
	char	*pPath;
	char	dirPath[MAX_PATH];

	// prime and skip to first seperator
	strcpy( dirPath, pTargetFilename );
	pPath = strchr( dirPath, '\\' );
	while ( pPath )
	{		
		pPath = strchr( pPath+1, '\\' );
		if ( pPath )
		{
			*pPath = '\0';
			DmMkdir( dirPath );
			*pPath = '\\';
		}
	}

	return true;
}
