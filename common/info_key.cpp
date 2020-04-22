//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "info_key.h"

#include <stdio.h>
#include <string.h>
#include "tier1/strtools.h"

#define MAX_KV_LEN 127
/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
const char *Info_ValueForKey ( const char *s, const char *key )
{
	char	pkey[512];
	static	char value[4][512];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;
	
	valueindex = (valueindex + 1) % 4;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!stricmp(key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey ( char *s, const char *key )
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strstr (key, "\\"))
	{
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!stricmp (key, pkey) )
		{
			// This is safe because we're copying within the same string
			Q_strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}

void Info_RemovePrefixedKeys (char *start, char prefix)
{
	char	*s;
	char	pkey[512];
	char	value[512];
	char	*o;

	s = start;

	while (1)
	{
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (pkey[0] == prefix)
		{
			Info_RemoveKey (start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}
}

bool Info_IsKeyImportant( const char *key )
{
	if ( key[0] == '*' )
		return true;
	if ( !stricmp( key, "tracker" ) )
		return true;

	return false;
}

char *Info_FindLargestKey( char *s, int maxsize )
{
	char	key[256];
	char	value[256];
	char	*o;
	int		l;
	static char largest_key[256];
	int     largest_size = 0;

	*largest_key = 0;

	if (*s == '\\')
		s++;
	while (*s)
	{
		int size = 0;

		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		*o = 0;
		size = strlen( key );

		if (!*s)
		{
			return largest_key;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;

		size += strlen( value );

		if ( (size > largest_size) && !Info_IsKeyImportant(key) )
		{
			largest_size = size;
			Q_strncpy( largest_key, key, sizeof( largest_key ) );
		}
	}

	return largest_key;
}

void Info_SetValueForStarKey ( char *s, const char *key, const char *value, int maxsize )
{
	char	news[1024], *v;
	int		c;

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
		return;
	}

	if (strstr (key, "..") || strstr (value, "..") )
	{
//		Con_Printf ("Can't use keys or values with a ..\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		return;
	}

	if (strlen(key) > MAX_KV_LEN || strlen(value) > MAX_KV_LEN)
	{
		return;
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	Q_snprintf (news, sizeof( news ), "\\%s\\%s", key, value);

 	if ( (int)(strlen(news) + strlen(s)) >= maxsize)
	{
		// no more room in buffer to add key/value
		if ( Info_IsKeyImportant( key ) )
		{
			// keep removing the largest key/values until we have room
			char *largekey;
			do {
				largekey = Info_FindLargestKey( s, maxsize );
				Info_RemoveKey( s, largekey );
			} while ( ((int)(strlen(news) + strlen(s)) >= maxsize) && *largekey != 0 );

			if ( largekey[0] == 0 )
			{
				// no room to add setting
				return;
			}
		}
		else
		{
			// no room to add setting
			return;
		}
	}

	// only copy ascii values
	s += strlen(s);
	v = news;
	while (*v)
	{
		c = (unsigned char)*v++;

		// Strip out high ascii characters
		c &= 127;

		if (c > 13)
		{
			*s++ = c;
		}
	}
	*s = 0;
}

void Info_SetValueForKey (char *s, const char *key, const char *value, int maxsize)
{
	if (key[0] == '*')
	{
		return;
	}

	Info_SetValueForStarKey (s, key, value, maxsize);
}



