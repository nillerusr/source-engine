//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains lots of stuff that really doesn't fit anywhere else. Including
//	the implementations of the various Util:: functions
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "util.h" 
#include "TFStatsApplication.h"
#include <stdarg.h>
#include <string.h>
#include <vector>




using namespace std;

//------------------------------------------------------------------------------------------------------
// Function:	Util::string2svrID
// Purpose:	takes a string and extracts a server assigned ID out of it.
// Input:	s - the string from which to extract the server assigned ID
// Output:	int
//------------------------------------------------------------------------------------------------------
int Util::string2svrID(string s)
{
	const char* text=s.c_str();
	const char* read=&text[strlen(text)-1];
	while (read != text)
	{
		if (*read=='<' && *(read+1) != 'W') // if we've found a svrID
			break;
		
		read--;
	}
	
	if (read==text)
		return -1;

	int retval=-1;
	sscanf(read,"<%i>",&retval);
	return retval;

}
//friendly weapon names so users don't have to look at names like "gl_grenade"
map<string,string> Util::frWeapNmTbl;
void Util::initFriendlyWeaponNameTable()
{
	frWeapNmTbl["gl_grenade"]="Grenade Launcher";
	frWeapNmTbl["pipebomb"]="Pipebombs";
	frWeapNmTbl["timer"]="Infection";
	frWeapNmTbl["ac"]="Assault Cannon";
	frWeapNmTbl["infection"]="Infection";
	frWeapNmTbl["flames"]="Flames";
	frWeapNmTbl["rocket"]="Rocket Launcher";
	frWeapNmTbl["sentrygun"]="Sentry Gun";
	frWeapNmTbl["sniperrifle"]="Sniper Rifle";
	frWeapNmTbl["headshot"]="Head Shot";
	frWeapNmTbl["knife"]="Combat Knife";
	frWeapNmTbl["nails"]="Nail Gun";
	frWeapNmTbl["axe"]="Crowbar";
	frWeapNmTbl["shotgun"]="Shotgun";
	frWeapNmTbl["autorifle"]="Auto Rifle";
	frWeapNmTbl["supershotgun"]="Super Shotgun";
	frWeapNmTbl["supernails"]="Super Nailgun";
	frWeapNmTbl["railgun"]="Rail Gun";
	frWeapNmTbl["spanner"]="Spanner";

	//grenades
	frWeapNmTbl["caltrop"]="Caltrops";
	frWeapNmTbl["mirvgrenade"]="MIRV Grenade";
	frWeapNmTbl["nailgrenade"]="Nail Grenade";
	frWeapNmTbl["normalgrenade"]="Hand Grenade";
	frWeapNmTbl["gasgrenade"]="Gas Grenade";
	frWeapNmTbl["empgrenade"]="EMP Grenade";
}


//------------------------------------------------------------------------------------------------------
// Function:	getFriendlyWeaponName
// Purpose:	 turns a non-friendly weapon name into a friendly one
// Input:	s - the non-friendly weapon name which you want to make friendly
//		this function returns the non friendly name if the friendly one isn't found
// Output:	const string& 
//------------------------------------------------------------------------------------------------------
const string& Util::getFriendlyWeaponName(const string& s)
{
	if (frWeapNmTbl[s] == "")
		return s;
	else
		return frWeapNmTbl[s];
}

//map of team colors, indexed by team ID
const char* Util::teamcolormap[]=
{
	{"blue"},
	{"red"},
	{"yellow"},
	{"green"},
};


//friendly english stuff
char* Util::Months[]=
{
	//{""},
	{"January"},
	{"February"},
	{"March"},
	{"April"},
	{"May"},
	{"June"},
	{"July"},
	{"August"},
	{"September"},
	{"October"},
	{"November"},
	{"December"}
};
char* Util::numberSuffixes[]=
{
	{"th"},
	{"st"},
	{"nd"},
	{"rd"}
};
char* Util::daysofWeek[]=
{
	{"Sunday"},
	{"Monday"},
	{"Tuesday"},
	{"Wednesday"},
	{"Thursday"},
	{"Friday"},
	{"Saturday"}
};
char* Util::ampm[]=
{
	{"am"},
	{"pm"}
};

