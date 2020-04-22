//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "vpc.h"
#include "dependencies.h"
#include "tier1/checksum_md5.h"

class CVCProjInfo
{
public:
	CUtlString m_ProjectName;
	CUtlString m_ProjectGUID;
};


struct RegStartPoint
{
	HKEY baseKey;
	const char* const baseDir;
};

class CSolutionGenerator_Win32 : public IBaseSolutionGenerator
{
public:
	void GetVCPROJSolutionGUID( char (&szSolutionGUID)[256] )
	{
		HKEY hKey;
		int firstVer = 8;
		const int lastVer = 14; // Handle up to VS 14, AKA VS 2015
		if ( g_pVPC->Is2010() )
		{
			firstVer = 10;
		}
		else if ( g_pVPC->BUse2008() )
		{
			firstVer = 9;
		}
		for ( int vsVer = firstVer; vsVer <= lastVer; ++vsVer )
		{
			// Handle both VisualStudio and VCExpress (used by some SourceSDK customers)
			RegStartPoint searchPoints[] =
			{
				{ HKEY_LOCAL_MACHINE, "Software\\Microsoft\\VisualStudio\\%d.0\\Projects" }, // Visual Studio Professional
				{ HKEY_LOCAL_MACHINE, "Software\\Microsoft\\VCExpress\\%d.0\\Projects" }, // VC Express 2010 and 2012
				{ HKEY_CURRENT_USER, "Software\\Microsoft\\WDExpress\\%d.0_Config\\Projects" }, // WinDev Express -- VS Express starting with VS 2013
			};
			for ( int j = 0; j < ARRAYSIZE(searchPoints); ++j )
			{
				RegStartPoint& searchPoint = searchPoints[ j ];
				char pRegKeyName[1000];
				V_snprintf( pRegKeyName, ARRAYSIZE(pRegKeyName), searchPoint.baseDir, vsVer );
				LONG ret = RegOpenKeyEx( searchPoint.baseKey, pRegKeyName, 0, KEY_READ, &hKey );
				//if ( ret != ERROR_SUCCESS )
				//	g_pVPC->VPCError( "Unable to open registry key %s.", pRegKeyName );

				for ( int i=0; i < 200; i++ )
				{
					char szKeyName[MAX_PATH];	
					DWORD dwKeyNameSize = sizeof( szKeyName );
					ret = RegEnumKeyEx( hKey, i, szKeyName, &dwKeyNameSize, NULL, NULL, NULL, NULL );
					if ( ret == ERROR_NO_MORE_ITEMS )
						break;
			
					HKEY hSubKey;	
					LONG ret = RegOpenKeyEx( hKey, szKeyName, 0, KEY_READ, &hSubKey );
					if ( ret == ERROR_SUCCESS )
					{
						DWORD dwType;
						char ext[MAX_PATH];
						DWORD dwExtLen = sizeof( ext );
						ret = RegQueryValueEx( hSubKey, "DefaultProjectExtension", NULL, &dwType, (BYTE*)ext, &dwExtLen );
						RegCloseKey( hSubKey );

						// VS 2012 and beyond has the DefaultProjectExtension as vcxproj instead of vcproj
						if ( ret == ERROR_SUCCESS && dwType == REG_SZ && ( V_stricmp( ext, "vcproj" ) == 0 || V_stricmp( ext, "vcxproj" ) == 0 ) )
						{
							V_strncpy( szSolutionGUID, szKeyName, ARRAYSIZE(szSolutionGUID) );
							RegCloseKey( hKey );
							return;
						}
					}
				}

				RegCloseKey( hKey );
			}
		}
		g_pVPC->VPCError( "Unable to find RegKey for .vcproj or .vcxproj files in solutions." );
	}

