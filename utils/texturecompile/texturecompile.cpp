//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// vmpi_bareshell.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <conio.h>
#include <process.h>
#include "vmpi.h"
#include "filesystem.h"
#include "vmpi_filesystem.h"
#include "vmpi_distribute_work.h"
#include "vmpi_tools_shared.h"
#include "cmdlib.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlstringmap.h"
#include "tier0/icommandline.h"
#include "tier1/strtools.h"
#include "threads.h"
#include "tier0/dbg.h"
#include "interface.h"
#include "ilaunchabledll.h"
#include <direct.h>
#include "io.h"
#include <sys/types.h>
#include <sys/stat.h>

#define STARTWORK_PACKETID	5
#define WORKUNIT_PACKETID	6
#define ERRMSG_PACKETID		7
#define TEXTUREHADERROR_PACKETID		8
#define MACHINE_NAME 9

#define TEXTURES_PER_WORKUNIT	1

#ifdef _DEBUG
//#define DEBUGFP
#endif

const char *g_pGameDir = NULL;
const char *g_pTextureOutputDir = NULL;
char g_WorkerTempPath[MAX_PATH];
char g_ExeDir[MAX_PATH];
CUtlVector<char *> g_CompileCommands;
int g_nTexturesPerWorkUnit = 0;
#ifdef DEBUGFP
FILE *g_WorkerDebugFp = NULL;
#endif
bool g_bGotStartWorkPacket = false;
double g_flStartTime;
bool g_bVerbose = false;

struct TextureInfo_t
{
};

void Texture_ParseTextureInfoFromCompileCommands( int nCompileCommandID, TextureInfo_t &shaderInfo );
void Worker_GetSourceFiles( int iWorkUnit );

void Worker_GetLocalCopyOfTextureSource( const char *pVtfName );

// g_ByteCode["shadername"][shadercombo][bytecodeoffset]
//typedef CUtlVector<CUtlBuffer> VectorOfBuffers_t;

//CUtlStringMap<TextureInfo_t> g_TextureToTextureInfo;

typedef CUtlVector<char> CharVector_t;

struct SourceTargetPair_t
{
	char *pSrcName;
	char *pTargetName;
};

CUtlVector<SourceTargetPair_t> g_SourceTargetPairs;

CUtlStringMap<bool> g_Master_TextureHadError;

CDispatchReg g_DistributeWorkReg( WORKUNIT_PACKETID, DistributeWorkDispatch );

unsigned long VMPI_Stats_GetJobWorkerID( void )
{
	return 0;
}


bool StartWorkDispatch( MessageBuffer *pBuf, int iSource, int iPacketID )
{
	g_bGotStartWorkPacket = true;
	return true;
}

CDispatchReg g_StartWorkReg( STARTWORK_PACKETID, StartWorkDispatch );

bool ErrMsgDispatch( MessageBuffer *pBuf, int iSource, int iPacketID )
{
	static CUtlStringMap<bool> errorMessages;
	if( !errorMessages.Defined( pBuf->data + 1 ) )
	{
		errorMessages[pBuf->data + 1] = true;
		fprintf( stderr, "ERROR: %s\n", pBuf->data + 1 );
	}
	return true;
}

CDispatchReg g_ErrMsgReg( ERRMSG_PACKETID, ErrMsgDispatch );

bool TextureHadErrorDispatch( MessageBuffer *pBuf, int iSource, int iPacketID )
{
	g_Master_TextureHadError[pBuf->data+1] = true;
	return true;
}

CDispatchReg g_TextureHadErrorReg( TEXTUREHADERROR_PACKETID, TextureHadErrorDispatch );

void DebugOut( const char *pMsg, ... )
{
	char msg[2048];
	va_list marker;
	va_start( marker, pMsg );
	_vsnprintf( msg, sizeof( msg ), pMsg, marker );
	va_end( marker );
	
	if (g_bVerbose)
	{
		Msg( "%s", msg );
	}

#ifdef DEBUGFP
	fprintf( g_WorkerDebugFp, "%s", msg );
	fflush( g_WorkerDebugFp );
#endif
}


