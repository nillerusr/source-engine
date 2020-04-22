//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implemenation of CTFStatsReport
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include <string.h>
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


//------------------------------------------------------------------------------------------------------
// Function:	CTFStatsReport::CTFStatsReport
// Purpose:	CTFStatsReport Constructor
// Input:	pLogFile - the list of events we'll be reporting on
//------------------------------------------------------------------------------------------------------
CTFStatsReport::CTFStatsReport()
{
	init();
	generate();
}

void CTFStatsReport::genJavaScript()
{
	errno=0;
	string jsname(g_pApp->supportDirectory);
	jsname+=g_pApp->os->pathSeperator();
	jsname+="support.js";

	FILE* f=fopen( jsname.c_str(),"wt");
	if (!f)
		g_pApp->fatalError("Can't open output file \"%s\"! (reason: %s)\nPlease make sure that the file does not exist OR\nif the file does exit, make sure it is not read-only",jsname.c_str(),strerror(errno));
	else 
		fprintf(f,javaScriptSource);

		fclose(f);
#ifndef WIN32
		chmod( jsname.c_str(),PERMIT);
#endif

}
void CTFStatsReport::genStyleSheet()
{
	errno=0;
	string cssname(g_pApp->supportDirectory);
	cssname+=g_pApp->os->pathSeperator();
	cssname+="style.css";

	FILE* f=fopen(cssname.c_str(),"wt");
	if (!f)
		g_pApp->fatalError("Can't open output file \"%s\"! (reason: %s)\nPlease make sure that the file does not exist OR\nif the file does exit, make sure it is not read-only",cssname.c_str(),strerror(errno));
	else 
		fprintf(f,styleSheetSource);

	fclose(f);
#ifndef WIN32
		chmod(cssname.c_str(),PERMIT);
#endif
}

void CTFStatsReport::genAllPlayersStyleSheet()
{
	errno=0;
	string cssname(g_pApp->playerDirectory.c_str());
	cssname+=g_pApp->os->pathSeperator();
	cssname+="support";
	cssname+=g_pApp->os->pathSeperator();
	cssname+="style.css";

	FILE* f=fopen(cssname.c_str(),"wt");
	if (!f)
		g_pApp->fatalError("Can't open output file \"%s\"! (reason: %s)\nPlease make sure that the file does not exist OR\nif the file does exit, make sure it is not read-only",cssname.c_str(),strerror(errno));
	else 
		fprintf(f,styleSheetSource);

	fclose(f);
#ifndef WIN32
		chmod(cssname.c_str(),PERMIT);
#endif
}


void CTFStatsReport::genIndex()
{
	CHTMLFile html("index.html","TF Match Report");
	html.write("<script language=\"JavaScript\">\n");
	html.write("<!--\n");
	html.write("function VerifyBrowser()\n");
	html.write("{\n");
	html.write("	if (navigator.appName != \"Microsoft Internet Explorer\" && navigator.appName != \"Netscape\")");
	html.write("		return false;");
	html.write("	version= new Number(navigator.appVersion.charAt(0));");
	html.write("//	alert(version.valueOf());");
	html.write("	if (version.valueOf() >=4 )\n");
	html.write("	{\n");
	html.write("		return true;\n");
	html.write("	}\n");
	html.write("	return false;\n");
	html.write("}\n");
	html.write("\n");
	html.write("if (VerifyBrowser())\n");
	html.write("	top.location = \"index2.html\"\n");
	html.write("else\n");
	html.write("{\n");
	html.write("	document.write(\"<div class=whitetext align = left> Your browser:</div>\");\n");
	html.write("	document.write(\"<div class=whitetext align = left>\", navigator.appName, \"</div>\");\n");
	html.write("	document.write(\"<div class=whitetext align = left>\", navigator.appVersion, \"</div>\");\n");
	html.write("	document.write(\"<div class=whitetext align = left>  is not compatible with TFStats </font></div>\");\n");
	html.write("	document.write(\"<div class=whitetext align = left> TFStats supports IE5, IE4 and Netscape 4</font></div>\");\n");
	html.write("	document.write(\"<div class=whitetext align = left> <a  target=_top href=\\\"http://www.microsoft.com/windows/ie/download/ie5all.htm\\\">Download IE5</a></div>\");\n");
	html.write("	document.write(\"<div class=whitetext align = left> <a  target=_top href=\\\"http://www.netscape.com/computing/download/index.html\\\">Download Netscape 4</a></div>\");\n");
	html.write("}\n");
	html.write("//-->\n");
	html.write("</script>\n");


	CHTMLFile html2("index2.html","TF Match Report",CHTMLFile::dontPrintBody);
	html2.write("<frameset border=0 framespacing=0 frameborder=0 rows=81,*>\n");
	html2.write("<frame src=top.html marginheight=0 marginwidth=0 scrolling=no name=top>\n");
	html2.write("<frameset framespacing=0 frameborder=0 cols=211,*>\n");
	html2.write("<frame src=nav.html marginheight=0 marginwidth=0 scrolling=no name=nav>\n");
	html2.write("<frame src=MatchResults.html marginheight=0 marginwidth=0 scrolling=yes name=data>\n");
	html2.write("</frameset>");
	html2.write("</frameset>");
}

