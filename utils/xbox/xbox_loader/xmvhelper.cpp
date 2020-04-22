//========= Copyright Valve Corporation, All rights reserved. ============//
//-----------------------------------------------------------------------------
// File: WMVPlayer.cpp
//
// Desc: This helper class provides simple WMV decoding and playback 
//       functionality.  It will be expanded as new playback methods are 
//       exposed
//
// Hist: 2.7.03 - Created, based on work by Jeff Sullivan
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "xbox_loader.h"
#include <xtl.h>
#include "XMVHelper.h"
#include "XBUtil.h"
#include <stdio.h>


// Funtion Prototypes for packet loading functions for loading from a file.
HRESULT CALLBACK GetNextPacket( DWORD dwContext,
                                        void **ppPacket, 
                                        DWORD* pOffsetToNextPacket );

HRESULT CALLBACK ReleasePreviousPacket( DWORD dwContext, 
                                        LONGLONG llNextReadByteOffset, 
                                        DWORD dwNextPacketSize );

// Funtion Prototypes for packet loading functions for loading from a block of memory.
HRESULT CALLBACK GetNextMemoryPacket( DWORD dwContext,
                                        void **ppPacket, 
                                        DWORD* pOffsetToNextPacket );

HRESULT CALLBACK ReleasePreviousMemoryPacket( DWORD dwContext, 
                                        LONGLONG llNextReadByteOffset, 
                                        DWORD dwNextPacketSize );




//-----------------------------------------------------------------------------
// Name: CXMVPlayer()
// Desc: Constructor for CXMVPlayer
//-----------------------------------------------------------------------------
CXMVPlayer::CXMVPlayer()
{
    m_pXMVDecoder = NULL;
    ZeroMemory( &m_VideoDesc, sizeof( m_VideoDesc ) );
    ZeroMemory( &m_AudioDesc, sizeof( m_AudioDesc ) );
    for ( UINT i=0; i<XMVPLAYER_NUMTEXTURES; i++ )
    {
        m_pTextures[i]  = NULL;
    }

    m_dwCurrentFrame = -1;      // Will be zero after we decode the first frame.

    m_bPlaying = FALSE;
    m_bOverlaysEnabled = FALSE;

    m_loadContext.hFile = INVALID_HANDLE_VALUE;
    m_loadContext.pInputBuffer = 0;
    m_physicalBuffer = 0;
	m_bError = FALSE;
}




//-----------------------------------------------------------------------------
// Name: ~CXMVPlayer()
// Desc: Destructor for CXMVPlayer
//-----------------------------------------------------------------------------
CXMVPlayer::~CXMVPlayer()
{
    Destroy();
}




