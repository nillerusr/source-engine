//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

#undef PROPERTYNAME
#define PROPERTYNAME( X, Y ) { X##_##Y, #X, #Y },
static PropertyName_t s_Xbox360PropertyNames[] =
{
	#include "projectgenerator_xbox360.inc"
	{ -1, NULL, NULL }
};

IBaseProjectGenerator* GetXbox360ProjectGenerator()
{
	static CProjectGenerator_Xbox360 *s_pProjectGenerator = NULL;
	if ( !s_pProjectGenerator )
	{
		s_pProjectGenerator = new CProjectGenerator_Xbox360();
	}

	return s_pProjectGenerator->GetProjectGenerator();
}

CProjectGenerator_Xbox360::CProjectGenerator_Xbox360()
{
	m_pVCProjGenerator = new CVCProjGenerator();
	m_pVCProjGenerator->SetupGeneratorDefinition( this, "xbox360.def", s_Xbox360PropertyNames );
}

bool CProjectGenerator_Xbox360::WriteFile( CProjectFile *pFile )
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

bool CProjectGenerator_Xbox360::WriteFolder( CProjectFolder *pFolder )
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

bool CProjectGenerator_Xbox360::WriteConfiguration( CProjectConfiguration *pConfig )
{
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
		pOutputName = "Debug|Xbox 360";
	}
	else if ( !V_stricmp( pConfig->m_Name.Get(), "release" ) )
	{
		pOutputName = "Release|Xbox 360";
	}
	else
	{
		return false;
	}

	m_XMLWriter.Write( CFmtStrMax( "Name=\"%s\"", pOutputName ) );

	// write configuration properties
	for ( int i = 0; i < pConfig->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
	{
		int sortedIndex = pConfig->m_PropertyStates.m_PropertiesInOutputOrder[i];
		WriteProperty( &pConfig->m_PropertyStates.m_Properties[sortedIndex] );
	}
	
	if ( !pConfig->m_bIsFileConfig && pConfig->m_PropertyStates.m_Properties.Count() )
	{
		WriteProperty( NULL, "UseOfMFC", "-1" );
		WriteProperty( NULL, "UseOfATL", "0" );
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

	if ( !WriteTool( "VCCLX360CompilerTool", pConfig->GetCompilerTool() ) )
		return false;

	if ( !WriteNULLTool( "VCManagedResourceCompilerTool", pConfig ) )
		return false;

	if ( !WriteNULLTool( "VCResourceCompilerTool", pConfig ) )
		return false;

	if ( !WriteTool( "VCPreLinkEventTool", pConfig->GetPreLinkEventTool() ) )
		return false;

	if ( !WriteTool( "VCX360LinkerTool", pConfig->GetLinkerTool() ) )
		return false;

	if ( !WriteTool( "VCLibrarianTool", pConfig->GetLibrarianTool() ) )
		return false;

	if ( !WriteNULLTool( "VCALinkTool", pConfig ) )
		return false;

	if ( !WriteTool( "VCX360ImageTool", pConfig->GetXboxImageTool() ) )
		return false;

	if ( !WriteTool( "VCBscMakeTool", pConfig->GetBrowseInfoTool() ) )
		return false;

	if ( !WriteTool( "VCX360DeploymentTool", pConfig->GetXboxDeploymentTool() ) )
		return false;

	if ( !WriteTool( "VCPostBuildEventTool", pConfig->GetPostBuildEventTool() ) )
		return false;
	
	if ( !pConfig->m_bIsFileConfig )
	{
		m_XMLWriter.PushNode( "DebuggerTool" );
		m_XMLWriter.PopNode( false );
	}

	m_XMLWriter.PopNode( true );

	return true;
}

bool CProjectGenerator_Xbox360::WriteToXML()
{
	m_XMLWriter.PushNode( "VisualStudioProject" );
	m_XMLWriter.Write( "ProjectType=\"Visual C++\"" );
	
	if ( g_pVPC->BUse2008() )
		m_XMLWriter.Write( "Version=\"9.00\"" );
	else
		m_XMLWriter.Write( "Version=\"8.00\"" );

	m_XMLWriter.Write( CFmtStrMax( "Name=\"%s\"", m_pVCProjGenerator->GetProjectName().Get() ) );
	m_XMLWriter.Write( CFmtStrMax( "ProjectGUID=\"%s\"", m_pVCProjGenerator->GetGUIDString().Get() ) );
	m_XMLWriter.Write( ">" );

	m_XMLWriter.PushNode( "Platforms" );
	m_XMLWriter.PushNode( "Platform" );
	m_XMLWriter.Write( "Name=\"Xbox 360\"" );
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

bool CProjectGenerator_Xbox360::Save( const char *pOutputFilename )
{
	if ( !m_XMLWriter.Open( pOutputFilename ) )
		return false;

	bool bValid = WriteToXML();

	m_XMLWriter.Close();

	return bValid;
}

bool CProjectGenerator_Xbox360::WriteNULLTool( const char *pToolName, const CProjectConfiguration *pConfig )
{
	if ( pConfig->m_bIsFileConfig )
		return true;

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( CFmtStr( "Name=\"%s\"", pToolName ) );

	m_XMLWriter.PopNode( false );

	return true;
}

bool CProjectGenerator_Xbox360::WriteTool( const char *pToolName, const CProjectTool *pProjectTool )
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

bool CProjectGenerator_Xbox360::WriteProperty( const PropertyState_t *pPropertyState, const char *pOutputName, const char *pOutputValue )
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
			g_pVPC->VPCError( "CProjectGenerator_Xbox360: WriteProperty, %s - not implemented", pOutputName );
		}
	}

	return true;
}
