//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

#undef PROPERTYNAME
#define PROPERTYNAME( X, Y ) { X##_##Y, #X, #Y },
static PropertyName_t s_Win32PropertyNames[] =
{
	#include "projectgenerator_win32.inc"
	{ -1, NULL, NULL }
};

IBaseProjectGenerator* GetWin32ProjectGenerator()
{
	static CProjectGenerator_Win32 *s_pProjectGenerator = NULL;
	if ( !s_pProjectGenerator )
	{
		s_pProjectGenerator = new CProjectGenerator_Win32();
	}

	return s_pProjectGenerator->GetProjectGenerator();
}

CProjectGenerator_Win32::CProjectGenerator_Win32()
{
	m_pVCProjGenerator = new CVCProjGenerator();
	m_pVCProjGenerator->SetupGeneratorDefinition( this, "win32_2005.def", s_Win32PropertyNames );
}

bool CProjectGenerator_Win32::WriteFile( CProjectFile *pFile )
{
	m_XMLWriter.PushNode( "File" );
	m_XMLWriter.Write( CFmtStrMax( "RelativePath=\"%s\"", pFile->m_Name.Get() ) );
	m_XMLWriter.Write( ">" );

	for ( int i = 0; i < pFile->m_Configs.Count(); i++ )
	{
		if ( !WriteConfiguration( pFile->m_Configs[i] ) )
			return false;
	}

	m_XMLWriter.PopNode( true );
	
	return true;
}

bool CProjectGenerator_Win32::WriteFolder( CProjectFolder *pFolder )
{
	m_XMLWriter.PushNode( "Filter" );
	m_XMLWriter.Write( CFmtStrMax( "Name=\"%s\"", m_XMLWriter.FixupXMLString( pFolder->m_Name.Get() ) ) );
	m_XMLWriter.Write( ">" );

	for ( int iIndex = pFolder->m_Files.Head(); iIndex != pFolder->m_Files.InvalidIndex(); iIndex = pFolder->m_Files.Next( iIndex ) )
	{
		if ( !WriteFile( pFolder->m_Files[iIndex] ) )
			return false;
	}

	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolder( pFolder->m_Folders[iIndex] ) )
			return false;
	}

	m_XMLWriter.PopNode( true );
	
	return true;
}

bool CProjectGenerator_Win32::WriteConfiguration( CProjectConfiguration *pConfig )
{
	const char *pTargetPlatformName = g_pVPC->IsPlatformDefined( "win64" ) ? "x64" : "Win32";

	if ( pConfig->m_bIsFileConfig )
	{
		m_XMLWriter.PushNode( "FileConfiguration" );
	}
	else
	{
		m_XMLWriter.PushNode( "Configuration" );
	}

	const char *pOutputName = "???";
	if ( !V_stricmp( pConfig->m_Name.Get(), "debug" ) )
	{
		pOutputName = "Debug";
	}
	else if ( !V_stricmp( pConfig->m_Name.Get(), "release" ) )
	{
		pOutputName = "Release";
	}
	else
	{
		return false;
	}

	m_XMLWriter.Write( CFmtStrMax( "Name=\"%s|%s\"", pOutputName, pTargetPlatformName ) );

	// write configuration properties
	for ( int i = 0; i < pConfig->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
	{
		int sortedIndex = pConfig->m_PropertyStates.m_PropertiesInOutputOrder[i];
		WriteProperty( &pConfig->m_PropertyStates.m_Properties[sortedIndex] );
	}
	
	m_XMLWriter.Write( ">" );

	if ( !WriteTool( "VCPreBuildEventTool", pConfig->GetPreBuildEventTool() ) )
		return false;

	if ( !WriteTool( "VCCustomBuildTool", pConfig->GetCustomBuildTool() ) )
		return false;

	if ( !WriteNULLTool( "VCXMLDataGeneratorTool", pConfig ) )
		return false;

	if ( !WriteNULLTool( "VCWebServiceProxyGeneratorTool", pConfig ) )
		return false;

	if ( !WriteNULLTool( "VCMIDLTool", pConfig ) )
		return false;

	if ( !WriteTool( "VCCLCompilerTool", pConfig->GetCompilerTool() ) )
		return false;

	if ( !WriteNULLTool( "VCManagedResourceCompilerTool", pConfig ) )
		return false;

	if ( !WriteTool( "VCResourceCompilerTool", pConfig->GetResourcesTool() ) )
		return false;

	if ( !WriteTool( "VCPreLinkEventTool", pConfig->GetPreLinkEventTool() ) )
		return false;

	if ( !WriteTool( "VCLinkerTool", pConfig->GetLinkerTool() ) )
		return false;

	if ( !WriteTool( "VCLibrarianTool", pConfig->GetLibrarianTool() ) )
		return false;

	if ( !WriteNULLTool( "VCALinkTool", pConfig ) )
		return false;

	if ( !WriteTool( "VCManifestTool",  pConfig->GetManifestTool() ) )
		return false;

	if ( !WriteTool( "VCXDCMakeTool", pConfig->GetXMLDocGenTool() ) )
		return false;

	if ( !WriteTool( "VCBscMakeTool", pConfig->GetBrowseInfoTool() ) )
		return false;

	if ( !WriteNULLTool( "VCFxCopTool", pConfig ) )
		return false;

	if ( !pConfig->GetLibrarianTool() )
	{
		if ( !WriteNULLTool( "VCAppVerifierTool", pConfig ) )
			return false;

		if ( !WriteNULLTool( "VCWebDeploymentTool", pConfig ) )
			return false;
	}

	if ( !WriteTool( "VCPostBuildEventTool", pConfig->GetPostBuildEventTool() ) )
		return false;
	
	m_XMLWriter.PopNode( true );

	return true;
}

bool CProjectGenerator_Win32::WriteToXML()
{
	const char *pTargetPlatformName = g_pVPC->IsPlatformDefined( "win64" ) ? "x64" : "Win32";

	m_XMLWriter.PushNode( "VisualStudioProject" );
	m_XMLWriter.Write( "ProjectType=\"Visual C++\"" );
	
	if ( g_pVPC->BUse2008() )
		m_XMLWriter.Write( "Version=\"9.00\"" );
	else
		m_XMLWriter.Write( "Version=\"8.00\"" );

	m_XMLWriter.Write( CFmtStrMax( "Name=\"%s\"", m_pVCProjGenerator->GetProjectName().Get() ) );
	m_XMLWriter.Write( CFmtStrMax( "ProjectGUID=\"%s\"", m_pVCProjGenerator->GetGUIDString().Get() ) );
	if ( g_pVPC->BUseP4SCC() )
		m_XMLWriter.Write( "SccProjectName=\"Perforce Project\"\nSccLocalPath=\"..\"\nSccProvider=\"MSSCCI:Perforce SCM\"\n" );
	m_XMLWriter.Write( ">" );

	m_XMLWriter.PushNode( "Platforms" );
	m_XMLWriter.PushNode( "Platform" );
	m_XMLWriter.Write( CFmtStrMax( "Name=\"%s\"", pTargetPlatformName) );
	m_XMLWriter.PopNode( false );
	m_XMLWriter.PopNode( true );

	m_XMLWriter.PushNode( "ToolFiles" );
	m_XMLWriter.PopNode( true );

	CUtlVector< CUtlString > configurationNames;
	m_pVCProjGenerator->GetAllConfigurationNames( configurationNames ); 

	// write the root configurations
	m_XMLWriter.PushNode( "Configurations" );
	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		CProjectConfiguration *pConfiguration = NULL;
		if ( m_pVCProjGenerator->GetRootConfiguration( configurationNames[i].Get(), &pConfiguration ) )
		{
			if ( !WriteConfiguration( pConfiguration ) )
				return false;
		}
	}
	m_XMLWriter.PopNode( true );

	m_XMLWriter.PushNode( "References" );
	m_XMLWriter.PopNode( true );

	m_XMLWriter.PushNode( "Files" );

	CProjectFolder *pRootFolder = m_pVCProjGenerator->GetRootFolder();

	for ( int iIndex = pRootFolder->m_Folders.Head(); iIndex != pRootFolder->m_Folders.InvalidIndex(); iIndex = pRootFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolder( pRootFolder->m_Folders[iIndex] ) )
			return false;
	}

	for ( int iIndex = pRootFolder->m_Files.Head(); iIndex != pRootFolder->m_Files.InvalidIndex(); iIndex = pRootFolder->m_Files.Next( iIndex ) )
	{
		if ( !WriteFile( pRootFolder->m_Files[iIndex] ) )
			return false;
	}

	m_XMLWriter.PopNode( true );

	m_XMLWriter.PopNode( true );

	return true;
}

