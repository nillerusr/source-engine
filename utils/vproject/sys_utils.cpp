//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	SYS_UTILS.C
//
//=====================================================================================//
#include "sys_utils.h"

//-----------------------------------------------------------------------------
//	Sys_SplitRegistryKey
//
//-----------------------------------------------------------------------------
static BOOL Sys_SplitRegistryKey( const CHAR *key, CHAR *key0, int key0Len, CHAR *key1, int key1Len )
{
	if ( !key )
		return false;
	
	int len = (int)strlen( key );
	if ( !len )
		return false;

	int Start = -1;
	for ( int i = len-1; i >= 0; i-- )
	{
		if ( key[i] == '\\' )
			break;
		else
			Start=i;
	}

	if ( Start == -1 )
		return false;
	
	_snprintf( key0, Start, key );
	key0[Start] = '\0';

	_snprintf( key1, ( len-Start )+1, key+Start );
	key1[( len-Start )+1] = '\0';

	return true;
}

//-----------------------------------------------------------------------------
//	Sys_SetRegistryString
//
//-----------------------------------------------------------------------------
BOOL Sys_SetRegistryString( const CHAR *key, const CHAR *value )
{
	HKEY	hKey;
	CHAR	key0[256];
	CHAR	key1[256];

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
BOOL Sys_GetRegistryString( const CHAR *key, CHAR *value, const CHAR* defValue, int valueLen )
{
	HKEY	hKey;
	CHAR	key0[256];
	CHAR	key1[256];

	if ( defValue )
		_snprintf( value, valueLen, defValue );

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
BOOL Sys_SetRegistryInteger( const CHAR *key, int value )
{
	HKEY	hKey;
	CHAR	key0[256];
	CHAR	key1[256];

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
BOOL Sys_GetRegistryInteger( const CHAR *key, int defValue, int &value )
{
	HKEY	hKey;
	CHAR	key0[256];
	CHAR	key1[256];

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
//	Sys_NormalizePath
//
//-----------------------------------------------------------------------------
void Sys_NormalizePath( CHAR* path, bool forceToLower )
{
	int srcLen = (int)strlen( path );
	for ( int i=0; i<srcLen; i++ )
	{
		if ( path[i] == '/' )
			path[i] = '\\';
		else if ( forceToLower && ( path[i] >= 'A' && path[i] <= 'Z' ) )
			path[i] = path[i] - 'A' + 'a';
	}
}

//-----------------------------------------------------------------------------
//	Sys_SkipWhitespace
//
//-----------------------------------------------------------------------------
CHAR* Sys_SkipWhitespace( CHAR *data, BOOL *hasNewLines, int* pNumLines ) 
{
	int c;

	while( ( c = *data ) <= ' ' ) 
	{
		if ( c == '\n' ) 
		{
			if ( pNumLines )
				(*pNumLines)++;

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
//	Sys_PeekToken
//
//-----------------------------------------------------------------------------
CHAR* Sys_PeekToken( CHAR *dataptr, BOOL bAllowLineBreaks )
{
	CHAR	*saved;
	CHAR	*pToken;

	saved  = dataptr;
	pToken = Sys_GetToken( &saved, bAllowLineBreaks, NULL );

	return pToken;
}

//-----------------------------------------------------------------------------
//	Sys_GetToken
//
//-----------------------------------------------------------------------------
CHAR* Sys_GetToken( CHAR** dataptr, BOOL allowLineBreaks, int* pNumLines )
{
	CHAR		c;
	int			len;
	BOOL		hasNewLines;
	CHAR*		data;
	static CHAR	token[MAX_SYSTOKENCHARS];

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
		data = Sys_SkipWhitespace( data, &hasNewLines, pNumLines );
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

		if ( c == '/' && data[1] == '/' )
		{
			// skip double slash comments
			data += 2;
			while ( *data && *data != '\n' )
				data++;
			if ( *data && *data == '\n' )
			{
				data++;
				if ( pNumLines )
					(*pNumLines)++;
			}
		}
		else if ( c =='/' && data[1] == '*' ) 
		{
			// skip /* */ comments
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) )
			{
				if ( *data == '\n' && pNumLines )
					(*pNumLines)++;
				data++;
			}

			if ( *data ) 
				data += 2;
		}
		else
			break;
	}

	// handle quoted strings
	if ( c == '\"' || c == '<' )
	{
		data++;
		for ( ;; )
		{
			c = *data++;
			if ( c == '\"' || c == '>' || !c )
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
	} 
	while ( c > ' ' );

	if ( len >= MAX_SYSTOKENCHARS ) 
		len = 0;

	token[len] = '\0';
	*dataptr   = (CHAR*)data;
	
	return token;
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

void Sys_StripQuotesFromToken( CHAR *pToken )
{
	int len;

	len = (int)strlen( pToken );
	if ( len >= 2 && pToken[0] == '\"' )
	{
		memcpy( pToken, pToken+1, len-1 );
		pToken[len-2] = '\0';
	}
}


