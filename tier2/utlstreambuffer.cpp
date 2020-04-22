//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// Serialization/unserialization buffer
//=============================================================================//


#include "tier2/utlstreambuffer.h"
#include "tier2/tier2.h"
#include "filesystem.h"


//-----------------------------------------------------------------------------
// default stream chunk size
//-----------------------------------------------------------------------------
enum
{
	DEFAULT_STREAM_CHUNK_SIZE = 16 * 1024
};


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CUtlStreamBuffer::CUtlStreamBuffer( ) : BaseClass( DEFAULT_STREAM_CHUNK_SIZE, DEFAULT_STREAM_CHUNK_SIZE, 0 )
{
	SetUtlBufferOverflowFuncs( &CUtlStreamBuffer::StreamGetOverflow, &CUtlStreamBuffer::StreamPutOverflow );
	m_hFileHandle = FILESYSTEM_INVALID_HANDLE;
	m_pFileName = NULL;
	m_pPath = NULL;
}

CUtlStreamBuffer::CUtlStreamBuffer( const char *pFileName, const char *pPath, int nFlags, bool bDelayOpen ) :
	BaseClass( DEFAULT_STREAM_CHUNK_SIZE, DEFAULT_STREAM_CHUNK_SIZE, nFlags )
{
	SetUtlBufferOverflowFuncs( &CUtlStreamBuffer::StreamGetOverflow, &CUtlStreamBuffer::StreamPutOverflow );

	if ( bDelayOpen )
	{
		m_pFileName = V_strdup( pFileName );

		if ( pPath )
		{
			int nPathLen = Q_strlen( pPath );
			m_pPath = new char[ nPathLen + 1 ];
			Q_strcpy( m_pPath, pPath );
		}
		else
		{
			m_pPath = new char[ 1 ];
			m_pPath[0] = 0;
		}

		m_hFileHandle = FILESYSTEM_INVALID_HANDLE;
	}
	else
	{
		m_pFileName = NULL;
		m_pPath = NULL;
		m_hFileHandle = OpenFile( pFileName, pPath );
		if ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			return;
		}
	}

	if ( IsReadOnly() )
	{
		// NOTE: MaxPut may not actually be this exact size for text files;
		// it could be slightly less owing to the /r/n -> /n conversion
		m_nMaxPut = g_pFullFileSystem->Size( m_hFileHandle );

		// Read in the first bytes of the file
		if ( Size() > 0 )
		{
			int nSizeToRead = min( Size(), m_nMaxPut );
			ReadBytesFromFile( nSizeToRead, 0 );
		}
	}
}


void CUtlStreamBuffer::Close()
{
	if ( !IsReadOnly() )
	{
		// Write the final bytes
		int nBytesToWrite = TellPut() - m_nOffset;
		if ( nBytesToWrite > 0 )
		{
			if ( ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE ) && m_pFileName )
			{
				m_hFileHandle = OpenFile( m_pFileName, m_pPath );
				if( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
				{
					Error( "CUtlStreamBuffer::Close() Unable to open file %s!\n", m_pFileName );
				}
			}
			if ( m_hFileHandle != FILESYSTEM_INVALID_HANDLE )
			{
				if ( g_pFullFileSystem )
				{
					int nBytesWritten = g_pFullFileSystem->Write( Base(), nBytesToWrite, m_hFileHandle );
					if( nBytesWritten != nBytesToWrite )
					{
						Error( "CUtlStreamBuffer::Close() Write %s failed %d != %d.\n", m_pFileName, nBytesWritten, nBytesToWrite );
					}
				}
			}
		}
	}

	if ( m_hFileHandle != FILESYSTEM_INVALID_HANDLE )
	{
		if ( g_pFullFileSystem )
			g_pFullFileSystem->Close( m_hFileHandle );
		m_hFileHandle = FILESYSTEM_INVALID_HANDLE;
	}

	if ( m_pFileName )
	{
		delete[] m_pFileName;
		m_pFileName = NULL;
	}

	if ( m_pPath )
	{
		delete[] m_pPath;
		m_pPath = NULL;
	}

	m_Error = 0;
}

