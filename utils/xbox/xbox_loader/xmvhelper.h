//========= Copyright Valve Corporation, All rights reserved. ============//
//-----------------------------------------------------------------------------
// File: XMVHelper.h
//
// Desc: Definition of XMV playback helper class for playing XMVs in a number
//      of different ways.
//
// Hist: 2.7.03 - Created, based on work by Jeff Sullivan
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef _XMVHELPER_H_
#define _XMVHELPER_H_

#include <xtl.h>
#include <xmv.h>

// Number of textures to allocate for decoding. A larger number may help to
// smooth out any frame rate variations if you are decoding to a texture.
// When decoding to overlays two textures are always used.
const DWORD XMVPLAYER_NUMTEXTURES = 2;




//-----------------------------------------------------------------------------
// Name: LOAD_CONTEXT
// Desc: This structure is used to hold information used by the packet callbacks.
//-----------------------------------------------------------------------------
struct LOAD_CONTEXT
{
    // Memory available for loading packets - maximum packet size.
    DWORD       dwPacketSize;

    // Packet that XMV was decoding out of, and might still be, that
    // we want to load into as soon as we can.
    BYTE*       pPendingReleasePacket;
    // Packet that XMV is decoding out of.
    BYTE*       pDecodingPacket;
    // Packet we are currently loading into.
    BYTE*       pLoadingPacket;


    // Information for when reading packets out of a file.
    // Handle to the file we are reading from.
    HANDLE      hFile;

    // Overlapped structure to control asynchronous reading.
    OVERLAPPED  Overlapped;


    // Information for when copying packets out of a memory buffer.
    // The size and location of the buffer we are reading from if we are
    // playing a movie from memory.
    BYTE*       pInputBuffer;
    DWORD       inputSize;
    // The offset in the memory buffer that we are currently reading from.
    DWORD       readOffset;
    // The size of the packet that we have 'queued up' for reading. This
    // is a packet that we have warned about via ReleasePreviousMemoryPacket
    // but have not yet been asked to load.
    DWORD       currentPacketSize;
};




class CXMVPlayer
{
public:
    CXMVPlayer();
    ~CXMVPlayer();

    // Open a .wmv file and prepare it for playing.
    // The format parameter specifies what type of texture it should be unpacked into ( if allocateTextures
    // is TRUE ). Only YUY2, LIN_X8R8G8B8 and LIN_A8R8G8B8 are supported as formats.
    // If you plan to play the movie with Play() or otherwise use overlays then format must be D3DFMT_YUY2
    // pDevice is needed in order to create textures.
    // allocateTextures should be TRUE if you plan to play the movie manually with AdvanceFrame.
    // It should be FALSE if you plan to play the movie with Play().
    // IsPlaying() will return TRUE if the movie opened successfully.
    HRESULT             OpenFile( const CHAR* lpFilename, D3DFORMAT format, LPDIRECT3DDEVICE8 pDevice, BOOL bAllocateTextures  );

    // Open a file using the packet interface. This method of opening a .wmv file can be easily
    // customized to read the data from game specific file formats. The GetNextPacket and
    // ReleasePreviousPacket functions from XMVHelper.cpp are used to read the packets. See above for
    // information about the other parameters.
    HRESULT             OpenFileForPackets( const CHAR* lpFilename, D3DFORMAT format, LPDIRECT3DDEVICE8 pDevice, BOOL bAllocateTextures  );

    // Open a block of memory for playing using the packet interface. This method of opening a .wmv file can be easily
    // customized to read the data from game specific file formats, or to retain the block of data in memory.
    // The GetNextMemoryPacket and ReleasePreviousMemoryPacket functions from XMVHelper.cpp are used to read
    // the packets. See above for information about the other parameters.
    HRESULT             OpenMovieFromMemory( const CHAR* lpFilename, D3DFORMAT format, LPDIRECT3DDEVICE8 pDevice, BOOL bAllocateTextures  );

    // Destroy frees up all the movie resources. IsPlaying() will return FALSE afterwards. Destroy()
    // can be called during movie playback, but is not thread safe.
    HRESULT             Destroy();

    // The AdvanceFrame functions move to the next frame if it is time for that. Call this
    // function frequently - typically 30-60 times per second.

