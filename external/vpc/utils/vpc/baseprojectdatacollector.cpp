//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "vpc.h"
#include "baseprojectdatacollector.h"
#include "tier1/utlstack.h"
#include "p4lib/ip4.h"

static const char * const s_rgsAmbiguousPropertyNames[] =
{
    "$CommandLine",
};

// ------------------------------------------------------------------------------------------------ //
// CSpecificConfig implementation.
// ------------------------------------------------------------------------------------------------ //

CSpecificConfig::CSpecificConfig( CSpecificConfig *pParentConfig )
	: m_pParentConfig( pParentConfig )
{
	m_pKV = new KeyValues( "" );
	m_bFileExcluded = false;
	m_bIsSchema = false;
	m_bIsDynamic = false;
}

CSpecificConfig::~CSpecificConfig()
{
	m_pKV->deleteThis();
}

const char* CSpecificConfig::GetConfigName()
{
	return m_pKV->GetName();
}

const char* CSpecificConfig::GetOption( const char *pOptionName )
{
	const char *pRet = m_pKV->GetString( pOptionName, NULL );
	if ( pRet )
		return pRet;

	if ( m_pParentConfig )
		return m_pParentConfig->m_pKV->GetString( pOptionName, NULL );

	return NULL;
}


// ------------------------------------------------------------------------------------------------ //
// CFileConfig implementation.
// ------------------------------------------------------------------------------------------------ //

CFileConfig::~CFileConfig()
{
	Term();
}

void CFileConfig::Term()
{
	m_Configurations.PurgeAndDeleteElements();
}

const char* CFileConfig::GetName()
{
	return m_Filename.String();
}

CSpecificConfig* CFileConfig::GetConfig( const char *pConfigName )
{
	int i = m_Configurations.Find( pConfigName );
	if ( i == m_Configurations.InvalidIndex() )
		return NULL;
	else
		return m_Configurations[i];
}

CSpecificConfig* CFileConfig::GetOrCreateConfig( const char *pConfigName, CSpecificConfig *pParentConfig )
{
	int i = m_Configurations.Find( pConfigName );
	if ( i == m_Configurations.InvalidIndex() )
	{
		CSpecificConfig *pConfig = new CSpecificConfig( pParentConfig );
		i = m_Configurations.Insert( pConfigName, pConfig );
	}

	return m_Configurations[i];
}

bool CFileConfig::IsExcludedFrom( const char *pConfigName )
{
	CSpecificConfig *pSpecificConfig = GetConfig( pConfigName );
	if ( pSpecificConfig )
		return pSpecificConfig->m_bFileExcluded;
	else
		return false;
}

bool CFileConfig::IsDynamicFile( const char *pConfigName )
{
	CSpecificConfig *pSpecificConfig = GetConfig( pConfigName );
	if ( pSpecificConfig )
		return pSpecificConfig->m_bIsDynamic;
	else
		return false;
}


// ------------------------------------------------------------------------------------------------ //
// CBaseProjectDataCollector implementation.
// ------------------------------------------------------------------------------------------------ //

CBaseProjectDataCollector::CBaseProjectDataCollector( CRelevantPropertyNames *pNames ) : m_Files( k_eDictCompareTypeFilenames )
{
	m_RelevantPropertyNames.m_nNames = 0;
	m_RelevantPropertyNames.m_pNames = NULL;

	if ( pNames )
	{
		m_RelevantPropertyNames = *pNames;
	}
}

CBaseProjectDataCollector::~CBaseProjectDataCollector()
{
	Term();
}

void CBaseProjectDataCollector::StartProject()
{
    for (int i = 0; i < m_RelevantPropertyNames.m_nNames; i++)
    {
        for (int j = 0; j < V_ARRAYSIZE(s_rgsAmbiguousPropertyNames); j++ )
        {
            if ( V_stricmp( m_RelevantPropertyNames.m_pNames[i], s_rgsAmbiguousPropertyNames[j] ) == 0 )
                g_pVPC->VPCWarning( "Property name %s may occur in multiple contexts and should be fully qualified", m_RelevantPropertyNames.m_pNames[i] );
        }
    }
	m_ProjectName = "UNNAMED";
	m_CurFileConfig.Push( &m_BaseConfigData );
	m_CurSpecificConfig.Push( NULL );
}

void CBaseProjectDataCollector::EndProject()
{
}

void CBaseProjectDataCollector::Term()
{
	m_BaseConfigData.Term();
	m_Files.PurgeAndDeleteElements();
	m_CurFileConfig.Purge();
	m_CurSpecificConfig.Purge();
}

