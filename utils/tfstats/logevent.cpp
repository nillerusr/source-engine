//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of CLogEvent
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "LogEvent.h"
#include "util.h"
#include "memdbg.h"



//For debugging more than anything
const char* CLogEvent::TypeNames[]=
{	
	{"No Type/Invalid!"},
	{"Log File Initialize"},
	{"Server Spawn"},
	{"Server Shutdown"},
	{"Log Closed"},
	{"Server Misc"},
	{"Server Name"},
	{"Team Rename"},
	{"Level Change"},
	{"Cvar Assignment"},
	{"Map CRC"},
	{"Team Join"},
	{"Connect"},
	{"Enter game"},
	{"Disconnect"},
	{"Name Change"},
	{"Frag!"},
	{"Team frag!"},
	{"Suicide!"},
	{"Killed by world!"},
	{"Build"},
	{"Match Results Marker"},
	{"Match Draw"},
	{"Match Victor"},
	{"Match Team Results"},
	{"Talk"},
	{"Team Talk"},
	{"Cure"},
	{"Named Goal Activated"},
	{"Anon Goal Activated"},
	{"Named Broadcast"},
	{"Anon Broadcast"},
	{"Change Class"},
};


//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::getArgument
// Purpose:	returns the iWhichArg'th argument
// Input:	iWhichArg - the desired argument
// Output:	const CLogEventArgument*
//------------------------------------------------------------------------------------------------------
const CLogEventArgument* CLogEvent::getArgument(int iWhichArg) const 
{
	if (iWhichArg < m_args.size())
		return m_args[iWhichArg];
	else
		return NULL;
}


//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::parseArgs
// Purpose:	 extracts the arguments out of the event text.
//------------------------------------------------------------------------------------------------------
void CLogEvent::parseArgs()
{

	char temp[512];
	char* write=temp;
	const char* read=m_EventMessage;

	int i=0;
	while (*read)
	{
		
		if (*read == '\"')	
		{
			//parseArgument moves the read pointer to the char after the closing "
			parseArgument(++read);
			*(write++)='[';
			*(write++)=(char)(i++)+48; //convert int to char by adding 48 
			*(write++)=']';
		}	
		else 
			*write++=*read;

		*read++;
	}
	*write=0;

	Util::str2lowercase(temp,temp);

	m_StrippedText=new TRACKED char[strlen(temp)+1];
	strcpy(m_StrippedText,temp);
}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::parseArgument
// Purpose:	helper function for parseArgs, this actually removes the argument
// Input:	raw - the string from which we want to remove the argument
//------------------------------------------------------------------------------------------------------
void CLogEvent::parseArgument(const char*& raw)
{

	char* atemp;
	if (!(atemp=strchr(raw,'\"')))
		return;

	*atemp=0;	//null out the closing "

	CLogEventArgument* newarg=new CLogEventArgument(raw);
	newarg->init(raw);
	m_args.push_back(newarg);


	*atemp='\"';	//restore it.
	raw=atemp;	//advance the pointer
}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::keywordsOccur
// Purpose:	 tests to see if all of the given keywords occur in the text for this event
// Input:	s1 - first keyword (required)
//				s2 - second keyword (optional)
//				s3 - third keyword (optional)
// Output:	Returns true if the event text contains all of the keywords passed in
//------------------------------------------------------------------------------------------------------
bool CLogEvent::keywordsOccur(char* s1,char* s2,char* s3)
{
	bool result=(strstr(m_StrippedText,s1)!=NULL);
	if (s2)
	{
		result = result && (strstr(m_StrippedText,s2)!=NULL);
		if (s3)
		{
			result = result && (strstr(m_StrippedText,s3)!=NULL);
		}
	}
	return result;
}




//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::determineType
// Purpose:	 this is a big dumb if statement to determine the type of this event
//------------------------------------------------------------------------------------------------------
//this is pretty cheesy
void CLogEvent::determineType()
{
	
	//for now just do this in a big dumb if statement
	
	if (keywordsOccur("killed","self","with"))
		 m_EventType=SUICIDE;
	else if (keywordsOccur("log closed"))
		 m_EventType=LOG_CLOSED;
	else if (keywordsOccur("server name is"))
		 m_EventType=SERVER_NAME;
	else if (keywordsOccur("team name of"))
		 m_EventType=TEAM_RENAME;
	else if (keywordsOccur("killed","by","world"))
		 m_EventType=KILLED_BY_WORLD;
	else if (keywordsOccur("killed","(teammate)"))
		 m_EventType=TEAM_FRAG;
	else if (keywordsOccur("killed","with"))
		 m_EventType=FRAG;
	else if (keywordsOccur("say_team"))
		 m_EventType=SAY_TEAM;
	else if (keywordsOccur("say"))
		 m_EventType=SAY;
	else if (keywordsOccur("joined team"))
		 m_EventType=TEAM_JOIN;
	else if (keywordsOccur("changed to team"))
		 m_EventType=TEAM_JOIN;
	else if (keywordsOccur("log file started"))
		 m_EventType=LOG_FILE_INIT;
	else if (keywordsOccur("spawning server"))
		 m_EventType=SERVER_SPAWN;
	else if (keywordsOccur("connected","address"))
		 m_EventType=CONNECT;
	else if (keywordsOccur("has entered the game"))
		 m_EventType=ENTER_GAME;
	else if (keywordsOccur("disconnected"))
		 m_EventType=DISCONNECT;
	else if (keywordsOccur("changed name to"))
		 m_EventType=NAME_CHANGE;
	else if (keywordsOccur("built"))
		 m_EventType=BUILD;
	else if (keywordsOccur("map crc"))
		 m_EventType=MAP_CRC;
	else if (keywordsOccur("match","results","=------="))
		 m_EventType=MATCH_RESULTS_MARKER;
	else if (keywordsOccur("activated the goal"))
		m_EventType=NAMED_GOAL_ACTIVATE;
	else if (keywordsOccur("goal", "was activated"))
		m_EventType=ANON_GOAL_ACTIVATE;
	else if (keywordsOccur("named broadcast"))
		m_EventType=NAMED_BROADCAST;
	else if (keywordsOccur("broadcast"))
		m_EventType=ANON_BROADCAST;
	else if (keywordsOccur("changed class"))
		m_EventType=CLASS_CHANGE;
	else if (keywordsOccur("-> draw <-"))
		 m_EventType=MATCH_DRAW;
	else if (keywordsOccur("defeated"))
		 m_EventType=MATCH_VICTOR;
	else if (keywordsOccur("results"))
		 m_EventType=MATCH_TEAM_RESULTS;
	else if (keywordsOccur("="))
		 m_EventType=CVAR_ASSIGN;
	else  m_EventType=SERVER_MISC;
}



