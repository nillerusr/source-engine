//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "util.h"
#include <string.h>

#define NUM_BUFFERS 4
#define MAX_INFO_TOKEN_LENGTH 512
const char *CUtil::InfoGetValue( const char *s, const char *key )
{
	char	pkey[MAX_INFO_TOKEN_LENGTH];
	// Use multiple buffers so compares
	// work without stomping on each other
	static	char value[NUM_BUFFERS][MAX_INFO_TOKEN_LENGTH];	
	static	int	valueindex;
	char	*o;
	
	valueindex = (valueindex + 1) % NUM_BUFFERS;

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

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: This function is supposed to localise the strings, but for now just return internal value
// Input  : *stringName - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CUtil::GetString(const char *stringName)
{
	return stringName;
}

static CUtil g_Util;
CUtil *util = &g_Util;


