//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <windows.h>
#include <stdlib.h>
#include <conio.h>
#include <direct.h>
#include <io.h>
#include "tier1/interface.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "filesystem.h"
#include "tier1/KeyValues.h"
#include "time.h"
#include "tier1/utllinkedlist.h"
#include "tier1/checksum_crc.h"



#define SSU_CONFIG_FILENAME "SymbolStoreUpdate.cfg"


enum FileUpdateStatus_t
{
	FILEUPDATE_CANTGETLOCALCOPY,
	FILEUPDATE_CRC_MATCH,			// The CRC matched the last one that was shoved in there so it didn't have to update the symbol store.
	FILEUPDATE_UPDATED,
	FILEUPDATE_ERROR
};

typedef enum
{
	FILETYPE_LOCAL=0,
	FILETYPE_VSS,
	FILETYPE_PERFORCE
} FileType_t;


class CFileInfo
{
public:
	CFileInfo()
	{
		m_Status = STATUS_OK;
		m_P4Client[0] = 0;
		m_BinaryName[0] = 0;
		m_FileType = FILETYPE_LOCAL;
		m_bDidCRC = false;
	}


public:

	FileType_t m_FileType;	// Where does this file come from?
	
	char m_P4Client[256];	// If nonzero length, then this is a perforce binary we need to get.
	char m_BinaryName[256];	// Name of the dll or exe file.

	bool m_bDidCRC;			// Used so we don't keep shoving the same things into the symbol store.
	CRC32_t m_LastCRC;

	enum
	{
		STATUS_OK,
		STATUS_ON_PROBATION			// This means we sent an email warning about this file not being available.
									// We can only get back to STATUS_OK when we can update the file successfully.
	};
	int m_Status;
};


IFileSystem *g_pFileSystem = 0;
CSysModule *g_pFSModule = 0;

char g_SymbolStoreDir[256];
CUtlLinkedList<CFileInfo*,int> g_FileInfos;

unsigned long g_nUpdateIntervalSeconds = 60 * 3;	// Every 3 minutes by default.


int g_nSuccessfulSymbolStores;
int g_nSuccessfulP4Updates;
int g_nSuccessfulVSSGets;
int g_nSuccessfulP4Gets;
int g_nUnnecessaryP4Updates;


char* GetTimeString()
{
	time_t ltime;
	time( &ltime );
	char *pStr = ctime( &ltime );

	static char tempStr[512];
	Q_strncpy( tempStr, pStr, sizeof( tempStr ) );
	
	char *pEnd = strchr( tempStr, '\n' );
	if ( pEnd )
		*pEnd = 0;

	pEnd = strchr( tempStr, '\r' );
	if ( pEnd )
		*pEnd = 0;

	return tempStr;
}


void ParseConfigFile()
{
	// Open the KeyValues file.
	KeyValues *pKV = new KeyValues( "" );
	if ( !pKV->LoadFromFile( g_pFileSystem, SSU_CONFIG_FILENAME ) )
		Error( "Error loading config file %s.\n", SSU_CONFIG_FILENAME );


	// Set the SSDIR environment variable.
	KeyValues *pDir = pKV->FindKey( "ssdir" );
	if ( !pDir )
		Error( "Config file %s is missing 'ssdir' key", SSU_CONFIG_FILENAME );

	char szBuffer[512];
	Q_snprintf( szBuffer, sizeof( szBuffer ), "SSDIR=%s", pDir->GetString() );
	if ( _putenv( szBuffer ) != 0 )
		Error( "_putenv( %s ) failed.\n", szBuffer );



	pDir = pKV->FindKey( "SymbolStore" );
	if ( !pDir )
		Error( "Config file %s is missing 'SymbolStore' key.\n", SSU_CONFIG_FILENAME );

	Q_strncpy( g_SymbolStoreDir, pDir->GetString(), sizeof( g_SymbolStoreDir ) );


	g_nUpdateIntervalSeconds = pKV->GetInt( "UpdateIntervalSeconds", g_nUpdateIntervalSeconds );
	Msg( "Update interval set to %d seconds.\n", g_nUpdateIntervalSeconds );


	// Create a tracker for each file in the info.
	for ( KeyValues *pKey=pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		bool bVSSBinary = (stricmp( pKey->GetName(), "binary" ) == 0);
		bool bLocalBinary = (stricmp( pKey->GetName(), "localbinary" ) == 0);
		bool bPerforceBinary = (stricmp( pKey->GetName(), "p4binary" ) == 0);
		if ( bVSSBinary || bLocalBinary || bPerforceBinary )
		{
			CFileInfo *pInfo = new CFileInfo;
			strncpy( pInfo->m_BinaryName, pKey->GetString(), sizeof( pInfo->m_BinaryName ) );
			
			if ( bLocalBinary )
				pInfo->m_FileType = FILETYPE_LOCAL;
			else if ( bVSSBinary )
				pInfo->m_FileType = FILETYPE_VSS;
			else
				pInfo->m_FileType = FILETYPE_PERFORCE;

			g_FileInfos.AddToTail( pInfo );
		}
	}
	if ( g_FileInfos.Count() == 0 )
		Error( "No 'binary' specifications in %s.\n", SSU_CONFIG_FILENAME );
}


