//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "stdafx.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#elif _LINUX
#define stricmp strcasecmp
#endif
#include "tier1/strtools.h"
#include "tier0/dbg.h"
#include "KeyValues.h"
#include "cmdlib.h"
#include "tier0/icommandline.h"
#include "vcprojconvert.h"
#include "makefilecreator.h"

SpewRetval_t SpewFunc( SpewType_t type, char const *pMsg )
{	
	printf( "%s", pMsg );
#ifdef _WIN32
	OutputDebugString( pMsg );
#endif
	
	if ( type == SPEW_ERROR )
	{
		printf( "\n" );
#ifdef _WIN32
		OutputDebugString( "\n" );
#endif
	}
	else if (type == SPEW_ASSERT)
	{
		return SPEW_DEBUGGER;
	}

	return SPEW_CONTINUE;
}

class MyFileSystem : public IBaseFileSystem 
{
public:
	int Read( void* pOutput, int size, FileHandle_t file ) { return fread( pOutput, 1, size, (FILE *)file); }
	int Write( void const* pInput, int size, FileHandle_t file ) { return fwrite( pInput, 1, size, (FILE *)file); }
	FileHandle_t Open( const char *pFileName, const char *pOptions, const char *pathID = 0 ) { return (FileHandle_t)fopen( pFileName, pOptions); }
	void Close( FileHandle_t file ) { fclose( (FILE *)file ); }
	void Seek( FileHandle_t file, int pos, FileSystemSeek_t seekType ) {}
	unsigned int Tell( FileHandle_t file ) { return 0;}
	unsigned int Size( FileHandle_t file ) { return 0;}
	unsigned int Size( const char *pFileName, const char *pPathID = 0 ) { return 0; }
	void Flush( FileHandle_t file ) { fflush((FILE *)file); }
	bool Precache( const char *pFileName, const char *pPathID = 0 ) {return false;}
	bool FileExists( const char *pFileName, const char *pPathID = 0 ) {return false;}
	bool IsFileWritable( char const *pFileName, const char *pPathID = 0 ) {return false;}
	bool SetFileWritable( char const *pFileName, bool writable, const char *pPathID = 0 ) {return false;}
	long GetFileTime( const char *pFileName, const char *pPathID = 0 ) { return 0; }
	bool ReadFile( const char *pFileName, const char *pPath, CUtlBuffer &buf, int nMaxBytes = 0, int nStartingByte = 0, FSAllocFunc_t pfnAlloc = NULL ) {return false;}
	bool WriteFile( const char *pFileName, const char *pPath, CUtlBuffer &buf ) {return false;}
	bool UnzipFile( const char *,const char *,const char * ) {return false;}
};

MyFileSystem g_MyFS;
IBaseFileSystem *g_pFileSystem = &g_MyFS;

//-----------------------------------------------------------------------------
// Purpose: help text
//-----------------------------------------------------------------------------
void printusage( void )
{
	Msg( "usage:  vcprojtomake <vcproj filename> \n" );
}

//-----------------------------------------------------------------------------
// Purpose: debug helper, spits out a human readable keyvalues version of the various configs
//-----------------------------------------------------------------------------
void OutputKeyValuesVersion( CVCProjConvert & proj )
{
	KeyValues *kv = new KeyValues( "project" );
	for ( int projIndex = 0; projIndex < proj.GetNumConfigurations(); projIndex++ )
	{
		CVCProjConvert::CConfiguration & config = proj.GetConfiguration(projIndex);
		KeyValues *configKv = kv->FindKey( config.GetName().String(), true );
		int fileCount = 0;
		for( int fileIndex = 0; fileIndex < config.GetNumFileNames(); fileIndex++ )
		{
			if ( config.GetFileType(fileIndex) == CVCProjConvert::CConfiguration::FILE_SOURCE ) 
			{
				char num[20];
				Q_snprintf( num, sizeof(num), "%i", fileCount );
				fileCount++;
				configKv->SetString( num, config.GetFileName(fileIndex) );
			}
		}
	}
	kv->SaveToFile( g_pFileSystem, "files.vdf" );
	kv->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : argc - 
//			argv[] - 
// Output : int
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	SpewOutputFunc( SpewFunc );

	Msg( "Valve Software - vcprojtomake.exe (%s)\n", __DATE__ );
	CommandLine()->CreateCmdLine( argc, argv );

	if ( CommandLine()->ParmCount() < 2)
	{
		printusage();
		return 0;
	}

	CVCProjConvert proj;
	if ( !proj.LoadProject( CommandLine()->GetParm( 1 )) )
	{
		Msg( "Failed to parse project\n" );
		return -1;
	}

	OutputKeyValuesVersion(proj);

	CMakefileCreator makefile;
	makefile.CreateMakefiles( proj );
	return 0;
}
