//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//=============================================================================

#ifndef IBIK_H
#define IBIK_H

#ifdef _WIN32
#pragma once
#endif

#include "appframework/IAppSystem.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct BGR888_t;
class IMaterial;

//-----------------------------------------------------------------------------
// Parameters for creating a new BINK
//-----------------------------------------------------------------------------
struct BIKParams_t
{
	BIKParams_t() :
		m_nFrameRate( 0 ), m_nFrameScale( 1 ), m_nWidth( 0 ), m_nHeight( 0 ),
		m_nSampleRate( 0 ), m_nSampleBits( 0 ), m_nNumChannels( 0 )
	{
		m_pFileName[ 0 ] = 0;
	}

	char		m_pFileName[ 256 ];
	char		m_pPathID[ 256 ];

	// fps = m_nFrameRate / m_nFrameScale
	// for integer framerates, set framerate to the fps, and framescale to 1
	// for ntsc-style framerates like 29.97 (or 23.976 or 59.94),
	// set framerate to 30,000 (or 24,000 or 60,000) and framescale to 1001
	// yes, framescale is an odd naming choice, but it matching MS's AVI api
	int			m_nFrameRate;
	int			m_nFrameScale;

	int			m_nWidth;
	int			m_nHeight;

	// Sound/.wav info
	int			m_nSampleRate;
	int			m_nSampleBits;
	int			m_nNumChannels;
};

//-----------------------------------------------------------------------------
// Handle to an BINK
//-----------------------------------------------------------------------------
typedef unsigned short BIKHandle_t;
enum
{
	BIKHANDLE_INVALID = (BIKHandle_t)~0
};


//-----------------------------------------------------------------------------
// Handle to an BINK material
//-----------------------------------------------------------------------------
typedef unsigned short BIKMaterial_t;
enum
{
	BIKMATERIAL_INVALID = (BIKMaterial_t)~0
};


//-----------------------------------------------------------------------------
// Main AVI interface
//-----------------------------------------------------------------------------
#define BIK_INTERFACE_VERSION "VBik001"

class IBik : public IAppSystem
{
public:
	// Create/destroy a BINK material (a materialsystem IMaterial)
	virtual BIKMaterial_t CreateMaterial( const char *pMaterialName, const char *pFileName, const char *pPathID ) = 0;
	virtual void DestroyMaterial( BIKMaterial_t hMaterial ) = 0;
	
	// Update the frame (if necessary)
	virtual bool Update( BIKMaterial_t hMaterial ) = 0;

	// Gets the IMaterial associated with an BINK material
	virtual IMaterial* GetMaterial( BIKMaterial_t hMaterial ) = 0;

	// Returns the max texture coordinate of the BINK
	virtual void GetTexCoordRange( BIKMaterial_t hMaterial, float *pMaxU, float *pMaxV ) = 0;

	// Returns the frame size of the BINK (stored in a subrect of the material itself)
	virtual void GetFrameSize( BIKMaterial_t hMaterial, int *pWidth, int *pHeight ) = 0;

	// Returns the frame rate of the BINK
	virtual int GetFrameRate( BIKMaterial_t hMaterial ) = 0;

	// Returns the total frame count of the BINK
	virtual int GetFrameCount( BIKMaterial_t hMaterial ) = 0;

	// Sets the frame for an BINK material (use instead of SetTime)
	virtual void SetFrame( BIKMaterial_t hMaterial, float flFrame ) = 0;

#ifdef WIN32
#if !defined( _X360 )
	// Sets the direct sound device that Bink will decode to
	virtual bool SetDirectSoundDevice( void	*pDevice ) = 0;
#else
	//needs to be called after xaudio is initialized
	virtual bool HookXAudio( void ) = 0;
#endif
#elif defined( LINUX )
	// Sets the SDL device that Bink will decode to
	virtual bool SetSDLDevice( int frequency, uint16 format, int nchannels ) = 0;

	// Called to mix in Bink audio. Returns true if audio was mixed in, false otherwise.
	virtual bool SDLMixerAudioCallback( void *stream, int len ) = 0;
#endif

	// Plays a given Bink file until it completes or the user presses ESC, SPACE, or ENTER
	virtual void PlayBinkVideo( const char *filename, void *mainWindow, int width, int height, float forcedMinTime ) = 0;
};



#endif // IBIK_H
