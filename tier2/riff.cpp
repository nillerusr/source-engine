//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// Avoid these warnings:
#pragma warning(disable : 4512) // warning C4512: 'InFileRIFF' : assignment operator could not be generated
#pragma warning(disable : 4514) // warning C4514: 'RIFFName' : unreferenced inline function has been removed

#include "riff.h"
#include <stdio.h>
#include <string.h>
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if 0
//-----------------------------------------------------------------------------
// Purpose: Test code that implements the interface on stdio
//-----------------------------------------------------------------------------
class StdIOReadBinary : public IFileReadBinary
{
public:
	int open( const char *pFileName )
	{
		return (int)fopen( pFileName, "rb" );
	}

	int read( void *pOutput, int size, int file )
	{
		FILE *fp = (FILE *)file;

		return fread( pOutput, size, 1, fp );
	}

	void seek( int file, int pos )
	{
		fseek( (FILE *)file, pos, SEEK_SET );
	}

	unsigned int tell( int file )
	{
		return ftell( (FILE *)file );
	}

	unsigned int size( int file )
	{
		FILE *fp = (FILE *)file;
		if ( !fp )
			return 0;

		unsigned int pos = ftell( fp );
		fseek( fp, 0, SEEK_END );
		unsigned int size = ftell( fp );

		fseek( fp, pos, SEEK_SET );
		return size;
	}

	void close( int file )
	{
		FILE *fp = (FILE *)file;

		fclose( fp );
	}
};
#endif


#define RIFF_ID MAKEID('R','I','F','F')


