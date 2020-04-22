//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef COMMON_H
#define COMMON_H
#pragma once

#ifndef WORLDSIZE_H
#include "worldsize.h"
#endif

#include "basetypes.h"
#include "filesystem.h"
#include "mathlib/vector.h" // @Note (toml 05-01-02): solely for definition of QAngle
#include "qlimits.h"
#define INCLUDED_STEAM2_USERID_STRUCTS	
#include "steamcommon.h"
#include "steam/steamclientpublic.h"


class Vector;
struct cache_user_t;

//============================================================================

#define COM_COPY_CHUNK_SIZE 1024   // For copying operations

#ifndef NULL
#define NULL ((void *)0)
#endif

#include "tier1/strtools.h"

//============================================================================
char *COM_StringCopy(const char *text);	// allocates memory and copys text
void COM_StringFree(const char *text);	// frees memory allocated by COM_StringCopy
void COM_AddNoise( unsigned char *data, int length, int number ); // Changes n random bits in a data block

//============================================================================
extern void COM_WriteFile (const char *filename, void *data, int len);
extern int COM_OpenFile( const char *filename, FileHandle_t* file );
extern void COM_CloseFile( FileHandle_t hFile );
extern void COM_CreatePath (const char *path);
extern int COM_FileSize (const char *filename);
extern int COM_ExpandFilename (char *filename, int maxlength);
extern byte *COM_LoadFile (const char *path, int usehunk, int *pLength);
extern bool COM_IsValidPath( const char *pszFilename );
extern bool COM_IsValidLogFilename( const char *pszFilename );

const char *COM_Parse (const char *data);
const char *COM_ParseLine (const char *data);
int COM_TokenWaiting( const char *buffer );

extern bool com_ignorecolons;
extern	char		com_token[1024];

void COM_Init (void);
void COM_Shutdown( void );
bool COM_CheckGameDirectory( const char *gamedir );
void COM_ParseDirectoryFromCmd( const char *pCmdName, char *pDirName, int maxlen, const char *pDefault );

#define Bits2Bytes(b) ((b+7)>>3)

// returns a temp buffer of at least 512 bytes 
char	*tmpstr512();
// does a varargs printf into a temp buffer.
// Returns char* because of bad historical reasons.
char	*va(PRINTF_FORMAT_STRING const char *format, ...) FMTFUNCTION( 1, 2 );
// prints a vector into a temp buffer.
const char    *vstr(Vector& v);

//============================================================================
extern	char	com_basedir[MAX_OSPATH];
extern	char	com_gamedir[MAX_OSPATH];

byte *COM_LoadStackFile (const char *path, void *buffer, int bufsize, int& filesize );
void COM_LoadCacheFile (const char *path, cache_user_t *cu);
byte* COM_LoadFile(const char *path, int usehunk, int *pLength);

void COM_CopyFileChunk( FileHandle_t dst, FileHandle_t src, int nSize );
bool COM_CopyFile( const char *pSourcePath, const char *pDestPath );

void COM_SetupLogDir( const char *mapname );
void COM_GetGameDir(char *szGameDir, int maxlen);
int COM_CompareFileTime(const char *filename1, const char *filename2, int *iCompare);
int COM_GetFileTime( const char *pFileName );
const char *COM_ParseFile(const char *data, char *token, int maxtoken);

extern char gszDisconnectReason[256];
extern char gszExtendedDisconnectReason[256];
extern bool gfExtendedError;
extern uint8 g_eSteamLoginFailure;
void COM_ExplainDisconnection( bool bPrint, PRINTF_FORMAT_STRING const char *fmt, ... );

const char *COM_DXLevelToString( int dxlevel );  // convert DX level to string

void COM_Log( const char *pszFile, PRINTF_FORMAT_STRING const char *fmt, ...) FMTFUNCTION( 2, 3 ); // Log a debug message to specified file ( if pszFile == NULL uses c:\\hllog.txt )
void COM_LogString( char const *pchFile, char const *pchString );

const char *COM_FormatSeconds( int seconds ); // returns seconds as hh:mm:ss string

const char *COM_GetModDirectory(); // return the mod dir (rather than the complete -game param, which can be a path)

void *COM_CompressBuffer_LZSS( const void *source, unsigned int sourceLen, unsigned int *compressedLen, unsigned int maxCompressedLen = 0 );
bool COM_BufferToBufferCompress_LZSS( void *dest, unsigned int *destLen, const void *source, unsigned int sourceLen );
unsigned int COM_GetIdealDestinationCompressionBufferSize_LZSS( unsigned int uncompressedSize );

void *COM_CompressBuffer_Snappy( const void *source, unsigned int sourceLen, unsigned int *compressedLen, unsigned int maxCompressedLen = 0 );
bool COM_BufferToBufferCompress_Snappy( void *dest, unsigned int *destLen, const void *source, unsigned int sourceLen );
unsigned int COM_GetIdealDestinationCompressionBufferSize_Snappy( unsigned int uncompressedSize );

/// Fetch ideal working buffer size.  You should allocate the buffer you wish to compress into
/// at least this big, in order to get the best performance when using COM_BufferToBufferCompress
inline unsigned int COM_GetIdealDestinationCompressionBufferSize( unsigned int uncompressedSize )
{
	return COM_GetIdealDestinationCompressionBufferSize_LZSS( uncompressedSize );
}

/// Compress the source data into a newly allocated buffer.  Returns the buffer and its
/// size.  Note that the buffer may have been allocated to a larger size than necessary,
/// and the compressed size may be larger than the size of the input!
///
/// If maxCompressedLen is nonzero, then we will fail compression if the compressed data
/// exceeds this size.  Depending on the compressor used, we might be able to terminate
/// early in this case
inline void *COM_CompressBuffer( const void *source, unsigned int sourceLen, unsigned int *compressedLen, unsigned int maxCompressedLen = 0 )
{
	return COM_CompressBuffer_LZSS( source, sourceLen, compressedLen, maxCompressedLen );
}

/// Compress data to the specified buffer.  Returns false if compression fails or the data cannot fit into
/// the specified buffer.  If false is returned, the destination buffer and size field are not modified.
/// (Note that this differs from previous behaviour.)
inline bool COM_BufferToBufferCompress( void *dest, unsigned int *destLen, const void *source, unsigned int sourceLen )
{
	return COM_BufferToBufferCompress_LZSS( dest, destLen, source, sourceLen );
}

/// Returns true if compression succeeded, false otherwise
bool COM_BufferToBufferDecompress( void *dest, unsigned int *destLen, const void *source, unsigned int sourceLen );

/// Fetch size of the decompressed data in a buffer that was created using COM_BufferToBufferCompress.
/// Returns -1 if buffer does not appear to be compressed.
int COM_GetUncompressedSize( const void *compressed, unsigned int compressedLen );

#endif // COMMON_H
