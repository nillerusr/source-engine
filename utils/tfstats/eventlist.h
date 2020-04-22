//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CEventList
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef EVENTLIST_H
#define EVENTLIST_H
#ifdef WIN32
#pragma once
#endif
#pragma warning(disable :4786)

#include <list>
#include <map>
#include "LogEvent.h"
#include "util.h"


typedef std::list<const CLogEvent*> event_list;
typedef std::list<const CLogEvent*>::iterator CEventListIterator;


using namespace std;

#include <cstring>
#include <string>

//------------------------------------------------------------------------------------------------------
// Purpose:  CEventList is just a thin wrapper around a list of CLogEvent objects
// It also provides a factory method to read and return a CEventList from a 
// log file
//------------------------------------------------------------------------------------------------------
class CEventList
{
public:
	static CEventList* readEventList(const char* filename);	
	void insert(CEventListIterator cli, const CLogEvent* cle){m_List.insert(cli,cle);}
	CEventListIterator begin(){return m_List.begin();}
	CEventListIterator end(){return m_List.end();}
	bool empty(){return m_List.empty();}
	
private:
	event_list m_List;
	
	
	
};



#endif // EVENTLIST_H
