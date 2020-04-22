//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface to CAllPlayersStats
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef ALLPLAYERSSTATS_H
#define ALLPLAYERSSTATS_H
#ifdef WIN32
#pragma once
#endif
#pragma warning (disable:  4786)

#include "report.h"
#include <map>
#include <vector>
#include <string>
using namespace std;

//------------------------------------------------------------------------------------------------------
// Purpose: CAllPlayersStats is a whole page report element that reports specific
// data about each player that has played on the server.  Data such as favourite
// weapon, rank, classes played, favourite class, and kills vs deaths.
//------------------------------------------------------------------------------------------------------
class CAllPlayersStats :public CReport
{
private:
	
	void init();
	
	
	public:
		explicit CAllPlayersStats(){init();}
		
		void generate();
		void writeHTML(CHTMLFile& html);
};

#endif // ALLPLAYERSSTATS_H