void InitFileSystem( const char *pchExeDir )
{
	// Init various modules.
	if ( !Sys_LoadInterface(
		"filesystem_stdio",
		FILESYSTEM_INTERFACE_VERSION,
		&g_pFSModule,
		(void**)&g_pFileSystem ) )
	{
		Error( "Error loading filesystem_stdio.\n" );
	}

	g_pFileSystem->AddSearchPath( pchExeDir, "BASE" );
}


bool StripFilenameFromPath( char *pStr )
{
	// Now strip off the filename.
	char *pLastSlash = pStr;
	char *pCur = pStr;
	while ( *pCur )
	{
		if ( *pCur == '/' || *pCur == '\\' )
			pLastSlash = pCur;
		++pCur;
	}
	if ( pLastSlash == pStr )
		return false;

	*pLastSlash = 0;
	return true;
}


int RunCommandGetOutput( const char *cmdLine, CUtlVector<char> &output )
{
	const char *pTempOutputFilename = "temp_output.txt";
	char fullCmdLine[2048];

	Q_snprintf( fullCmdLine, sizeof( fullCmdLine ), "%s > %s", cmdLine, pTempOutputFilename );

	int ret = system( fullCmdLine );
	if ( ret == 0 )
	{
		// Read the command output.
		FILE *fp = fopen( pTempOutputFilename, "rb" );
		if ( fp )
		{
			fseek( fp, 0, SEEK_END );
			output.SetSize( ftell( fp ) );
			fseek( fp, 0, SEEK_SET );
			fread( output.Base(), output.Count(), 1, fp );
			fclose( fp );
		}
		else
		{
			output.Purge();
			output.AddToTail( 0 );
			return ret;
		}
			
		// Add a null-terminator.
		output.AddToTail( 0 );
	}

	DeleteFile( pTempOutputFilename );
	return ret;
}


void GetFilenameBase( const char *pFilename, const char* &pPrefix, const char* &pFilenameBase )
{
	// Get just the base filename.
	pFilenameBase = pFilename;
	pPrefix = "SymbolStoreUpdateTempFiles\\";
	const char *pCur = pFilename;
	while ( *pCur )
	{
		if ( *pCur == '\\' || *pCur == '/' )
			pFilenameBase = pCur + 1;
		++pCur;
	}
}


bool GetMostRecentP4Revision( CFileInfo *pInfo, char syncCommand[512] )
{
	CUtlVector<char> output;
	if ( pInfo->m_FileType == FILETYPE_PERFORCE )
	{
		char cmd[512];
		Q_snprintf( cmd, sizeof( cmd ), "p4 changes -m 1 \"%s\"", pInfo->m_BinaryName );

		if ( RunCommandGetOutput( cmd, output ) != 0 )
			return false;
	}
	else
	{
		if ( RunCommandGetOutput( "p4 changes -m 1", output ) != 0 )
			return false;
	}

	if ( Q_stristr( output.Base(), "change " ) != output.Base() )
		return false;

	int iRevisionNum;
	char *pNumber = output.Base() + 7;
	if ( sscanf( pNumber, "%d", &iRevisionNum ) == 1 )
	{
		if ( pInfo->m_FileType == FILETYPE_PERFORCE )
		{
			const char *pPrefix, *pFilenameBase;
			GetFilenameBase( pInfo->m_BinaryName, pPrefix, pFilenameBase );
			Q_snprintf( syncCommand, 512, "p4 print -o \"%s\" \"%s\"@%d", pFilenameBase, pInfo->m_BinaryName, iRevisionNum );
		}
		else
		{
			Q_snprintf( syncCommand, 512, "p4 sync //valvegames/main/src/...@%d", iRevisionNum );
		}

		return true;
	}
	else
	{
		return false;
	}
}


