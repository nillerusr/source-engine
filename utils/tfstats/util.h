//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Util.h and Util.cpp provide lots of helper stuff for TFStats to work
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef UTIL_H
#define UTIL_H
#ifdef WIN32
#pragma once
#endif
#pragma warning (disable:4786)
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
using std::string;
using std::map;

#ifdef WIN32
#include <direct.h>
#include <time.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PERMIT (S_IRWXU|S_IRWXG|S_IRWXO)
#endif

//leave these global
enum Consts
{
	TEAM_BLUE=0,
	TEAM_RED,
	TEAM_GREEN,
	TEAM_YELLOW,
	TEAM_NONE=4,
	MAX_TEAMS=4,
	ALL_TEAMS=4,
};

//Player Class support
enum player_class
{
	PC_UNDEFINED=0,
	PC_SCOUT,
	PC_SNIPER,
	PC_SOLDIER,
	PC_DEMOMAN,
	PC_MEDIC,
	PC_HWGUY,
	PC_PYRO,
	PC_SPY,
	PC_ENGINEER,
	PC_CIVILIAN,
	PC_RANDOM,
	PC_OBSERVER,
};

//english names, indexed by above enumeration
extern char* plrClassNames[];
//linear search, ack.
player_class playerClassNameToClassID(const char* plrClass);


//time support functions
#include <time.h>

class Util
{
public:
	//get hours from time_t number
	static int time_t2hours(time_t tmr);
	//get minutes from time_t number
	static int time_t2mins(time_t tmr);
	//get seconds from time_t number
	static int time_t2secs(time_t tmr);

	//friendly english stuff
	static char* Months[];
	static char* numberSuffixes[];
	static char* daysofWeek[];
	static char* ampm[];

	static void debug_dir_printf(PRINTF_FORMAT_STRING char* fmt,...);

	static void str2lowercase(char* out, const char* in);

		//friendly weapon names so users don't have to look at names like "gl_grenade"
	static const string& getFriendlyWeaponName(const string& s);
	static void initFriendlyWeaponNameTable();
	//map of team colors, indexed by team ID
	static const char* teamcolormap[];
	
	static map<string,string> frWeapNmTbl;
	static int string2svrID(string s);
	
	
	static const char* makeDurationString(time_t start, time_t end,char* out,char* tostr=" - ");
};



#endif // UTIL_H


