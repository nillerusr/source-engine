//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

static KeywordName_t s_KeywordNameTable[] =
{
	{"$General",				KEYWORD_GENERAL},
	{"$Debugging",				KEYWORD_DEBUGGING},
	{"$Compiler",				KEYWORD_COMPILER},
	{"$SNCCompiler", 			KEYWORD_PS3_SNCCOMPILER},
	{"$GCCCompiler",			KEYWORD_PS3_GCCCOMPILER},
	{"$Librarian",				KEYWORD_LIBRARIAN},
	{"$Linker",					KEYWORD_LINKER},
	{"$SNCLinker",				KEYWORD_PS3_SNCLINKER},
	{"$GCCLinker",				KEYWORD_PS3_GCCLINKER},
	{"$ManifestTool",			KEYWORD_MANIFEST},
	{"$XMLDocumentGenerator",	KEYWORD_XMLDOCGEN},
	{"$BrowseInformation",		KEYWORD_BROWSEINFO},
	{"$Resources",				KEYWORD_RESOURCES},
	{"$PreBuildEvent",			KEYWORD_PREBUILDEVENT},
	{"$PreLinkEvent",			KEYWORD_PRELINKEVENT},
	{"$PostBuildEvent",			KEYWORD_POSTBUILDEVENT},
	{"$CustomBuildStep",		KEYWORD_CUSTOMBUILDSTEP},
	{"$Xbox360ImageConversion",	KEYWORD_XBOXIMAGE},
	{"$ConsoleDeployment",		KEYWORD_XBOXDEPLOYMENT},
};

const char *CVPC::KeywordToName( configKeyword_e keyword )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_KeywordNameTable ) == KEYWORD_MAX );

	if ( keyword == KEYWORD_UNKNOWN )
	{
		return "???";
	}

	return s_KeywordNameTable[keyword].m_pName;
}

configKeyword_e CVPC::NameToKeyword( const char *pKeywordName )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_KeywordNameTable ) == KEYWORD_MAX );

	for ( int i = 0; i < ARRAYSIZE( s_KeywordNameTable ); i++ )
	{
		if ( !V_stricmp( pKeywordName, s_KeywordNameTable[i].m_pName ) )
		{
			return s_KeywordNameTable[i].m_Keyword;
		}
	}

	return KEYWORD_UNKNOWN;
}

//-----------------------------------------------------------------------------
//	VPC_Config_Keyword
//
//-----------------------------------------------------------------------------
void VPC_Config_Keyword( configKeyword_e keyword, const char *pkeywordToken )
{
	const char		*pToken;

	bool bShouldSkip = false;
	if ( !g_pVPC->GetProjectGenerator()->StartPropertySection( keyword, &bShouldSkip ) )
	{
		g_pVPC->VPCSyntaxError( "Unsupported Keyword: %s for target platform", pkeywordToken );
	}

	if ( bShouldSkip )
	{
		pToken = g_pVPC->GetScript().PeekNextToken( true );
		if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
			g_pVPC->VPCSyntaxError();

		g_pVPC->GetScript().SkipBracedSection();
	}
	else
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
			g_pVPC->VPCSyntaxError();
		
		while ( 1 )
		{
			pToken = g_pVPC->GetScript().GetToken( true );
			if ( !pToken || !pToken[0] )
				break;

			if ( !V_stricmp( pToken, "}" ) )
			{
				// end of section
				break;
			}

			// Copy off the token name so HandleProperty() doesn't have to (or else the parser will overwrite it on the next token).
			char tempTokenName[MAX_PATH];
			V_strncpy( tempTokenName, pToken, sizeof( tempTokenName ) );

			g_pVPC->GetProjectGenerator()->HandleProperty( tempTokenName );
		}
	}

	g_pVPC->GetProjectGenerator()->EndPropertySection( keyword );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_Configuration