//-----------------------------------------------------------------------------
// Name: Destroy()
// Desc: Free all resources and clear are resource pointers and handles.
//-----------------------------------------------------------------------------
HRESULT CXMVPlayer::Destroy()
{
    // Disable overlays if we were using them.
    if ( m_bOverlaysEnabled )
    {
        m_pDevice->EnableOverlay( FALSE );
        m_bOverlaysEnabled = FALSE;
    }

    // Free the XMV decoder.
    if ( NULL != m_pXMVDecoder )
    {
        m_pXMVDecoder->CloseDecoder();
        m_pXMVDecoder = NULL;
    }

    ZeroMemory( &m_VideoDesc, sizeof( m_VideoDesc ) );
    ZeroMemory( &m_AudioDesc, sizeof( m_AudioDesc ) );

    // Release our textures.
    for ( UINT i=0; i<XMVPLAYER_NUMTEXTURES; i++ )
    {
        if ( m_pTextures[i] )
            m_pTextures[i]->Release();
        m_pTextures[i] = 0;
    }

    m_dwCurrentFrame = -1;
	m_dwStartTime = 0;

    m_bPlaying = FALSE;

    // Release any file handles we were using.
    if( INVALID_HANDLE_VALUE != m_loadContext.hFile )
    {
        CloseHandle( m_loadContext.hFile );
        m_loadContext.hFile = INVALID_HANDLE_VALUE;
    }

    // Free up memory used for playing a movie from memory.
    if ( m_loadContext.pInputBuffer )
    {
        free( m_loadContext.pInputBuffer );
        m_loadContext.pInputBuffer = 0;
    }

    // Be sure to release the physical memory last!
    if( m_physicalBuffer )
    {
        XPhysicalFree( m_physicalBuffer );
        m_physicalBuffer = 0;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FinishOpeningFile()
// Desc: Helper function for the three Open functions. Enables the audio streams,
// initializes the video descriptor, and allocates textures if needed.
//-----------------------------------------------------------------------------
HRESULT CXMVPlayer::FinishOpeningFile( D3DFORMAT format, LPDIRECT3DDEVICE8 pDevice, BOOL bAllocateTextures   )
{
    assert( format == D3DFMT_YUY2 || format == D3DFMT_LIN_A8R8G8B8 );
    assert( XMVPLAYER_NUMTEXTURES >= 2);

    HRESULT hr = S_OK;

    m_pXMVDecoder->GetVideoDescriptor( &m_VideoDesc );

    // Enable the audio streams
    for ( unsigned i=0; i < m_VideoDesc.AudioStreamCount; i++ )
    {
        m_pXMVDecoder->GetAudioDescriptor( i, &m_AudioDesc );
        hr = m_pXMVDecoder->EnableAudioStream( i, 0, NULL, NULL);
        if ( FAILED( hr ) )
        {
            XBUtil_DebugPrint( "Unable to enable audio stream 0 (error %x)\n", hr );
            Destroy();
            return hr;
        }
    }

    for ( int i = 0; i < XMVPLAYER_NUMTEXTURES; i++ )
    {
        m_pTextures[i] = 0;
        if ( bAllocateTextures )
        {
            hr = pDevice->CreateTexture( m_VideoDesc.Width, m_VideoDesc.Height, 1, 0, format, 0, &m_pTextures[i] );
            if ( FAILED( hr ) )
            {
                XBUtil_DebugPrint( "Unable to create texture %d (error %x)\n", i, hr );
                Destroy();
                return hr;
            }
        }
    }

    // Initialize what texture we are decoding to, if decoding for texture mapping.
    m_nDecodeTextureIndex = 0;

    // Initialize the various texture pointers for use when decoding for overlays.
    pShowingTexture = m_pTextures[0];
    pDecodingTexture = m_pTextures[1];
    pSubmittedTexture = 0;

    m_bPlaying = TRUE;
	m_dwStartTime = GetTickCount();

    return hr;
}




//-----------------------------------------------------------------------------
// Name: OpenFile()
// Desc: Create an XMV decoder object that reads from a file.
//-----------------------------------------------------------------------------
HRESULT CXMVPlayer::OpenFile( const CHAR* lpFilename, D3DFORMAT format, LPDIRECT3DDEVICE8 pDevice, BOOL bAllocateTextures   )
{
    HRESULT hr  = S_OK;

	m_bError = FALSE;

    if ( NULL == lpFilename || NULL == pDevice )
    {
        XBUtil_DebugPrint( "Bad parameter to OpenFile()\n" );
		m_bError = TRUE;
        return E_FAIL;
    }

    hr = XMVDecoder_CreateDecoderForFile( XMVFLAG_SYNC_ON_NEXT_VBLANK, ( CHAR* )lpFilename, &m_pXMVDecoder );
    if ( FAILED( hr ) )
    {
        XBUtil_DebugPrint( "Unable to create XMV Decoder for %s (error: %x)\n", lpFilename, hr );
		m_bError = TRUE;
		return hr;
    }

    hr = FinishOpeningFile( format, pDevice, bAllocateTextures );
	
	if ( FAILED( hr ) )
	{
		m_bError = TRUE;
	}

    return hr;
}




//-----------------------------------------------------------------------------
// Name: OpenFileForPackets()
// Desc: Create an XMV decoder object that uses the packet reading interface.
// Currently this just reads from a file, but it can be altered to read from
// custom formats, start partway through a file, etc.
//-----------------------------------------------------------------------------
HRESULT CXMVPlayer::OpenFileForPackets( const CHAR* lpFilename, D3DFORMAT format, LPDIRECT3DDEVICE8 pDevice, BOOL bAllocateTextures  )
{
    HRESULT hr = S_OK;

    // We need to read in the first 4K of data for the XMV player to initialize
    // itself from. This is most conveniently read as an array of DWORDS.
    DWORD    first4Kbytes[4096 / sizeof( DWORD )];

    // Clear entire context struct to zero
    ZeroMemory( &m_loadContext, sizeof( m_loadContext ) );

    // Open the input file.
    m_loadContext.hFile = CreateFile( lpFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, 
                                NULL );

    if( m_loadContext.hFile == INVALID_HANDLE_VALUE )
    {
        Destroy();
        return E_INVALIDARG;
    }

    // Read the first page from the file. We opened it for 
    // overlapped IO so we do a pair of reads.
    m_loadContext.Overlapped.Offset     = 0;
    m_loadContext.Overlapped.OffsetHigh = 0;

    // Start the read.
    if( 0 == ReadFile( m_loadContext.hFile, first4Kbytes, sizeof( first4Kbytes ), NULL, &m_loadContext.Overlapped ) )
    {
        if( GetLastError() != ERROR_IO_PENDING )
        {
            Destroy();
            return E_FAIL;
        }
    }

    // Wait for the read to finish.
    DWORD dwBytesRead;
    if( !GetOverlappedResult( m_loadContext.hFile, &m_loadContext.Overlapped, &dwBytesRead, TRUE ) )
    {
        Destroy();
        return E_FAIL;
    }

    // Check size to make sure input is a valid XMV file.
    if( dwBytesRead != 4096 )
    {
        Destroy();
        return E_FAIL;
    }

    // Create an XMV decoder
    hr = XMVDecoder_CreateDecoderForPackets( XMVFLAG_SYNC_ON_NEXT_VBLANK, first4Kbytes, ( DWORD )&m_loadContext,
                                             GetNextPacket, ReleasePreviousPacket, &m_pXMVDecoder );
    if( FAILED( hr ) )
    {
        Destroy();
        return E_FAIL;
    }

    // The size of the first packet and the minimum size of the two packet buffers are stored in the
    // second and third DWORDS of the file. From xmv.h:
    // *    DWORD NextPacketSize  // The size of the next packet
    // *    DWORD ThisPacketSize  // The size of this packet
    // *    DWORD MaxPacketSize   // The size of the largest packet in the file
    DWORD dwThisPacketSize     = first4Kbytes[1];
    DWORD dwRequiredPacketSize = first4Kbytes[2];

    // Check for illegal parameters.
    if( dwThisPacketSize > dwRequiredPacketSize )
    {
        Destroy();
        return E_FAIL;
    }

    // XPhysicalAlloc is used so that 5.1 or compressed audio streams can be played.
    m_physicalBuffer = ( BYTE* )XPhysicalAlloc( dwRequiredPacketSize * 2, MAXULONG_PTR, 0, PAGE_READWRITE );

    // Save our information.
    m_loadContext.dwPacketSize    = dwRequiredPacketSize;
    m_loadContext.pLoadingPacket  = m_physicalBuffer;
    m_loadContext.pDecodingPacket = m_physicalBuffer + dwRequiredPacketSize;

    // Read the first packet.  We wind up re-reading the first 4096
    // bytes but it makes the logic for figuring out how much we read
    // a little bit easier...
    m_loadContext.Overlapped.Offset     = 0;
    m_loadContext.Overlapped.OffsetHigh = 0;

    if( 0 == ReadFile( m_loadContext.hFile, m_physicalBuffer, dwThisPacketSize, NULL,
                       &m_loadContext.Overlapped ) )
    {
        if( GetLastError() != ERROR_IO_PENDING )
        {
            Destroy();
            return E_FAIL;
        }
    }

    // Note - at this point the preceding read has *not* necessarily completed.
    // Don't try reading anything from that buffer until GetNextPacket has been
    // successfully called.

    hr = FinishOpeningFile( format, pDevice, bAllocateTextures );

    return hr;
}




//-----------------------------------------------------------------------------
// Name: OpenMovieFromMemory()
// Desc: Create an XMV decoder object that uses the packet reading interface to
// read from a block of memory. To simplify the memory management this function
// also allocates this block of memory and initializes it from a file.
//-----------------------------------------------------------------------------
HRESULT CXMVPlayer::OpenMovieFromMemory( const CHAR* lpFilename, D3DFORMAT format, LPDIRECT3DDEVICE8 pDevice, BOOL bAllocateTextures  )
{
    HRESULT hr = S_OK;

	m_bError = FALSE;

    // Read the entire file into memory.
    void* data;
    hr = XBUtil_LoadFile( lpFilename, &data, &m_loadContext.inputSize );
    if ( FAILED( hr ) )
	{
		m_bError = TRUE;
        return hr;
	}

    m_loadContext.pInputBuffer = ( BYTE* )data;

    // Check size to make sure input is a valid XMV file.
    if( m_loadContext.inputSize < 4096 )
    {
        Destroy();
		m_bError = TRUE;
        return E_FAIL;
    }

    // Get a DWORD pointer to the first 4K - needed by CreateDecoderForPackets
    DWORD* first4Kbytes = ( DWORD* )data;

    // Create an XMV decoder
    hr = XMVDecoder_CreateDecoderForPackets( XMVFLAG_SYNC_ON_NEXT_VBLANK, first4Kbytes, ( DWORD )&m_loadContext, GetNextMemoryPacket,
                                             ReleasePreviousMemoryPacket, &m_pXMVDecoder );
    if ( FAILED( hr ) )
    {
        Destroy();
		m_bError = TRUE;
        return E_FAIL;
    }

    // The size of the first packet and the minimum size of the two packet buffers are stored in the
    // second and third DWORDS of the file. From xmv.h:
    // *    DWORD NextPacketSize  // The size of the next packet
    // *    DWORD ThisPacketSize  // The size of this packet
    // *    DWORD MaxPacketSize   // The size of the largest packet in the file
    DWORD dwThisPacketSize     = first4Kbytes[1];
    DWORD dwRequiredPacketSize = first4Kbytes[2];

    // Check for illegal parameters.
    if( dwThisPacketSize > dwRequiredPacketSize )
    {
        Destroy();
		m_bError = TRUE;
        return E_FAIL;
    }

    // XPhysicalAlloc is used so that 5.1 or compressed audio streams can be played.
    m_physicalBuffer = ( BYTE* )XPhysicalAlloc( dwRequiredPacketSize * 2, MAXULONG_PTR, 0, PAGE_READWRITE );

    // Save our information for the callback functions.
    // The size of our two memory blocks.
    m_loadContext.dwPacketSize    = dwRequiredPacketSize;
    // The addresses of our two memory blocks.
    m_loadContext.pLoadingPacket  = m_physicalBuffer;
    m_loadContext.pDecodingPacket = m_physicalBuffer + dwRequiredPacketSize;

    // Information about the block of memory the movie is stored in.
    m_loadContext.pInputBuffer = ( BYTE* )data;
    m_loadContext.inputSize = m_loadContext.inputSize;
    m_loadContext.readOffset = 0;
    m_loadContext.currentPacketSize = dwThisPacketSize;

    hr = FinishOpeningFile( format, pDevice, bAllocateTextures );

	if ( FAILED( hr ) )
	{
		m_bError = TRUE;
	}

    return hr;
}




//-----------------------------------------------------------------------------
// Name: AdvanceFrameForTexturing()
// Desc: Unpack the appropriate frames of data for use as textures.
//-----------------------------------------------------------------------------
LPDIRECT3DTEXTURE8 CXMVPlayer::AdvanceFrameForTexturing( LPDIRECT3DDEVICE8 pDevice )
{
    // You must pass bAllocateTextures==TRUE to Open if you're going to use GetTexture/AdvanceFrame.
    assert( m_pTextures[0] );

    LPDIRECT3DSURFACE8  pSurface;
    pDecodingTexture->GetSurfaceLevel( 0, &pSurface );

    // Decode some information to the current draw texture.
    XMVRESULT xr = XMV_NOFRAME;
    m_pXMVDecoder->GetNextFrame( pSurface, &xr, NULL );

    switch ( xr )
    {
        case XMV_NOFRAME:
            // Do nothing - we didn't get a frame.
            break;

        case XMV_NEWFRAME:
            ++m_dwCurrentFrame;
            // GetNextFrame produced a new frame. So, the texture we were decoding
            // to becomes available for drawing as a texture. 
            pShowingTexture = pDecodingTexture;

            // Setup for decoding to the next texture.
            m_nDecodeTextureIndex = ( m_nDecodeTextureIndex + 1 ) % XMVPLAYER_NUMTEXTURES;
            pDecodingTexture = m_pTextures[ m_nDecodeTextureIndex ];
            break;

        case XMV_ENDOFFILE:
            m_bPlaying = FALSE;
            break;

        case XMV_FAIL:
            // Data corruption or file read error. We'll treat that the same as
            // end of file.
            m_bPlaying = FALSE;
			m_bError = TRUE;
            break;
    }

    SAFE_RELEASE( pSurface );

    // If we haven't decoded the first frame then return zero.
    if ( m_dwCurrentFrame < 0 )
        return 0;

    return pShowingTexture;
}




//-----------------------------------------------------------------------------
// Name: AdvanceFrameForOverlays()
// Desc: Unpack the appropriate frames of data for use as an overlay.
//-----------------------------------------------------------------------------
LPDIRECT3DTEXTURE8 CXMVPlayer::AdvanceFrameForOverlays( LPDIRECT3DDEVICE8 pDevice )
{
    // You must pass bAllocateTextures==TRUE to Open if you're going to use GetTexture/AdvanceFrame.
    assert( m_pTextures[0] );

    // You have to call CXMVPlayer::EnableOverlays() if you are going to use overlays.
    assert( m_bOverlaysEnabled );

    // If a texture has been submitted to be used as an overlay then we have to
    // wait for GetUpdateOverlayState() to return TRUE before we can assume that
    // the previous texture has *stopped* being displayed. Once GetUpdateOverlayState()
    // returns TRUE then we know that pSubmittedTexture is being displayed, which
    // means that, pShowingTexture is available as a decoding target.
    if ( pSubmittedTexture )
    {
        // If GetOverlayUpdateStatus() returns FALSE then we can still proceed and
        // call GetNextFrame(), but we will pass NULL for the surface parameter.
        // Some work will still be done, but none of the surfaces will be altered.
        if ( pDevice->GetOverlayUpdateStatus() )
        {
            // The call to UpdateOverlay() with pSubmittedTexture must have taken
            // effect now, so pShowingTexture is available as a decoding target.
            assert( !pDecodingTexture );
            pDecodingTexture = pShowingTexture;
            pShowingTexture = pSubmittedTexture;
            pSubmittedTexture = NULL;
        }
    }

    LPDIRECT3DSURFACE8  pSurface = NULL;
    if ( pDecodingTexture )
        pDecodingTexture->GetSurfaceLevel( 0, &pSurface );

    // Decode some information to the current draw texture, which may be NULL.
    // pDecodingTexture will be NULL if one texture has been submitted as a new
    // overlay but the other one is still being displayed as an overlay.
    // If pSurface is NULL GetNextFrame() will still do some work.
    XMVRESULT xr = XMV_NOFRAME;
    m_pXMVDecoder->GetNextFrame( pSurface, &xr, NULL );

    switch ( xr )
    {
        case XMV_NOFRAME:
            // Do nothing - we didn't get a frame.
            break;

        case XMV_NEWFRAME:
            ++m_dwCurrentFrame;
            // GetNextFrame produced a new frame. So, the texture we were decoding
            // to becomes available for displaying as an overlay. 

            // The other texture is not ready to be a decoding target. It is still
            // being displayed as an overlay. So, we assign the newly decoded
            // texture to pSubmittedTexture for the program to submit as an overlay,
            // but we don't yet move the previously submitted texture from pShowing
            // to pDecoding. That happens on a subsequent call to this function, after
            // GetOverlayUpdateStatus() returns TRUE to tell us that there are no
            // overlay swaps pending.
            assert( pDecodingTexture );
            assert( !pSubmittedTexture );
            pSubmittedTexture = pDecodingTexture;
            pDecodingTexture = NULL;
            break;

        case XMV_ENDOFFILE:
            m_bPlaying = FALSE;
            break;

        case XMV_FAIL:
            // Data corruption or file read error. We'll treat that the same as
            // end of file.
            m_bPlaying = FALSE;
			m_bError = TRUE;
            break;
    }

    SAFE_RELEASE( pSurface );

    // If we just unpacked a new frame then we return that texture
    // and the program must call UpdateOverlay() with the surface
    // from that texture.
    // If we didn't unpack a frame then the program should do nothing -
    // the previous overlay will continue to be displayed.
    if ( XMV_NEWFRAME == xr )
        return pSubmittedTexture;

    // No new frame to display.
    return 0;
}




//-----------------------------------------------------------------------------
// Name: TerminatePlayback()
// Desc: Calls XMVDecoder::TerminatePlayback()
//-----------------------------------------------------------------------------
void CXMVPlayer::TerminatePlayback()
{
    m_pXMVDecoder->TerminatePlayback();
}




//-----------------------------------------------------------------------------
// Name: Play()
// Desc: Calls XMVDecoder::Play() to play the entire movie.
//-----------------------------------------------------------------------------
HRESULT CXMVPlayer::Play( DWORD Flags, RECT* pRect )
{
    // You have to call Open before calling Play.
    assert( m_pXMVDecoder );

    // Don't pass bAllocateTextures==TRUE to Open if you're going to use Play.
    assert( !m_pTextures[0] );

    return m_pXMVDecoder->Play( Flags, pRect );
}




//-----------------------------------------------------------------------------
// Name: EnableOverlays()
// Desc: Enable the overlay planes for playing the movie in them, and record
// that the overlays should be disabled when Destroy() is called.
//-----------------------------------------------------------------------------
void CXMVPlayer::EnableOverlays( LPDIRECT3DDEVICE8 pDevice )
{
    m_pDevice = pDevice;
    pDevice->EnableOverlay( TRUE );
    m_bOverlaysEnabled = TRUE;
}




//-----------------------------------------------------------------------------
// Name: GetNextPacket()
// Desc: Callback function to get next packet from a file
//-----------------------------------------------------------------------------
static HRESULT CALLBACK GetNextPacket( DWORD dwContext, VOID** ppPacket, 
                                DWORD* pOffsetToNextPacket )
{
    LOAD_CONTEXT* pContext = ( LOAD_CONTEXT* )dwContext;
    if( NULL == pContext )
        return E_FAIL;

    // If the next packet is fully loaded then return it, 
    // otherwise return NULL.
    DWORD dwBytesRead;    
    if( GetOverlappedResult( pContext->hFile, &pContext->Overlapped, 
                             &dwBytesRead, FALSE ) )
    {
        // Make the old decoding packet pending.
        pContext->pPendingReleasePacket = pContext->pDecodingPacket;
        pContext->pDecodingPacket       = pContext->pLoadingPacket;
        pContext->pLoadingPacket        = NULL;

        // Offset to the next packet.
        *pOffsetToNextPacket = dwBytesRead;

        // Set *ppPacket to the data we just loaded.
        *ppPacket            = pContext->pDecodingPacket;
    }
    else
    {
        DWORD dwError = GetLastError();

        // If we're waiting on the IO to finish, just do nothing.
        if( dwError != ERROR_IO_INCOMPLETE )
            return HRESULT_FROM_WIN32( dwError );

        *ppPacket            = NULL;
        *pOffsetToNextPacket = 0;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ReleasePreviousPacket()
// Desc: Callback function to release previous packet from a file
//-----------------------------------------------------------------------------
static HRESULT CALLBACK ReleasePreviousPacket( DWORD dwContext, LONGLONG llNextReadByteOffset, 
                                        DWORD dwNextPacketSize )
{
    LOAD_CONTEXT* pContext = ( LOAD_CONTEXT* )dwContext;
    if( NULL == pContext )
        return E_FAIL;

    if( dwNextPacketSize != 0 )
    {
        // Start the next load.
        pContext->Overlapped.Offset     = ( DWORD )( llNextReadByteOffset & 0xFFFFFFFF );
        pContext->Overlapped.OffsetHigh = ( DWORD )( llNextReadByteOffset >> 32 );

        // Check for bad input file - buffer overrun
        if( dwNextPacketSize > pContext->dwPacketSize )
            return E_FAIL;

        pContext->pLoadingPacket        = pContext->pPendingReleasePacket;
        pContext->pPendingReleasePacket = NULL;

        if( 0 == ReadFile( pContext->hFile, pContext->pLoadingPacket, 
                       dwNextPacketSize, NULL, &pContext->Overlapped ) )
        {
            if( GetLastError() != ERROR_IO_PENDING )
                return HRESULT_FROM_WIN32( GetLastError() );
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetNextMemoryPacket()
// Desc: Callback function to get next packet from a file,
// and setup for the next packet.
//-----------------------------------------------------------------------------
static HRESULT CALLBACK GetNextMemoryPacket( DWORD dwContext, VOID** ppPacket, 
                                DWORD* pOffsetToNextPacket )
{
    LOAD_CONTEXT* pContext = ( LOAD_CONTEXT* )dwContext;
    if( NULL == pContext )
        return E_FAIL;

    DWORD dwBytesRead = pContext->inputSize - pContext->readOffset;
    if ( pContext->currentPacketSize < dwBytesRead )
        dwBytesRead = pContext->currentPacketSize;

    memcpy( pContext->pLoadingPacket, pContext->pInputBuffer + pContext->readOffset , dwBytesRead );
    pContext->readOffset +=dwBytesRead;

    // Swap pointers so that next time we load it goes into the other packet block.
    BYTE* temp = pContext->pLoadingPacket;
    pContext->pLoadingPacket = pContext->pDecodingPacket;
    pContext->pDecodingPacket = temp;

    // Offset to the next packet.
    *pOffsetToNextPacket = dwBytesRead;

    // Set *ppPacket to the data we just loaded.
    *ppPacket = pContext->pDecodingPacket;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ReleasePreviousMemoryPacket()
// Desc: Callback function to release previous packet from a block of memory,
// and setup for the next packet.
//-----------------------------------------------------------------------------
static HRESULT CALLBACK ReleasePreviousMemoryPacket( DWORD dwContext, LONGLONG llNextReadByteOffset, 
                                        DWORD dwNextPacketSize )
{
    LOAD_CONTEXT* pContext = ( LOAD_CONTEXT* )dwContext;
    if( NULL == pContext )
        return E_FAIL;

    // Check for bad input file - buffer overrun
    if( dwNextPacketSize > pContext->dwPacketSize )
        return E_FAIL;

    // Record the size of the next packet we are supposed to read, for GetNextMemoryPacket.
    pContext->currentPacketSize = dwNextPacketSize;

    return S_OK;
}
