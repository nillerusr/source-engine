//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CSharpshooterAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef SHARPSHOOTERAWARD_H
#define SHARPSHOOTERAWARD_H
#ifdef WIN32
#pragma once
#endif
#include "Award.h"
#include <map>


using namespace std;
//------------------------------------------------------------------------------------------------------
// Purpose: CSharpshooterAward is the award for best sniper.
// It is calculated by finding all of a sniper's kills with his rifle, then totaling them
// up, with headshots being worth three regular shots.
//------------------------------------------------------------------------------------------------------
class CSharpshooterAward: public CAward
{
protected:
	static double HS_VALUE;
	static double SHOT_VALUE;


	map<PID,int> numhs;
	map<PID,int> numshots;
	map<PID,int> sharpshooterscore;
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CSharpshooterAward():CAward("Sharpshooter"){}
	void getWinner();
};
#endif // SHARPSHOOTERAWARD_H
