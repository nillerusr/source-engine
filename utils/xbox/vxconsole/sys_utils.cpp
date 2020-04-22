//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	SYS_UTILS.C
//
//=====================================================================================//
#include "vxconsole.h"

CHAR g_szRegistryPrefix[256];

//-----------------------------------------------------------------------------
//	Sys_SetRegistryPrefix
//
//-----------------------------------------------------------------------------
void Sys_SetRegistryPrefix( const CHAR *pPrefix )
{
	_snprintf_s( g_szRegistryPrefix, sizeof( g_szRegistryPrefix ), _TRUNCATE, pPrefix );
}

//-----------------------------------------------------------------------------
//	Sys_SplitRegistryKey
//
//-----------------------------------------------------------------------------
static BOOL Sys_SplitRegistryKey( const CHAR *key, CHAR *key0, int key0Len, CHAR *key1, int key1Len )
{
	if ( !key )
	{
		return false;
	}
	
	int len = (int)strlen( key );
	if ( !len )
	{
		return false;
	}

	int Start = -1;
	for ( int i=len-1; i>=0; i-- )
	{
		if ( key[i] == '\\' )
			break;
		else
			Start=i;
	}

	if ( Start == -1 )
		return false;
	
	_snprintf_s( key0, Start, _TRUNCATE, key );
	_snprintf_s( key1, ( len-Start )+1, _TRUNCATE, key+Start );

	return true;
}

//-----------------------------------------------------------------------------
//	Sys_SetRegistryString
//
//-----------------------------------------------------------------------------
BOOL Sys_SetRegistryString( const CHAR *keyName, const CHAR *value )
{
	HKEY	hKey;
	CHAR	key0[256];
	CHAR	key1[256];
	CHAR	keyBuff[256];
	CHAR	*key;

	strcpy_s( keyBuff, sizeof( keyBuff ), g_szRegistryPrefix );
	strcat_s( keyBuff, sizeof( keyBuff ), keyName );
	key = keyBuff;
	
	HKEY hSlot = HKEY_CURRENT_USER;
	if ( !strncmp( key, "HKEY_LOCAL_MACHINE", 18 ) )
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if ( !strncmp( key, "HKEY_CURRENT_USER", 17 ) )
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	if ( !Sys_SplitRegistryKey( key, key0, sizeof( key0 ), key1, sizeof( key1 ) ) )
		return false;

	if ( RegCreateKeyEx( hSlot,key0,NULL,NULL,REG_OPTION_NON_VOLATILE, value ? KEY_WRITE : KEY_ALL_ACCESS,NULL,&hKey,NULL )!=ERROR_SUCCESS )
		return false;

	if ( RegSetValueEx( hKey, key1, NULL, REG_SZ, ( UCHAR* )value, (int)strlen( value ) + 1 ) != ERROR_SUCCESS )
	{
		RegCloseKey( hKey );
		return false;
	}

	// success
	RegCloseKey( hKey );
	return true;
}

//-----------------------------------------------------------------------------
//	Sys_GetRegistryString
//
//-----------------------------------------------------------------------------
BOOL Sys_GetRegistryString( const CHAR *keyName, CHAR *value, const CHAR* defValue, int valueLen )
{
	HKEY	hKey;
	CHAR	key0[256];
	CHAR	key1[256];
	CHAR	keyBuff[256];
	CHAR	*key;

	strcpy_s( keyBuff, sizeof( keyBuff ), g_szRegistryPrefix );
	strcat_s( keyBuff, sizeof( keyBuff ), keyName );
	key = keyBuff;

	if ( defValue )
	{
		_snprintf_s( value, valueLen, _TRUNCATE, defValue );
	}

	HKEY hSlot = HKEY_CURRENT_USER;
	if ( !strncmp( key, "HKEY_LOCAL_MACHINE", 18 ) )
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if ( !strncmp( key, "HKEY_CURRENT_USER", 17 ) )
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	if ( !Sys_SplitRegistryKey( key,key0,256,key1,256 ) )
		return false;

	if ( RegOpenKeyEx( hSlot,key0,NULL,KEY_READ,&hKey )!=ERROR_SUCCESS )
		return false;

	unsigned long len=valueLen;
	if ( RegQueryValueEx( hKey,key1,NULL,NULL,( UCHAR* )value,&len )!=ERROR_SUCCESS )
	{		
		RegCloseKey( hKey );
		return false;
	}

	// success
	RegCloseKey( hKey );
	return true;
}