CUtlString CBaseProjectDataCollector::GetProjectName()
{
	return m_ProjectName;
}

void CBaseProjectDataCollector::SetProjectName( const char *pProjectName )
{
	char tmpBuf[MAX_PATH];
	V_strncpy( tmpBuf, pProjectName, sizeof( tmpBuf ) );
	V_strlower( tmpBuf );
	m_ProjectName = tmpBuf;
}

// Get a list of all configurations.
void CBaseProjectDataCollector::GetAllConfigurationNames( CUtlVector< CUtlString > &configurationNames )
{
	configurationNames.Purge();
	for ( int i=m_BaseConfigData.m_Configurations.First(); i != m_BaseConfigData.m_Configurations.InvalidIndex(); i=m_BaseConfigData.m_Configurations.Next(i) )
	{
		configurationNames.AddToTail( m_BaseConfigData.m_Configurations.GetElementName(i) );
	}
}

void CBaseProjectDataCollector::StartConfigurationBlock( const char *pConfigName, bool bFileSpecific )
{
	CFileConfig *pFileConfig = m_CurFileConfig.Top();

	// Find or add a new config block.
	char sLowerCaseConfigName[MAX_PATH];
	V_strncpy( sLowerCaseConfigName, pConfigName, sizeof( sLowerCaseConfigName ) );
	V_strlower( sLowerCaseConfigName );

	int index = pFileConfig->m_Configurations.Find( sLowerCaseConfigName );
	if ( index == -1 )
	{
		CSpecificConfig *pParent = ( pFileConfig==&m_BaseConfigData ? NULL : m_BaseConfigData.GetOrCreateConfig( sLowerCaseConfigName, NULL ) );

		CSpecificConfig *pConfig = new CSpecificConfig( pParent );
		pConfig->m_bFileExcluded = false;
		pConfig->m_pKV->SetName( sLowerCaseConfigName );
		index = pFileConfig->m_Configurations.Insert( sLowerCaseConfigName, pConfig );
	}

	// Remember what the current config is.
	m_CurSpecificConfig.Push( pFileConfig->m_Configurations[index] );
}

void CBaseProjectDataCollector::EndConfigurationBlock()
{
	m_CurSpecificConfig.Pop();
}

bool CBaseProjectDataCollector::StartPropertySection( configKeyword_e keyword, bool *pbShouldSkip )
{
    m_CurPropertySection.Push( keyword );
    return true;
}

void CBaseProjectDataCollector::HandleProperty( const char *pProperty, const char *pCustomScriptData )
{
    CFmtStr sQualifiedProperty( "%s%s%s", m_CurPropertySection.Count() ? g_pVPC->KeywordToName( m_CurPropertySection.Top() ) : "",
                                m_CurPropertySection.Count() ? "/" : "",
                                pProperty );
    bool bSetQualifiedProperty = false;
	int i;
	for ( i=0; i < m_RelevantPropertyNames.m_nNames; i++ )
	{
		if ( V_stricmp( m_RelevantPropertyNames.m_pNames[i], pProperty ) == 0 )
			break;
		if ( V_stricmp( m_RelevantPropertyNames.m_pNames[i], sQualifiedProperty.Access() ) == 0 )
        {
            bSetQualifiedProperty = true;
			break;
        }
	}
	if ( i == m_RelevantPropertyNames.m_nNames )
	{
		// not found
		return;
	}

	if ( pCustomScriptData )
	{
		g_pVPC->GetScript().PushScript( "HandleProperty( custom script data )", pCustomScriptData );
	}

	const char *pNextToken = g_pVPC->GetScript().PeekNextToken( false );
	if ( pNextToken && pNextToken[0] != 0 )
	{
		// Pass in the previous value so the $base substitution works.
		CSpecificConfig *pConfig = m_CurSpecificConfig.Top();
		const char *pBaseString = pConfig->m_pKV->GetString( bSetQualifiedProperty ? sQualifiedProperty.Access() : pProperty );
		char buff[MAX_SYSTOKENCHARS];
		if ( g_pVPC->GetScript().ParsePropertyValue( pBaseString, buff, sizeof( buff ) ) )
		{
			pConfig->m_pKV->SetString( bSetQualifiedProperty ? sQualifiedProperty.Access() : pProperty, buff );
		}
	}

	if ( pCustomScriptData )
	{
		// Restore prior script state
		g_pVPC->GetScript().PopScript();
	}
}

