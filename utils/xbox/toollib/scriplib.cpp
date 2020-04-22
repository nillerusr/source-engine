#include "toollib.h"

char	g_tl_token[MAXTOKEN];
char*	g_tl_scriptbuff;
char*	g_tl_scriptptr;
char*	g_tl_scriptendptr;
int		g_tl_scriptsize;
int		g_tl_scriptline;
bool	g_tl_endofscript;
bool	g_tl_tokenready;
int		g_tl_oldscriptline;
char*	g_tl_oldscriptptr;

/*****************************************************************************
	TL_FreeScriptFile

*****************************************************************************/
void TL_FreeScriptFile(void)
{
	if (g_tl_scriptbuff)
	{
		TL_Free(g_tl_scriptbuff);
		g_tl_scriptbuff = NULL;
	}
}

/*****************************************************************************
	TL_LoadScriptFile

*****************************************************************************/
void TL_LoadScriptFile(const char* filename)
{
	g_tl_scriptsize = TL_LoadFile(filename,(void **)&g_tl_scriptbuff);

	TL_ResetParser();
}

/*****************************************************************************
	TL_SetScriptData

*****************************************************************************/
void TL_SetScriptData(char* data, int length)
{
	g_tl_scriptbuff = data;
	g_tl_scriptsize = length;

	TL_ResetParser();
}

/*****************************************************************************
	TL_UnGetToken

*****************************************************************************/
void TL_UnGetToken(void)
{
	g_tl_tokenready = true;
}

/*****************************************************************************
	TL_GetToken

*****************************************************************************/
char* TL_GetToken(bool crossline)
{
	char*	tokenptr;

	if (g_tl_tokenready)
	{
		g_tl_tokenready = false;
		return (g_tl_token);
	}

	g_tl_token[0] = '\0';

	if (g_tl_scriptptr >= g_tl_scriptendptr)
	{
		if (!crossline)
			TL_Error("TL_GetToken: Line %d is incomplete",g_tl_scriptline);

		g_tl_endofscript = true;
		return (NULL);
	}

skipspace:
	while (*g_tl_scriptptr <= ' ')
	{
		if (g_tl_scriptptr >= g_tl_scriptendptr)
		{
			if (!crossline)
				TL_Error("GetToken: Line %i is incomplete",g_tl_scriptline);

			g_tl_endofscript = true;
			return (NULL);
		}

		if (*g_tl_scriptptr++ == '\n')
		{
			if (!crossline)
				TL_Error("GetToken: Line %i is incomplete",g_tl_scriptline);

			g_tl_scriptline++;
		}
	}

	if (g_tl_scriptptr >= g_tl_scriptendptr)
	{
		if (!crossline)
			TL_Error("GetToken: Line %i is incomplete",g_tl_scriptline);

		g_tl_endofscript = true;
		return (NULL);
	}

	// skip commented line
	if ((g_tl_scriptptr[0] == ';') || (g_tl_scriptptr[0] == '/' && g_tl_scriptptr[1] == '/'))
	{
		if (!crossline)
			TL_Error("GetToken: Line %i is incomplete",g_tl_scriptline);

		while (*g_tl_scriptptr++ != '\n')
		{
			if (g_tl_scriptptr >= g_tl_scriptendptr)
			{
				g_tl_endofscript = true;
				return (NULL);
			}
		}

		g_tl_scriptline++;
		goto skipspace;
	}

	tokenptr = g_tl_token;
	if (g_tl_scriptptr[0] == '\"' && g_tl_scriptptr[1])
	{
		// copy quoted token
		do
		{
			*tokenptr++ = *g_tl_scriptptr++;
			if (g_tl_scriptptr == g_tl_scriptendptr)
				break;

			if (tokenptr == &g_tl_token[MAXTOKEN])
				TL_Error("GetToken: Token too large on line %i",g_tl_scriptline);
		}
		while (*g_tl_scriptptr >= ' ' && *g_tl_scriptptr != '\"');

		if (g_tl_scriptptr[0] == '\"')
			*tokenptr++ = *g_tl_scriptptr++;
	}
	else
	{
		// copy token
		while (*g_tl_scriptptr > ' ' && *g_tl_scriptptr != ';' && *g_tl_scriptptr != '\"')
		{
			*tokenptr++ = *g_tl_scriptptr++;
			if (g_tl_scriptptr == g_tl_scriptendptr)
				break;

			if (tokenptr == &g_tl_token[MAXTOKEN])
				TL_Error("GetToken: Token too large on line %i",g_tl_scriptline);
		}
	}

	*tokenptr = '\0';

	return (g_tl_token);
}

/*****************************************************************************
	TL_SkipRestOfLine

*****************************************************************************/
void TL_SkipRestOfLine(void)
{
	while (*g_tl_scriptptr++ != '\n')
	{
		if (g_tl_scriptptr >= g_tl_scriptendptr)
		{
			break;
		}
	}

	g_tl_scriptline++;
}

/*****************************************************************************
	TL_TokenAvailable

	Returns (TRUE) if token available on line.
*****************************************************************************/
bool TL_TokenAvailable (void)
{
	char*	ptr;
	
	ptr = g_tl_scriptptr;
	while (*ptr <= ' ')
	{
		if (ptr >= g_tl_scriptendptr)
		{
			g_tl_endofscript = true;
			return (false);
		}

		if (*ptr++ == '\n')
			return (false);
	}

	return (true);
}


/*****************************************************************************
	TL_EndOfScript

	Returns (TRUE) at end of script
*****************************************************************************/
bool TL_EndOfScript(void)
{
	if (g_tl_scriptptr >= g_tl_scriptendptr)
	{
		g_tl_endofscript = true;
		return (true);
	}

	return (false);
}

/*****************************************************************************
	TL_ResetParser

*****************************************************************************/
void TL_ResetParser(void)
{
	g_tl_scriptptr    = g_tl_scriptbuff;
	g_tl_scriptendptr = g_tl_scriptptr + g_tl_scriptsize;
	g_tl_scriptline   = 1;
	g_tl_endofscript  = false;
	g_tl_tokenready   = false;
}

/*****************************************************************************
	TL_SaveParser

*****************************************************************************/
void TL_SaveParser(void)
{
	g_tl_oldscriptline = g_tl_scriptline;
	g_tl_oldscriptptr  = g_tl_scriptptr;		
}

/*****************************************************************************
	TL_RestoreParser

*****************************************************************************/
void TL_RestoreParser(void)
{
	g_tl_scriptline = g_tl_oldscriptline;
	g_tl_scriptptr  = g_tl_oldscriptptr;		
}




