//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#pragma warning (disable:4786)
//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose:  Implementation of the TFStatsApplication class. 
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "TFStatsApplication.h"
#include "TFStatsReport.h"
#include "LogEvent.h"
#include "ScoreBoard.h"
#include "WhoKilledWho.h"
#include "memdbg.h"
#include "awards.h"
#include "MatchResults.h"
#include "DialogueReadout.h"
#include "cvars.h"
#include "html.h"
#include "TextFile.h"
#include "CustomAward.h"
#include "PlayerSpecifics.h"
#include "util.h"
#include "AllPlayersStats.h"
#include <errno.h>



CTFStatsApplication* g_pApp; //global!

//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsApplication::printUsage
// Purpose:	 Prints out how to use the program
//------------------------------------------------------------------------------------------------------
void CTFStatsApplication::printUsage()
{
	
	printf("TFStats version %li.%li\n",majorVer,minorVer);
	printf("\nUSAGE:\n");
#ifdef WIN32
	printf("TFstatsRT.exe <log file name> <optional switches>\n");
#else
	printf("TFstats_l <log file name> <optional switches>\n");
#endif
	printf("the available switches are detailed in the documentation. (TFStats.htm): \n");
	printf("---\n");
	printf("After TFStats is done, several new HTML files will be in the current directory,\n");
	printf("point your browser to the index.html in this directory and it will do the rest.\n");
#ifdef WIN32
	printf("(you should just be able to do \"start index.html\")\n");
#endif
	printf("TFStats uses Regex++:\nRegex++ © 1998-9 Dr John Maddock.\n");
	
}

#include "CustomAwardList.h"

//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsApplication::DoAwards
// Purpose:	 This reports all of the awards
// Input:	MatchResultsPage - the page on which to write the awards' HTML
//------------------------------------------------------------------------------------------------------
void CTFStatsApplication::DoAwards(CHTMLFile& MatchResultsPage)
{
	int chresult=os->chdir(g_pApp->outputDirectory.c_str());

	MatchResultsPage.p();
	MatchResultsPage.write("<img src=\"%s/awards.gif\">\n",g_pApp->supportHTTPPath.c_str());
	MatchResultsPage.div("awards");
	
	//do custom awards first, they're usually the important ones for the match
	CCustomAwardList* pCust;
	os->chdir(ruleDirectory.c_str());
	pCust=CCustomAwardList::readCustomAwards(g_pMatchInfo->mapName());
	os->chdir(outputDirectory.c_str());
	if(pCust)
	{
		
		CCustomAwardIterator it;
		for (it=pCust->begin();it!=pCust->end();++it)
		{
			(*it)->report(MatchResultsPage);
		}
	
		delete pCust;
		MatchResultsPage.hr(450,true);
	}
	os->chdir(outputDirectory.c_str());
	
	//do scout award here.
	CSurvivalistAward csrva; csrva.report(MatchResultsPage);

	//sniper award
	CSharpshooterAward cssa; cssa.report(MatchResultsPage);
	
	//soldier award
	CRocketryAward cra; cra.report(MatchResultsPage);
	
	//demoman awards
	CGrenadierAward cga; cga.report(MatchResultsPage);
	CDemolitionsAward cda; cda.report(MatchResultsPage);
	
	//medic awards
	CCureAward cca;cca.report(MatchResultsPage);
	CBiologicalWarfareAward cbwa; cbwa.report(MatchResultsPage);
	
	//HW award
	CAssaultCannonAward caca;caca.report(MatchResultsPage);
	
	//pyro award
	CFlamethrowerAward cfa;cfa.report(MatchResultsPage);
	
	//spy award
	CKnifeAward cka;cka.report(MatchResultsPage);

	//engineer awards
	CBestSentryAward cbsa; cbsa.report(MatchResultsPage); 
	CSentryRebuildAward cba2;cba2.report(MatchResultsPage);
	
	
	MatchResultsPage.hr(450,true);
	
	//non class specific
	CKamikazeAward ckami;ckami.report(MatchResultsPage);
	CTalkativeAward cta;cta.report(MatchResultsPage);
	CTeamKillAward ctka; ctka.report(MatchResultsPage); 
	

	

	MatchResultsPage.enddiv();
}

//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsApplication::DoMatchResults
// Purpose:	This creates the MatchResults page of the report (the main page)
//------------------------------------------------------------------------------------------------------
void CTFStatsApplication::DoMatchResults()
{
	int chresult=os->chdir(g_pApp->outputDirectory.c_str());
	CHTMLFile MatchResultsPage("MatchResults.html","results");
	CMatchResults cmr;
	cmr.report(MatchResultsPage);
	DoAwards(MatchResultsPage);
	
	CScoreboard cs;
	cs.report(MatchResultsPage);

	
	CWhoKilledWho cwkw; cwkw.report(MatchResultsPage);

}	