//-----------------------------------------------------------------------------
//	Sys_SetRegistryInteger
//
//-----------------------------------------------------------------------------
BOOL Sys_SetRegistryInteger( const CHAR *keyName, int value )
{
	HKEY	hKey;
	CHAR	key0[256];
	CHAR	key1[256];
	CHAR	keyBuff[256];
	CHAR	*key;

	strcpy_s( keyBuff, sizeof( keyBuff ), g_szRegistryPrefix );
	strcat_s( keyBuff, sizeof( keyBuff ), keyName );
	key = keyBuff;

	HKEY hSlot = HKEY_CURRENT_USER;
	if ( !strncmp( key, "HKEY_LOCAL_MACHINE", 18 ) )
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if ( !strncmp( key, "HKEY_CURRENT_USER", 17 ) )
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	if ( !Sys_SplitRegistryKey( key,key0,256,key1,256 ) )
		return false;

	if ( RegCreateKeyEx( hSlot, key0, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL )!=ERROR_SUCCESS )
		return false;
		
	if ( RegSetValueEx( hKey, key1, NULL, REG_DWORD, ( UCHAR* )&value, 4 )!=ERROR_SUCCESS )
	{
		RegCloseKey( hKey );
		return false;
	}

	// success
	RegCloseKey( hKey );
	return true;
}

//-----------------------------------------------------------------------------
//	Sys_GetRegistryInteger
//
//-----------------------------------------------------------------------------
BOOL Sys_GetRegistryInteger( const CHAR *keyName, int defValue, int &value )
{
	HKEY	hKey;
	CHAR	key0[256];
	CHAR	key1[256];
	CHAR	keyBuff[256];
	CHAR	*key;

	strcpy_s( keyBuff, sizeof( keyBuff ), g_szRegistryPrefix );
	strcat_s( keyBuff, sizeof( keyBuff ), keyName );
	key = keyBuff;

	value = defValue;

	HKEY hSlot = HKEY_CURRENT_USER;
	if ( !strncmp( key, "HKEY_LOCAL_MACHINE", 18 ) )
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if ( !strncmp( key, "HKEY_CURRENT_USER", 17 ) )
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	if ( !Sys_SplitRegistryKey( key, key0, 256, key1, 256 ) )
		return false;

	if ( RegOpenKeyEx( hSlot, key0, NULL, KEY_READ, &hKey ) != ERROR_SUCCESS )
		return false;

	unsigned long len=4;
	if ( RegQueryValueEx( hKey, key1, NULL, NULL, ( UCHAR* )&value, &len ) != ERROR_SUCCESS )
	{		
		RegCloseKey( hKey );
		return false;
	}

	// success
	RegCloseKey( hKey );
	return true;
}

//-----------------------------------------------------------------------------
//	Sys_MessageBox
//
//-----------------------------------------------------------------------------
void Sys_MessageBox( const CHAR* title, const CHAR* format, ... )
{
	CHAR	msg[2048];
	va_list	argptr;

	va_start( argptr, format );
	vsprintf_s( msg, sizeof( msg ), format, argptr );
	va_end( argptr );
	
	MessageBox( NULL, msg, title, MB_OK|MB_TASKMODAL|MB_TOPMOST );
}

//-----------------------------------------------------------------------------
//	Sys_CopyString
//
//-----------------------------------------------------------------------------
CHAR* Sys_CopyString( const CHAR* str )
{
	int len = (int)strlen( str );
	CHAR *ptr = ( CHAR* )Sys_Alloc( len+1 );
	memcpy( ptr,str,len+1 );

	return ( ptr );
}

