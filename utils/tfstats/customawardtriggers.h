//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Interfaces of the CustomAwardTrigger tree. Both types of
// Custom award triggers and their base class
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef CUSTOMAWARDTRIGGERS_H
#define CUSTOMAWARDTRIGGERS_H
#ifdef WIN32
#pragma once
#endif
#pragma warning(disable :4786)
#include "TextFile.h"
#include "LogEvent.h"
#include <vector>
#include <list>
#include <map>

using std::map;
using std::list;
using std::vector;
using std::string;
//------------------------------------------------------------------------------------------------------
// Purpose: CCustomAwardTrigger is the base class for both types of award 
// triggers. An award trigger is an object that recognizes a certain type of event
// in the log file, and if it matches that event, then it "triggers" and the custom
// award which owns it increments the counter for the player who triggered the
// trigger.
//------------------------------------------------------------------------------------------------------
class CCustomAwardTrigger
{
public:
	static CCustomAwardTrigger* readTrigger(CTextFile& f);
	int plrValue;
	int teamValue;
	
	map<string,string> extraProps;	
	
	virtual bool matches(const CLogEvent* le)=0;

	virtual PID  plrIDFromEvent(const CLogEvent* ple){return -1;}
	virtual string getTrackString(const CLogEvent* ple){return "";}
	CCustomAwardTrigger(int value, int tmVal, map<string,string> extras){plrValue=value;teamValue=tmVal;extraProps=extras;}
};


//------------------------------------------------------------------------------------------------------
// Purpose: CBroadcastTrigger scans broadcast events for matching data
//------------------------------------------------------------------------------------------------------
class CBroadcastTrigger: public CCustomAwardTrigger
{
public:
	CBroadcastTrigger(int value, int teamValue, vector<string>& keys,map<string,string> extras);
	vector<string> broadcastStrings;
	virtual bool matches(const CLogEvent* le);
	virtual PID  plrIDFromEvent(const CLogEvent* ple){return ple->getArgument(1)->asPlayerGetPID();}
	
	//this class doesn't need this function
	//virtual string getTrackString(const CLogEvent* ple){return "";}
};


//------------------------------------------------------------------------------------------------------
// Purpose: CGoalTrigger scans goal activations for matching data
//------------------------------------------------------------------------------------------------------
class CGoalTrigger: public CCustomAwardTrigger
{
public:
	CGoalTrigger(int value, int teamValue, vector<string>& keys,map<string,string> extras);
	vector<string> goalNames;
	virtual bool matches(const CLogEvent* le);
	virtual PID  plrIDFromEvent(const CLogEvent* ple){return ple->getArgument(0)->asPlayerGetPID();}
	
	//this class doesn't need this function
	//virtual string getTrackString(const CLogEvent* ple){return "";}
};


//------------------------------------------------------------------------------------------------------
// Purpose: CFullSearchTrigger scans FullSearch activations for matching data
//------------------------------------------------------------------------------------------------------
class CFullSearchTrigger: public CCustomAwardTrigger
{
public:
	int regExpCompare(string exp,string cmp);

	map<string,string> varexpressions;
	
	CFullSearchTrigger(int value, int teamValue, vector<string>& ks,map<string,string> extras);
	
	vector<string> keys;
	
	string winnerVar;
	
	bool compare(string str_msg,string str_key,map<string,string>& varmatches);
	virtual bool matches(const CLogEvent* le);
	virtual PID  plrIDFromEvent(const CLogEvent* ple);

	//this class does
	virtual string getTrackString(const CLogEvent* ple);
};



#endif // CUSTOMAWARDTRIGGERS_H