void CBaseProjectDataCollector::EndPropertySection( configKeyword_e keyword )
{
	configKeyword_e kw;
	m_CurPropertySection.Pop( kw );
	Assert( kw == keyword );
}

void CBaseProjectDataCollector::StartFolder( const char *pFolderName )
{
}
void CBaseProjectDataCollector::EndFolder()
{
}

bool CBaseProjectDataCollector::StartFile( const char *pFilename, bool bWarnIfAlreadyExists )
{
	CFileConfig *pFileConfig = new CFileConfig;
	pFileConfig->m_Filename = pFilename;
	pFileConfig->m_nInsertOrder = m_Files.Count();
	m_Files.Insert( pFilename, pFileConfig );

	m_CurFileConfig.Push( pFileConfig );
	m_CurSpecificConfig.Push( NULL );

	char szFullPath[MAX_PATH];

	V_GetCurrentDirectory( szFullPath, sizeof( szFullPath ) );
	V_AppendSlash( szFullPath, sizeof( szFullPath ) );
	V_strncat( szFullPath, pFilename, sizeof( szFullPath ) );
	V_RemoveDotSlashes( szFullPath );

#if 0
	// Add file to Perforce if it isn't there already
	if ( Sys_Exists( szFullPath ) )
	{
		if ( m_bP4AutoAdd && p4 && !p4->IsFileInPerforce( szFullPath ) )
		{
			p4->OpenFileForAdd( szFullPath );
			VPCStatus( "%s automatically opened for add in default changelist.", szFullPath );

		}
	}
	else
	{
		// g_pVPC->Warning( "%s not found on disk at location specified in project script.", szFullPath );
	}
#endif

	return true;
}

void CBaseProjectDataCollector::EndFile()
{
	m_CurFileConfig.Pop();
	m_CurSpecificConfig.Pop();
}

// This is actually just per-file configuration data.
void CBaseProjectDataCollector::FileExcludedFromBuild( bool bExcluded )
{
	CSpecificConfig *pConfig = m_CurSpecificConfig.Top();
	pConfig->m_bFileExcluded = bExcluded;
}

void CBaseProjectDataCollector::FileIsSchema( bool bIsSchema )
{
	CSpecificConfig *pConfig = m_CurSpecificConfig.Top();
	pConfig->m_bIsSchema = bIsSchema;
}

void CBaseProjectDataCollector::FileIsDynamic( bool bIsDynamic )
{
	CSpecificConfig *pConfig = m_CurSpecificConfig.Top();
	pConfig->m_bIsDynamic = bIsDynamic;
}


bool CBaseProjectDataCollector::RemoveFile( const char *pFilename )
{
	bool bRet = false;
	int i = m_Files.Find( pFilename );
	if ( i != m_Files.InvalidIndex() )
	{
		delete m_Files[i];
		m_Files.RemoveAt( i );
		bRet = true;
	}
	return bRet;
}

void CBaseProjectDataCollector::DoStandardVisualStudioReplacements( const char *pStartString, const char *pFullInputFilename, char *pOut, int outLen )
{
	// Decompose the input filename.
	char sInputDir[MAX_PATH], sFileBase[MAX_PATH];
	if ( !V_ExtractFilePath( pFullInputFilename, sInputDir, sizeof( sInputDir ) ) )
		V_strcpy( sInputDir, "." );

	V_FileBase( pFullInputFilename, sFileBase, sizeof( sFileBase ) );

	// Handle $(InputPath), $(InputDir), $(InputName)
	char *strings[2] =
		{
			(char*)stackalloc( outLen ),
			(char*)stackalloc( outLen )
		};

	V_StrSubst( pStartString, "$(InputPath)", pFullInputFilename, strings[0], outLen );
	V_StrSubst( strings[0], "$(InputDir)", sInputDir, strings[1], outLen );
	V_StrSubst( strings[1], "$(InputName)", sFileBase, strings[0], outLen );
	V_StrSubst( strings[0], "$(IntDir)", "$(OBJ_DIR)", strings[1], outLen );
	V_StrSubst( strings[1], "$(InputFileName)", pFullInputFilename + V_strlen(sInputDir), strings[0], outLen );
	V_StrSubst( strings[0], "$(ConfigurationName)", "${CONFIGURATION}", strings[1], outLen );
	V_StrSubst( strings[1], "$(Configuration)", "${CONFIGURATION}", strings[0], outLen );

	V_strncpy( pOut, strings[0], outLen );
	V_FixSlashes( pOut, '/' );
}
