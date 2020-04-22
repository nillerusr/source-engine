//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#pragma warning (disable:4786)
//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#ifndef PLRPERSIST_H
#define PLRPERSIST_H
#ifdef WIN32
#pragma once
#endif
#include <time.h>
#include <map>
#include <string>
#include <list>
#include <utility>
#include "TimeIndexedList.h"
#include "Player.h"
#include "TextFile.h"
using namespace std;

//------------------------------------------------------------------------------------------------------
// Purpose: Represents persistent player data. This class is used to save and load
//	Player data from the disk.  
//------------------------------------------------------------------------------------------------------
class CPlrPersist
{
public:
	unsigned long WONID;
	int kills;
	int deaths;
	time_t timeon;

	bool valid;

	string faveString(map<string,int>& theMap);

	map<string,int> nickmap;
	string faveName();
	map<string,int> weapmap;
	string faveWeap();
	map<string,int> classmap;
	string faveClass();
	list<pair<time_t,time_t> > playtimes;

	time_t lastplayed;
	
	int matches;
	int suicides;
	int faveweapkills;
	double rank();
	
	
	CPlrPersist()
	{
		kills=deaths=suicides=faveweapkills=matches=0;WONID=-1;
	}
	

	
	void read(unsigned long WONID);
	void read(CTextFile& f);
	void merge(CPlrPersist& cpp,bool mergeOverlaps=false);
	void generate(CPlayer& cp);
	void write();

	list<pair<time_t,time_t> >::iterator timesOverlap(time_t start, time_t end,bool testself=true);
};



#endif // PLRPERSIST_H
