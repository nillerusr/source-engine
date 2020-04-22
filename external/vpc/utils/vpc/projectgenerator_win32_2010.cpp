//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

#undef PROPERTYNAME
#define PROPERTYNAME( X, Y ) { X##_##Y, #X, #Y },
static PropertyName_t s_Win32PropertyNames_2010[] =
{
	#include "projectgenerator_win32_2010.inc"
	{ -1, NULL, NULL }
};

IBaseProjectGenerator* GetWin32ProjectGenerator_2010()
{
	static CProjectGenerator_Win32_2010 *s_pProjectGenerator = NULL;
	if ( !s_pProjectGenerator )
	{
		s_pProjectGenerator = new CProjectGenerator_Win32_2010();
	}

	return s_pProjectGenerator->GetProjectGenerator();
}

CProjectGenerator_Win32_2010::CProjectGenerator_Win32_2010()
{
	m_pVCProjGenerator = new CVCProjGenerator();
	m_pVCProjGenerator->SetupGeneratorDefinition( this, "win32_2010.def", s_Win32PropertyNames_2010 );
}

enum TypeKeyNames_e
{
	TKN_LIBRARY = 0,
	TKN_INCLUDE,
	TKN_COMPILE,
	TKN_RESOURCECOMPILE,
	TKN_CUSTOMBUILD,
	TKN_NONE,
	TKN_MAX_COUNT,
};

static const char *s_TypeKeyNames[] =
{
	"Library",
	"ClInclude",
	"ClCompile",
	"ResourceCompile",
	"CustomBuild",
	"None"
};

const char *CProjectGenerator_Win32_2010::GetKeyNameForFile( CProjectFile *pFile )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_TypeKeyNames ) == TKN_MAX_COUNT );

	const char *pExtension = V_GetFileExtension( pFile->m_Name.Get() );

	const char *pKeyName = s_TypeKeyNames[TKN_NONE];
	if ( pExtension )
	{
		if ( pFile->m_Configs.Count() && pFile->m_Configs[0]->GetCustomBuildTool() )
		{
			pKeyName = s_TypeKeyNames[TKN_CUSTOMBUILD];
		}
		else if ( IsCFileExtension( pExtension ) )
		{
			pKeyName = s_TypeKeyNames[TKN_COMPILE];
		}
		else if ( IsHFileExtension( pExtension ) )
		{
			pKeyName = s_TypeKeyNames[TKN_INCLUDE];
		}
		else if ( !V_stricmp( pExtension, "lib" ) )
		{
			pKeyName = s_TypeKeyNames[TKN_LIBRARY];
		}
		else if ( !V_stricmp( pExtension, "rc" ) )
		{
			pKeyName = s_TypeKeyNames[TKN_RESOURCECOMPILE];
		}
	}

	return pKeyName;
}

bool CProjectGenerator_Win32_2010::WritePropertyGroupTool( CProjectTool *pProjectTool, CProjectConfiguration *pConfiguration )
{
	if ( !pProjectTool )
		return true;

	for ( int i = 0; i < pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
	{
		int sortedIndex = pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder[i];
		if ( !pProjectTool->m_PropertyStates.m_Properties[sortedIndex].m_pToolProperty->m_bEmitAsGlobalProperty )
			continue;

		if ( !WriteProperty( &pProjectTool->m_PropertyStates.m_Properties[sortedIndex], true, pConfiguration->m_Name.Get() ) )
			return false;
	}

	return true;
}

bool CProjectGenerator_Win32_2010::WriteFile( CProjectFile *pFile, const char *pFileTypeName )
{
	const char *pKeyName = GetKeyNameForFile( pFile );
	if ( V_stricmp( pFileTypeName, pKeyName ) )
	{
		// skip it
		return true;
	}

	if ( !pFile->m_Configs.Count() )
	{
		m_XMLWriter.Write( CFmtStrMax( "<%s Include=\"%s\" />", pKeyName, pFile->m_Name.Get() ) );
	}
	else
	{
		m_XMLWriter.PushNode( pKeyName, CFmtStr( "Include=\"%s\"", pFile->m_Name.Get() ) );

		for ( int i = 0; i < pFile->m_Configs.Count(); i++ )
		{
			if ( !WriteConfiguration( pFile->m_Configs[i] ) )
				return false;
		}

		m_XMLWriter.PopNode( true );
	}
	
	return true;
}

bool CProjectGenerator_Win32_2010::WriteFolder( CProjectFolder *pFolder, const char *pFileTypeName, int nDepth )
{
	if ( !nDepth )
	{
		m_XMLWriter.PushNode( "ItemGroup" );
	}

	for ( int iIndex = pFolder->m_Files.Head(); iIndex != pFolder->m_Files.InvalidIndex(); iIndex = pFolder->m_Files.Next( iIndex ) )
	{
		if ( !WriteFile( pFolder->m_Files[iIndex], pFileTypeName ) )
			return false;
	}

	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolder( pFolder->m_Folders[iIndex], pFileTypeName, nDepth+1 ) )
			return false;
	}

	if ( !nDepth )
	{
		m_XMLWriter.PopNode( true );
	}
	
	return true;
}

