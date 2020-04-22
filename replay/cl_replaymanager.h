//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef CL_REPLAYMANAGER_H
#define CL_REPLAYMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "genericpersistentmanager.h"
#include "replay/ireplaymanager.h"
#include "cl_replaycontext.h"
#include "replay/replay.h"

//----------------------------------------------------------------------------------------

class IReplayFactory;

//----------------------------------------------------------------------------------------

//
// Manages and serializes all replays on the client.
//
class CReplayManager : public CGenericPersistentManager< CReplay >,
					   public IReplayManager
{
	typedef CGenericPersistentManager< CReplay > BaseClass;

public:
	CReplayManager();
	~CReplayManager();

	virtual bool	Init( CreateInterfaceFn fnCreateFactory );
	void			Shutdown();

	void			OnSessionStart();
	void			OnSessionEnd();
	int				GetNumReplaysDependentOnSession( ReplayHandle_t hSession );

	// IReplayManager
	virtual CReplay *GetReplay( ReplayHandle_t hReplay );
	virtual void	FlagReplayForFlush( CReplay *pReplay, bool bForceImmediate );
	virtual int		GetUnrenderedReplayCount();
	virtual void	DeleteReplay( ReplayHandle_t hReplay, bool bNotifyUI );
	virtual CReplay	*GetPlayingReplay();
	virtual CReplay	*GetReplayForCurrentLife();
	virtual void	GetReplays( CUtlLinkedList< CReplay *, int > &lstReplays );
	virtual void	GetReplaysAsQueryableItems( CUtlLinkedList< IQueryableReplayItem *, int > &lstReplays );
	virtual int		GetReplayCount() const	{ return Count(); }
	virtual float	GetDownloadProgress( const CReplay *pReplay );
	virtual const char	*GetReplaysDir() const;

	void			CommitPendingReplayAndBeginDownload();
	void			CompletePendingReplay();
	void			AddEventsForListen();
	void			ClearPendingReplay();
	void			SanityCheckReplay( CReplay *pReplay );
	void			SaveDanglingReplay();
	void			OnClientSideDisconnect();

	inline ObjContainer_t	&Replays() { return m_vecObjs; }

	bool			Commit( CReplay *pNewReplay );

	void			UpdateCurrentReplayDataFromServer();
	void			OnReplayRecordingCvarChanged();
	void			AttemptToSetupNewReplay();

	CReplay			*m_pReplayThisLife;	// Valid only between replay completion (ie player death) and player respawn, otherwise NULL

	//
	// CGenericPersistentManager
	//
	virtual const char	*GetRelativeIndexPath() const;

private:
	//
	// CGenericPersistentManager
	//
	virtual const char	*GetDebugName() const		{ return "replay manager"; }
	virtual const char	*GetIndexFilename() const	{ return "replays." GENERIC_FILE_EXTENSION; }
	virtual CReplay	*Create();
	virtual int		GetVersion() const;
	virtual void	Think();
	virtual IReplayContext *GetReplayContext() const;
	virtual bool	ShouldLoadObj( const CReplay *pReplay ) const;
	virtual void	OnObjLoaded( CReplay *pReplay );

	//
	// CBaseThinker
	//
	virtual float	GetNextThinkTime() const;

	void			DebugThink();
	void			InitReplay( CReplay *pReplay );
	IReplayFactory	*GetReplayFactory( CreateInterfaceFn fnCreateFactory );
	void			CleanupReplay( CReplay *&pReplay );
	void			FreeLifeIfNotSaved( CReplay *&pReplay );
	CReplay			*CreatePendingReplay();

	CReplay				*m_pPendingReplay;	// This is the replay we're currently recording - one which will
											// either be committed (via Commit()) or not, depending on whether
											// the player chooses to save the replay.

	CReplay				*m_pReplayLastLife;	// The previous life (ie between the player's previous spawn and the current spawn), if any (otherwise NULL)
	float				m_flPlayerSpawnCreateReplayFailTime;
	IReplayFactory		*m_pReplayFactory;
};

//----------------------------------------------------------------------------------------

inline CReplay *GetReplay( ReplayHandle_t hReplay )
{
	extern CClientReplayContext *g_pClientReplayContextInternal;
	return g_pClientReplayContextInternal->m_pReplayManager->GetReplay( hReplay );
}

//----------------------------------------------------------------------------------------

#define FOR_EACH_REPLAY( _i ) 	FOR_EACH_OBJ( CL_GetReplayManager(), _i )
#define GET_REPLAY_AT( _i ) 	CL_GetReplayManager()->m_vecObjs[ _i ]

//----------------------------------------------------------------------------------------

#endif // CL_REPLAYMANAGER_H
