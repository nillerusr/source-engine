//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of the TFStatsApplication class. 
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef TFSTATSAPPLICATION_H
#define TFSTATSAPPLICATION_H
#ifdef WIN32
#pragma once
#endif
#include <string>
using std::string;
#include "util.h"
#include "HTML.h"
#include "TFStatsOSInterface.h"

//------------------------------------------------------------------------------------------------------
// Purpose:  Instances of this class contain information that is specific to one run
//of TFStats.  This serves as the main entry point for the program as well. 
//------------------------------------------------------------------------------------------------------
class CTFStatsApplication
{
public:
	CTFStatsOSInterface* os;
	string outputDirectory;
	string inputDirectory;
	string ruleDirectory;
	string supportDirectory;
	string supportHTTPPath;
	string playerDirectory;
	string playerHTTPPath;

	string logFileName;

	bool eliminateOldPlayers;
	
	int elimDays;
	time_t getCutoffSeconds();	
	
	void makeAndSaveDirectory(string& dir);
	void makeDirectory(string& dir);

	//command line switches
	//stored here with the name of the switch as the index
	//and the value of the switch as the data
	std::map<string,string> cmdLineSwitches;
	void parseCmdLineArg(const char* in, char* var, char* val);
	void ParseCommandLine(int argc, const char* argv[]);
	


	void fatalError(PRINTF_FORMAT_STRING char* fmt,...);
	void warning(PRINTF_FORMAT_STRING char* fmt,...);

	void DoAwards(CHTMLFile& MatchResultsPage);
	void DoMatchResults();
	
	void printUsage();
	void main(int argc, const char* argv[]);

	int majorVer;
	int minorVer;
};

extern CTFStatsApplication* g_pApp;
#endif // TFSTATSAPPLICATION_H