bool CProjectGenerator_Win32_2010::WriteConfiguration( CProjectConfiguration *pConfig )
{
	if ( !pConfig->m_bIsFileConfig )
	{
		const char *pTargetPlatformName = g_pVPC->IsPlatformDefined( "win64" ) ? "x64" : "Win32";

		m_XMLWriter.PushNode( "PropertyGroup", CFmtStr( "Condition=\"'$(Configuration)|$(Platform)'=='%s|%s'\" Label=\"Configuration\"", pConfig->m_Name.Get(), pTargetPlatformName ) );

		for ( int i = 0; i < pConfig->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
		{
			int sortedIndex = pConfig->m_PropertyStates.m_PropertiesInOutputOrder[i];
			if ( pConfig->m_PropertyStates.m_Properties[sortedIndex].m_pToolProperty->m_bEmitAsGlobalProperty )
				continue;

			if ( !WriteProperty( &pConfig->m_PropertyStates.m_Properties[sortedIndex] ) )
				return false;
		}

		m_XMLWriter.PopNode( true );
	}
	else
	{
		for ( int i = 0; i < pConfig->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
		{
			int sortedIndex = pConfig->m_PropertyStates.m_PropertiesInOutputOrder[i];
			if ( !WriteProperty( &pConfig->m_PropertyStates.m_Properties[sortedIndex], true, pConfig->m_Name.Get() ) )
				return false;
		}

		if ( !WriteTool( "ClCompile", pConfig->GetCompilerTool(), pConfig ) )
			return false;

		if ( !WriteTool( "CustomBuildStep", pConfig->GetCustomBuildTool(), pConfig ) )
			return false;
	}

	return true;
}

bool CProjectGenerator_Win32_2010::WriteTools( CProjectConfiguration *pConfig )
{
	const char *pTargetPlatformName = g_pVPC->IsPlatformDefined( "win64" ) ? "x64" : "Win32";

	m_XMLWriter.PushNode( "ItemDefinitionGroup", CFmtStr( "Condition=\"'$(Configuration)|$(Platform)'=='%s|%s'\"", pConfig->m_Name.Get(), pTargetPlatformName ) );

	if ( !WriteTool( "PreBuildEvent", pConfig->GetPreBuildEventTool(), pConfig ) )
		return false;

	if ( !WriteTool( "ClCompile", pConfig->GetCompilerTool(), pConfig ) )
		return false;

	if ( !WriteTool( "ResourceCompile", pConfig->GetResourcesTool(), pConfig ) )
		return false;

	if ( !WriteTool( "PreLinkEvent", pConfig->GetPreLinkEventTool(), pConfig ) )
		return false;

	if ( !WriteTool( "Link", pConfig->GetLinkerTool(), pConfig ) )
		return false;

	if ( !WriteTool( "Lib", pConfig->GetLibrarianTool(), pConfig ) )
		return false;

	if ( !WriteTool( "Manifest", pConfig->GetManifestTool(), pConfig ) )
		return false;

	if ( !WriteTool( "Xdcmake", pConfig->GetXMLDocGenTool(), pConfig ) )
		return false;

	if ( !WriteTool( "Bscmake", pConfig->GetBrowseInfoTool(), pConfig ) )
		return false;

	if ( !WriteTool( "PostBuildEvent", pConfig->GetPostBuildEventTool(), pConfig ) )
		return false;

	if ( !WriteTool( "CustomBuildStep", pConfig->GetCustomBuildTool(), pConfig ) )
		return false;

	m_XMLWriter.PopNode( true );

	return true;
}

bool CProjectGenerator_Win32_2010::WritePrimaryXML( const char *pOutputFilename )
{
	if ( !m_XMLWriter.Open( pOutputFilename, true ) )
		return false;

	const char *pTargetPlatformName = g_pVPC->IsPlatformDefined( "win64" ) ? "x64" : "Win32";

	m_XMLWriter.PushNode( "Project", "DefaultTargets=\"Build\" ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\"" );

	m_XMLWriter.PushNode( "ItemGroup", "Label=\"ProjectConfigurations\"" );
	CUtlVector< CUtlString > configurationNames;
	m_pVCProjGenerator->GetAllConfigurationNames( configurationNames ); 
	const char *pPlatformString = "Win32";
	if ( g_pVPC->IsPlatformDefined( "WIN64" ) )
		pPlatformString = "x64";
	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		m_XMLWriter.PushNode( "ProjectConfiguration", CFmtStr( "Include=\"%s|%s\"", configurationNames[i].Get(), pTargetPlatformName ) );
		m_XMLWriter.WriteLineNode( "Configuration", "", configurationNames[i].Get() );
		m_XMLWriter.WriteLineNode( "Platform", "", CFmtStr( "%s", pTargetPlatformName ) );
		m_XMLWriter.PopNode( true );
	}
	m_XMLWriter.PopNode( true );

	m_XMLWriter.PushNode( "PropertyGroup", "Label=\"Globals\"" );
	m_XMLWriter.WriteLineNode( "ProjectName", "", m_pVCProjGenerator->GetProjectName().Get() );
	m_XMLWriter.WriteLineNode( "ProjectGuid", "", m_pVCProjGenerator->GetGUIDString().Get() );
	if ( g_pVPC->BUseP4SCC() )
	{
		m_XMLWriter.WriteLineNode( "SccProjectName", "", "Perforce Project" );
		// it looks like 2k10 (at least) doesn't hook files in the project but not under
		// the project root into source control, so make all the projects local paths
		// the solution dir
		char szCurrentDirectory[MAX_PATH];
		V_GetCurrentDirectory( szCurrentDirectory, V_ARRAYSIZE( szCurrentDirectory ) );
		char szRelativeFilename[MAX_PATH];
		if ( !V_MakeRelativePath( g_pVPC->GetStartDirectory(), szCurrentDirectory, szRelativeFilename, sizeof( szRelativeFilename ) ) )
			V_strncpy( szRelativeFilename, ".", V_ARRAYSIZE( szRelativeFilename ) );
		m_XMLWriter.WriteLineNode( "SccLocalPath", "", szRelativeFilename );
		m_XMLWriter.WriteLineNode( "SccProvider", "", "MSSCCI:Perforce SCM" );
	}
	m_XMLWriter.PopNode( true );

	m_XMLWriter.Write( "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />" );

	// When building 64 bit, use 64 bit toolchain (there is no 64 bit toolchain for 32 bit projects).
	// This property is written early/specially to ensure it is written prior to Microsoft.Cpp.props
	if ( g_pVPC->IsPlatformDefined( "win64" ) && !g_pVPC->BUse32BitTools() )
	{
		m_XMLWriter.PushNode( "PropertyGroup" );
		m_XMLWriter.WriteLineNode( "PreferredToolArchitecture", NULL, "x64" );
		m_XMLWriter.PopNode( true );
	}

	// write the root configurations
	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		CProjectConfiguration *pConfiguration = NULL;
		if ( m_pVCProjGenerator->GetRootConfiguration( configurationNames[i].Get(), &pConfiguration ) )
		{
			if ( !WriteConfiguration( pConfiguration ) )
				return false;
		}
	}

	m_XMLWriter.Write( "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />" );
	m_XMLWriter.PushNode( "ImportGroup", "Label=\"ExtensionSettings\"" );
	m_XMLWriter.PopNode( true );

	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		m_XMLWriter.PushNode( "ImportGroup", CFmtStr( "Condition=\"'$(Configuration)|$(Platform)'=='%s|%s'\" Label=\"PropertySheets\"", configurationNames[i].Get(), pTargetPlatformName ) );
		m_XMLWriter.Write( "<Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />" );
		m_XMLWriter.PopNode( true );
	}
	
	m_XMLWriter.Write( "<PropertyGroup Label=\"UserMacros\" />" );

	m_XMLWriter.PushNode( "PropertyGroup" );
	m_XMLWriter.WriteLineNode( "_ProjectFileVersion", "", "10.0.30319.1" );
	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		CProjectConfiguration *pConfiguration = NULL;
		if ( m_pVCProjGenerator->GetRootConfiguration( configurationNames[i].Get(), &pConfiguration ) )
		{
			for ( int j = 0; j < pConfiguration->m_PropertyStates.m_PropertiesInOutputOrder.Count(); j++ )
			{
				int sortedIndex = pConfiguration->m_PropertyStates.m_PropertiesInOutputOrder[j];
				if ( !pConfiguration->m_PropertyStates.m_Properties[sortedIndex].m_pToolProperty->m_bEmitAsGlobalProperty )
					continue;

				if ( !WriteProperty( &pConfiguration->m_PropertyStates.m_Properties[sortedIndex], true, pConfiguration->m_Name.Get() ) )
					return false;
			}

			if ( !WritePropertyGroupTool( pConfiguration->GetPreBuildEventTool(), pConfiguration ) )
				return false;

			if ( !WritePropertyGroupTool( pConfiguration->GetPreLinkEventTool(), pConfiguration ) )
				return false;

			if ( !WritePropertyGroupTool( pConfiguration->GetLinkerTool(), pConfiguration ) )
				return false;

			if ( !WritePropertyGroupTool( pConfiguration->GetLibrarianTool(), pConfiguration ) )
				return false;

			if ( !WritePropertyGroupTool( pConfiguration->GetPostBuildEventTool(), pConfiguration ) )
				return false;
		}
	}
	m_XMLWriter.PopNode( true );

	// write the tool configurations
	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		CProjectConfiguration *pConfiguration = NULL;
		if ( m_pVCProjGenerator->GetRootConfiguration( configurationNames[i].Get(), &pConfiguration ) )
		{
			if ( !WriteTools( pConfiguration ) )
				return false;
		}
	}

	// write root folders
	for ( int i = 0; i < TKN_MAX_COUNT; i++ )
	{
		if ( !WriteFolder( m_pVCProjGenerator->GetRootFolder(), s_TypeKeyNames[i], 0 ) )
			return false;
	}
	
	m_XMLWriter.Write( "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />" );
	m_XMLWriter.PushNode( "ImportGroup", "Label=\"ExtensionTargets\"" );
	m_XMLWriter.PopNode( true );

	m_XMLWriter.PopNode( true );

	m_XMLWriter.Close();

	return true;
}

