//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_MOVIEPLAYER_H
#define PANORAMA_MOVIEPLAYER_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "../data/iimagesource.h"
#include "../data/panoramavideoplayer.h"
#include "panorama/uischeduleddel.h"

namespace panorama
{
class CToggleButton;
class CLabel;
class CSlider;

DECLARE_PANEL_EVENT0( MoviePlayerAudioStart );
DECLARE_PANEL_EVENT0( MoviePlayerAudioStop );
DECLARE_PANEL_EVENT0( MoviePlayerPlaybackStart );
DECLARE_PANEL_EVENT0( MoviePlayerPlaybackStop );
DECLARE_PANEL_EVENT1( MoviePlayerPlaybackEnded, EVideoPlayerPlaybackError );
DECLARE_PANORAMA_EVENT0( MoviePlayerTogglePlayPause );
DECLARE_PANORAMA_EVENT0( MoviePlayerFastForward );
DECLARE_PANEL_EVENT0( MoviePlayerUIVisible );
DECLARE_PANORAMA_EVENT0( MoviePlayerJumpBack );
DECLARE_PANORAMA_EVENT0( MoviePlayerVolumeControl );
DECLARE_PANORAMA_EVENT0( MoviePlayerFullscreenControl );
DECLARE_PANORAMA_EVENT1( MoviePlayerSetRepresentation, int );
DECLARE_PANORAMA_EVENT0( MoviePlayerSelectVideoQuality );


//-----------------------------------------------------------------------------
// Purpose: Base class for controls that pop above movie button bar
//-----------------------------------------------------------------------------
class CMovieControlPopupBase : public CPanel2D
{
public:
	CMovieControlPopupBase( CPanel2D *pInvokingPanel, const char *pchPanelID );
	virtual ~CMovieControlPopupBase() {}

	void Show( float flVolume );
	void Close();

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;

protected:
	bool EventCancelled( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );

	CPanel2D *m_pInvisibleBackground;
	CPanel2D *m_pInvokingPanel;
	CPanel2D *m_pPopupBackground;
};


//-----------------------------------------------------------------------------
// Purpose: Top level menu for volume slider
//-----------------------------------------------------------------------------
class CVolumeSliderPopup : public CMovieControlPopupBase
{
	DECLARE_PANEL2D( CVolumeSliderPopup, CMovieControlPopupBase );

public:
	CVolumeSliderPopup( CPanel2D *pInvokingPanel, const char *pchPanelID );
	virtual ~CVolumeSliderPopup() {}

	void Show( float flVolume );
	virtual bool OnKeyDown( const KeyData_t &unichar ) OVERRIDE;

private:
	bool EventSliderValueChanged( const CPanelPtr< IUIPanel > &pPanel, float flValue );
	
	CSlider *m_pSlider;
};


//-----------------------------------------------------------------------------
// Purpose: Top level menu for showing video resolutions to select
//-----------------------------------------------------------------------------
class CMovieVideoQualityPopup : public CMovieControlPopupBase
{
	DECLARE_PANEL2D( CMovieVideoQualityPopup, CMovieControlPopupBase );

public:
	CMovieVideoQualityPopup( CPanel2D *pInvokingPanel, const char *pchPanelID );
	virtual ~CMovieVideoQualityPopup() {}

	void AddRepresentation( int iRep, int nHeight );
	void Show( int iFocusRep, int nVideoHeight );

private:
	struct Representation_t
	{
		int m_iRep;
		int m_nHeight;
	};

	bool EventSetRepresentation( int iRep );
	static bool SortRepresentations( const Representation_t &lhs, const Representation_t &rhs );
	
	CUtlVector< Representation_t > m_vecRepresentations;
};


//-----------------------------------------------------------------------------
// Purpose: Movie panel. Just displays the movie
//-----------------------------------------------------------------------------
class CMoviePanel : public CPanel2D
{
	DECLARE_PANEL2D( CMoviePanel, CPanel2D );

public:
	CMoviePanel( CPanel2D *parent, const char *pchPanelID );
	virtual ~CMoviePanel();

	CVideoPlayerPtr GetMovie() { return m_pVideoPlayer; }
	void SetMovie( const char *pchFile );
	void SetMovie( CVideoPlayerPtr pVideoPlayer );
	bool IsSet() { return (m_pVideoPlayer != NULL); }
	void Clear();
	void SetPlaybackVolume( float flVolume );
	void SuggestMovieHeight();

	virtual void Paint();

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) OVERRIDE;
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;	
	bool EventVideoPlayerInitialized( IVideoPlayer *pIMovie );

private:
	CVideoPlayerPtr m_pVideoPlayer;
};


//-----------------------------------------------------------------------------
// Purpose: Displays debug info for a movie
//-----------------------------------------------------------------------------
class CMovieDebug : public CPanel2D
{
	DECLARE_PANEL2D( CMovieDebug, CPanel2D );

public:
	CMovieDebug( CPanel2D *pParent, const char *pchID );
	virtual ~CMovieDebug() {}

	void Show( CVideoPlayerPtr pVideoPlayer );

private:
	void Update();

	CVideoPlayerPtr m_pVideoPlayer;
	CLabel *m_pDimensions;
	CLabel *m_pResolution;
	CLabel *m_pFileType;
	CLabel *m_pVideoSegment;
	CLabel *m_pVideoBandwidth;

	panorama::CUIScheduledDel m_scheduledUpdate;
};


