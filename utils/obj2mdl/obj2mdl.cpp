//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include <windows.h>
#include "tier0/dbg.h"
#include "strtools.h"
#include "utlvector.h"

bool verbose = false;

SpewRetval_t SpewFunc( SpewType_t type, char const *pMsg )
{	
	printf( "%s", pMsg );
	OutputDebugString( pMsg );
	
	if ( type == SPEW_ERROR )
	{
		printf( "\n" );
		OutputDebugString( "\n" );
	}

	return SPEW_CONTINUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void printusage( )
{
	printf( "usage:  obj2mdl <modelname.obj file> \n\n" );
	printf( "The directory containing the <modelname>.obj file must also contain a\n" );
	printf( "<modelname>.mtl file.\n" );
}

void CreateTemplateQC( const char *pcDirectory, const char *pcBaseName, const char *pcSteamName )
{
	char sz_Buffer[MAX_PATH];

	V_snprintf( sz_Buffer, MAX_PATH, "%s%s.qc", pcDirectory, pcBaseName );

	FILE *fp = fopen( sz_Buffer, "w" );

	V_snprintf( sz_Buffer, MAX_PATH, "$modelname \"contrib\\%s\\%s.mdl\"\n", pcSteamName, pcBaseName );
	fputs( sz_Buffer,  fp );
	fputs( "$upaxis Y\n",  fp );
	fputs( "$scale 1.00\n",  fp );
	V_snprintf( sz_Buffer, MAX_PATH, "$body \"Body\" \"%s.obj\"\n", pcBaseName );
	fputs( sz_Buffer,  fp );
	fputs( "$surfaceprop \"cloth\"\n", fp );
	V_snprintf( sz_Buffer, MAX_PATH, "$cdmaterials \"models\\contrib\\%s\\%s\"\n", pcSteamName, pcBaseName );
	fputs( sz_Buffer,  fp );
	V_snprintf( sz_Buffer, MAX_PATH, "$sequence \"idle\" \"%s.obj\" fps 30 numframes 30 loop\n", pcBaseName );
	fputs( sz_Buffer,  fp );
	V_snprintf( sz_Buffer, MAX_PATH, "$collisionmodel \"%s.obj\" {\n", pcBaseName );
	fputs( sz_Buffer,  fp );
	fputs( "$mass 5.0\n",  fp );
	fputs( "}\n",  fp );

	fclose( fp );
}

void CreateTemplateVMT( const char *pcDirectory, const char *pcBaseName, const char *pcTGAColorFile, const char *pcTGANormalFile, const char *pcSteamName )
{
	char sz_Buffer[MAX_PATH];
	char szTGAColorFileBase[MAX_PATH];
	char szTGANormalFileBase[MAX_PATH];

	V_snprintf( sz_Buffer, MAX_PATH, "%s%s.vmt", pcDirectory, pcBaseName );

	FILE *fp = fopen( sz_Buffer, "w" );

/*	fputs( "\"UnlitGeneric\"\n",  fp );
	fputs( "{\n",  fp );
	V_StripExtension( pcTGAColorFile, szTGAColorFileBase, sizeof( szTGAColorFileBase ) );
	V_snprintf( sz_Buffer, MAX_PATH, "\t\"$baseTexture\" \"models/contrib/%s/%s/%s\"\n", pcSteamName, pcBaseName, szTGAColorFileBase );
	fputs( sz_Buffer,  fp );
	fputs( "\t\$translucent\t1\n",  fp );
	fputs( "\t\"$vertexcolor\"\t1\n}\n",  fp );  */

	fputs( "\"VertexlitGeneric\"\n",  fp );
	fputs( "{\n",  fp );
	V_StripExtension( pcTGAColorFile, szTGAColorFileBase, sizeof( szTGAColorFileBase ) );
	V_snprintf( sz_Buffer, MAX_PATH, "\t\"$baseTexture\" \"models/contrib/%s/%s/%s\"\n", pcSteamName, pcBaseName, szTGAColorFileBase );
	fputs( sz_Buffer,  fp );
	
	if ( pcTGANormalFile && ( V_strlen( pcTGANormalFile ) > 0 ) )
	{
		V_StripExtension( pcTGANormalFile, szTGANormalFileBase, sizeof( szTGANormalFileBase ) );
		V_snprintf( sz_Buffer, MAX_PATH, "\t\"$bumpmap\" \"models/contrib/%s/%s/%s\"\n", pcSteamName, pcBaseName, szTGANormalFileBase );
		fputs( sz_Buffer,  fp );
	}

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$detail", "effects/tiledfire/fireLayeredSlowTiled512.vtf" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$detailscale", "5" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" %s\n", "$detailblendfactor", ".01" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" %s\n", "$detailblendmode", "6" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$yellow", "0" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$phong", "1" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$phongexponent", "25" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$phongboost", "5" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$lightwarptexture", "models\\lightwarps\\weapon_lightwarp" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$phongfresnelranges", "[.25 .5 1]" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$basemapalphaphongmask", "1" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$rimlight", "1" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$rimlightexponent", "4" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$rimlightboost", "2" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\"%s\" \"%s\"\n", "$cloakPassEnabled", "1" );
	fputs( sz_Buffer,  fp );

	fputs( "\t\"Proxies\"\n",  fp );
	fputs( "\t{\n",  fp );
	fputs( "\t\t\"weapon_invis\"\n",  fp );
	fputs( "\t\t{\n",  fp );
	fputs( "\t\t}\n",  fp );
	fputs( "\t\t\"AnimatedTexture\"\n",  fp );
	fputs( "\t\t{\n",  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\t\t\"%s\" \"%s\"\n", "animatedtexturevar", "$detail" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\t\t\"%s\" \"%s\"\n", "animatedtextureframenumvar", "$detailframe" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\t\t\"%s\" %s\n", "animatedtextureframerate", "30" );
	fputs( sz_Buffer,  fp );

	fputs( "\t\t}\n",  fp );

	fputs( "\t\t\"BurnLevel\"\n",  fp );
	fputs( "\t\t{\n",  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\t\t\"%s\" \"%s\"\n", "resultVar", "$detailblendfactor" );
	fputs( sz_Buffer,  fp );

	fputs( "\t\t}\n",  fp );

	fputs( "\t\t\"YellowLevel\"\n",  fp );
	fputs( "\t\t{\n",  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\t\t\"%s\" \"%s\"\n", "resultVar", "$yellow" );
	fputs( sz_Buffer,  fp );

	fputs( "\t\t}\n",  fp );


	fputs( "\t\t\"Equals\"\n",  fp );
	fputs( "\t\t{\n",  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\t\t\"%s\" \"%s\"\n", "srcVar1", "$yellow" );
	fputs( sz_Buffer,  fp );

	V_snprintf( sz_Buffer, MAX_PATH, "\t\t\t\"%s\" \"%s\"\n", "resultVar", "$color2" );
	fputs( sz_Buffer,  fp );

	fputs( "\t\t}\n",  fp );
	fputs( "\t}\n",  fp );
	fputs( "}\n",  fp ); 
	fclose( fp );
}

