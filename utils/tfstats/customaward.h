//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Interface to CCustomAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef CUSTOMAWARD_H
#define CUSTOMAWARD_H
#ifdef WIN32
#pragma once
#endif
#pragma warning(disable :4786)
#include "Award.h"
#include "TextFile.h"
#include "CustomAwardTriggers.h"
#include <list>

using namespace std;
//------------------------------------------------------------------------------------------------------
// Purpose: CCustomAward represents an award that is user-definable via
// a configuration file.  Other than their runtime definitions, Custom awards
// act just like other static awards.
//------------------------------------------------------------------------------------------------------
class CCustomAward: public CAward
{
public: 
	//factory method.
	static CCustomAward* readCustomAward(CTextFile& f);
protected:
	list<CCustomAwardTrigger*> triggers;
	bool namemode;

	map<string,string> extraProps;

	map<PID,int> plrscores; //this is wrt to the current award.  score in this sense is not related to game score
											// but simply a score that is relative to other contenders for the award.
	map<PID,int> plrnums; //this is the number of times any of the triggers was activated

	map<string,int> stringscores; //this is wrt to the current award.  score in this sense is not related to game score
											// but simply a score that is relative to other contenders for the award.
	map<string,int> stringnums; //this is the number of times any of the triggers was activated
	
	string noWinnerMsg;
	string extraInfoMsg;
	virtual void extendedinfo(CHTMLFile& html);
	virtual void noWinner(CHTMLFile& html);

public:
	explicit CCustomAward(CMatchInfo* pmi):CAward("custom_temp"){}

	void getWinner();

	
};

#endif // CUSTOMAWARD_H