    // AdvanceFrameForTexturing returns the most recent frame decoded, for texturing.
    // It will return zero if the first frame has not been decoded, but otherwise
    // will always return a texture.
    LPDIRECT3DTEXTURE8 AdvanceFrameForTexturing( LPDIRECT3DDEVICE8 pDevice );

    // AdvanceFrameForOverlays returns newly decoded frames, for use as an overlays.
    // You MUST call UpdateOverlay() with the texture's surface.
    // It will return zero if there is not a *new* frame available.
    LPDIRECT3DTEXTURE8 AdvanceFrameForOverlays( LPDIRECT3DDEVICE8 pDevice );

    // Get information about the movie playing.
    DWORD               GetWidth() const { return m_VideoDesc.Width; }
    DWORD               GetHeight() const { return m_VideoDesc.Height; }
    DWORD               GetCurrentFrame() const { return m_dwCurrentFrame; }
	DWORD				GetElapsedTime() const { return m_bPlaying ? GetTickCount() - m_dwStartTime : 0; }

    // IsPlaying returns TRUE if a movie has been opened but has not ended or been destroyed.
    BOOL                IsPlaying() const { return m_bPlaying; }
	BOOL				IsFailed() const { return m_bError; }

    // Call TerminatePlayback on the decoder. It may take a few hundred ms before the movie stops.
    // This function is thread safe, as long as you ensure that you don't call Destroy() in another
    // thread simultaneously.
    void                TerminatePlayback();
    HRESULT             Play( DWORD Flags, RECT* pRect );

    // Call this to enable overlays if you will be playing the movie in an overlay using
    // AdvanceFrame. The overlays will be disabled when Destroy() is called.
    void                EnableOverlays( LPDIRECT3DDEVICE8 pDevice );

private:
    XMVDecoder*         m_pXMVDecoder;              // Pointer to the XMV decoder object.
    XMVVIDEO_DESC       m_VideoDesc;                // Description of the .xmv files video data.
    XMVAUDIO_DESC       m_AudioDesc;                // Description of the .xmv files audio data.
    LPDIRECT3DTEXTURE8  m_pTextures[XMVPLAYER_NUMTEXTURES]; // Textures to cycle through.
    DWORD               m_nDecodeTextureIndex;      // Index of the current texture to decode to.

    // These texture pointers are copies of the values in m_pTextures. They are used to track
    // the decode/display/pending status.
    // When displaying using textures pShowingTexture points to the texture to display,
    // pDecodingTexture points to the texture to decode into, and pSubmittedTexture is always
    // zero.
    // When displaying using overlays pShowingTexture points to the texture that the hardware
    // is currently displaying, pDecodingTexture points to the texture to decode into, and
    // pSubmittedTexture is a surface that has been submitted to the overlay system, but not
    // necessarily displayed yet. At any given time one of these is always zero. When
    // pDecodingTexture is zero that means that pShowingTexture might be being displayed, or
    // the hardware might have switched to pSubmittedTexture, we don't know yet.
    LPDIRECT3DTEXTURE8  pShowingTexture;
    LPDIRECT3DTEXTURE8  pDecodingTexture;
    LPDIRECT3DTEXTURE8  pSubmittedTexture;

    int                 m_dwCurrentFrame;           // Current frame number - minus one if not yet started.
	DWORD				m_dwStartTime;

    BOOL                m_bPlaying;                 // Is a movie currently playing?
    BOOL                m_bOverlaysEnabled;         // Are overlays enabled? For disabling them on destroy.
    LPDIRECT3DDEVICE8   m_pDevice;                  // For use by Destroy to disable overlays.
	BOOL				m_bError;

    LOAD_CONTEXT        m_loadContext;              // Data for communicating with packet callback functions.

    BYTE*               m_physicalBuffer;           // Pointer to block of physical memory used for loading packets.

    // Helper function for opening files.
    HRESULT             FinishOpeningFile( D3DFORMAT Format, LPDIRECT3DDEVICE8 pDevice, BOOL bAllocateTextures  );

    // Copy constructor and assignment operator are private and unimplemented
    // to prevent unintentional, unsupported, and disastrous copying.
    CXMVPlayer( const CXMVPlayer& source );
    CXMVPlayer& operator=( const CXMVPlayer& source );
};

#endif //#ifndef _XMVPLAYER_H_
