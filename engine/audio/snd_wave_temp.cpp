//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Create an output wave stream.  Used to record audio for in-engine movies or
// mixer debugging.
//
//=====================================================================================//

#include "audio_pch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IFileSystem *g_pFileSystem;
// FIXME: shouldn't this API be part of IFileSystem?
extern bool COM_CopyFile( const char *netpath, const char *cachepath );

// Create a wave file
void WaveCreateTmpFile( const char *filename, int rate, int bits, int nChannels )
{
	char tmpfilename[MAX_PATH];
	Q_StripExtension( filename, tmpfilename, sizeof( tmpfilename ) );
	Q_DefaultExtension( tmpfilename, ".WAV", sizeof( tmpfilename ) );

	FileHandle_t file;
	file = g_pFileSystem->Open( tmpfilename, "wb" );
	if ( file == FILESYSTEM_INVALID_HANDLE )
		return;

	int chunkid = LittleLong( RIFF_ID );
	int chunksize = LittleLong( 0 );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );
	g_pFileSystem->Write( &chunksize, sizeof(int), file );

	chunkid = LittleLong( RIFF_WAVE );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );

	// create a 16-bit PCM stereo output file
	PCMWAVEFORMAT fmt = { { 0 } };
	fmt.wf.wFormatTag = LittleWord( (short)WAVE_FORMAT_PCM );
	fmt.wf.nChannels = LittleWord( (short)nChannels );
	fmt.wf.nSamplesPerSec = LittleDWord( rate );
	fmt.wf.nAvgBytesPerSec = LittleDWord( rate * bits * nChannels / 8 );
	fmt.wf.nBlockAlign = LittleWord( (short)( 2 * nChannels) );
	fmt.wBitsPerSample = LittleWord( (short)bits );

	chunkid = LittleLong( WAVE_FMT );
	chunksize = LittleLong( sizeof(fmt) );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );
	g_pFileSystem->Write( &chunksize, sizeof(int), file );
	g_pFileSystem->Write( &fmt, sizeof( PCMWAVEFORMAT ), file );

	chunkid = LittleLong( WAVE_DATA );
	chunksize = LittleLong( 0 );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );
	g_pFileSystem->Write( &chunksize, sizeof(int), file );

	g_pFileSystem->Close( file );
}

void WaveAppendTmpFile( const char *filename, void *pBuffer, int sampleBits, int numSamples )
{
	char tmpfilename[MAX_PATH];
	Q_StripExtension( filename, tmpfilename, sizeof( tmpfilename ) );
	Q_DefaultExtension( tmpfilename, ".WAV", sizeof( tmpfilename ) );

	FileHandle_t file;
	file = g_pFileSystem->Open( tmpfilename, "r+b" );
	if ( file == FILESYSTEM_INVALID_HANDLE )
		return;

	g_pFileSystem->Seek( file, 0, FILESYSTEM_SEEK_TAIL );

	if ( IsX360() && sampleBits == 16 )
	{
		short *pSwapped = (short * )_alloca( numSamples * sampleBits/8 );
		for ( int i=0; i<numSamples; i++ )
		{
			pSwapped[i] = LittleShort( ((short*)pBuffer)[i] );
		}
		g_pFileSystem->Write( pSwapped, numSamples * sizeof( short ), file );
	}
	else
	{
		g_pFileSystem->Write( pBuffer, numSamples * sampleBits/8, file );
	}

	g_pFileSystem->Close( file );
}

void WaveFixupTmpFile( const char *filename )
{
	char tmpfilename[MAX_PATH];
	Q_StripExtension( filename, tmpfilename, sizeof( tmpfilename ) );
	Q_DefaultExtension( tmpfilename, ".WAV", sizeof( tmpfilename ) );

	FileHandle_t file;
	file = g_pFileSystem->Open( tmpfilename, "r+b" );
	if ( FILESYSTEM_INVALID_HANDLE == file )
	{
		Warning( "WaveFixupTmpFile( '%s' ) failed to open file for editing\n", tmpfilename );
		return;
	}
	
	// file size goes in RIFF chunk
	int size = g_pFileSystem->Size( file ) - 2*sizeof( int );
	// offset to data chunk
	int headerSize = (sizeof(int)*5 + sizeof(PCMWAVEFORMAT));
	// size of data chunk
	int dataSize = size - headerSize;

	size = LittleLong( size );
	g_pFileSystem->Seek( file, sizeof( int ), FILESYSTEM_SEEK_HEAD );
	g_pFileSystem->Write( &size, sizeof( int ), file );

	// skip the header and the 4-byte chunk tag and write the size
	dataSize = LittleLong( dataSize );
	g_pFileSystem->Seek( file, headerSize+sizeof( int ), FILESYSTEM_SEEK_HEAD );
	g_pFileSystem->Write( &dataSize, sizeof( int ), file );

	g_pFileSystem->Close( file );
}

CON_COMMAND( movie_fixwave, "Fixup corrupted .wav file if engine crashed during startmovie/endmovie, etc." )
{
	if ( args.ArgC() != 2 )
	{
		Msg ("Usage: movie_fixwave wavname\n");
		return;
	}

	char const *wavname = args.Arg( 1 );
	if ( !g_pFileSystem->FileExists( wavname ) )
	{
		Warning( "movie_fixwave: File '%s' does not exist\n", wavname );
		return;
	}

	char tmpfilename[256];
	Q_StripExtension( wavname, tmpfilename, sizeof( tmpfilename ) );
	Q_strncat( tmpfilename, "_fixed", sizeof( tmpfilename ), COPY_ALL_CHARACTERS );
	Q_DefaultExtension( tmpfilename, ".wav", sizeof( tmpfilename ) );

	// Now copy the file
	Msg( "Copying '%s' to '%s'\n", wavname, tmpfilename );
	COM_CopyFile( wavname, tmpfilename );

	Msg( "Performing fixup on '%s'\n", tmpfilename );
	WaveFixupTmpFile( tmpfilename );
}
