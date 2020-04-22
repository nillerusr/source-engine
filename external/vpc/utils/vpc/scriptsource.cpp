//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//
//
//==================================================================================================

#include "vpc.h"

#define MAX_SCRIPT_STACK_SIZE	32

CScript::CScript()
{
	m_ScriptName = "(empty)";
	m_nScriptLine = 0;
	m_pScriptData = NULL;
	m_pScriptLine = &m_nScriptLine;

	m_Token[0] = '\0';
	m_PeekToken[0] = '\0';
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CScript::SkipWhitespace( const char *data, bool *pHasNewLines, int* pNumLines ) 
{
	int c;

	while ( ( c = *data ) <= ' ' ) 
	{
		if ( c == '\n' ) 
		{
			if ( pNumLines )
			{
				(*pNumLines)++;
			}

			if ( pHasNewLines )
			{
				*pHasNewLines = true;
			}
		}
		else if ( !c )
		{
			return ( NULL );
		}

		data++;
	}

	return data;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CScript::SkipToValidToken( const char *data, bool *pHasNewLines, int* pNumLines ) 
{
	int c;

	for ( ;; )
	{
		data = SkipWhitespace( data, pHasNewLines, pNumLines );

		c = *data;
		if ( !c )
		{
			break;
		}

		if ( c == '/' && data[1] == '/' )
		{
			// skip double slash comments
			data += 2;
			while ( *data && *data != '\n' )
			{
				data++;
			}
			if ( *data && *data == '\n' )
			{
				data++;
				if ( pNumLines )
				{
					(*pNumLines)++;
				}
				if ( pHasNewLines )
				{
					*pHasNewLines = true;
				}
			}
		}
		else if ( c == '/' && data[1] == '*' ) 
		{
			// skip /* */ comments
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) )
			{
				if ( *data == '\n' )
				{
					if ( pNumLines )
					{
						(*pNumLines)++;
					}
					if ( pHasNewLines )
					{
						*pHasNewLines = true;
					}
				}
				data++;
			}

			if ( *data ) 
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	return data;
}

//-----------------------------------------------------------------------------
//	The next token should be an open brace.
//	Skips until a matching close brace is found.
//	Internal brace depths are properly skipped.
//-----------------------------------------------------------------------------
void CScript::SkipBracedSection( const char** dataptr, int* numlines ) 
{
	const char*	token;
	int	depth;

	depth = 0;
	do 
	{
		token = GetToken( dataptr, true, numlines );
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
//-----------------------------------------------------------------------------
void CScript::SkipRestOfLine( const char** dataptr, int* numlines ) 
{
	const char*	p;
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
// Does not corrupt results obtained with GetToken().
//-----------------------------------------------------------------------------
const char* CScript::PeekNextToken( const char *dataptr, bool bAllowLineBreaks )
{
	// save the primary token, about to be corrupted
	char savedToken[MAX_SYSTOKENCHARS];
	V_strncpy( savedToken, m_Token, MAX_SYSTOKENCHARS );

	const char *pSaved = dataptr;
	const char *pToken = GetToken( &pSaved, bAllowLineBreaks, NULL );

	// restore
	V_strncpy( m_PeekToken, pToken, MAX_SYSTOKENCHARS );
	V_strncpy( m_Token, savedToken, MAX_SYSTOKENCHARS );

	return m_PeekToken;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CScript::GetToken( const char **dataptr, bool allowLineBreaks, int *pNumLines )
{
	char		c;
	char		endSymbol;
	int			len;
	bool		hasNewLines;
	const char*		data;

	c           = 0;
	data        = *dataptr;
	len         = 0;
	m_Token[0]  = 0;
	hasNewLines = false;

	// make sure incoming data is valid
	if ( !data )
	{
		*dataptr = NULL;
		return m_Token;
	}

	for ( ;; )
	{
		// skip whitespace
		data = SkipWhitespace( data, &hasNewLines, pNumLines );
		if ( !data )
		{
			*dataptr = NULL;
			return m_Token;
		}
		
		if ( hasNewLines && !allowLineBreaks )
		{
			*dataptr = data;
			return m_Token;
		}

		c = *data;

		if ( c == '/' && data[1] == '/' )
		{
			// skip double slash comments
			data += 2;
			while ( *data && *data != '\n' )
			{
				data++;
			}
			if ( *data && *data == '\n' )
			{
				if ( !allowLineBreaks )
					continue;

				data++;
				if ( pNumLines )
				{
					(*pNumLines)++;
				}
			}
		}
		else if ( c =='/' && data[1] == '*' ) 
		{
			// skip /* */ comments
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) )
			{
				if ( *data == '\n' && pNumLines )
				{
					(*pNumLines)++;
				}
				data++;
			}

			if ( *data ) 
			{
				data += 2;
			}
		}
		else
			break;
	}

	// handle scoped strings "???" <???> [???]
	if ( c == '\"' || c == '<' || c == '[')
	{
		bool bConditionalExpression = false;
		endSymbol = '\0';
		switch ( c )
		{
		case '\"':
			endSymbol = '\"';
			break;
		case '<':
			endSymbol = '>';
			break;
		case '[':
			bConditionalExpression = true;
			endSymbol = ']';
			break;
		}

		// want to preserve entire conditional expession [blah...blah...blah]
		// maintain a conditional's open/close scope characters
		if ( !bConditionalExpression )
		{
			// skip past scope character
			data++;
		}

		for ( ;; )
		{
			c = *data++;

			if ( c == endSymbol || !c )
			{
				if ( c == endSymbol && bConditionalExpression )
				{
					// keep end symbol
					m_Token[len++] = c;
				}
			
				m_Token[len] = 0;
				*dataptr = (char*)data;
				return m_Token;
			}

			if ( len < MAX_SYSTOKENCHARS-1 )
			{
				m_Token[len++] = c;
			}
		}
	}

	// parse a regular word
	do
	{
		if ( len < MAX_SYSTOKENCHARS )
		{
			m_Token[len++] = c;
		}

		data++;
		c = *data;
	} 
	while ( c > ' ' );

	if ( len >= MAX_SYSTOKENCHARS ) 
	{
		len = 0;
	}

	m_Token[len] = '\0';
	*dataptr = (char*)data;
	
	return m_Token;
}

void CScript::PushScript( const char *pFilename )
{
	// parse the text script
	if ( !Sys_Exists( pFilename ) )
	{
		g_pVPC->VPCError( "Cannot open %s", pFilename );	
	}

	char *pScriptBuffer;
	Sys_LoadTextFileWithIncludes( pFilename, &pScriptBuffer, false );

	PushScript( pFilename, pScriptBuffer, 1, true );
}

void CScript::PushScript( const char *pScriptName, const char *pScriptData, int nScriptLine, bool bFreeScriptAtPop )
{
	if ( m_ScriptStack.Count() > MAX_SCRIPT_STACK_SIZE )
	{
		g_pVPC->VPCError( "PushScript( scriptname=%s ) - stack overflow\n", pScriptName );
	}

	// Push the current state onto the stack.
	m_ScriptStack.Push( GetCurrentScript() );

	// Set their state as the current state.
	m_ScriptName = pScriptName;
	m_pScriptData = pScriptData;
	m_nScriptLine = nScriptLine;
	m_bFreeScriptAtPop = bFreeScriptAtPop;
}

void CScript::PushCurrentScript()
{
	PushScript( m_ScriptName.Get(), m_pScriptData, m_nScriptLine, m_bFreeScriptAtPop );
}

CScriptSource CScript::GetCurrentScript()
{
	return CScriptSource( m_ScriptName.Get(), m_pScriptData, m_nScriptLine, m_bFreeScriptAtPop );
}

void CScript::RestoreScript( const CScriptSource &scriptSource )
{
	m_ScriptName = scriptSource.GetName();
	m_pScriptData = scriptSource.GetData();
	m_nScriptLine = scriptSource.GetLine();
	m_bFreeScriptAtPop = scriptSource.IsFreeScriptAtPop();
}

void CScript::PopScript()
{
	if ( m_ScriptStack.Count() == 0 )
	{
		g_pVPC->VPCError( "PopScript(): stack is empty" );
	}

	if ( m_bFreeScriptAtPop && m_pScriptData )
	{
		free( (void *)m_pScriptData );
	}

	// Restore the top entry on the stack and pop it off.
	const CScriptSource &state = m_ScriptStack.Top();
	m_ScriptName = state.GetName();
	m_pScriptData = state.GetData();
	m_nScriptLine = state.GetLine();
	m_bFreeScriptAtPop = state.IsFreeScriptAtPop();

	m_ScriptStack.Pop();
}

void CScript::EnsureScriptStackEmpty()
{
	if ( m_ScriptStack.Count() != 0 )
	{
		g_pVPC->VPCError( "EnsureScriptStackEmpty(): script stack is not empty!" );
	}
}

void CScript::SpewScriptStack()
{
	if ( m_ScriptStack.Count() )
	{
		CUtlString str;

		// emit stack with current at top
		str += "Script Stack:\n";
		str += CFmtStr( "   %s Line:%d\n", m_ScriptName.String(), m_nScriptLine );
		for ( int i = m_ScriptStack.Count() - 1; i >= 0; i-- )
		{
			if ( i == 0 && !m_ScriptStack[i].GetData() && m_ScriptStack[i].GetLine() <= 0 )
			{
				// ignore empty bottom of stack
				break;
			}

			str += CFmtStr( "   %s Line:%d\n", m_ScriptStack[i].GetName(), m_ScriptStack[i].GetLine() );
		}
		str += "\n";		

		Log_Msg( LOG_VPC, "%s", str.String() );
	}
}

const char *CScript::GetToken( bool bAllowLineBreaks )
{
	return GetToken( &m_pScriptData, bAllowLineBreaks, m_pScriptLine );
}

const char *CScript::PeekNextToken( bool bAllowLineBreaks )
{
	return PeekNextToken( m_pScriptData, bAllowLineBreaks );
}

void CScript::SkipRestOfLine() 
{
	SkipRestOfLine( &m_pScriptData, m_pScriptLine );
}

void CScript::SkipBracedSection() 
{
	SkipBracedSection( &m_pScriptData, m_pScriptLine );
}

void CScript::SkipToValidToken() 
{
	m_pScriptData = SkipToValidToken( m_pScriptData, NULL, m_pScriptLine );
}

//-----------------------------------------------------------------------------
//	Handles expressions of the form <$BASE> <xxx> ... <xxx> [condition]
//	Output is a concatenated string.
//
//	Returns true if expression should be used, false if it should be ignored due
//	to an optional condition that evaluated false.
//-----------------------------------------------------------------------------
bool CScript::ParsePropertyValue( const char *pBaseString, char *pOutBuff, int outBuffSize )
{
	const char **pScriptData = &m_pScriptData;
	int *pScriptLine = m_pScriptLine; 

	const char	*pToken;
	const char	*pNextToken;
	char	*pOut = pOutBuff;
	int		remaining = outBuffSize-1;
	int		len;
	bool	bAllowNextLine = false;
	char	buffer1[MAX_SYSTOKENCHARS];
	char	buffer2[MAX_SYSTOKENCHARS];
	bool	bResult = true;

	while ( 1 )
	{
		pToken = GetToken( pScriptData, bAllowNextLine, pScriptLine );
		if ( !pToken || !pToken[0] )
			g_pVPC->VPCSyntaxError();

		pNextToken = PeekNextToken( *pScriptData, false );
		if ( !pNextToken || !pNextToken[0] )
		{
			// current token is last token
			// last token can be optional conditional, need to identify
			// backup and reparse up to last token
			if ( pToken && pToken[0] == '[' )
			{
				// last token is an optional conditional
				bResult = g_pVPC->EvaluateConditionalExpression( pToken );
				break;
			}
		}

		if ( !V_stricmp( pToken, "\\" ) )
		{
			bAllowNextLine = true;
			continue;
		}
		else
		{
			bAllowNextLine = false;
		}

		if ( !V_stricmp( pToken, "\\n" ) )
		{
			pToken = "\n";
		}

		if ( pToken[0] )
		{
			// handle reserved macro
			if ( !pBaseString )
				pBaseString = "";
			strcpy( buffer1, pToken );
			Sys_ReplaceString( buffer1, "$base", pBaseString, buffer2, sizeof( buffer2 ) );

			g_pVPC->ResolveMacrosInString( buffer2, buffer1, sizeof( buffer1 ) );

			len = strlen( buffer1 );
			if ( remaining < len )
				len = remaining;

			if ( len > 0 )
			{
				memcpy( pOut, buffer1, len );
				pOut += len;
				remaining -= len;
			}
		}

		pToken = PeekNextToken( *pScriptData, false );
		if ( !pToken || !pToken[0] || !V_stricmp( pNextToken, "}" ) )
			break;
	}

	*pOut++ = '\0';

	if ( !pOutBuff[0] )
		g_pVPC->VPCSyntaxError();

	return bResult;
}