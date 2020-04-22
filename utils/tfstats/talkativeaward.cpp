//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Implementation of CTalkativeAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "TalkativeAward.h"

//------------------------------------------------------------------------------------------------------
// Function:	CTalkativeAward::getWinner
// Purpose:	accumulates text for each player, and determines a winner
//------------------------------------------------------------------------------------------------------
void CTalkativeAward::getWinner()
{
	CEventListIterator it;
	
	for (it=g_pMatchInfo->eventList()->begin(); it != g_pMatchInfo->eventList()->end(); ++it)
	{
		if ((*it)->getType()==CLogEvent::SAY ||(*it)->getType()==CLogEvent::SAY_TEAM)
		{
			PID mouth=(*it)->getArgument(0)->asPlayerGetPID();
			fulltalktext[mouth]+=" ";
			fulltalktext[mouth]+=(*it)->getArgument(1)->getStringValue();
			numtalks[mouth]++;
			winnerID=mouth;
			fNoWinner=false;
		}
	}
	
	map<PID,int>::iterator talkiter;
	
	for (talkiter=numtalks.begin();talkiter!=numtalks.end();++talkiter)
	{
		int currID=(*talkiter).first;
		if (numtalks[currID]>numtalks[winnerID])
			winnerID=currID;
	}
}

//------------------------------------------------------------------------------------------------------
// Function:	CTalkativeAward::noWinner
// Purpose:	writes html to indicate that no one won this award.
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CTalkativeAward::noWinner(CHTMLFile& html)
{
	html.write("No one talked during this match.");
}

//------------------------------------------------------------------------------------------------------
// Function:	CTalkativeAward::extendedinfo
// Purpose:	calculates how many words a player said, and reports that.
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CTalkativeAward::extendedinfo(CHTMLFile& html)
{
	int i=0;
	char seps[]=" ";
	char* string= new char[fulltalktext[winnerID].length()+1];
	strcpy(string,fulltalktext[winnerID].c_str());
	char* token = strtok( string, seps );
	while( token != NULL )
	{
		token = strtok( NULL, seps );
		i++;
	}

	if (i==1)
		html.write("%s said only 1 word.",winnerName.c_str());
	else
		html.write("%s spoke %li words.",winnerName.c_str(),i);
	delete [] string;
}

