//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "vpc.h"
#include "dependencies.h"
#include "baseprojectdatacollector.h"
#include "tier0/fasttimer.h"


#define VPC_CRC_CACHE_VERSION 3

extern char *g_IncludeSeparators[2];

static const char *g_pDependencyRelevantProperties[] =
{
	g_pOption_AdditionalProjectDependencies,
	g_pOption_AdditionalOutputFiles,
	g_pOption_AdditionalIncludeDirectories,
	g_pOption_GameOutputFile,
	g_pOption_ImportLibrary,
	g_pOption_OutputFile,
};

static CRelevantPropertyNames g_DependencyRelevantPropertyNames =
{
	g_pDependencyRelevantProperties,
	V_ARRAYSIZE( g_pDependencyRelevantProperties )
};


bool IsSharedLibraryFile( const char *pFilename )
{
	const char *pExt = V_GetFileExtension( pFilename );
	if ( pExt && ( V_stricmp( pExt, "so" ) == 0 || V_stricmp( pExt, "dylib" ) == 0 || V_stricmp( pExt, "dll" ) == 0 ) ) 
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool IsLibraryFile( const char *pFilename )
{
	const char *pExt = V_GetFileExtension( pFilename );
	if ( IsSharedLibraryFile( pFilename ) || ( pExt && ( V_stricmp( pExt, "lib" ) == 0 || V_stricmp( pExt, "a" ) == 0 ) ) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

static inline bool IsSourceFile( const char *pFilename )
{
	const char *pExt = V_GetFileExtension( pFilename );

	if ( pExt && ( IsCFileExtension( pExt ) || IsHFileExtension( pExt ) ||
					!V_stricmp( pExt, "rc" ) || !V_stricmp( pExt, "inc" ) ) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



// ------------------------------------------------------------------------------------------------------- //
// CDependency functions.
// ------------------------------------------------------------------------------------------------------- //

CDependency::CDependency( CProjectDependencyGraph *pDependencyGraph ) :
	m_pDependencyGraph( pDependencyGraph )
{
	m_iDependencyMark = m_pDependencyGraph->m_iDependencyMark - 1;
	m_bCheckedIncludes = false;
	m_nCacheModificationTime = m_nCacheFileSize = 0;
}

CDependency::~CDependency()
{
}

const char* CDependency::GetName() const
{
	return m_Filename.String();
}

bool CDependency::CompareAbsoluteFilename( const char *pAbsPath  ) const
{
	return ( V_stricmp( m_Filename.String(), pAbsPath ) == 0 );
}

bool CDependency::DependsOn( CDependency *pTest, int flags )
{
	m_pDependencyGraph->ClearAllDependencyMarks();
	CUtlVector<CUtlBuffer> callTreeOutputStack;
	if ( FindDependency_Internal( callTreeOutputStack, pTest, flags, 1 ) )
	{
		if ( g_pVPC->IsShowDependencies() )
		{
			printf( "-------------------------------------------------------------------------------\n" );
			printf( "%s\n", GetName() );
			int i;
			for( i = callTreeOutputStack.Count() - 1; i >= 0; i-- )
			{
				printf( ( const char * )callTreeOutputStack[i].Base() );
			}
			printf( "-------------------------------------------------------------------------------\n" );
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool CDependency::FindDependency_Internal( CUtlVector<CUtlBuffer> &callTreeOutputStack, CDependency *pTest, int flags, int depth )
{
	if ( pTest == this )
		return true;

	// Don't revisit us.
	if ( HasBeenMarked() )
		return false;

	Mark();

	// Don't recurse further?
	if ( depth > 1 && !(flags & k_EDependsOnFlagRecurse) )
		return false;

	// Don't go into the children of libs if they don't want.
	if ( !(flags & k_EDependsOnFlagTraversePastLibs) && m_Type == k_eDependencyType_Library )
		return false;

	// Go through everything I depend on. If any of those things 
	for ( int iDepList=0; iDepList < 2; iDepList++ )
	{
		if ( iDepList == 1 && !(flags & k_EDependsOnFlagCheckAdditionalDependencies) )
			continue;

		CUtlVector<CDependency*> &depList = (iDepList == 0 ? m_Dependencies : m_AdditionalDependencies);

		for ( int i=0; i < depList.Count(); i++ )
		{
			CDependency *pChild = depList[i];
			if ( pChild->FindDependency_Internal( callTreeOutputStack, pTest, flags, depth+1 ) )
			{
				if ( g_pVPC->IsShowDependencies() )
				{
					char buf[2048];
					V_strncpy( buf, "depends on ", sizeof( buf ) );
					V_strcat( buf, pChild->GetName(), sizeof( buf ) );
					V_strcat( buf, "\n", sizeof( buf ) );
					int n = callTreeOutputStack.AddToTail();
					CUtlBuffer &b = callTreeOutputStack[n];
					b.EnsureCapacity( V_strlen( buf ) + 2 );
					b.PutString( buf );
					b.PutChar( 0 );
				}
				return true;
			}
		}
	}

	return false;
}

void CDependency::Mark()
{
	m_iDependencyMark = m_pDependencyGraph->m_iDependencyMark;
}

bool CDependency::HasBeenMarked()
{
	return m_iDependencyMark == m_pDependencyGraph->m_iDependencyMark;
}


CDependency_Project::CDependency_Project( CProjectDependencyGraph *pDependencyGraph )
	: CDependency( pDependencyGraph )
{
	m_iProjectIndex = -1;
}


void CDependency_Project::StoreProjectParameters( const char *szScriptName )
{
	m_StoredOutputFilename = g_pVPC->GetOutputFilename();
	V_GetCurrentDirectory( m_szStoredCurrentDirectory, sizeof( m_szStoredCurrentDirectory ) );
	V_strncpy( m_szStoredScriptName, szScriptName, sizeof( m_szStoredScriptName ) );
	m_StoredConditionalsActive.SetSize( g_pVPC->m_Conditionals.Count() );

	for ( int iConditional=0; iConditional < g_pVPC->m_Conditionals.Count(); iConditional++ )
	{
		m_StoredConditionalsActive[iConditional] = g_pVPC->m_Conditionals[iConditional].m_bGameConditionActive;
	}
}


void CDependency_Project::ExportProjectParameters()
{
	g_pVPC->SetOutputFilename( m_StoredOutputFilename.Get() );
	V_SetCurrentDirectory( m_szStoredCurrentDirectory );
	
	if ( m_StoredConditionalsActive.Count() > g_pVPC->m_Conditionals.Count() )
	{
		g_pVPC->VPCError( "ExportProjectParameters( %s ) - too many defines stored.", m_szStoredScriptName );
	}
	
	for ( int iConditional=0; iConditional < g_pVPC->m_Conditionals.Count(); iConditional++ )
	{
		g_pVPC->m_Conditionals[iConditional].m_bGameConditionActive = m_StoredConditionalsActive[iConditional];
	}
}

int CDependency_Project::FindByProjectName( CUtlVector<CDependency_Project*> &projects, const char *pTestName )
{
	for ( int i=0; i < projects.Count(); i++ )
	{
		CDependency_Project *pProject = projects[i];

		if ( V_stricmp( pProject->m_ProjectName.String(), pTestName ) == 0 )
			return i;
	}

	return -1;
}


// This is responsible for scanning a project file and pulling out:
// - a list of libraries it uses
// - the $AdditionalIncludeDirectories paths
// - a list of source files it uses
// - the name of the file it generates
class CSingleProjectScanner : public CBaseProjectDataCollector
{
public:
	typedef CBaseProjectDataCollector BaseClass;

	CSingleProjectScanner() : CBaseProjectDataCollector( &g_DependencyRelevantPropertyNames )
	{
		m_bInLinker = false;
	}
	
	virtual void EndProject()
	{
	}

	void ScanProjectFile( CProjectDependencyGraph *pGraph, const char *szScriptName, CDependency_Project *pProject )
	{
		// Someday we'll pass this interface down into VPC_ParseProjectScript instead of using the global.
		IBaseProjectGenerator *pOldGenerator = g_pVPC->GetProjectGenerator();
		g_pVPC->SetProjectGenerator( this );

		// This has VPC parse the script and CBaseProjectDataCollector collects all the data into lists of the
		// stuff we care about like source files and include paths.
		m_ScriptName = szScriptName;
		g_pVPC->ParseProjectScript( szScriptName, 0, true, false );

		int iConfig = m_BaseConfigData.m_Configurations.First();
		if ( iConfig != m_BaseConfigData.m_Configurations.InvalidIndex() )
		{
			CSpecificConfig *pConfig = m_BaseConfigData.m_Configurations[iConfig];

			SetupIncludeDirectories( pConfig, szScriptName );
			SetupFilesList( pGraph, pProject );
			SetupImportLibrary( pGraph, pConfig, szScriptName );
			SetupAdditionalProjectDependencies( pProject, pConfig );
			SetupAdditionalOutputFiles( pProject, pConfig );

		}

		g_pVPC->SetProjectGenerator( pOldGenerator );
		Term();
	}

	void SetupFilesList( CProjectDependencyGraph *pGraph, CDependency_Project *pProject )
	{
		for ( int i=m_Files.First(); i != m_Files.InvalidIndex(); i=m_Files.Next( i ) )
		{
			CFileConfig *pFile = m_Files[i];

			// If this file is excluded from all configs, then exclude it.
			if ( pFile->m_Configurations.Count() > 0 )
			{
				int nExcluded = 0;
				for ( int iSpecific=pFile->m_Configurations.First(); iSpecific != pFile->m_Configurations.InvalidIndex(); iSpecific=pFile->m_Configurations.Next( iSpecific ) )
				{
					CSpecificConfig *pTest = pFile->m_Configurations[iSpecific];
					if ( pTest->m_bFileExcluded && !pTest->m_bIsSchema )
						++nExcluded;
				}
				if ( nExcluded == (int)m_BaseConfigData.m_Configurations.Count() )
					continue;
			}


			// Make this an absolute path.
			const char *pFilename = pFile->GetName();
			char sAbsolutePath[MAX_PATH];
			V_MakeAbsolutePath( sAbsolutePath, sizeof( sAbsolutePath ), pFilename );

			// Don't bother with source files if we're not building the full dependency set.			
			if ( !pGraph->m_bFullDependencySet )
				if ( IsSourceFile( sAbsolutePath ) )
					continue;

			// For source files, don't bother with files that don't exist. If we do create entries
			// for files that don't exist, then they'll have a "cache file size"
			if ( !Sys_Exists( sAbsolutePath ) && IsSourceFile( sAbsolutePath ) )
				continue;

			// Add an entry for this file.
			CDependency *pDep = pGraph->FindOrCreateDependency( sAbsolutePath );
			pProject->m_Dependencies.AddToTail( pDep );

			// Add includes.
			if ( pDep->m_Type == k_eDependencyType_SourceFile )
				AddIncludesForFile( pGraph, pDep );
		}
	}

	void AddIncludesForFile( CProjectDependencyGraph *pGraph, CDependency *pFile )
	{
		// Have we already parsed this file for its includes?
		if ( pFile->m_bCheckedIncludes )
			return;
		
		pFile->m_bCheckedIncludes = true;

		// Setup all the include paths we want to search.
		CUtlVector<CUtlString> includeDirs;
		char szDir[MAX_PATH];
		if ( !V_ExtractFilePath( pFile->GetName(), szDir, sizeof( szDir ) ) )
			g_pVPC->VPCError( "AddIncludesForFile: V_ExtractFilePath( %s ) failed.", pFile->GetName() );

		includeDirs.AddToTail( szDir );
		includeDirs.AddMultipleToTail( m_IncludeDirectories.Count(), m_IncludeDirectories.Base() );

		// Get all the #include directives.
		CUtlVector<CUtlString> includes;
		GetIncludeFiles( pFile->GetName(), includes );
		++pGraph->m_nFilesParsedForIncludes;

		// Now see which of them we can open.
		for ( int iIncludeFile=0; iIncludeFile < includes.Count(); iIncludeFile++ )
		{
			for ( int iIncludeDir=0; iIncludeDir < includeDirs.Count(); iIncludeDir++ )
			{
				char szFullName[MAX_PATH];
				V_ComposeFileName( includeDirs[iIncludeDir].String(), includes[iIncludeFile].String(), szFullName, sizeof( szFullName ) );
			
				CDependency *pIncludeFile = pGraph->FindDependency( szFullName );
				if ( !pIncludeFile )
				{
					if ( !Sys_Exists( szFullName ) )
						continue;

					// Find or add the dependency.
					pIncludeFile = pGraph->FindOrCreateDependency( szFullName );
				}
				pFile->m_Dependencies.AddToTail( pIncludeFile );

				// Recurse.
				AddIncludesForFile( pGraph, pIncludeFile );
			}
		}
	}

	bool SeekToIncludeStart( const char* &pSearchPos )
	{
		while ( 1 )
		{
			++pSearchPos;
			if ( *pSearchPos == 0 || *pSearchPos == '\r' || *pSearchPos == '\n' )
				return false;

			if ( *pSearchPos == '\"' || *pSearchPos == '<' )
			{
				++pSearchPos;
				return true;
			}
		}
	}

	bool SeekToIncludeEnd( const char* &pSearchPos )
	{
		while ( 1 )
		{
			++pSearchPos;
			if ( *pSearchPos == 0 || *pSearchPos == '\r' || *pSearchPos == '\n' )
				return false;

			if ( *pSearchPos == '\"' || *pSearchPos == '>' )
				return true;
		}
	}

	void GetIncludeFiles( const char *pFilename, CUtlVector<CUtlString> &includes )
	{
		char *pFileData;
		int ret = Sys_LoadFile( pFilename, (void**)&pFileData, false );
		if ( ret == -1 )
		{
			if ( g_pVPC->IsVerbose() )
			{
				g_pVPC->VPCWarning( "GetIncludeFiles( %s ) - can't open file (included by project %s).", pFilename, m_ScriptName.String() );
			}
			return;
		}

		const char *pSearchPos = pFileData;
		while ( 1 )
		{
			const char *pLookFor = "#include";
			const char *pIncludeStatement = V_strstr( pSearchPos, pLookFor );
			if ( !pIncludeStatement )
				break;
		
			pSearchPos = pIncludeStatement + V_strlen( pLookFor );
			
			if ( !SeekToIncludeStart( pSearchPos ) )
				continue;
			const char *pFilenameStart = pSearchPos;

			if ( !SeekToIncludeEnd( pSearchPos ) )
				continue;
			const char *pFilenameEnd = pSearchPos;

			if ( (pFilenameEnd - pFilenameStart) > MAX_PATH-10 )
				g_pVPC->VPCError( "Include statement too long in %s.", pFilename );

			char szIncludeFilename[MAX_PATH], szFixed[MAX_PATH];
			V_strncpy( szIncludeFilename, pFilenameStart, pFilenameEnd - pFilenameStart + 1 );

			// Fixup double slashes.
			V_StrSubst( szIncludeFilename, "\\\\", "\\", szFixed, sizeof( szFixed ) );
			V_FixSlashes( szFixed );

			includes.AddToTail( szFixed );
		}
		
		free( pFileData );		
	}

	void SetupIncludeDirectories( CSpecificConfig *pConfig, const char *szScriptName )
	{
		if ( m_BaseConfigData.m_Configurations.Count() == 0 )
			g_pVPC->VPCError( "No configurations for %s in project %s.", szScriptName, m_ScriptName.String() );

		const char *pIncludes = pConfig->m_pKV->GetString( g_pOption_AdditionalIncludeDirectories, "" );
		CSplitString relativeIncludeDirs( pIncludes, (const char**)g_IncludeSeparators, V_ARRAYSIZE( g_IncludeSeparators ) );

		for ( int i=0; i < relativeIncludeDirs.Count(); i++ )
		{
			char sAbsolute[MAX_PATH];
			V_MakeAbsolutePath( sAbsolute, sizeof( sAbsolute ), relativeIncludeDirs[i] );
			m_IncludeDirectories.AddToTail( sAbsolute );
		}
	}

	void SetupImportLibrary( CProjectDependencyGraph *pGraph, CSpecificConfig *pConfig, const char *szScriptName )
	{
		m_ImportLibrary = pConfig->m_pKV->GetString( g_pOption_ImportLibrary, NULL );
		// XXX(JohnS): For projects that define a separate "GameOutputFile" step, that is the final product.  This was
		//             kind of hackily added originally -- the $OutputFile directive was relative to the base directory,
		//             but some generators (XCode) put all their outputs into a object directory, then use
		//             $GameOutputFile to *actually* output.
		m_LinkerOutputFile = pConfig->m_pKV->GetString( g_pOption_GameOutputFile, NULL );
		if ( !m_LinkerOutputFile.Length() )
		{
			m_LinkerOutputFile = pConfig->m_pKV->GetString( g_pOption_OutputFile, NULL );
		}
	}

	void SetupAdditionalProjectDependencies( CDependency_Project *pProject, CSpecificConfig *pConfig )
	{
		const char *pVal = pConfig->m_pKV->GetString( g_pOption_AdditionalProjectDependencies );
		if ( pVal )
		{
			pProject->m_AdditionalProjectDependencies.Purge();

			CSplitString outStrings ( pVal, ";" );
			for ( int i=0; i < outStrings.Count(); i++ )
			{
				char szProjectName[MAX_PATH];
				sprintf( szProjectName, "%s", outStrings[i] );

				if ( g_pVPC->IsDecorateProject() )
				{
					g_pVPC->DecorateProjectName( szProjectName );
				}
				pProject->m_AdditionalProjectDependencies.AddToTail( szProjectName );
			}
		}
	}

	void SetupAdditionalOutputFiles( CDependency_Project *pProject, CSpecificConfig *pConfig )
	{
		const char *pVal = pConfig->m_pKV->GetString( g_pOption_AdditionalOutputFiles );
		if ( pVal )
		{
			pProject->m_AdditionalOutputFiles.Purge();

			CSplitString outStrings( pVal, ";" );
			for ( int i=0; i < outStrings.Count(); i++ )
			{
				pProject->m_AdditionalOutputFiles.AddToTail( outStrings[i] );
			}
		}
	}

	virtual const char* GetProjectFileExtension()
	{
		return "UNUSED";
	}

protected:
	
	virtual bool StartPropertySection( configKeyword_e keyword, bool *pbShouldSkip )
	{
		m_bInLinker = ( keyword == KEYWORD_LINKER || keyword == KEYWORD_LIBRARIAN );
		return true;
	}
	
	virtual void HandleProperty( const char *pProperty, const char *pCustomScriptData )
	{
		// We don't want the $OutputFile property from the $BrowseInformation section.
		if ( V_stricmp( pProperty, g_pOption_OutputFile ) == 0 && !m_bInLinker )
			return;

		BaseClass::HandleProperty( pProperty, pCustomScriptData );
	}

	virtual void EndPropertySection( configKeyword_e keyword )
	{
		m_bInLinker = false;
	}

public:
	// Project include directories. These strings are deleted when the object goes away.
	CUtlVector<CUtlString> m_IncludeDirectories;
	CUtlString m_ImportLibrary;
	CUtlString m_LinkerOutputFile;
	CUtlString m_ScriptName;
	bool m_bInLinker;
};


CProjectDependencyGraph::CProjectDependencyGraph()
{
	m_iDependencyMark = 0;
	m_bFullDependencySet = false;
	m_bHasGeneratedDependencies = false;
}


void CProjectDependencyGraph::BuildProjectDependencies( int nBuildProjectDepsFlags, CUtlVector< CDependency_Project *> *pPhase1Projects )
{
	m_bFullDependencySet = ( ( nBuildProjectDepsFlags & BUILDPROJDEPS_FULL_DEPENDENCY_SET ) != 0 );
	m_nFilesParsedForIncludes = 0;
	
	if ( m_bFullDependencySet )
	{
		Log_Msg( LOG_VPC, "\nBuilding full dependency set (all sources and headers)..." );
	}
	else
	{
		Log_Msg( LOG_VPC, "\nBuilding partial dependency set (libs only)..." );
	}

	// Have it iterate ALL projects in the list, with whatever platform conditionals are around.
	// When it visits a 
	CUtlVector<projectIndex_t> projectList;
	CUtlVector<int> oldState;

	if ( nBuildProjectDepsFlags & BUILDPROJDEPS_CHECK_ALL_PROJECTS )
	{
		// So iterate all projects.
		projectList.SetSize( g_pVPC->m_Projects.Count() );
		for ( int i=0; i < g_pVPC->m_Projects.Count(); i++ )
			projectList[i] = i;

		// Simulate /allgames but remember the old state too.
		for ( int j=0; j<g_pVPC->m_Conditionals.Count(); j++ )
		{
			if ( g_pVPC->m_Conditionals[j].type == CONDITIONAL_GAME )
			{
				oldState.AddToTail( (j << 16) + (int)g_pVPC->m_Conditionals[j].m_bDefined );
				g_pVPC->m_Conditionals[j].m_bDefined = true;
			}
		}
	}
	else
	{
		projectList.AddMultipleToTail( g_pVPC->m_TargetProjects.Count(), g_pVPC->m_TargetProjects.Base() );
	}

	// Load any prior results so we don't have to regenerate the whole cache (which can take a couple minutes).
	char sCacheFile[MAX_PATH] = {0};
	V_ComposeFileName( g_pVPC->GetSourcePath(), "vpc.cache", sCacheFile, sizeof( sCacheFile ) );
	if ( m_bFullDependencySet )
	{
		if ( !LoadCache( sCacheFile ) )
		{
			Log_Msg( LOG_VPC, "\n\nNo vpc.cache file found.\nThis will take a minute to generate dependency info from all the sources.\nPut the kleenex down.\nNext time it will have a cache file and be fast.\n\n" );
		}
	}

	CFastTimer timer;
	timer.Start();
	g_pVPC->IterateTargetProjects( projectList, this );
	timer.End();

	ResolveAdditionalProjectDependencies( pPhase1Projects );

	// Restore the old game defines state?
	if ( nBuildProjectDepsFlags & BUILDPROJDEPS_CHECK_ALL_PROJECTS )
	{
		for ( int i=0; i < oldState.Count(); i++ )
		{
			int iDefine = oldState[i] >> 16;
			g_pVPC->m_Conditionals[iDefine].m_bDefined = ( (oldState[i] & 1) != 0 );
		}
	}		

	// Save the expensive work we did into a cache file so it can be used next time.
	if ( m_bFullDependencySet )
	{
		SaveCache( sCacheFile );
	}

	Log_Msg( LOG_VPC, "\n\n" );
	if ( m_nFilesParsedForIncludes > 0 )
	{
		Log_Msg( LOG_VPC, "%d files parsed in %.2f seconds for #includes.\n", m_nFilesParsedForIncludes, timer.GetDuration().GetSeconds() );
	}

	m_bHasGeneratedDependencies = true;
}

void CProjectDependencyGraph::ResolveAdditionalProjectDependencies( CUtlVector< CDependency_Project *> *pPhase1Projects )
{
	for ( int iMainProject=0; iMainProject < m_Projects.Count(); iMainProject++ )
	{
		CDependency_Project *pMainProject = m_Projects[iMainProject];

		for ( int i=0; i < pMainProject->m_AdditionalProjectDependencies.Count(); i++ )
		{
			const char *pLookingFor = pMainProject->m_AdditionalProjectDependencies[i].String();

			// Look for this project name among all the projects.
			int j;
			for ( j=0; j < m_Projects.Count(); j++ )
			{
				if ( V_stricmp( m_Projects[j]->m_ProjectName.String(), pLookingFor ) == 0 )
					break;
			}

			if ( j == m_Projects.Count() )
			{
				//VPCError( "Project %s lists '%s' in its $AdditionalProjectDependencies, but there is no project by that name.", pMainProject->GetName(), pLookingFor );
				continue;
			}

			if ( pMainProject->m_AdditionalDependencies.Find( m_Projects[j] ) == pMainProject->m_AdditionalDependencies.InvalidIndex() )
				pMainProject->m_AdditionalDependencies.AddToTail( m_Projects[j] );
		}


		if ( pPhase1Projects != NULL )
		{
			//
			// See if there's a project in phase 1 built from the same VPC, and, if so, add it as a dependency.
			// This prevents a bunch of race conditions when projects are built in a distributed fashion
			// (ala Incredibuild) and the projects step on each other.
			//
			const char *pFileName = pMainProject->m_Filename.String();

			int j;
			for ( j = 0; j < pPhase1Projects->Count(); j++ )
			{
				if ( V_stricmp( (*pPhase1Projects)[j]->m_Filename.String(), pFileName ) == 0)
				{
					break;
				}
			}
			if ( j == pPhase1Projects->Count() )
			{
				continue;
			}

			if ( pMainProject->m_AdditionalDependencies.Find( (*pPhase1Projects)[j] ) == pMainProject->m_AdditionalDependencies.InvalidIndex() )
			{
				pMainProject->m_AdditionalDependencies.AddToTail( (*pPhase1Projects)[j] );
			}
		}
	}
}

bool CProjectDependencyGraph::HasGeneratedDependencies() const
{
	return m_bHasGeneratedDependencies;
}

bool CProjectDependencyGraph::VisitProject( projectIndex_t iProject, const char *szProjectName )
{
	// Read in the project.
	if ( !Sys_Exists( szProjectName ) )
	{
		return false;
	}

	// Add another dot for the pacifier.
	Log_Msg( LOG_VPC, "." );

	// Add this project.
	CDependency_Project *pProject = new CDependency_Project( this );
	
	char szAbsolute[MAX_PATH];
	V_MakeAbsolutePath( szAbsolute, sizeof( szAbsolute ), szProjectName );
	pProject->m_Filename = szAbsolute;

	pProject->m_Type = k_eDependencyType_Project;
	pProject->m_iProjectIndex = iProject;
	m_Projects.AddToTail( pProject );
	m_AllFiles.Insert( szAbsolute, pProject );

	// Remember various parameters passed to us so we can regenerate this project without having 
	// to call VPC_IterateTargetProjects.
	pProject->StoreProjectParameters( szProjectName );

	char sAbsProjectFilename[MAX_PATH];
	V_MakeAbsolutePath( sAbsProjectFilename, sizeof( sAbsProjectFilename ), g_pVPC->GetOutputFilename() );
	pProject->m_ProjectFilename = sAbsProjectFilename;

	// Scan the project file and get all its libs, cpp, and h files.
	CSingleProjectScanner scanner;
	scanner.ScanProjectFile( this, szAbsolute, pProject );
	pProject->m_IncludeDirectories = scanner.m_IncludeDirectories;
	pProject->m_ProjectName = scanner.m_ProjectName;

	// Get a list of all files that depend on this project, starting with the .lib if it generates one.
	CUtlVector<CUtlString> outputFiles;
	outputFiles = pProject->m_AdditionalOutputFiles;

	// Now note that the import library depends on this project.
	// $(ImportLibrary) will be a lib in the case of DLLs that create libs (like tier0).
	// $(OutputFile) will be a lib in the case of static libs (like tier1).
	const char *pLinkerOutputFile = scanner.m_LinkerOutputFile.String();
	const char *pImportLibrary = scanner.m_ImportLibrary.String();
	if ( !IsLibraryFile( pImportLibrary ) )
	{
		pImportLibrary = pLinkerOutputFile;
	}

	if ( IsLibraryFile( pImportLibrary ) )
	{
		outputFiles.AddToTail( pImportLibrary );
	}

	// The string that we replace $(TargetName) with is the output project filename without the path or extension.
	// That'll be something like "tier0_360".
	char sTargetNameReplacement[MAX_PATH];
	V_FileBase( pLinkerOutputFile, sTargetNameReplacement, sizeof( sTargetNameReplacement ) );
	
	// Now add a CDependency for each file.
	for ( int i=0; i < outputFiles.Count(); i++ )
	{
		const char *pFilename = outputFiles[i].String();
	
		// Replace $(TargetName) and fixup the path.
		char sReplaced[MAX_PATH], sAbsImportLibrary[MAX_PATH];
		V_StrSubst( pFilename, "$(TargetName)", sTargetNameReplacement, sReplaced, sizeof( sReplaced ) );
		V_MakeAbsolutePath( sAbsImportLibrary, sizeof( sAbsImportLibrary ), sReplaced );

		CDependency *pImportLibrary = FindOrCreateDependency( sAbsImportLibrary );
		pImportLibrary->m_Dependencies.AddToTail( pProject );
	}

	return true;
}


void CProjectDependencyGraph::GetProjectDependencyTree( projectIndex_t iProject, CUtlVector<projectIndex_t> &dependentProjects, bool bDownwards )
{
	// First add the project itself.
	if ( dependentProjects.Find( iProject ) == dependentProjects.InvalidIndex() )
		dependentProjects.AddToTail( iProject );

	// Now add anything that depends on it.
	for ( int i=0; i < m_Projects.Count(); i++)
	{
		CDependency_Project *pProject = m_Projects[i];
	
		if ( pProject->m_iProjectIndex != iProject )
			continue;

		// Ok, this project/game/platform combo comes from iProject. Now find anything that depends on it.
		for ( int iOther=0; iOther < m_Projects.Count(); iOther++ )
		{
			CDependency_Project *pOther = m_Projects[iOther];

			if ( pOther->m_iProjectIndex == iProject )
				continue;

			bool bThereIsADependency;
			if ( bDownwards )
				bThereIsADependency = pProject->DependsOn( pOther, k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagCheckAdditionalDependencies | k_EDependsOnFlagRecurse | k_EDependsOnFlagTraversePastLibs );
			else
				bThereIsADependency = pOther->DependsOn( pProject, k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagCheckAdditionalDependencies | k_EDependsOnFlagRecurse | k_EDependsOnFlagTraversePastLibs );

			if ( bThereIsADependency )
			{
				if ( dependentProjects.Find( pOther->m_iProjectIndex ) == dependentProjects.InvalidIndex() )
					dependentProjects.AddToTail( pOther->m_iProjectIndex );
			}
		}
	}
}


CDependency* CProjectDependencyGraph::FindDependency( const char *pFilename )
{
	int i = m_AllFiles.Find( pFilename );
	if ( i == m_AllFiles.InvalidIndex() )
		return NULL;
	else
		return m_AllFiles[i];
}


CDependency* CProjectDependencyGraph::FindOrCreateDependency( const char *pFilename )
{
	// Fix up stuff like blah/../blah
	char sFixed[MAX_PATH];

	V_FixupPathName( sFixed, sizeof( sFixed ), pFilename );
	pFilename = sFixed;

	CDependency *pDependency = FindDependency( pFilename );
	if ( pDependency )
		return pDependency;

	// Couldn't find it. Create one.
	pDependency = new CDependency( this );
	pDependency->m_Filename = pFilename;
	m_AllFiles.Insert( pFilename, pDependency );

	Sys_FileInfo( pFilename, pDependency->m_nCacheFileSize, pDependency->m_nCacheModificationTime );

	if ( IsSourceFile( pFilename ) )
		pDependency->m_Type = k_eDependencyType_SourceFile;
	else if ( IsLibraryFile( pFilename ) )
		pDependency->m_Type = k_eDependencyType_Library;
	else
		pDependency->m_Type = k_eDependencyType_Unknown;

	return pDependency;
}


void CProjectDependencyGraph::ClearAllDependencyMarks()
{
	if ( m_iDependencyMark == 0xFFFFFFFF )
	{
		m_iDependencyMark = 1;
		for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=m_AllFiles.Next(i) )
		{
			m_AllFiles[i]->m_iDependencyMark = 0;
		}
	}
	else
	{
		// The 99.9999999% chance case.
		++m_iDependencyMark;
	}
}

bool CProjectDependencyGraph::LoadCache( const char *pFilename )
{
	FILE *fp = fopen( pFilename, "rb" );
	if ( !fp )
		return false;

	int version;
	fread( &version, sizeof( version ), 1, fp );
	if ( version != VPC_CRC_CACHE_VERSION )
	{
		g_pVPC->VPCWarning( "Invalid dependency cache file version in %s.", pFilename );
		return false;
	}

	while ( 1 )
	{
		byte bMore;
		if ( fread( &bMore, 1, 1, fp ) != 1 || bMore == 0 )
			break;
	
		CUtlString filename = ReadString( fp );
		CDependency *pDep = FindOrCreateDependency( filename.String() );
		if ( pDep->m_Dependencies.Count() != 0 )
			g_pVPC->VPCError( "Cache loading dependency %s but it already exists!", filename.String() );

		fread( &pDep->m_nCacheFileSize, sizeof( pDep->m_nCacheFileSize ), 1, fp );
		fread( &pDep->m_nCacheModificationTime, sizeof( pDep->m_nCacheModificationTime ), 1, fp );

		int nDependencies;
		fread( &nDependencies, sizeof( nDependencies ), 1, fp );
		pDep->m_Dependencies.SetSize( nDependencies );

		for ( int iDependency=0; iDependency < nDependencies; iDependency++ )
		{
			CUtlString childDepName = ReadString( fp );
			CDependency *pChildDep = FindOrCreateDependency( childDepName.String() );
			pDep->m_Dependencies[iDependency] = pChildDep;
		}
	}

	fclose( fp );

	int nOriginalEntries = m_AllFiles.Count();

	CheckCacheEntries();
	RemoveDirtyCacheEntries();
	MarkAllCacheEntriesValid();

	Log_Msg( LOG_VPC, "\n\nLoaded %d valid dependency cache entries (%d were out of date).\n\n", m_AllFiles.Count(), nOriginalEntries-m_AllFiles.Count() );
	return true;
}

bool CProjectDependencyGraph::SaveCache( const char *pFilename )
{
	FILE *fp = fopen( pFilename, "wb" );
	if ( !fp )
		return false;

	// Write the version.
	int version = VPC_CRC_CACHE_VERSION;
	fwrite( &version, sizeof( version ), 1, fp );

	// Write each file.
	for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=m_AllFiles.Next( i ) )
	{
		CDependency *pDep = m_AllFiles[i];
		
		// We only care about source files.
		if ( pDep->m_Type != k_eDependencyType_SourceFile )
			continue;

		// Write that there's a file here.
		byte bYesThereIsAFileHere = 1;
		fwrite( &bYesThereIsAFileHere, 1, 1, fp );

		WriteString( fp, pDep->m_Filename );
		fwrite( &pDep->m_nCacheFileSize, sizeof( pDep->m_nCacheFileSize ), 1, fp );
		fwrite( &pDep->m_nCacheModificationTime, sizeof( pDep->m_nCacheModificationTime ), 1, fp );

		int nDependencies = pDep->m_Dependencies.Count();
		fwrite( &nDependencies, sizeof( nDependencies ), 1, fp );

		for ( int iDependency=0; iDependency < pDep->m_Dependencies.Count(); iDependency++ )
		{
			WriteString( fp, pDep->m_Dependencies[iDependency]->m_Filename );
		}
	}

	// Write a terminator.
	byte bNoMore = 0;
	fwrite( &bNoMore, 1, 1, fp );

	fclose( fp );

	Sys_CopyToMirror( pFilename );

	return true;
}

void CProjectDependencyGraph::WriteString( FILE *fp, CUtlString &utlString )
{
	const char *pStr = utlString.String();
	int len = V_strlen( pStr );
	fwrite( &len, sizeof( len ), 1, fp );
	fwrite( pStr, len, 1, fp );
}

CUtlString CProjectDependencyGraph::ReadString( FILE *fp )
{
	int len;
	fread( &len, sizeof( len ), 1, fp );

	char *pTemp = new char[len+1];
	fread( pTemp, len, 1, fp );
	pTemp[len] = 0;

	CUtlString ret = pTemp;
	delete [] pTemp;

	return ret;
}


void CProjectDependencyGraph::CheckCacheEntries()
{
	for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=m_AllFiles.Next( i ) )
	{
		CDependency *pDep = m_AllFiles[i];
		pDep->m_bCacheDirty = false;

		if ( pDep->m_Type != k_eDependencyType_SourceFile )
			continue;
	
		int64 fileSize, modTime;
		if ( !Sys_FileInfo( pDep->m_Filename.String(), fileSize, modTime ) ||
		     pDep->m_nCacheFileSize != fileSize ||
			 pDep->m_nCacheModificationTime != modTime )
		{
			pDep->m_bCacheDirty = true;
		}
	}
}

void CProjectDependencyGraph::RemoveDirtyCacheEntries()
{
	// NOTE: This could be waaaay more efficient by pointing files at their parents and removing all the way
	// up the chain rather than iterating over and over but this keeps the data structures simple.
	bool bAnyDirty = true;
	while ( bAnyDirty )
	{
		bAnyDirty = false;

		for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=m_AllFiles.Next( i ) )
		{
			CDependency *pDep = m_AllFiles[i];
			if ( pDep->m_bCacheDirty )
				continue;

			// If any of its children are dirty, then mark this guy as dirty and make sure to remove the child.
			for ( int iChild=0; iChild < pDep->m_Dependencies.Count(); iChild++ )
			{
				CDependency *pChild = pDep->m_Dependencies[iChild];
				if ( pChild->m_bCacheDirty )
				{
					pDep->m_bCacheDirty = true;
					bAnyDirty = true;
				}
			}
		}
	}
	
	// Now that any dirty children have flagged their parents as dirty, we can remove them.
	int iNext;
	for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=iNext )
	{
		iNext = m_AllFiles.Next( i );

		if ( m_AllFiles[i]->m_bCacheDirty )
		{
			delete m_AllFiles[ i ];
			m_AllFiles.RemoveAt( i );
		}
	}
}


void CProjectDependencyGraph::MarkAllCacheEntriesValid()
{
	for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=m_AllFiles.Next( i ) )
	{
		CDependency *pDep = m_AllFiles[i];
		pDep->m_bCheckedIncludes = true;
	}
}



// This is called by VPC_IterateTargetProjects and all it does is look forf a 
class CGameFilterProjectIterator : public IProjectIterator
{
public:
	virtual bool VisitProject( projectIndex_t iProject, const char *szProjectName )
	{
		char szAbsolute[MAX_PATH];
		V_MakeAbsolutePath( szAbsolute, sizeof( szAbsolute ), szProjectName );
		
		// Ok, we've got an (absolute) project filename. Search all the projects for one with that name.
		bool bAdded = false;
		for ( int i=0; i < m_pAllProjectsList->Count(); i++ )
		{
			CDependency_Project *pProject = m_pAllProjectsList->Element( i );

			if ( pProject->CompareAbsoluteFilename( szAbsolute ) )
			{
				m_pOutProjectsList->AddToTail( pProject );
				bAdded = true;
				break;
			}
		}

		if ( !bAdded )
		{
			g_pVPC->VPCError( "CGameFilterProjectIterator::VisitProject( %s ) - no project found by that name.", szProjectName );
			return false;
		}

		return true;
	}

public:
	const CUtlVector<CDependency_Project*> *m_pAllProjectsList;
	CUtlVector<CDependency_Project*> *m_pOutProjectsList;
};

void CProjectDependencyGraph::TranslateProjectIndicesToDependencyProjects( CUtlVector<projectIndex_t> &projectList, CUtlVector<CDependency_Project*> &out )
{
	CGameFilterProjectIterator iterator;
	iterator.m_pAllProjectsList = &m_Projects;
	iterator.m_pOutProjectsList = &out;

	g_pVPC->IterateTargetProjects( projectList, &iterator );
}