bool CheckFilesExist( const char *pcDirectory, const char *pcOBJFile, const char *pcMTLFile )
{
	WIN32_FIND_DATA wfd;
	HANDLE ff;
	char szSearchFile[MAX_PATH];

	V_snprintf( szSearchFile, MAX_PATH, "%s%s", pcDirectory, pcOBJFile );

	if ( ( ff = FindFirstFile( szSearchFile, &wfd ) ) != INVALID_HANDLE_VALUE )
	{
		V_snprintf( szSearchFile, MAX_PATH, "%s%s", pcDirectory, pcMTLFile );

		if ( ( ff = FindFirstFile( szSearchFile, &wfd ) ) != INVALID_HANDLE_VALUE )
		{
			return true;
		}
	}

	return false;
}

bool ParseMTL( const char *pcDirectory, const char *pcOBJFile, const char *pcMTLFile, char *pcTGAColorFile, size_t nTGAColorBufSize, char *pcTGASpecularFile, size_t nTGASpecularBufSize )
{
	char szMTLFile[MAX_PATH];
	V_snprintf( szMTLFile, MAX_PATH, "%s%s", pcDirectory, pcMTLFile );

	FILE *fp = fopen( szMTLFile, "r" );
	char szLine[MAX_PATH];

	while( fgets( szLine, sizeof( szLine ), fp ) )
	{
		char szToken[MAX_PATH];
		char szValue[MAX_PATH];
		char cTab;

		sscanf( szLine, "%s%c%s", szToken, &cTab, szValue );
		if ( V_stristr( szToken, "map_Kd" ) )
		{
			V_strncpy( pcTGAColorFile, V_UnqualifiedFileName( szValue ), nTGAColorBufSize );
		}
		else if ( V_stristr( szLine, "map_Ks" ) )
		{
			V_strncpy( pcTGASpecularFile, V_UnqualifiedFileName( szValue ), nTGASpecularBufSize );
		}
	}

	fclose( fp );

	return true;
}

