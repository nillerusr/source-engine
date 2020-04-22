//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CTalkativeAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef TALKATIVEAWARD_H
#define TALKATIVEAWARD_H
#ifdef WIN32
#pragma once
#endif
#include "Award.h"
#include <map>
using namespace std;

//------------------------------------------------------------------------------------------------------
// Purpose: CTalkativeAward is an award given to the player who speaks the most
// words. 
//------------------------------------------------------------------------------------------------------
class CTalkativeAward: public CAward
{
protected:
	map<PID,int> numtalks;
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
	
	map<int,string> fulltalktext;
public:
	explicit CTalkativeAward():CAward("Bigmouth"){}
	void getWinner();
};
#endif // TALKATIVEAWARD_H
