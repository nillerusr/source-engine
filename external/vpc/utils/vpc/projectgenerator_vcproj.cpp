//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

CProjectFile::CProjectFile( CVCProjGenerator *pGenerator, const char *pFilename )
{
	m_pGenerator = pGenerator;
	m_Name = pFilename;
}

CProjectFile::~CProjectFile()
{
	m_Configs.PurgeAndDeleteElements();
}

bool CProjectFile::GetConfiguration( const char *pConfigName, CProjectConfiguration **ppConfig )
{
	if ( !pConfigName || !pConfigName[0] )
	{
		g_pVPC->VPCError( "Empty or bad configuration name." );
	}

	if ( ppConfig )
	{
		// assume not found
		*ppConfig = NULL;
	}

	for ( int i = 0; i < m_Configs.Count(); i++ )
	{
		if ( !V_stricmp( m_Configs[i]->m_Name.Get(), pConfigName ) )
		{
			// found
			if ( ppConfig )
			{
				*ppConfig = m_Configs[i];
			}
			return true;
		}
	}

	// not found
	return false;
}

bool CProjectFile::AddConfiguration( const char *pConfigName, CProjectConfiguration **ppConfig )
{
	if ( ppConfig )
	{
		// assume not found
		*ppConfig = NULL;
	}

	if ( GetConfiguration( pConfigName, NULL ) )
	{
		// found, cannot add duplicate
		return false;
	}

	// add in alphabetic order
	CProjectConfiguration *pNewConfig = new CProjectConfiguration( m_pGenerator, pConfigName, m_Name.Get() );

	int iIndex = 0;
	for ( iIndex = 0; iIndex < m_Configs.Count(); iIndex++ )
	{
		if ( V_stricmp( pConfigName, m_Configs[iIndex]->m_Name.Get() ) < 0 )
		{
			m_Configs.InsertBefore( iIndex, pNewConfig );
			break;
		}
	}
	if ( iIndex == m_Configs.Count() )
	{
		m_Configs.AddToTail( pNewConfig );
	}

	if ( ppConfig )
	{
		*ppConfig = pNewConfig;
	}
	return true;
}

bool CProjectFile::RemoveConfiguration( CProjectConfiguration *pConfiguration )
{
	for ( int i = 0; i < m_Configs.Count(); i++ )
	{
		if ( m_Configs[i] == pConfiguration )
		{
			m_Configs.Remove( i );
			delete pConfiguration;
			return true;
		}
	}

	return false;
}

CProjectFolder::CProjectFolder( CVCProjGenerator *pGenerator, const char *pFolderName )
{
	m_pGenerator = pGenerator;
	m_Name = pFolderName;
}

CProjectFolder::~CProjectFolder()
{
	m_Folders.PurgeAndDeleteElements();
	m_Files.PurgeAndDeleteElements();
}

bool CProjectFolder::GetFolder( const char *pFolderName, CProjectFolder **pFolder )
{
	if ( pFolder )
	{
		// assume not found
		*pFolder = NULL;
	}

	if ( !pFolderName || !pFolderName[0] )
	{
		g_pVPC->VPCError( "Empty or bad folder name." );
	}

	for ( int iIndex = m_Folders.Head(); iIndex != m_Folders.InvalidIndex(); iIndex = m_Folders.Next( iIndex ) )
	{
		if ( !V_stricmp( m_Folders[iIndex]->m_Name.Get(), pFolderName ) )
		{
			// found
			if ( pFolder )
			{
				*pFolder = m_Folders[iIndex];
			}
			return true;
		}
	}

	// not found
	return false;
}

bool CProjectFolder::AddFolder( const char *pFolderName, CProjectFolder **pFolder )
{
	if ( pFolder )
	{
		// assume not added
		*pFolder = NULL;
	}

	if ( GetFolder( pFolderName, NULL ) )
	{
		// found, cannot add duplicate
		return false;
	}

	CProjectFolder *pNewFolder = new CProjectFolder( m_pGenerator, pFolderName );

	// maintain sorted ascending alphabetic order
	int iIndex;
	for ( iIndex = m_Folders.Head(); iIndex != m_Folders.InvalidIndex(); iIndex = m_Folders.Next( iIndex ) )
	{
		if ( V_stricmp( pFolderName, m_Folders[iIndex]->m_Name.Get() ) < 0 )
		{
			m_Folders.InsertBefore( iIndex, pNewFolder );
			break;
		}
	}
	if ( iIndex == m_Folders.InvalidIndex() )
	{
		m_Folders.AddToTail( pNewFolder );
	}

	if ( pFolder )
	{
		*pFolder = pNewFolder;
	}
	return true;
}

void CProjectFolder::AddFile( const char *pFilename, CProjectFile **ppFile )
{
	if ( !pFilename || !pFilename[0] )
	{
		g_pVPC->VPCError( "Empty or bad filename." );
	}

	CProjectFile *pNewFile = new CProjectFile( m_pGenerator, pFilename );

	// maintain sorted ascending alphabetic order
	int iIndex;
	for ( iIndex = m_Files.Head(); iIndex != m_Files.InvalidIndex(); iIndex = m_Files.Next( iIndex ) )
	{
		if ( g_pVPC->IsPlatformDefined( "PS3" ) )
		{
			// temporary legacy behavior for diff ease until I can be sure project generation is equivalent
			iIndex = m_Files.InvalidIndex();
			break;
		}

		// the COM layer for WIN32 sorted by filename only, and NOT the entire path
		if ( V_stricmp( V_GetFileName( pFilename ), V_GetFileName( m_Files[iIndex]->m_Name.Get() ) ) < 0 )
		{
			m_Files.InsertBefore( iIndex, pNewFile );
			break;
		}
	}
	if ( iIndex == m_Files.InvalidIndex() )
	{
		m_Files.AddToTail( pNewFile );
	}

	if ( ppFile )
	{
		*ppFile = pNewFile;
	}
}

bool CProjectFolder::FindFile( const char *pFilename )
{
	if ( !pFilename || !pFilename[0] )
	{
		g_pVPC->VPCError( "Empty or bad filename." );
	}

	for ( int iIndex = m_Files.Head(); iIndex != m_Files.InvalidIndex(); iIndex = m_Files.Next( iIndex ) )
	{
		if ( !V_stricmp( m_Files[iIndex]->m_Name.Get(), pFilename ) )
		{
			// found
			return true;
		}
	}

	return false;
}

bool CProjectFolder::RemoveFile( const char *pFilename )
{
	if ( !pFilename || !pFilename[0] )
	{
		g_pVPC->VPCError( "Empty or bad filename." );
	}

	for ( int iIndex = m_Files.Head(); iIndex != m_Files.InvalidIndex(); iIndex = m_Files.Next( iIndex ) )
	{
		if ( !V_stricmp( m_Files[iIndex]->m_Name.Get(), pFilename ) )
		{
			// found, remove
			delete m_Files[iIndex];
			m_Files.Unlink( iIndex );
			return true;
		}
	}

	return false;
}

bool CPropertyStateLessFunc::Less( const int& lhs, const int& rhs, void *pContext )
{
	int lhsPropertyId = (( CPropertyStates* )pContext)->m_Properties[lhs].m_pToolProperty->m_nPropertyId;
	int rhsPropertyId = (( CPropertyStates* )pContext)->m_Properties[rhs].m_pToolProperty->m_nPropertyId;

	return lhsPropertyId < rhsPropertyId;
}

