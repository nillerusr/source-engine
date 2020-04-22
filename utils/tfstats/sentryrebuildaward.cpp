//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of CSentryRebuildAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "SentryRebuildAward.h"

//------------------------------------------------------------------------------------------------------
// Function:	CSentryRebuildAward::getWinner
// Purpose:	scans to find whose sentry guns were built the most often
//------------------------------------------------------------------------------------------------------
void CSentryRebuildAward::getWinner()
{
	CEventListIterator it;
	
	for (it=g_pMatchInfo->eventList()->begin(); it != g_pMatchInfo->eventList()->end(); ++it)
	{
		if ((*it)->getType()==CLogEvent::BUILD)
		{
			string built=(*it)->getArgument(1)->getStringValue();
			
			if (stricmp(built.c_str(),"sentry") == 0)
			{
				PID builder=(*it)->getArgument(0)->asPlayerGetPID();
				numbuilds[builder]++;
				winnerID=builder;
				fNoWinner=false;
			}
		}
	}
	
	map<PID,int>::iterator builditer;

	for (builditer=numbuilds.begin();builditer!=numbuilds.end();++builditer)
	{
		int currID=(*builditer).first;
		if (numbuilds[currID]>numbuilds[winnerID])
			winnerID=currID;
	}
}

//------------------------------------------------------------------------------------------------------
// Function:	CSentryRebuildAward::noWinner
// Purpose:	writes html indicating that no one built any sentries
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CSentryRebuildAward::noWinner(CHTMLFile& html)
{
	html.write("No sentries were built during this match.");
}

//------------------------------------------------------------------------------------------------------
// Function:	CSentryRebuildAward::extendedinfo
// Purpose:	reports how many times the winner had to rebuild their sentry
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CSentryRebuildAward::extendedinfo(CHTMLFile& html)
{
	
	if (numbuilds[winnerID] == 1)
		html.write("No one had to rebuild a sentry gun in this match.!",winnerName.c_str());
	else
		html.write("%s had to build a sentry gun %li times!",winnerName.c_str(),numbuilds[winnerID]);
}

