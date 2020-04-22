//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#if defined( WIN32) && !defined( _X360 )
#include "winlite.h"
#endif
#include "tier0/platform.h"
#include "MPAFile.h"
#include "soundchars.h"
#include "tier1/utlrbtree.h"

#include "memdbgon.h"

extern IFileSystem *g_pFullFileSystem;


// exception class
CMPAException::CMPAException(ErrorIDs ErrorID, const char *szFile, const char *szFunction, bool bGetLastError ) :
m_ErrorID( ErrorID ), m_bGetLastError( bGetLastError )
{
    m_szFile = szFile ? strdup(szFile) : NULL;
    m_szFunction = szFunction ? strdup(szFunction) : NULL;
}

// copy constructor (necessary for exception throwing without pointers)
CMPAException::CMPAException(const CMPAException& Source)
{
    m_ErrorID = Source.m_ErrorID;
    m_bGetLastError = Source.m_bGetLastError;
    m_szFile = Source.m_szFile ? strdup(Source.m_szFile) : NULL;
    m_szFunction = Source.m_szFunction ? strdup(Source.m_szFunction) : NULL;
}

// destructor
CMPAException::~CMPAException()
{
    if( m_szFile )
        free( (void*)m_szFile );
    if( m_szFunction )
        free( (void*)m_szFunction );
}

// should be in resource file for multi language applications
const char *m_szErrors[] = 
{
    "Can't open the file.", 
    "Can't set file position.",
    "Can't read from file.",
    "Reached end of buffer.",
    "No VBR Header found.",
    "Incomplete VBR Header.",
    "No subsequent frame found within tolerance range.",
    "No frame found."

};

#define MAX_ERR_LENGTH 256
void CMPAException::ShowError()
{
    char szErrorMsg[MAX_ERR_LENGTH] = {0};
    char szHelp[MAX_ERR_LENGTH];

    // this is not buffer-overflow-proof!
    if( m_szFunction )
    {
        sprintf( szHelp, _T("%s: "), m_szFunction );
        strcat( szErrorMsg, szHelp );
    }
    if( m_szFile )
    {
        sprintf( szHelp, _T("'%s'\n"), m_szFile );
        strcat( szErrorMsg, szHelp );
    }
    strcat( szErrorMsg, m_szErrors[m_ErrorID] );

#if defined(WIN32) && !defined(_X360)
    if( m_bGetLastError )
    {
        // get error message of last system error id
        LPVOID pMsgBuf;
        if ( FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL,
                            GetLastError(),
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                            (LPTSTR) &pMsgBuf,
                            0,
                            NULL ))
        {
            strcat( szErrorMsg, "\n" );
            strcat( szErrorMsg, (const char *)pMsgBuf );
            LocalFree( pMsgBuf );
        }
    }
#endif
    // show error message
    Warning( "%s\n", szErrorMsg );
}

// 1KB is inital buffersize, each time the buffer needs to be increased it is doubled
const uint32 CMPAFile::m_dwInitBufferSize = 1024;   


CMPAFile::CMPAFile( const char * szFile, uint32 dwFileOffset, FileHandle_t hFile ) :
m_pBuffer(NULL), m_dwBufferSize(0), m_dwBegin( dwFileOffset ), m_dwEnd(0),
m_dwNumTimesRead(0), m_bVBRFile( false ), m_pVBRHeader(NULL), m_bMustReleaseFile( false ),
m_pMPAHeader(NULL), m_hFile( hFile ), m_szFile(NULL), m_dwFrameNo(1)
{
    // open file, if not already done
    if( m_hFile == FILESYSTEM_INVALID_HANDLE )
    {
        Open( szFile );
        m_bMustReleaseFile = true;
    }
    // save filename
    m_szFile = strdup( szFile );
    
    // set end of MPEG data (assume file end)
    if( m_dwEnd <= 0 )
    {
        // get file size
        m_dwEnd = g_pFullFileSystem->Size( m_hFile );
    }

    // find first valid MPEG frame
    m_pMPAHeader = new CMPAHeader( this );

    // is VBR header available?
    CVBRHeader::VBRHeaderType HeaderType = CVBRHeader::NoHeader;
    uint32 dwOffset = m_pMPAHeader->m_dwSyncOffset;
    if( CVBRHeader::IsVBRHeaderAvailable( this, HeaderType, dwOffset ) )
    {
        try
        {
            // read out VBR header
            m_pVBRHeader = new CVBRHeader( this, HeaderType, dwOffset );
            
            m_bVBRFile = true;
            m_dwBytesPerSec = m_pVBRHeader->m_dwBytesPerSec;
            if( m_pVBRHeader->m_dwBytes > 0 )
                m_dwEnd = m_dwBegin + m_pVBRHeader->m_dwBytes;
        }

        catch(CMPAException& Exc)
        {
            Exc.ShowError();
        }   
    }
    
    if( !m_pVBRHeader )
    {
        // always skip empty (32kBit) frames
        m_bVBRFile = m_pMPAHeader->SkipEmptyFrames();
        m_dwBytesPerSec = m_pMPAHeader->GetBytesPerSecond();    
    }
}