// Worker should implement this so it will quit nicely when the master disconnects.
void MyDisconnectHandler( int procID, const char *pReason )
{
	// If we're a worker, then it's a fatal error if we lose the connection to the master.
	if ( !g_bMPIMaster )
	{
		Msg( "Master disconnected.\n ");
		DebugOut( "Master disconnected.\n" );
		TerminateProcess( GetCurrentProcess(), 1 );
	}
}


// pBuf is ready to read the results written to the buffer in ProcessWorkUnitFn.
// work is done. .master gets it back this way.
// compiled code in pBuf
void Master_ReceiveWorkUnitFn( uint64 iWorkUnit, MessageBuffer *pBuf, int iWorker )
{
	DebugOut( "Master_ReceiveWorkUnitFn\n" );
	int textureStart = iWorkUnit * g_nTexturesPerWorkUnit;
	int textureEnd = textureStart + g_nTexturesPerWorkUnit;
	textureEnd = min( g_CompileCommands.Count(), textureEnd );
	int i;
	for( i = textureStart; i < textureEnd; i++ )
	{
		int len;
		pBuf->read( &len, sizeof( len ) );
		if( len == 0 )
		{
			continue;
		}
		CUtlBuffer fileData;
		fileData.EnsureCapacity( len );
		pBuf->read( fileData.Base(), len );

		Warning( "%s\n", g_CompileCommands[i] );
		FILE *fp;
		fp = fopen( g_CompileCommands[i], "wb" );
		if ( !fp )
			Error( "Can't open %s for writing.\n", g_CompileCommands[i] );
		fwrite( fileData.Base(), 1, len, fp );
		fclose( fp );
	}
}

// same as "system", but doesn't pop up a window
void MySystem( char *pCommand )
{
	FILE *batFp = fopen( "temp.bat", "w" );
	fprintf( batFp, "%s\n", pCommand );
	fclose( batFp );
	
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	ZeroMemory( &pi, sizeof(pi) );
	
	// Start the child process. 
	if( !CreateProcess( NULL, // No module name (use command line). 
		"temp.bat", // Command line. 
		NULL,             // Process handle not inheritable. 
		NULL,             // Thread handle not inheritable. 
		FALSE,            // Set handle inheritance to FALSE. 
		IDLE_PRIORITY_CLASS | CREATE_NO_WINDOW,                // No creation flags. 
		NULL,             // Use parent's environment block. 
		g_WorkerTempPath, // Use parent's starting directory. 
		&si,              // Pointer to STARTUPINFO structure.
		&pi )             // Pointer to PROCESS_INFORMATION structure.
		) 
	{
		Error( "CreateProcess failed." );
		Assert( 0 );
	}
	
	// Wait until child process exits.
	WaitForSingleObject( pi.hProcess, INFINITE );
	
	// Close process and thread handles. 
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
}

void VTFNameToTGAName( const char *pSrcName, char *pDstName )
{
	pDstName[0] = '\0';
	const char *pMaterials = Q_stristr( pSrcName, "materials" );
	Assert( pMaterials );
	if( pMaterials )
	{
		Q_strncpy( pDstName, pSrcName, pMaterials - pSrcName + 1 );
	}
	Q_strcat( pDstName, "materialsrc", MAX_PATH );
	Q_strcat( pDstName, pMaterials + strlen( "materials" ), MAX_PATH );
	Q_StripExtension( pDstName, pDstName, strlen( pDstName ) );
	Q_strcat( pDstName, ".tga", MAX_PATH );
}