//-----------------------------------------------------------------------------
//	Sys_Alloc
//
//-----------------------------------------------------------------------------
void* Sys_Alloc( int size )
{
	int*	ptr;

	if ( !size )
	{
		Sys_Error( "Sys_Alloc(): zero size" );
	}

	size = AlignValue( size, 4 );

	// allocate fixed zero init block
	ptr = ( int* )malloc( size );
	if ( !ptr )
	{
		Sys_Error( "Sys_Alloc(): %d bytes not available",size );
	}

	V_memset( ptr, 0, size );

	return ptr;	
}

//-----------------------------------------------------------------------------
//	Sys_Free
//
//-----------------------------------------------------------------------------
void Sys_Free( void* ptr )
{
	if ( !ptr )
	{
		// already freed - easier for me, not really an error
		return;
	}

	free( ptr );
}

//-----------------------------------------------------------------------------
//	Sys_Error
//
//-----------------------------------------------------------------------------
void Sys_Error( const CHAR* format, ... )
{
	va_list		argptr;
	CHAR		msg[MAX_SYSPRINTMSG];
		
	va_start( argptr, format );
	vsprintf_s( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	MessageBox( NULL, msg, "FATAL ERROR", MB_OK|MB_ICONHAND );
}

//-----------------------------------------------------------------------------
//	Sys_LoadFile
//
//-----------------------------------------------------------------------------
int Sys_LoadFile( const CHAR* filename, void** bufferptr, bool bText )
{
	int		handle;
	long	length;
	CHAR*	buffer;

	*bufferptr = NULL;

	if ( !Sys_Exists( filename ) )
	{
		return ( -1 );
	}
		
	int openFlags = bText ? _O_TEXT : _O_BINARY;
	_sopen_s( &handle, filename, _O_RDONLY|openFlags, _SH_DENYWR, _S_IREAD );
	if ( handle == -1 )
	{
		char szError[MAX_PATH];
		strerror_s( szError, sizeof( szError ), errno );
		Sys_Error( "Sys_LoadFile(): Error opening %s: %s", filename, szError );
	}

	// allocate a buffer with an auto null terminator
	length = Sys_FileLength( handle );
	buffer = ( CHAR* )Sys_Alloc( length+1 );

	int numBytesRead = _read( handle, buffer, length );
	_close( handle );

	if ( bText )
	{
		length = numBytesRead;
	}
	else if ( length != numBytesRead )
	{
		Sys_Error( "Sys_LoadFile(): read failure" );
	}
	
	// for parsing
	buffer[length] = '\0';

	*bufferptr = ( void* )buffer;

	return ( length );
}

//-----------------------------------------------------------------------------
//	Sys_SaveFile
//
//-----------------------------------------------------------------------------
bool Sys_SaveFile( const CHAR* filename, void* buffer, long count, bool bText )
{
	int	handle;
	int	status;
	
	int openFlags = bText ? _O_TEXT : _O_BINARY;
	_sopen_s( &handle, filename, _O_RDWR|_O_CREAT|_O_TRUNC|openFlags, _SH_DENYNO, _S_IREAD|_S_IWRITE );
	if ( handle == -1 )
	{
		char szError[MAX_PATH];
		strerror_s( szError, sizeof( szError ), errno );
		Sys_Error( "Sys_SaveFile(): Error opening %s: %s", filename, szError );
		return false;
	}

	status = _write( handle, buffer, count );
	if ( status != count )
	{
		Sys_Error( "Sys_SaveFile(): write failure %d, errno=%d", status, errno );
		return false;
	}

	_close( handle );
	return true;
}

//-----------------------------------------------------------------------------
//	Sys_FileLength
//
//-----------------------------------------------------------------------------
long Sys_FileLength( const CHAR* filename, bool bText )
{
	long	length;

	if ( filename )
	{
		int handle;
		int openFlags = bText ? _O_TEXT : _O_BINARY;
		_sopen_s( &handle, filename, _O_RDONLY|openFlags, _SH_DENYWR, _S_IREAD );
		if ( handle == -1 )
		{
			// file does not exist
			return -1;
		}

		length = _lseek( handle, 0, SEEK_END );
		_close( handle );
	}
	else
	{
		return -1;
	}

	return length;
}

//-----------------------------------------------------------------------------
//	Sys_FileLength
//
//-----------------------------------------------------------------------------
long Sys_FileLength( int handle )
{
	long	pos;
	long	length;

	if ( handle != -1 )
	{
		pos = _lseek( handle, 0, SEEK_CUR );
		length = _lseek( handle, 0, SEEK_END );
		_lseek( handle, pos, SEEK_SET );
	}
	else
	{
		return -1;
	}

	return length;
}

//-----------------------------------------------------------------------------
//	Sys_NormalizePath
//
//-----------------------------------------------------------------------------
void Sys_NormalizePath( CHAR* path, bool forceToLower )
{
	int	i;
	int	srclen;

	srclen = (int)strlen( path );
	for ( i=0; i<srclen; i++ )
	{
		if ( path[i] == '/' )
			path[i] = '\\';
		else if ( forceToLower && ( path[i] >= 'A' && path[i] <= 'Z' ) )
			path[i] = path[i] - 'A' + 'a';
	}
}

//-----------------------------------------------------------------------------
//	Sys_AddFileSeperator
//
//-----------------------------------------------------------------------------
void Sys_AddFileSeperator( CHAR* path, int pathLen )
{
	int	srclen;

	if ( !path[0] )
	{
		strcpy_s( path, pathLen, ".\\" );
		return;
	}

	srclen = (int)strlen( path );
	if ( path[srclen-1] == '\\' )
	{
		return;
	}

	strcat_s( path, pathLen, "\\" );
}

//-----------------------------------------------------------------------------
//	Sys_StripFilename
//
//	Removes filename from path.
//-----------------------------------------------------------------------------
void Sys_StripFilename( const CHAR* inpath, CHAR* outpath, int outPathLen )
{
	int	length;

	strcpy_s( outpath, outPathLen, inpath );

	length = (int)strlen( outpath )-1;
	while ( ( length > 0 ) && ( outpath[length] != '\\' ) && ( outpath[length] != '/' ) && ( outpath[length] != ':' ) )
		length--;

	// leave possible seperator
	if ( !length )
		outpath[0] = '\0';
	else		
		outpath[length+1] = '\0';
}

//-----------------------------------------------------------------------------
//	Sys_StripExtension
//
//	Removes extension from path.
//-----------------------------------------------------------------------------
void Sys_StripExtension( const CHAR* inpath, CHAR* outpath, int outPathLen )
{
	int	length;

	strcpy_s( outpath, outPathLen, inpath );

	length = (int)strlen( outpath )-1;
	while ( length > 0 && outpath[length] != '.' )
	{
		length--;
	}

	if ( length && outpath[length] == '.' )
	{
		outpath[length] = '\0';
	}
}

//-----------------------------------------------------------------------------
//	Sys_StripPath
//
//	Removes path from full path.
//-----------------------------------------------------------------------------
void Sys_StripPath( const CHAR* inpath, CHAR* outpath, int outPathLen )
{
	const CHAR*	src;

	src = inpath + strlen( inpath );
	while ( ( src != inpath ) && ( *( src-1 ) != '\\' ) && ( *( src-1 ) != '/' ) && ( *( src-1 ) != ':' ) )
	{
		src--;
	}

	strcpy_s( outpath, outPathLen, src );
}

//-----------------------------------------------------------------------------
//	Sys_GetExtension
//
//	Gets any extension from the full path.
//-----------------------------------------------------------------------------
void Sys_GetExtension( const CHAR* inpath, CHAR* outpath, int outPathLen )
{
	const CHAR*	src;

	src = inpath + strlen( inpath ) - 1;

	// back up until a . or the start
	while ( src != inpath && *( src-1 ) != '.' )
	{
		src--;
	}

	if ( src == inpath )
	{
		*outpath = '\0';	// no extension
		return;
	}

	strcpy_s( outpath, outPathLen, src );
}

//-----------------------------------------------------------------------------
//	Sys_AddExtension
//
//	Adds extension to end of path.
//-----------------------------------------------------------------------------
void Sys_AddExtension( const CHAR* extension, CHAR* outpath, int outPathLen )
{
	CHAR*	src;
	
	if ( outpath[0] )
	{
		src = outpath + strlen( outpath ) - 1;
		while ( ( src != outpath ) && ( *src != '\\' ) && ( *src != '/' ) )
		{
			if ( *src == '.' )
				return;
			src--;
		}
	}

	strcat_s( outpath, outPathLen, extension );
}

//-----------------------------------------------------------------------------
//	Sys_TempFilename
//
//	Make a temporary filename at specified path.
//-----------------------------------------------------------------------------
void Sys_TempFilename( CHAR* temppath, int tempPathLen )
{
	CHAR*	ptr;
	
	ptr = _tempnam( "c:\\", "tmp" );
	strcpy_s( temppath, tempPathLen, ptr );
	free( ptr );
}

//-----------------------------------------------------------------------------
//	Sys_Exists
//
//	Returns TRUE if file exists.
//-----------------------------------------------------------------------------
BOOL Sys_Exists( const CHAR* filename )
{
	FILE*	test;

	fopen_s( &test, filename, "rb" );
	if ( test == NULL )
	{
		return false;
	}

	fclose( test );

	return true;
}

//-----------------------------------------------------------------------------
//	Sys_SkipWhitespace
//
//-----------------------------------------------------------------------------
CHAR* Sys_SkipWhitespace( CHAR *data, BOOL *hasNewLines, int* numlines ) 
{
	int c;

	while( ( c = *data ) <= ' ' ) 
	{
		if ( c == '\n' ) 
		{
			if ( numlines )
				( *numlines )++;

			if ( hasNewLines )
				*hasNewLines = true;
		}
		else if ( !c )
			return ( NULL );

		data++;
	}

	return ( data );
}

//-----------------------------------------------------------------------------
//	Sys_GetToken
//
//-----------------------------------------------------------------------------
CHAR* Sys_GetToken( CHAR** dataptr, BOOL allowLineBreaks, int* numlines )
{
	CHAR		c;
	int			len;
	BOOL		hasNewLines;
	CHAR*		data;
	static CHAR	token[MAX_SYSTOKENCHARS];

	if ( numlines )
		*numlines = 0;

	c           = 0;
	data        = *dataptr;
	len         = 0;
	token[0]    = 0;
	hasNewLines = false;

	// make sure incoming data is valid
	if ( !data )
	{
		*dataptr = NULL;
		return ( token );
	}

	for ( ;; )
	{
		// skip whitespace
		data = Sys_SkipWhitespace( data,&hasNewLines,numlines );
		if ( !data )
		{
			*dataptr = NULL;
			return ( token );
		}
		
		if ( hasNewLines && !allowLineBreaks )
		{
			*dataptr = data;
			return ( token );
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;
			while ( *data && *data != '\n' )
				data++;
		}
		// skip /* */ comments
		else if ( c =='/' && data[1] == '*' ) 
		{
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) ) 
				data++;

			if ( *data ) 
				data += 2;
		}
		else
			break;
	}

	// handle quoted strings
	if ( c == '\"' )
	{
		data++;
		for ( ;; )
		{
			c = *data++;
			if ( c == '\"' || !c )
			{
				token[len] = 0;
				*dataptr = ( CHAR* )data;
				return ( token );
			}
			if ( len < MAX_SYSTOKENCHARS )
				token[len++] = c;
		}
	}

	// parse a regular word
	do
	{
		if ( len < MAX_SYSTOKENCHARS )
			token[len++] = c;

		data++;
		c = *data;
		if ( c == '\n' && numlines )
			( *numlines )++;
	} while ( c > ' ' );

	if ( len >= MAX_SYSTOKENCHARS ) 
		len = 0;

	token[len] = '\0';
	*dataptr   = ( CHAR* ) data;
	
	return ( token );
}

