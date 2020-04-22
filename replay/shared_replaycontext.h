//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SHARED_REPLAYCONTEXT_H
#define SHARED_REPLAYCONTEXT_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "baserecordingsessionmanager.h"
#include "baserecordingsessionblockmanager.h"
#include "errorsystem.h"

//----------------------------------------------------------------------------------------

class CBaseRecordingSessionManager;
class CBaseRecordingSessionBlockManager;
class CErrorSystem;
class IThreadPool;

//----------------------------------------------------------------------------------------

class CSharedReplayContext
{
public:
	CSharedReplayContext( IReplayContext *pOwnerContext );
	virtual ~CSharedReplayContext();

	// Sets up public data members and such
	virtual bool						Init( CreateInterfaceFn fnFactory );
	virtual void						Shutdown();

	virtual void						Think();

	virtual bool						IsInitialized() const	{ return m_bInit; }
	
	virtual const char					*GetRelativeBaseDir() const;
	virtual const char					*GetBaseDir() const;
	virtual const char					*GetReplaySubDir() const;

	IThreadPool							*m_pThreadPool;

	CBaseRecordingSessionManager		*m_pRecordingSessionManager;
	CBaseRecordingSessionBlockManager	*m_pRecordingSessionBlockManager;

	CErrorSystem						*m_pErrorSystem;

	CUtlString							m_strRelativeBasePath;	// eg: "/replay/server/"
	CUtlString							m_strBasePath;		// eg: "/user/home/tfadmin/tf/replay/server/"
	CUtlString							m_strSubDir;		// "client" or "server"

	bool								m_bInit;			// Initialized yet?  Set by outer class.

private:
	bool								InitThreadPool();
	void								EnsureDirHierarchy();

	IReplayContext						*m_pOwnerContext;
};

//----------------------------------------------------------------------------------------

#define LINK_TO_SHARED_REPLAYCONTEXT_IMP() \
	CSharedReplayContext	*m_pShared; \
	virtual bool			IsInitialized() const		{ return m_pShared && m_pShared->IsInitialized(); } \
	virtual const char		*GetRelativeBaseDir() const	{ return m_pShared->GetRelativeBaseDir(); } \
	virtual const char		*GetBaseDir() const			{ return m_pShared->GetBaseDir(); } \
	virtual const char		*GetReplaySubDir() const	{ return m_pShared->GetReplaySubDir(); } \
	virtual IReplayErrorSystem	*GetErrorSystem()		{ return m_pShared->m_pErrorSystem; } \
	virtual IRecordingSessionManager	*GetRecordingSessionManager() \
	{ \
		return m_pShared->m_pRecordingSessionManager; \
	} \
	virtual CBaseRecordingSession *GetRecordingSession( ReplayHandle_t hSession ) \
	{ \
		return static_cast< CBaseRecordingSession * >( m_pShared->m_pRecordingSessionManager->Find( hSession ) ); \
	} \
	virtual CBaseRecordingSessionBlockManager *GetRecordingSessionBlockManager() \
	{ \
		return m_pShared->m_pRecordingSessionBlockManager; \
	}

//----------------------------------------------------------------------------------------

class CJob;

bool RunJobToCompletion( IThreadPool *pThreadPool, CJob *pJob );

//----------------------------------------------------------------------------------------

#endif	// SHARED_REPLAYCONTEXT_H
