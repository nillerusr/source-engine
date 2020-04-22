//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of CSurvivalistAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "SurvivalistAward.h"

//------------------------------------------------------------------------------------------------------
// Function:	CSurvivalistAward::getWinner
// Purpose:	generates a winner for this award by determining who died the least
// while playing a scout
//------------------------------------------------------------------------------------------------------
void CSurvivalistAward::getWinner()
{
	map<PID,bool> isScout;		//to keep track of whether or not a player is a scout now
	map<PID,bool> wasScout;	//true if a player ever was a scout (and therefore is in contention for the award)
	
	CEventListIterator it;
	for (it=g_pMatchInfo->eventList()->begin(); it != g_pMatchInfo->eventList()->end(); ++it)
	{
		switch((*it)->getType())
		{
		case CLogEvent::CLASS_CHANGE:
			{
				PID pid=(*it)->getArgument(0)->asPlayerGetPID();
				player_class newpc=playerClassNameToClassID((*it)->getArgument(1)->getStringValue());
				
				if (newpc == PC_SCOUT)
				{
					wasScout[pid]=isScout[pid]=true;
					numdeaths[pid]=0;
				}
				else
					isScout[pid]=false;
			}
			break;
		case CLogEvent::FRAG:
		case CLogEvent::TEAM_FRAG:
			{
				PID dead=(*it)->getArgument(1)->asPlayerGetPID();
				if (isScout[dead])
					numdeaths[dead]++;
			}
			break;
		case CLogEvent::SUICIDE:
		case CLogEvent::KILLED_BY_WORLD:
			{
				PID dead=(*it)->getArgument(0)->asPlayerGetPID();
				if (isScout[dead])
					numdeaths[dead]++;
			}
			break;
		}
	}

	fNoWinner=true;
	winnerID=-1;

	map<PID,int>::iterator deathiter;

	for (deathiter=numdeaths.begin();deathiter!=numdeaths.end();++deathiter)
	{
		PID pid=(*deathiter).first;
		int deaths=(*deathiter).second;

		if (fNoWinner)
		{
			fNoWinner=false;
			winnerID=pid;
			continue;
		}

		if (deaths <= numdeaths[winnerID])
			winnerID=pid;
	}

}

//------------------------------------------------------------------------------------------------------
// Function:	CSurvivalistAward::noWinner
// Purpose:	writes html to indicate that no one won this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CSurvivalistAward::noWinner(CHTMLFile& html)
{
	html.write("No one was cured during this match.");
}

//------------------------------------------------------------------------------------------------------
// Function:	CSurvivalistAward::extendedinfo
// Purpose:	reports how many times the winner died
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CSurvivalistAward::extendedinfo(CHTMLFile& html)
{
	if (numdeaths[winnerID]==0)
		html.write("%s didn't die at all as a scout!",winnerName.c_str());
	else if (numdeaths[winnerID]==1)
		html.write("%s only died once as a scout!",winnerName.c_str());
	else
		html.write("%s only died %li times as a scout!",winnerName.c_str(),numdeaths[winnerID]);
}

