//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of CKamikazeAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "KamikazeAward.h"

//------------------------------------------------------------------------------------------------------
// Function:	CKamikazeAward::getWinner
// Purpose:	determines the winner of the award
//------------------------------------------------------------------------------------------------------
void CKamikazeAward::getWinner()
{
	CEventListIterator it;
	
	for (it=g_pMatchInfo->eventList()->begin(); it != g_pMatchInfo->eventList()->end(); ++it)
	{
		if ((*it)->getType()==CLogEvent::SUICIDE || (*it)->getType()==CLogEvent::KILLED_BY_WORLD)
		{
			PID kami=(*it)->getArgument(0)->asPlayerGetPID();
			numdeaths[kami]++;
			winnerID=kami;
			fNoWinner=false;
		}
	}
	
	map<PID,int>::iterator kamiter;

	for (kamiter=numdeaths.begin();kamiter!=numdeaths.end();++kamiter)
	{
		int currID=(*kamiter).first;
		if (numdeaths[currID]>numdeaths[winnerID])
			winnerID=currID;
	}
}

//------------------------------------------------------------------------------------------------------
// Function:	CKamikazeAward::noWinner
// Purpose:	writes html indicating that no one won this award
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CKamikazeAward::noWinner(CHTMLFile& html)
{
	html.write("No one killed themselves during this match! Good work!");
}

//------------------------------------------------------------------------------------------------------
// Function:	CKamikazeAward::extendedinfo
// Purpose:	 reports how many times the winner killed him/herself
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CKamikazeAward::extendedinfo(CHTMLFile& html)
{
	if (numdeaths[winnerID]==1)
		html.write("%s suicided once.",winnerName.c_str());
	else
		html.write("%s was self-victimized %li times.",winnerName.c_str(),numdeaths[winnerID]);
}

