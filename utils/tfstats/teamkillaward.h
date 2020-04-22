//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CTeamKillAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef TEAMKILLAWARD_H
#define TEAMKILLAWARD_H
#ifdef WIN32
#pragma once
#endif
#include "Award.h"
#include <map>


using namespace std;
//------------------------------------------------------------------------------------------------------
// Purpose: CTeamKillAward is an award given to the player who kills more of 
// their teammates than any other player.
//------------------------------------------------------------------------------------------------------
class CTeamKillAward: public CAward
{
protected:
	map<PID,int> numbetrayals;
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CTeamKillAward():CAward("Antisocial"){}
	void getWinner();
};
#endif // TEAMKILLAWARD_H
