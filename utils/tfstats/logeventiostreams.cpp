//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if 0
//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Implementation of CLogEvent's C++ IO Stream stuff. this isn't used currently
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#include "LogEvent.h"
#include <string.h>

//none of this is used. I opted for the FILE* implementation instead, this one was giving some weird results, and not working right.
CLogEvent::CLogEvent(istream& is)
:m_EventCode('\0'),m_EventTime(0),m_Valid(false),m_Next(NULL),m_StrippedText(NULL),m_EventType(INVALID)
{
	readEvent(is);
}

void CLogEvent::print(ostream& os)
{
	os << "(" <<m_EventTime<<") Event Type: "<<TypeNames[m_EventType]<<endl;
	os << "Args: ";
	
	for(int i=0;i<m_args.size();i++)
		cout<< "\t"<<m_args[i]->getStringValue()<<endl; 
	
}

void CLogEvent::readEvent(istream& is)
{
	readEventCode(is);
	readEventTime(is);
	readEventMessage(is);
	determineType();

	if(is)
		m_Valid=true;
	else
		m_Valid=false;

}

//note this function assumes you're at the start of a line

void CLogEvent::readEventCode(istream& is)
{
	is>>m_EventCode;
}


void CLogEvent::readEventMessage(istream& is)
{
	char temp[512]={0,0,0,0};
	is.getline(temp,512,'\n');
	
	m_EventMessage=new char[strlen(temp)];
	strcpy(m_EventMessage,temp);

}


void CLogEvent::readEventTime(istream& is)
{

	int month,day,year;
	int hour,minute,second;
//	fscanf(f," %i/%i/%i - %i:%i:%i: ",&month,&day,&year,&hour,&minute,&second);

	is >> month;
	is.ignore();	//'/'
	is >> day;
	is.ignore();	//'/'
	is >> year;
	is.ignore(3);	//' - '
	is >> hour;
	is.ignore();	//':'
	is >> minute;
	is.ignore();	//':'
	is >> second;
	is.ignore();	//':'
		


	tm t;
	t.tm_isdst=0;
	t.tm_hour=hour;
	t.tm_mday=day;
	t.tm_min=minute;
	t.tm_sec=second;
	t.tm_year=year-1900;	//note no y2k prob here, so says the CRT manual
							//this allows values greater than 99, but it
							//just wants the input with 1900 subtracted.
	t.tm_mon=month;

	m_EventTime=mktime(&t);

}

#endif