//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsApplication::parseCmdLineArg
// Purpose:	This parses variables and values out of passed in commandline args
// Input:	in - the full command line argument
//				var - the output buffer in which to place the name of the variable
//				val - the output buffer in which to place the value of the variable
//------------------------------------------------------------------------------------------------------
void CTFStatsApplication::parseCmdLineArg(const char* in, char* var, char* val)
{
	char* pEq=strchr(in,'=');
	if (!pEq || pEq==in)
	{
		var[0]=val[0]=0;
	}
	else
	{	
		*pEq=0;
		strcpy(var,in);
		strcpy(val,pEq+1);
		Util::str2lowercase(var,var);
		Util::str2lowercase(val,val);
		*pEq='=';
	}

}

//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsApplication::ParseCommandLine
// Purpose:	 this parses the commandline into the cmdLineSwitches map. Also 
//	recognizes certain switches and sets variables and creates directories 
//	accordingly to their values
// Input:	argc - count of arguments from commandline
//				argv[] - the arguments
//------------------------------------------------------------------------------------------------------
void CTFStatsApplication::ParseCommandLine(int argc,const char* argv[])
{
	char var[100];
	char val[100];
	
	//first find if we want to display startup info
	bool displayStartupInfo=false;
	for(int i=2;i<argc;i++)
	{
		parseCmdLineArg(argv[i],var,val);
		if (!var[0])
			fatalError("Malformed switch,  required format is <variable>=<value> with no spaces");
		
		cmdLineSwitches[var]=val;
		if (stricmp(var,"displaystartupinfo")==0)
			if (stricmp(val,"yes")==0)
			{
				displayStartupInfo=true;
				break;
			}
	}
	


	if (displayStartupInfo)
	{
		printf("%21s %-21s\n","Command line","Switches");
		printf("-------------------------------------------\n");
	}
	for(i=2;i<argc;i++)
	{
		parseCmdLineArg(argv[i],var,val);
		if (!var[0])
			fatalError("Malformed switch,  required format is <variable>=<value> with no spaces");
	
		if (displayStartupInfo)
			printf("%20s = %-20s\n",var,val);
		cmdLineSwitches[var]=val;
	}


	
	outputDirectory=os->removeDirSlash(cmdLineSwitches["outputdir"]);
	makeAndSaveDirectory(outputDirectory);
	
	ruleDirectory=os->removeDirSlash(cmdLineSwitches["ruledir"]);
	makeAndSaveDirectory(ruleDirectory);
	
	if (cmdLineSwitches["eliminateoldplayers"]=="yes")
	{
		eliminateOldPlayers=true;
		elimDays=atoi(cmdLineSwitches["oldplayercutoff"].c_str());
	}
	else
	{
		eliminateOldPlayers=false;
		elimDays=0;
	}

	
	if (cmdLineSwitches["usesupportdir"]=="yes")
	{
		supportDirectory=os->removeDirSlash(cmdLineSwitches["supportdir"]);
		supportHTTPPath=os->removeDirSlash(cmdLineSwitches["supporthttppath"]);
	}
	else
	{
		supportDirectory=g_pApp->outputDirectory+g_pApp->os->pathSeperator();
		supportDirectory+="support";
		supportHTTPPath="support";
	}
	makeAndSaveDirectory(supportDirectory);
	
	
	if (cmdLineSwitches["persistplayerstats"]=="yes")
	{
		playerDirectory=os->removeDirSlash(cmdLineSwitches["playerdir"]);
		playerHTTPPath=os->removeDirSlash(cmdLineSwitches["playerhttppath"]);
	}
	else
	{
		playerDirectory=g_pApp->outputDirectory+g_pApp->os->pathSeperator();
		playerDirectory+="players";
		playerHTTPPath="players";
	}
	makeAndSaveDirectory(playerDirectory);
	makeDirectory(playerDirectory+"support");
}


