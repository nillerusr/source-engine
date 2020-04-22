//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Generates a file list based on command line wildcard spec and drives
// conversion routines based on file extension.  The conversion routines should be
// !!!elsewhere!!! in libraries that the game also uses at runtime to convert data.
// This tool as spec'd should just be file iteration.
//
//=====================================================================================//

#include "MakeGameData.h"

// MAKESCENESIMAGE is defined for the external tool. In general, it only
//  supports the -pcscenes option. This gets built into MakeScenesImage.exe.

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class MakeGameDataApp : public CDefaultAppSystemGroup< CSteamAppSystemGroup >
{
public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown();
};

DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( MakeGameDataApp );

char			g_szSourcePath[MAX_PATH];
char			g_targetPath[MAX_PATH];
char			g_zipPath[MAX_PATH];
bool			g_bForce;
bool			g_bTest;
bool			g_bMakeZip;
CXZipTool		g_MasterXZip;
DiskWriteMode_t	g_WriteModeForConversions;
char			g_szGamePath[MAX_PATH];
char			g_szModPath[MAX_PATH];
bool			g_bModPathIsValid;
bool			g_bQuiet;
bool			g_bMakeScenes;
bool			g_bMakeScenesPC;
bool			g_bIsPlatformZip;
bool			g_bUseMapList;

IPhysicsCollision	*g_pPhysicsCollision;
CSysModule			*g_pPhysicsModule;

CUtlVector< CUtlString > g_ValidMapList;
CUtlVector< errorList_t > g_errorList;

const char *g_GameNames[] = 
{
	"ep2",
	"episodic",
	"hl2",
	"portal",
	"platform",
	"tf",
	NULL
};

// all known languages
const char *g_pLanguageSuffixes[] = 
{
	"_dannish",
	"_dutch",
	"_english",
	"_finnish",
	"_french",
	"_german",
	"_italian",
	"_japanese",
	"_korean",
	"_koreana",
	"_norwegian",
	"_polish",
	"_portuguese",
	"_russian",
	"_russion_buka",
	"_schinese",
	"_spanish",
	"_swedish",
	"_tchinese",
	"_thai",
};

// 360 is shipping with support for only these languages
const char *g_pTargetLanguageSuffixes[] = 
{
	"_english.",
	"_french.",
	"_german.",
};

// Master list of files that can go into the zip
static char *s_AllowedExtensionsInZip[] = 
{
	// Explicitly lacking from this list, thus excluded from the zip
	// .ain - AINs are external to the zip
	// .bsp - BSPs are external to the zip
	// .mp3 - MP3s aren't supported

	// Extensions with conversions
	// Purposely using .360 encoding to ensure pc versions don't leak in
	".360.wav",
	".360.vtf",
	".360.mdl",
	".360.ani",
	".dx90.360.vtx",
	".360.vvd",
	".360.phy",
	".360.dat",
	".360.lst",
	".360.vcs",
	".360.image",
	".360.pcf",

	// Extensions without conversions (taken as is)
	".rc",
	".txt",
	".cfg",
	".res",
	".vfe",
	".vbf",
	".vmt",
	".raw",
	".lst",
	".bns",
};



//-----------------------------------------------------------------------------
// Determine game path
//-----------------------------------------------------------------------------
void GetGamePath()
{
	GetCurrentDirectory( sizeof( g_szGamePath ), g_szGamePath );

	char szFullPath[MAX_PATH];
	if ( _fullpath( szFullPath, g_szGamePath, sizeof( szFullPath ) ) )
	{
		strcpy( g_szGamePath, szFullPath );
	}
	V_AppendSlash( g_szGamePath, sizeof( g_szGamePath ) );

	char *pGameDir = V_stristr( g_szGamePath, "game\\" );
	if ( !pGameDir )
	{
		Msg( "ERROR: Failed to determine game directory from current path. Expecting 'game' in current path." );
		exit( 1 );
	}

	// kill any trailing dirs
	pGameDir[4] = '\0';
}