CPropertyStates::CPropertyStates()
{
	m_PropertiesInOutputOrder.SetLessContext( this );
}

PropertyState_t *CPropertyStates::GetProperty( int nPropertyId )
{
	for ( int i = 0 ; i < m_Properties.Count(); i++ )
	{
		if ( m_Properties[i].m_pToolProperty->m_nPropertyId == nPropertyId )
		{
			return &m_Properties[i];
		}
	}

	return NULL;
}

PropertyState_t *CPropertyStates::GetProperty( const char *pPropertyName )
{
	if ( pPropertyName[0] == '$' )
	{
		pPropertyName++;
	}

	for ( int i = 0; i < m_Properties.Count(); i++ )
	{
		const char *pParseString = m_Properties[i].m_pToolProperty->m_ParseString.Get();
		if ( pParseString[0] == '$' )
		{
			pParseString++;
		}
		if ( !V_stricmp( pPropertyName, pParseString ) )
		{
			return &m_Properties[i];
		}
	
		const char *pLegacyString = m_Properties[i].m_pToolProperty->m_LegacyString.Get();
		if ( pLegacyString[0] )
		{
			if ( pLegacyString[0] == '$' )
			{
				pLegacyString++;
			}
			if ( !V_stricmp( pPropertyName, pLegacyString ) )
			{
				return &m_Properties[i];
			}
		}
	}

	return NULL;
}

bool CPropertyStates::SetStringProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	// find possible current value
	const char *pCurrentValue = NULL;
	for ( int i = 0; i < m_Properties.Count(); i++ )
	{
		if ( m_Properties[i].m_pToolProperty == pToolProperty )
		{
			pCurrentValue = m_Properties[i].m_StringValue.Get();
			break;
		}
	}

	if ( !pCurrentValue && pRootTool )
	{
		// fallback to root tool's config to find current value
		for ( int i = 0; i < pRootTool->m_PropertyStates.m_Properties.Count(); i++ )
		{
			if ( pRootTool->m_PropertyStates.m_Properties[i].m_pToolProperty == pToolProperty )
			{
				pCurrentValue = pRootTool->m_PropertyStates.m_Properties[i].m_StringValue.Get();
				break;
			}
		}
	}

	// feed in current value to resolve $BASE
	// possibly culled or tokenized new value
	char buff[MAX_SYSTOKENCHARS];
	if ( !g_pVPC->GetScript().ParsePropertyValue( pCurrentValue, buff, sizeof( buff ) ) )
		return true;

	if ( pToolProperty->m_bFixSlashes )
	{
		V_FixSlashes( buff );
	}

	if ( pToolProperty->m_bPreferSemicolonNoComma )
	{
		CUtlString buffCopy = buff;
		V_StrSubst( buffCopy.Get(), ",", ";", buff, sizeof( buff ), false );
	}

	if ( pToolProperty->m_bPreferSemicolonNoSpace )
	{
		CUtlString buffCopy = buff;
		V_StrSubst( buffCopy.Get(), " ", ";", buff, sizeof( buff ), false );
	}

	if ( pToolProperty->m_bAppendSlash )
	{
		int len = strlen( buff );
		if ( len >= 1 && buff[len-1] != '\\' )
		{
			V_strncat( buff, "\\", sizeof( buff ) );
		}
	}

	if ( !V_stricmp( pToolProperty->m_ParseString.Get(), "$CommandLine" ) && !V_strnicmp( buff, "echo ", 5 ) )
	{
		// the COM layer auto appended a CR-LF for a command line with an echo
		int len = strlen( buff );
		if ( ( len >= 1 && buff[len-1] != '\n' ) &&
			( len >= 12 && V_stricmp( buff + len - 12, "&#x0D;&#x0A;" ) ) )
		{
			V_strncat( buff, "\n", sizeof( buff ) );
		}
	}

	if ( pCurrentValue && !V_stricmp( pCurrentValue, buff ) )
	{
		g_pVPC->VPCWarning( "%s matches default setting, [%s line:%d]", pToolProperty->m_ParseString.Get(), g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetLine() );
	}

	if ( pCurrentValue )
	{
		// update existing state
		// always replace or add strings due to case changes
		for ( int i = 0; i < m_Properties.Count(); i++ )
		{
			if ( m_Properties[i].m_pToolProperty == pToolProperty )
			{
				m_Properties[i].m_StringValue = buff;
				return true;
			}
		}
	}

	// add
	int iIndex = m_Properties.AddToTail();
	m_Properties[iIndex].m_pToolProperty = pToolProperty;
 	m_Properties[iIndex].m_StringValue = buff;

	m_PropertiesInOutputOrder.Insert( iIndex );

	return true;
}

bool CPropertyStates::SetListProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	char buff[MAX_SYSTOKENCHARS];
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, buff, sizeof( buff ) ) )
		return true;

	// resolve the parsed token to an expected ordinal
	const char *pNewOrdinalValue = NULL;
	for ( int i = 0; i < pToolProperty->m_Ordinals.Count() ;i++ )
	{
		if ( !V_stricmp( pToolProperty->m_Ordinals[i].m_ParseString.Get(), buff ) )
		{
			pNewOrdinalValue = pToolProperty->m_Ordinals[i].m_ValueString.Get();
			break;
		}
	}

	if ( !pNewOrdinalValue && !V_stricmp( buff, "default" ) )
	{
		// allow "default" if not explicitly provided
		// same as empty, state stays unaffected
		return true;
	}

	if ( !pNewOrdinalValue )
	{
		g_pVPC->VPCSyntaxError( "Unknown Ordinal for %s", pToolProperty->m_ParseString.Get() );
	}

	// find possible current value
	const char *pCurrentOrdinalValue = NULL;
	for ( int i = 0; i < m_Properties.Count(); i++ )
	{
		if ( m_Properties[i].m_pToolProperty == pToolProperty )
		{
			pCurrentOrdinalValue = m_Properties[i].m_StringValue.Get();
			break;
		}
	}

	if ( !pCurrentOrdinalValue && pRootTool )
	{
		// fallback to root tool's config to find current value
		for ( int i = 0; i < pRootTool->m_PropertyStates.m_Properties.Count(); i++ )
		{
			if ( pRootTool->m_PropertyStates.m_Properties[i].m_pToolProperty == pToolProperty )
			{
				pCurrentOrdinalValue = pRootTool->m_PropertyStates.m_Properties[i].m_StringValue.Get();
				break;
			}
		}
	}

	if ( pCurrentOrdinalValue && !V_stricmp( pCurrentOrdinalValue, pNewOrdinalValue ) )
	{
		g_pVPC->VPCWarning( "%s matches default setting, [%s line:%d]", pToolProperty->m_ParseString.Get(), g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetLine() );
	}

	if ( pCurrentOrdinalValue )
	{
		// update existing state
		for ( int i = 0; i < m_Properties.Count(); i++ )
		{
			if ( m_Properties[i].m_pToolProperty == pToolProperty )
			{
				m_Properties[i].m_OrdinalString = buff;
				m_Properties[i].m_StringValue = pNewOrdinalValue;
				return true;
			}
		}
	}

	// add
	int iIndex = m_Properties.AddToTail();
	m_Properties[iIndex].m_pToolProperty = pToolProperty;
 	m_Properties[iIndex].m_OrdinalString = buff;
 	m_Properties[iIndex].m_StringValue = pNewOrdinalValue;

	m_PropertiesInOutputOrder.Insert( iIndex );

	return true;
}