// You must append data to pBuf with the work unit results.
void Worker_ProcessWorkUnitFn( int iThread, uint64 iWorkUnit, MessageBuffer *pBuf )
{
	DebugOut( "Worker_ProcessWorkUnitFn textures/workunit=%d\n", g_nTexturesPerWorkUnit );
	Worker_GetSourceFiles( iWorkUnit );
	int textureStart = iWorkUnit * g_nTexturesPerWorkUnit;
	int textureEnd = textureStart + g_nTexturesPerWorkUnit;
	textureEnd = min( g_CompileCommands.Count(), textureEnd );

	int i;
	for( i = textureStart; i < textureEnd; i++ )
	{
		DebugOut( "texture to compile: \"%s\"\n", g_CompileCommands[i] );
		char cmdline[1024];
		char tganame[1024];
		VTFNameToTGAName( g_CompileCommands[i], tganame );
		sprintf( cmdline, "vtex -allowdebug -vproject \"%s%s\" -mkdir -nopause \"%s%s\"", g_WorkerTempPath, g_pGameDir + 3, g_WorkerTempPath, tganame + 3 ); // hack hack
		DebugOut( cmdline );
		DebugOut( "\n" );
//		MySystem( cmdline );
		system( cmdline );

		char localVTFName[1024];
		sprintf( localVTFName, "%s%s", g_WorkerTempPath, g_CompileCommands[i] + 3 );
		DebugOut( "local: \"%s\"\n", localVTFName );
		
		FILE *fp = fopen( localVTFName, "rb" );
		if( fp )
		{
			// Send the compiled shader to the master
			fseek( fp, 0, SEEK_END );
			int len = ftell( fp );
			fseek( fp, 0, SEEK_SET );
			CUtlBuffer buf;
			buf.EnsureCapacity( len );
			int nBytesRead = fread( buf.Base(), 1, len, fp );
			fclose( fp );
			buf.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );

			pBuf->write( &len, sizeof( len ) );
			pBuf->write( buf.Base(), len );
		}
		else
		{
//			static CUtlStringMap<bool> m_FileAlreadyFailed;
//			
//			if( !m_FileAlreadyFailed.Defined( g_CompileCommands[i] ) )
//			{
//				m_FileAlreadyFailed[g_CompileCommands[i]] = true;
//				CUtlVector<char> fileNameBuf;
//				fileNameBuf.AddToTail( ( char )TEXTUREHADERROR_PACKETID );
//				int len = strlen( g_CompileCommands[i] );
//				fileNameBuf.AddMultipleToTail( len + 1, g_CompileCommands[i] );
//				VMPI_SendData( fileNameBuf.Base(), fileNameBuf.Count(), VMPI_MASTER_ID );
//			}

			// Write zero to signify that we didn't do anything here.
			int len = 0;
			pBuf->write( &len, sizeof( len ) );
		}
		VMPI_HandleSocketErrors();
	}
}

void MakeDirHier( const char *pPath )
{
	char temp[1024];
	Q_strncpy( temp, pPath, 1024 );
	int i;
	for( i = 0; i < strlen( temp ); i++ )
	{
		if( temp[i] == '/' || temp[i] == '\\' )
		{
			temp[i] = '\0';
//			DebugOut( "mkdir( %s )\n", temp );
			mkdir( temp );
			temp[i] = '\\';
		}
	}
//	DebugOut( "mkdir( %s )\n", temp );
	mkdir( temp );
}

void Worker_ReadFilesToCopy( void )
{
	// Create virtual files for all of the stuff that we need to compile the shader
	// make sure and prefix the file name so that it doesn't find it locally.
	char filename[1024];
	sprintf( filename, "%s\\filestocopy.txt", g_pGameDir );
	DebugOut( "using \"%s\" as filestocopy\n", filename );
	char buf[1024];
	FileHandle_t fp = g_pFileSystem->Open( filename, "r" );
	if( fp == FILESYSTEM_INVALID_HANDLE )
	{
		fprintf( stderr, "Can't open uniquefilestocopy.txt!\n" );
		exit( -1 );
	}

	while( CmdLib_FGets( buf, sizeof( buf ), fp ) )
	{
		// get rid of the newline if there is one.
		int len = strlen( buf );
		if( buf[len-1] == 0xd || buf[len-1] == 0xa )
		{
			buf[len-1] = '\0';
		}
//		DebugOut( "buf: %s\n", buf );

		char *pStar = Q_stristr( buf, "*" );
		Assert( pStar );
		if( !pStar )
		{
			continue;
		}

		*pStar = '\0';
		char *pVtfName = buf;
		char *pSrcName = pStar + 1;

		SourceTargetPair_t &pair = g_SourceTargetPairs[g_SourceTargetPairs.AddToTail()];
		pair.pSrcName = strdup( pSrcName );
		pair.pTargetName = strdup( pVtfName );
	}
	g_pFileSystem->Close( fp );
}