#include <string>
void CTFStatsReport::genImages()
{
	int chresult=g_pApp->os->chdir(g_pApp->supportDirectory.c_str());
	
	 gifAwards.writeOut();
	 jpgBgLeft.writeOut();
	 jpgBgTop.writeOut();
	 gifBoxScore.writeOut();
	 gifGameDialogOff.writeOut();
	 gifGameDialogOn.writeOut();
	 gifMatchStatsOff.writeOut();
	 gifMatchStatsOn.writeOut();
	 gifScores.writeOut();
	 gifServerSettingsOff.writeOut();
	 gifServerSettingsOn.writeOut();
	gifDetailedScores.writeOut();
	gifPlayerStatsMatchOff.writeOut();
	gifPlayerStatsMatchOn.writeOut();
	gifPlayerStatsServerOff.writeOut();
	gifPlayerStatsServerOn.writeOut();



	chresult=g_pApp->os->chdir(g_pApp->outputDirectory.c_str());
}
void CTFStatsReport::genTopFrame()
{
	CHTMLFile html("top.html","top",CHTMLFile::printBody,NULL,0,0);
	html.write("<table width=100%% height=100%% cellpadding=0 cellspacing=0 border=0 background=\"%s/bgtop.jpg\">\n",g_pApp->supportHTTPPath.c_str());
	html.write("\t<tr valign=top>\n");
	html.write("\t\t<td>\n");
	
	html.write("\t\t<div class=server>\n");
	html.write("\t\t<nobr>%s Match Report</nobr>\n",g_pMatchInfo->getServerName().c_str());

	html.write("\t\t</div>\n");
	
	html.write("\t\t<div class=match>\n");
	if(g_pMatchInfo->mapName()!="")
		html.write("\t\tmap: %s ",g_pMatchInfo->mapName().c_str());
	
	char buf[1000];
	html.write(" %s\n",Util::makeDurationString(matchstart,matchend,buf));
	html.write("\t\t</div>\n");
	
	html.write("\t\t</td>\n");
	html.write("\t</tr>\n");
	html.write("</table>\n");
}

inline void writeEventHandlers(CHTMLFile& html, char* id)
{
	html.write(" OnClick=\" clearAll();  %sImgLink.on(); return true; \" ",id,id);
	//html.write(" OnMouseDown=\"imageOn('%s');return true;\" ",id);
	html.write(" OnMouseOver=\" window.status='%s'; %sImgLink.over();return true; \" ",id,id);
	html.write(" OnMouseOut=\" window.status=' ';%sImgLink.out();return true; \" ",id,id);
}
	


