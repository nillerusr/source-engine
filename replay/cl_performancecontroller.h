//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef PERFORMANCECONTROLLER_H
#define PERFORMANCECONTROLLER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/ireplayperformancecontroller.h"
#include "basethinker.h"
#include "replay/replayhandle.h"
#include <utllinkedlist.h>

//----------------------------------------------------------------------------------------

class IReplayPerformancePlaybackHandler;
class IReplayPerformanceEditor;
class KeyValues;
class Vector;
class QAngle;
class CReplayPerformance;
class CReplay;
class CJob;

//----------------------------------------------------------------------------------------

class CPerformanceController : public CBaseThinker,
							   public IReplayPerformanceController
{
public:
	CPerformanceController();
	~CPerformanceController();

	//
	// IReplayPerforamnceController
	//
	virtual void		SetEditor( IReplayPerformanceEditor *pEditor );

	virtual void		StartRecording( CReplay *pReplay, bool bSnip );
	virtual void		Stop();	// Stop playback/recording
	virtual bool		SaveAsync();
	virtual bool		SaveAsAsync( const wchar_t *pTitle );

	virtual bool		IsSaving() const;

	virtual void		SaveThink();

	virtual bool		GetLastSaveStatus() const;

	virtual bool		IsRecording() const;
	virtual bool		IsPlaying() const;
	virtual bool		IsPlaybackDataLeft();
	virtual bool		IsDirty() const;
	virtual void		NotifyDirty();

	virtual void		OnSignonStateFull();
	virtual float		GetPlaybackTimeScale() const;

private:
	//
	// Common to recording/playback
	//
	void				Cleanup();
	void				CleanupDbgStream();
	void				CleanupStream();	// Cleans up m_pRoot if necessary
	float				GetTime() const;	// Get m_pCurEvent time
	void				ClearDirtyFlag();

	enum State_t
	{
		STATE_DORMANT = -1,	// Not playing back or recording
		STATE_RECORDING,
		STATE_PLAYING,
	};

	State_t				m_nState;

	//
	// CBaseThinker
	//
	virtual void		Think();
	virtual float		GetNextThinkTime() const;

	//
	// Recorder-specific:
	//
	virtual void		NotifyPauseState( bool bPaused );

	virtual void		Snip();
	virtual void		NotifyRewinding();
	virtual void		ClearRewinding();

	virtual bool		IsRewinding() const		{ return m_bRewinding; }
	virtual const KeyValues	*GetUnsavedRecordingData() const	{ return m_pRoot; }

	virtual void		AddEvent_Camera_Change_FirstPerson( float flTime, int nEntityIndex );
	virtual void		AddEvent_Camera_Change_ThirdPerson( float flTime, int nEntityIndex );
	virtual void		AddEvent_Camera_Change_Free( float flTime );
	virtual void		AddEvent_Camera_ChangePlayer( float flTime, int nEntIndex );
	virtual void		AddEvent_Camera_SetView( const SetViewParams_t &params );
	virtual void		AddEvent_TimeScale( float flTime, float flScale );

	void				CreateNewScratchPerformance( CReplay *pReplay );
	bool				DumpStreamToFileAsync( const char *pFullFilename );
	bool				FlushReplay();
	bool				IsCameraChangeEvent( int nType ) const;
	void				AddEvent( KeyValues *pEvent );
	void				RemoveDuplicateEventsFromQueue();

	ReplayHandle_t		m_hReplay;
	CReplayPerformance	*m_pSavedPerformance;	// Points to the saved performance - scratch copies to saved - should not be modified directly
	CReplayPerformance	*m_pScratchPerformance;	// The working performance, ie the temporary performance we muck with until the user saves or discards
	bool				m_bRewinding;
	bool				m_bPaused;			// Maintain our own state for paused/playing for event queueing
	CUtlLinkedList< KeyValues * >	m_EventQueue;	// If user pauses and changes camera, etc, it gets queued here - if multiple camera changes occur, previous is stomped
	IReplayPerformanceEditor		*m_pEditor;	// Pointer to the editor UI in the client

	//
	// Playback-specific
	//
	void				PlaybackThink();
	void				ReadSetViewEvent( KeyValues *pEventSubKey, Vector &origin, QAngle &angles, float &fov,
	  									  float *pAccel, float *pSpeed, float *pRotFilter );
	void				DebugRender();
	bool				SetupPlaybackHandler();
	void				SetupPlaybackFromPerformance( CReplayPerformance *pPerformance );
	void				SetupPlaybackExistingStream();	// Don't load anything from disk - use what's already in memory - used for rewind
	void				FinishBeginPerformancePlayback();
	virtual CReplayPerformance	*GetPerformance();
	virtual CReplayPerformance	*GetSavedPerformance();
	virtual bool		HasSavedPerformance();

	KeyValues			*m_pRoot;
	KeyValues			*m_pCurEvent;
	KeyValues			*m_pDbgRoot;		// Copy of performance data for debug rendering
	KeyValues			*m_pSetViewEvent;
	bool				m_bViewOverrideMode;
	bool				m_bDirty;			// If we recorded at all, was anything changed?
	float				m_flLastCamSetViewTime;
	float				m_flTimeScale;

	CJob				*m_pSaveJob;
	bool				m_bLastSaveStatus;
	
	IReplayPerformancePlaybackHandler		*m_pPlaybackHandler;
};

//----------------------------------------------------------------------------------------

#endif // PERFORMANCECONTROLLER_H