void Worker_GetSourceFiles( int iWorkUnit )
{
	DebugOut( "Worker_GetSourceFiles( %d )\n", iWorkUnit );
	int textureStart = iWorkUnit * g_nTexturesPerWorkUnit;
	int textureEnd = textureStart + g_nTexturesPerWorkUnit;
	textureEnd = min( g_CompileCommands.Count(), textureEnd );
	int i;
	for( i = textureStart; i < textureEnd; i++ )
	{
		Worker_GetLocalCopyOfTextureSource( g_CompileCommands[i] );
	}
}

void Worker_GetFileFromMaster( const char *pFileName )
{
	DebugOut( "Worker_GetFileFromMaster: \"%s\"\n", pFileName );
	FileHandle_t fp2 = g_pFileSystem->Open( pFileName, "rb" );
	bool bZeroLength = false;
	if( fp2 == FILESYSTEM_INVALID_HANDLE )
	{
		bZeroLength = true;
		Warning( "zero length file: \"%s\"\n", pFileName );
		// make a zero length file hack hack hack
//			continue;
	}
	CUtlVector<char> fileBuf;
	int fileLen = 0;
	if( !bZeroLength )
	{
//		printf( "getting local copy of file: \"%s\"\n", pFileName );
		fileLen = g_pFileSystem->Size( fp2 );
		fileBuf.SetCount( fileLen );
		g_pFileSystem->Read( fileBuf.Base(), fileLen, fp2 );
		g_pFileSystem->Close( fp2 );
	}

	// create the dir that the file needs to go into.
	char path[1024];
	char filename[1024];
	sprintf( path, "%s%s", g_WorkerTempPath, pFileName + 3 ); // dear lord . .skip the u:\ BUG BUG BUG
//		printf( "creating \"%s\"\n", path );
	Q_StripFilename( path );
	MakeDirHier( path );

	sprintf( filename, "%s%s", g_WorkerTempPath, pFileName + 3 ); // dear lord . .skip the u:\ BUG BUG BUG
//	printf( "creating \"%s\"\n", pFileName );
	
	FILE *fp3 = fopen( filename, "wb" );
	if( !fp3 )
	{
		Error( "Couldn't open \"%s\"\n", filename );
	}
	if( !bZeroLength )
	{
		fwrite( fileBuf.Base(), 1, fileLen, fp3 );
	}
	fclose( fp3 );
}

void Worker_GetLocalCopyOfTextureSource( const char *pVtfName )
{
	int i;
	for( i = 0; i < g_SourceTargetPairs.Count(); i++ )
	{
//		DebugOut( "comparing: \"%s\" \"%s\"\n", pVtfName, g_SourceTargetPairs[i].pTargetName );
		if( Q_stricmp( pVtfName, g_SourceTargetPairs[i].pTargetName ) == 0 )
		{
//			DebugOut( "MATCH!\n" );
			Worker_GetFileFromMaster( g_SourceTargetPairs[i].pSrcName );
		}
	}
}

void Worker_GetLocalCopyOfBinary( const char *pFilename )
{
	CUtlBuffer fileBuf;
	char tmpFilename[MAX_PATH];
	sprintf( tmpFilename, "%s\\%s", g_ExeDir, pFilename );
	printf( "trying to open: %s\n", tmpFilename );
	
	FILE *fp = fopen( tmpFilename, "rb" );
	if( !fp )
	{
		Assert( 0 );
		fprintf( stderr, "Can't open %s!\n", pFilename );
		exit( -1 );
	}
	fseek( fp, 0, SEEK_END );
	int fileLen = ftell( fp );
	fseek( fp, 0, SEEK_SET );
	fileBuf.EnsureCapacity( fileLen );
	int nBytesRead = fread( fileBuf.Base(), 1, fileLen, fp );
	fclose( fp );
	fileBuf.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );

	char newFilename[MAX_PATH];
	sprintf( newFilename, "%s%s", g_WorkerTempPath, pFilename );
	
	DebugOut( "this is fucked \"%s\"\n", newFilename );
	FILE *fp2 = fopen( newFilename, "wb" );
	if( !fp2 )
	{
		Assert( 0 );
		fprintf( stderr, "Can't open %s!\n", newFilename );
		exit( -1 );
	}
	fwrite( fileBuf.Base(), 1, fileLen, fp2 );
	fclose( fp2 );
}