bool CPropertyStates::SetBoolProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool, bool bEnabled )
{
	const char *pNewOrdinalValue = bEnabled ? "1" :"0";

	// find possible current value
	const char *pCurrentOrdinalValue = NULL;
	for ( int i = 0; i < m_Properties.Count(); i++ )
	{
		if ( m_Properties[i].m_pToolProperty == pToolProperty )
		{
			pCurrentOrdinalValue = m_Properties[i].m_StringValue.Get();
			break;
		}
	}

	if ( !pCurrentOrdinalValue && pRootTool )
	{
		// fallback to root tool's config to find current value
		for ( int i = 0; i < pRootTool->m_PropertyStates.m_Properties.Count(); i++ )
		{
			if ( pRootTool->m_PropertyStates.m_Properties[i].m_pToolProperty == pToolProperty )
			{
				pCurrentOrdinalValue = pRootTool->m_PropertyStates.m_Properties[i].m_StringValue.Get();
				break;
			}
		}
	}

	if ( pCurrentOrdinalValue && !V_stricmp( pCurrentOrdinalValue, pNewOrdinalValue ) )
	{
		g_pVPC->VPCWarning( "%s matches default setting, [%s line:%d]", pToolProperty->m_ParseString.Get(), g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetLine() );
	}

	if ( pCurrentOrdinalValue )
	{
		// update existing state
		for ( int i = 0; i < m_Properties.Count(); i++ )
		{
			if ( m_Properties[i].m_pToolProperty == pToolProperty )
			{
				m_Properties[i].m_StringValue = pNewOrdinalValue;
				return true;
			}
		}
	}

	// add
	int iIndex = m_Properties.AddToTail();
	m_Properties[iIndex].m_pToolProperty = pToolProperty;
 	m_Properties[iIndex].m_StringValue = pNewOrdinalValue;

	m_PropertiesInOutputOrder.Insert( iIndex );

	return true;
}

bool CPropertyStates::SetBoolProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	char buff[MAX_SYSTOKENCHARS];
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, buff, sizeof( buff ) ) )
		return true;

	return SetBoolProperty( pToolProperty, pRootTool, Sys_StringToBool( buff ) );
}

bool CPropertyStates::SetBoolProperty( ToolProperty_t *pToolProperty, bool bEnabled )
{
	return SetBoolProperty( pToolProperty, NULL, bEnabled );
}

bool CPropertyStates::SetIntegerProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	char buff[MAX_SYSTOKENCHARS];
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, buff, sizeof( buff ) ) )
		return true;

	// ensure the parsed token is a real integer and not just quietly mapped to 0
	int nParsedValue = atoi( buff );
	if ( V_stricmp( CFmtStr( "%d", nParsedValue ), buff ) )
	{
		g_pVPC->VPCSyntaxError( "Unrecognized integer value: %s", buff );
	}

	// find possible current value
	const char *pCurrentOrdinalValue = NULL;
	for ( int i = 0; i < m_Properties.Count(); i++ )
	{
		if ( m_Properties[i].m_pToolProperty == pToolProperty )
		{
			pCurrentOrdinalValue = m_Properties[i].m_StringValue.Get();
			break;
		}
	}

	if ( !pCurrentOrdinalValue && pRootTool )
	{
		// fallback to root tool's config to find current value
		for ( int i = 0; i < pRootTool->m_PropertyStates.m_Properties.Count(); i++ )
		{
			if ( pRootTool->m_PropertyStates.m_Properties[i].m_pToolProperty == pToolProperty )
			{
				pCurrentOrdinalValue = pRootTool->m_PropertyStates.m_Properties[i].m_StringValue.Get();
				break;
			}
		}
	}

	if ( pCurrentOrdinalValue && ( atoi( pCurrentOrdinalValue ) == atoi( buff ) ) )
	{
		g_pVPC->VPCWarning( "%s matches default setting, [%s line:%d]", pToolProperty->m_ParseString.Get(), g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetLine() );
	}

	if ( pCurrentOrdinalValue )
	{
		// update existing state
		for ( int i = 0; i < m_Properties.Count(); i++ )
		{
			if ( m_Properties[i].m_pToolProperty == pToolProperty )
			{
				m_Properties[i].m_StringValue = buff;
				return true;
			}
		}
	}

	// add
	int iIndex = m_Properties.AddToTail();
	m_Properties[iIndex].m_pToolProperty = pToolProperty;
 	m_Properties[iIndex].m_StringValue = buff;

	m_PropertiesInOutputOrder.Insert( iIndex );

	return true;
}

bool CPropertyStates::SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	bool bHandled = false;
	switch ( pToolProperty->m_nType )
	{
	case PT_BOOLEAN:
		bHandled = SetBoolProperty( pToolProperty, pRootTool );
		break;

	case PT_STRING:
		bHandled = SetStringProperty( pToolProperty, pRootTool );
		break;

	case PT_INTEGER:
		bHandled = SetIntegerProperty( pToolProperty, pRootTool );
		break;

	case PT_LIST:
		bHandled = SetListProperty( pToolProperty, pRootTool );
		break;

	case PT_IGNORE:
		bHandled = true;
		g_pVPC->GetScript().SkipRestOfLine();
		break;

	case PT_DEPRECATED:
		g_pVPC->VPCError( "SetProperty: Property %s has been deprecated and is no longer supported!", pToolProperty->m_ParseString.Get() );
		break;

	default:
		g_pVPC->VPCError( "SetProperty: Unknown type for %s - requires implementation", pToolProperty->m_ParseString.Get() );
	}

	return bHandled;
}

static bool FilesSortLessFunc( CProjectFile* const &pLHS, CProjectFile* const &pRHS )
{
	return CaselessStringLessThan( pLHS->m_Name.Get(), pRHS->m_Name.Get() );
}

