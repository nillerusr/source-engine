//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Interface of CLogEvent
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef LOGEVENT_H
#define LOGEVENT_H
#ifdef WIN32
#pragma once
#endif
#pragma warning(disable :4786)


#include "Argument.h"

#ifdef WIN32
//#include <strstrea.h>
#else
//#include <strstream.h>
#endif
#include <time.h>
//#include <iostream.h>

#include <stdio.h>



//------------------------------------------------------------------------------------------------------
// Purpose: CLogEvent represents an event in the log file.  e.g. one line.
// It can have one of several types (enumerated below) and a list of arguments
// is attached as well.
//------------------------------------------------------------------------------------------------------
class CLogEvent
{
public:
	enum Type
	{
		NOTYPE =0,
		INVALID = 0,
		LOG_FILE_INIT,
		SERVER_SPAWN,
		SERVER_SHUTDOWN,
		LOG_CLOSED,
		SERVER_MISC,
		SERVER_NAME,
		TEAM_RENAME,
		LEVEL_CHANGE,
		CVAR_ASSIGN,
		MAP_CRC,
		TEAM_JOIN,
		CONNECT,
		ENTER_GAME,
		DISCONNECT,
		NAME_CHANGE,
		FRAG,
		TEAM_FRAG,
		SUICIDE,
		KILLED_BY_WORLD,
		BUILD,
		MATCH_RESULTS_MARKER,
		MATCH_DRAW,
		MATCH_VICTOR,
		MATCH_TEAM_RESULTS,
		SAY,
		SAY_TEAM,
		CURE,
		NAMED_GOAL_ACTIVATE,
		ANON_GOAL_ACTIVATE,
		NAMED_BROADCAST,
		ANON_BROADCAST,
		CLASS_CHANGE,
		NUM_TYPES
	};
	
	char* m_StrippedText;	
private:
	ArgVector m_args;

	char m_EventCode;
	
	time_t m_EventTime;
	
	bool m_Valid;
	char* m_EventMessage;
	Type m_EventType;


	bool keywordsOccur(char* s1,char* s2=NULL,char* s3=NULL);
	void parseArgs();
//	void readEventTime(istream& is);
//	void readEventCode(istream& is);
//	void readEventMessage(istream& is);
	void parseArgument(const char*& raw); //ref to pointer to constant char, gotta love it.
	void determineType();

public:
	CLogEvent* m_Next;

	CLogEvent();
	~CLogEvent();
	bool isValid(){return m_Valid;}
	
//	explicit CLogEvent(istream& is);
//	virtual void readEvent(istream& is);
//	virtual void print(ostream& os);
	
	
	

	CLogEvent::Type getType() const {return m_EventType;}
	time_t getTime() const {return m_EventTime;}
	const CLogEventArgument* getArgument(int i) const;

	const char* getFullMessage() const {return m_EventMessage;}




	static const char* TypeNames[];	

	
	//unused stuff 
protected:	
	void readEventTime(FILE* f);
	void readEventCode(FILE* f);
	void readEventMessage(FILE* f);

public:
	explicit CLogEvent(FILE* f);
	virtual void readEvent(FILE* f);
	virtual void print(FILE* f=stdout);
	
};



#endif // LOGEVENT_H
