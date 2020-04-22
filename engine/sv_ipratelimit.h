//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: rate limits OOB server queries
//
//=============================================================================//

#ifndef SVIPRATELIMIT_H
#define SVIPRATELIMIT_H
#ifdef _WIN32
#pragma once
#endif

#include "netadr.h"
#include "sv_ipratelimit.h"
#include "convar.h"
#include "utlrbtree.h"

class CIPRateLimit
{
public:
	CIPRateLimit(ConVar *maxSec, ConVar *maxWindow, ConVar *maxSecGlobal);
	~CIPRateLimit();

	// updates an ip entry, return true if the ip is allowed, false otherwise
	bool CheckIP( netadr_t ip );

private:
	bool CheckIPInternal( netadr_t ip );

	enum { MAX_TREE_SIZE = 32768, START_TREE_SIZE = 2048, FLUSH_TIMEOUT = 120 };

	typedef int ip_t;
	struct iprate_s
	{
		ip_t ip;
		long lastTime;
		int count;
	};

	static bool LessIP( const struct CIPRateLimit::iprate_s &lhs, const struct CIPRateLimit::iprate_s &rhs );

	CUtlRBTree< struct iprate_s, ip_t > m_IPTree;
	int m_iGlobalCount;
	long m_lLastTime;
	double m_flFlushTime;

	ConVar *m_maxSec;
	ConVar *m_maxWindow;
	ConVar *m_maxSecGlobal;
};

// returns false if this IP exceeds rate limits
bool CheckConnectionLessRateLimits( netadr_t & adr );

#endif // SVIPRATELIMIT_H