bool GetLocalCopyOfFile( CFileInfo *pInfo, const char* &pPrefix, const char* &pFilenameBase )
{
	// Get the file.
	pPrefix = "";

	char cmdLine[4096];
	int ret = 0;
	if ( pInfo->m_FileType == FILETYPE_LOCAL )
	{
		// m_BinaryName points right at a file on the local HD or a UNC path.
		pFilenameBase = pInfo->m_BinaryName;
		return _access( pInfo->m_BinaryName, 0 ) == 0;
	}
	else if ( pInfo->m_FileType == FILETYPE_VSS )
	{
		// This file comes from vss.
		Q_snprintf( cmdLine, sizeof( cmdLine ), "ss.exe get %s -I- -GLSymbolStoreUpdateTempFiles >> command_output.txt", pInfo->m_BinaryName );
		ret = system( cmdLine );

		if ( ret == 0 )
		{
			++g_nSuccessfulVSSGets;
			GetFilenameBase( pInfo->m_BinaryName, pPrefix, pFilenameBase );
			return true;
		}
		else
		{
			Warning( "Failed to get '%s' from vss (error %d).\n", pInfo->m_BinaryName, ret );
			return false;
		}
	}
	else
	{
		GetFilenameBase( pInfo->m_BinaryName, pPrefix, pFilenameBase );

		// This file comes from Perforce.
		Q_snprintf( cmdLine, sizeof( cmdLine ), "p4 print -o \"SymbolStoreUpdateTempFiles\\%s\" \"%s\" >> command_output.txt", pFilenameBase, pInfo->m_BinaryName );
		ret = system( cmdLine );

		if ( ret == 0 )
		{
			++g_nSuccessfulP4Gets;
			return true;
		}
		else
		{
			Warning( "Failed to get '%s' from Perforce (error %d).\n", pInfo->m_BinaryName, ret );
			return false;
		}
	}
}


void StoreP4Revision( CFileInfo *pInfo, const char *pFilenameBase )
{
	if ( pInfo->m_FileType != FILETYPE_VSS && pInfo->m_FileType != FILETYPE_PERFORCE )
		return;
	
	char cmdLine[512];
	char syncCommand[512];
	if ( GetMostRecentP4Revision( pInfo, syncCommand ) )
	{
		// Now find the directory where it put the file and put the current src_main Perforce revision up there.
		CUtlVector<char> output;
		Q_snprintf( cmdLine, sizeof( cmdLine ), "symstore query /f SymbolStoreUpdateTempFiles\\%s /s %s", pFilenameBase, g_SymbolStoreDir );
		int ret = RunCommandGetOutput( cmdLine, output );

		if ( ret == 0 )
		{
			char *pStr = Q_stristr( output.Base(), g_SymbolStoreDir );
			if ( pStr )
			{
				char *pSpace = pStr;
				while ( *pSpace && !isspace( *pSpace ) )
					++pSpace;

				*pSpace = 0;
				StripFilenameFromPath( pStr );

				// Now put the Perforce revision into a file in that directory.
				char revisionFilename[512];
				Q_snprintf( revisionFilename, sizeof( revisionFilename ), "%s\\p4revision.txt", pStr );
				if ( _access( revisionFilename, 00 ) != 0 )
				{
					FILE *fp = fopen( revisionFilename, "wt" );
					if ( fp )
					{
						fprintf( fp, "%s", syncCommand );
						fclose( fp );
						++g_nSuccessfulP4Updates;
					}
				}
				else
				{
					++g_nUnnecessaryP4Updates;
				}
			}
		}
	}
}


bool GetFileCRC( const char *pFilename, CRC32_t *pCRC )
{
	FILE *fp = fopen( pFilename, "rb" );
	if ( !fp )
		return false;

	CUtlVector<char> data;
	fseek( fp, 0, SEEK_END );
	data.SetSize( ftell( fp ) );
	fseek( fp, 0, SEEK_SET );

	fread( data.Base(), 1, data.Count(), fp );
	fclose( fp );
	

	CRC32_Init( pCRC );
	CRC32_ProcessBuffer( pCRC, data.Base(), data.Count() );
	CRC32_Final( pCRC );
	
	return true;
}


FileUpdateStatus_t UpdateFileInSymbolStore( CFileInfo *pInfo )
{
	const char *pFilenameBase, *pPrefix;

	if ( !GetLocalCopyOfFile( pInfo, pPrefix, pFilenameBase ) )
		return FILEUPDATE_CANTGETLOCALCOPY;

	char fullFilename[512];
	Q_snprintf( fullFilename, sizeof( fullFilename ), "%s%s", pPrefix, pFilenameBase );
	CRC32_t crc;
	if ( !GetFileCRC( fullFilename, &crc ) )
	{
		Warning( "GetFileCRC( %s ) failed.\n", fullFilename );
		return FILEUPDATE_ERROR;
	}

	// If the file's CRC is the same, then don't bother updating the symbol store again.
	if ( pInfo->m_bDidCRC && pInfo->m_LastCRC == crc )
		return FILEUPDATE_CRC_MATCH;

	pInfo->m_bDidCRC = true;
	pInfo->m_LastCRC = crc;

	// Now run the symbol store updater.
	char cmdLine[512];
	Q_snprintf( cmdLine, sizeof( cmdLine ), "symstore add /f \"%s\" /s \"%s\" /o /t SourceEngine >> command_output.txt", fullFilename, g_SymbolStoreDir );
	int ret = system( cmdLine );
	if ( ret == 0 )
	{
		++g_nSuccessfulSymbolStores;

		// Ask Perforce what the current revision # is.
		StoreP4Revision( pInfo, pFilenameBase  );
		return FILEUPDATE_UPDATED;
	}
	else
	{
		Warning( "%s - symstore.exe failed on '%s'.\n", GetTimeString(), pInfo->m_BinaryName );
		return FILEUPDATE_ERROR;
	}
}