//-----------------------------------------------------------------------------
//	Sys_SkipBracedSection
//
//	The next token should be an open brace.
//	Skips until a matching close brace is found.
//	Internal brace depths are properly skipped.
//-----------------------------------------------------------------------------
void Sys_SkipBracedSection( CHAR** dataptr, int* numlines ) 
{
	CHAR*	token;
	int	depth;

	depth = 0;
	do 
	{
		token = Sys_GetToken( dataptr, true, numlines );
		if ( token[1] == '\0' ) 
		{
			if ( token[0] == '{' )
				depth++;
			else if ( token[0] == '}' )
				depth--;
		}
	}
	while( depth && *dataptr );
}

//-----------------------------------------------------------------------------
//	Sys_SkipRestOfLine
//
//-----------------------------------------------------------------------------
void Sys_SkipRestOfLine( CHAR** dataptr, int* numlines ) 
{
	CHAR*	p;
	int	c;

	p = *dataptr;
	while ( ( c = *p++ ) != '\0' ) 
	{
		if ( c == '\n' ) 
		{
			if ( numlines )
				( *numlines )++;
			break;
		}
	}
	*dataptr = p;
}

//-----------------------------------------------------------------------------
//	Sys_FileTime
//
//	Returns a file's last write time
//-----------------------------------------------------------------------------
BOOL Sys_FileTime( CHAR* filename, FILETIME* time )
{
	HANDLE		hFile; 
 
	hFile = CreateFile( 
				filename,
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, 
                NULL ); 
 
	if ( hFile == INVALID_HANDLE_VALUE ) 
		return ( false );

    // Retrieve the file times for the file.
    if ( !GetFileTime( hFile, NULL, NULL, time ) )
	{
		CloseHandle( hFile );
		return ( false );
	}

	CloseHandle( hFile );
    return ( true );
}