CProjectConfiguration::CProjectConfiguration( CVCProjGenerator *pGenerator, const char *pConfigName, const char *pFilename )
{
	m_pGenerator = pGenerator;
	m_Name = pConfigName;
	m_bIsFileConfig = ( pFilename != NULL );

	m_pDebuggingTool = NULL;
	m_pCompilerTool = NULL;
	m_pLibrarianTool = NULL;
	m_pLinkerTool = NULL; 
	m_pManifestTool = NULL; 
	m_pXMLDocGenTool = NULL; 
	m_pBrowseInfoTool = NULL; 
	m_pResourcesTool = NULL; 
	m_pPreBuildEventTool = NULL;
	m_pPreLinkEventTool = NULL;
	m_pPostBuildEventTool = NULL;
	m_pCustomBuildTool = NULL;
	m_pXboxImageTool = NULL;
	m_pXboxDeploymentTool = NULL;

	if ( !m_bIsFileConfig )
	{
		m_pDebuggingTool = new CDebuggingTool( pGenerator );
		m_pCompilerTool = new CCompilerTool( pGenerator, pConfigName, false );
		m_pLibrarianTool = new CLibrarianTool( pGenerator );
		m_pLinkerTool = new CLinkerTool( pGenerator );
		m_pManifestTool = new CManifestTool( pGenerator );
		m_pXMLDocGenTool = new CXMLDocGenTool( pGenerator );
		m_pBrowseInfoTool = new CBrowseInfoTool( pGenerator );
		m_pResourcesTool = new CResourcesTool( pGenerator );
		m_pPreBuildEventTool = new CPreBuildEventTool( pGenerator );
		m_pPreLinkEventTool = new CPreLinkEventTool( pGenerator );
		m_pPostBuildEventTool = new CPostBuildEventTool( pGenerator );
		m_pCustomBuildTool = new CCustomBuildTool( pGenerator, pConfigName, false );
		m_pXboxImageTool = new CXboxImageTool( pGenerator );
		m_pXboxDeploymentTool = new CXboxDeploymentTool( pGenerator );
	}
	else
	{
		// a file's config can only be the compiler or the custom build tool
		const char *pExtension = V_GetFileExtension( pFilename );
		bool bIsCPP = IsCFileExtension( pExtension );
		bool bIsLib = pExtension && !V_stricmp( pExtension, "lib" );
		if ( bIsCPP )
		{
			m_pCompilerTool = new CCompilerTool( pGenerator, pConfigName, true );
		}
		else if ( bIsLib )
		{
			m_pLibrarianTool = new CLibrarianTool( pGenerator );
		}
		else
		{
			m_pCustomBuildTool = new CCustomBuildTool( pGenerator, pConfigName, true );
		}
	}
}

CProjectConfiguration::~CProjectConfiguration()
{
	delete m_pDebuggingTool;
	delete m_pCompilerTool;
	delete m_pLibrarianTool;
	delete m_pLinkerTool; 
	delete m_pManifestTool; 
	delete m_pXMLDocGenTool; 
	delete m_pBrowseInfoTool; 
	delete m_pResourcesTool; 
	delete m_pPreBuildEventTool;
	delete m_pPreLinkEventTool;
	delete m_pPostBuildEventTool;
	delete m_pCustomBuildTool;
	delete m_pXboxImageTool;
	delete m_pXboxDeploymentTool;
}

bool CProjectConfiguration::IsEmpty()
{
	if ( m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pDebuggingTool && m_pDebuggingTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pCompilerTool && m_pCompilerTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pLibrarianTool && m_pLibrarianTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pLinkerTool && m_pLinkerTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pManifestTool && m_pManifestTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pXMLDocGenTool && m_pXMLDocGenTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pBrowseInfoTool && m_pBrowseInfoTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pResourcesTool && m_pResourcesTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pPreBuildEventTool && m_pPreBuildEventTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pPreLinkEventTool && m_pPreLinkEventTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pPostBuildEventTool && m_pPostBuildEventTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pCustomBuildTool && m_pCustomBuildTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pXboxImageTool && m_pXboxImageTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pXboxDeploymentTool && m_pXboxDeploymentTool->m_PropertyStates.m_Properties.Count() )
		return false;

	return true;
}

bool CProjectConfiguration::SetProperty( ToolProperty_t *pToolProperty )
{
	bool bHandled = m_PropertyStates.SetProperty( pToolProperty );

	// have to mimic what the COM layer used to do which is to configure itself based on the type of application its building
	// VPC enforces a strict order, configuration blocks must come before any tool block to allow this to be rational
	if ( bHandled && !V_stricmp( pToolProperty->m_ParseString, "$ConfigurationType" ) )
	{
		PropertyState_t *pPropertyState = m_PropertyStates.GetProperty( pToolProperty->m_nPropertyId );
		if ( pPropertyState && 
			( ( V_stristr( pPropertyState->m_OrdinalString.Get(), "static library" ) || !V_stricmp( pPropertyState->m_OrdinalString.Get(), "LIB" ) ) ) )
		{
			// static library does not get these tools
			delete m_pResourcesTool;
			m_pResourcesTool = NULL;

			delete m_pManifestTool;
			m_pManifestTool = NULL;

			delete m_pLinkerTool;
			m_pLinkerTool = NULL;

			delete m_pXboxImageTool;
			m_pXboxImageTool = NULL;

			delete m_pXboxDeploymentTool;
			m_pXboxDeploymentTool = NULL;
		}
		else
		{
			// exe/dlls do not get the librarian
			delete m_pLibrarianTool;
			m_pLibrarianTool = NULL;
		}
	}

	return bHandled;
}

bool CProjectTool::SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	return m_PropertyStates.SetProperty( pToolProperty, pRootTool );
}

//-----------------------------------------------------------------------------

bool CCompilerTool::SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	if ( m_bIsFileConfig )
	{
		CProjectConfiguration *pConfig;
		if ( !GetGenerator()->GetRootConfiguration( m_ConfigName.Get(), &pConfig ) )
			return false;

		return CProjectTool::SetProperty( pToolProperty, pConfig->GetCompilerTool() );
	}
	return CProjectTool::SetProperty( pToolProperty );
}


//-----------------------------------------------------------------------------

bool CCustomBuildTool::SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	if ( m_bIsFileConfig )
	{
		CProjectConfiguration *pConfig;
		if ( !GetGenerator()->GetRootConfiguration( m_ConfigName.Get(), &pConfig ) )
			return false;

		return CProjectTool::SetProperty( pToolProperty, pConfig->GetCustomBuildTool() );
	}
	return CProjectTool::SetProperty( pToolProperty );
}

// These are the only properties we care about for makefiles.
static const char *g_pRelevantSchemaProperties[] =
{
	g_pOption_AdditionalIncludeDirectories,
	g_pOption_PreprocessorDefinitions,
	g_pOption_AdditionalProjectDependencies,
};


CRelevantPropertyNames g_RelevantSchemaPropertyNames =
{
	g_pRelevantSchemaProperties,
	V_ARRAYSIZE( g_pRelevantSchemaProperties )
};


CVCProjGenerator::CVCProjGenerator() : 
	BaseClass( &g_RelevantSchemaPropertyNames )
{
	m_pGeneratorDefinition = NULL;
	m_pRootFolder = NULL;
	m_FileDictionary.SetLessFunc( FilesSortLessFunc );

	Clear();
}

void CVCProjGenerator::Clear()
{
	m_nActivePropertySection = KEYWORD_UNKNOWN;

	m_pProjectFile = NULL;
	m_pConfig = NULL;
	m_pFileConfig = NULL;

	m_pDebuggingTool = NULL;
	m_pCompilerTool = NULL;
	m_pLibrarianTool = NULL;
	m_pLinkerTool = NULL; 
	m_pManifestTool = NULL; 
	m_pXMLDocGenTool = NULL; 
	m_pBrowseInfoTool = NULL; 
	m_pResourcesTool = NULL; 
	m_pPreBuildEventTool = NULL;
	m_pPreLinkEventTool = NULL;
	m_pPostBuildEventTool = NULL;
	m_pCustomBuildTool = NULL;
	m_pXboxImageTool = NULL;
	m_pXboxDeploymentTool = NULL;

	m_spFolderStack.Purge();
	m_spCompilerStack.Purge();
	m_spCustomBuildToolStack.Purge();

	// undefined until set
	m_VSIType = PS3_VSI_TYPE_UNDEFINED;

	m_FileDictionary.Purge();

	// setup expected root folder
	delete m_pRootFolder;
	m_pRootFolder = new CProjectFolder( this, "???" );

	// setup the root configurations
	m_RootConfigurations.PurgeAndDeleteElements();

	CProjectConfiguration *pDebugConfig = new CProjectConfiguration( this, "Debug", NULL );
	m_RootConfigurations.AddToTail( pDebugConfig );

	CProjectConfiguration *pReleaseConfig = new CProjectConfiguration( this, "Release", NULL );
	m_RootConfigurations.AddToTail( pReleaseConfig );
}

