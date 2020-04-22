//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef LOCALNETWORKBACKDOOR_H
#define LOCALNETWORKBACKDOOR_H
#ifdef _WIN32
#pragma once
#endif

#include "quakedef.h"
#include "cl_localnetworkbackdoor.h"
#include "icliententitylist.h"
#include "LocalNetworkBackdoor.h"
#include "iclientnetworkable.h"
#include "basehandle.h"
#include "client_class.h"
#include "dt_localtransfer.h"
#include "client.h"
#include "cdll_engine_int.h"
#include "datacache/imdlcache.h"
#include "sys_dll.h"
#include "utllinkedlist.h"
#include "edict.h"
#include "server.h"


class SendTable;

class CLocalNetworkBackdoor
{
public:
	
	void StartEntityStateUpdate();
	void EndEntityStateUpdate();

	void EntityDormant( int iEnt, int iSerialNum );
	void AddToPendingDormantEntityList( unsigned short iEdict );
	void ProcessDormantEntities();

	void NotifyEdictFlagsChange( int iEdict )
	{
		// If they newly added the dontsend flag, then we need to run it through EntityDormant.
		if ( sv.edicts[iEdict].m_fStateFlags & FL_EDICT_DONTSEND )
			AddToPendingDormantEntityList( iEdict );
	}

	
	void EntState( 
		int iEnt, 
		int iSerialNum, 
		int iClass, 
		const SendTable *pSendTable, 
		const void *pSourceEnt,
		bool bChanged,
		bool bShouldTransmit );

	void ClearState();
	void StartBackdoorMode();
	void StopBackdoorMode();

	
	// This is called when the client DLL is loaded to precalculate data to let it copy data faster.
	static void InitFastCopy();


private:
	// Temporarily built up while processing a frame.
	CBitVec<MAX_EDICTS> m_EntsAlive;
	
	// This should correspond to which m_CachedEntState entries have a non-null m_pNetworkable pointer.
	CBitVec<MAX_EDICTS> m_PrevEntsAlive;

	// Entities that get created during a frame are remembered here.
	unsigned short m_EntsCreatedIndices[MAX_EDICTS];
	int m_nEntsCreated;

	// Entities that changed but weren't created go here.	
	unsigned short m_EntsChangedIndices[MAX_EDICTS];
	int m_nEntsChanged;

	// Tell the client DLL about entities that need to be notified about being dormant.
	// Anything that EntityDormant() would care about needs to get added to this list.
	CUtlLinkedList<unsigned short,unsigned short> m_PendingDormantEntities;

	// This data is cached in here so we don't have to call a lot of virtuals to get it from the client DLL.
	class CCachedEntState
	{
	public:
		CCachedEntState()
		{
			m_iSerialNumber = -1;
		}

		bool	m_bDormant;
		int		m_iSerialNumber;
		void	*m_pDataPointer;
		IClientNetworkable *m_pNetworkable;
	};
	
	CCachedEntState 	m_CachedEntState[MAX_EDICTS];
};


// The client will set this if it decides to use the fast path.
extern CLocalNetworkBackdoor *g_pLocalNetworkBackdoor;


#endif // LOCALNETWORKBACKDOOR_H