bool CProjectGenerator_Win32_2010::WriteFolderToSecondaryXML( CProjectFolder *pFolder, const char *pParentPath )
{
	CUtlString parentPath = CFmtStr( "%s%s%s", pParentPath, pParentPath[0] ? "\\" : "", pFolder->m_Name.Get() );

	MD5Context_t ctx;
	unsigned char digest[MD5_DIGEST_LENGTH];
	V_memset( &ctx, 0, sizeof( ctx ) );
	V_memset( digest, 0, sizeof( digest ) );
	MD5Init( &ctx );
	MD5Update( &ctx, (unsigned char *)parentPath.Get(), strlen( parentPath.Get() ) );
	MD5Final( digest, &ctx );

	char szMD5[64];
	V_binarytohex( digest, MD5_DIGEST_LENGTH, szMD5, sizeof( szMD5 ) );
	V_strupr( szMD5 );

	char szGUID[MAX_PATH];
	V_snprintf( szGUID, sizeof( szGUID ), "{%8.8s-%4.4s-%4.4s-%4.4s-%12.12s}", szMD5, &szMD5[8], &szMD5[12], &szMD5[16], &szMD5[20] );

	m_XMLFilterWriter.PushNode( "Filter", CFmtStr( "Include=\"%s\"", parentPath.Get() ) );
	m_XMLFilterWriter.WriteLineNode( "UniqueIdentifier", "", szGUID );
	m_XMLFilterWriter.PopNode( true );

	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolderToSecondaryXML( pFolder->m_Folders[iIndex], parentPath.Get() ) )
			return false;
	}

	return true;
}