//-----------------------------------------------------------------------------
//	Sys_GetSystemTime
//
//	Current time marker in milliseconds
//-----------------------------------------------------------------------------
DWORD Sys_GetSystemTime( void )
{
	LARGE_INTEGER	qwTime;
	LARGE_INTEGER	qwTicksPerSec;
	float			msecsPerTick;

	// Get the frequency of the timer
	QueryPerformanceFrequency( &qwTicksPerSec );
	msecsPerTick = 1000.0f/( FLOAT )qwTicksPerSec.QuadPart;

	QueryPerformanceCounter( &qwTime );
	return ( ( DWORD )( msecsPerTick * ( FLOAT )qwTime.QuadPart ) );
}

//-----------------------------------------------------------------------------
//	Sys_ColorScale
//
//-----------------------------------------------------------------------------
COLORREF Sys_ColorScale( COLORREF color, float scale )
{
	int r;
	int	g;
	int	b;

	r = color & 0xFF;
	g = ( color >> 8 ) & 0xFF;
	b = ( color >> 16 ) & 0xFF;

	r = ( int )( ( float )r * scale );
	g = ( int )( ( float )g * scale );
	b = ( int )( ( float )b * scale );

	if ( r > 255 )
		r = 255;
	if ( g > 255 )
		g = 255;
	if ( b > 255 )
		b = 255;

	color = RGB( r, g, b );
	return ( color );
}

