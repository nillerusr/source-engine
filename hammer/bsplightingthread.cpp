//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "bsplightingthread.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



// --------------------------------------------------------------------------- //
// Global functions.
// --------------------------------------------------------------------------- //
IBSPLightingThread* CreateBSPLightingThread( IVRadDLL *pDLL )
{
	CBSPLightingThread *pRet = new CBSPLightingThread;

	if( pRet->Init( pDLL ) )
	{
		return pRet;
	}
	else
	{
		delete pRet;
		return 0;
	}
}

DWORD WINAPI ThreadMainLoop_Static( LPVOID lpParameter )
{
	return ((CBSPLightingThread*)lpParameter)->ThreadMainLoop();
}


// --------------------------------------------------------------------------- //
// Static helpers.
// --------------------------------------------------------------------------- //

class CCSLock
{
public:
					CCSLock( CRITICAL_SECTION *pCS )
					{
						EnterCriticalSection( pCS );
						m_pCS = pCS;
					}

					~CCSLock()
					{
						LeaveCriticalSection( m_pCS );
					}

	CRITICAL_SECTION *m_pCS;
};



// --------------------------------------------------------------------------- //
// 
// --------------------------------------------------------------------------- //

CBSPLightingThread::CBSPLightingThread()
{
	InitializeCriticalSection( &m_CS );
	
	m_hThread = 0;
	m_ThreadID = 0;
	
	m_ThreadCmd = THREADCMD_NONE;
	m_ThreadState = STATE_IDLE;
}


CBSPLightingThread::~CBSPLightingThread()
{
	if( m_hThread )
	{
		// Stop the current lighting process if one is going on.
		Interrupt();

		// Tell the thread to exit.
		SetThreadCmd( THREADCMD_EXIT );

		// Wait for the thread to exit.
		WaitForSingleObject( m_hThread, INFINITE );

		// Now we can close the thread handle.
		CloseHandle( m_hThread );
		m_hThread = NULL;
	}

	DeleteCriticalSection( &m_CS );
}


void CBSPLightingThread::Release()
{
	delete this;
}


void CBSPLightingThread::StartLighting( char const *pVMFFileWithEntities )
{
	// First, kill any lighting going on.
	Interrupt();

	// Store the VMF file data for the thread.
	int len = strlen( pVMFFileWithEntities ) + 1;
	m_VMFFileWithEntities.CopyArray( pVMFFileWithEntities, len );

	// Tell the thread to start lighting.
	SetThreadState( STATE_LIGHTING );
	SetThreadCmd( THREADCMD_LIGHT );
}


int CBSPLightingThread::GetCurrentState()
{
	return GetThreadState();
}


void CBSPLightingThread::Interrupt()
{
	if( GetThreadState() == STATE_LIGHTING )
	{
		m_pVRadDLL->Interrupt();
		
		while( GetThreadState() == STATE_LIGHTING )		
			Sleep( 10 );
	}
}


float CBSPLightingThread::GetPercentComplete()
{
	return m_pVRadDLL->GetPercentComplete();
}


bool CBSPLightingThread::Init( IVRadDLL *pDLL )
{
	m_pVRadDLL = pDLL;

	m_hThread = CreateThread(
		NULL,
		0,
		ThreadMainLoop_Static,
		this,
		0,
		&m_ThreadID );

	if( !m_hThread )
		return false;

	SetThreadPriority( m_hThread, THREAD_PRIORITY_LOWEST );
	return true;
}


DWORD CBSPLightingThread::ThreadMainLoop()
{
	while( 1 )
	{
		int cmd = GetThreadCmd();

		if( cmd == THREADCMD_NONE )
		{
			// Keep waiting for a new command.
			Sleep( 10 );
		}
		else if( cmd == THREADCMD_LIGHT )
		{
			if( m_pVRadDLL->DoIncrementalLight( m_VMFFileWithEntities.Base() ) )
				SetThreadState( STATE_FINISHED );
			else
				SetThreadState( STATE_IDLE );
		}
		else if( cmd == THREADCMD_EXIT )
		{
			return 0;
		}
	}
}


int CBSPLightingThread::GetThreadCmd()
{
	CCSLock lock( &m_CS );

	int ret = m_ThreadCmd;
	m_ThreadCmd = THREADCMD_NONE;

	return ret;
}


void CBSPLightingThread::SetThreadCmd( int cmd )
{
	CCSLock lock( &m_CS );
	m_ThreadCmd = cmd;
}


int CBSPLightingThread::GetThreadState()
{
	CCSLock lock( &m_CS );
	return m_ThreadState;
}


void CBSPLightingThread::SetThreadState( int state )
{
	CCSLock lock( &m_CS );
	m_ThreadState = state;
}