bool ParseArguments( int argc, char* argv[], char *pcDirectory, size_t nDirectoryBufSize, char *pcBaseName, size_t nBaseNameBufSize, char *pcOBJFile, size_t nOBJFileBufSize, char *pcMTLFile, size_t nMTLFileBufSize )
{
	if ( argc < 2)
	{
		printusage();

		return false;
	}
	else
	{
		//
		// Setup OBJ and MTL globals. Exit if neither type is passed in
		//
		char szFile[MAX_PATH];

		V_ExtractFilePath( argv[1], pcDirectory, nDirectoryBufSize );
		V_strncpy( szFile, V_UnqualifiedFileName( argv[1] ), MAX_PATH );

		// Make sure that the file passed in was an OBJ
		if ( V_stristr( szFile, ".obj" ) ) 
		{
			V_FileBase( szFile, pcBaseName, nBaseNameBufSize );
			V_strncpy( pcOBJFile, szFile, nOBJFileBufSize );
			V_snprintf( pcMTLFile, nMTLFileBufSize, "%s.mtl", pcBaseName );
		}
		else
		{
			// Invalid file type passed in
			printusage();

			return false;
		}
	}

	return true;
}

bool GetSteamUserName( char *pcSteamName, size_t nSteamNameBufSize )
{
	HKEY hKey;
	char szModInstallPath[MAX_PATH];

	pcSteamName[0] = NULL;

	if ( ERROR_SUCCESS == RegOpenKey( HKEY_CURRENT_USER, "Software\\Valve\\Steam", &hKey ) ) 
	{
		DWORD dwSize = sizeof( szModInstallPath );
		RegQueryValueEx( hKey, "ModInstallPath", NULL, NULL, (LPBYTE)szModInstallPath, &dwSize );
		RegCloseKey( hKey );
		V_StripFilename( szModInstallPath );
		V_FileBase( szModInstallPath, pcSteamName, nSteamNameBufSize  );
	}

	return ( V_strlen( pcSteamName ) > 0 );
}

bool GetSDKBinDirectory( char *pcSDKBinDir, size_t nBuffSize )
{
	HKEY hKey;
	char szEngineVersion[MAX_PATH];
	char szSDKPath[MAX_PATH];

	*pcSDKBinDir = szEngineVersion[0] = szSDKPath[0] = NULL;
	if ( ERROR_SUCCESS == RegOpenKey( HKEY_CURRENT_USER, "Software\\Valve\\Source SDK", &hKey ) ) 
	{
		DWORD dwSize = sizeof( szEngineVersion );
		RegQueryValueEx( hKey, "EngineVer", NULL, NULL, (LPBYTE)szEngineVersion, &dwSize );
		RegCloseKey( hKey );
	}

	if ( ERROR_SUCCESS == RegOpenKey( HKEY_CURRENT_USER, "Environment", &hKey ) ) 
	{
		DWORD dwSize = sizeof( szSDKPath );
		RegQueryValueEx( hKey, "sourcesdk", NULL, NULL, (LPBYTE)szSDKPath, &dwSize );
		RegCloseKey( hKey );
	}

	if ( szEngineVersion[0] && szSDKPath[0] )
	{
		V_snprintf( pcSDKBinDir, nBuffSize, "%s\\bin\\%s\\bin", szSDKPath, szEngineVersion );
		return true;
	}

	return false;
}