const char *SetPathToExeDirectory()
{
	static char filename[MAX_PATH];
	if ( GetModuleFileName( GetModuleHandle( NULL ), filename, sizeof( filename ) ) == 0 )
		Error( "GetModuleFileNameEx failed.\n" );

	// Now strip off the filename.
	if ( !StripFilenameFromPath( filename ) )
		Error( "GetModuleFilename returned bad filename (%s).\n", filename );

	if ( _chdir( filename ) != 0 )
		Error( "_chdir( %s ) failed.\n", filename );

	return filename;
}


int main( int argc, char **argv )
{
	const char *pchExeDir = SetPathToExeDirectory();


	// Make this process idle priority.
	SetPriorityClass( GetCurrentProcess(), IDLE_PRIORITY_CLASS );


	// Initialize stuff.
	InitFileSystem( pchExeDir );
	ParseConfigFile();	


	// Clear out the temp files directory.
	if ( _access( "SymbolStoreUpdateTempFiles", 00 ) == 0 )
	{
		system( "rd /s /q SymbolStoreUpdateTempFiles" );
		system( "md SymbolStoreUpdateTempFiles" );
	}

	
	unsigned long nextUpdateTime = GetTickCount();
	int nPasses = 0;
	
	Msg( "\nUpdating files\n" );
	Msg( "- press F to force a refresh\n" );
	Msg( "- press ESC or Q to exit\n" );
	Msg( "\n" );
	while ( 1 )
	{
		if ( kbhit() )
		{
			int ch = getch();
			if ( toupper( ch ) == 'F' )
			{
				Msg( "\n\n'F' pressed, forcing an update.\n\n" );
				nextUpdateTime = 0;
			}
			else if ( toupper( ch ) == 'P' )
			{
				Msg( "\n\nP pressed. Pause for how many minutes? " );
				float flMinutes = 0;
				scanf( "%f", &flMinutes );
				Msg( "\nPausing for %f minutes...\n\n", flMinutes );
				nextUpdateTime = GetTickCount() + (int)( flMinutes * 60 * 1000 );
			}
			else if ( ch == 27 || toupper( ch ) == 'Q' )
			{
				break;
			}
		}

		if ( GetTickCount() >= nextUpdateTime )
		{
			g_nSuccessfulSymbolStores = 0;
			g_nSuccessfulP4Updates = 0;
			g_nSuccessfulVSSGets = 0;
			g_nSuccessfulP4Gets = 0;
			g_nUnnecessaryP4Updates = 0;

			int nCRCMatches = 0;

			// For each file, grab its exe and put it in the store, then grab its PDB and do the same.
			int iCount = 0;
			FOR_EACH_LL( g_FileInfos, j )
			{
				if ( UpdateFileInSymbolStore( g_FileInfos[j] ) == FILEUPDATE_CRC_MATCH )
					++nCRCMatches;

				++iCount;

				Msg( "\rUpdated %d of %d - %d CRC matches...      ", iCount, g_FileInfos.Count(), nCRCMatches );
				if ( kbhit() )
					break;
			}			
			
			// Wait for 2 minutes.
			nextUpdateTime = GetTickCount() + g_nUpdateIntervalSeconds * 1000;
			Msg( "\n\n%s\n"
				 "Pass %d completed.\n", GetTimeString(), ++nPasses );
			Msg( "- %d successful vss gets, %d successful p4 gets\n", g_nSuccessfulVSSGets, g_nSuccessfulP4Gets );
			Msg( "- %d successful symbol store updates\n", g_nSuccessfulSymbolStores );
			Msg( "- %d new p4revision.txt files written\n", g_nSuccessfulP4Updates );
			Msg( "\n" );
		}
		else
		{
			Sleep( 300 );
		}
	}
	
	Msg( "\n\nKey pressed, exiting.\n" );

	return 0;
}


