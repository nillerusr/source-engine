//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Panorama specific video player code
//=============================================================================//

#ifndef PANORAMA_VIDEO_PLAYER_H
#define PANORAMA_VIDEO_PLAYER_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/smartptr.h"
#include "../common/video/ivideoplayer.h"
#include "panorama/controls/panelptr.h"

namespace panorama
{

class IUIRenderEngine;
class IUIDoubleBufferedYUV420Texture;
class CPanoramaVideoPlayer;


//-----------------------------------------------------------------------------
// Purpose: Renders video frames from video player for tenfoot
//-----------------------------------------------------------------------------
class CVideoPlayerVideoRenderer : public IVideoPlayerVideoCallback
{
public:
	CVideoPlayerVideoRenderer( IUIRenderEngine *pSurface );
	virtual ~CVideoPlayerVideoRenderer();

	uint32 GetTextureID() { return m_unTextureID; }
	uint32 GetTextureWidth();
	uint32 GetTextureHeight();

	// IVideoPlayerVideoCallback
	virtual bool BPresentYUV420Texture( uint nWidth, uint nHeight, void *pPlaneY, void *pPlaneU, void *pPlaneV, uint unStrideY, uint unStrideU, uint unStrideV ) OVERRIDE;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif 

private:
	CInterlockedUInt m_unTextureID;

	// used by video threads
	IUIRenderEngine *m_pSurface;
	IUIDoubleBufferedYUV420Texture *m_pYUV420DoubleBufferedTexture;
};


//-----------------------------------------------------------------------------
// Purpose: Renders audio frames from video player for tenfoot
//-----------------------------------------------------------------------------
class CVideoPlayerAudioRenderer : public IVideoPlayerAudioCallback
{
public:
	CVideoPlayerAudioRenderer();
	virtual ~CVideoPlayerAudioRenderer();

	void SetPlaybackVolume( float flVolume );
	void MarkShuttingDown();
	void Shutdown();

	// IVideoPlayerAudioCallback
	virtual bool InitAudioOutput( int nSampleRate, int nChannels ) OVERRIDE;
	virtual void FreeAudioOutput() OVERRIDE;
	virtual bool IsReadyForAudioData() OVERRIDE;
	virtual void *GetAudioBuffer() OVERRIDE;
	virtual uint32 GetAudioBufferSize() OVERRIDE;
	virtual uint32 GetAudioBufferMinSize() OVERRIDE;
	virtual void CommitAudioBuffer( uint32 unBytes ) OVERRIDE;
	virtual uint32 GetRemainingCommittedAudio() OVERRIDE;
	virtual uint32 GetMixedMilliseconds() OVERRIDE;
	virtual uint32 GetPlaybackLatency() OVERRIDE;
	virtual void Pause() OVERRIDE;
	virtual void Resume() OVERRIDE;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif 

private:
	bool OnInitAudioMainThread( CVideoPlayerAudioRenderer *pThis, int nSampleRate, int nChannels );
	bool OnFreeAudioMainThread( CVideoPlayerAudioRenderer *pThis );

	CInterlockedInt m_bShuttingDown;
	CThreadEvent m_eventWait;
#ifdef SUPPORTS_AUDIO
	IAudioOutputStream *m_pAudioStream;
#endif
	float m_flVolume;
};


//-----------------------------------------------------------------------------
// Purpose: Dispatches events to main panorama thread
//-----------------------------------------------------------------------------
typedef CUtlDelegate< void ( EVideoPlayerEvent ) > VideoPlayerEventDelegate_t;
class CVideoPlayerEventDispatcher : public IVideoPlayerEventCallback
{
public:
	CVideoPlayerEventDispatcher( CPanoramaVideoPlayer *pPlayer );
	~CVideoPlayerEventDispatcher();

	void RegisterEventListener( IUIPanel *pPanel );
	void UnregisterEventListener( IUIPanel *pPanel );

	void RegisterEventCallback( VideoPlayerEventDelegate_t del );
	void UnregisterEventCallback( VideoPlayerEventDelegate_t del );

	bool VideoPlayerEventUIThread( CVideoPlayerEventDispatcher *pDispatcher, EVideoPlayerEvent eEvent );

	// IVideoPlayerEventCallback
	virtual void VideoPlayerEvent( EVideoPlayerEvent eEvent ) OVERRIDE;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif 

private:
	void DispatchVideoEvent( EVideoPlayerEvent eEvent, IUIPanel *pTarget );

	CPanoramaVideoPlayer *m_pPlayer;
	CUtlVector< CPanelPtr< IUIPanel > > m_vecListeners;
	CUtlVector< VideoPlayerEventDelegate_t > m_vecCallbacks;
	double m_flLastRepeatEventDispatch;
};


//-----------------------------------------------------------------------------
// Purpose: Helper to create a tenfoot video player
//-----------------------------------------------------------------------------
class CPanoramaVideoPlayer : public IVideoPlayer, public ::CRefCount
{
public:
	CPanoramaVideoPlayer( IUIPanel *pPanel );
	CPanoramaVideoPlayer( IUIRenderEngine *pSurface );
	virtual ~CPanoramaVideoPlayer();

