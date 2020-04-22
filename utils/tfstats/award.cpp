//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Implementation of CAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "Award.h"

//------------------------------------------------------------------------------------------------------
// Function:	CAward::CAward
// Purpose:	Constructor.
// Input:	name - the name of the award
//				pmi - a pointer to the match information
//------------------------------------------------------------------------------------------------------
CAward::CAward(char* name)
:awardName(name),fNoWinner(true),winnerID(-1)
{}

//------------------------------------------------------------------------------------------------------
// Function:	CAward::generate
// Purpose:	overrides generate to call the more-semantically correct getWinner()
// when dealing with subclasses of awards. Also after getWinner has determined
// which PID is the winner, this assigns the correct name to the winnerName field
//------------------------------------------------------------------------------------------------------
void CAward::generate()
{
	winnerName="";
	getWinner();
	if (winnerID!=-1)
		winnerName=g_pMatchInfo->playerName(winnerID);
}

//------------------------------------------------------------------------------------------------------
// Function:	CAward::writeHTML
// Purpose:	writes the award to the given html page
// Input:	html - the page to output the award to
//------------------------------------------------------------------------------------------------------
void CAward::writeHTML(CHTMLFile& html)
{
	if (fNoWinner || (winnerID == -1 && winnerName==""))
		noWinner(html);
	else
	{
		html.write("The <font class=brightawards>%s</font> award goes to %s!    ",awardName.c_str(),winnerName.c_str());
		extendedinfo(html);
	}
	html.br();
	html.write("\n");
}
CAward::~CAward()
{

}
