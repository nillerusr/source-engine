//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Implementation of CPlayerReport;
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "util.h"
#include "PlayerReport.h"

#include "PlrPersist.h"

map<unsigned long,bool> CPlayerReport::alreadyPersisted;
map<unsigned long,bool> CPlayerReport::alreadyWroteCombStats;


//------------------------------------------------------------------------------------------------------
// Function:	CPlayerReport::writeHTML
// Purpose:	writes the player's stats out as HTML
// Input:	html - the html file to which we're writing
//------------------------------------------------------------------------------------------------------
void CPlayerReport::writeHTML(CHTMLFile& html)
{
	//if we're writing the stats for a persistent player, just pass execution off to that function!
	if (reportingPersistedPlayer)
	{
		writePersistHTML(html);
		return;
	}

	pPlayer->totalTimeOn(); //this ensures that the logoff time is correct

	if (iWhichTeam==ALL_TEAMS)
		pPlayer->merge();
	else if (!pPlayer->teams.contains(iWhichTeam))
	{
		return;
	}
	int tid=iWhichTeam;

	if (tid==ALL_TEAMS)
		html.write("<font class=headline>%s</font><br>\n",pPlayer->name.c_str());
	else
		html.write("<font class=player%s2>%s</font><hr align=left width=60%%>\n",Util::teamcolormap[tid],pPlayer->name.c_str());

	if (pPlayer->aliases.size() > 1)
	{
		map<string,bool> namePrinted;
		namePrinted[pPlayer->name.c_str()]=true;

		html.write("<font class=whitetext>aliases:</font> <font class=awards2>");
		CTimeIndexedList<string>::iterator nmiter=pPlayer->aliases.begin();
		bool printed1=false;
		for (nmiter;nmiter!=pPlayer->aliases.end();++nmiter)
		{
			if (namePrinted[nmiter->data]!=true)
			{
				if (printed1)
					html.write(", ");
				html.write(nmiter->data.c_str());
				namePrinted[nmiter->data]=true;
				printed1=true;
			}
		}
		html.write("</font><br>\n");
	}

	html.write("<font class=whitetext>rank:</font> <font class=awards2> %.2lf </font><br>\n",pPlayer->perteam[tid].rank());
	html.write("<font class=whitetext>kills/deaths:</font> <font class=awards2>%li/%li </font><br>\n",pPlayer->perteam[tid].kills,pPlayer->perteam[tid].deaths);
	html.write("<font class=whitetext>time:</font> <font class=awards2> %01li:%02li:%02li </font><br>\n",Util::time_t2hours(pPlayer->perteam[tid].timeOn()),Util::time_t2mins(pPlayer->perteam[tid].timeOn()),Util::time_t2secs(pPlayer->perteam[tid].timeOn()));
	

	int numClassesPlayed=pPlayer->perteam[tid].classesplayed.numDifferent();
	player_class faveClass=pPlayer->perteam[tid].classesplayed.favourite();
	
	if (numClassesPlayed == 1)
	{
		if (faveClass!=PC_UNDEFINED)
			html.write("<font class=whitetext>class:</font> <font class=awards2> %s </font><br>\n",plrClassNames[faveClass]);
	}
	else if (numClassesPlayed > 1)
	{
		if (faveClass!=PC_UNDEFINED)
			html.write("<font class=whitetext>favorite class:</font> <font class=awards2> %s </font><br>\n",plrClassNames[faveClass]);

		html.write("<font class=whitetext>classes played:</font> <font class=awards2> ");
		bool printedone=false;
		for(int pc=PC_SCOUT;pc!=PC_OBSERVER;++pc)
		{
			if (pPlayer->perteam[tid].classesplayed.contains((player_class)pc))
			{
				if (printedone)
					html.write(", ");
				
				html.write(plrClassNames[pc]);
				printedone=true;
			}
		}
		html.write(" </font><br>\n");
	}
	


	const string weap=pPlayer->perteam[tid].faveWeapon();
	const string faveWeap=Util::getFriendlyWeaponName(weap);
	if (pPlayer->perteam[tid].kills!=0)
	{
		char lowerWeapName[50];
		Util::str2lowercase(lowerWeapName,faveWeap.c_str());
		html.write("<font class=whitetext>favorite weapon:</font> <font class=awards2> %s</font><br>\n",faveWeap.c_str());
		html.write("<font class=whitetext>kills with %s:</font> <font class=awards2> %li</font><br>\n",lowerWeapName,pPlayer->perteam[tid].faveWeapKills());
	}
	
	
	int numTeamsPlayed=pPlayer->teams.numDifferent();
	
	if (numTeamsPlayed > 1)
	{
		if (iWhichTeam==ALL_TEAMS)
			html.write("<font class=whitetext>Played on</font> <font class=whitetext> ");
		else
			html.write("<font class=whitetext>Also played on</font> <font class=whitetext> ");

		map<int,bool> alreadyPrinted;
		CTimeIndexedList<int>::iterator tmiter=pPlayer->teams.begin();
		bool printed1=false;
		for (tmiter;tmiter!=pPlayer->teams.end();++tmiter)
		{
			int team=tmiter->data;
			if (team != iWhichTeam && !alreadyPrinted[team])
			{
				if (printed1)
				html.write(" and ");
					html.write("<font class=player%s>%s</font>",Util::teamcolormap[team],Util::teamcolormap[team]);//
				printed1=true;
				alreadyPrinted[team]=true;
			}
		}
		html.write("</font><br>\n");
	}

	if (numTeamsPlayed > 1 && iWhichTeam != ALL_TEAMS)
	{
		html.write("<a class=whitetext href=\"%lu.html\"> <u> Combined stats for this match </u> </a> <br> \n",pPlayer->pid);
		
		if (!alreadyWroteCombStats[pPlayer->pid])
		{
			CPlayerReport cpr(pPlayer,ALL_TEAMS);
			char numbuf[200];
			char namebuf[200];
			sprintf(numbuf,"%lu.html",pPlayer->pid);
			sprintf(namebuf,"Combined match statistics for %s",pPlayer->name.c_str());
			cpr.makeHTMLPage(numbuf,namebuf);
			alreadyWroteCombStats[pPlayer->pid]=true;
		}
	}

	if (g_pApp->cmdLineSwitches["persistplayerstats"]=="yes" && !g_pMatchInfo->isLanGame())
		html.write("<a class=whitetext href=\"%s/allplayers.html#%lu\"> <u> Combined stats on this server </u> </a> <br> \n",g_pApp->playerHTTPPath.c_str(),pPlayer->WONID,pPlayer->name.c_str());

	if (g_pMatchInfo->isLanGame())
		return;
	if (alreadyPersisted[pPlayer->WONID] || reportingPersistedPlayer)
		return;
	
	alreadyPersisted[pPlayer->WONID]=true;
	
	if (g_pApp->cmdLineSwitches["persistplayerstats"]=="yes")
	{
		CPlrPersist cpp;
		CPlrPersist onDisk;
		cpp.generate(*pPlayer);
		onDisk.read(pPlayer->WONID);
		cpp.merge(onDisk);
		cpp.write();
	}
	
}