bool CMPAFile::GetNextFrame()
{
    uint32 dwOffset = m_pMPAHeader->m_dwSyncOffset + m_pMPAHeader->m_dwRealFrameSize;
    try
    {
        CMPAHeader* pFrame = new CMPAHeader( this, dwOffset, false );
    
        delete m_pMPAHeader;
        m_pMPAHeader = pFrame;
        if( m_dwFrameNo > 0 )
            m_dwFrameNo++;
    }
    catch(...)
    {
        return false;
    }
    return true;
}

bool CMPAFile::GetPrevFrame()
{
    uint32 dwOffset = m_pMPAHeader->m_dwSyncOffset-MPA_HEADER_SIZE;
    try
    {
        // look backward from dwOffset on
        CMPAHeader* pFrame = new CMPAHeader( this, dwOffset, false, true );
    
        delete m_pMPAHeader;
        m_pMPAHeader = pFrame;
        if( m_dwFrameNo > 0 )
            m_dwFrameNo --;
    }
    catch(...)
    {
        return false;
    }
    
    return true;
}

bool CMPAFile::GetFirstFrame()
{
    uint32 dwOffset = 0;
    try
    {
        CMPAHeader* pFrame = new CMPAHeader( this, dwOffset, false );
    
        delete m_pMPAHeader;
        m_pMPAHeader = pFrame;
        m_dwFrameNo = 1;
    }
    catch(...)
    {
        return false;
    }
    return true;
}

bool CMPAFile::GetLastFrame()
{
    uint32 dwOffset = m_dwEnd - m_dwBegin - MPA_HEADER_SIZE;
    try
    {
        // look backward from dwOffset on
        CMPAHeader* pFrame = new CMPAHeader( this, dwOffset, false, true );
    
        delete m_pMPAHeader;
        m_pMPAHeader = pFrame;
        m_dwFrameNo = 0;
    }
    catch(...)
    {
        return false;
    }
    
    return true;
}

// destructor
CMPAFile::~CMPAFile(void)
{
    delete m_pMPAHeader;
    
    if( m_pVBRHeader )
        delete m_pVBRHeader;
    
    if( m_pBuffer )
        delete[] m_pBuffer;
    
    // close file
    if( m_bMustReleaseFile )
        g_pFullFileSystem->Close( m_hFile );    

    if( m_szFile )
        free( (void*)m_szFile );
}

// open file
void CMPAFile::Open( const char * szFilename )
{
    // open with CreateFile (no limitation of 128byte filename length, like in mmioOpen)
    m_hFile = g_pFullFileSystem->Open( szFilename, "rb", "GAME" );//::CreateFile( szFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if( m_hFile == FILESYSTEM_INVALID_HANDLE )
    {
        // throw error
        throw CMPAException( CMPAException::ErrOpenFile, szFilename, _T("CreateFile"), true );
    }
}

// set file position
void CMPAFile::SetPosition( int offset )
{
    /*
    LARGE_INTEGER liOff;

    liOff.QuadPart = lOffset;
    liOff.LowPart = ::SetFilePointer(m_hFile, liOff.LowPart, &liOff.HighPart, dwMoveMethod );
    if (liOff.LowPart  == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR )
    {
        // throw error
        throw CMPAException( CMPAException::ErrSetPosition, m_szFile, _T("SetFilePointer"), true );
    }
    */

    g_pFullFileSystem->Seek( m_hFile, offset, FILESYSTEM_SEEK_HEAD );
}

