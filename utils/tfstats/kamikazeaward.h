//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CKamikazeAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef KAMIKAZEAWARD_H
#define KAMIKAZEAWARD_H
#ifdef WIN32
#pragma once
#endif
#include "Award.h"
#include <map>

using namespace std;
//------------------------------------------------------------------------------------------------------
// Purpose: CKamikazeAward is an award given to the player who kills him/herself
// the most often.
//------------------------------------------------------------------------------------------------------
class CKamikazeAward: public CAward
{
protected:
	map<PID,int> numdeaths;
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CKamikazeAward():CAward("Kamikaze"){}
	void getWinner();
};
#endif // KAMIKAZEAWARD_H
