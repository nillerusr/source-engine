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
// Purpose:  Implementation of all the custom award trigger classes
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#include "TFStatsApplication.h"
#include "CustomAwardTriggers.h"
#include "memdbg.h"
#include "util.h"

using namespace std;
//------------------------------------------------------------------------------------------------------
// Function:	CCustomAwardTrigger::readTrigger
// Purpose:	reads a trigger definition from the given rule file and returns a new trigger
// Input:	f - a pointer to the TextFile object that represents the rule file to read from
// Output:	CCustomAwardTrigger*
//------------------------------------------------------------------------------------------------------
CCustomAwardTrigger* CCustomAwardTrigger::readTrigger(CTextFile& f)
{
	CCustomAwardTrigger* retval=NULL;
	
	string type="fullsearch";
	vector<string> keys;
	int value = 1 ;
	int teamValue = 1;
	
	
	f.discard("{");
	map<string,string> extraProps;
	const char* token=f.getToken();
	
	while (token)	
	{
		if (stricmp(token,"value")==0)
		{
			f.discard("=");
			value=atoi(f.readString());
			f.discard(";");
		}
		else if (stricmp(token,"teamvalue")==0)
		{
			f.discard("=");
			teamValue=atoi(f.readString());
			f.discard(";");
		}
		else if (stricmp(token,"type")==0)
		{
			f.discard("=");
			type=f.readString();
			f.discard(";");
		}
		else if (stricmp(token,"key")==0)
		{
			f.discard("=");
			char lowerbuf[500];
			Util::str2lowercase(lowerbuf,f.readString());
			keys.push_back(lowerbuf);
			f.discard(";");
		}
		else if (stricmp(token,"}")==0)
		{
			break;
		}
		else
		{
			f.discard("=");
			char lowerbuf[500];
			char lowerbuf2[500];
			//oops, have to do this first.  CTextfile uses a static buffer to return strings
			Util::str2lowercase(lowerbuf2,token);
			Util::str2lowercase(lowerbuf,f.readString());
			extraProps[lowerbuf2]=lowerbuf;
			f.discard(";");
		}
				
		token=f.getToken();
	}

	if (type=="broadcast")
		retval= new TRACKED CBroadcastTrigger(value,teamValue,keys,extraProps);
	else if (type=="goal")
		retval = new TRACKED CGoalTrigger(value,teamValue,keys,extraProps);
	else if (type=="fullsearch")
		retval = new TRACKED CFullSearchTrigger(value,teamValue,keys,extraProps);
	else
		g_pApp->fatalError("Invalid trigger type while parsing %s:\n\"%s\" is not a valid trigger type, please use \"broadcast\", \"goal\"  or \"fullsearch\"",f.fileName().c_str(),type);
	
	return retval;
}

//------------------------------------------------------------------------------------------------------
// Function: CBroadcastTrigger::CBroadcastTrigger
// Purpose:	Constructor for CBroadcastTrigger
// Input:	value - the value of the trigger relative to other triggers
//				teamValue - the teamValue of the trigger (not used)
//				keys - strings to search for in the text of any broadcast event
//------------------------------------------------------------------------------------------------------
CBroadcastTrigger::CBroadcastTrigger (int value, int teamValue, vector<string>& keys,map<string,string> extras)
:CCustomAwardTrigger(value,teamValue,extras)
{
	//this line works in win32, but not in G++... g++ doesn't seem to have vector::assign
	//broadcastStrings.assign(keys.begin(),keys.end());
	
	//make a new temp object, and assign it to broadcastStrings
	broadcastStrings=vector<string>(keys);
}


//------------------------------------------------------------------------------------------------------
// Function:	CBroadcastTrigger::matches
// Purpose:	 Determines if a given event is a broadcast and  matches any of the keys
// Input:	le - the event we're testing
// Output:	Returns true if the given event triggers this trigger
//------------------------------------------------------------------------------------------------------
bool CBroadcastTrigger::matches(const CLogEvent* le)
{
	if (le->getType() == CLogEvent::NAMED_BROADCAST)// || le->getType() == CLogEvent::ANON_BROADCAST)
	{
		//broadcastID is arg0
		string BroadID=le->getArgument(0)->getStringValue();
		vector<string>::iterator it;

		for (it=broadcastStrings.begin();it!=broadcastStrings.end();++it)
		{
			string s=*it;
			if (BroadID==*it)
				return true;
		}	
	}
	return false;
}


//------------------------------------------------------------------------------------------------------
// Function:	CGoalTrigger::CGoalTrigger
// Purpose:	Constructor for CGoalTrigger
// Input:	value - the value of the trigger relative to other triggers
//				teamValue - the teamValue of the trigger (not used)
//				keys - the names of goals that will cause this trigger to trigger
//------------------------------------------------------------------------------------------------------
CGoalTrigger::CGoalTrigger(int value, int teamValue, vector<string>& keys,map<string,string> extras)
:CCustomAwardTrigger(value,teamValue,extras)
{
	//this line works in win32, but not in G++... g++ doesn't seem to have vector::assign
	//goalNames.assign(keys.begin(),keys.end());
	
	//make a new temp object, and assign it to broadcastStrings
	//does this introduce a memory leak? 

	goalNames=vector<string>(keys);
}

//------------------------------------------------------------------------------------------------------
// Function:	CGoalTrigger::matches
// Purpose:	 Determines if a given event is a goal activation and  matches any of the keys
// Input:	le - the event we're testing
// Output:	Returns true if the given event triggers this trigger
//------------------------------------------------------------------------------------------------------
bool CGoalTrigger::matches(const CLogEvent* le)
{
	
	if (le->getType() == CLogEvent::NAMED_GOAL_ACTIVATE)
	{
		string n=le->getArgument(1)->getStringValue();
		vector<string>::iterator it;

		for (it=goalNames.begin();it!=goalNames.end();++it)
		{
				int diff=strnicmp(n.c_str(),(*it).c_str(),(*it).length());
				if (diff==0)
					return true;
		}
	}
	return false;
}


