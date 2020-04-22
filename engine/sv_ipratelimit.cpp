//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles all the functions for implementing remote access to the engine
//
//=============================================================================//

#include "sv_ipratelimit.h"
#include "filesystem.h"
#include "sv_log.h"

static ConVar	sv_logblocks("sv_logblocks", "0", 0, "If true when log when a query is blocked (can cause very large log files)");


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CIPRateLimit::CIPRateLimit(ConVar *maxSec, ConVar *maxWindow, ConVar *maxSecGlobal)
:	m_IPTree( 0, START_TREE_SIZE, LessIP),
	m_maxSec( maxSec ),
	m_maxWindow( maxWindow ),
	m_maxSecGlobal( maxSecGlobal )
{
	m_iGlobalCount = 0;
	m_lLastTime = -1;
	m_flFlushTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CIPRateLimit::~CIPRateLimit()
{
}

//-----------------------------------------------------------------------------
// Purpose: sort func for rb tree
//-----------------------------------------------------------------------------
bool CIPRateLimit::LessIP( const struct CIPRateLimit::iprate_s &lhs, const struct CIPRateLimit::iprate_s &rhs )
{
	return ( lhs.ip < rhs.ip );
}

//-----------------------------------------------------------------------------
// Purpose: return false and potentially log a warning if this IP has exceeded limits
//-----------------------------------------------------------------------------
bool CIPRateLimit::CheckIP( netadr_t adr )
{
	bool ret = CheckIPInternal(adr);
	if ( !ret && sv_logblocks.GetBool() == true )
	{
		g_Log.Printf("Traffic from %s was blocked for exceeding rate limits\n", adr.ToString() );
	}
	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: return false if this IP has exceeded limits
//-----------------------------------------------------------------------------
bool CIPRateLimit::CheckIPInternal( netadr_t adr )
{
	// check the global count first in case we are being spammed
	m_iGlobalCount++;
	long curTime = (long)Plat_FloatTime();

	if( (curTime - m_lLastTime) > m_maxWindow->GetFloat() )
	{
		m_lLastTime = curTime;
		m_iGlobalCount = 1;
	}

	float query_rate = static_cast<float>( m_iGlobalCount ) / m_maxWindow->GetFloat(); // add one so the bottom is never zero
	if( m_maxSecGlobal->GetFloat() > FLT_EPSILON && query_rate > m_maxSecGlobal->GetFloat() ) 
	{
		return false;
	}

	
	// check the per ip rate (do this first, so one person dosing doesn't add to the global max rate
	ip_t clientIP;
	memcpy( &clientIP, adr.ip, sizeof(ip_t) );

	if( m_IPTree.Count() > MAX_TREE_SIZE && curTime > ( m_flFlushTime + FLUSH_TIMEOUT/4 ) ) // if we have stored too many items
	{
		m_flFlushTime = curTime;
		ip_t tmp = m_IPTree.LastInorder(); // we step BACKWARD's through the tree
		int i = m_IPTree.FirstInorder();
		while( (m_IPTree.Count() > (2*MAX_TREE_SIZE)/3) && i < m_IPTree.MaxElement() ) // trim 1/3 the entries from the tree and only traverse the max nodes
		{ 	
			if ( !m_IPTree.IsValidIndex( tmp ) )
				break;

			if ( (curTime - m_IPTree[ tmp ].lastTime) > FLUSH_TIMEOUT  &&
			      m_IPTree[ tmp ].ip != clientIP ) 
			{
				ip_t removeIPT = tmp;
				tmp = m_IPTree.PrevInorder( tmp );
				m_IPTree.RemoveAt( removeIPT );
				continue;
			}
			i++;
			tmp = m_IPTree.PrevInorder( tmp );
		}
	}

	// now find the entry and check if its within our rate limits
	struct iprate_s findEntry = { clientIP };
	ip_t entry = m_IPTree.Find( findEntry );

	if( m_IPTree.IsValidIndex( entry ) )
	{
		m_IPTree[ entry ].count++; // a new hit

		if( (curTime - m_IPTree[ entry ].lastTime) > m_maxWindow->GetFloat() )
		{
			m_IPTree[ entry ].lastTime = curTime;
			m_IPTree[ entry ].count = 1;
		}
		else
		{
			float flQueryRate = static_cast<float>( m_IPTree[ entry ].count) / m_maxWindow->GetFloat(); // add one so the bottom is never zero
			if( flQueryRate > m_maxSec->GetFloat() )
			{
				return false;
			}
		}
	}
	else
	{	
		// not found, insert this new guy
		struct iprate_s newEntry;
		newEntry.count = 1;
		newEntry.lastTime = curTime;
		newEntry.ip = clientIP;
		m_IPTree.Insert( newEntry );
	}


	return true;
}
