//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BSPLIGHTINGTHREAD_H
#define BSPLIGHTINGTHREAD_H
#ifdef _WIN32
#pragma once
#endif


#include "stdafx.h"
#include "ibsplightingthread.h"
#include "utlvector.h"


class CBSPLightingThread : public IBSPLightingThread
{
public:

						CBSPLightingThread();


// IBSPLightingThread functions.
public:
	virtual				~CBSPLightingThread();
	virtual void		Release();
	virtual void		StartLighting( char const *pVMFFileWithEntities );
	virtual int			GetCurrentState();
	virtual void		Interrupt();
	virtual float		GetPercentComplete();


// Other functions.
public:

	// This is called immediately after the constructor. It creates the thread.
	bool				Init( IVRadDLL *pDLL );



// Threadsafe functions.
public:

	enum
	{
		THREADCMD_NONE=0,
		THREADCMD_LIGHT=1,
		THREADCMD_EXIT=2
	};

	// Get the current command to the thread. Resets to THREADCMD_NONE on exit.
	int					GetThreadCmd();
	void				SetThreadCmd( int cmd );

	// Returns an IBSPLightingThread::STATE_ define.
	int					GetThreadState();
	void				SetThreadState( int state );
	

public:

	// The thread's run function.
	DWORD				ThreadMainLoop();


public:

	int					m_ThreadCmd;		// Next command for the thread to run.
	int					m_ThreadState;		// Current state of the thread.

	CUtlVector<char>	m_VMFFileWithEntities;


public:
	IVRadDLL			*m_pVRadDLL;

	HANDLE				m_hThread;
	DWORD				m_ThreadID;
	CRITICAL_SECTION	m_CS;
};


#endif // BSPLIGHTINGTHREAD_H
