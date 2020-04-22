//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	SYS_SCRIPTLIB.CPP
//
//
//=====================================================================================//
#include "vxconsole.h"

char	g_sys_token[MAXTOKEN];
char*	g_sys_scriptbuff;
char*	g_sys_scriptptr;
char*	g_sys_scriptendptr;
int		g_sys_scriptsize;
int		g_sys_scriptline;
bool	g_sys_endofscript;
bool	g_sys_tokenready;
int		g_sys_oldscriptline;
char*	g_sys_oldscriptptr;

//-----------------------------------------------------------------------------
//	Sys_FreeScriptFile
//
//-----------------------------------------------------------------------------
void Sys_FreeScriptFile(void)
{
	if (g_sys_scriptbuff)
	{
		Sys_Free(g_sys_scriptbuff);
		g_sys_scriptbuff = NULL;
	}
}

//-----------------------------------------------------------------------------
//	Sys_LoadScriptFile
//
//-----------------------------------------------------------------------------
void Sys_LoadScriptFile(const char* filename)
{
	g_sys_scriptsize = Sys_LoadFile(filename,(void **)&g_sys_scriptbuff);

	Sys_ResetParser();
}

//-----------------------------------------------------------------------------
//	Sys_SetScriptData
//
//-----------------------------------------------------------------------------
void Sys_SetScriptData(const char* data, int length)
{
	g_sys_scriptbuff = (char *)data;
	g_sys_scriptsize = length;

	Sys_ResetParser();
}

//-----------------------------------------------------------------------------
//	Sys_UnGetToken
//
//-----------------------------------------------------------------------------
void Sys_UnGetToken(void)
{
	g_sys_tokenready = true;
}

//-----------------------------------------------------------------------------
//	Sys_GetToken
//
//-----------------------------------------------------------------------------
char* Sys_GetToken(bool crossline)
{
	char*	tokenptr;

	if (g_sys_tokenready)
	{
		g_sys_tokenready = false;
		return (g_sys_token);
	}

	g_sys_token[0] = '\0';

	if (g_sys_scriptptr >= g_sys_scriptendptr)
	{
		g_sys_endofscript = true;
		return (NULL);
	}

skipspace:
	while (*g_sys_scriptptr <= ' ')
	{
		if (g_sys_scriptptr >= g_sys_scriptendptr)
		{
			g_sys_endofscript = true;
			return (NULL);
		}

		if (*g_sys_scriptptr++ == '\n')
		{
			if (!crossline)
			{
				// unexpected newline at g_sys_scriptline
				return (NULL);
			}

			g_sys_scriptline++;
		}
	}

	if (g_sys_scriptptr >= g_sys_scriptendptr)
	{
		g_sys_endofscript = true;
		return (NULL);
	}

	// skip commented line
	if ((g_sys_scriptptr[0] == ';') || (g_sys_scriptptr[0] == '/' && g_sys_scriptptr[1] == '/'))
	{
		if (!crossline)
		{
			// unexpected newline at g_sys_scriptline
			return (NULL);
		}

		while (*g_sys_scriptptr++ != '\n')
		{
			if (g_sys_scriptptr >= g_sys_scriptendptr)
			{
				g_sys_endofscript = true;
				return (NULL);
			}
		}

		g_sys_scriptline++;
		goto skipspace;
	}

	tokenptr = g_sys_token;
	if (g_sys_scriptptr[0] == '\"' && g_sys_scriptptr[1])
	{
		// copy quoted token
		do
		{
			*tokenptr++ = *g_sys_scriptptr++;
			if (g_sys_scriptptr == g_sys_scriptendptr)
				break;

			if (tokenptr == &g_sys_token[MAXTOKEN])
			{
				// token too large
				return NULL;
			}
		}
		while (*g_sys_scriptptr >= ' ' && *g_sys_scriptptr != '\"');

		if (g_sys_scriptptr[0] == '\"')
			*tokenptr++ = *g_sys_scriptptr++;
	}
	else
	{
		// copy token
		while (*g_sys_scriptptr > ' ' && *g_sys_scriptptr != ';' && *g_sys_scriptptr != '\"')
		{
			*tokenptr++ = *g_sys_scriptptr++;
			if (g_sys_scriptptr == g_sys_scriptendptr)
				break;

			if (tokenptr == &g_sys_token[MAXTOKEN])
			{
				// token too large
				return NULL;
			}
		}
	}

	*tokenptr = '\0';

	return (g_sys_token);
}

//-----------------------------------------------------------------------------
//	Sys_SkipRestOfLine
//
//-----------------------------------------------------------------------------
void Sys_SkipRestOfLine(void)
{
	while (*g_sys_scriptptr++ != '\n')
	{
		if (g_sys_scriptptr >= g_sys_scriptendptr)
		{
			break;
		}
	}

	g_sys_scriptline++;

	// flush any queued token
	g_sys_tokenready = false;
}

//-----------------------------------------------------------------------------
//	Sys_TokenAvailable
//
//	Returns (TRUE) if token available on line.
//-----------------------------------------------------------------------------
bool Sys_TokenAvailable (void)
{
	char*	ptr;
	
	ptr = g_sys_scriptptr;
	while (*ptr <= ' ')
	{
		if (ptr >= g_sys_scriptendptr)
		{
			g_sys_endofscript = true;
			return (false);
		}

		if (*ptr++ == '\n')
			return (false);
	}

	return (true);
}


//-----------------------------------------------------------------------------
//	Sys_EndOfScript
//
//	Returns (TRUE) at end of script
//-----------------------------------------------------------------------------
bool Sys_EndOfScript(void)
{
	if (g_sys_scriptptr >= g_sys_scriptendptr)
	{
		g_sys_endofscript = true;
		return (true);
	}

	return (false);
}

//-----------------------------------------------------------------------------
//	Sys_ResetParser
//
//-----------------------------------------------------------------------------
void Sys_ResetParser(void)
{
	g_sys_scriptptr    = g_sys_scriptbuff;
	g_sys_scriptendptr = g_sys_scriptptr + g_sys_scriptsize;
	g_sys_scriptline   = 1;
	g_sys_endofscript  = false;
	g_sys_tokenready   = false;
}

//-----------------------------------------------------------------------------
//	Sys_SaveParser
//
//-----------------------------------------------------------------------------
void Sys_SaveParser(void)
{
	g_sys_oldscriptline = g_sys_scriptline;
	g_sys_oldscriptptr  = g_sys_scriptptr;		
}

//-----------------------------------------------------------------------------
//	Sys_RestoreParser
//
//-----------------------------------------------------------------------------
void Sys_RestoreParser(void)
{
	g_sys_scriptline = g_sys_oldscriptline;
	g_sys_scriptptr  = g_sys_oldscriptptr;		
	g_sys_tokenready = false;
}

//-----------------------------------------------------------------------------
//	Sys_StripQuotesFromToken
//
//-----------------------------------------------------------------------------
void Sys_StripQuotesFromToken( char *pToken )
{
	int len;

	len = strlen( pToken );
	if ( len >= 2 && pToken[0] == '\"' )
	{
		memcpy( pToken, pToken+1, len-1 );
		pToken[len-2] = '\0';
	}
}