//
//-----------------------------------------------------------------------------
void VPC_Keyword_Configuration()
{
	const char					*pToken;
	char					szConfigName[MAX_PATH];
	bool					bAllowNextLine = false;
	int						i;
	CUtlVector<CUtlString>	configs;
	char					buff[MAX_SYSTOKENCHARS];

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;

		if ( !V_stricmp( pToken, "\\" ) )
		{
			bAllowNextLine = true;
			continue;
		}
		else
		{
			bAllowNextLine = false;
		}

		int index = configs.AddToTail();
		configs[index] = pToken;

		// check for another optional config
		pToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
		if ( !pToken || !pToken[0] || !V_stricmp( pToken, "{" ) || !V_stricmp( pToken, "}" ) || (pToken[0] == '$') )
			break;
	}

	// no configuration specified, use all known
	if ( !configs.Count() )
	{
		g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configs );
		if ( !configs.Count() )
		{
			g_pVPC->VPCError( "Trying to parse a configuration block and no configs have been defined yet.\n[%s line:%d]", g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetLine() );
		}
	}

	// save parser state
	CScriptSource scriptSource = g_pVPC->GetScript().GetCurrentScript();

	for ( i=0; i<configs.Count(); i++ )
	{
		// restore parser state
		g_pVPC->GetScript().RestoreScript( scriptSource );

		V_strncpy( szConfigName, configs[i].String(), sizeof( szConfigName ) );

		// get access objects
		g_pVPC->GetProjectGenerator()->StartConfigurationBlock( szConfigName, false );

		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
		{
			g_pVPC->VPCSyntaxError();
		}

		while ( 1 )
		{
			g_pVPC->GetScript().SkipToValidToken();
			if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, buff, sizeof( buff ) ) )
			{
				g_pVPC->GetScript().SkipBracedSection();
				continue;
			}

			if ( !V_stricmp( buff, "}" ) )
			{
				// end of section
				break;
			}

			configKeyword_e keyword = g_pVPC->NameToKeyword( buff );
			if ( keyword == KEYWORD_UNKNOWN )
			{
				g_pVPC->VPCSyntaxError();
			}
			else
			{
				VPC_Config_Keyword( keyword, buff );
			}
		}

		g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
	}
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_FileConfiguration
//
//-----------------------------------------------------------------------------
void VPC_Keyword_FileConfiguration()
{
	const char	*pToken;
	char		szConfigName[MAX_PATH];
	bool		bAllowNextLine = false;
	char		buff[MAX_SYSTOKENCHARS];
	CUtlVector< CUtlString > configurationNames;

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;

		if ( !V_stricmp( pToken, "\\" ) )
		{
			bAllowNextLine = true;
			continue;
		}
		else
		{
			bAllowNextLine = false;
		}

		strcpy( szConfigName, pToken );
		configurationNames.AddToTail( pToken );

		// check for another optional config
		pToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
		if ( !pToken || !pToken[0] || !V_stricmp( pToken, "{" ) || !V_stricmp( pToken, "}" ) || (pToken[0] == '$') )
			break;
	}

	// no configuration specified, use all known
	if ( configurationNames.Count() == 0 )
	{
		g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
	}

	// save parser state
	CScriptSource scriptSource = g_pVPC->GetScript().GetCurrentScript();

	for ( int i=0; i < configurationNames.Count(); i++ )
	{
		// restore parser state
		g_pVPC->GetScript().RestoreScript( scriptSource );

		// Tell the generator we're about to feed it configuration data for this file.
		g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), true );

		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
		{
			g_pVPC->VPCSyntaxError();
		}

		while ( 1 )
		{
			g_pVPC->GetScript().SkipToValidToken();

			pToken = g_pVPC->GetScript().PeekNextToken( true );
			if ( pToken && pToken[0] && !V_stricmp( pToken, "$ExcludedFromBuild" ) )
			{
				pToken = g_pVPC->GetScript().GetToken( true );
				if ( !pToken || !pToken[0] )
					g_pVPC->VPCSyntaxError();

				char buff[MAX_SYSTOKENCHARS];
				if ( g_pVPC->GetScript().ParsePropertyValue( NULL, buff, sizeof( buff ) ) )
				{
					g_pVPC->GetProjectGenerator()->FileExcludedFromBuild( Sys_StringToBool( buff ) );
				}

				continue;
			}

			if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, buff, sizeof( buff ) ) )
			{
				g_pVPC->GetScript().SkipBracedSection();
				continue;
			}

			if ( !V_stricmp( buff, "}" ) )
			{
				// end of section
				break;
			}

			configKeyword_e keyword = g_pVPC->NameToKeyword( buff );
			switch ( keyword )
			{
			case KEYWORD_COMPILER:
			case KEYWORD_PS3_SNCCOMPILER:
			case KEYWORD_PS3_GCCCOMPILER:
			case KEYWORD_RESOURCES:
			case KEYWORD_CUSTOMBUILDSTEP:
				VPC_Config_Keyword( keyword, buff );
				break;
			default:
				g_pVPC->VPCSyntaxError();
			}
		}
		
		g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
	}
}


