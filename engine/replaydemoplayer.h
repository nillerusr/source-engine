//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#if defined( REPLAY_ENABLED )

#ifndef REPLAYDEMOPLAYER_H
#define REPLAYDEMOPLAYER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/ireplaydemoplayer.h"
#include "replay/ireplaymovie.h"
#include "cl_demo.h"
#include "utlstring.h"

//----------------------------------------------------------------------------------------

class CReplay;

//----------------------------------------------------------------------------------------

class CReplayDemoPlayer : public CDemoPlayer,
						  public IReplayDemoPlayer
{
public:
	typedef CDemoPlayer BaseClass;

	CReplayDemoPlayer();

	virtual bool	StartPlayback( const char *pFilename, bool bAsTimeDemo );
	virtual void	StopPlayback();
	virtual void	OnLastDemoInLoopPlayed();
	virtual bool	ShouldLoopDemos();

	//
	// IReplayDemoPlayer
	//
	virtual void	PlayReplay( ReplayHandle_t hReplay, int iPerformance );
	virtual void	PlayNextReplay();
	virtual void	ClearReplayList();
	virtual void	AddReplayToList( ReplayHandle_t hReplay, int iPerformance );
	virtual CReplay	*GetCurrentReplay();
	virtual CReplayPerformance *GetCurrentPerformance();
	virtual void	PauseReplay();
	virtual bool	IsReplayPaused();
	virtual void	ResumeReplay();
	virtual void	OnSignonStateFull();

	//
	// CDemoPlayer
	//
	virtual void	OnStopCommand();
	virtual netpacket_t *ReadPacket();
	virtual float	GetPlaybackTimeScale();

private:
	void DisplayFailedToPlayMsg( int iPerformance );
	float CalcMovieLength() const;

	class CInStartPlaybackGuard
	{
	public:
		CInStartPlaybackGuard( bool &bState ) :	m_bState( bState ) { m_bState = true; }
		~CInStartPlaybackGuard() { m_bState = false; }
		bool	&m_bState;
	};

	struct PlaybackInfo_t
	{
		PlaybackInfo_t() : m_hReplay( REPLAY_HANDLE_INVALID ), m_iPerformance( -1 ),
			m_nStartTick( -1 ), m_nEndTick( -1 )  {}

		ReplayHandle_t	m_hReplay;
		int				m_iPerformance;
		int				m_nStartTick;
		int				m_nEndTick;
	};

	PlaybackInfo_t	*GetCurrentPlaybackInfo();
	const PlaybackInfo_t *GetCurrentPlaybackInfo() const;

	const CReplay	*GetCurrentReplay() const;

	CUtlVector< PlaybackInfo_t * > m_vecReplaysToPlay;
	IReplayMovie	*m_pMovie;
	int				m_nCurReplayIndex;
	bool			m_bInStartPlayback;
	bool			m_bStopCommandEncountered;	// We only want to handle OnStopCommand() once per playback
	float			m_flStartRenderTime;
	bool			m_bFullSignonStateReached;
};

//----------------------------------------------------------------------------------------

extern IDemoPlayer *g_pReplayDemoPlayer;

//----------------------------------------------------------------------------------------

#endif // REPLAYDEMOPLAYER_H

#endif // #if defined( REPLAY_ENABLED )
