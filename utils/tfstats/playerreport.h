//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Interface of CPlayerReport
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef PLAYERREPORT_H
#define PLAYERREPORT_H
#ifdef WIN32
#pragma once
#endif
#include "Player.h"
#include "Report.h"
#include "PlrPersist.h"

//------------------------------------------------------------------------------------------------------
// Purpose:  Reports a specific player's stats. 
//------------------------------------------------------------------------------------------------------
class CPlayerReport: public CReport
{
private:
	CPlayer* pPlayer;	
	CPlrPersist* pPersist;
	int iWhichTeam;
	
	static map<unsigned long,bool> alreadyPersisted;
	static map<unsigned long,bool> alreadyWroteCombStats;
	bool reportingPersistedPlayer;

	void writePersistHTML(CHTMLFile& html);
public:
	CPlayerReport(CPlayer* pP,int t):pPlayer(pP),iWhichTeam(t){reportingPersistedPlayer=false;}
	CPlayerReport(CPlrPersist* pPP):pPersist(pPP) {iWhichTeam=-1;reportingPersistedPlayer=true;}

	virtual void writeHTML(CHTMLFile& html);
};


#endif // PLAYERREPORT_H
