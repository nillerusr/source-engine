//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CLogEventArgument
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef ARGUMENT_H
#define ARGUMENT_H
#ifdef WIN32
#pragma once
#endif

#include "pid.h"
#include <vector>
#include <string>

//------------------------------------------------------------------------------------------------------
// Purpose: CLogEventArgument represents a variable in the text of an event.
// for example, "X" killed "Y" with "Z".  X Y and Z are all represented by seperate
// instances of this class.
//------------------------------------------------------------------------------------------------------
class CLogEventArgument
{
public:
	enum
	{
		INVALID_PID=-1,
		INVALID_WONID=0,
		VALUE_WID=64,
		PLAYER_NAME_WID=VALUE_WID,
		WONID_WID=(10+6),  //<WON:xxxxxxxxxx>
		PLAYERID_WID=4	  // <xx> //only up to 99 players :(
	};

private:
	char* m_ArgText;
	bool m_Valid;
public:
	explicit CLogEventArgument(const char* text);
	CLogEventArgument();
	void init(const char* text);

	//int asPlayerGetID() const;
	int asPlayerGetSvrPID() const;
	unsigned long asPlayerGetWONID() const;
	PID asPlayerGetPID() const;
	char* asPlayerGetName(char* copybuf) const;
	std::string asPlayerGetName() const;
	double getFloatValue() const;
	const char* getStringValue() const;
	char* getStringValue(char* copybuf) const;
	bool isValid() const {return m_Valid;}
};

typedef std::vector<CLogEventArgument*> ArgVector;

#endif // ARGUMENT_H
