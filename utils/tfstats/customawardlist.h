//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CCustomAwardList
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef CUSTOMAWARDLIST_H
#define CUSTOMAWARDLIST_H
#ifdef WIN32
#pragma once
#endif
#include "CustomAward.h"
#include <list>

using namespace std;
typedef list<CCustomAward*>::iterator CCustomAwardIterator;
//------------------------------------------------------------------------------------------------------
// Purpose: this is just a thin wrapper around a list of CCustomAward*s
// also provided is a static factory method to read a list of custom awards
// out of a configuration file
//------------------------------------------------------------------------------------------------------
class CCustomAwardList
{
public:
	list<CCustomAward*> theList;

	//factory method
	static CCustomAwardList* readCustomAwards(string mapname);

	CCustomAwardIterator begin(){return theList.begin();}
	CCustomAwardIterator end(){return theList.end();}
};
#endif // CUSTOMAWARDLIST_H