//-----------------------------------------------------------------------------
//	Sys_IsWildcardMatch
//
//	See if a string matches a wildcard specification that uses * or ?
//-----------------------------------------------------------------------------
bool Sys_IsWildcardMatch( const CHAR *wildcardString, const CHAR *stringToCheck, bool caseSensitive )
{
	CHAR wcChar;
	CHAR strChar;

	if ( !_stricmp( wildcardString, "*.*" ) || !_stricmp( wildcardString, "*" ) )
	{
		// matches everything
		return true;
	}

	// use the starMatchesZero variable to determine whether an asterisk
	// matches zero or more characters ( TRUE ) or one or more characters
	// ( FALSE )
	bool starMatchesZero = true;

	for ( ;; )
	{
		strChar = *stringToCheck;
		if ( !strChar )
		{
			break;
		}

		wcChar = *wildcardString;
		if ( !wcChar )
		{
			break;
		}

		// we only want to advance the pointers if we successfully assigned
		// both of our char variables, so we'll do it here rather than in the
		// loop condition itself
		*stringToCheck++;
		*wildcardString++;

		// if this isn't a case-sensitive match, make both chars uppercase
		// ( thanks to David John Fielder ( Konan ) at http://innuendo.ev.ca
		// for pointing out an error here in the original code )
		if ( !caseSensitive )
		{
			wcChar = (CHAR)toupper( wcChar );
			strChar = (CHAR)toupper( strChar );
		}

		// check the wcChar against our wildcard list
		switch ( wcChar )
		{
			// an asterisk matches zero or more characters
			case '*' :
				// do a recursive call against the rest of the string,
				// until we've either found a match or the string has
				// ended
				if ( starMatchesZero )
					*stringToCheck--;

				while ( *stringToCheck )
				{
					if ( Sys_IsWildcardMatch( wildcardString, stringToCheck++, caseSensitive ) )
						return true;
				}

				break;

			// a question mark matches any single character
			case '?' :
				break;

			// if we fell through, we want an exact match
			default :
				if ( wcChar != strChar )
					return false;
				break;
		}
	}

	// if we have any asterisks left at the end of the wildcard string, we can
	// advance past them if starMatchesZero is TRUE ( so "blah*" will match "blah" )
	while ( ( *wildcardString ) && ( starMatchesZero ) )
	{
		if ( *wildcardString == '*' )
			wildcardString++;
		else
			break;
	}
	
	// if we got to the end but there's still stuff left in either of our strings,
	// return false; otherwise, we have a match
	if ( ( *stringToCheck ) || ( *wildcardString ) )
		return false;
	else
		return true;
}