//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::CLogEvent
// Purpose:	CLogEvent constructor
//------------------------------------------------------------------------------------------------------
CLogEvent::CLogEvent()
:m_EventCode('\0'),m_EventTime(0),m_Valid(false),m_Next(NULL),m_StrippedText(NULL),m_EventType(INVALID),m_EventMessage(NULL)
{}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::~CLogEvent
// Purpose:	CLogEvent destructor
//------------------------------------------------------------------------------------------------------
CLogEvent::~CLogEvent()
{
	//this errors?!
	if (m_EventMessage)
		delete[] m_EventMessage;
	if (m_StrippedText)
		delete[] m_StrippedText;
}



//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::print
// Purpose:	debugging function, prints this event to a file
// Input:	f - the file to print to
//------------------------------------------------------------------------------------------------------
void CLogEvent::print(FILE* f)
{
	fprintf(f,"(%li) Event: %s\n",m_EventTime,m_EventMessage);
}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::CLogEvent
// Purpose:	CLogEvent constructor that reads an event from the specified file
// Input:	f - the file to read from
//------------------------------------------------------------------------------------------------------
CLogEvent::CLogEvent(FILE* f)
:m_EventCode('\0'),m_EventTime(0),m_Valid(false),m_Next(NULL),m_StrippedText(NULL),m_EventType(INVALID),m_EventMessage(NULL)
{
	readEvent(f);
}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::readEvent
// Purpose:	reads an event by reading each part, then checking if it was successful
// Input:	f - the file to read from
//------------------------------------------------------------------------------------------------------
void CLogEvent::readEvent(FILE* f)
{
	m_Valid=true;
	if (m_Valid) readEventCode(f);
	if (m_Valid) readEventTime(f);
	if (m_Valid) readEventMessage(f);
	if (m_Valid) parseArgs();
	if (m_Valid) determineType();
	if (m_Valid) m_Valid=!feof(f);
}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::readEventCode
// Purpose:	reads the event code, the first character on the line (should be 'L')
// Input:	f - the file to read from
//------------------------------------------------------------------------------------------------------
void CLogEvent::readEventCode(FILE* f)
{
	fscanf(f," %c ",&m_EventCode);
	if (m_EventCode!='L')
		m_Valid=false;

	if (feof(f))
		m_Valid=false;
}


//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::readEventMessage
// Purpose:	reads the text of the event message.
// Input:	f - the file to read from
//------------------------------------------------------------------------------------------------------
void CLogEvent::readEventMessage(FILE* f)
{
	char temp[512];
	fgets(temp,512,f);

	//special case hack for broadcasts
	if (strncmp(temp,"Named Broadcast:",16)==0 ||  strncmp(temp,"Broadcast:",16)==0)
	{
		while(1)
		{
			fpos_t temp_pos;
			fgetpos(f,&temp_pos);
			CLogEvent cle(f);
			fseek(f,temp_pos,SEEK_SET);			
			if (cle.isValid())
			{
				//if the next log event is valid, then this broadcast did not span lines
				break;
			}
			else
			{
				temp[strlen(temp)-1]=' ';		//rid ourselves of newline
				temp[strlen(temp)]=0;		//rid ourselves of newline
				char buf[512];
				fgets(buf,512,f);
				strcat(temp,buf);
			}
		}
	}

	if (feof(f))
	{
		m_Valid=false;
	}
	else	
	{
		temp[strlen(temp)-1]=0;		//rid ourselves of newline
		m_EventMessage=new TRACKED char[strlen(temp)+1];
		strcpy(m_EventMessage,temp);
	}

}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEvent::readEventTime
// Purpose:	reads and converts the time the event happened into a time_t
// Input:	f - the file to read from
//------------------------------------------------------------------------------------------------------
void CLogEvent::readEventTime(FILE* f)
{

	int month=-1,day=-1,year=-1;
	int hour=-1,minute=-1,second=-1;
	fscanf(f," %d/%d/%d - %d:%d:%d: ",&month,&day,&year,&hour,&minute,&second);
	if (month==-1 ||day==-1 ||year==-1 ||  hour==-1 || minute==-1 || second==-1)
		m_Valid=false;
	else if (feof(f))
		m_Valid=false;
	else
	{
		tm t;
		t.tm_isdst=0;
		t.tm_hour=hour;
		t.tm_mday=day;
		t.tm_min=minute;
		t.tm_sec=second;
		t.tm_year=year-1900;	//note no y2k prob here, so says the CRT manual
								//this allows values greater than 99, but it
								//just wants the input with 1900 subtracted.
		t.tm_mon=month-1; //jan = 0
		m_EventTime=mktime(&t);

		

	}

}