bool GetSDKSourcesDirectory( char *pcSDKSourcesDir, size_t nBufSize )
{
	HKEY hKey;
	char szSDKPath[MAX_PATH];
	char szVProjectPath[MAX_PATH];

	*pcSDKSourcesDir = NULL;
	if ( ERROR_SUCCESS == RegOpenKey( HKEY_CURRENT_USER, "Environment", &hKey ) ) 
	{
		DWORD dwSize = sizeof( szSDKPath );
		RegQueryValueEx( hKey, "sourcesdk", NULL, NULL, (LPBYTE)szSDKPath, &dwSize );

		dwSize = sizeof( szVProjectPath );
		RegQueryValueEx( hKey, "vproject", NULL, NULL, (LPBYTE)szVProjectPath, &dwSize );
		RegCloseKey( hKey );
		
		V_snprintf( pcSDKSourcesDir, nBufSize, "%s_content\\%s", szSDKPath, V_UnqualifiedFileName( szVProjectPath ) );

		return true;
	}

	return false;
}

void RunCommandLine( const char *pCmdLine, const char *pWorkingDir )
{
	STARTUPINFO startupInfo;
	memset( &startupInfo, 0, sizeof( startupInfo ) );
	startupInfo.cb = sizeof( startupInfo );

	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof( pi ) );
	
	CreateProcess( 
		NULL,
		(char*)pCmdLine,
		NULL,
		NULL,
		FALSE,
		CREATE_NO_WINDOW,
		NULL,
		pWorkingDir,
		&startupInfo,
		&pi );

	WaitForSingleObject(pi.hProcess, INFINITE);
}

void CompileVTEX( const char *pcDirectory, const char *pcTGAColorFile, const char *pcSteamName, const char *pcBaseName )
{
	char szCmdLine[MAX_PATH];
	char szSDKBinDir[MAX_PATH];
	char szSDKSourcesDir[MAX_PATH];

	GetSDKBinDirectory( szSDKBinDir, sizeof( szSDKBinDir ) );
	GetSDKSourcesDirectory( szSDKSourcesDir, sizeof( szSDKSourcesDir ) );

	V_snprintf( szCmdLine, sizeof( szCmdLine ), "\"%s\\vtex.exe\" -nopause \"%s\\materialsrc\\models\\contrib\\%s\\%s\\%s\"", szSDKBinDir, szSDKSourcesDir, pcSteamName, pcBaseName, pcTGAColorFile );
	RunCommandLine( szCmdLine, szSDKBinDir );
}

void CompileMDL( const char *pcObjFile, const char *pcSteamName, const char *pcBaseName )
{
	char szCmdLine[MAX_PATH];
	char szSDKBinDir[MAX_PATH];
	char szSDKSourcesDir[MAX_PATH];

	GetSDKBinDirectory( szSDKBinDir, sizeof( szSDKBinDir ) );
	GetSDKSourcesDirectory( szSDKSourcesDir, sizeof( szSDKSourcesDir ) );

	V_snprintf( szCmdLine, sizeof( szCmdLine ), "\"%s\\studiomdl.exe\" -nop4 \"%s\\modelsrc\\contrib\\%s\\%s\\%s.qc\"", szSDKBinDir, szSDKSourcesDir, pcSteamName, pcBaseName, pcBaseName );
	RunCommandLine( szCmdLine, szSDKBinDir );
}

bool GetVProjectDirectory( char *pcVProjectPath, size_t nBufSize )
{
	bool bRetVal = false;
	HKEY hKey;
	pcVProjectPath[0] = NULL;

	if ( ERROR_SUCCESS == RegOpenKey( HKEY_CURRENT_USER, "Environment", &hKey ) ) 
	{
		DWORD dwSize = nBufSize;
		RegQueryValueEx( hKey, "vproject", NULL, NULL, (LPBYTE)pcVProjectPath, &dwSize );
		RegCloseKey( hKey );

		bRetVal = true;
	}
	
	return bRetVal;
}

