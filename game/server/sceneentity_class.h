//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef SCENEENTITY_CLASS_H
#define SCENEENTITY_CLASS_H
#ifdef _WIN32
#pragma once
#endif

#include "ichoreoeventcallback.h"

class CSceneListManager;

//-----------------------------------------------------------------------------
// Purpose: FIXME, need to deal with save/restore
//-----------------------------------------------------------------------------
class CSceneEntity : public CPointEntity, public IChoreoEventCallback
{
	friend class CInstancedSceneEntity;
public:

	enum
	{
		SCENE_ACTION_UNKNOWN = 0,
		SCENE_ACTION_CANCEL,
		SCENE_ACTION_RESUME,
	};

	enum
	{
		SCENE_BUSYACTOR_DEFAULT = 0,
		SCENE_BUSYACTOR_WAIT,
		SCENE_BUSYACTOR_INTERRUPT,
		SCENE_BUSYACTOR_INTERRUPT_CANCEL,
	};




	DECLARE_CLASS( CSceneEntity, CPointEntity );
	DECLARE_SERVERCLASS();

	CSceneEntity( void );
	~CSceneEntity( void );

	// From IChoreoEventCallback
	virtual void			StartEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );
	virtual void			EndEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );
	virtual void			ProcessEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );
	virtual bool			CheckEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );


	virtual int				UpdateTransmitState();
	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo );

	void					SetRecipientFilter( IRecipientFilter *filter );

	virtual void			Activate();

	virtual	void			Precache( void );
	virtual void			Spawn( void );
	virtual void			UpdateOnRemove( void );

	virtual void			OnRestore();
	virtual void			OnLoaded();

	virtual int				DrawDebugTextOverlays();

	DECLARE_DATADESC();

	virtual void			OnSceneFinished( bool canceled, bool fireoutput );

	virtual void			DoThink( float frametime );
	virtual void			PauseThink( void );

	bool					IsPlayingBack() const			{ return m_bIsPlayingBack; }
	bool					IsPaused() const				{ return m_bPaused; }
	bool					IsMultiplayer() const			{ return m_bMultiplayer; }

	bool					IsInterruptable();
	virtual void			ClearInterrupt();
	virtual void			CheckInterruptCompletion();

	virtual bool			InterruptThisScene( CSceneEntity *otherScene );
	void					RequestCompletionNotification( CSceneEntity *otherScene );

	virtual void			NotifyOfCompletion( CSceneEntity *interruptor );

	void					AddListManager( CSceneListManager *pManager );

	void					ClearActivatorTargets( void );

	void					SetBreakOnNonIdle( bool bBreakOnNonIdle ) { m_bBreakOnNonIdle = bBreakOnNonIdle; }
	bool					ShouldBreakOnNonIdle( void ) { return m_bBreakOnNonIdle; }

	// Inputs
	void InputStartPlayback( inputdata_t &inputdata );
	void InputPausePlayback( inputdata_t &inputdata );
	void InputResumePlayback( inputdata_t &inputdata );
	void InputCancelPlayback( inputdata_t &inputdata );
	void InputCancelAtNextInterrupt( inputdata_t &inputdata );
	void InputPitchShiftPlayback( inputdata_t &inputdata );
	void InputTriggerEvent( inputdata_t &inputdata );

	// If the scene is playing, finds an actor in the scene who can respond to the specified concept token
	void InputInterjectResponse( inputdata_t &inputdata );

	// If this scene is waiting on an actor, give up and quit trying.
	void InputStopWaitingForActor( inputdata_t &inputdata );

	virtual void StartPlayback( void );
	virtual void PausePlayback( void );
	virtual void ResumePlayback( void );
	virtual void CancelPlayback( void );
	virtual void PitchShiftPlayback( float fPitch );
	virtual void QueueResumePlayback( void );

	bool		 ValidScene() const;

	// Scene load/unload
	static CChoreoScene			*LoadScene( const char *filename, IChoreoEventCallback *pCallback );

	void					UnloadScene( void );

	struct SpeakEventSound_t
	{
		CUtlSymbol	m_Symbol;
		float		m_flStartTime;
	};

	static bool SpeakEventSoundLessFunc( const SpeakEventSound_t& lhs, const SpeakEventSound_t& rhs );

	bool					GetSoundNameForPlayer( CChoreoEvent *event, CBasePlayer *player, char *buf, size_t buflen );

	void					BuildSortedSpeakEventSoundsPrefetchList( 
		CChoreoScene *scene, 
		CUtlSymbolTable& table, 
		CUtlRBTree< SpeakEventSound_t >& soundnames, 
		float timeOffset );
	void					PrefetchSpeakEventSounds( CUtlSymbolTable& table, CUtlRBTree< SpeakEventSound_t >& soundnames );

	// Event handlers
	virtual void			DispatchStartExpression( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchEndExpression( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchStartFlexAnimation( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchEndFlexAnimation( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchStartGesture( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchEndGesture( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchStartLookAt( CChoreoScene *scene, CBaseFlex *actor, CBaseEntity *actor2, CChoreoEvent *event );
	virtual void			DispatchEndLookAt( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchStartMoveTo( CChoreoScene *scene, CBaseFlex *actor, CBaseEntity *actor2, CChoreoEvent *event );
	virtual void			DispatchEndMoveTo( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual	void			DispatchStartSpeak( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event, soundlevel_t iSoundlevel );
	virtual void			DispatchEndSpeak( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchStartFace( CChoreoScene *scene, CBaseFlex *actor, CBaseEntity *actor2, CChoreoEvent *event );
	virtual void			DispatchEndFace( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchStartSequence( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchEndSequence( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchStartSubScene( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchStartInterrupt( CChoreoScene *scene, CChoreoEvent *event );
	virtual void			DispatchEndInterrupt( CChoreoScene *scene, CChoreoEvent *event );
	virtual void			DispatchStartGeneric( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchEndGeneric( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );

	// NPC can play interstitial vcds (such as responding to the player doing something during a scene)
	virtual void			DispatchStartPermitResponses( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchEndPermitResponses( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );


	// Global events
	virtual void			DispatchProcessLoop( CChoreoScene *scene, CChoreoEvent *event );
	virtual void			DispatchPauseScene( CChoreoScene *scene, const char *parameters );
	virtual void			DispatchStopPoint( CChoreoScene *scene, const char *parameters );

	virtual float			EstimateLength( void );

	void					CancelIfSceneInvolvesActor( CBaseEntity *pActor );
	bool					InvolvesActor( CBaseEntity *pActor );		// NOTE: returns false if scene hasn't loaded yet

	void					GenerateSoundScene( CBaseFlex *pActor, const char *soundname );

	virtual float			GetPostSpeakDelay()	{ return 1.0; }

	bool					HasUnplayedSpeech( void );
	bool					HasFlexAnimation( void );

	void					SetCurrentTime( float t, bool forceClientSync );

	void					InputScriptPlayerDeath( inputdata_t &inputdata );

	// Data
public:
	string_t				m_iszSceneFile;

	string_t				m_iszResumeSceneFile;
	EHANDLE					m_hWaitingForThisResumeScene;
	bool					m_bWaitingForResumeScene;

	string_t				m_iszTarget1;
	string_t				m_iszTarget2;
	string_t				m_iszTarget3;
	string_t				m_iszTarget4;
	string_t				m_iszTarget5;
	string_t				m_iszTarget6;
	string_t				m_iszTarget7;
	string_t				m_iszTarget8;

	EHANDLE					m_hTarget1;
	EHANDLE					m_hTarget2;
	EHANDLE					m_hTarget3;
	EHANDLE					m_hTarget4;
	EHANDLE					m_hTarget5;
	EHANDLE					m_hTarget6;
	EHANDLE					m_hTarget7;
	EHANDLE					m_hTarget8;

	CNetworkVar( bool, m_bIsPlayingBack );
	CNetworkVar( bool, m_bPaused );
	CNetworkVar( bool, m_bMultiplayer );
	CNetworkVar( float, m_flForceClientTime );

	float					m_flCurrentTime;
	float					m_flFrameTime;
	bool					m_bCancelAtNextInterrupt;

	float					m_fPitch;

	bool					m_bAutomated;
	int						m_nAutomatedAction;
	float					m_flAutomationDelay;
	float					m_flAutomationTime;

	// A pause from an input requires another input to unpause (it's a hard pause)
	bool					m_bPausedViaInput;

	// Waiting for the actor to be able to speak.
	bool					m_bWaitingForActor;

	// Waiting for a point at which we can interrupt our actors
	bool					m_bWaitingForInterrupt;
	bool					m_bInterruptedActorsScenes;

	bool					m_bBreakOnNonIdle;

public:
	virtual CBaseFlex		*FindNamedActor( int index );
	virtual CBaseFlex		*FindNamedActor( CChoreoActor *pChoreoActor );
	virtual CBaseFlex		*FindNamedActor( const char *name );
	virtual CBaseEntity		*FindNamedEntity( const char *name, CBaseEntity *pActor = NULL, bool bBaseFlexOnly = false, bool bUseClear = false );
	virtual CBaseEntity		*FindNamedEntityFallback( const char *name, CBaseEntity *pActor = NULL, bool bBaseFlexOnly = false, bool bUseClear = false );
	CBaseEntity				*FindNamedTarget( string_t iszTarget, bool bBaseFlexOnly = false );
	virtual CBaseEntity		*FindNamedEntityClosest( const char *name, CBaseEntity *pActor = NULL, bool bBaseFlexOnly = false, bool bUseClear = false, const char *pszSecondary = NULL );

	EOZ_Hacks::ResponseFollowup m_followup;

private:

	CUtlVector< CHandle< CBaseFlex > >		m_hActorList;
	CUtlVector< CHandle< CBaseEntity > >	m_hRemoveActorList;

private:

	inline void				SetRestoring( bool bRestoring );

	// Prevent derived classed from using this!
	virtual void			Think( void ) {};


	void					ClearSceneEvents( CChoreoScene *scene, bool canceled );
	void					ClearSchedules( CChoreoScene *scene );

	float					GetSoundSystemLatency( void );
	void					PrecacheScene( CChoreoScene *scene );

	CChoreoScene			*GenerateSceneForSound( CBaseFlex *pFlexActor, const char *soundname );

	bool					CheckActors();

	void					PrefetchAnimBlocks( CChoreoScene *scene );

	bool					ShouldNetwork() const;
	// Set if we tried to async the scene but the FS returned that the data was not loadable
	bool					m_bSceneMissing;

	CChoreoScene			*m_pScene;
	CNetworkVar( int, m_nSceneStringIndex );

	const ConVar			*m_pcvSndMixahead;

	COutputEvent			m_OnStart;
	COutputEvent			m_OnCompletion;
	COutputEvent			m_OnCanceled;
	COutputEvent			m_OnTrigger1;
	COutputEvent			m_OnTrigger2;
	COutputEvent			m_OnTrigger3;
	COutputEvent			m_OnTrigger4;
	COutputEvent			m_OnTrigger5;
	COutputEvent			m_OnTrigger6;
	COutputEvent			m_OnTrigger7;
	COutputEvent			m_OnTrigger8;
	COutputEvent			m_OnTrigger9;
	COutputEvent			m_OnTrigger10;
	COutputEvent			m_OnTrigger11;
	COutputEvent			m_OnTrigger12;
	COutputEvent			m_OnTrigger13;
	COutputEvent			m_OnTrigger14;
	COutputEvent			m_OnTrigger15;
	COutputEvent			m_OnTrigger16;

	int						m_nInterruptCount;
	bool					m_bInterrupted;
	CHandle< CSceneEntity >	m_hInterruptScene;

	bool					m_bCompletedEarly;

	bool					m_bInterruptSceneFinished;
	CUtlVector< CHandle< CSceneEntity > >	m_hNotifySceneCompletion;
	CUtlVector< CHandle< CSceneListManager > >	m_hListManagers;

	bool					m_bRestoring;

	bool					m_bGenerated;
	string_t				m_iszSoundName;
	CHandle< CBaseFlex >	m_hActor;

	EHANDLE					m_hActivator;

	int						m_BusyActor;

	int						m_iPlayerDeathBehavior;

	CRecipientFilter		*m_pRecipientFilter;

public:
	void					SetBackground( bool bIsBackground );
	bool					IsBackground( void );
};




#endif // SCENEENTITY_CLASS_H
