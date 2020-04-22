//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Interface of CMatchInfo
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#pragma warning(disable:4786)
#ifndef MATCHINFO_H
#define MATCHINFO_H
#ifdef WIN32
#pragma once
#endif
#include <map>
#include <string>
#include "EventList.h"
#include "util.h"
#include "player.h"
#include "time.h"
using namespace std;

//------------------------------------------------------------------------------------------------------
// Purpose: CMatchInfo is a collection of data gleaned from the logfile. An 
// instance of this class contains info that many different report elements and
// awards use, such as player names, teams, and things like that.
//------------------------------------------------------------------------------------------------------
class CMatchInfo
{
private:	
	CPlayerList players;
public:
	CPlayerListIterator playerBegin(){return players.begin();}
	CPlayerListIterator playerEnd(){return players.end();}
	
	CPlayerList& playerList(){return players;}
private:		
	string teamnames[MAX_TEAMS];
	bool team_exists[MAX_TEAMS];
	string servername;
	string mapname;

	
	
	int numPlrs;
	time_t logclosetime;
	time_t logopentime;
	CEventList* plogfile;
	bool bLanGame;
	
public:

	explicit CMatchInfo(CEventList* plf);

	bool isLanGame(){return bLanGame;}

	void generate();
	CEventList* eventList(){return plogfile;}

	int numPlayers(){return numPlrs;}
	string mapName(){return mapname;}

	
	char* playerName(PID pid,char* out){if (pid==1) return "PID=-1!"; strcpy(out,players[pid].name.c_str());return out;}
	string playerName(PID pid){return pid==-1?string("PID=-1!"):players[pid].name;}
	
	PID  getPlayerID(string name);
	//unsigned int  getPlayerWONID(string name);
	
	
	//unsigned int getPlayerWONID(PID pid){return players[pid].WONID;}
	
	//int playerTeamID(PID p){return players[p].team;}
	
	string teamName(int TID){return teamnames[TID];}
	int teamID(string teamname);
	bool teamExists(int tid){return team_exists[tid];}
	
	string getServerName(){return servername;}
	
	time_t getTimeOn(PID pid);
	time_t logCloseTime(){return logclosetime;}
	time_t logOpenTime(){return logopentime;}

	
};

extern CMatchInfo* g_pMatchInfo;

#endif // MATCHINFO_H