//-----------------------------------------------------------------------------
// Purpose: Movie player. Includes UI
//-----------------------------------------------------------------------------
class CMoviePlayer : public CPanel2D
{
	DECLARE_PANEL2D( CMoviePlayer, CPanel2D );

public:
	CMoviePlayer( CPanel2D *parent, const char *pchPanelID );
	virtual ~CMoviePlayer();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	CVideoPlayerPtr GetMovie() { return m_pMoviePanel->GetMovie(); }
	void SetMovie( const char *pchFile );
	void SetMovie( CVideoPlayerPtr pVideoPlayer );
	bool IsSet() { return m_pMoviePanel->IsSet(); }
	void Clear();

	enum EAutoplay
	{
		k_EAutoplayOff,
		k_EAutoplayOnLoad,
		k_EAutoplayOnFocus
	};

	enum EControls
	{
		k_EControlsNone,
		k_EControlsMinimal,
		k_EControlsFull,
		k_EControlsInvalid
	};

	void SetAutoplay( EAutoplay eAutoPlay, bool bSkipPlay = false );
	void SetRepeat( bool bRepeat );
	void SetControls( EControls eControls );
	void SetControls( const char *pchControls );


	virtual bool OnGamePadDown( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnKeyTyped( const KeyData_t &unichar ) OVERRIDE;
	virtual panorama::IUIPanel *OnGetDefaultInputFocus() OVERRIDE;

	void Play();
	void Pause();
	void Stop();
	void TogglePlayPause();
	void FastForward();
	void Rewind();
	void SetPlaybackVolume( float flVolume );

	// title control
	void SetTitleText( const char *pchText );
	void ShowTitle( bool bImmediatelyVisible = false );
	void HideTitle();
	bool BAdjustingVolume();

	virtual bool OnMouseButtonDown( const MouseData_t &code );

protected:
	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties );

	static EControls EControlsFromString( const char *pchControls );

	bool EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel );
	bool EventInputFocusLost( const CPanelPtr< IUIPanel > &ptrPanel );
	bool EventMovieInitialized( IVideoPlayer *pIMovie );
	bool EventVideoPlayerPlaybackStateChanged( IVideoPlayer *pIMovie );
	bool EventVideoPlayerChangedRepresentation( IVideoPlayer *pIMovie );
	bool EventVideoPlayerEnded( IVideoPlayer *pIMovie );
	bool EventActivated( const CPanelPtr< IUIPanel > &ptrPanel, EPanelEventSource_t eSource );
	bool EventCancelled( const CPanelPtr< IUIPanel > &ptrPanel, EPanelEventSource_t eSource );
	bool EventMovieTogglePlayPause();
	bool EventMoviePlayerFastForward();
	bool EventMoviePlayerJumpBack();
	bool EventMoviePlayerVolumeControl();
	bool EventMoviePlayerSelectQuality();
	bool EventSoundVolumeChanged( ESoundType eSoundType, float flVolume );
	bool EventSoundMuteChanged( bool bMute );
	bool EventSetRepresentation( int iRep );

	void UpdateFullUI();
	void UpdateTimeline();
	void UpdatePlayPauseButton();
	void UpdatePlaybackSpeed();
	void Seek( uint unOffset );
	void RaisePlaybackStartEvents();
	void RaisePlaybackStopEvents();
	void DisplayControls( bool bVisible );
	void DisplayTimeline( bool bVisible );
	bool BAnyControlsVisible();
	bool BControlBarVisible();
	bool BTimelineVisible();
	void UpdateMovingPlayingStyle();
	void UpdateVolumeControls();
	void SetAudioVolumeStyle( CPanoramaSymbol symStyle );

	void ShowTitleInternal( bool bImmediatelyVisible = false );
	void HideTitleInternal();

private:
	CMoviePanel *m_pMoviePanel;
	CPanelPtr< CMovieDebug > m_ptrDebug;

	// minimal UI
	CPanel2D *m_pLoadingThrobber;
	CPanel2D *m_pPlayIndicator;
	CPanoramaSymbol m_symMoviePlaybackStyle;

	// title sections
	CPanel2D *m_pPlaybackTitleAndControls;
	CLabel *m_pPlaybackTitle;
	bool m_bExternalShowTitle;

	// full UI
	CPanel2D *m_pPlaybackControls;
	CPanel2D *m_pPlaybackProgressBar;
	CToggleButton *m_pPlayPauseBtn;
	CLabel *m_pPlaybackSpeed;
	CPanel2D *m_pTimeline;
	CPanel2D *m_pControlBarRow;
	CPanel2D *m_pVolumeControl;
	CLabel *m_pErrorMessage;
	CPanelPtr< CVolumeSliderPopup > m_ptrVolumeSlider;
	CPanelPtr< CMovieVideoQualityPopup > m_ptrVideoQualityPopup;
	CButton *m_pVideoQualityBtn;

	bool m_bInConstructor;
	bool m_bRaisedAudioStartEvent;
	bool m_bRaisedPlaybackStartEvent;
	bool m_bHadFocus;
	bool m_bCloseControlsOnPlay;

	EAutoplay m_eAutoplay;
	EControls m_eControls;
	bool m_bDisableActivatePause;
	bool m_bShowControlsNotFullscreen;
	bool m_bRepeat;
	bool m_bMuted;								// muted flag
	float m_flVolume;							// playback volume, defaults to movie volume setting
	int m_iDesiredVideoRepresentation;			// representation selected by user or -1. Video player might not yet have changed to playing this rep
};


} // namespace panorama

#endif // PANORAMA_MOVIEPLAYER_H