//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsApplication::main
// Purpose:	 The REAL main of the program
// Input:	argc - count of arguments from commandline
//				argv[] - the arguments
//------------------------------------------------------------------------------------------------------
void CTFStatsApplication::main(int argc,const char* argv[])
{
	if (argc<=1){printUsage();return;}
	g_pApp=this;
	
	//TODO: move this into OS interface
	TFStats_setNewHandler();
	
	inputDirectory=".";
	makeAndSaveDirectory(inputDirectory);
	
	//this call also sets up various directories to be used
	ParseCommandLine(argc,argv);		
	
	Util::initFriendlyWeaponNameTable();

	//LogFile is read in here.
	//MUST do this before we chdir to the output directory
	os->chdir(inputDirectory.c_str());
	CEventList* plogfile=NULL;
	plogfile=CEventList::readEventList(argv[1]);
	if (!plogfile)
		fatalError("No valid data found in logfile %s\n",argv[1]);
		
	
	os->chdir(outputDirectory.c_str());
	//make match information object
	g_pMatchInfo= new CMatchInfo(plogfile);
	
	CTFStatsReport matchreport;
	
	
	
	matchreport.genImages();
	
	
	matchreport.genJavaScript();
	matchreport.genStyleSheet();
	
	if (cmdLineSwitches["persistplayerstats"]=="yes")
		matchreport.genAllPlayersStyleSheet();

	matchreport.genIndex();
	
	os->chdir(outputDirectory.c_str());
	matchreport.genTopFrame();
	matchreport.genNavFrame();
	
	
	DoMatchResults();

	
	CCVarList ccvl;
	ccvl.makeHTMLPage("cvars.html","Server Settings");
	
	CDialogueReadout cdr;
	cdr.makeHTMLPage("dialogue.html","Dialogue");

	CPlayerSpecifics cps;
	cps.makeHTMLPage("players.html","Players");

	
	if (cmdLineSwitches["persistplayerstats"]=="yes")
	{
		if(!g_pMatchInfo->isLanGame())
		{
			g_pApp->os->chdir(g_pApp->playerDirectory.c_str());
			CAllPlayersStats caps;
			caps.makeHTMLPage("allplayers.html","All Players");
		}
		else
		{
			g_pApp->warning("Lan Game detected, player stats will not be saved");
		}
	}
	printf("TFStats completed successfully\n\n\n");
}

//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsApplication::makeAndSaveDirectory
// Purpose:	 Creates a directory and writes the full directory path back into the
//	passed in string.  In effect, this turns a potentially relative path into an
//	absolute path, and ensures the directory exists.
// Input:	dir - the string that contains the pathname, and upon return will contain
//	the absolute pathname
//------------------------------------------------------------------------------------------------------
void CTFStatsApplication::makeAndSaveDirectory(string& dir)
{
	char startpath[500];
	char fullpath[500];
	os->getcwd(startpath,500);
	if (!os->makeHier(dir))
	{
		fatalError("Failed to make directory \"%s\", aborting. (Reason: %s)",dir.c_str(),strerror(errno));
	}
	os->chdir(dir.c_str());
	os->getcwd(fullpath,500);
	dir=fullpath;
	os->addDirSlash(dir);
	os->chdir(startpath);
}


//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsApplication::makeDirectory
// Purpose:	 Creates a directory.
// Input:	dir - the directory to create
//------------------------------------------------------------------------------------------------------
void CTFStatsApplication::makeDirectory(string& dir)
{
	char startpath[500];
	os->getcwd(startpath,500);
	
	if (!os->makeHier(dir))
	{
		fatalError("Failed to make directory \"%s\". (Reason: %s)",dir.c_str(),strerror(errno));
	}
	os->chdir(startpath);
}


//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsApplication::fatalError
// Purpose:	prints an error message and exits
// Input:	fmt - the format string
//				... - optional arguments
//------------------------------------------------------------------------------------------------------
void CTFStatsApplication::fatalError(char* fmt,...)
{
	va_list v;
	va_start(v,fmt);
	fprintf(stderr,"Fatal Error: ");
	vfprintf(stderr,fmt,v);
	fprintf(stderr,"\n***Aborting. \n");
	exit(-1);
}

//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsApplication::warning
// Purpose:	prints a warning message and returns. (program doesn't exit)
// Input:	fmt - format string
//				... - optional arguments
//------------------------------------------------------------------------------------------------------
void CTFStatsApplication::warning(char* fmt,...)
{
	va_list v;
	va_start(v,fmt);
	fprintf(stderr,"Warning: ");	
	vfprintf(stderr,fmt,v);
	fprintf(stderr,"\n");
}

//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsApplication::getCutoffSeconds
// Purpose:	Turns the number of cutoff days into seconds. Cutoff days are how
//	many days players can be absent from the server and still have their stats
// reported in the all-player stats
// Output:	time_t the number of cutoff seconds
//------------------------------------------------------------------------------------------------------
time_t CTFStatsApplication::getCutoffSeconds()	
{
	return 60*60*24*elimDays;
}