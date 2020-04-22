//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of CSharpshooterAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "SharpshooterAward.h"

double CSharpshooterAward::HS_VALUE = 3.00;
double CSharpshooterAward::SHOT_VALUE = 1.00;

//------------------------------------------------------------------------------------------------------
// Function:	CSharpshooterAward::getWinner
// Purpose:	this totals up each sniper's score and determines a winner
//------------------------------------------------------------------------------------------------------
void CSharpshooterAward::getWinner()
{
	CEventListIterator it;
	
	for (it=g_pMatchInfo->eventList()->begin(); it != g_pMatchInfo->eventList()->end(); ++it)
	{
		if ((*it)->getType()==CLogEvent::FRAG)
		{
			if (strcmp((*it)->getArgument(2)->getStringValue(),"sniperrifle")==0)
			{
				PID pid=(*it)->getArgument(0)->asPlayerGetPID();
				sharpshooterscore[pid]+=SHOT_VALUE;
				numshots[pid]++;
			}
			else if (strcmp((*it)->getArgument(2)->getStringValue(),"headshot")==0)
			{
				PID pid=(*it)->getArgument(0)->asPlayerGetPID();
				sharpshooterscore[pid]+=HS_VALUE;
				numhs[pid]++;
			}
		}
	}
	
	int winnerScore=0;
	winnerID=-1;
	fNoWinner=true;

	map<PID,int>::iterator it2=sharpshooterscore.begin();
	for (it2;it2!=sharpshooterscore.end();++it2)
	{
		PID pid=(*it2).first;
		int score=(*it2).second;
		if (score > winnerScore)
		{
			winnerID=pid;
			winnerScore=score;
			fNoWinner=false;
		}
	}


}

//------------------------------------------------------------------------------------------------------
// Function:	CSharpshooterAward::noWinner
// Purpose:	 this writes html to indicate that no snipers got any kills
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CSharpshooterAward::noWinner(CHTMLFile& html)
{
	html.write("No one was sniped during this match");
}

//------------------------------------------------------------------------------------------------------
// Function:	CSharpshooterAward::extendedinfo
// Purpose:	this reports how many headshots and normal shots the winner got
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CSharpshooterAward::extendedinfo(CHTMLFile& html)
{
	int hs=numhs[winnerID];
	int shots=numshots[winnerID];
	if (hs && shots)
		html.write("%s got %li headshots and %li normal shots!",winnerName.c_str(),hs,shots);
	else if (hs && !shots)
		html.write("All of %s's %li snipes were headshots!",winnerName.c_str(),hs);
	else if (shots && !hs)
		html.write("%s sniped %li people!",winnerName.c_str(),shots);
}

