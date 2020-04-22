//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "vpc.h"
#include "dependencies.h"
#include "utlgraph.h"

class CSolutionGenerator_CodeLite : public IBaseSolutionGenerator {
public:
	CSolutionGenerator_CodeLite() {
		m_nIndent = 0;
		m_fp = NULL;
	}

	virtual void GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects ) {
		char szSolutionName[MAX_PATH];
		V_FileBase( pSolutionFilename, szSolutionName, MAX_PATH );

		char szSolutionFileBaseName[MAX_PATH];

		// Default extension.
		char szTmpSolutionFilename[MAX_PATH];
		if ( !V_GetFileExtension( pSolutionFilename ) ) {
			V_strncpy( szSolutionFileBaseName, pSolutionFilename, sizeof( szSolutionFileBaseName ) );
			V_snprintf( szTmpSolutionFilename, sizeof( szTmpSolutionFilename ), "%s.workspace", pSolutionFilename );
			pSolutionFilename = szTmpSolutionFilename;
		} else {
			V_StripExtension( pSolutionFilename, szSolutionFileBaseName, sizeof( szSolutionFileBaseName ) );
		}

		Msg( "\nWriting CodeLite workspace %s.\n\n", pSolutionFilename );

		// Write the file.
		m_fp = fopen( pSolutionFilename, "wt" );
		if ( !m_fp )
			g_pVPC->VPCError( "Can't open %s for writing.", pSolutionFilename );

		Write( "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" );
		Write( "<CodeLite_Workspace Name=\"%s\" Database=\"%s.tags\">\n", szSolutionName, szSolutionFileBaseName );

		++m_nIndent;
		Write( "<Project Name=\"all\" Path=\"%s.project\" Active=\"Yes\"/>\n", szSolutionFileBaseName);
		for ( int i=0; i < projects.Count(); i++ ) {
			CDependency_Project *pCurProject = projects[i];
			project_t *pProjectT = &g_pVPC->m_Projects[ pCurProject->m_iProjectIndex ];

			char szProjectFileBaseName[MAX_PATH];
			V_StripExtension( pCurProject->m_ProjectFilename.String(), szProjectFileBaseName, sizeof( szProjectFileBaseName ) );

			Write( "<Project Name=\"%s\" Path=\"%s.project\"/>\n", pProjectT->name.String(), szProjectFileBaseName );
		}

		Write( "<BuildMatrix>\n" );
		++m_nIndent;
		Write( "<WorkspaceConfiguration Name=\"Debug\" Selected=\"no\">\n" );
		++m_nIndent;
		Write( "<Project Name=\"all\" ConfigName=\"Debug\"/>\n" );
		for ( int i=0; i < projects.Count(); i++ ) {
			CDependency_Project *pCurProject = projects[i];
			project_t *pProjectT = &g_pVPC->m_Projects[ pCurProject->m_iProjectIndex ];

			Write( "<Project Name=\"%s\" ConfigName=\"Debug\"/>\n", pProjectT->name.String() );
		}
		--m_nIndent;
		Write( "</WorkspaceConfiguration>\n" );

		Write( "<WorkspaceConfiguration Name=\"Release\" Selected=\"yes\">\n" );
		++m_nIndent;
		Write( "<Project Name=\"all\" ConfigName=\"Release\"/>\n" );
		for ( int i=0; i < projects.Count(); i++ ) {
			CDependency_Project *pCurProject = projects[i];
			project_t *pProjectT = &g_pVPC->m_Projects[ pCurProject->m_iProjectIndex ];

			Write( "<Project Name=\"%s\" ConfigName=\"Release\"/>\n", pProjectT->name.String() );
		}
		--m_nIndent;
		Write( "</WorkspaceConfiguration>\n" );
		--m_nIndent;
		Write( "</BuildMatrix>\n" );
		--m_nIndent;
		Write( "</CodeLite_Workspace>\n" );

		fclose( m_fp );

		WriteBuildOrderProject( szSolutionFileBaseName, projects );
	}

	void WriteBuildOrderProject( const char *pszSolutionFileBaseName, CUtlVector<CDependency_Project*> &projects ) {
		// write out a project with no files to encode the build order (dependencies)
		char szProjectFileName[MAX_PATH];
		V_snprintf( szProjectFileName, sizeof( szProjectFileName ), "%s.project", pszSolutionFileBaseName );

		m_nIndent = 0;
		m_fp = fopen( szProjectFileName, "wt" );
		if ( !m_fp )
			g_pVPC->VPCError( "Can't open %s for writing.", szProjectFileName );

		Write( "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" );
		Write( "<CodeLite_Project Name=\"all\" InternalType=\"\">\n" );
		{
			++m_nIndent;
			Write( "<Description/>\n" );
			Write( "<Dependencies/>\n" );
			Write( "<Settings Type=\"Static Library\">\n" );
			{
				++m_nIndent;
				Write( "<GlobalSettings>\n" );
				++m_nIndent;
				Write( "<Compiler Options=\"\" C_Options=\"\">\n" );
				++m_nIndent;
				Write( "<IncludePath Value=\"\"/>\n" );
				--m_nIndent;
				Write( "</Compiler>\n" );
				Write( "<Linker Options=\"\">\n" );
				++m_nIndent;
				Write( "<LibraryPath Value=\"\"/>\n" );
				--m_nIndent;
				Write( "</Linker>\n" );
				Write( "<ResourceCompiler Options=\"\"/>\n" );
				--m_nIndent;
				Write( "</GlobalSettings>\n" );

				Write( "<Configuration Name=\"Debug\" CompilerType=\"gnu g++\" DebuggerType=\"GNU gdb debugger\" Type=\"Executable\" BuildCmpWithGlobalSettings=\"append\" BuildLnkWithGlobalSettings=\"append\" BuildResWithGlobalSettings=\"append\">\n" );
#if 0
				++m_nIndent;
				Write( "<Compiler Options=\"-g\" C_Options="" Required="yes" PreCompiledHeader="">\n" );
				++m_nIndent;
				Write( "<IncludePath Value=\"\"/>" );
				--m_nIndent;
				--m_nIndent;
#endif
				Write( "</Configuration>\n" );

				Write( "<Configuration Name=\"Release\" CompilerType=\"gnu g++\" DebuggerType=\"GNU gdb debugger\" Type=\"Executable\" BuildCmpWithGlobalSettings=\"append\" BuildLnkWithGlobalSettings=\"append\" BuildResWithGlobalSettings=\"append\">\n" );
#if 0
				++m_nIndent;
				Write( "<Compiler Options=\"-g\" C_Options="" Required="yes" PreCompiledHeader="">\n" );
				++m_nIndent;
				Write( "<IncludePath Value=\"\"/>" );
				--m_nIndent;
				--m_nIndent;
#endif
				Write( "</Configuration>\n" );

				--m_nIndent;
			}
			Write( "</Settings>\n" );

			--m_nIndent;

			CUtlGraph<int, int> dependencyGraph;

			// walk the project list building a dependency graph 
			for ( int i=0; i < projects.Count(); i++ ) 
			{

				CDependency_Project *pCurProject = projects[i];

				CUtlVector<CDependency_Project*> additionalProjectDependencies;
				ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );

				//project_t *pProjectT = &g_projects[ pCurProject->m_iProjectIndex ];
				//printf( "%s depends on\n", pProjectT->name.String() );

				for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ ) {
					if ( i == iTestProject )
						continue;

					// do I depend on anyone?
					CDependency_Project *pTestProject = projects[iTestProject];
					int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
					if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() ) {
						// add an edge from this project to the one it depends on
						dependencyGraph.AddEdge( i, iTestProject, 1 );
						//printf( " %s -> %s\n", projects[ i ]->m_ProjectName.String(),
						//					   projects[ iTestProject ]->m_ProjectName.String() );

					}
				}
			}

			Write( "<Dependencies Name=\"Debug\">\n" );
			++m_nIndent;

			CUtlVector<int> visitedList;
			for( int i = 0; i < projects.Count(); i++ ) {
				TraverseFrom( projects, dependencyGraph, visitedList, i );
			}
			--m_nIndent;
			Write( "</Dependencies>\n" );

			Write( "<Dependencies Name=\"Release\">\n" );
			++m_nIndent;
			visitedList.Purge();
			for( int i = 0; i < projects.Count(); i++ ) {
				TraverseFrom( projects, dependencyGraph, visitedList, i );
			}
			--m_nIndent;
			Write( "</Dependencies>\n" );

		}
		Write( "</CodeLite_Project>\n" );
		fclose( m_fp );
	}

	void TraverseFrom( CUtlVector<CDependency_Project*> &projects, CUtlGraph<int, int> &dependencyGraph, CUtlVector<int> &visitedList, int root ) {
		CUtlGraphVisitor<int> visitor( dependencyGraph );

		if ( visitedList.Find( root ) != visitedList.InvalidIndex() )
			return;

		// this project has no dependencies, just emit it
		if ( !visitor.Begin( root ) )
		{
			Write( "<Project Name=\"%s\"/>\n", projects[ root ]->m_ProjectName.String() );
			visitedList.AddToTail( root );
			return;
		}

		// printf( "considering %i (%s)\n", root, projects[ root ]->m_ProjectName.String() );

		while ( visitor.Advance() ) {
			// printf( "%i (%s) depends on %i (%s)\n", root, projects[ root ]->m_ProjectName.String(), visitor.CurrentNode(), projects[ visitor.CurrentNode() ]->m_ProjectName.String() );
			TraverseFrom( projects, dependencyGraph, visitedList, visitor.CurrentNode() );
		}

		Write( "<Project Name=\"%s\"/>\n", projects[ root ]->m_ProjectName.String() );
		visitedList.AddToTail( root );
		// printf( "emitting %i (%s)\n", root, projects[ root ]->m_ProjectName.String() );
	}

	void ResolveAdditionalProjectDependencies(
	    CDependency_Project *pCurProject,
	    CUtlVector<CDependency_Project*> &projects,
	    CUtlVector<CDependency_Project*> &additionalProjectDependencies ) {
		for ( int i=0; i < pCurProject->m_AdditionalProjectDependencies.Count(); i++ ) {
			const char *pLookingFor = pCurProject->m_AdditionalProjectDependencies[i].String();

			int j;
			for ( j=0; j < projects.Count(); j++ ) {
				if ( V_stricmp( projects[j]->m_ProjectName.String(), pLookingFor ) == 0 )
					break;
			}

			if ( j == projects.Count() )
				g_pVPC->VPCError( "Project %s lists '%s' in its $AdditionalProjectDependencies, but there is no project by that name in the selected projects.", pCurProject->GetName(), pLookingFor );

			additionalProjectDependencies.AddToTail( projects[j] );
		}
	}

	const char* FindInFile( const char *pFilename, const char *pFileData, const char *pSearchFor ) {
		const char *pPos = V_stristr( pFileData, pSearchFor );
		if ( !pPos )
			g_pVPC->VPCError( "Can't find ProjectGUID in %s.", pFilename );

		return pPos + V_strlen( pSearchFor );
	}

	void Write( const char *pMsg, ... ) {
		char sOut[8192];

		va_list marker;
		va_start( marker, pMsg );
		V_vsnprintf( sOut, sizeof( sOut ), pMsg, marker );
		va_end( marker );

		for ( int i=0; i < m_nIndent; i++ )
			fprintf( m_fp, "  " );

		fprintf( m_fp, "%s", sOut );
	}

	FILE *m_fp;
	int m_nIndent;
};


static CSolutionGenerator_CodeLite g_SolutionGenerator_CodeLite;
IBaseSolutionGenerator* GetSolutionGenerator_CodeLite() {
	return &g_SolutionGenerator_CodeLite;
}