CUtlStreamBuffer::~CUtlStreamBuffer()
{
	Close();
}


//-----------------------------------------------------------------------------
// Open the file. normally done in constructor
//-----------------------------------------------------------------------------
void CUtlStreamBuffer::Open( const char *pFileName, const char *pPath, int nFlags )
{
	if ( IsOpen() )
	{
		Close();
	}

	m_Get = 0;
	m_Put = 0;
	m_nTab = 0;
	m_nOffset = 0;
	m_Flags = nFlags;
	m_hFileHandle = OpenFile( pFileName, pPath );
	if ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
		return;

	if ( IsReadOnly() )
	{
		// NOTE: MaxPut may not actually be this exact size for text files;
		// it could be slightly less owing to the /r/n -> /n conversion
		m_nMaxPut = g_pFullFileSystem->Size( m_hFileHandle );

		// Read in the first bytes of the file
		if ( Size() > 0 )
		{
			int nSizeToRead = min( Size(), m_nMaxPut );
			ReadBytesFromFile( nSizeToRead, 0 );
		}
	}
	else
	{
		if ( m_Memory.NumAllocated() != 0 )
		{
			m_nMaxPut = -1;
			AddNullTermination();
		}
		else
		{
			m_nMaxPut = 0;
		}
	}
}


//-----------------------------------------------------------------------------
// Is the file open?
//-----------------------------------------------------------------------------
bool CUtlStreamBuffer::IsOpen() const
{
	if ( m_hFileHandle != FILESYSTEM_INVALID_HANDLE )
		return true;

	// Delayed open case
	return ( m_pFileName != 0 );
}


//-----------------------------------------------------------------------------
// Grow allocation size to fit requested size
//-----------------------------------------------------------------------------
void CUtlStreamBuffer::GrowAllocatedSize( int nSize )
{
	int nNewSize = Size();
	if ( nNewSize < nSize + 1 )
	{
		while ( nNewSize < nSize + 1 )
		{
			nNewSize += DEFAULT_STREAM_CHUNK_SIZE; 
		}
		m_Memory.Grow( nNewSize - Size() );
	}
}