void CTFStatsReport::genNavFrame()
{
	string imgname(g_pApp->supportHTTPPath);
	imgname+="/bgleft.jpg";
	CHTMLFile html("nav.html","top",CHTMLFile::printBody,imgname.c_str(),0,0);
	
	string jshttppath(g_pApp->supportHTTPPath);
	jshttppath+="/support.js";
	
	html.write("<script language=\"JavaScript\" src=\"%s\"></script>\n",jshttppath.c_str());
	html.write("<script language=\"JavaScript\">\n");
	html.write("<!--\n");
	html.write("MatchStatsImgLink = new SuperImage(\"MatchStats\",\"%s/match.statistics.on.gif\",\"%s/match.statistics.off.gif\");\n",g_pApp->supportHTTPPath.c_str(),g_pApp->supportHTTPPath.c_str());
	html.write("DialoguePageImgLink = new SuperImage(\"DialoguePage\",\"%s/game.dialog.on.gif\",\"%s/game.dialog.off.gif\");\n",g_pApp->supportHTTPPath.c_str(),g_pApp->supportHTTPPath.c_str());
	html.write("ServerSettingsImgLink = new SuperImage(\"ServerSettings\",\"%s/server.settings.on.gif\",\"%s/server.settings.off.gif\");\n",g_pApp->supportHTTPPath.c_str(),g_pApp->supportHTTPPath.c_str());
	html.write("PlayerStatsMatchImgLink = new SuperImage(\"PlayerStatsMatch\",\"%s/player.statistics.match.on.gif\",\"%s/player.statistics.match.off.gif\");\n",g_pApp->supportHTTPPath.c_str(),g_pApp->supportHTTPPath.c_str());
	html.write("PlayerStatsServerImgLink = new SuperImage(\"PlayerStatsServer\",\"%s/player.statistics.server.on.gif\",\"%s/player.statistics.server.off.gif\");\n",g_pApp->supportHTTPPath.c_str(),g_pApp->supportHTTPPath.c_str());
	html.write("function clearAll()\n");
	html.write("{\n");
	html.write("\tMatchStatsImgLink.off();\n");
	html.write("\tDialoguePageImgLink.off();\n");
	html.write("\tServerSettingsImgLink.off();\n");
	html.write("\tPlayerStatsMatchImgLink.off();\n");
	html.write("\tPlayerStatsServerImgLink.off();\n");
	html.write("}\n");
	html.write("//-->\n");
	html.write("</script>\n");
	html.write("<table width=100%% height=100%% cellpadding=0 cellspacing=0 border=0 >\n");
	html.write("\t<tr valign=top>\n");
	html.write("\t\t<td>\n");
	html.write("\t\t\t<div class=nav>\n");
	html.write("\t\t\t<p><a href=\"MatchResults.html\" target=\"data\"");writeEventHandlers(html,"MatchStats");html.write("><img name=\"MatchStats\"src=\"%s/match.statistics.off.gif\" border=0></a>\n",g_pApp->supportHTTPPath.c_str());
	html.write("\t\t\t<p><a href=\"players.html\" target=\"data\"");writeEventHandlers(html,"PlayerStatsMatch");html.write("><img name=\"PlayerStatsMatch\" src=\"%s/player.statistics.match.off.gif\" border=0></a>\n",g_pApp->supportHTTPPath.c_str());
	html.write("\t\t\t<p><a href=\"dialogue.html\" target=\"data\"");writeEventHandlers(html,"DialoguePage");html.write("><img name=\"DialoguePage\" src=\"%s/game.dialog.off.gif\" border=0></a>\n",g_pApp->supportHTTPPath.c_str());
	
	
	if(g_pApp->cmdLineSwitches["persistplayerstats"]=="yes" && !g_pMatchInfo->isLanGame())
	{
		string s(g_pApp->playerHTTPPath);
		s+="/allplayers.html";
		html.write("\t\t\t<p><a href=\"%s\" target=\"data\"",s.c_str());writeEventHandlers(html,"PlayerStatsServer");html.write("><img name=\"PlayerStatsServer\" src=\"%s/player.statistics.server.off.gif\" border=0></a>\n",g_pApp->supportHTTPPath.c_str());
	}
	html.write("\t\t\t<p><a href=\"cvars.html\" target=\"data\"");writeEventHandlers(html,"ServerSettings");html.write("><img name=\"ServerSettings\" src=\"%s/server.settings.off.gif\" border=0></a>\n",g_pApp->supportHTTPPath.c_str());
	html.write("\t\t\t</div>\n");
	html.write("\t\t</td>\n");
	html.write("\t</tr>\n");
	html.write("</table>\n");

	html.write("<script language=\"JavaScript\">\n");
	html.write("<!--\n");
	html.write("clearAll();\n");
	html.write("MatchStatsImgLink.on();\n");
	html.write("//-->\n");
	html.write("</script>\n");
	
}

CTFStatsReport::~CTFStatsReport()
{
	
}

void CTFStatsReport::init()
{
	matchstart=0;
	matchend=0;
	matchhours=0;
	matchminutes=0;
	matchseconds=0;
}

/*
const char* CTFStatsReport::makeDurationString()
{
	//TODO:
	//handle case where start and end dates are not the same day

	tm* pstart=localtime(&matchstart);
	if (!pstart)
	{
		sprintf(DurationString,"");
		return DurationString;
	}
	
	int sday=pstart->tm_mday;
	
	int sweekday=pstart->tm_wday;
	int smo=pstart->tm_mon;
	int syear=pstart->tm_year+1900;

	int shour=pstart->tm_hour;
	if (pstart->tm_isdst)
		shour=(shour+23)%24; //this substracts 1 while accounting for backing up past 0
	int smin=pstart->tm_min;
	
	tm* pend=localtime(&matchend);
	if (!pend)
		pend=pstart;
	int ehour=pend->tm_hour;
	int emin=pend->tm_min;
	if (pend->tm_isdst)
		ehour=(ehour+23)%24; //this substracts 1 while accounting for backing up past 0

	
	char* matchtz=NULL;
	matchtz=g_pApp->os->gettzname()[0];

	sprintf(DurationString,"%02li:%02li - %02li:%02li %s, %s %s %li%s %li",shour,smin,ehour,emin,matchtz, Util::daysofWeek[sweekday],Util::Months[smo],sday,(sday%10)<4?Util::numberSuffixes[sday%10]:"th",syear);

	return DurationString;
}*/
void CTFStatsReport::generate()
{
	if (g_pMatchInfo->eventList()->empty())
	{
		matchstart=0; 
		matchend=0;
	}
	else
	{
		matchstart=(*(g_pMatchInfo->eventList()->begin()))->getTime();
		CEventListIterator it=g_pMatchInfo->eventList()->end();
		--it;
		matchend=(*it)->getTime();
	}

	time_t matchtime=matchend-matchstart;
	tm* pmatchtime=gmtime(&matchtime);
	matchhours=pmatchtime->tm_hour;
	matchminutes=pmatchtime->tm_min;
	matchseconds=pmatchtime->tm_sec;
	
}