void CVCProjGenerator::SetupGeneratorDefinition( IVCProjWriter *pVCProjWriter, const char *pDefinitionName, PropertyName_t *pPropertyNames )
{
	m_pVCProjWriter = pVCProjWriter;

	delete m_pGeneratorDefinition;
	m_pGeneratorDefinition = new CGeneratorDefinition();
	m_pGeneratorDefinition->LoadDefinition( pDefinitionName, pPropertyNames );
}

const char* CVCProjGenerator::GetProjectFileExtension()
{
	if ( g_pVPC->Is2010() )
	{
		return "vcxproj";
	}
	return "vcproj";
}

void CVCProjGenerator::StartProject()
{
	if ( !m_pGeneratorDefinition )
	{
		g_pVPC->VPCError( "Missing a properly configured generator definition" );
	}

	BaseClass::StartProject();

	// create the default project
	// must have a root project for most operations
	m_ProjectName = "UNNAMED";
	m_OutputFilename = g_pVPC->GetOutputFilename();

	SetGUID( m_OutputFilename.Get() );
}

void CVCProjGenerator::EndProject()
{
	BaseClass::EndProject();

	// push generator definition scripts into CRC check
	CRC32_t scriptCRC = 0;
	const char *pScriptName = m_pGeneratorDefinition->GetScriptName( &scriptCRC );
	char scriptPath[MAX_PATH];
	g_pVPC->ResolveMacrosInString( CFmtStr( "$SRCDIR\\%s", pScriptName ), scriptPath, sizeof( scriptPath ) );
	g_pVPC->AddScriptToCRCCheck( scriptPath, scriptCRC );

	// done once, right before save
	ApplyInternalPreprocessorDefinitions();

	VPC_FakeKeyword_SchemaFolder( this );

#ifdef STEAM
#error( "NEEDS TO BE FIXED" )
	// add the perforce integration magic
	bstr = "Perforce Project";
	g_spProject->put_SccProjectName( bstr );
	bstr = "..";
	g_spProject->put_SccLocalPath( bstr );
	bstr = "MSSCCI:Perforce SCM";
	g_spProject->put_SccProvider( bstr );
#endif

	g_pVPC->VPCStatus( true, "Saving... Project: '%s' File: '%s'", GetProjectName().String(), g_pVPC->GetOutputFilename() );

	if ( m_ProjectName.IsEmpty() )
	{
		g_pVPC->VPCError( "Invalid Empty Project Name" );
	}

	if ( m_OutputFilename.IsEmpty() )
	{
		g_pVPC->VPCError( "Invalid Empty Output Filename" );
	}
	
	if ( m_GUIDString.IsEmpty() )
	{
		g_pVPC->VPCError( "Invalid Empty GUID String" );
	}

	// Save the .vcproj file.
	bool bValid = m_pVCProjWriter->Save( m_OutputFilename.Get() );
	if ( !bValid )
	{
		g_pVPC->VPCError( "Cannot save the specified project '%s' to '%s'", GetProjectName().Get(), m_OutputFilename.Get() );
	}

	// Expected to not be inside a property section.
	Assert( m_nActivePropertySection == KEYWORD_UNKNOWN ); 

	Clear();
}

void CVCProjGenerator::SetGUID( const char *pOutputFilename )
{
	char szBasename[MAX_PATH];
	V_FileBase( pOutputFilename, szBasename, sizeof( szBasename ) );

	// set the GUID
	MD5Context_t ctx;
	unsigned char digest[MD5_DIGEST_LENGTH];
	V_memset( &ctx, 0, sizeof( ctx ) );
	V_memset( digest, 0, sizeof( digest ) );
	MD5Init( &ctx );
	MD5Update( &ctx, (unsigned char *)szBasename, strlen( szBasename ) );
	MD5Final( digest, &ctx );

	char szMD5[64];
	V_binarytohex( digest, MD5_DIGEST_LENGTH, szMD5, sizeof( szMD5 ) );
	V_strupr( szMD5 );

	char szGUID[MAX_PATH];
	V_snprintf( szGUID, sizeof( szGUID ), "{%8.8s-%4.4s-%4.4s-%4.4s-%12.12s}", szMD5, &szMD5[8], &szMD5[12], &szMD5[16], &szMD5[20] );
	m_GUIDString = szGUID;
}

CUtlString CVCProjGenerator::GetProjectName()
{
	return m_ProjectName;
}

void CVCProjGenerator::SetProjectName( const char *pProjectName )
{
	m_ProjectName = pProjectName;
}

void CVCProjGenerator::StartFolder( const char *pFolderName )
{
	BaseClass::StartFolder( pFolderName );

	bool bValid;
	CProjectFolder *pFolder = NULL;

	if ( m_spFolderStack.Count() == 0 )
	{
		// add to root
		bValid = AddFolder( pFolderName, NULL, &pFolder );
	}
	else
	{
		// add as subfolder
		bValid = AddFolder( pFolderName, m_spFolderStack.Top(), &pFolder );
	}

	if ( !bValid )
	{
		// resolve failure
		// folder already exists, not an error
		// find the matching object
		pFolder = NULL;
		if ( m_spFolderStack.Count() == 0 )
		{
			// at root
			GetFolder( pFolderName, NULL, &pFolder );
		}
		else
		{
			// at subfolder
			GetFolder( pFolderName, m_spFolderStack.Top(), &pFolder );
		}
		if ( !pFolder )
		{
			g_pVPC->VPCError( "Cannot find expected folder %s", pFolderName );
		}
	}

	m_spFolderStack.Push( pFolder );
}

void CVCProjGenerator::EndFolder()
{
	BaseClass::EndFolder();

	if ( m_spFolderStack.Count() == 0 )
	{
		g_pVPC->VPCError( "EndFolder called and no folder has been started." );
	}

	m_spFolderStack.Pop();
}

bool CVCProjGenerator::StartFile( const char *pFilename, bool bWarnIfAlreadyExists )
{
	// normalize filename, filenames need to compare correctly
	char cleanFilename[MAX_PATH];
	V_strncpy( cleanFilename, pFilename, sizeof( cleanFilename ) );
	V_RemoveDotSlashes( cleanFilename, CORRECT_PATH_SEPARATOR );

	// some vpc scripts decided to unecessarily double quote their filenames
	// remove any incoming surrounding quotes, this only causes string handling problems (i.e. extension comparison, etc)
	// all files get serialized to xml output with mandatory surrounding quotes
	if ( cleanFilename[0] == '\"' )
	{
		int len = strlen( cleanFilename );
		if ( len > 1 && cleanFilename[len-1] == '\"' )
		{
			memcpy( cleanFilename, cleanFilename+1, len - 2 );
			cleanFilename[len-2] = '\0';
		}
	}

	pFilename = cleanFilename;

	BaseClass::StartFile( pFilename, bWarnIfAlreadyExists );

	CProjectFile *pFile = NULL;

	if ( m_spFolderStack.Count() == 0 )
	{
		// add at root
		AddFileToFolder( pFilename, NULL, bWarnIfAlreadyExists, &pFile );
	}
	else
	{
		// add at subfolder
		AddFileToFolder( pFilename, m_spFolderStack.Top(), bWarnIfAlreadyExists, &pFile );
	}

	m_pProjectFile = pFile;
	return ( pFile != NULL );
}