//------------------------------------------------------------------------------------------------------
// Function:	CFullSearchTrigger::CFullSearchTrigger
// Purpose:	Constructor for CFullSearchTrigger
// Input:	value - the value of the trigger relative to other triggers
//				teamValue - the teamValue of the trigger (not used)
//				ks - the names of FullSearchs that will cause this trigger to trigger
//------------------------------------------------------------------------------------------------------
CFullSearchTrigger::CFullSearchTrigger(int value, int teamValue, vector<string>& ks,map<string,string> extras)
:CCustomAwardTrigger(value,teamValue,extras)
{
	//this line works in win32, but not in G++... g++ doesn't seem to have vector::assign
	//FullSearchNames.assign(keys.begin(),keys.end());
	
	//make a new temp object, and assign it to broadcastStrings
	winnerVar=extraProps["winnervar"];
	
	keys=vector<string>(ks);
}

bool killws(const char*& cs)
{
	bool retval=false;
	while(isspace(*cs))
	{
		retval=true;
		cs++;
	}
	return retval;
}


#include <regex>
int regExprCompare(string sexpr,string scmp)
{
	regex expression(sexpr);
	cmatch what;
	if(query_match(scmp.c_str(), scmp.c_str() + strlen(scmp.c_str()), what, expression))
	{
		//matched!
		return 0;
	}
	else 
		return 1;


}

bool CFullSearchTrigger::compare(string str_msg,string str_key,map<string,string>& varmatches)
{
	const char* msg=str_msg.c_str();
	const char* key=str_key.c_str();
	
	bool match=true;
	char varbuf[100];
	char cmpbuf[100];
	while (1)
	{
		if (!*msg) break;
		if (!*key) break;
		//get a variable.
		if (*key=='%')
		{
			int i=0;
			while(*key && *key!=' ')
			{
				varbuf[i++]=*(key++);
			}
			varbuf[i]=0;
			if (winnerVar=="")
				winnerVar=varbuf;

			killws(msg);
			if (*msg=='\"')
			{
				msg++;
				int i=0;
				while (*msg && *msg!='\"')
				{
					cmpbuf[i++]=*msg++;
				}
				cmpbuf[i]=0;
				msg++; //skip past last "
			}
			else
			{
				int i=0;
				while (*msg!=' ')
				{
					cmpbuf[i++]=*msg++;
				}
				cmpbuf[i]=0;
			}
			
			string matchexpr=extraProps[varbuf];
			if (matchexpr=="")
			{
				//if blank, match any quote delimited string or space delimited word
				
				varmatches.insert(pair<string,string>(varbuf,cmpbuf));
			}
			else if (matchexpr.at(0)!='!' && matchexpr.at(1)!='!')
			{
				//do a normal string compare
				if (stricmp(matchexpr.c_str(),cmpbuf)==0)
					varmatches.insert(pair<string,string>(varbuf,cmpbuf));
				else
					return false;
			}
			else 
			{
				//in tfstats, reg expressions start with !! so skip past that
				const char* rexpr=matchexpr.c_str()+2;
				string test=rexpr;
				if (regExprCompare(rexpr,cmpbuf)==0)
					varmatches.insert(pair<string,string>(varbuf,cmpbuf));
				else
					return false;
			}
				
		
		}

		bool movedptr1 = killws(msg);
		bool movedptr2 = killws(key);

		if (!movedptr1 && !movedptr2)
		{
			if (!*msg) break;
			
			if (*msg!=*key)
			{
				match=false;
				break;
			}
 			msg++;
			key++;
		}
		if (!*msg) break;
	}

	return match;
}


//------------------------------------------------------------------------------------------------------
// Function:	CFullSearchTrigger::matches
// Purpose:	 
// Input:	le - the event we're testing
// Output:	Returns true if the given event triggers this trigger
//------------------------------------------------------------------------------------------------------
bool CFullSearchTrigger::matches(const CLogEvent* le)
{
	//test against full text
	//clear out the state from the last match attempt
	map<string,string> varmatches;
	bool match=compare(le->getFullMessage(),keys[0],varmatches);
	
#ifdef _CUSTOMDEBUG
#ifdef _DEBUG
	if (match)
	{
		map<string,string>::iterator it=varmatches.begin();
		for (it;it!=varmatches.end();++it)
		{
			debug_printf("matched %s with %s\n",it->first.c_str(),it->second.c_str());
		}
	}
#endif
#endif
	return match;
}

#include "pid.h"
PID  CFullSearchTrigger::plrIDFromEvent(const CLogEvent* ple)
{
#ifdef WIN32
	if (winnerVar.compare(0,5,"%plr_")==0)
#else
	if (winnerVar.at(0)=='%' &&
		winnerVar.at(1)=='p' &&
		winnerVar.at(2)=='l' &&
		winnerVar.at(3)=='r' &&
		winnerVar.at(4)=='_')
#endif
	{
		map<string,string> varmatches;
		compare(ple->getFullMessage(),keys[0],varmatches);
		string name=varmatches[winnerVar];
		int svrID=Util::string2svrID(name);
		return pidMap[svrID];
	}
	else
		return -1;
}

	//this class does
string CFullSearchTrigger::getTrackString(const CLogEvent* ple)
{
	map<string,string> varmatches;
	compare(ple->getFullMessage(),keys[0],varmatches);
	return varmatches[winnerVar];
}