bool CProjectGenerator_Win32::Save( const char *pOutputFilename )
{
	if ( !m_XMLWriter.Open( pOutputFilename ) )
		return false;

	bool bValid = WriteToXML();

	m_XMLWriter.Close();

	return bValid;
}

bool CProjectGenerator_Win32::WriteNULLTool( const char *pToolName, const CProjectConfiguration *pConfig )
{
	if ( pConfig->m_bIsFileConfig )
		return true;

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( CFmtStr( "Name=\"%s\"", pToolName ) );

	m_XMLWriter.PopNode( false );

	return true;
}

bool CProjectGenerator_Win32::WriteTool( const char *pToolName, const CProjectTool *pProjectTool )
{
	if ( !pProjectTool )
	{
		// not an error, some tools n/a for a config
		return true;
	}

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( CFmtStr( "Name=\"%s\"", pToolName ) );

	for ( int i = 0; i < pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
	{
		int sortedIndex = pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder[i];
		WriteProperty( &pProjectTool->m_PropertyStates.m_Properties[sortedIndex] );
	}

	m_XMLWriter.PopNode( false );
	
	return true;
}	

bool CProjectGenerator_Win32::WriteProperty( const PropertyState_t *pPropertyState, const char *pOutputName, const char *pOutputValue )
{
	if ( !pPropertyState )
	{
		m_XMLWriter.Write( CFmtStrMax( "%s=\"%s\"", pOutputName, pOutputValue ) );
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
				m_XMLWriter.Write( CFmtStrMax( "%s=\"%s\"", pOutputName, bEnabled ? "true" : "false" ) );
			}
			break;

		case PT_STRING:
			m_XMLWriter.Write( CFmtStrMax( "%s=\"%s\"", pOutputName, m_XMLWriter.FixupXMLString( pPropertyState->m_StringValue.Get() ) ) );
			break;

		case PT_LIST:
		case PT_INTEGER:
			m_XMLWriter.Write( CFmtStrMax( "%s=\"%s\"", pOutputName, pPropertyState->m_StringValue.Get() ) );
			break;

		case PT_IGNORE:
			break;

		default:
			g_pVPC->VPCError( "CProjectGenerator_Win32: WriteProperty, %s - not implemented", pOutputName );
		}
	}

	return true;
}