bool CProjectGenerator_Win32_2010::WriteFileToSecondaryXML( CProjectFile *pFile, const char *pParentPath, const char *pFileTypeName )
{
	const char *pKeyName = GetKeyNameForFile( pFile );
	if ( V_stricmp( pFileTypeName, pKeyName ) )
	{
		// skip it
		return true;
	}

	if ( pParentPath )
	{
		m_XMLFilterWriter.PushNode( pKeyName, CFmtStr( "Include=\"%s\"", pFile->m_Name.Get() ) );
		m_XMLFilterWriter.WriteLineNode( "Filter", "", pParentPath );
		m_XMLFilterWriter.PopNode( true );
	}
	else
	{
		m_XMLFilterWriter.Write( CFmtStr( "<%s Include=\"%s\" />", pKeyName, pFile->m_Name.Get() ) );
	}
	
	return true;
}

bool CProjectGenerator_Win32_2010::WriteFolderContentsToSecondaryXML( CProjectFolder *pFolder, const char *pParentPath, const char *pFileTypeName, int nDepth )
{
	CUtlString parentPath;
	if ( pParentPath )
	{
		parentPath = CFmtStr( "%s%s%s", pParentPath, pParentPath[0] ? "\\" : "", pFolder->m_Name.Get() );
	}

	if ( !nDepth )
	{
		m_XMLFilterWriter.PushNode( "ItemGroup", NULL );
	}

	for ( int iIndex = pFolder->m_Files.Head(); iIndex != pFolder->m_Files.InvalidIndex(); iIndex = pFolder->m_Files.Next( iIndex ) )
	{
		if ( !WriteFileToSecondaryXML( pFolder->m_Files[iIndex], parentPath.Get(), pFileTypeName ) )
			return false;
	}

	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolderContentsToSecondaryXML( pFolder->m_Folders[iIndex], parentPath.Get(), pFileTypeName, nDepth+1 ) )
			return false;
	}

	if ( !nDepth )
	{
		m_XMLFilterWriter.PopNode( true );
	}

	return true;
}

