//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CScoreboard
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef SCOREBOARD_H
#define SCOREBOARD_H
#ifdef WIN32
#pragma once
#endif
#pragma warning (disable:  4786)

#include "report.h"
#include <map>
#include <vector>
#include <string>
using namespace std;


//------------------------------------------------------------------------------------------------------
// Purpose: CScoreboard is a report element that outputs a scoreboard that tallies
// up all the kills and deaths for each player. it also displays (and sorts by) a 
// player's rank.  Rank is defined by how many kills a player got minus how many 
// times he died divided by the time he was in the game.
//------------------------------------------------------------------------------------------------------
class CScoreboard : public CReport
{
private:

	void init();
	
	public:
		explicit CScoreboard(){init();}
		
		void generate();
		void writeHTML(CHTMLFile& html);
};





#endif // SCOREBOARD_H