void Worker_GetLocalCopyOfBinaries( void )
{
	Worker_GetLocalCopyOfBinary( "vtex.exe" );
	Worker_GetLocalCopyOfBinary( "vtex_dll.dll" );
	Worker_GetLocalCopyOfBinary( "vstdlib.dll" );
	Worker_GetLocalCopyOfBinary( "tier0.dll" );
}

void Shared_ParseListOfCompileCommands( void )
{
	char buf[1024];

	char fileListFileName[1024];
	sprintf( fileListFileName, "%s\\texturelist.txt", g_pGameDir );
	FileHandle_t fp = g_pFileSystem->Open( fileListFileName, "r" );
	if( fp == FILESYSTEM_INVALID_HANDLE )
	{
		DebugOut( "Can't open %s!\n", fileListFileName );
		fprintf( stderr, "Can't open %s!\n", fileListFileName );
		exit( -1 );
	}
	while( CmdLib_FGets( buf, 1023, fp ) )
	{
		char *pNewString = new char[ strlen( buf ) + 1 ];
		strcpy( pNewString, buf );
		pNewString[strlen( pNewString ) - 2] = '\0';  // This is some hacky shit right here.
		int newID = g_CompileCommands.AddToTail();
		g_CompileCommands[newID] = pNewString;
	}
	g_pFileSystem->Close( fp );

//	printf( "%d compiles\n", g_CompileCommands.Count() );
	DebugOut( "%d compiles\n", g_CompileCommands.Count() );
}

void SetupPaths( int argc, char **argv )
{
	GetTempPath( sizeof( g_WorkerTempPath ), g_WorkerTempPath );
	strcat( g_WorkerTempPath, "texturecompiletemp\\" );
	char tmp[MAX_PATH];
	sprintf( tmp, "rd /s /q \"%s\"", g_WorkerTempPath );
	system( tmp );
	_mkdir( g_WorkerTempPath );
//	printf( "g_WorkerTempPath: \"%s\"\n", g_WorkerTempPath );

	CommandLine()->CreateCmdLine( argc, argv );
	g_pGameDir = CommandLine()->ParmValue( "-gamedir", "" );
	strcpy( g_ExeDir, argv[0] );
	Q_StripFilename( g_ExeDir );
	Q_FixSlashes( g_ExeDir );
//	printf( "exedir: \"%s\"\n", g_ExeDir );

	g_pTextureOutputDir = CommandLine()->ParmValue( "-textureoutputdir", "" );
//	printf( "shaderoutputdir: \"%s\"\n", g_pShaderOutputDir );

	g_bVerbose = CommandLine()->FindParm("-verbose") != 0;
}

void SetupDebugFile( void )
{
#ifdef DEBUGFP
	const char *pComputerName = getenv( "COMPUTERNAME" );
	char filename[MAX_PATH];
	sprintf( filename, "\\\\fileserver\\user\\gary\\debug\\%s.txt", pComputerName );
	g_WorkerDebugFp = fopen( filename, "w" );
	Assert( g_WorkerDebugFp );
	DebugOut( "opened debug file\n" );
#endif
}

