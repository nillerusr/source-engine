//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of CCureAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "CureAward.h"

//------------------------------------------------------------------------------------------------------
// Function:	CCureAward::getWinner
// Purpose:	determines who cured the most people during the match
//------------------------------------------------------------------------------------------------------
void CCureAward::getWinner()
{
	CEventListIterator it;
	for (it=g_pMatchInfo->eventList()->begin(); it != g_pMatchInfo->eventList()->end(); ++it)
	{
		if ((*it)->getType()==CLogEvent::CURE)
		{
				PID doc=(*it)->getArgument(0)->asPlayerGetPID();
				numcures[doc]++;
				winnerID=doc;
				fNoWinner=false;
			}
	}
	
	map<PID,int>::iterator cureiter;

	
	for (cureiter=numcures.begin();cureiter!=numcures.end();++cureiter)
	{
		PID currID=(*cureiter).first;
		if (numcures[currID]>numcures[winnerID])
			winnerID=currID;
	}
}

//------------------------------------------------------------------------------------------------------
// Function:	CCureAward::noWinner
// Purpose:	writes html indicating that no one was cured during this match
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CCureAward::noWinner(CHTMLFile& html)
{
	html.write("No one was cured during this match.");
}
			
//------------------------------------------------------------------------------------------------------
// Function:	CCureAward::extendedinfo
// Purpose:	reports how many people the winner cured
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CCureAward::extendedinfo(CHTMLFile& html)
{
	if (numcures[winnerID]==1)
		html.write("%s cured 1 sick person!",winnerName.c_str());
	else
		html.write("%s healed cured %li sick people!",winnerName.c_str(),numcures[winnerID]);
}