bool CProjectGenerator_Win32_2010::WriteSecondaryXML( const char *pOutputFilename )
{
	if ( !m_XMLFilterWriter.Open( pOutputFilename, true ) )
			return false;

	m_XMLFilterWriter.PushNode( "Project", "ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\"" );

	// write the root folders
	m_XMLFilterWriter.PushNode( "ItemGroup", NULL );
	CProjectFolder *pRootFolder = m_pVCProjGenerator->GetRootFolder();
	for ( int iIndex = pRootFolder->m_Folders.Head(); iIndex != pRootFolder->m_Folders.InvalidIndex(); iIndex = pRootFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolderToSecondaryXML( pRootFolder->m_Folders[iIndex], "" ) )
			return false;
	}
	m_XMLFilterWriter.PopNode( true );

	// write folder contents
	for ( int i = 0; i < TKN_MAX_COUNT; i++ )
	{
		if ( !WriteFolderContentsToSecondaryXML( pRootFolder, NULL, s_TypeKeyNames[i], 0 ) )
			return false;
	}

	m_XMLFilterWriter.PopNode( true );

	m_XMLFilterWriter.Close();

	return true;
}

bool CProjectGenerator_Win32_2010::WriteTool( const char *pToolName, const CProjectTool *pProjectTool, CProjectConfiguration *pConfig )
{
	if ( !pProjectTool )
	{
		// not an error, some tools n/a for a config
		return true;
	}

	if ( !pConfig->m_bIsFileConfig )
	{
		m_XMLWriter.PushNode( pToolName, NULL );
	}

	for ( int i = 0; i < pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
	{
		int sortedIndex = pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder[i];
		if ( !pConfig->m_bIsFileConfig )
		{
			if ( pProjectTool->m_PropertyStates.m_Properties[sortedIndex].m_pToolProperty->m_bEmitAsGlobalProperty )
				continue;

			if ( !WriteProperty( &pProjectTool->m_PropertyStates.m_Properties[sortedIndex] ) )
				return false;
		}
		else
		{
			if ( !WriteProperty( &pProjectTool->m_PropertyStates.m_Properties[sortedIndex], true, pConfig->m_Name.Get() ) )
				return false;
		}
	}

	if ( !pConfig->m_bIsFileConfig )
	{
		m_XMLWriter.PopNode( true );
	}
	
	return true;
}	

bool CProjectGenerator_Win32_2010::WriteProperty( const PropertyState_t *pPropertyState, bool bEmitConfiguration, const char *pConfigName, const char *pOutputName, const char *pOutputValue )
{
	if ( !pPropertyState )
	{
		m_XMLWriter.WriteLineNode( pOutputName, "", pOutputValue );
		return true;
	}

	if ( !pOutputName )
	{
		pOutputName = pPropertyState->m_pToolProperty->m_OutputString.Get();
		if ( !pOutputName[0] )
		{
			pOutputName = pPropertyState->m_pToolProperty->m_ParseString.Get();
			if ( pOutputName[0] == '$' )
			{
				pOutputName++;
			}
		}
	}

	const char *pCondition = "";
	CUtlString conditionString;
	if ( bEmitConfiguration )
	{
		const char *pTargetPlatformName = g_pVPC->IsPlatformDefined( "win64" ) ? "x64" : "Win32";

		conditionString = CFmtStr( " Condition=\"'$(Configuration)|$(Platform)'=='%s|%s'\"", pConfigName, pTargetPlatformName );
		pCondition = conditionString.Get();
	}

	if ( pPropertyState )
	{
		switch ( pPropertyState->m_pToolProperty->m_nType )
		{
		case PT_BOOLEAN:
			{
				bool bEnabled = Sys_StringToBool( pPropertyState->m_StringValue.Get() );
				if ( pPropertyState->m_pToolProperty->m_bInvertOutput )
				{
					bEnabled ^= 1;
				}
				m_XMLWriter.WriteLineNode( pOutputName, pCondition, bEnabled ? "true" : "false" );
			}
			break;

		case PT_STRING:
			m_XMLWriter.WriteLineNode( pOutputName, pCondition, m_XMLWriter.FixupXMLString( pPropertyState->m_StringValue.Get() ) );
			break;

		case PT_LIST:
		case PT_INTEGER:
			m_XMLWriter.WriteLineNode( pOutputName, pCondition, pPropertyState->m_StringValue.Get() );
			break;

		case PT_IGNORE:
			break;

		default:
			g_pVPC->VPCError( "CProjectGenerator_Win32_2010: WriteProperty, %s - not implemented", pOutputName );
		}
	}

	return true;
}

bool CProjectGenerator_Win32_2010::Save( const char *pOutputFilename )
{
	bool bValid = WritePrimaryXML( pOutputFilename );
	if ( bValid )
	{
		bValid = WriteSecondaryXML( CFmtStr( "%s.filters", pOutputFilename ) );
		if ( !bValid )
		{
			g_pVPC->VPCError( "Cannot save to the specified project '%s'", pOutputFilename );
		}
	}

	return bValid;
}