void LaunchHLMV( const char *pcSteamName, const char *pcBaseName )
{
	char szCmdLine[MAX_PATH];
	char szSDKBinDir[MAX_PATH];
	char szVProjectPath[MAX_PATH];

	GetSDKBinDirectory( szSDKBinDir, sizeof( szSDKBinDir ) );
	GetVProjectDirectory( szVProjectPath, sizeof( szVProjectPath ) );

	V_snprintf( szCmdLine, sizeof( szCmdLine ), "\"%s\\hlmv.exe\" -game \"%s\" \"%s\\models\\contrib\\%s\\%s\\%s.mdl\"", szSDKBinDir, szVProjectPath, szVProjectPath, pcSteamName, pcBaseName, pcBaseName );
	RunCommandLine( szCmdLine, szSDKBinDir );
}


void CopyVMT( const char *pcDirectory, const char *pcBaseName, const char *pcSteamName  )
{
	char szVProjectPath[MAX_PATH];
	char szSourceVMT[MAX_PATH];
	char szDestVMT[MAX_PATH];

	GetVProjectDirectory( szVProjectPath, sizeof( szVProjectPath ) );

	V_snprintf( szSourceVMT, sizeof( szSourceVMT), "%s%s.vmt", pcDirectory, pcBaseName );
	V_snprintf( szDestVMT, sizeof( szDestVMT), "%s\\materials\\models\\contrib\\%s\\%s\\%s.vmt", szVProjectPath, pcSteamName, pcBaseName, pcBaseName );
	DeleteFile( szDestVMT );
	CopyFile( szSourceVMT, szDestVMT, false );
}

bool FileExists( const char* filePathName )
{
	DWORD attribs = ::GetFileAttributesA( filePathName );
	if (attribs == INVALID_FILE_ATTRIBUTES) 
	{
		return false;
	}
	return ( ( attribs & FILE_ATTRIBUTE_DIRECTORY ) == 0 );
}


bool DirectoryExists(const char* dirName)
{
	DWORD attribs = ::GetFileAttributesA(dirName);
	if (attribs == INVALID_FILE_ATTRIBUTES) 
	{
		return false;
	}
	return ( ( attribs & FILE_ATTRIBUTE_DIRECTORY ) != 0 );
}

bool CopyFiles( const char *pcSourceDir, const char *pcPattern, const char *pcDestDir )
{
	char szFindPattern[MAX_PATH];
	bool bAllSucceeded = true;

	V_snprintf( szFindPattern, sizeof( szFindPattern ), "%s%s", pcSourceDir, pcPattern );

	WIN32_FIND_DATA findData;
	HANDLE hFind = FindFirstFile( szFindPattern, &findData );
	if ( hFind == INVALID_HANDLE_VALUE )
	{
		return false;
	}
	else
	{
		do
		{
			char szSrcPath[MAX_PATH];
			char szDestPath[MAX_PATH];

			V_snprintf( szSrcPath, sizeof( szSrcPath ), "%s%s", pcSourceDir, findData.cFileName );
			V_snprintf( szDestPath, sizeof( szDestPath ), "%s\\%s", pcDestDir, findData.cFileName );

			DeleteFile( szDestPath );
			CopyFile( szSrcPath, szDestPath, false );
			bAllSucceeded &= FileExists( szDestPath );

		} while ( FindNextFile( hFind, &findData ) );
		FindClose( hFind );

		return bAllSucceeded;
	}
}