void CVCProjGenerator::EndFile()
{
	BaseClass::EndFile();
}

bool CVCProjGenerator::RemoveFile( const char *pFilename )
{
	// normalize filename, filenames need to compare correctly
	char cleanFilename[MAX_PATH];
	V_strncpy( cleanFilename, pFilename, sizeof( cleanFilename ) );
	V_RemoveDotSlashes( cleanFilename, CORRECT_PATH_SEPARATOR );
	pFilename = cleanFilename;

	BaseClass::RemoveFile( pFilename );

	bool bValid;
	if ( m_spFolderStack.Count() == 0 )
	{
		// remove from root
		bValid = RemoveFileFromFolder( pFilename, NULL );
	}
	else
	{
		// remove at subfolder
		bValid = RemoveFileFromFolder( pFilename, m_spFolderStack.Top() );
	}

	return bValid;
}

bool CVCProjGenerator::Config_GetConfigurations( const char *pszConfigName )
{
	CProjectConfiguration *pConfig = NULL;
	bool bValid = GetRootConfiguration( pszConfigName, &pConfig );
	if ( !bValid )
	{
		g_pVPC->VPCError( "Could not get configuration '%s'", pszConfigName );
	}
	m_pConfig = pConfig;

	return true;
}

void CVCProjGenerator::StartConfigurationBlock( const char *pConfigName, bool bFileSpecific )
{
	BaseClass::StartConfigurationBlock( pConfigName, bFileSpecific );

	if ( bFileSpecific )
	{
		CProjectConfiguration *pFileConfig = NULL;
		bool bValid = m_pProjectFile->GetConfiguration( pConfigName, &pFileConfig );
		if ( !bValid )
		{
			// not found, must be valid config
			// must match predefined configurations, prevents misspellings
			if ( !IsConfigurationNameValid( pConfigName ) )
			{
				g_pVPC->VPCError( "File %s, Unknown configuration '%s'", m_pProjectFile->m_Name.Get(), pConfigName );
			}

			bValid = m_pProjectFile->AddConfiguration( pConfigName, &pFileConfig );
			if ( !bValid )
			{
				g_pVPC->VPCError( "File %s, Could not get file configuration '%s'", m_pProjectFile->m_Name.Get(), pConfigName );
			}
		}
		m_pFileConfig = pFileConfig;
	}
	else
	{
		Config_GetConfigurations( pConfigName );
	}
}

void CVCProjGenerator::EndConfigurationBlock()
{
	BaseClass::EndConfigurationBlock();

	if ( m_pFileConfig && m_pFileConfig->IsEmpty() )
	{
		// any file configuration (after parsing) that has no property state gets purged
		m_pProjectFile->RemoveConfiguration( m_pFileConfig );
	}

	m_pFileConfig = NULL;
}

void CVCProjGenerator::FileExcludedFromBuild( bool bExcluded )
{
	if ( !m_pFileConfig )
	{ 
		g_pVPC->VPCSyntaxError( "Cannot set $ExcludedFromBuild unless in a $File configuration context" );
	}

	BaseClass::FileExcludedFromBuild( bExcluded );

	ToolProperty_t* pToolProperty = m_pGeneratorDefinition->GetProperty( KEYWORD_GENERAL, "$ExcludedFromBuild" );
	if ( !pToolProperty )
	{
		g_pVPC->VPCError( "Missing proper declaration for $ExcludedFromBuild" );
	}

	m_pFileConfig->m_PropertyStates.SetBoolProperty( pToolProperty, bExcluded );
}

