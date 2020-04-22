//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "clientframe.h"
#include "framesnapshot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CClientFrame::CClientFrame( CFrameSnapshot *pSnapshot )
{
	last_entity = 0;
	transmit_always = NULL;	// bit array used only by HLTV and replay client
	from_baseline = NULL;
	tick_count = pSnapshot->m_nTickCount;
	m_pSnapshot = NULL;
	SetSnapshot( pSnapshot );
	m_pNext = NULL;
}

CClientFrame::CClientFrame( int tickcount )
{
	last_entity = 0;
	transmit_always = NULL;	// bit array used only by HLTV and replay client
	from_baseline = NULL;
	tick_count = tickcount;
	m_pSnapshot = NULL;
	m_pNext = NULL;
}

CClientFrame::CClientFrame( void )
{
	last_entity = 0;
	transmit_always = NULL;	// bit array used only by HLTV and replay client
	from_baseline = NULL;
	tick_count = 0;
	m_pSnapshot = NULL;
	m_pNext = NULL;
}

void CClientFrame::Init( int tickcount )
{
	tick_count = tickcount;
}

void CClientFrame::Init( CFrameSnapshot *pSnapshot )
{
	tick_count = pSnapshot->m_nTickCount;
	SetSnapshot( pSnapshot );
}

CClientFrame::~CClientFrame()
{
	SetSnapshot( NULL );	// Release our reference to the snapshot.

	if ( transmit_always != NULL )
	{
		delete transmit_always;
		transmit_always = NULL;
	}
}

void CClientFrame::SetSnapshot( CFrameSnapshot *pSnapshot )
{
	if ( m_pSnapshot == pSnapshot )
		return;

	if( pSnapshot )
		pSnapshot->AddReference();

	if ( m_pSnapshot )
		m_pSnapshot->ReleaseReference();

	m_pSnapshot = pSnapshot;
}

void CClientFrame::CopyFrame( CClientFrame &frame )
{
	tick_count = frame.tick_count;	
	last_entity = frame.last_entity;
	
	SetSnapshot( frame.GetSnapshot() ); // adds reference to snapshot

	transmit_entity = frame.transmit_entity;

	if ( frame.transmit_always )
	{
		Assert( transmit_always == NULL );
		transmit_always = new CBitVec<MAX_EDICTS>;
		*transmit_always = *(frame.transmit_always);
	}
}

CClientFrame *CClientFrameManager::GetClientFrame( int nTick, bool bExact )
{
	if ( nTick < 0 )
		return NULL;

	CClientFrame *frame = m_Frames;
	CClientFrame *lastFrame = frame;

	while ( frame != NULL )
	{
		if ( frame->tick_count >= nTick  )
		{
			if ( frame->tick_count == nTick )
				return frame;
			
			if ( bExact )
				return NULL;

			return lastFrame;
		}

		lastFrame = frame;
		frame = frame->m_pNext;	
	}

	if ( bExact )
		return NULL;
	
	return lastFrame;
}

int	CClientFrameManager::CountClientFrames( void )
{

#if _DEBUG
	int count = 0;
	CClientFrame *f = m_Frames;
	while ( f )
	{
		count++;
		f = f->m_pNext;
	}
	Assert( m_nFrames == count );
#endif

	return m_nFrames;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *cl -
//-----------------------------------------------------------------------------
int CClientFrameManager::AddClientFrame( CClientFrame *frame)
{
	Assert( frame->tick_count > 0 );

	if ( !m_Frames )
	{
		// first client frame at all
		Assert( m_LastFrame == NULL && m_nFrames == 0 );
		m_Frames = frame;
		m_LastFrame = frame;
		m_nFrames = 1;
		return 1;
	}

	Assert( m_Frames != NULL && m_nFrames > 0 );
	Assert( m_LastFrame->m_pNext == NULL );
	m_LastFrame->m_pNext = frame;
	m_LastFrame = frame;
	return ++m_nFrames;
}

void CClientFrameManager::RemoveOldestFrame( void )
{
	CClientFrame *frame = m_Frames; // first

	if ( !frame )
		return;	// no frames at all

	Assert( m_nFrames > 0 );
	m_Frames = frame->m_pNext; // unlink head
	// deleting frame will decrease global reference counter
	FreeFrame( frame );

	if ( --m_nFrames == 0 )
	{
		Assert( m_LastFrame == frame && m_Frames == NULL );
		m_LastFrame = NULL;
	}
}

void CClientFrameManager::DeleteClientFrames(int nTick)
{
	
	if ( nTick < 0 )
	{
		while ( m_nFrames > 0 )
		{
			RemoveOldestFrame();
		}
	}
	else
	{
		CClientFrame *frame = m_Frames;
		// rebuild m_LastFrame while iterating forward through the list
		m_LastFrame = NULL;
		while ( frame )
		{
			if ( frame->tick_count < nTick )
			{
				// Delete this frame
				CClientFrame* next = frame->m_pNext;
				if ( m_Frames == frame )
					m_Frames = next;
				FreeFrame( frame );
				if ( --m_nFrames == 0 )
				{
					Assert( next == NULL );
					m_LastFrame = m_Frames = NULL;
					break;
				}
				Assert( m_LastFrame != frame && m_nFrames > 0 );
				frame = next;
				if ( m_LastFrame )
					m_LastFrame->m_pNext = next;
			}
			else
			{
				Assert( m_LastFrame == NULL || m_LastFrame->m_pNext == frame );
				m_LastFrame = frame;
				frame = frame->m_pNext;
			}
		}
	}



}


//-----------------------------------------------------------------------------
// Class factory for frames
//-----------------------------------------------------------------------------
CClientFrame* CClientFrameManager::AllocateFrame()
{
	return m_ClientFramePool.Alloc();
}

void CClientFrameManager::FreeFrame( CClientFrame* pFrame )
{
	if ( pFrame->IsMemPoolAllocated() )
	{
		m_ClientFramePool.Free( pFrame );
	}
	else
	{
		delete pFrame;
	}
}

CClientFrameManager::CClientFrameManager( void )
:	m_ClientFramePool( MAX_CLIENT_FRAMES, CUtlMemoryPool::GROW_SLOW ),
	m_Frames(NULL),
	m_LastFrame(NULL),
	m_nFrames(0)
{
}

CClientFrameManager::~CClientFrameManager( void )
{
	DeleteClientFrames( -1 );
	Assert( m_nFrames == 0 );
}