bool CopyMaterialSourcesToSrcTree( const char *pcDirectory, const char *pcSteamName, const char *pcBaseName )
{
	char szSDKSourcesDir[MAX_PATH];
	char szBuffer[MAX_PATH];
	bool bAllSucceeded = true;

	GetSDKSourcesDirectory( szSDKSourcesDir, sizeof( szSDKSourcesDir ) );

	// Root
	CreateDirectory( szSDKSourcesDir, NULL );
	bAllSucceeded &= DirectoryExists( szSDKSourcesDir );

	//
	// Materials
	//
	V_snprintf( szBuffer, sizeof( szBuffer ), "%s%s", szSDKSourcesDir, "\\materialsrc" );
	CreateDirectory( szBuffer, NULL );
	bAllSucceeded &= DirectoryExists( szBuffer );

	V_snprintf( szBuffer, sizeof( szBuffer ), "%s%s", szSDKSourcesDir, "\\materialsrc\\models" );
	CreateDirectory( szBuffer, NULL );
	bAllSucceeded &= DirectoryExists( szBuffer );

	V_snprintf( szBuffer, sizeof( szBuffer ), "%s%s", szSDKSourcesDir, "\\materialsrc\\models\\contrib" );
	CreateDirectory( szBuffer, NULL );
	bAllSucceeded &= DirectoryExists( szBuffer );

	V_snprintf( szBuffer, sizeof( szBuffer ), "%s%s%s", szSDKSourcesDir, "\\materialsrc\\models\\contrib\\", pcSteamName );
	CreateDirectory( szBuffer, NULL );
	bAllSucceeded &= DirectoryExists( szBuffer );

	V_snprintf( szBuffer, sizeof( szBuffer ), "%s%s%s\\%s", szSDKSourcesDir, "\\materialsrc\\models\\contrib\\", pcSteamName, pcBaseName );
	CreateDirectory( szBuffer, NULL );
	bAllSucceeded &= DirectoryExists( szBuffer );

	if ( bAllSucceeded )
	{
		CopyFiles( pcDirectory, "*.tga", szBuffer );
	}

	return bAllSucceeded;
}


bool ReplaceLineInTXTFile( const char *pcFilePath, const char* pcFirstChars, const char* pcReplacement )
{
	char sz_Buffer[MAX_PATH];
	CUtlStringList fileContents;

	// Read the old
	FILE *fp = fopen( pcFilePath, "r" );

	if ( !fp )
	{
		return false;
	}

	while( fgets( sz_Buffer, sizeof( sz_Buffer ), fp ) )
	{
		if ( !V_strncmp( sz_Buffer, pcFirstChars, V_strlen( pcFirstChars ) ) )
		{
			V_strncpy( sz_Buffer, pcReplacement, V_strlen( pcReplacement ) + 1 );
		}
		fileContents.CopyAndAddToTail( sz_Buffer );
	}

	fclose( fp );

	//
	// Write the new
	//
	fp = fopen( pcFilePath, "w" );

	if ( !fp )
	{
		fileContents.PurgeAndDeleteElements();
		return false;
	}

	FOR_EACH_VEC( fileContents, i )
	{
		fputs( fileContents[i], fp );
	}

	fclose( fp );

	fileContents.PurgeAndDeleteElements();
	return true;
}


