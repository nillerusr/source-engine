//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of CLogEventArgument
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "argument.h"
#include "memdbg.h"
using namespace std;

//------------------------------------------------------------------------------------------------------
// Function:	CLogEventArgument::CLogEventArgument
// Purpose:	Constructor that builds the object out of the passed in string of text
// Input:	text - text representing the argument
//------------------------------------------------------------------------------------------------------
CLogEventArgument::CLogEventArgument(const char* text)
{
	init(text);

}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEventArgument::CLogEventArgument
// Purpose:	 Default constructor
//------------------------------------------------------------------------------------------------------
CLogEventArgument::CLogEventArgument()
:m_ArgText(NULL),m_Valid(false)
{}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEventArgument::init
// Purpose:	initializes the argument
// Input:	text - the text representing the argument
//------------------------------------------------------------------------------------------------------
void CLogEventArgument::init(const char* text) 
{

	int len=strlen(text);
	m_ArgText=new TRACKED char[len+1];
	strcpy(m_ArgText,text);
	m_Valid=true;
}

char* findStartOfSvrID(char* cs)
{
	char* read=&cs[strlen(cs)-1];
	while (read != cs)
	{
		if (*read=='<' && *(read+1) != 'W') // if we've found a svrID
			break;
		read--;
	}
	return read;
}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEventArgument::asPlayerGetID
// Purpose:	treats the argument as a player name, and returns the player ID.
// Note: PlayerName args have this form: "name<pid><WON:wonid>"
// Output:	the ID of the player represented by this argument
//------------------------------------------------------------------------------------------------------
int CLogEventArgument::asPlayerGetSvrPID() const
{
	char* read=findStartOfSvrID(m_ArgText);
	if (read==m_ArgText)
		return -1;

	int retval=-1;
	sscanf(read,"<%i>",&retval);
	return retval;
}


/*
PID CLogEventArgument::asPlayerGetPID() const
{
	char* openPID=NULL;
	int svrPID=INVALID_PID;
	if (openPID=strchr(m_ArgText,'<'))
	{
		openPID++;
		sscanf(openPID,"%i",&svrPID);
	}
	unsigned long wonID;
	if (openPID=strstr(m_ArgText,"<WON:"))
	{
		openPID+=5;
		sscanf(openPID,"%li",&wonID);
	}

	
	return PID(svrPID,wonID);
	
}
*/
//------------------------------------------------------------------------------------------------------
// Function:	CLogEventArgument::asPlayerGetName
// Purpose:	treats the argument as a player name, and copies/returns the player name.
// Note: PlayerName args have this form: "name<pid><WONID:wonid>"
// Input:	copybuf - the buffer to copy the name into
// Output:	char* the pointer to the buffer that the name was copied into
//------------------------------------------------------------------------------------------------------

char* CLogEventArgument::asPlayerGetName(char* copybuf) const
{
	char* eon=findStartOfSvrID(m_ArgText);
	bool noPID=(eon==m_ArgText);
	char old=*eon;
	if (!noPID)
		*eon=0;

	strcpy(copybuf,m_ArgText);
	if (!noPID)
		*eon=old;
	return copybuf;
	
}


//------------------------------------------------------------------------------------------------------
// Function:	CLogEventArgument::asPlayerGetName
// Purpose:	an alternate form of the above function that returns the playername 
// as a C++ string, rather than buffercopying it around
// Output:	string: the player's name
//------------------------------------------------------------------------------------------------------
string CLogEventArgument::asPlayerGetName() const
{
	char* eon=findStartOfSvrID(m_ArgText);
	bool noPID=(eon==m_ArgText);
	
	char old=*eon;
	if (!noPID)
		*eon=0;
	

	string s(m_ArgText);
	if (!noPID)
		*eon=old;
	return s;
}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEventArgument::asPlayerGetWONID
// Purpose:	treats the argument as a player name, and returns the player's wonid
// Note: PlayerName args have this form: "name<pid><WON:wonid>"
// Output:	int: the WONID of the player
//------------------------------------------------------------------------------------------------------

unsigned long CLogEventArgument::asPlayerGetWONID() const
{
	char* openPID=NULL;
	unsigned long retval=INVALID_WONID;
	if (openPID=strstr(m_ArgText,"<WON:"))
	{
		openPID+=5; //move past the <WON: string
		sscanf(openPID,"%lu",&retval);
	}

	return retval;
}


unsigned long CLogEventArgument::asPlayerGetPID() const
{
	int svrPID=asPlayerGetSvrPID();
	
	if (pidMap[svrPID]==0 || pidMap[svrPID]==-1)
		pidMap[svrPID]=svrPID;
	return pidMap[svrPID];
}


//------------------------------------------------------------------------------------------------------
// Function:	CLogEventArgument::getFloatValue
// Purpose:	 treats the argument as a floating point value, and returns it
// Output:	double
//------------------------------------------------------------------------------------------------------
double CLogEventArgument::getFloatValue() const
{
	return atof(m_ArgText);
}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEventArgument::getStringValue
// Purpose:	 treats the argument as a string and returns a pointer to the argument
// text itself.  note the pointer is const, so the argument can't be modified by 
// the caller (unless they perform some nefarious casting on the returned pointer)
// Output:	const char*
//------------------------------------------------------------------------------------------------------
const char* CLogEventArgument::getStringValue() const
{
	return m_ArgText;
}

//------------------------------------------------------------------------------------------------------
// Function:	CLogEventArgument::getStringValue
// Purpose:	 an alternate form of the above that copies the string into a caller
// supplied buffer then returns a pointer to that buffer
// Input:	copybuf - the buffer into which the string is to be copied
// Output:	char* the pointer to the buffer that the caller passed in
//------------------------------------------------------------------------------------------------------
char* CLogEventArgument::getStringValue(char* copybuf) const
{
	strcpy(copybuf,m_ArgText);
	return copybuf;
}

