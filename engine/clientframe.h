//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CLIENTFRAME_H
#define CLIENTFRAME_H
#ifdef _WIN32
#pragma once
#endif

#include <bitvec.h>
#include <const.h>
#include <tier1/mempool.h>

class CFrameSnapshot;

#define MAX_CLIENT_FRAMES	128

class CClientFrame
{
public:

	CClientFrame( void );
	CClientFrame( int tickcount );
	CClientFrame( CFrameSnapshot *pSnapshot );
	virtual ~CClientFrame();
	void Init( CFrameSnapshot *pSnapshot );
	void Init( int tickcount );

	// Accessors to snapshots. The data is protected because the snapshots are reference-counted.
	inline CFrameSnapshot*	GetSnapshot() const { return m_pSnapshot; };
	void					SetSnapshot( CFrameSnapshot *pSnapshot );
	void					CopyFrame( CClientFrame &frame );
	virtual bool		IsMemPoolAllocated() { return true; }

public:

	// State of entities this frame from the POV of the client.
	int					last_entity;	// highest entity index
	int					tick_count;	// server tick of this snapshot

	// Used by server to indicate if the entity was in the player's pvs
	CBitVec<MAX_EDICTS>	transmit_entity; // if bit n is set, entity n will be send to client
	CBitVec<MAX_EDICTS>	*from_baseline;	// if bit n is set, this entity was send as update from baseline
	CBitVec<MAX_EDICTS>	*transmit_always; // if bit is set, don't do PVS checks before sending (HLTV only)

	CClientFrame*		m_pNext;

private:

	// Index of snapshot entry that stores the entities that were active and the serial numbers
	// for the frame number this packed entity corresponds to
	// m_pSnapshot MUST be private to force using SetSnapshot(), see reference counters
	CFrameSnapshot		*m_pSnapshot;
};

// TODO substitute CClientFrameManager with an intelligent structure (Tree, hash, cache, etc)
class CClientFrameManager
{
public:
	CClientFrameManager(void);
	virtual ~CClientFrameManager(void);

	int				AddClientFrame( CClientFrame *pFrame ); // returns current count
	CClientFrame	*GetClientFrame( int nTick, bool bExact = true );
	void			DeleteClientFrames( int nTick );	// -1 for all
	int				CountClientFrames( void );	// returns number of frames in list
	void			RemoveOldestFrame( void );  // removes the oldest frame in list

	CClientFrame*	AllocateFrame();

private:
	void			FreeFrame( CClientFrame* pFrame );

	CClientFrame	*m_Frames;		// updates can be delta'ed from here
	CClientFrame	*m_LastFrame;
	int				m_nFrames;
	CClassMemoryPool< CClientFrame >	m_ClientFramePool;
};

#endif // CLIENTFRAME_H