//english names, indexed by enumeration player_class in util.h
char* plrClassNames[]={
	{"Undefined"},
	{"scout"},
	{"sniper"},
	{"soldier"},
	{"demoman"},
	{"medic"},
	{"hwguy"},
	{"pyro"},
	{"spy"},
	{"engineer"},
	{"civilian"},
	{"RandomPC"},
	{"observer"},
};
#define NUM_CLASSES 12


//------------------------------------------------------------------------------------------------------
// Function:	playerClassNameToClassID
// Purpose:	 determines the classID for the given class Name and returns it
// Input:	plrClass - the name of the class.
// Output:	player_class
//------------------------------------------------------------------------------------------------------
player_class playerClassNameToClassID(const char* plrClass)
{
	for (int i=0;i<NUM_CLASSES;i++)
	{
		if (stricmp(plrClass,plrClassNames[i])==0)
			return (player_class)i;
	}
	return PC_UNDEFINED;
}



//------------------------------------------------------------------------------------------------------
// Function:	Util::time_t2hours
// Purpose:	returns how many hours are in the given time
// Input:	tmr - the time to convert
// Output:	int
//------------------------------------------------------------------------------------------------------
int Util::time_t2hours(time_t tmr)
{
	tm* pstart=gmtime(&tmr);
	if (!pstart)
		return 0;

	return pstart->tm_hour;
}

//------------------------------------------------------------------------------------------------------
// Function:	Util::time_t2mins
// Purpose:	 returns how many minutes of the hour are in the given time.
// Input:	tmr - the time to convert
// Output:	int
//------------------------------------------------------------------------------------------------------
int Util::time_t2mins(time_t tmr)
{
	tm* pstart=gmtime(&tmr);
	if (!pstart)
		return 0;

	return pstart->tm_min;
}

//------------------------------------------------------------------------------------------------------
// Function:	Util::time_t2secs
// Purpose:	 returns how many seconds of the minute are in the given time
// Input:	tmr - the time to convert
// Output:	int
//------------------------------------------------------------------------------------------------------
int Util::time_t2secs(time_t tmr)
{
	tm* pstart=gmtime(&tmr);
	if (!pstart)
		return 0;

	return pstart->tm_sec;
}

//------------------------------------------------------------------------------------------------------
// Function:	str2lowercase
// Purpose:	 portable _strlwr.  linux doesn't support _strlwr
// Input:	out - destination of lower case string
//				in - string to lowercasify
//------------------------------------------------------------------------------------------------------
void Util::str2lowercase(char* out , const char* in)
{
	while(*in)
	{
		*(out++)=tolower(*in++);
	}
	*out=0;
}



//#define _DIRDEBUG
void Util::debug_dir_printf(char* fmt,...)
{
#ifdef _DEBUG
#ifdef _DIRDEBUG
	va_list v;
	va_start(v,fmt);
	vfprintf(stdout,fmt,v);
#endif
#endif
}




const char* Util::makeDurationString(time_t start, time_t end,char* out,char* tostr)
{
	//TODO:
	//handle case where start and end dates are not the same day

	tm* pstart=localtime(&start);
	if (!pstart)
	{
		sprintf(out,"");
		return out;
	}
	
	int sday=pstart->tm_mday;
	
	int sweekday=pstart->tm_wday;
	int smo=pstart->tm_mon;
	int syear=pstart->tm_year+1900;

	int shour=pstart->tm_hour;
	if (pstart->tm_isdst)
		shour=(shour+23)%24; //this substracts 1 while accounting for backing up past 0
	int smin=pstart->tm_min;
	
	tm* pend=localtime(&end);
	if (!pend)
		pend=pstart;
	int ehour=pend->tm_hour;
	int emin=pend->tm_min;
	if (pend->tm_isdst)
		ehour=(ehour+23)%24; //this substracts 1 while accounting for backing up past 0

	
	char* matchtz=NULL;
	matchtz=g_pApp->os->gettzname()[0];

	sprintf(out,"%02li:%02li%s%02li:%02li %s, %s %s %li%s %li",shour,smin,tostr,ehour,emin,matchtz, Util::daysofWeek[sweekday],Util::Months[smo],sday,(sday%10)<4?Util::numberSuffixes[sday%10]:"th",syear);

	return out;
}
