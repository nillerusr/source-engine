//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CPlayer
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#pragma warning (disable:4786)
#ifndef PLAYER_H
#define PLAYER_H
#ifdef WIN32
#pragma once
#endif

#include <time.h>
#include <map>
#include <string>

#include "util.h"

#include "TimeIndexedList.h"
#include "pid.h"
class CMatchInfo;

class CPlayer
{
public:
	struct plr_per_team_data
	{
		int kills;
		int deaths;
		int suicides;
		//double rank;
		int teamkills;
		int teamkilled;
		std::map<std::string,int> weaponKills;
		std::string faveweapon;
		int faveweapkills;
		double rank();
		std::string faveWeapon();
		int faveWeapKills();
		plr_per_team_data(){kills=deaths=suicides=teamkills=teamkilled=faveweapkills=0;}
		
		CTimeIndexedList<player_class> classesplayed; //stores class, indexed by the time when that class was switched to.
		
		time_t timeon;
		time_t timeOn();
	};
	
	CTimeIndexedList<player_class> allclassesplayed; //stores class, indexed by the time when that class was switched to.
	
	CTimeIndexedList<int> teams;
	plr_per_team_data perteam[MAX_TEAMS+1];
	
	CTimeIndexedList<std::string> aliases; 
	std::string name; //this will be set to the favourite name of the player
	//int team;
	int svrPID;
	unsigned long WONID;
	string ipAddress;
	int reconnects;
	
	
	PID pid;
	time_t logontime;
	time_t logofftime;
	time_t totalTimeOn();



	
//	int teamID(){return team;}
	CPlayer();

	void nameFound(time_t t, std::string alias);	



	

	//merge stats from all teams into 5th "team" (all teams)
	void merge();
};

#include "pid.h"
typedef std::map<PID,CPlayer> CPlayerList;
typedef CPlayerList::iterator CPlayerListIterator;

#endif // PLAYER_H