//-----------------------------------------------------------------------------
// Purpose: Opens a RIFF file using the given I/O mechanism
// Input  : *pFileName 
//			&io - I/O interface
//-----------------------------------------------------------------------------
InFileRIFF::InFileRIFF( const char *pFileName, IFileReadBinary &io ) : m_io(io)
{
	m_file = m_io.open( pFileName );
	
	int riff = 0;
	if ( !m_file )
	{
		m_riffSize = 0;
		m_riffName = 0;
		return;
	}

	riff = ReadInt();
	if ( riff != RIFF_ID )
	{
		printf( "Not a RIFF File [%s]\n", pFileName );
		m_riffSize = 0;
	}
	else
	{
		// we store size as size of all chunks
		// subtract off the RIFF form type (e.g. 'WAVE', 4 bytes)
		m_riffSize = ReadInt() - 4;
		m_riffName = ReadInt();

		// HACKHACK: LWV files don't obey the RIFF format!!!
		// Do this or miss the linguistic chunks at the end. Lame!
		// subtract off 12 bytes for (RIFF, size, WAVE)
		m_riffSize = m_io.size( m_file ) - 12;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Close the file
//-----------------------------------------------------------------------------
InFileRIFF::~InFileRIFF( void )
{
	m_io.close( m_file );
}


//-----------------------------------------------------------------------------
// Purpose: read a 4-byte int out of the stream
// Output : int = read value, default is zero
//-----------------------------------------------------------------------------
int InFileRIFF::ReadInt( void )
{
	int tmp = 0;
	m_io.read( &tmp, sizeof(int), m_file );
	tmp = LittleLong( tmp );

	return tmp;
}

//-----------------------------------------------------------------------------
// Purpose: Read a block of binary data
// Input  : *pOutput - pointer to destination memory
//			dataSize - size of block to read
// Output : int - number of bytes read
//-----------------------------------------------------------------------------
int InFileRIFF::ReadData( void *pOutput, int dataSize )
{
	int count = m_io.read( pOutput, dataSize, m_file );

	return count;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the file position
// Output : int (bytes from start of file)
//-----------------------------------------------------------------------------
int InFileRIFF::PositionGet( void )
{
	return m_io.tell( m_file );
}


//-----------------------------------------------------------------------------
// Purpose: Seek to file position
// Input  : position - bytes from start of file
//-----------------------------------------------------------------------------
void InFileRIFF::PositionSet( int position )
{
	m_io.seek( m_file, position );
}

//-----------------------------------------------------------------------------
// Purpose: Used to write a RIFF format file
//-----------------------------------------------------------------------------
OutFileRIFF::OutFileRIFF( const char *pFileName, IFileWriteBinary &io ) : m_io( io )
{
	m_file = m_io.create( pFileName );

	if ( !m_file )
		return;

	int riff = RIFF_ID;
	m_io.write( &riff, 4, m_file );

	m_riffSize = 0;
	m_nNamePos = m_io.tell( m_file );

	// Save room for the size and name now
	WriteInt( 0 );

	// Write out the name
	WriteInt( RIFF_WAVE );

	m_bUseIncorrectLISETLength = false;
	m_nLISETSize = 0;
}

OutFileRIFF::~OutFileRIFF( void )
{
	if ( !IsValid() )
		return;

	unsigned int size = m_io.tell( m_file ) -8;
	m_io.seek( m_file, m_nNamePos );

	if ( m_bUseIncorrectLISETLength )
	{
		size = m_nLISETSize - 8;
	}

	WriteInt( size );
	m_io.close( m_file );
}

void OutFileRIFF::HasLISETData( int position )
{
	m_bUseIncorrectLISETLength = true;
	m_nLISETSize = position;
}

bool OutFileRIFF::WriteInt( int number )
{
	if ( !IsValid() )
		return false;

	m_io.write( &number, sizeof( int ), m_file );
	return true;
}

bool OutFileRIFF::WriteData( void *pOutput, int dataSize )
{
	if ( !IsValid() )
		return false;

	m_io.write( pOutput, dataSize, m_file );
	return true;
}

int OutFileRIFF::PositionGet( void )
{
	if ( !IsValid() )
		return 0;

	return m_io.tell( m_file );
}

void OutFileRIFF::PositionSet( int position )
{
	if ( !IsValid() )
		return;

	m_io.seek( m_file, position );
}


//-----------------------------------------------------------------------------
// Purpose: Create an iterator for the given file
// Input  : &riff - riff file
//			size - size of file or sub-chunk
//-----------------------------------------------------------------------------
IterateRIFF::IterateRIFF( InFileRIFF &riff, int size )
	: m_riff(riff), m_size(size)
{
	if ( !m_riff.RIFFSize() )
	{
		// bad file, just be an empty iterator
		ChunkClear();
		return;
	}

	// get the position and parse a chunk
	m_start = riff.PositionGet();
	ChunkSetup();
}


//-----------------------------------------------------------------------------
// Purpose: Set up a sub-chunk iterator
// Input  : &parent - parent iterator
//-----------------------------------------------------------------------------
IterateRIFF::IterateRIFF( IterateRIFF &parent )
	: m_riff(parent.m_riff), m_size(parent.ChunkSize())
{
	m_start = parent.ChunkFilePosition();
	ChunkSetup();
}

//-----------------------------------------------------------------------------
// Purpose: Parse the chunk at the current file position 
//			This object will iterate over the sub-chunks of this chunk.
//			This makes for easy hierarchical parsing
//-----------------------------------------------------------------------------
void IterateRIFF::ChunkSetup( void )
{
	m_chunkPosition = m_riff.PositionGet();

	m_chunkName = m_riff.ReadInt();
	m_chunkSize = m_riff.ReadInt();
}

//-----------------------------------------------------------------------------
// Purpose: clear chunk setup, ChunkAvailable will return false
//-----------------------------------------------------------------------------
void IterateRIFF::ChunkClear( void )
{
	m_chunkSize = -1;
}

//-----------------------------------------------------------------------------
// Purpose: If there are chunks left to read beyond this one, return true
//-----------------------------------------------------------------------------
bool IterateRIFF::ChunkAvailable( void )
{
	if ( m_chunkSize != -1 && m_chunkSize < 0x10000000 )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Go to the next chunk in the file, return true if there is one.
//-----------------------------------------------------------------------------
bool IterateRIFF::ChunkNext( void )
{
	if ( !ChunkAvailable() )
		return false;

	int nextPos = m_chunkPosition + 8 + m_chunkSize;
	
	// chunks are aligned
	nextPos += m_chunkSize & 1;

	if ( nextPos >= (m_start + m_size) )
	{
		ChunkClear();
		return false;
	}

	m_riff.PositionSet( nextPos );

	ChunkSetup();
	return ChunkAvailable();

}


//-----------------------------------------------------------------------------
// Purpose: get the chunk FOURCC as an int
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int IterateRIFF::ChunkName( void )
{
	return m_chunkName;
}


//-----------------------------------------------------------------------------
// Purpose: get the size of this chunk
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int IterateRIFF::ChunkSize( void )
{
	return m_chunkSize;
}

//-----------------------------------------------------------------------------
// Purpose: Read the entire chunk into a buffer
// Input  : *pOutput - dest buffer
// Output : int bytes read
//-----------------------------------------------------------------------------
int IterateRIFF::ChunkRead( void *pOutput )
{
	return m_riff.ReadData( pOutput, ChunkSize() );
}


//-----------------------------------------------------------------------------
// Purpose: Read a partial chunk (updates file position for subsequent partial reads).
// Input  : *pOutput - dest buffer
//			dataSize - partial size
// Output : int - bytes read
//-----------------------------------------------------------------------------
int IterateRIFF::ChunkReadPartial( void *pOutput, int dataSize )
{
	return m_riff.ReadData( pOutput, dataSize );
}


//-----------------------------------------------------------------------------
// Purpose: Read a 4-byte int
// Output : int - read int
//-----------------------------------------------------------------------------
int IterateRIFF::ChunkReadInt( void )
{
	return m_riff.ReadInt();
}

//-----------------------------------------------------------------------------
// Purpose: Used to iterate over an InFileRIFF
//-----------------------------------------------------------------------------
IterateOutputRIFF::IterateOutputRIFF( OutFileRIFF &riff )
: m_riff( riff )
{
	if ( !m_riff.IsValid() )
		return;

	m_start = m_riff.PositionGet();
	m_chunkPosition = m_start;
	m_chunkStart = -1;
}

IterateOutputRIFF::IterateOutputRIFF( IterateOutputRIFF &parent )
	: m_riff(parent.m_riff)
{
	m_start = parent.ChunkFilePosition();
	m_chunkPosition = m_start;
	m_chunkStart = -1;
}

void IterateOutputRIFF::ChunkWrite( unsigned int chunkname, void *pOutput, int size )
{
	m_chunkPosition = m_riff.PositionGet();

	m_chunkName = chunkname;
	m_chunkSize = size;

	m_riff.WriteInt( chunkname );
	m_riff.WriteInt( size );
	m_riff.WriteData( pOutput, size );

	m_chunkPosition = m_riff.PositionGet();
	
	m_chunkPosition += m_chunkPosition & 1;

	m_riff.PositionSet( m_chunkPosition );

	m_chunkStart = -1;
}

void IterateOutputRIFF::ChunkWriteInt( int number )
{
	m_riff.WriteInt( number );
}

void IterateOutputRIFF::ChunkWriteData( void *pOutput, int size )
{
	m_riff.WriteData( pOutput, size );
}

void IterateOutputRIFF::ChunkFinish( void )
{
	Assert( m_chunkStart != -1 );

	m_chunkPosition = m_riff.PositionGet();
	
	int size = m_chunkPosition - m_chunkStart - 8;

	m_chunkPosition += m_chunkPosition & 1;

	m_riff.PositionSet( m_chunkStart + sizeof( int ) );

	m_riff.WriteInt( size );

	m_riff.PositionSet( m_chunkPosition );

	m_chunkStart = -1;
}

void IterateOutputRIFF::ChunkStart( unsigned int chunkname )
{
	Assert( m_chunkStart == -1 );

	m_chunkStart = m_riff.PositionGet();

	m_riff.WriteInt( chunkname );
	m_riff.WriteInt( 0 );
}

void IterateOutputRIFF::ChunkSetPosition( int position )
{
	m_riff.PositionSet( position );
}

unsigned int IterateOutputRIFF::ChunkGetPosition( void )
{
	return m_riff.PositionGet();
}

void IterateOutputRIFF::CopyChunkData( IterateRIFF& input )
{
	if (  input.ChunkSize() > 0 )
	{
		char *buffer = new char[ input.ChunkSize() ];
		Assert( buffer );

		input.ChunkRead( buffer );

		// Don't copy/write the name or size, just the data itself
		ChunkWriteData( buffer, input.ChunkSize() );

		delete[] buffer;
	}
}

void IterateOutputRIFF::SetLISETData( int position )
{
	m_riff.HasLISETData( position );
}