//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#pragma warning (disable:4786)
#include <string>

#include "Player.h"
#include "MatchInfo.h"
using namespace std;


CPlayer::CPlayer()
:teams()
{
	WONID=pid=svrPID=-1;
	logontime=logofftime=reconnects=0;
	name="uninitted";
}

void CPlayer::nameFound(time_t t, string alias)
{
	if (aliases.atTime(t)!=alias) 
	{
		aliases.add(t,alias); 
	}
	name=aliases.favourite();
}

time_t CPlayer::plr_per_team_data::timeOn()
{
	/*if (logofftime==0)
	{
		logofftime=g_pMatchInfo->logCloseTime();
	}
	
	return logofftime-logontime;
	*/
	return timeon;
}
time_t CPlayer::totalTimeOn()
{
	if (logofftime==0)
	{
		logofftime=g_pMatchInfo->logCloseTime();
	}
	
	return logofftime-logontime;
}


double CPlayer::plr_per_team_data::rank()
{
	double d = (kills-deaths);
	double time=((double)timeOn())/1000.0;

	if (time < .000001)
		return d;

	return d/time;
}


string CPlayer::plr_per_team_data::faveWeapon()
{
	if (faveweapon=="" && weaponKills.begin()!=weaponKills.end())
	{
		faveweapkills=0;
		//noKills=false; 
		
		map<string,int>::iterator weapIt=weaponKills.begin();
		string& fave=(string&) (*weapIt).first;
		int faveKills=(*weapIt).second;
		
		for (weapIt;weapIt!=weaponKills.end();++weapIt)
		{
			const string& weapName=(*weapIt).first;
			int kills=(*weapIt).second;
			
			if (kills < faveKills)
				continue;
			
			fave=weapName;
			faveKills=kills;
		}
		
		faveweapkills=faveKills;
		faveweapon=fave;
	}
	
	return faveweapon;
}

int CPlayer::plr_per_team_data::faveWeapKills()
{
	if (faveweapkills==0)
	{
		//calculate favourite weapon stats
		faveWeapon();
	}
	return faveweapkills;
}

void CPlayer::merge()
{
	perteam[ALL_TEAMS].kills=perteam[0].kills+perteam[1].kills+perteam[2].kills+perteam[3].kills;
	perteam[ALL_TEAMS].deaths=perteam[0].deaths+perteam[1].deaths+perteam[2].deaths+perteam[3].deaths;
	perteam[ALL_TEAMS].suicides=perteam[0].suicides+perteam[1].suicides+perteam[2].suicides+perteam[3].suicides;
	perteam[ALL_TEAMS].teamkills=perteam[0].teamkills+perteam[1].teamkills+perteam[2].teamkills+perteam[3].teamkills;
	perteam[ALL_TEAMS].teamkilled=perteam[0].teamkilled+perteam[1].teamkilled+perteam[2].teamkilled+perteam[3].teamkilled;
	perteam[ALL_TEAMS].timeon=perteam[0].timeon+perteam[1].timeon+perteam[2].timeon+perteam[3].timeon;
	
	
	for (int i=0;i<MAX_TEAMS;i++)
	{
		map<std::string,int>::iterator it; 
		for (it=perteam[i].weaponKills.begin();it!=perteam[i].weaponKills.end();++it)
		{
			string weapname=(*it).first;
			int kills=(*it).second;
			perteam[ALL_TEAMS].weaponKills[weapname]+=kills;
		}
	}
	//this is probably only a shallow copy, but that's ok
	perteam[ALL_TEAMS].classesplayed=allclassesplayed;
}
