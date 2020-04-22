//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CSentryRebuildAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef SENTRYREBUILDAWARD_H
#define SENTRYREBUILDAWARD_H
#ifdef WIN32
#pragma once
#endif
#include "Award.h"
#include <map>


using namespace std;
//------------------------------------------------------------------------------------------------------
// Purpose:  CSentryRebuildAward is given to the engineers who built a sentry
// gun the most times
//------------------------------------------------------------------------------------------------------
class CSentryRebuildAward: public CAward
{
protected:
	map<PID,int> numbuilds;
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CSentryRebuildAward():CAward("Worst Sentry Placement"){}
	void getWinner();
};
#endif // SENTRYREBUILDAWARD_H
