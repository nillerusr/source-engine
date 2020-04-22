//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface to CPlayerSpecifics
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef PLAYERSPECIFICS_H
#define PLAYERSPECIFICS_H
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
// Purpose: CPlayerSpecifics is a whole page report element that reports specific
// data about each player in the game.  Data such as favourite weapon, rank, 
// classes played, favourite class, and kills vs deaths.
//------------------------------------------------------------------------------------------------------
class CPlayerSpecifics :public CReport
{
private:
	
	void init();
	
	
	public:
		explicit CPlayerSpecifics(){init();}
		
		void generate();
		void writeHTML(CHTMLFile& html);
};

#endif // PLAYERSPECIFICS_H