void WriteTexture( const char *pTextureName )
{
#if 0
	CUtlVector<CUtlBuffer> &byteCodeArray = g_ByteCode[pShaderName];
	const ShaderInfo_t &shaderInfo = g_ShaderToShaderInfo[pShaderName];
//	printf( "%s : %d combos centroid mask: 0x%x numDynamicCombos: %d flags: 0x%x\n", 
//		pShaderName, shaderInfo.m_nTotalShaderCombos, 
//		shaderInfo.m_CentroidMask, shaderInfo.m_nDynamicCombos, shaderInfo.m_Flags );
	CUtlBuffer header;
	CUtlBuffer body;
	header.PutInt( SHADER_VCS_VERSION_NUMBER ); // version
	header.PutInt( shaderInfo.m_nTotalShaderCombos );
	header.PutInt( shaderInfo.m_nDynamicCombos );
	header.PutUnsignedInt( shaderInfo.m_Flags );
	header.PutUnsignedInt( shaderInfo.m_CentroidMask );
	
	int headerSize = sizeof( int ) * 5;
	int directorySize = sizeof( int ) * 2 * shaderInfo.m_nTotalShaderCombos;
	int bodyOffset = headerSize + directorySize;
	int nCombo;
	for( nCombo = 0; nCombo < shaderInfo.m_nTotalShaderCombos; nCombo++ )
	{
		CUtlBuffer &byteCode = byteCodeArray[nCombo];
		if( byteCode.TellPut() == 0 )
		{
			// This is a skipped combo.
			header.PutInt( -1 );
			header.PutInt( 0 );
		}
		else
		{
			header.PutInt( body.TellPut() + bodyOffset );
			header.PutInt( byteCode.TellPut() );
			body.Put( byteCode.Base(), byteCode.TellPut() );
		}
	}

	char filename[MAX_PATH];
	char filename2[MAX_PATH];
//	strcpy( filename2, g_pShaderOutputDir );
	strcpy( filename2, g_pShaderPath );
	strcat( filename2, "\\shaders\\fxc" );

	struct	_stat buf;
	if( _stat( filename2, &buf ) == -1 )
	{
		printf( "mkdir %s\n", filename2 );
		// doh. . need to make the directory that the vcs file is going to go into.
		_mkdir( filename2 );
	}

	strcat( filename2, "\\" );
	strcpy( filename, pShaderName );
	char *dot = strstr( filename, "." );
	if( dot )
	{
		*dot = '\0';
	}
	strcat( filename, ".vcs" );
	strcat( filename2, filename );
	if( _stat( filename2, &buf ) != -1 )
	{
		// The file exists, let's see if it's writable.
		if( !( buf.st_mode & _S_IWRITE ) )
		{
			// It isn't writable. . we'd better change it's permissions (or check it out possibly).			
			printf( "Warning: making %s writable!\n", filename2 );
			_chmod( filename2, _S_IREAD | _S_IWRITE );
		}
	}
	FILE *fp = fopen( filename2, "wb" );
	if( !fp )
	{
		printf( "Can't open %s\n", filename2 );
		return;
	}
	printf( "writing %s\n", filename );
	fwrite( header.Base(), 1, headerSize + directorySize, fp );
	fwrite( body.Base(), 1, body.TellPut(), fp );
	fclose( fp );
#endif
}

void TouchFile( const char *path )
{
	Warning( "TouchFile: %s\n", path );
	char dir[MAX_PATH];
	Q_strcpy( dir, path );
	Q_StripFilename( dir );
	MakeDirHier( dir );
	FILE *fp = fopen( path, "wb" );
	fclose( fp );
}

