//========= Copyright 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

void CVPC::SetMacro( const char *pName, const char *pValue, bool bSetupDefineInProjectFile )
{
	// Setup the macro.
	VPCStatus( false, "Set Macro: $%s = %s", pName, pValue );

	macro_t *pMacro = FindOrCreateMacro( pName, true, pValue );
	pMacro->m_bSetupDefineInProjectFile = bSetupDefineInProjectFile;
	pMacro->m_bInternalCreatedMacro = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
macro_t *CVPC::FindOrCreateMacro( const char *pName, bool bCreate, const char *pValue )
{
	for ( int i = 0; i < m_Macros.Count(); i++ )
	{
		if ( !V_stricmp( pName, m_Macros[i].name.String() ) )
		{
			if ( pValue && V_stricmp( pValue, m_Macros[i].value.String() ) )
			{
				// update
				m_Macros[i].value = pValue;
			}

			return &m_Macros[i];
		}
	}

	if ( !bCreate )
	{
		return NULL;
	}

	int index = m_Macros.AddToTail();
	m_Macros[index].name  = pName;
	m_Macros[index].value = pValue;

	return &m_Macros[index];
}

int CVPC::GetMacrosMarkedForCompilerDefines( CUtlVector< macro_t* > &macroDefines )
{
	macroDefines.Purge();

	for ( int i = 0; i < m_Macros.Count(); i++ )
	{
		if ( m_Macros[i].m_bSetupDefineInProjectFile )
		{
			macroDefines.AddToTail( &m_Macros[i] );
		}
	}

	return macroDefines.Count();
}

static int __cdecl SortMacrosByNameLength( const macro_t *lhs, const macro_t *rhs )
{
	if ( lhs->name.Length() < rhs->name.Length() )
		return 1;
	if ( lhs->name.Length() > rhs->name.Length() )
		return -1;
	return 0;
}

void CVPC::ResolveMacrosInStringInternal( char const *pString, char *pOutBuff, int outBuffSize, bool bStringIsConditional )
{
	char	macroName[MAX_SYSTOKENCHARS];
	char	buffer1[MAX_SYSTOKENCHARS];
	char	buffer2[MAX_SYSTOKENCHARS];
	int		i;

	// ensure a "greedy" match by sorting longest to shortest
	m_Macros.Sort( SortMacrosByNameLength );

	// iterate and resolve user macros until all macros resolved
	strcpy( buffer1, pString );
	bool bDone;
	do
	{
		bDone = true;
		bool bDoReplace = true;
		for ( i=0; i<m_Macros.Count(); i++ )
		{
			sprintf( macroName, "$%s", m_Macros[i].name.String() );
			const char *pFound = V_stristr( buffer1, macroName );
			if ( pFound && bStringIsConditional )
			{
				// if expanding a conditional, give conditionals priority over macros
				// i.e. if the string we've found begins both a macro and conditional,
				// don't expand the macro
				for ( int j = 0; j < m_Conditionals.Count(); j++ )
				{
					if ( V_stristr( pFound+1, m_Conditionals[j].name.String() ) == pFound+1 )
					{
						bDoReplace = false;
						// the warning is super chatty about $LINUX and $POSIX
						if ( V_stricmp( macroName, "$LINUX" ) && V_stricmp( macroName, "$POSIX" ) )
							g_pVPC->VPCWarning( "Not replacing macro %s with its value (%s) in conditional %s\n", macroName, m_Macros[i].value.Length() ? m_Macros[i].value.String() : "null" , pFound );
						break;
					}
				}
			}

			// can't use ispunct as '|' and '&' are punctuation, but we dont want to warn on them
			if ( pFound && bStringIsConditional && bDoReplace &&
				( isalnum( pFound[strlen(macroName)] ) || pFound[strlen(macroName)] == '_' ) )
				g_pVPC->VPCWarning( "Replacing macro %s with its value (%s) in conditional %s\n", macroName, m_Macros[i].value.Length() ? m_Macros[i].value.String() : "null" , pFound );

			if ( bDoReplace && Sys_ReplaceString( buffer1, macroName, m_Macros[i].value.String(), buffer2, sizeof( buffer2 ) ) )
			{
				bDone = false;
			}
			strcpy( buffer1, buffer2 );
		}
	}
	while ( !bDone );

	int len = strlen( buffer1 );
	if ( outBuffSize < len )
		len = outBuffSize;
	memcpy( pOutBuff, buffer1, len );
	pOutBuff[len] = '\0';
}

void CVPC::ResolveMacrosInString( char const *pString, char *pOutBuff, int outBuffSize )
{
	ResolveMacrosInStringInternal( pString, pOutBuff, outBuffSize, false );
}

void CVPC::ResolveMacrosInConditional( char const *pString, char *pOutBuff, int outBuffSize )
{
	ResolveMacrosInStringInternal( pString, pOutBuff, outBuffSize, true );
}

void CVPC::RemoveScriptCreatedMacros()
{
	for ( int i=0; i < m_Macros.Count(); i++ )
	{
		if ( !m_Macros[i].m_bInternalCreatedMacro )
		{
			m_Macros.Remove( i );
			--i;
		}
	}
}

const char *CVPC::GetMacroValue( const char *pName )
{
	for ( int i = 0; i < m_Macros.Count(); i++ )
	{
		if ( !V_stricmp( pName, m_Macros[i].name.String() ) )
		{
			return m_Macros[i].value.String();
		}
	}

	// not found
	return "";
}
