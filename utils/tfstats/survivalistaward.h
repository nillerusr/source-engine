//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CSurvivalistAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef SURVIVALISTAWARD_H
#define SURVIVALISTAWARD_H
#ifdef WIN32
#pragma once
#endif
#include "Award.h"

//------------------------------------------------------------------------------------------------------
// Purpose: CSurvivalistAward is an award given to the player who died the least
// while playing as a scout
//------------------------------------------------------------------------------------------------------
class CSurvivalistAward: public CAward
{
protected:
	map <PID,int> numdeaths;
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CSurvivalistAward():CAward("Survivalist"){}
	void getWinner();
};
#endif // SURVIVALISTAWARD_H
