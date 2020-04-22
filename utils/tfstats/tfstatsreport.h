//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of CTFStatsReport
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef TFSTATSREPORT_H
#define TFSTATSREPORT_H
#ifdef WIN32
#pragma once
#endif
#ifndef __TFSTATSREPORT_H
#define __TFSTATSREPORT_H
#pragma warning(disable:4786)
#include <time.h>
#include "BinaryResource.h"
#include "MatchInfo.h"
#include "HTML.h"
#include "util.h"	

//------------------------------------------------------------------------------------------------------
// Purpose:  This class is reponsible for generating the top and nav frames of the
//	report. also for generating all the resources such as jpgs, gifs, javascripts and
//	style sheets
//------------------------------------------------------------------------------------------------------
class CTFStatsReport  
{
private:
	time_t matchstart;
	time_t matchend;
	int matchhours;
	int matchminutes;
	int matchseconds;

	bool valid;
	void init();

/*
	const char* makeDurationString();
	char DurationString[300];
*/


	void generate();

	static char* javaScriptSource;
	static char* styleSheetSource;
	//CMatchInfo* pMatchInfo;


	static CBinaryResource gifAwards;
	static CBinaryResource jpgBgLeft;
	static CBinaryResource jpgBgTop;
	static CBinaryResource gifBoxScore;
	static CBinaryResource gifGameDialogOff;
	static CBinaryResource gifGameDialogOn;
	static CBinaryResource gifMatchStatsOff;
	static CBinaryResource gifMatchStatsOn;
	static CBinaryResource gifScores;
	static CBinaryResource gifServerSettingsOff;
	static CBinaryResource gifServerSettingsOn;
	static CBinaryResource gifDetailedScores;
	static CBinaryResource gifPlayerStatsMatchOff;
	static CBinaryResource gifPlayerStatsMatchOn;
	static CBinaryResource gifPlayerStatsServerOff;
	static CBinaryResource gifPlayerStatsServerOn;


	
public:

	explicit CTFStatsReport();
	
	void genImages();
	void genJavaScript();
	void genStyleSheet();
	void genAllPlayersStyleSheet();
	void genIndex();
	void genTopFrame();
	void genNavFrame();

	virtual ~CTFStatsReport();

};

#endif 
#endif // TFSTATSREPORT_H