	virtual uint32 GetTextureID() { return m_videoCallback.GetTextureID(); }
	uint32 GetTextureWidth() { return m_videoCallback.GetTextureWidth(); }
	uint32 GetTextureHeight() { return m_videoCallback.GetTextureHeight(); }

	void RegisterEventListener( IUIPanel *pPanel ) { m_eventCallback.RegisterEventListener( pPanel ); }
	void UnregisterEventListener( IUIPanel *pPanel ) { m_eventCallback.UnregisterEventListener( pPanel ); }
	
	void RegisterEventCallback( VideoPlayerEventDelegate_t del ) { m_eventCallback.RegisterEventCallback( del ); }
	void UnregisterEventCallback( VideoPlayerEventDelegate_t del ) { m_eventCallback.UnregisterEventCallback( del ); }

	void SetPlaybackVolume( float flVolume ) { m_audioCallback.SetPlaybackVolume( flVolume ); }

	// IVideoPlayer
	virtual bool BLoad( const char *pchURL ) OVERRIDE;
	virtual bool BLoad( const byte *pubData, uint cubData ) OVERRIDE;
	virtual void Play() OVERRIDE { m_pVideoPlayer->Play(); }
	virtual void Stop() OVERRIDE;
	virtual void Pause() OVERRIDE { m_pVideoPlayer->Pause(); }
	virtual void SetPlaybackSpeed( float flPlaybackSpeed ) OVERRIDE { m_pVideoPlayer->SetPlaybackSpeed( flPlaybackSpeed ); }
	virtual void Seek( uint unSeekMS ) OVERRIDE { m_pVideoPlayer->Seek( unSeekMS ); }
	virtual void SetRepeat( bool bRepeat ) OVERRIDE { m_pVideoPlayer->SetRepeat( bRepeat ); }
	virtual void SuggestMaxVeritcalResolution( int nHeight ) OVERRIDE { m_pVideoPlayer->SuggestMaxVeritcalResolution( nHeight ); }
	virtual EVideoPlayerPlaybackState GetPlaybackState() OVERRIDE { return m_pVideoPlayer->GetPlaybackState(); }
	virtual bool IsStoppedForBuffering() OVERRIDE { return m_pVideoPlayer->IsStoppedForBuffering(); }
	virtual float GetPlaybackSpeed() OVERRIDE { return m_pVideoPlayer->GetPlaybackSpeed(); }
	virtual uint32 GetDuration() OVERRIDE { return m_pVideoPlayer->GetDuration(); }
	virtual uint32 GetCurrentPlaybackTime() OVERRIDE { return m_pVideoPlayer->GetCurrentPlaybackTime(); }
	virtual EVideoPlayerPlaybackError GetPlaybackError() OVERRIDE { return m_pVideoPlayer->GetPlaybackError(); }
	virtual void GetVideoResolution( int *pnWidth, int *pnHeight ) OVERRIDE { m_pVideoPlayer->GetVideoResolution( pnWidth, pnHeight ); }
	virtual int GetVideoDownloadRate() OVERRIDE { return m_pVideoPlayer->GetVideoDownloadRate(); }
	virtual int GetVideoRepresentationCount() OVERRIDE { return m_pVideoPlayer->GetVideoRepresentationCount(); }
	virtual bool BGetVideoRepresentationInfo( int iRep, int *pnWidth, int *pnHeight ) OVERRIDE { return m_pVideoPlayer->BGetVideoRepresentationInfo( iRep, pnWidth, pnHeight ); }
	virtual int GetCurrentVideoRepresentation() OVERRIDE { return m_pVideoPlayer->GetCurrentVideoRepresentation(); }
	virtual void ForceVideoRepresentation( int iRep ) OVERRIDE { return m_pVideoPlayer->ForceVideoRepresentation( iRep ); }
	virtual void GetVideoSegmentInfo( int *pnCurrent, int *pnTotal ) OVERRIDE { m_pVideoPlayer->GetVideoSegmentInfo( pnCurrent, pnTotal );  }
	virtual bool BHasAudioTrack() OVERRIDE { return m_pVideoPlayer->BHasAudioTrack(); }

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif 

private:
	IVideoPlayer *m_pVideoPlayer;
	CVideoPlayerVideoRenderer m_videoCallback;
	CVideoPlayerAudioRenderer m_audioCallback;
	CVideoPlayerEventDispatcher m_eventCallback;
};

typedef CSmartPtr< CPanoramaVideoPlayer > CVideoPlayerPtr;

} // namespace panorama


#endif // PANORAMA_VIDEO_PLAYER_H