//-----------------------------------------------------------------------------
//	Sys_NumberToCommaString
//
//	Add commas to number
//-----------------------------------------------------------------------------
char *Sys_NumberToCommaString( __int64 number, char *buffer, int bufferSize )
{
	char	temp[256];
	char	temp2[256];
	int		inLen;
	char	*inPtr;
	char	*outPtr;
	int		i;

	sprintf_s( temp, sizeof( temp ), "%I64d", number );

	// build string backwards
	inLen  = (int)strlen( temp );
	inPtr  = temp+inLen-1;
	outPtr = temp2;
	while ( inLen > 0 )
	{
		for ( i=0; i<3 && inLen > 0; i++, inLen-- )
		{
			*outPtr++ = *inPtr--;
		}
		if ( inLen > 0 )
			*outPtr++ = ',';
	}
	*outPtr++ = '\0';

	// reverse string
	inLen  = (int)strlen( temp2 );
	inPtr  = temp2;
	outPtr = temp;
	for ( i=inLen-1; i>=0; i-- )
	{
		*outPtr++ = inPtr[i];
	}
	*outPtr++ = '\0';

	_snprintf_s( buffer, bufferSize, _TRUNCATE, temp );

	return buffer;
}

//-----------------------------------------------------------------------------
//	Sys_CreatePath
//
//-----------------------------------------------------------------------------
void Sys_CreatePath( const char* pInPath )
{
	char*	ptr;
	char	dirPath[MAX_PATH];

	// prime and skip to first seperator
	strcpy_s( dirPath, sizeof( dirPath ), pInPath );

	if ( dirPath[0] == '\\' && dirPath[1] == '\\' )
	{
		ptr = strchr( dirPath+1, '\\' );
	}
	else
	{
		ptr = strchr( dirPath, '\\' );
	}

	while ( ptr )
	{		
		ptr = strchr( ptr+1, '\\' );
		if ( ptr )
		{
			*ptr = '\0';
			CreateDirectory( dirPath, NULL );
			*ptr = '\\';
		}
	}
}