bool CopyModelSourcesToSrcTree( const char *pcDirectory, const char *pcSteamName, const char *pcBaseName )
{
	char szSDKSourcesDir[MAX_PATH];
	char szBuffer[MAX_PATH];
	bool bAllSucceeded = true;

	GetSDKSourcesDirectory( szSDKSourcesDir, sizeof( szSDKSourcesDir ) );

	// Root
	CreateDirectory( szSDKSourcesDir, NULL );
	bAllSucceeded &= DirectoryExists( szSDKSourcesDir );

	//
	// Models
	//
	V_snprintf( szBuffer, sizeof( szBuffer ), "%s%s", szSDKSourcesDir, "\\modelsrc" );
	CreateDirectory( szBuffer, NULL );
	bAllSucceeded &= DirectoryExists( szBuffer );

	V_snprintf( szBuffer, sizeof( szBuffer ), "%s%s", szSDKSourcesDir, "\\modelsrc\\contrib" );
	CreateDirectory( szBuffer, NULL );
	bAllSucceeded &= DirectoryExists( szBuffer );

	V_snprintf( szBuffer, sizeof( szBuffer ), "%s%s%s", szSDKSourcesDir, "\\modelsrc\\contrib\\", pcSteamName );
	CreateDirectory( szBuffer, NULL );
	bAllSucceeded &= DirectoryExists( szBuffer );

	V_snprintf( szBuffer, sizeof( szBuffer ), "%s%s%s\\%s", szSDKSourcesDir, "\\modelsrc\\contrib\\", pcSteamName, pcBaseName );
	CreateDirectory( szBuffer, NULL );
	bAllSucceeded &= DirectoryExists( szBuffer );

	if ( bAllSucceeded )
	{
		CopyFiles( pcDirectory, "*.obj", szBuffer );
		CopyFiles( pcDirectory, "*.mtl", szBuffer );
		CopyFiles( pcDirectory, "*.qc", szBuffer );
	}

	// Replace some tags in the OBJ and MTL to satisfy some assumptions made by studiomdl
	char szFileToChange[MAX_PATH];
	char szReplacementLine[MAX_PATH];

	V_snprintf( szFileToChange, sizeof( szFileToChange ), "%s\\%s.mtl", szBuffer, pcBaseName );
	V_snprintf( szReplacementLine, sizeof( szReplacementLine ), "newmtl %s\n", pcBaseName );
	ReplaceLineInTXTFile( szFileToChange, "newmtl", szReplacementLine );
	V_snprintf( szReplacementLine, sizeof( szReplacementLine ), "map_Kd %s.tga\n", pcBaseName );
	ReplaceLineInTXTFile( szFileToChange, "map_Kd", szReplacementLine );

	V_snprintf( szFileToChange, sizeof( szFileToChange ), "%s\\%s.obj", szBuffer, pcBaseName );
	V_snprintf( szReplacementLine, sizeof( szReplacementLine ), "g %s\n", pcBaseName );
	ReplaceLineInTXTFile( szFileToChange, "g ", szReplacementLine );
	V_snprintf( szReplacementLine, sizeof( szReplacementLine ), "usemtl %s\n", pcBaseName );
	ReplaceLineInTXTFile( szFileToChange, "usemtl ", szReplacementLine );

	return bAllSucceeded;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : argc - 
//			argv[] - 
// Output : int
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	char szDirectory[MAX_PATH];
	char szOBJFile[MAX_PATH];
	char szMTLFile[MAX_PATH];
	char szTGAColorFile[MAX_PATH];
	char szTGANormalFile[MAX_PATH];
	char szTGATransparencyFile[MAX_PATH];
	char szTGASpecularFile[MAX_PATH];
	char szBaseName[MAX_PATH];
	char szSteamName[64];

	SpewOutputFunc( SpewFunc );

	szDirectory[0] = szMTLFile[0] = szOBJFile[0] = szSteamName[0] = szTGAColorFile[0] = szTGANormalFile[0] = szTGATransparencyFile[0] = szTGASpecularFile[0] = NULL;

	if ( ParseArguments( argc, argv, szDirectory, sizeof( szDirectory ), szBaseName, sizeof( szBaseName ), szOBJFile, sizeof( szOBJFile ), szMTLFile, sizeof( szMTLFile ) ) )
	{
		if ( CheckFilesExist(szDirectory, szOBJFile, szMTLFile ) )
		{
			printf( "Valve Software - obj2mdl.exe (%s)\n", __DATE__ );
			printf( "--- OBJ to MDL file conversion helper ---\n" );
			if ( !GetSteamUserName( szSteamName, sizeof( szSteamName ) ) )
			{
				printf( "--- Unable to get Steam user name. Exiting. ---\n" );
			}
			printf( "--- Reading MTL file ---\n" );
			ParseMTL( szDirectory, szOBJFile, szMTLFile, szTGAColorFile, sizeof( szTGAColorFile ), szTGASpecularFile, sizeof( szTGASpecularFile ) );
			printf( "--- Creating VMT and QC files ---\n" );
			CreateTemplateVMT( szDirectory, szBaseName, szTGAColorFile, szTGANormalFile, szSteamName );
			CreateTemplateQC( szDirectory, szBaseName, szSteamName );
			CopyMaterialSourcesToSrcTree( szDirectory, szSteamName, szBaseName );
			printf( "--- Compiling TGAs into a VTEX with vtex.exe---\n" );
			CompileVTEX( szDirectory, szTGAColorFile, szSteamName, szBaseName );
			CopyVMT( szDirectory, szBaseName, szSteamName );
			printf( "--- Compiling OBJs into an MDL with studiomdl.exe---\n" );
			CopyModelSourcesToSrcTree( szDirectory, szSteamName, szBaseName );
			CompileMDL( szOBJFile, szSteamName, szBaseName );
//			printf( "--- Launching model viewer ---\n" );
//			LaunchHLMV( szSteamName, szBaseName );
			
			return 0;
		}
	}

	return 1;
}
