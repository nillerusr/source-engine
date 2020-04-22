//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef TESTSCRIPTMGR_H
#define TESTSCRIPTMGR_H
#ifdef _WIN32
#pragma once
#endif


#include "filesystem.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utldict.h"

class CCommand;


// This represents a loop structure.
class CLoopInfo
{
public:
	int				m_nCount;			// How many times we've hit the loop.
	double			m_flStartTime;		// What time the loop started at.
	char			m_Name[64];
	unsigned long	m_iNextCommandPos;	// Position in the file of the next command.
	int				m_ListIndex;		// Index into CTestScriptMgr::m_Loops.
};


class CTestScriptMgr
{
public:
				CTestScriptMgr();
	virtual		~CTestScriptMgr();
	
	bool		StartTestScript( const char *pFilename );
	void		Term();
	bool		IsInitted() const;

	// The engine calls this when it gets to certain points during execution. The 'Test_StepTo' command
	// stops executing scripts until it reaches a checkpoint with a matching names.
	void		CheckPoint( const char *pName );

	// This pauses the script until execution reaches the specified checkpoint.
	// If bOnce is true, then it won't start waiting if the specified checkpoint was already hit.
	void		SetWaitCheckPoint( const char *pCheckPointName, bool bOnce=false );


private:
	// Runs until it hits a command that makes the script wait, like 'Test_Wait' or 'Test_StepTo'.
	void		RunCommands();

	// Returns true if we're waiting for the timer.
	bool		IsTimerWaiting() const;

	// Returns true if we're waiting for a checkpoint.
	bool		IsCheckPointWaiting() const;

	void		SetWaitTime( float flSeconds );

	CLoopInfo*	FindLoop( const char *pLoopName );
	void		StartLoop( const char *pLoopName );
	void		LoopCount( const char *pLoopName, int nTimes );
	void		LoopForNumSeconds( const char *pLoopName, double nSeconds );

	// Make sure the file is open. Error out if not.
	void		ErrorIfNotInitted();


private:
	FileHandle_t	m_hFile;
	char			m_NextCheckPoint[32];	// If this is > 0 length it will wait to run script until you hit
											// the next checkpoint with this name.

	double			m_WaitUntil;			// It waits to execute scripts until the time reaches here.


	CUtlLinkedList<CLoopInfo*,int>	m_Loops;
	CUtlDict<int, int> m_CheckPointsHit;	// Which checkpoints we've hit.

	// Console command handlers.
	friend void Test_Wait( const CCommand &args );
	friend void Test_RunFrame( const CCommand &args );
	friend void Test_StartLoop( const CCommand &args );
	friend void Test_LoopCount( const CCommand &args );
	friend void Test_Loop( const CCommand &args );
	friend void Test_LoopForNumSeconds( const CCommand &args );
};


inline CTestScriptMgr* GetTestScriptMgr()
{
	extern CTestScriptMgr g_TestScriptMgr;
	return &g_TestScriptMgr;
}


#endif // TESTSCRIPTMGR_H