//-----------------------------------------------------------------------------
// Load up more of the stream when we overflow
//-----------------------------------------------------------------------------
bool CUtlStreamBuffer::StreamPutOverflow( int nSize )
{
	if ( !IsValid() || IsReadOnly() )
		return false;

	// Make sure the allocated size is at least as big as the requested size
	if ( nSize > 0 )
	{
		GrowAllocatedSize( nSize + 2 );
	}

	// Don't write the last byte (for NULL termination logic to work)
	int nBytesToWrite = TellPut() - m_nOffset - 1;
	if ( ( nBytesToWrite > 0 ) || ( nSize < 0 ) )
	{
		if ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			m_hFileHandle = OpenFile( m_pFileName, m_pPath );
			if( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
				return false;
		}
	}

	if ( nBytesToWrite > 0 )
	{
		int nBytesWritten = g_pFullFileSystem->Write( Base(), nBytesToWrite, m_hFileHandle );
		if ( nBytesWritten != nBytesToWrite )
		{
			m_Error	|= FILE_WRITE_ERROR;
			return false;
		}

		// This is necessary to deal with auto-NULL terminiation
		m_Memory[0] = *(unsigned char*)PeekPut( -1 );
		if ( TellPut() < Size() )
		{
			m_Memory[1] = *(unsigned char*)PeekPut( );
		}
		m_nOffset = TellPut() - 1;
	}

	if ( nSize < 0 )
	{
		m_nOffset = -nSize-1;
		g_pFullFileSystem->Seek( m_hFileHandle, m_nOffset, FILESYSTEM_SEEK_HEAD );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Reads bytes from the file; fixes up maxput if necessary and null terminates
//-----------------------------------------------------------------------------
int CUtlStreamBuffer::ReadBytesFromFile( int nBytesToRead, int nReadOffset )
{
	if ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
	{
		if ( !m_pFileName )
		{
			Warning( "File has not been opened!\n" );
			Assert(0);
			return 0;
		}

		m_hFileHandle = OpenFile( m_pFileName, m_pPath );
		if ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			Error( "Unable to read file %s!\n", m_pFileName );
			return 0;
		}
		if ( m_nOffset != 0 )
		{
			g_pFullFileSystem->Seek( m_hFileHandle, m_nOffset, FILESYSTEM_SEEK_HEAD );
		}
	}

	char *pReadPoint = (char*)Base() + nReadOffset;
	int nBytesRead = g_pFullFileSystem->Read( pReadPoint, nBytesToRead, m_hFileHandle );
	if ( nBytesRead != nBytesToRead )
	{
		// Since max put is a guess at the start, 
		// we need to shrink it based on the actual # read
		if ( m_nMaxPut > TellGet() + nReadOffset + nBytesRead )
		{
			m_nMaxPut = TellGet() + nReadOffset + nBytesRead;
		}
	}

	if ( nReadOffset + nBytesRead < Size() )
	{
		// This is necessary to deal with auto-NULL terminiation
		pReadPoint[nBytesRead] = 0;
	}

	return nBytesRead;
}


//-----------------------------------------------------------------------------
// Load up more of the stream when we overflow
//-----------------------------------------------------------------------------
bool CUtlStreamBuffer::StreamGetOverflow( int nSize )
{
	if ( !IsValid() || !IsReadOnly() )
		return false;

	// Shift the unread bytes down
	// NOTE: Can't use the partial overlap path if we're seeking. We'll 
	// get negative sizes passed in if we're seeking.
	int nUnreadBytes;
	bool bHasPartialOverlap = ( nSize >= 0 ) && ( TellGet() >= m_nOffset ) && ( TellGet() <= m_nOffset + Size() );
	if ( bHasPartialOverlap )
	{
		nUnreadBytes = Size() - ( TellGet() - m_nOffset );
		if ( ( TellGet() != m_nOffset ) && ( nUnreadBytes > 0 ) )
		{
			memmove( Base(), (const char*)Base() + TellGet() - m_nOffset, nUnreadBytes );
		}
	}
	else
	{
		m_nOffset = TellGet();
		g_pFullFileSystem->Seek( m_hFileHandle, m_nOffset, FILESYSTEM_SEEK_HEAD );
		nUnreadBytes = 0;
	}

	// Make sure the allocated size is at least as big as the requested size
	if ( nSize > 0 )
	{
		GrowAllocatedSize( nSize );
	}

	int nBytesToRead = Size() - nUnreadBytes;
	int nBytesRead = ReadBytesFromFile( nBytesToRead, nUnreadBytes );
	if ( nBytesRead == 0 )
		return false;

	m_nOffset = TellGet();
	return ( nBytesRead + nUnreadBytes >= nSize ); 
}


//-----------------------------------------------------------------------------
// open file unless already failed to open
//-----------------------------------------------------------------------------
FileHandle_t CUtlStreamBuffer::OpenFile( const char *pFileName, const char *pPath )
{
	if ( m_Error & FILE_OPEN_ERROR )
		return FILESYSTEM_INVALID_HANDLE;

	char openflags[ 3 ] = "xx";
	openflags[ 0 ] = IsReadOnly() ? 'r' : 'w';
	openflags[ 1 ] = IsText() && !ContainsCRLF() ? 't' : 'b';

	FileHandle_t fh = g_pFullFileSystem->Open( pFileName, openflags, pPath );
	if( fh == FILESYSTEM_INVALID_HANDLE )
	{
		m_Error	|= FILE_OPEN_ERROR;
	}

	return fh;
}