	virtual void GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects )
	{
		// Default extension.
		char szTmpSolutionFilename[MAX_PATH];
		if ( !V_GetFileExtension( pSolutionFilename ) )
		{
			V_snprintf( szTmpSolutionFilename, sizeof( szTmpSolutionFilename ), "%s.sln", pSolutionFilename );
			pSolutionFilename = szTmpSolutionFilename;
		}

		Msg( "\nWriting solution file %s.\n\n", pSolutionFilename );

		char szSolutionGUID[256];
		GetVCPROJSolutionGUID( szSolutionGUID );

		CUtlVector<CVCProjInfo> vcprojInfos;
		GetProjectInfos( projects, vcprojInfos );

		// Write the file.
		FILE *fp = fopen( pSolutionFilename, "wt" );
		if ( !fp )
			g_pVPC->VPCError( "Can't open %s for writing.", pSolutionFilename );

		
		if ( g_pVPC->Is2015() )
		{
			fprintf( fp, "\xef\xbb\xbf\nMicrosoft Visual Studio Solution File, Format Version 12.00\n" ); // still on 12
			fprintf( fp, "# Visual Studio 2015\n" );
		}
		else if ( g_pVPC->Is2013() )
		{
			fprintf( fp, "\xef\xbb\xbf\nMicrosoft Visual Studio Solution File, Format Version 12.00\n" ); // Format didn't change from VS 2012 to VS 2013
			fprintf( fp, "# Visual Studio 2013\n" );
		}
		else if ( g_pVPC->Is2012() )
		{
			fprintf( fp, "\xef\xbb\xbf\nMicrosoft Visual Studio Solution File, Format Version 12.00\n" );
			fprintf( fp, "# Visual Studio 2012\n" );
		}
		else if ( g_pVPC->Is2010() )
		{
			fprintf( fp, "\xef\xbb\xbf\nMicrosoft Visual Studio Solution File, Format Version 11.00\n" );
			fprintf( fp, "# Visual Studio 2010\n" );
		}
		else if ( g_pVPC->BUse2008() )
		{
			fprintf( fp, "\xef\xbb\xbf\nMicrosoft Visual Studio Solution File, Format Version 10.00\n" );
			fprintf( fp, "# Visual Studio 2008\n" );
		}
		else
		{
			fprintf( fp, "\xef\xbb\xbf\nMicrosoft Visual Studio Solution File, Format Version 9.00\n" );
			fprintf( fp, "# Visual Studio 2005\n" );
		}
		fprintf( fp, "#\n" );
		fprintf( fp, "# Automatically generated solution:\n" );
		fprintf( fp, "# devtools\\bin\\vpc " );
		for ( int k = 1; k < __argc; ++ k )
			fprintf( fp, "%s ", __argv[k] );
		fprintf( fp, "\n" );
		fprintf( fp, "#\n" );
		fprintf( fp, "#\n" );

		if ( !g_pVPC->Is2010() )
		{
			// if /slnItems <filename> is passed on the command line, build a Solution Items project
			const char *pSolutionItemsFilename = g_pVPC->GetSolutionItemsFilename();
			if ( pSolutionItemsFilename[0] != '\0' )
			{
				fprintf( fp, "Project(\"{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = \"Solution Items\", \"Solution Items\", \"{AAAAAAAA-8B4A-11D0-8D11-90A07D6D6F7D}\"\n" );
				fprintf( fp, "\tProjectSection(SolutionItems) = preProject\n" );
				WriteSolutionItems( fp );
				fprintf( fp, "\tEndProjectSection\n" );
				fprintf( fp, "EndProject\n" );
			}
		}

		for ( int i=0; i < projects.Count(); i++ )
		{
			CDependency_Project *pCurProject = projects[i];
			CVCProjInfo *pProjInfo = &vcprojInfos[i];

			// Get a relative filename for the vcproj file.
			const char *pFullProjectFilename = pCurProject->m_ProjectFilename.String();
			char szRelativeFilename[MAX_PATH];
			if ( !V_MakeRelativePath( pFullProjectFilename, g_pVPC->GetSourcePath(), szRelativeFilename, sizeof( szRelativeFilename ) ) )
				g_pVPC->VPCError( "Can't make a relative path (to the base source directory) for %s.", pFullProjectFilename );

			fprintf( fp, "Project(\"%s\") = \"%s\", \"%s\", \"{%s}\"\n", szSolutionGUID, pProjInfo->m_ProjectName.String(), szRelativeFilename, pProjInfo->m_ProjectGUID.String() );
			bool bHasDependencies = false;

			for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
			{
				if ( i == iTestProject )
					continue;

				CDependency_Project *pTestProject = projects[iTestProject];
				if ( pCurProject->DependsOn( pTestProject, k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagRecurse ) ||
					pCurProject->DependsOn( pTestProject, k_EDependsOnFlagCheckAdditionalDependencies | k_EDependsOnFlagTraversePastLibs ) )
				{
					if ( !bHasDependencies )
					{
						fprintf( fp, "\tProjectSection(ProjectDependencies) = postProject\n" );
						bHasDependencies = true;
					}
					fprintf( fp, "\t\t{%s} = {%s}\n", vcprojInfos[iTestProject].m_ProjectGUID.String(), vcprojInfos[iTestProject].m_ProjectGUID.String() );
				}
			}
			if ( bHasDependencies )
				fprintf( fp, "\tEndProjectSection\n" );

			fprintf( fp, "EndProject\n" );
		}

		fclose( fp );
		Sys_CopyToMirror( pSolutionFilename );
	}
	
	const char* FindInFile( const char *pFilename, const char *pFileData, const char *pSearchFor )
	{
		const char *pPos = V_stristr( pFileData, pSearchFor );
		if ( !pPos )
		{
			g_pVPC->VPCError( "Can't find %s in %s.", pSearchFor, pFilename );
		}

		return pPos + V_strlen( pSearchFor );
	}

	void GetProjectInfos( CUtlVector<CDependency_Project*> &projects, CUtlVector<CVCProjInfo> &vcprojInfos )
	{
		for ( int i=0; i < projects.Count(); i++ )
		{
			CDependency_Project *pCurProject = projects[i];
			const char *pFilename = pCurProject->m_ProjectFilename.String();

			CVCProjInfo vcprojInfo;
			
			char *pFileData;
			int nResult = Sys_LoadFile( pFilename, (void**)&pFileData, false );
			if ( nResult == -1 )
				g_pVPC->VPCError( "Can't open %s to get ProjectGUID.", pFilename );

			const char *pSearchFor;
			if ( g_pVPC->Is2010() )
			{
				pSearchFor = "<ProjectGuid>{";
			}
			else
			{
				pSearchFor = "ProjectGUID=\"{";
			}

			const char *pPos = FindInFile( pFilename, pFileData, pSearchFor );
			char szGuid[37];
			const char *pGuid = pPos;
			V_strncpy( szGuid, pGuid, sizeof( szGuid ) );
			vcprojInfo.m_ProjectGUID = szGuid;

			const char *pEnd;
			if ( g_pVPC->Is2010() )
			{
				pPos = FindInFile( pFilename, pFileData, "<ProjectName>" );
				pEnd = V_stristr( pPos, "<" );
			}
			else
			{
				pPos = FindInFile( pFilename, pFileData, "Name=\"" );
				pEnd = V_stristr( pPos, "\"" );
			}

			if ( !pEnd || (pEnd - pPos) > 1024 || (pEnd - pPos) <= 0 )
				g_pVPC->VPCError( "Can't find valid 'Name=' in %s.", pFilename );

			char szName[256];
			V_strncpy( szName, pPos, (pEnd - pPos) + 1 );
			vcprojInfo.m_ProjectName = szName;

			vcprojInfos.AddToTail( vcprojInfo );			

			free( pFileData );
		}
	}

	// Parse g_SolutionItemsFilename, reading in filenames (including wildcards),
	// and add them to the Solution Items project we're already writing.
	void WriteSolutionItems( FILE *fp )
	{
		char szFullSolutionItemsPath[MAX_PATH];
		if ( V_IsAbsolutePath( g_pVPC->GetSolutionItemsFilename() ) )
			V_strncpy( szFullSolutionItemsPath, g_pVPC->GetSolutionItemsFilename(), sizeof( szFullSolutionItemsPath ) );
		else
			V_ComposeFileName( g_pVPC->GetStartDirectory(), g_pVPC->GetSolutionItemsFilename(), szFullSolutionItemsPath, sizeof( szFullSolutionItemsPath ) );
		
		g_pVPC->GetScript().PushScript( szFullSolutionItemsPath );

		int numSolutionItems = 0;
		while ( g_pVPC->GetScript().GetData() )
		{
			// read a line
			const char *pToken = g_pVPC->GetScript().GetToken( false );

			// strip out \r\n chars
			char *end = V_strstr( pToken, "\n" );
			if ( end )
			{
				*end = '\0';
			}
			end = V_strstr( pToken, "\r" );
			if ( end )
			{
				*end = '\0';
			}

			// bail on strings too small to be paths
			if ( V_strlen( pToken ) < 3 )
				continue;

			// compose an absolute path w/o any ../
			char szFullPath[MAX_PATH];
			if ( V_IsAbsolutePath( pToken ) )
				V_strncpy( szFullPath, pToken, sizeof( szFullPath ) );
			else
				V_ComposeFileName( g_pVPC->GetStartDirectory(), pToken, szFullPath, sizeof( szFullPath ) );

			if ( !V_RemoveDotSlashes( szFullPath ) )
				continue;

			if ( V_strstr( szFullPath, "*" ) != NULL )
			{
				// wildcard!
				char szWildcardPath[MAX_PATH];
				V_strncpy( szWildcardPath, szFullPath, sizeof( szWildcardPath ) );
				V_StripFilename( szWildcardPath );

				struct _finddata32_t data;
				intptr_t handle = _findfirst32( szFullPath, &data );
				if ( handle != -1L )
				{
					do 
					{
						if ( ( data.attrib & _A_SUBDIR ) == 0 )
						{
							// not a dir, just a filename - add it
							V_ComposeFileName( szWildcardPath, data.name, szFullPath, sizeof( szFullPath ) );

							if ( V_RemoveDotSlashes( szFullPath ) )
							{
								fprintf( fp, "\t\t%s = %s\n", szFullPath, szFullPath );
								++numSolutionItems;
							}
						}
					} while ( _findnext32( handle, &data ) == 0 );

					_findclose( handle );
				}
			}
			else
			{
				// just a file - add it
				fprintf( fp, "\t\t%s = %s\n", szFullPath, szFullPath );
				++numSolutionItems;
			}
		}

		g_pVPC->GetScript().PopScript();

		Msg( "Found %d solution files in %s\n", numSolutionItems, g_pVPC->GetSolutionItemsFilename() );
	}
};


static CSolutionGenerator_Win32 g_SolutionGenerator_Win32;
IBaseSolutionGenerator* GetSolutionGenerator_Win32()
{
	return &g_SolutionGenerator_Win32;
}

