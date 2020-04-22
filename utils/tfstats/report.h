//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Implementation of CReport
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef REPORT_H
#define REPORT_H
#ifdef WIN32
#pragma once
#pragma warning(disable:4786)
#endif

#include "MatchInfo.h"
#include "HTML.h"


//------------------------------------------------------------------------------------------------------
// Purpose: CReport is the base class for all elements of a report. This includes
// things like scoreboards and awards.
//------------------------------------------------------------------------------------------------------
class CReport
{
protected:
	//every element must have some info about the match to go off of.
	//moved into global pointer.  g_pMatchInfo
	//CMatchInfo* pMatchInfo;
	virtual void init(){}
public:
	//explicit CReport(CMatchInfo* pMInfo):pMatchInfo(pMInfo){}
	explicit CReport(){}
	virtual void writeHTML(CHTMLFile& html){}
	virtual void generate(){}
	
	virtual void makeHTMLPage(char* pageName,char* pageTitle);
	virtual void report(CHTMLFile& html);
	virtual ~CReport(){}
};	


#endif // REPORT_H
