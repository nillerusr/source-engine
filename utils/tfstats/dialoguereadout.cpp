//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of CDialogueReadout
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "DialogueReadout.h"
#include "util.h"


//------------------------------------------------------------------------------------------------------
// Function:	CDialogueReadout::writeHTML
// Purpose:	generates and writes the report. generate() is not used in this object
// because there is no real intermediate data. Really data is just taken from the 
// event list, massaged abit, and written out to the html file. There is no calculation
// of stats or figures so no intermedidate data creation is needed.
// Input:	html - the html file that we want to write to. 
//------------------------------------------------------------------------------------------------------
void CDialogueReadout::writeHTML(CHTMLFile& html)
{
	html.write("<img src=\"%s/game.dialog.on.gif\">\n",g_pApp->supportHTTPPath.c_str()); 
	html.div("dialog");
	CEventListIterator it;
	bool MM2Messages=false;

	if (g_pApp->cmdLineSwitches["displaymm2"]=="on" || 
		g_pApp->cmdLineSwitches["displaymm2"]=="yes" || 
		g_pApp->cmdLineSwitches["displaymm2"]=="true" || 
		g_pApp->cmdLineSwitches["displaymm2"]=="1")
		MM2Messages=true;

	
	for (it=g_pMatchInfo->eventList()->begin(); it != g_pMatchInfo->eventList()->end(); ++it)
	{
		if ((*it)->getType()==CLogEvent::SAY || (MM2Messages && ((*it)->getType()==CLogEvent::SAY_TEAM)))
		{
			char talked[512]={0};
			PID talkerPID=(*it)->getArgument(0)->asPlayerGetPID();
			string talkerName=(*it)->getArgument(0)->asPlayerGetName();
			
			for (int i=1;(*it)->getArgument(i);i++)
			{
				char temp[512];
				(*it)->getArgument(i)->getStringValue(temp);
				strcat(talked,"\"");
				strcat(talked,temp);
				strcat(talked,"\"");				
			}
			
			bool isTeamMsg= (*it)->getType()==CLogEvent::SAY_TEAM;
			int teamID=g_pMatchInfo->playerList()[talkerPID].teams.atTime((*it)->getTime());
			
			const char* aa;
			const char* bb;

			if (teamID<4 && teamID >= 0)
			{
				aa="player";
				bb=Util::teamcolormap[teamID];
			}
			else
			{
				aa="whitetext";
				bb="";
			}
			
			html.write("<tr><td><font class=%s%s>%s%s:</font><font color=white>  %s</font></tr></td>\n",aa,bb,talkerName.c_str(),isTeamMsg?" (Team)":"",talked);
			html.br();
		}
		else if ( (*it)->getType()==CLogEvent::KILLED_BY_WORLD)
		{
			PID plr=(*it)->getArgument(0)->asPlayerGetPID();
			string plrName=(*it)->getArgument(0)->asPlayerGetName();
			int teamID=g_pMatchInfo->playerList()[plr].teams.atTime((*it)->getTime());
			html.write("<tr><td><font class=player%s>%s</font><font color=white> died.</font></tr></td>\n",Util::teamcolormap[teamID],plrName.c_str());
			html.br();
		}
		else if ((*it)->getType()==CLogEvent::SUICIDE)
		{
			PID plr=(*it)->getArgument(0)->asPlayerGetPID();
			string plrName=(*it)->getArgument(0)->asPlayerGetName();
			int teamID=g_pMatchInfo->playerList()[plr].teams.atTime((*it)->getTime());
			html.write("<tr><td><font class=player%s>%s</font><font color=white> committed suicide.</font></tr></td>\n",Util::teamcolormap[teamID],plrName.c_str());
			html.br();
		}		
		else if ((*it)->getType()==CLogEvent::TEAM_JOIN)
		{
			PID plr=(*it)->getArgument(0)->asPlayerGetPID();
			string plrName=(*it)->getArgument(0)->asPlayerGetName();
			time_t eventtime=(*it)->getTime();
			bool firstJoin=!g_pMatchInfo->playerList()[plr].teams.anythingAtTime(eventtime-1);
			int oldTeamID=g_pMatchInfo->playerList()[plr].teams.atTime(eventtime-1);
			int teamID=g_pMatchInfo->playerList()[plr].teams.atTime(eventtime);
			string teamName=g_pMatchInfo->teamName(teamID);
			if (firstJoin)
				html.write("<tr><td><font class=player%s>%s</font><font color=white> joined team <font class=player%s>%s</font>.</font></tr></td>\n",Util::teamcolormap[teamID],plrName.c_str(),Util::teamcolormap[teamID],teamName.c_str());
			else
				html.write("<tr><td><font class=player%s>%s</font><font color=white> changed teams to <font class=player%s>%s</font>.</font></tr></td>\n",Util::teamcolormap[oldTeamID],plrName.c_str(),Util::teamcolormap[teamID],teamName.c_str());
			html.br();
		}		
		else if ((*it)->getType()==CLogEvent::FRAG)
		{
			PID killer=(*it)->getArgument(0)->asPlayerGetPID();
			string  killerName=(*it)->getArgument(0)->asPlayerGetName();
			PID killee=(*it)->getArgument(1)->asPlayerGetPID();
			string killeeName=(*it)->getArgument(1)->asPlayerGetName();
			string weaponName = (*it)->getArgument(2)->getStringValue();
			
			int killerTeamID=g_pMatchInfo->playerList()[killer].teams.atTime((*it)->getTime());
			int killeeTeamID=g_pMatchInfo->playerList()[killee].teams.atTime((*it)->getTime());
			
			bool countKill=true;

			//gotta account for timer/infection double kills for medics!
			if (weaponName=="infection")
			{
				//test to see if the previous event was a timer from the same player, and a kill, and with the timer.
				CEventListIterator it2=it;
				if ((--it2)!=g_pMatchInfo->eventList()->begin())
				{
					if ((*it2)->getType() == CLogEvent::FRAG)
						if ((*it2)->getArgument(2)->getStringValue()=="timer")
							if ((*it2)->getArgument(0)->asPlayerGetPID()==killer)
								countKill=false;
				}
			}
			if (countKill)
			{
				html.write("<tr><td><font class=player%s>%s</font><font color=white> killed </font><font class=player%s>%s</font><font color=white> with  %s. </font></tr></td>\n",
								Util::teamcolormap[killerTeamID],killerName.c_str(),Util::teamcolormap[killeeTeamID],killeeName.c_str(),weaponName.c_str());
				html.br();
			}
			
		}
		else if ((*it)->getType()==CLogEvent::TEAM_FRAG)
		{
			PID killer=(*it)->getArgument(0)->asPlayerGetPID();
			string  killerName=(*it)->getArgument(0)->asPlayerGetName();
			PID killee=(*it)->getArgument(1)->asPlayerGetPID();
			string killeeName=(*it)->getArgument(1)->asPlayerGetName();
			
			int killerTeamID=g_pMatchInfo->playerList()[killer].teams.atTime((*it)->getTime());
			int killeeTeamID=g_pMatchInfo->playerList()[killee].teams.atTime((*it)->getTime());
			html.write("<tr><td><font class=player%s>%s</font><font color=white> teamkilled </font><font class=player%s>%s.</font></tr></td>\n",
								Util::teamcolormap[killerTeamID],killerName.c_str(),Util::teamcolormap[killeeTeamID],killeeName.c_str());
			html.br();
		}

	}
	
	html.enddiv();
}