int TextureCompile_Main( int argc, char* argv[] )
{
	InstallSpewFunction();
	g_bSuppressPrintfOutput = false;
	g_flStartTime = Plat_FloatTime();

	SetupDebugFile();
	numthreads = 1; // holy shit batman!
	SetupPaths( argc, argv );
	
	// Master, start accepting connections.
	// Worker, make a connection.
	DebugOut( "Before VMPI_Init\n" );
	g_bSuppressPrintfOutput = true;
	VMPIRunMode mode = VMPI_RUN_NETWORKED;
	if ( !VMPI_Init( argc, argv, "dependency_info_texturecompile.txt", MyDisconnectHandler, mode ) )
	{
		g_bSuppressPrintfOutput = false;
		DebugOut( "MPI_Init failed.\n" );
		Error( "MPI_Init failed." );
	}
	g_bSuppressPrintfOutput = false;

	DebugOut( "After VMPI_Init\n" );

	int maxFileSystemMemoryUsageBytes = 50000000;
	CmdLib_InitFileSystem( ".", maxFileSystemMemoryUsageBytes );
	
	DebugOut( "After VMPI_FileSystem_Init\n" );
	Shared_ParseListOfCompileCommands();
	DebugOut( "After Shared_ParseListOfCompileCommands\n" );

	// 4-ish work units per machine
	g_nTexturesPerWorkUnit = TEXTURES_PER_WORKUNIT;
	int nWorkUnits = g_CompileCommands.Count() / g_nTexturesPerWorkUnit + 1;

//	printf( "nWorkUnits: %d\n", nWorkUnits );
//	printf( "g_nShadersPerWorkUnit: %d\n", g_nShadersPerWorkUnit );

	DebugOut( "Before conditional\n" );
	if ( g_bMPIMaster )
	{
		// Send all of the workers the complete list of work to do.
		DebugOut( "Before STARTWORK_PACKETID\n" );

		char packetID = STARTWORK_PACKETID;
		VMPI_SendData( &packetID, sizeof( packetID ), VMPI_PERSISTENT );

		// nWorkUnits is how many work units. . .1000 is good.
		// The work unit number impies which combo to do.
		DebugOut( "Before DistributeWork\n" );
		DistributeWork( nWorkUnits, WORKUNIT_PACKETID, NULL, Master_ReceiveWorkUnitFn );
	}
	else
	{
		// wait until we get a packet from the master to start doing stuff.
		MessageBuffer buf;
		DebugOut( "Before VMPI_DispatchUntil\n" );
		while ( !g_bGotStartWorkPacket )
		{
			VMPI_DispatchNextMessage();
		}
		DebugOut( "after VMPI_DispatchUntil\n" );

		Worker_ReadFilesToCopy();
//		Worker_GetSourceFiles();
//		DebugOut( "Before Worker_GetLocalCopyOfShaders\n" );
//		Worker_GetLocalCopyOfTextureSource();
//		DebugOut( "Before Worker_GetLocalCopyOfBinaries\n" );
		DebugOut( "Before _chdir\n" );
		_chdir( g_WorkerTempPath );

		// DIE DIE KILL KILL AHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		char path[MAX_PATH];
		sprintf( path, "%s%s\\bin\\server.dll", g_WorkerTempPath, g_pGameDir + 3 ); // hack hack
		TouchFile( path );
		sprintf( path, "%s%s\\bin\\client.dll", g_WorkerTempPath, g_pGameDir + 3 );// hack hack
		TouchFile( path );

		Worker_GetLocalCopyOfBinaries();

		// nWorkUnits is how many work units. . .1000 is good.
		// The work unit number impies which combo to do.
		DebugOut( "Before DistributeWork\n" );

		DistributeWork( nWorkUnits, WORKUNIT_PACKETID, Worker_ProcessWorkUnitFn, NULL );
	}

	DebugOut( "Before VMPI_Finalize\n" );
	g_bSuppressPrintfOutput = true;
	VMPI_FileSystem_Term();
	VMPI_Finalize();
	g_bSuppressPrintfOutput = false;
	
	if( g_bMPIMaster )
	{
/*
		printf( "\n" );
		int nStrings = g_ByteCode.GetNumStrings();
		int i;
		for( i = 0; i < nStrings; i++ )
		{
			if( g_Master_TextureHadError.Defined( g_ByteCode.String( i ) ) )
			{
				printf( "FAILED: \"%s\"\n", g_ByteCode.String( i ) );
			}
			else
			{
//				printf( "\"%s\" succeeded!\n", g_ByteCode.String( i ) );
				WriteTexture( g_ByteCode.String( i ) );
			}
		}
		
		double end = Plat_FloatTime();
		
		char str[512];
		GetHourMinuteSecondsString( (int)( end - g_flStartTime ), str, sizeof( str ) );
		Msg( "%s elapsed\n", str );
*/
	}
	return 0;
}

class CTextureCompileDLL : public ILaunchableDLL
{
	int main( int argc, char **argv );
};

int CTextureCompileDLL::main( int argc, char **argv )
{
	return TextureCompile_Main( argc, argv );
}

EXPOSE_SINGLE_INTERFACE( CTextureCompileDLL, ILaunchableDLL, LAUNCHABLE_DLL_INTERFACE_VERSION );
