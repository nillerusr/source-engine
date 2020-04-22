//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// hltvtest.h: hltv test system
//
//////////////////////////////////////////////////////////////////////

#ifndef HLTVTEST_H
#define HLTVTEST_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

class CHLTVServer;

class CHLTVTestSystem
{
public:
	CHLTVTestSystem(void);
	~CHLTVTestSystem(void);

	void RunFrame();
	bool StartTest(int nClients, const char *pszAddress);
	void RetryTest(int nClients);
	bool StopsTest();

protected:

	CUtlVector<CHLTVServer*>	m_Servers;
};

extern CHLTVTestSystem *hltvtest;	// The global HLTV server/object. NULL on xbox.

#endif // HLTVSERVER_H