bool CVCProjGenerator::StartPropertySection( configKeyword_e eKeyword, bool *pbShouldSkip )
{
	BaseClass::StartPropertySection( eKeyword );

	*pbShouldSkip = false;
	m_nActivePropertySection = KEYWORD_UNKNOWN;
	bool bHandled = false;

	switch ( eKeyword )
	{
	case KEYWORD_GENERAL:
		bHandled = true;
		break;

	case KEYWORD_DEBUGGING:
		m_pDebuggingTool = m_pConfig->GetDebuggingTool();
		if ( !m_pDebuggingTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_COMPILER:
	case KEYWORD_PS3_SNCCOMPILER:
	case KEYWORD_PS3_GCCCOMPILER:
		eKeyword = SetPS3VisualStudioIntegrationType( eKeyword );
		if ( eKeyword == KEYWORD_UNKNOWN )
		{
			// skip this section
			break;
		}

		m_spCompilerStack.Push( m_pCompilerTool );
		if ( m_pFileConfig )
		{
			m_pCompilerTool = m_pFileConfig->GetCompilerTool();
		}
		else
		{
			m_pCompilerTool = m_pConfig->GetCompilerTool();
		}
		if ( !m_pCompilerTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_LIBRARIAN:
		m_pLibrarianTool = m_pConfig->GetLibrarianTool();
		if ( !m_pLibrarianTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_LINKER:
	case KEYWORD_PS3_SNCLINKER:
	case KEYWORD_PS3_GCCLINKER:
		eKeyword = SetPS3VisualStudioIntegrationType( eKeyword );
		if ( eKeyword == KEYWORD_UNKNOWN )
		{
			// skip this section
			break;
		}

		m_pLinkerTool = m_pConfig->GetLinkerTool();
		if ( !m_pLinkerTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_MANIFEST:
		if ( !( g_pVPC->IsPlatformDefined( "WIN32" ) || g_pVPC->IsPlatformDefined( "WIN64" ) ) )
		{
			// windows specific
			break;
		}

		m_pManifestTool = m_pConfig->GetManifestTool();
		if ( !m_pManifestTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_XMLDOCGEN:
		if ( !( g_pVPC->IsPlatformDefined( "WIN32" ) || g_pVPC->IsPlatformDefined( "WIN64" ) ) )
		{
			// windows specific
			break;
		}

		m_pXMLDocGenTool = m_pConfig->GetXMLDocGenTool();
		if ( !m_pXMLDocGenTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_BROWSEINFO:
		if ( g_pVPC->IsPlatformDefined( "PS3" ) )
		{
			// not for ps3
			break;
		}

		m_pBrowseInfoTool = m_pConfig->GetBrowseInfoTool();
		if ( !m_pBrowseInfoTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_RESOURCES:
		if ( !( g_pVPC->IsPlatformDefined( "WIN32" ) || g_pVPC->IsPlatformDefined( "WIN64" ) ) )
		{
			// windows specific
			break;
		}

		m_pResourcesTool = m_pConfig->GetResourcesTool();
		if ( !m_pResourcesTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_PREBUILDEVENT:
		m_pPreBuildEventTool = m_pConfig->GetPreBuildEventTool();
		if ( !m_pPreBuildEventTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_PRELINKEVENT:
		m_pPreLinkEventTool = m_pConfig->GetPreLinkEventTool();
		if ( !m_pPreLinkEventTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_POSTBUILDEVENT:
		m_pPostBuildEventTool = m_pConfig->GetPostBuildEventTool();
		if ( !m_pPostBuildEventTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_CUSTOMBUILDSTEP:
		m_spCustomBuildToolStack.Push( m_pCustomBuildTool );
		if ( m_pFileConfig )
		{
			m_pCustomBuildTool = m_pFileConfig->GetCustomBuildTool();
		}
		else
		{
			m_pCustomBuildTool = m_pConfig->GetCustomBuildTool();
		}
		if ( !m_pCustomBuildTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_XBOXIMAGE:
		if ( !g_pVPC->IsPlatformDefined( "X360" ) )
		{
			// xbox generator specific
			break;
		}

		m_pXboxImageTool = m_pConfig->GetXboxImageTool();
		if ( !m_pXboxImageTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_XBOXDEPLOYMENT:
		if ( !g_pVPC->IsPlatformDefined( "X360" ) )
		{
			// xbox generator specific
			break;
		}

		m_pXboxDeploymentTool = m_pConfig->GetXboxDeploymentTool();
		if ( !m_pXboxDeploymentTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	default:
		// unknown
		return false;
	}

	if ( bHandled )
	{
		// handled
		m_nActivePropertySection = eKeyword;
	}
	else
	{
		// allow other platform specifc sections to just be quietly ignored
		*pbShouldSkip = true;
	}

	return true;
}
				
void CVCProjGenerator::EndPropertySection( configKeyword_e eKeyword )
{
	BaseClass::EndPropertySection( eKeyword );

	switch( eKeyword )
	{
	case KEYWORD_CUSTOMBUILDSTEP:
		m_spCustomBuildToolStack.Pop( m_pCustomBuildTool );
		break;

	case KEYWORD_COMPILER:
	case KEYWORD_PS3_SNCCOMPILER:
	case KEYWORD_PS3_GCCCOMPILER:
		eKeyword = SetPS3VisualStudioIntegrationType( eKeyword );
		m_spCompilerStack.Pop( m_pCompilerTool );
		break;
	}

	m_nActivePropertySection = KEYWORD_UNKNOWN;
}

void CVCProjGenerator::HandleProperty( const char *pPropertyName, const char *pCustomScriptData )
{
	// don't allow the baseclass to alter the script state
	g_pVPC->GetScript().PushCurrentScript();
	BaseClass::HandleProperty( pPropertyName, pCustomScriptData );
	g_pVPC->GetScript().PopScript();

	if ( pCustomScriptData )
	{
		g_pVPC->GetScript().PushScript( "HandleProperty custom data", pCustomScriptData );
	}

	ToolProperty_t *pToolProperty = m_pGeneratorDefinition->GetProperty( m_nActivePropertySection, pPropertyName );
	if ( !pToolProperty )
	{
		// unknown property
		g_pVPC->VPCSyntaxError( "Unknown property %s", pPropertyName );
	}

	const char *pToken = g_pVPC->GetScript().PeekNextToken( false );
	if ( !pToken || !pToken[0] )
	{
		// quietly ignoring any property without a value
		// not an error
		if ( pCustomScriptData )
		{
			g_pVPC->GetScript().PopScript();
		}
		return;
	}

	CProjectConfiguration *pConfig = NULL;
	CProjectTool *pTool = NULL;
	switch ( m_nActivePropertySection )
	{
	case KEYWORD_GENERAL:
		pConfig = m_pConfig;
		break;

	case KEYWORD_DEBUGGING:
		pTool = m_pDebuggingTool;
		break;

	case KEYWORD_COMPILER:
	case KEYWORD_PS3_SNCCOMPILER:
	case KEYWORD_PS3_GCCCOMPILER:
		pTool = m_pCompilerTool;
		break;
	
	case KEYWORD_LIBRARIAN:
		pTool = m_pLibrarianTool;
		break;

	case KEYWORD_LINKER:
	case KEYWORD_PS3_SNCLINKER:
	case KEYWORD_PS3_GCCLINKER:
		pTool = m_pLinkerTool;
		break;

	case KEYWORD_MANIFEST:
		pTool = m_pManifestTool;
		break;

	case KEYWORD_XMLDOCGEN:
		pTool = m_pXMLDocGenTool;
		break;

	case KEYWORD_BROWSEINFO:
		pTool = m_pBrowseInfoTool;
		break;

	case KEYWORD_RESOURCES:
		pTool = m_pResourcesTool;
		break;

	case KEYWORD_PREBUILDEVENT:
		pTool = m_pPreBuildEventTool;
		break;

	case KEYWORD_PRELINKEVENT:
		pTool = m_pPreLinkEventTool;
		break;

	case KEYWORD_POSTBUILDEVENT:
		pTool = m_pPostBuildEventTool;
		break;

	case KEYWORD_CUSTOMBUILDSTEP:
		pTool = m_pCustomBuildTool;
		break;

	case KEYWORD_XBOXIMAGE:
		pTool = m_pXboxImageTool;
		break;

	case KEYWORD_XBOXDEPLOYMENT:
		pTool = m_pXboxDeploymentTool;
		break;

	default:
		g_pVPC->VPCError( "HandleProperty: No support for Tool:%s Property:%s - requires implementation", g_pVPC->KeywordToName( m_nActivePropertySection ), pPropertyName );
	}

	bool bHandled = false;
	if ( pTool )
	{
		bHandled = pTool->SetProperty( pToolProperty );
	}
	else if ( pConfig )
	{
		bHandled = pConfig->SetProperty( pToolProperty );
	}

	if ( !bHandled )
	{
		g_pVPC->VPCError( "HandleProperty: Failed to set %s", pPropertyName );
	}

	if ( pCustomScriptData )
	{
		g_pVPC->GetScript().PopScript();
	}
}

bool CVCProjGenerator::GetFolder( const char *pFolderName, CProjectFolder *pParentFolder, CProjectFolder **ppOutFolder )
{
	bool bValid;
	if ( !pParentFolder )
	{
		bValid = m_pRootFolder->GetFolder( pFolderName, ppOutFolder );
	}
	else
	{
		bValid = pParentFolder->GetFolder( pFolderName, ppOutFolder );
	}
	return bValid;
}

bool CVCProjGenerator::AddFolder( const char *pFolderName, CProjectFolder *pParentFolder, CProjectFolder **ppOutFolder )
{
	bool bValid;
	if ( !pParentFolder )
	{
		bValid = m_pRootFolder->AddFolder( pFolderName, ppOutFolder );
	}
	else
	{
		bValid = pParentFolder->AddFolder( pFolderName, ppOutFolder );
	}
	return bValid;
}

bool CVCProjGenerator::FindFile( const char *pFilename, CProjectFile **ppFile )
{
	CProjectFile findProjectFile( this, pFilename );

	int iIndex = m_FileDictionary.Find( &findProjectFile );
	if ( iIndex != m_FileDictionary.InvalidIndex() )
	{
		// found
		if ( ppFile )
		{
			*ppFile = m_FileDictionary[iIndex];
		}
		return true;
	}
	
	// not found
	if ( ppFile )
	{
		*ppFile = NULL;
	}

	return false;
}

void CVCProjGenerator::AddFileToFolder( const char *pFilename, CProjectFolder *pFolder, bool bWarnIfAlreadyExists, CProjectFile **ppFile )
{
	if ( FindFile( pFilename, ppFile ) )
	{
		// already present
		if ( bWarnIfAlreadyExists )
		{
			g_pVPC->VPCWarning( "File %s already exists in project", pFilename );
		}
		return;
	}

	CProjectFile *pFile;
	if ( !pFolder )
	{
		// add at root
		m_pRootFolder->AddFile( pFilename, &pFile );
	}
	else
	{
		// add at folder
		pFolder->AddFile( pFilename, &pFile );
	}

	// add to dictionary
	m_FileDictionary.Insert( pFile );
	if ( ppFile )
	{
		*ppFile = pFile;
	}
}

bool CVCProjGenerator::RemoveFileFromFolder( const char *pFilename, CProjectFolder *pFolder )
{
	bool bFound = false;
	CProjectFile findProjectFile( this, pFilename );

	int iIndex = m_FileDictionary.Find( &findProjectFile );
	if ( iIndex != m_FileDictionary.InvalidIndex() )
	{
		bFound = true;
		m_FileDictionary.RemoveAt( iIndex );
	}

	if ( !bFound )
		return false;

	if ( !pFolder )
	{
		m_pRootFolder->RemoveFile( pFilename );
	}
	else
	{
		pFolder->RemoveFile( pFilename );
	}

	return bFound;
}

void CVCProjGenerator::GetAllConfigurationNames( CUtlVector< CUtlString > &configurationNames )
{
	configurationNames.Purge();
	for ( int i = 0; i < m_RootConfigurations.Count(); i++ )
	{
		configurationNames.AddToTail( m_RootConfigurations[i]->m_Name.Get() );
	}
}

bool CVCProjGenerator::GetRootConfiguration( const char *pConfigName, CProjectConfiguration **ppConfig )
{
	if ( !pConfigName || !pConfigName[0] )
	{
		g_pVPC->VPCError( "Empty or bad configuration name." );
	}

	if ( ppConfig )
	{
		// assume not found
		*ppConfig = NULL;
	}

	for ( int i = 0; i < m_RootConfigurations.Count(); i++ )
	{
		if ( !V_stricmp( m_RootConfigurations[i]->m_Name.Get(), pConfigName ) )
		{
			// found
			if ( ppConfig )
			{
				*ppConfig = m_RootConfigurations[i];
			}
			return true;
		}
	}

	return false;
}

bool CVCProjGenerator::IsConfigurationNameValid( const char *pConfigName )
{
	return GetRootConfiguration( pConfigName, NULL );
}

configKeyword_e CVCProjGenerator::SetPS3VisualStudioIntegrationType( configKeyword_e eKeyword )
{
	PS3VSIType_e vsiType = PS3_VSI_TYPE_UNDEFINED;

	switch ( eKeyword )
	{
	case KEYWORD_COMPILER:
	case KEYWORD_LINKER:
		if ( !g_pVPC->IsPlatformDefined( "PS3" ) )
		{
			return eKeyword;
		}

		if ( m_VSIType == PS3_VSI_TYPE_UNDEFINED )
		{
			// PS3 defaults to SNC, unless explictly specified
			vsiType = PS3_VSI_TYPE_SNC;
		}
		else
		{
			// already set
			vsiType = m_VSIType;
		}
		break;

	case KEYWORD_PS3_SNCCOMPILER:
	case KEYWORD_PS3_SNCLINKER:
		if ( !g_pVPC->IsPlatformDefined( "PS3" ) )
		{
			// ps3 generator specific
			// not available for other platforms
			return KEYWORD_UNKNOWN;
		}
		vsiType = PS3_VSI_TYPE_SNC;
		break;

	case KEYWORD_PS3_GCCCOMPILER:
	case KEYWORD_PS3_GCCLINKER:
		if ( !g_pVPC->IsPlatformDefined( "PS3" ) )
		{
			// ps3 generator specific
			// not available for other platforms
			return KEYWORD_UNKNOWN;
		}
		vsiType = PS3_VSI_TYPE_GCC;
		break;

	default:
		g_pVPC->VPCError( "Unknown PS3 compiler/linker type" );
		break;
	}

	if ( m_VSIType == PS3_VSI_TYPE_UNDEFINED )
	{
		// once set, compiler/linker choice (snc or gcc) cannot be changed
		m_VSIType = vsiType;
	}
	else if ( m_VSIType != vsiType )
	{
		// cannot intermix tool properties, they must be exclusive
		g_pVPC->VPCSyntaxError( "PS3 compiler/linker (GCC or SNC) already set, cannot be changed" );
	}

	// remap ambiguous compiler/linker tool to explicit SNC/GCC tool flavor
	if ( eKeyword == KEYWORD_COMPILER )
	{
		eKeyword = ( m_VSIType == PS3_VSI_TYPE_SNC ) ? KEYWORD_PS3_SNCCOMPILER : KEYWORD_PS3_GCCCOMPILER;
	}
	else if ( eKeyword == KEYWORD_LINKER )
	{
		eKeyword = ( m_VSIType == PS3_VSI_TYPE_SNC ) ? KEYWORD_PS3_SNCLINKER : KEYWORD_PS3_GCCLINKER;
	}

	return eKeyword;
}

void CVCProjGenerator::ApplyInternalPreprocessorDefinitions()
{
	// prep to add in vpc generated compiler defines
	CUtlVector< macro_t* > macroDefines;
	g_pVPC->GetMacrosMarkedForCompilerDefines( macroDefines );

	if ( !macroDefines.Count() )
	{
		// nothing to fixup
		return;
	}

	// get all the vpc macros that have been marked for auto adding as compiler define
	CUtlString extraDefineString;
	for ( int i = 0; i < macroDefines.Count(); i++ )
	{
		macro_t *pMacro = macroDefines[i];

		CUtlString tempString;
		tempString.Format( ";%s=%s", pMacro->name.String(), pMacro->value.String() );
		extraDefineString += tempString;
	}

	// fixup root configurations
	for ( int i = 0; i < m_RootConfigurations.Count(); i++ )
	{
		CCompilerTool *pCompilerTool = m_RootConfigurations[i]->GetCompilerTool();
		if ( pCompilerTool )
		{
			PropertyState_t *pPropertyState = pCompilerTool->m_PropertyStates.GetProperty( "$PreprocessorDefinitions" );
			if ( pPropertyState )
			{
				pPropertyState->m_StringValue += extraDefineString;
			}
		}
	}

	// fixup any file confiuration overrides
	for ( int iIndex = m_FileDictionary.FirstInorder(); iIndex != m_FileDictionary.InvalidIndex(); iIndex = m_FileDictionary.NextInorder( iIndex ) )
	{
		CProjectFile *pProjectFile = m_FileDictionary[iIndex];
		for ( int i = 0; i < pProjectFile->m_Configs.Count(); i++ )
		{
			CCompilerTool *pCompilerTool = pProjectFile->m_Configs[i]->GetCompilerTool();
			if ( pCompilerTool )
			{
				PropertyState_t *pPropertyState = pCompilerTool->m_PropertyStates.GetProperty( "$PreprocessorDefinitions" );
				if ( pPropertyState )
				{
					pPropertyState->m_StringValue += extraDefineString;
				}
			}
		}
	}
}