//-----------------------------------------------------------------------------
// Determine mod path
//-----------------------------------------------------------------------------
bool GetModPath()
{
	char	szDirectory[MAX_PATH];
	char	szLastDirectory[MAX_PATH];

	// non destructively determine the mod directory
	bool bFound = false;
	szLastDirectory[0] = '\0';
	GetCurrentDirectory( sizeof( szDirectory ), szDirectory );
	while ( 1 )
	{
		V_ComposeFileName( szDirectory, "gameinfo.txt", g_szModPath, sizeof( g_szModPath ) );
		struct _stat statBuf;
		if ( _stat( g_szModPath, &statBuf ) != -1 )
		{
			bFound = true;
			V_strncpy( g_szModPath, szDirectory, sizeof( g_szModPath ) );
			break;
		}

		// previous dir
		V_ComposeFileName( szDirectory, "..", g_szModPath, sizeof( g_szModPath ) );

		char fullPath[MAX_PATH];
		if ( _fullpath( fullPath, g_szModPath, sizeof( fullPath ) ) )
		{
			strcpy( szDirectory, fullPath );
		}

		if ( !V_stricmp( szDirectory, szLastDirectory ) )
		{
			// can back up no further
			break;
		}
		strcpy( szLastDirectory, szDirectory );
	}

	if ( !bFound )
	{
		// use current directory instead
		GetCurrentDirectory( sizeof( g_szModPath ), g_szModPath );
	}

	return bFound;
}