//------------------------------------------------------------------------------------------------------
// Function:	CPlayerReport::writePersistHTML
// Purpose:		writes a persistent player's stats out. these look slightly different
//	than normal players stats
// Input:	html - the html file to write the html to
//------------------------------------------------------------------------------------------------------
void CPlayerReport::writePersistHTML(CHTMLFile& html)
{
	html.write("<a name=\"%lu\">\n",pPersist->WONID);
	html.write("<font class=headline2>%s</font><hr align=left width=60%%>\n",pPersist->faveName().c_str());


	map<string,bool> namePrinted;
	namePrinted[pPersist->faveName()]=true;
	
	map<string,int>::iterator nmiter=pPersist->nickmap.begin();
	bool printed1=false;
	for (nmiter;nmiter!=pPersist->nickmap.end();++nmiter)
	{
		if (namePrinted[nmiter->first]!=true)
		{
			if (!printed1)
				html.write("<font class=whitetext>other names used:</font> <font class=awards2>");
			if (printed1)
				html.write(", ");
			html.write(nmiter->first.c_str());
			namePrinted[nmiter->first]=true;
			printed1=true;
		}
	}
	if (printed1)
		html.write("</font><br>\n");
	

	
	html.write("<font class=whitetext>rank:</font> <font class=awards2> %.2lf </font><br>\n",pPersist->rank());
	html.write("<font class=whitetext>kills/deaths:</font> <font class=awards2>%li/%li </font><br>\n",pPersist->kills,pPersist->deaths);
	html.write("<font class=whitetext>time:</font> <font class=awards2> %01li:%02li:%02li </font><br>\n",Util::time_t2hours(pPersist->timeon),Util::time_t2mins(pPersist->timeon),Util::time_t2secs(pPersist->timeon));
	html.write("<font class=whitetext>matches played:</font> <font class=awards2> %li </font><br>\n",pPersist->matches);


	string faveClass=pPersist->faveClass();
	if (faveClass!="Undefined")
		html.write("<font class=whitetext>favorite class:</font> <font class=awards2> %s </font><br>\n",faveClass.c_str());

	
	bool printedone=false;
	map<string,int>::iterator classit=pPersist->classmap.begin();
	map<string,bool> classPrinted;
	classPrinted[pPersist->faveClass()]=true;
	for(classit;classit!=pPersist->classmap.end();++classit)
	{
		if (classPrinted[classit->first]==false)
		{
			if (!printedone)
				html.write("<font class=whitetext>other classes played:</font> <font class=awards2> ");
			if (printedone)
				html.write(", ");
			html.write(classit->first.c_str());
			classPrinted[classit->first]=true;
			printedone=true;
		}
		
		
	}
	if (printedone)
		html.write(" </font><br>\n");

	const string weap=pPersist->faveWeap();
	const string faveWeap=Util::getFriendlyWeaponName(weap);
	if (pPersist->kills!=0)
	{
		char lowerWeapName[50];
		Util::str2lowercase(lowerWeapName,faveWeap.c_str());
		html.write("<font class=whitetext>favorite weapon:</font> <font class=awards2> %s</font><br>\n",faveWeap.c_str());
		html.write("<font class=whitetext>kills with %s:</font> <font class=awards2> %li</font><br>\n",lowerWeapName,pPersist->faveweapkills);
	}

}