//-----------------------------------------------------------------------------
//	Just advances past config keywords without acting on them
//
//-----------------------------------------------------------------------------
void VPC_Read_Config_Keywords( const char *pkeywordToken )
{
	const char		*pToken;

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
		g_pVPC->VPCSyntaxError();

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
			break;

		if ( !V_stricmp( pToken, "}" ) )
		{
			// end of section
			break;
		}
	}
}


//-----------------------------------------------------------------------------
//	Read a configuration block that applies to an entire folder
//
//-----------------------------------------------------------------------------
void VPC_Keyword_FolderConfiguration( folderConfig_t *pFolderConfig )
{
	pFolderConfig->Clear();

	const char	*pToken;
	char		szConfigName[MAX_PATH];
	bool		bAllowNextLine = false;
	char		buff[MAX_SYSTOKENCHARS];

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;

		if ( !V_stricmp( pToken, "\\" ) )
		{
			bAllowNextLine = true;
			continue;
		}
		else
		{
			bAllowNextLine = false;
		}

		strcpy( szConfigName, pToken );
		pFolderConfig->vecConfigurationNames.AddToTail( pToken );

		// check for another optional config
		pToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
		if ( !pToken || !pToken[0] || !V_stricmp( pToken, "{" ) || !V_stricmp( pToken, "}" ) || ( pToken[0] == '$' ) )
			break;
	}

	// no configuration specified, use all known
	if ( pFolderConfig->vecConfigurationNames.Count() == 0 )
	{
		g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( pFolderConfig->vecConfigurationNames );
	}

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
	{
		g_pVPC->VPCSyntaxError();
	}
	
	// save parser state so we remember where the block is.  We'll refer back to it when handlign individual files.
	pFolderConfig->scriptSource = g_pVPC->GetScript().GetCurrentScript();

	// just read past all the tokens.  We'll reparse this later, per file.
	//	it would be cool to parse just once, but leaf code in the project generator expects the parser to be in the right position and parses directly.
	while ( 1 )
	{
		g_pVPC->GetScript().SkipToValidToken();

		if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, buff, sizeof( buff ) ) )
		{
			g_pVPC->GetScript().SkipBracedSection();
			continue;
		}

		if ( !V_stricmp( buff, "}" ) )
		{
			// end of section
			break;
		}

		configKeyword_e keyword = g_pVPC->NameToKeyword( buff );
		switch ( keyword )
		{
		case KEYWORD_COMPILER:
		case KEYWORD_PS3_SNCCOMPILER:
		case KEYWORD_PS3_GCCCOMPILER:
		case KEYWORD_RESOURCES:
		case KEYWORD_CUSTOMBUILDSTEP:
			{
				VPC_Read_Config_Keywords( buff );
			}
			break;
		default:
			g_pVPC->VPCSyntaxError();
		}
	}
}


//-----------------------------------------------------------------------------
//	Apply a folder-wide configuration to an individual file
//
//-----------------------------------------------------------------------------
void VPC_ApplyFolderConfigurationToFile( const folderConfig_t &folderConfig )
{
	char		buff[MAX_SYSTOKENCHARS];
	g_pVPC->GetScript().PushCurrentScript();

	for ( int i = 0; i < folderConfig.vecConfigurationNames.Count(); i++ )
	{
		g_pVPC->GetScript().RestoreScript( folderConfig.scriptSource );
		
		// Tell the generator we're about to feed it configuration data for this file.
		g_pVPC->GetProjectGenerator()->StartConfigurationBlock( folderConfig.vecConfigurationNames[i].String(), true );

		while ( 1 )
		{
			g_pVPC->GetScript().SkipToValidToken();

			if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, buff, sizeof( buff ) ) )
			{
				g_pVPC->GetScript().SkipBracedSection();
				continue;
			}

			if ( !V_stricmp( buff, "}" ) )
			{
				// end of section
				break;
			}

			configKeyword_e keyword = g_pVPC->NameToKeyword( buff );
			switch ( keyword )
			{
			case KEYWORD_COMPILER:
			case KEYWORD_PS3_SNCCOMPILER:
			case KEYWORD_PS3_GCCCOMPILER:
			case KEYWORD_RESOURCES:
			case KEYWORD_CUSTOMBUILDSTEP:
				VPC_Config_Keyword( keyword, buff );
				break;
			default:
				g_pVPC->VPCSyntaxError();
			}
		}
		
		g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
	}

	g_pVPC->GetScript().PopScript();
}