//-----------------------------------------------------------------------------
// Setup File system and search paths
//-----------------------------------------------------------------------------
bool SetupFileSystem()
{
	if ( g_bModPathIsValid )
	{
		CFSSteamSetupInfo steamInfo;
		steamInfo.m_pDirectoryName = g_szModPath;
		steamInfo.m_bOnlyUseDirectoryName = true;
		steamInfo.m_bToolsMode = true;
		steamInfo.m_bSetSteamDLLPath = true;
		steamInfo.m_bSteam = false;
		if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
			return false;

		CFSMountContentInfo fsInfo;
		fsInfo.m_pFileSystem = g_pFullFileSystem;
		fsInfo.m_bToolsMode = true;
		fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;
		if ( FileSystem_MountContent( fsInfo ) != FS_OK )
			return false;

		// Finally, load the search paths for the "GAME" path.
		CFSSearchPathsInit searchPathsInit;
		searchPathsInit.m_pDirectoryName = steamInfo.m_GameInfoPath;
		searchPathsInit.m_pFileSystem = fsInfo.m_pFileSystem;
		if ( FileSystem_LoadSearchPaths( searchPathsInit ) != FS_OK )
			return false;

		char platform[MAX_PATH];
		Q_strncpy( platform, steamInfo.m_GameInfoPath, MAX_PATH );
		Q_StripTrailingSlash( platform );
		Q_strncat( platform, "/../platform", MAX_PATH, MAX_PATH );
		fsInfo.m_pFileSystem->AddSearchPath( platform, "PLATFORM" );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Helper utility, read file into buffer
//-----------------------------------------------------------------------------
bool ReadFileToBuffer( const char *pSourceName, CUtlBuffer &buffer, bool bText, bool bNoOpenFailureWarning )
{
	return scriptlib->ReadFileToBuffer( pSourceName, buffer, bText, bNoOpenFailureWarning );
}

//-----------------------------------------------------------------------------
// Purpose: Helper utility, Write buffer to file
//-----------------------------------------------------------------------------
bool WriteBufferToFile( const char *pTargetName, CUtlBuffer &buffer, bool bWriteToZip, DiskWriteMode_t writeMode )
{
	if ( g_bTest )
		return true;

	bool bSuccess = scriptlib->WriteBufferToFile( pTargetName, buffer, writeMode );
	bool bZipSuccess = true;

	if ( bSuccess && g_bMakeZip && !g_bTest && bWriteToZip )
	{
		if ( !g_MasterXZip.AddBuffer( pTargetName, buffer, true ) )
		{
			Msg( "WriteBufferToFile(): Error adding file %s\n", pTargetName );
			bZipSuccess = false;
		}
	}

	return bSuccess && bZipSuccess;
}

//-----------------------------------------------------------------------------
// Compress data
//-----------------------------------------------------------------------------
bool CompressCallback( CUtlBuffer &inputBuffer, CUtlBuffer &outputBuffer )
{
	if ( !inputBuffer.TellPut() )
	{
		// nothing to do
		return false;
	}

	unsigned int compressedSize;
	unsigned char *pCompressedOutput = LZMA_OpportunisticCompress( (unsigned char *)inputBuffer.Base() + inputBuffer.TellGet(), inputBuffer.TellPut() - inputBuffer.TellGet(), &compressedSize );
	if ( pCompressedOutput )
	{
		outputBuffer.EnsureCapacity( compressedSize );
		outputBuffer.Put( pCompressedOutput, compressedSize );
		free( pCompressedOutput );
		return true;
	}

	return false;
}



//-----------------------------------------------------------------------------
// Some converters need to run a final pass.
//-----------------------------------------------------------------------------
void DoPostProcessingFunctions( bool bWriteToZip )
{
	if ( g_bMakeScenes || g_bMakeScenesPC )
	{
		// scenes are converted and aggregated into one image
		CreateSceneImageFile( g_szModPath, bWriteToZip, g_bMakeScenesPC, g_bQuiet, g_WriteModeForConversions );
	}

	ProcessDXSupportConfig( bWriteToZip );
}

//-----------------------------------------------------------------------------
// Startup any conversion modules.
//-----------------------------------------------------------------------------
bool InitConversionModules()
{
	return true;
}

//-----------------------------------------------------------------------------
// Shutdown and cleanup any conversion modules.
//-----------------------------------------------------------------------------
void ShutdownConversionModules()
{
}

//-----------------------------------------------------------------------------
// Purpose: Distribute to worker function
//-----------------------------------------------------------------------------
bool CreateTargetFile( const char *pSourceName, const char *pTargetName, fileType_e fileType, bool bWriteToZip )
{
	// resolve relative source to absolute path
	// same workers use subdir name to determine conversion parameters
	char fullSourcePath[MAX_PATH];
	if ( _fullpath( fullSourcePath, pSourceName, sizeof( fullSourcePath ) ) )
	{
		pSourceName = fullSourcePath;
	}

	// distribute to actual worker
	// workers can expect exact final decorated filenames
	bool bSuccess = false;
	switch ( fileType )
	{
		case FILETYPE_UNKNOWN:
			break;

		case FILETYPE_VTF:
			bSuccess = CreateTargetFile_VTF( pSourceName, pTargetName, bWriteToZip );
			break;

		case FILETYPE_WAV:
			bSuccess = CreateTargetFile_WAV( pSourceName, pTargetName, bWriteToZip );
			break;

		case FILETYPE_MDL:
		case FILETYPE_ANI:
		case FILETYPE_VTX:
		case FILETYPE_VVD:
		case FILETYPE_PHY:
			bSuccess = CreateTargetFile_Model( pSourceName, pTargetName, bWriteToZip );
			break;

		case FILETYPE_BSP:
			bSuccess = CreateTargetFile_BSP( pSourceName, pTargetName, bWriteToZip );
			break;

		case FILETYPE_AIN:
			bSuccess = CreateTargetFile_AIN( pSourceName, pTargetName, bWriteToZip );
			break;

		case FILETYPE_CCDAT:
			bSuccess = CreateTargetFile_CCDAT( pSourceName, pTargetName, bWriteToZip );
			break;

		case FILETYPE_MP3:
			bSuccess = CreateTargetFile_MP3( pSourceName, pTargetName, bWriteToZip );
			break;

		case FILETYPE_RESLST:
			bSuccess = CreateTargetFile_RESLST( pSourceName, pTargetName, bWriteToZip );
			break;

		case FILETYPE_PCF:
			bSuccess = CreateTargetFile_PCF( pSourceName, pTargetName, bWriteToZip );
			break;

			// others...
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: Determine file type based on source extension.  Generate .360.xxx
// target name.
//-----------------------------------------------------------------------------
fileType_e ResolveFileType( const char *pSourceName, char *pTargetName, int targetNameSize )
{
	char szFullSourcePath[MAX_PATH];
	_fullpath( szFullSourcePath, pSourceName, sizeof( szFullSourcePath ) );

	char sourceExtension[MAX_PATH];
	V_ExtractFileExtension( pSourceName, sourceExtension, sizeof( sourceExtension ) );

	// default unrecognized
	fileType_e fileType = FILETYPE_UNKNOWN;

	if ( !V_stricmp( sourceExtension, "wav" ) )
	{
		 fileType = FILETYPE_WAV;
	}
	else if ( !V_stricmp( sourceExtension, "vtf" ) )
	{
		 fileType = FILETYPE_VTF;
	}
	else if ( !V_stricmp( sourceExtension, "mdl" ) )
	{
		fileType = FILETYPE_MDL;
	}
	else if ( !V_stricmp( sourceExtension, "ani" ) )
	{
		fileType = FILETYPE_ANI;
	}
	else if ( !V_stricmp( sourceExtension, "vvd" ) )
	{
		fileType = FILETYPE_VVD;
	}
	else if ( !V_stricmp( sourceExtension, "phy" ) )
	{
		fileType = FILETYPE_PHY;
	}
	else if ( !V_stricmp( sourceExtension, "bsp" ) )
	{
		fileType = FILETYPE_BSP;
	}
	else if ( !V_stricmp( sourceExtension, "ain" ) )
	{
		fileType = FILETYPE_AIN;
	}
	else if ( !V_stricmp( sourceExtension, "dat" ) )
	{
		if ( V_stristr( pSourceName, "closecaption_" ) )
		{
			// only want closecaption dat files, ignore all others
			fileType = FILETYPE_CCDAT;
		}
	}
	else if ( !V_stricmp( sourceExtension, "vtx" ) )
	{
		if ( V_stristr( pSourceName, ".dx90" ) )
		{
			// only want dx90 version, ignore all others
			fileType = FILETYPE_VTX;
		}
	}
	else if ( !V_stricmp( sourceExtension, "mp3" ) )
	{
		// mp3's are already pre-converted into .360.wav
		// slam the expected name here to simplify the external logic which will do the right thing
		V_StripExtension( pSourceName, pTargetName, targetNameSize );
		V_strcat( pTargetName, ".360.wav", targetNameSize );
		return FILETYPE_MP3;
	}
	else if ( !V_stricmp( sourceExtension, "lst" ) )
	{
		if ( V_stristr( szFullSourcePath, "reslists_xbox\\" ) )
		{
			// only want reslists map versions, due to special processing, ignore all others
			fileType = FILETYPE_RESLST;
		}
	}
	else if ( !V_stricmp( sourceExtension, "pcf" ) )
	{
		fileType = FILETYPE_PCF;
	}
	else if ( !V_stricmp( sourceExtension, "image" ) )
	{
		if ( V_stristr( szFullSourcePath, "scenes\\" ) )
		{
			// only want scene image, ignore all others
			fileType = FILETYPE_SCENEIMAGE;
		}
	}

	if ( fileType != FILETYPE_UNKNOWN && !V_stristr( pSourceName, ".360." ) )
	{
		char targetExtension[MAX_PATH];
		sprintf( targetExtension, ".360.%s", sourceExtension );

		V_StripExtension( pSourceName, pTargetName, targetNameSize );
		V_strcat( pTargetName, targetExtension, targetNameSize );
	}
	else
	{
		// unknown or already converted, target is same as input
		V_strncpy( pTargetName, pSourceName, targetNameSize );
	}

	return fileType;
}

//-----------------------------------------------------------------------------
// Purpose: Returns TRUE if file is a known localized file.
//-----------------------------------------------------------------------------
bool IsLocalizedFile( const char *pFileName )
{
	for ( int i = 0; i<ARRAYSIZE( g_pLanguageSuffixes ); i++ )
	{
		if ( V_stristr( pFileName, g_pLanguageSuffixes[i] ) )
		{
			// a localized file
			return true;
		}
	}

	// not a known localized file
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns TRUE if file is a supported localization.
//-----------------------------------------------------------------------------
bool IsLocalizedFileValid( const char *pFileName, const char *pLanguageSuffix )
{
	// file is a localized version
	if ( pLanguageSuffix )
	{
		if ( V_stristr( pFileName, pLanguageSuffix ) )
		{
			// allow it
			return true;
		}

		return false;
	}

	// must match the target supported languages
	for ( int i = 0; i < ARRAYSIZE( g_pTargetLanguageSuffixes ); i++  )
	{
		if ( V_stristr( pFileName, g_pTargetLanguageSuffixes[i] ) )
		{
			// allow it
			return true;
		}
	}
	
	// does not match a target language, not allowed
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Check against a list of allowed filetypes for inclusion in the zip
//-----------------------------------------------------------------------------
bool IncludeInZip( const char *pSourceName )
{
	if ( g_bIsPlatformZip )
	{
		// only allow known valid platform directories
		if ( !V_stristr( pSourceName, "materials\\" ) &&
			 !V_stristr( pSourceName, "resource\\" ) &&
			 !V_stristr( pSourceName, "vgui\\" ) )
		{
			// exclude it
			return false;
		}
	}

	for ( int i = 0; i < ARRAYSIZE( s_AllowedExtensionsInZip ); ++i )
	{
		const char *pAllowedExtension = s_AllowedExtensionsInZip[i];
		if ( !V_stristr( pSourceName, pAllowedExtension ) )
		{
			continue;
		}
	
		if ( !V_stricmp( pAllowedExtension, ".lst" ) )
		{
			// only want ???_exclude.lst files
			if  ( V_stristr( pSourceName, "_exclude.lst" ) )
			{
				return true;
			}
			return false;
		}

		if ( !V_stricmp( pAllowedExtension, ".txt" ) || !V_stricmp( pAllowedExtension, ".360.dat" ) )
		{
			if ( IsLocalizedFile( pSourceName ) && !IsLocalizedFileValid( pSourceName ) )
			{
				// exclude unsupported languages
				return false;
			}

			if ( !V_stricmp( pAllowedExtension, ".txt" ) && V_stristr( pSourceName, "closecaption_" ) )
			{
				// exclude all the closecaption_<language>.txt files
				return false;
			}
		}

		return true;
	}

	// exclude it
	return false;
}

//-----------------------------------------------------------------------------
// Returns true if map is in list, otherwise false
//-----------------------------------------------------------------------------
bool IsMapNameInList( const char *pMapName, CUtlVector< CUtlString > &mapList )
{
	char szBaseName[MAX_PATH];

	V_FileBase( pMapName, szBaseName, sizeof( szBaseName ) );
	V_strlower( szBaseName );

	if ( mapList.Find( szBaseName ) != mapList.InvalidIndex() )
	{
		// found
		return true;
	}

	// not found
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the list of valid BSPs
//-----------------------------------------------------------------------------
void BuildValidMapList( CUtlVector< CUtlString > &mapList )
{
	char szFilename[MAX_PATH];
	V_ComposeFileName( g_szModPath, "maplist.txt", szFilename, sizeof( szFilename ) );

	CUtlBuffer buffer;
	if ( !ReadFileToBuffer( szFilename, buffer, true, true ) )
	{
		return;
	}

	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "" );

	char szToken[MAX_PATH];
	char szMapName[MAX_PATH];
	for ( ;; )
	{
		int nTokenSize = buffer.ParseToken( &breakSet, szToken, sizeof( szToken ) );
		if ( nTokenSize <= 0 )
		{
			break;
		}

		// reslists are pc built, filenames can be sloppy
		V_FileBase( szToken, szMapName, sizeof( szMapName ) );
		V_strlower( szMapName );

		mapList.AddToTail( szMapName );
	}
}

#define DO_UPDATE	0x01
#define DO_ZIP		0x02

//-----------------------------------------------------------------------------
// Purpose: drive the file creation
//-----------------------------------------------------------------------------
void GenerateTargetFiles( CUtlVector<fileList_t> &fileList )
{
	char			sourcePath[MAX_PATH];
	char			sourceFile[MAX_PATH];
	char			targetFile[MAX_PATH];
	struct _stat	sourceStatBuf;
	struct _stat	targetStatBuf;

	strcpy( sourcePath, g_szSourcePath );
	V_StripFilename( sourcePath );
	if ( !sourcePath[0] )
		strcpy( sourcePath, "." );
	V_AppendSlash( sourcePath, sizeof( sourcePath ) );

	// default is to update and zip
	CUtlVector< int > updateList;
	updateList.AddMultipleToTail( fileList.Count() );
	for ( int i=0; i<fileList.Count(); i++ )
	{	
		updateList[i] = DO_UPDATE|DO_ZIP;
	}
	int numMatches = 0;

	// build update list
	for ( int i=0; i<fileList.Count(); i++ )
	{
		if ( fileList[i].fileName.IsEmpty() )
		{
			// ignore entries that have been culled
			updateList[i] = 0;
			continue;
		}

		const char *ptr = fileList[i].fileName.String();
		if ( !strnicmp( ptr, ".\\", 2 ) )
			ptr += 2;
		else if ( !strnicmp( ptr, sourcePath, strlen( sourcePath ) ) )
			ptr += strlen( sourcePath );

		strcpy( sourceFile, sourcePath );
		strcat( sourceFile, ptr );
		strcpy( targetFile, g_targetPath );
		strcat( targetFile, ptr );

		fileType_e fileType = ResolveFileType( sourceFile, targetFile, sizeof( targetFile ) );

		// check the target name for inclusion due to extension modifications
		if ( !IncludeInZip( targetFile ) )
		{
			// exclude from zip
			updateList[i] &= ~DO_ZIP;
		}

		if ( fileType == FILETYPE_UNKNOWN )
		{
			// No conversion function, can't do anything
			updateList[i] &= ~DO_UPDATE;
			continue;
		}
		else
		{
			// known filetype, which has a conversion
			// the wildcard match may catch existing converted files, which need to be rejected
			// cull exisiting conversions from the work list 
			// the non-converted filename is part of the same wildcard match, gets caught and resolved
			// the non-converted filename will then go through the proper conversion path
			if ( V_stristr( sourceFile, ".360." ) )
			{
				// cull completely
				updateList[i] = 0;
				continue;
			}
		}

		if ( fileType == FILETYPE_BSP || fileType == FILETYPE_AIN || fileType == FILETYPE_RESLST )
		{
			if ( g_ValidMapList.Count() && !IsMapNameInList( sourceFile, g_ValidMapList ) )
			{
				// cull completely
				updateList[i] = 0;
				continue;
			}
		}

		int retVal = _stat( sourceFile, &sourceStatBuf );
		if ( retVal != 0 )
		{
			// couldn't get source, skip update or zip
			updateList[i] = 0;
			continue;
		}

		retVal = _stat( targetFile, &targetStatBuf );
		if ( retVal != 0 )
		{
			// target doesn't exit, update is required
			continue;
		}

		// track valid candidates
		numMatches++;

		if ( fileType == FILETYPE_MDL || fileType == FILETYPE_ANI || fileType == FILETYPE_VTX || fileType == FILETYPE_VVD || fileType == FILETYPE_PHY )
		{
			// models are converted in a pre-pass
			// let the update logic run and catch the conversions for zipping
			continue;
		}

		if ( !g_bForce )
		{
			if ( difftime( sourceStatBuf.st_mtime, targetStatBuf.st_mtime ) <= 0 )
			{
				// target is same or older, no update
				updateList[i] &= ~DO_UPDATE;
			}
		}
	}
	
	// cleanse and determine totals, makes succeeding logic simpler
	int numWorkItems = 0;
	int numFilesToUpdate = 0;
	int numFilesToZip = 0;
	for ( int i=0; i<fileList.Count(); i++ )
	{
		if ( updateList[i] & DO_UPDATE )
		{
			numFilesToUpdate++;
		}
		if ( g_bMakeZip && ( updateList[i] & DO_ZIP ) )
		{
			numFilesToZip++;
		}
		else
		{
			updateList[i] &= ~DO_ZIP;
		}
		if ( updateList[i] )
		{
			numWorkItems++;
		}
	}

	Msg( "\n" );
	Msg( "Matched %d/%d files.\n", numMatches, fileList.Count() );
	Msg( "Creating or Updating %d files.\n", numFilesToUpdate );
	if ( g_bMakeZip )
	{
		Msg( "Zipping %d files.\n", numFilesToZip );
	}

	InitConversionModules();

	if ( numFilesToZip && !g_bTest )
	{
		Msg( "Creating Zip: %s\n", g_zipPath );
		if ( !g_MasterXZip.Begin( g_zipPath, XBOX_DVD_SECTORSIZE ) )
		{
			Msg( "ERROR: Failed to open \"%s\" for writing.\n", g_zipPath );
			return;
		}
		else
		{
			SetupCriticalPreloadScript( g_szModPath );
		}
	}

	// iterate work list
	int progress = 0;
	for ( int i=0; i<fileList.Count(); i++ )
	{
		if ( !updateList[i] )
		{
			// no update or zip needed, skip
			continue;
		}
	
		const char *ptr = fileList[i].fileName.String();
		if ( !strnicmp( ptr, ".\\", 2 ) )
			ptr += 2;
		else if ( !strnicmp( ptr, sourcePath, strlen( sourcePath ) ) )
			ptr += strlen( sourcePath );

		strcpy( sourceFile, sourcePath );
		strcat( sourceFile, ptr );
		strcpy( targetFile, g_targetPath );
		strcat( targetFile, ptr );

		fileType_e fileType = ResolveFileType( sourceFile, targetFile, sizeof( targetFile ) );

		if ( !g_bQuiet )
		{
			Msg( "%d/%d:%s -> %s\n", progress+1, numWorkItems, sourceFile, targetFile );
		}

		bool bSuccess = true;
		if ( updateList[i] & DO_UPDATE )
		{
			// generate target file (and optionally zip output)
			bSuccess = CreateTargetFile( sourceFile, targetFile, fileType, (updateList[i] & DO_ZIP) != 0 );
			if ( !bSuccess )
			{
				// add to error list
				int error = g_errorList.AddToTail();
				g_errorList[error].result = false;
				g_errorList[error].fileName.Set( sourceFile );
			}
		}
		else if ( updateList[i] & DO_ZIP )
		{
			// existing target file is zipped
			CUtlBuffer targetBuffer;
			bSuccess = scriptlib->ReadFileToBuffer( targetFile, targetBuffer );
			if ( bSuccess )
			{
				if ( !g_bTest )
				{
					bSuccess = g_MasterXZip.AddBuffer( targetFile, targetBuffer, true );
				}
			}
			if ( !bSuccess )
			{
				// add to error list
				int error = g_errorList.AddToTail();
				g_errorList[error].result = false;
				g_errorList[error].fileName.Set( targetFile );
			}
		}

		progress++;
	}

	DoPostProcessingFunctions( !g_bTest );

	ShutdownConversionModules();

	if ( numFilesToZip && !g_bTest )
	{
		g_MasterXZip.End();
	}

	// iterate error list
	Msg( "\n" );
	for ( int i = 0; i < g_errorList.Count(); i++ )
	{
		Msg( "ERROR: could not process %s\n", g_errorList[i].fileName.String() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Spew Usage
//-----------------------------------------------------------------------------
void Usage()
{
	Msg( "usage: MakeGameData [filemask] [options]\n" );
	Msg( "options:\n" );
	Msg( "[-v]                    Version\n" );
	Msg( "[-q]                    Quiet (critical spew only)\n" );
	Msg( "[-h] [-help] [-?]       Help\n" );
	Msg( "[-t targetPath]         Alternate output path, will generate output at target\n" );
	Msg( "[-r] [-recurse]         Recurse into source directory\n" );
	Msg( "[-f] [-force]           Force update, otherwise checks timestamps\n" );
	Msg( "[-test]                 Skip writing to disk\n" );
	Msg( "[-z <zipname>]          Generate zip file AND create or update stale conversions\n" );
	Msg( "[-zo <zipname>]         Generate zip file ONLY (existing stale conversions get updated, no new conversions are written)\n" );
	Msg( "[-preloadinfo]          Spew contents of preload section in zip\n" );
	Msg( "[-xmaquality <quality>] XMA Encoding quality override, [0-100]\n" );
	Msg( "[-scenes]               Make 360 scene image cache.\n" );
	Msg( "[-pcscenes]             Make PC scene image cache.\n" );
    Msg( "[-usemaplist]           For BSP related conversions, restricts to maplist.txt.\n" );

	exit( 1 );
}

//-----------------------------------------------------------------------------
// Purpose: default output func
//-----------------------------------------------------------------------------
SpewRetval_t OutputFunc( SpewType_t spewType, char const *pMsg )
{
	printf( pMsg );

	if ( spewType == SPEW_ERROR )
	{
		return SPEW_ABORT;
	}
	return ( spewType == SPEW_ASSERT ) ? SPEW_DEBUGGER : SPEW_CONTINUE; 
}

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool MakeGameDataApp::Create()
{
	SpewOutputFunc( OutputFunc );

	AppSystemInfo_t appSystems[] = 
	{
		{ "mdllib.dll",	MDLLIB_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	AddSystem( g_pDataModel, VDATAMODEL_INTERFACE_VERSION );
	AddSystem( g_pDmSerializers, DMSERIALIZERS_INTERFACE_VERSION );

	// Load vphysics.dll
	if ( !Sys_LoadInterface( "vphysics.dll", VPHYSICS_COLLISION_INTERFACE_VERSION, &g_pPhysicsModule, (void**)&g_pPhysicsCollision ) )
	{
		Msg( "Failed to load vphysics interface\n" );
		return false;
	}

	bool bOk = AddSystems( appSystems );
	if ( !bOk )
		return false;

	return true;
}

bool MakeGameDataApp::PreInit()
{
	CreateInterfaceFn factory = GetFactory();

	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );

	if ( !g_pFullFileSystem || !g_pDataModel || !g_pPhysicsCollision || !mdllib )
	{
		Warning( "MakeGameData is missing a required interface!\n" );
		return false;
	}

	return true;
}

void MakeGameDataApp::PostShutdown()
{
	if ( g_pPhysicsModule )
	{
		Sys_UnloadModule( g_pPhysicsModule );
		g_pPhysicsModule = NULL;
		g_pPhysicsCollision = NULL;
	}
	
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}

//-----------------------------------------------------------------------------
//	main
//
//-----------------------------------------------------------------------------
int MakeGameDataApp::Main()
{
	int			argnum;

	// set the valve library printer
	SpewOutputFunc( OutputFunc );

	Msg( "\nMAKEGAMEDATA - Valve Xbox 360 Game Data Compiler (Build: %s %s)\n", __DATE__, __TIME__ );
	Msg( "(C) Copyright 1996-2006, Valve Corporation, All rights reserved.\n\n" );

	if ( CommandLine()->FindParm( "-v" ) || CommandLine()->FindParm( "-version" ) )
	{
		// spew just the version, used by batches for logging
		return 0;
	}

#ifndef MAKESCENESIMAGE
	if ( CommandLine()->ParmCount() < 2 || CommandLine()->FindParm( "?" ) || CommandLine()->FindParm( "-h" ) || CommandLine()->FindParm( "-help" ) )
	{
		Usage();
	}
#endif // MAKESCENESIMAGE

	bool bHasFileMask = false;

	const char *pFirstArg = CommandLine()->GetParm( 1 );
	if ( pFirstArg[0] == '-' )
	{
		// first arg is an option, assume *.*
		strcpy( g_szSourcePath, "*.*" );
	}
	else
	{
		strcpy( g_szSourcePath, pFirstArg );
		bHasFileMask = true;
	}

	bool bRecurse = false;
#ifndef MAKESCENESIMAGE
	bRecurse = CommandLine()->FindParm( "-recurse" ) || CommandLine()->FindParm( "-r" );
	g_bForce = CommandLine()->FindParm( "-force" ) || CommandLine()->FindParm( "-f" );
	g_bTest = CommandLine()->FindParm( "-test" ) != 0;
	g_bQuiet = CommandLine()->FindParm( "-quiet" ) || CommandLine()->FindParm( "-q" );
	g_bMakeScenes = CommandLine()->FindParm( "-scenes" ) != 0;
	g_bMakeScenesPC = CommandLine()->FindParm( "-pcscenes" ) != 0;
#else
	g_bMakeScenesPC = true;
#endif // MAKESCENESIMAGE

#ifndef MAKESCENESIMAGE
	g_bUseMapList = CommandLine()->FindParm( "-usemaplist" ) != 0;
#endif // MAKESCENESIMAGE

	// Set up zip file options
	g_WriteModeForConversions = WRITE_TO_DISK_ALWAYS;
	argnum = CommandLine()->FindParm( "-z" );
	if ( argnum )
	{
		strcpy( g_szSourcePath, "*.*" );
		g_bMakeZip = true;
		g_bMakeScenes = true;
		bRecurse = true;
	}
	else
	{
		argnum = CommandLine()->FindParm( "-zo" );
		if ( argnum )
		{
			strcpy( g_szSourcePath, "*.*" );
			g_bMakeZip = true;
			g_bMakeScenes = true;
			g_WriteModeForConversions = WRITE_TO_DISK_UPDATE;
			bRecurse = true;
		}
	}
	if ( g_bMakeZip )
	{
		strcat( g_zipPath, CommandLine()->GetParm( argnum + 1 ) );
	}

	// default target path is source
	strcpy( g_targetPath, g_szSourcePath );
	V_StripFilename( g_targetPath );
	if ( !g_targetPath[0] )
	{
		strcpy( g_targetPath, "." );
	}

	// override via command line
	argnum = CommandLine()->FindParm( "-t" );
	if ( argnum )
	{
		V_strcpy_safe( g_targetPath, CommandLine()->GetParm( argnum + 1 ) );
	}
	V_AppendSlash( g_targetPath, sizeof( g_targetPath ) );

	if ( CommandLine()->FindParm( "-preloadinfo" ) )
	{
		g_MasterXZip.SpewPreloadInfo( g_szSourcePath );
		return 0;
	}

#ifndef MAKESCENESIMAGE
	GetGamePath();
#endif // MAKESCENESIMAGE

	g_bModPathIsValid = GetModPath();
	if ( !SetupFileSystem() )
	{
		Msg( "ERROR: Failed to setup file system.\n" );
		exit( 1 );
	}

	// data model initialization
	g_pDataModel->SetUndoEnabled( false );
	g_pDataModel->OnlyCreateUntypedElements( true );
	g_pDataModel->SetDefaultElementFactory( NULL );

	g_bIsPlatformZip = g_bMakeZip && ( V_stristr( g_szModPath, "\\platform" ) != NULL );

	// cleanup any zombie temp files left from a possible prior abort or error 
	scriptlib->DeleteTemporaryFiles( "mgd_*.tmp" );

	if ( g_bMakeZip || g_bUseMapList )
	{
		// zips use the map list to narrow the bsp conversion to actual shipping maps
		BuildValidMapList( g_ValidMapList );
	}

	CUtlVector<fileList_t> fileList;
	if ( bHasFileMask || g_bMakeZip )
	{
		scriptlib->FindFiles( g_szSourcePath, bRecurse, fileList );
	}

	// model conversions require seperate pre-processing to achieve grouping
	if ( !PreprocessModelFiles( fileList ) )
	{
		return 0;
	}

	GenerateTargetFiles( fileList );

	return 0;
}