// read from file, return number of bytes read
uint32 CMPAFile::Read( void *pData, uint32 dwSize, uint32 dwOffset )
{
    uint32 dwBytesRead = 0;
    
    // set position first
    SetPosition( m_dwBegin+dwOffset );

    //if( !::ReadFile( m_hFile, pData, dwSize, &dwBytesRead, NULL ) )
    //  throw CMPAException( CMPAException::ErrReadFile, m_szFile, _T("ReadFile"), true );
    dwBytesRead = g_pFullFileSystem->Read( pData, dwSize, m_hFile );

    return dwBytesRead;
}

// convert from big endian to native format (Intel=little endian) and return as uint32 (32bit)
uint32 CMPAFile::ExtractBytes( uint32& dwOffset, uint32 dwNumBytes, bool bMoveOffset )
{   
    Assert( dwNumBytes > 0 );
    Assert( dwNumBytes <= 4 );  // max 4 byte

    // enough bytes in buffer, otherwise read from file
    if( !m_pBuffer || ( ((int)(m_dwBufferSize - dwOffset)) < (int)dwNumBytes) )
        FillBuffer( dwOffset + dwNumBytes );

    uint32 dwResult = 0;

    // big endian extract (most significant byte first) (will work on little and big-endian computers)
    uint32 dwNumByteShifts = dwNumBytes - 1;

    for( uint32 n=dwOffset; n < dwOffset+dwNumBytes; n++ )
    {
        dwResult |= ((byte)m_pBuffer[n]) << (8*dwNumByteShifts); // the bit shift will do the correct byte order for you                                                           
		dwNumByteShifts--;
    }
    
    if( bMoveOffset )
        dwOffset += dwNumBytes;

    return dwResult;
}

// throws exception if not possible
void CMPAFile::FillBuffer( uint32 dwOffsetToRead )
{
    uint32 dwNewBufferSize;
    
    // calc new buffer size
    if( m_dwBufferSize == 0 )
        dwNewBufferSize = m_dwInitBufferSize;
    else
        dwNewBufferSize = m_dwBufferSize*2;

    // is it big enough?
    if( dwNewBufferSize < dwOffsetToRead )
        dwNewBufferSize = dwOffsetToRead;
    
    // reserve new buffer
    BYTE* pNewBuffer = new BYTE[dwNewBufferSize];
    
    // take over data from old buffer
    if( m_pBuffer )
    {
        memcpy( pNewBuffer, m_pBuffer, m_dwBufferSize );

        // release old buffer 
        delete[] m_pBuffer;
    }
    m_pBuffer = (char*)pNewBuffer;
    
    // read <dwNewBufferSize-m_dwBufferSize> bytes from offset <m_dwBufferSize>
    uint32 dwBytesRead = Read( m_pBuffer+m_dwBufferSize, dwNewBufferSize-m_dwBufferSize, m_dwBufferSize );

    // no more bytes in buffer than read out from file
    m_dwBufferSize += dwBytesRead;
}

// Uses mp3 code from:  http://www.codeproject.com/audio/MPEGAudioInfo.asp

struct MP3Duration_t
{
    FileNameHandle_t h;
    float           duration;

    static bool LessFunc( const MP3Duration_t& lhs, const MP3Duration_t& rhs )
    {
        return lhs.h < rhs.h;
    }
};

CUtlRBTree< MP3Duration_t, int >    g_MP3Durations( 0, 0, MP3Duration_t::LessFunc );

float GetMP3Duration_Helper( char const *filename )
{
	float duration = 60.0f;

	// See if it's in the RB tree already...
	char fn[ 512 ];
	Q_snprintf( fn, sizeof( fn ), "sound/%s", PSkipSoundChars( filename ) );

	FileNameHandle_t h = g_pFullFileSystem->FindOrAddFileName( fn );

	MP3Duration_t search;
	search.h = h;
	
	int idx = g_MP3Durations.Find( search );
	if ( idx != g_MP3Durations.InvalidIndex() )
	{
		return g_MP3Durations[ idx ].duration;
	}

	try
	{
		CMPAFile MPAFile( fn, 0 );
		if ( MPAFile.m_dwBytesPerSec != 0 )
		{
			duration = (float)(MPAFile.m_dwEnd - MPAFile.m_dwBegin) / (float)MPAFile.m_dwBytesPerSec;
		}
	}
	catch ( ... )
	{
	}

	search.duration = duration;
	g_MP3Durations.Insert( search );

	return duration;
}
