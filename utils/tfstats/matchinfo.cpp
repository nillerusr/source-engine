//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Implemenatation of CMatchInfo
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "MatchInfo.h"

CMatchInfo* g_pMatchInfo=NULL; //global information about the match.

//------------------------------------------------------------------------------------------------------
// Function:	CMatchInfo::generate
// Purpose:	 generates the match info structure from the log file
//------------------------------------------------------------------------------------------------------
void CMatchInfo::generate()
{
	if (plogfile->empty())
		g_pApp->fatalError("No data in log file!\nPlease ensure that you are running TFstats on a valid log file!");

	CEventListIterator it=plogfile->begin();
	logopentime=(*it)->getTime();
	for (it;it!=plogfile->end();++it)
	{
		const CLogEvent* curr=(*it);
		switch(curr->getType())
		{
		case CLogEvent::CONNECT:
			{
				int sid=curr->getArgument(0)->asPlayerGetSvrPID();
				unsigned long WONid=curr->getArgument(0)->asPlayerGetWONID();
				string plrName=curr->getArgument(0)->asPlayerGetName();
				string ipAddress=curr->getArgument(1)->getStringValue();
				PID pid;
				PID foundpid=-1;
				if (WONid!=-1)
				{
					bLanGame=false;
					pid=pidMap[sid]=WONid;
				}
				else
				{
					bLanGame=true;
				
					CPlayerList::iterator it=players.begin();
					for (it;it!=players.end();++it)
					{
						PID currpid=it->first;
						CPlayer& cp=it->second;
						if (cp.ipAddress==ipAddress)
						{
							foundpid=currpid;
							break;
						}
					}
					if (it==players.end())	//if no ip addresses matched match by name
					{
						it = players.begin();
						for (it;it!=players.end();++it)
						{
							PID currpid=it->first;
							CPlayer& cp=it->second;
							if (cp.aliases.contains(plrName))
							{
								foundpid=currpid;
								break;
							}
						}
					}
				}
				if (foundpid != -1)
				{
					pid=pidMap[sid]=foundpid;
				}
				else
				{
					pid=pidMap[sid]=sid;
				}
				//printf("Checkpoint %lu\n",__LINE__);
				//printf("pid=%lu\n",pid);
				if (players[pid].pid==-1)
					players[pid].pid=pid;

				players[pid].ipAddress=ipAddress;
				players[pid].svrPID=sid;
				players[pid].WONID=WONid;
				//keep the pseudonym list updated
				players[pid].nameFound(curr->getTime(),plrName);

			}
			break;
		case CLogEvent::ENTER_GAME:
			{
				int sid=curr->getArgument(0)->asPlayerGetSvrPID();
				//PID pid=curr->getArgument(0)->asPlayerGetFullPID();
				unsigned long WONid=curr->getArgument(0)->asPlayerGetWONID();
				
				PID pid;
				if (WONid!=-1)
				{
					bLanGame=false;
					pid=pidMap[sid]=WONid;
				}
				else
				{
					bLanGame=true;
					
					//they may have matched based on IP or name.  
					//so check if the player structure pointed to by
					//the sid is valid, if so, don't reassign pid
					pid=pidMap[sid];
					if (players[pid].ipAddress=="")
						pid=pidMap[sid]=sid;
				}
				
				players[pid].svrPID=sid;
				players[pid].WONID=WONid;
				players[pid].pid=pid;
				string nm=curr->getArgument(0)->asPlayerGetName();
				
				//keep the pseudonym list updated
				players[pid].nameFound(curr->getTime(),nm);
			}
			break;
		case CLogEvent::CLASS_CHANGE:
			{
				PID pid=curr->getArgument(0)->asPlayerGetPID();
				time_t changetime=curr->getTime();

				player_class newpc=playerClassNameToClassID(curr->getArgument(1)->getStringValue());
				
				//keep the pseudonym list updated
				players[pid].nameFound(curr->getTime(),curr->getArgument(0)->asPlayerGetName());
				string plrname=curr->getArgument(0)->asPlayerGetName();
				
				players[pid].allclassesplayed.add(changetime,newpc);
				
				int currTeam=players[pid].teams.atTime(changetime);
				players[pid].perteam[currTeam].classesplayed.add(changetime,newpc);

			}
			break;
		case CLogEvent::NAME_CHANGE:
			{
				//keep the pseudonym list updated
				players[curr->getArgument(0)->asPlayerGetPID()].nameFound(curr->getTime(),curr->getArgument(1)->asPlayerGetName());
			}
			break;
		case CLogEvent::SUICIDE:
			{
				PID pid=(*it)->getArgument(0)->asPlayerGetPID();
				int team=players[pid].teams.atTime((*it)->getTime());
				
			//	players[pid].perteam[team].kills++;
				players[pid].perteam[team].deaths++;
				players[pid].perteam[team].suicides++;
				
				//keep the pseudonym list updated
				players[pid].nameFound(curr->getTime(),curr->getArgument(0)->asPlayerGetName());
			}
			break;	
		case CLogEvent::FRAG:
		case CLogEvent::TEAM_FRAG:
			{
				PID killerid=(*it)->getArgument(0)->asPlayerGetPID();
				PID killedid=(*it)->getArgument(1)->asPlayerGetPID();
				int killerTeam=players[killerid].teams.atTime((*it)->getTime());
				int killedTeam=players[killedid].teams.atTime((*it)->getTime());
				
				
				CPlayer& p1=players[killerid];
				CPlayer& p2=players[killedid];
				
				
				if (curr->getType() == CLogEvent::TEAM_FRAG)
				{
					players[killerid].perteam[killerTeam].teamkills++;
					players[killedid].perteam[killedTeam].teamkilled++;
				}
				else if (curr->getType() == CLogEvent::FRAG)
				{
					string weapName=(*it)->getArgument(2)->getStringValue();	

					bool countKill=true;

					//gotta account for timer/infection double kills for medics!
					if (weapName=="infection")
					{
						//test to see if the previous event was a timer from the same player, and a kill, and with the timer.
						CEventListIterator it2=it;
						if ((--it2)!=plogfile->begin())
						{
							if ((*it2)->getType() == CLogEvent::FRAG)
								if ((*it2)->getArgument(2)->getStringValue()=="timer")
									if ((*it2)->getArgument(0)->asPlayerGetPID()==killerid)
										countKill=false;
						}
					}
					if (countKill)
					{
						
						players[killerid].perteam[killerTeam].weaponKills[weapName]++;
						players[killerid].perteam[killerTeam].kills++;
						players[killedid].perteam[killedTeam].deaths++;
					}
				}

				//keep the pseudonym list updated
				players[killerid].nameFound(curr->getTime(),curr->getArgument(0)->asPlayerGetName());
				players[killedid].nameFound(curr->getTime(),curr->getArgument(1)->asPlayerGetName());
	
			}
			break;	
		case CLogEvent::TEAM_JOIN:
			{
				int team=curr->getArgument(1)->getFloatValue();
				team--; //teams are logged as 1-4.  tfstats stores them as 0-3
				PID pid=curr->getArgument(0)->asPlayerGetPID();
				
				
				CPlayer& p=players[pid];
				team_exists[team]=true;
				
				int oldteam=team;
				if(p.teams.anythingAtTime(curr->getTime()-1))
					oldteam=p.teams.atTime(curr->getTime()-1);
				else	//if this is the first team join, count them as in the game
					players[pid].logontime=curr->getTime();

				//keep the pseudonym list updated
				players[pid].nameFound(curr->getTime(),curr->getArgument(0)->asPlayerGetName());
		
				players[pid].teams.add(curr->getTime(),team);
				
				if (p.allclassesplayed.anythingAtTime(curr->getTime()))
				{
					player_class plrcurrclass=players[pid].allclassesplayed.atTime(curr->getTime());
					players[pid].perteam[oldteam].classesplayed.cut(curr->getTime());
					players[pid].perteam[team].classesplayed.add(curr->getTime(),plrcurrclass);
				}
			}
			break;
			
		case CLogEvent::TEAM_RENAME:
			{
				int teamid=curr->getArgument(0)->getFloatValue()-1;
				string tname=curr->getArgument(1)->getStringValue();
				teamnames[teamid]=tname;
			}
			break;
			
		case CLogEvent::SERVER_NAME:
			{
				servername=curr->getArgument(0)->getStringValue();
			}
			break;
		case CLogEvent::SERVER_SPAWN:
			{
				mapname=curr->getArgument(0)->getStringValue();
			}
			break;
		case CLogEvent::DISCONNECT:
			{
				PID pid=curr->getArgument(0)->asPlayerGetPID();
				players[pid].logofftime=curr->getTime();
				players[pid].allclassesplayed.endTime=curr->getTime();
				players[pid].allclassesplayed.cut(curr->getTime());
				players[pid].teams.cut(curr->getTime());
				players[pid].aliases.cut(curr->getTime());
				
				int currTeam=players[pid].teams.atTime(curr->getTime());
				players[pid].perteam[currTeam].classesplayed.cut(curr->getTime());

				//keep the pseudonym list updated
				if (pid!=-1) //sometimes disconnect messages have -1 for the pid
					players[pid].nameFound(curr->getTime(),curr->getArgument(0)->asPlayerGetName());

			}
			break;
		case CLogEvent::NAMED_BROADCAST:
			{
				//keep the pseudonym list updated
				const CLogEventArgument* pArg=curr->getArgument(1);
				PID pid=pArg->asPlayerGetPID();
				players[pid].nameFound(curr->getTime(),curr->getArgument(1)->asPlayerGetName());
			}
			break;
		case CLogEvent::NAMED_GOAL_ACTIVATE:
			{
				//keep the pseudonym list updated
				players[curr->getArgument(0)->asPlayerGetPID()].nameFound(curr->getTime(),curr->getArgument(0)->asPlayerGetName());
			}
			break;
		case CLogEvent::LOG_CLOSED:
			{
				logclosetime=curr->getTime();
			}
			break;
		}
		
#ifdef _DEBUG
#ifdef _PARSEDEBUG
		
		printf("%s:\n",CLogEvent::TypeNames[(int)curr->getType()]);
		fflush(stdout);
		printf("\t%s\n",curr->m_StrippedText);
		fflush(stdout);
		for (int i=0;curr->getArgument(i);i++)
		{
			if (i==0)
				printf("\t\targs: ");
			fflush(stdout);
			printf("\"%s\" ",curr->getArgument(i)->getStringValue());
		}
		printf("\n");
#endif
#endif
	}		
	
	
	if (logclosetime==0  && !plogfile->empty())
	{
		CEventListIterator it=plogfile->end();
		--it;
		logclosetime=(*it)->getTime();
	}
	
	map<PID,CPlayer>::iterator it2;
	for(it2=players.begin();it2!=players.end();++it2)
	{
		CPlayer& p=(*it2).second;
			
		
		if (p.aliases.endTime < logclosetime)
			p.aliases.endTime=logclosetime;
		
		p.name=p.aliases.favourite();
		
		if (p.allclassesplayed.endTime < logclosetime)
			p.allclassesplayed.endTime=logclosetime;

		if (p.teams.endTime < logclosetime)
			p.teams.endTime=logclosetime;

		for (int i=0;i<MAX_TEAMS;i++)
		{
			//if you have no kills you have to play on a team at least 30 seconds to be counted part of it
			//also give a one-suicide grace so they can killthemselves to get onto another team?
			if (p.teams.howLong(i) < 30 && p.perteam[i].kills==0)// && p.perteam[i].deaths < 1) 
					p.teams.remove(i);

			CTimeIndexedList<player_class>* v= &p.perteam[i].classesplayed;
			p.perteam[i].classesplayed.endTime=logclosetime;
			
			time_t t=p.teams.howLong(i);
			p.perteam[i].timeon=t;
		
		}

	}
}


//------------------------------------------------------------------------------------------------------
// Function:	CMatchInfo::getPlayerID
// Purpose:	 resolves a player name to that players PID
// Input:	name - the name
// Output:	PID the PID
//------------------------------------------------------------------------------------------------------
PID CMatchInfo::getPlayerID(string name)
{
	CPlayerListIterator it;
	
	//ugh! O(n)
	for (it=playerBegin();it!=playerEnd();++it)
	{
		PID id=(*it).first;
		CPlayer curr=(*it).second;
		
		if (curr.name == name)
			return id;
	}
	return -1; 
}

/*
unsigned long CMatchInfo::getPlayerWONID(string name)
{
	CPlayerListIterator it;
	
	//ugh! O(n)
	for (it=playerBegin();it!=playerEnd();++it)
	{
		CPlayer curr=(*it).second;
		
		if (curr.name == name)
			return curr.WONID;
	}
	return 0xffffffff; 
}
*/
//------------------------------------------------------------------------------------------------------
// Function:	CMatchInfo::CMatchInfo
// Purpose:	 Constructor
// Input:	plf - the log file
// Output:	
//------------------------------------------------------------------------------------------------------
CMatchInfo::CMatchInfo(CEventList* plf)
:numPlrs(0),logclosetime(0),plogfile(plf)
{
	teamnames[0]="Blue";
	teamnames[1]="Red";
	teamnames[2]="Yellow";
	teamnames[3]="Green";
	team_exists[0]=team_exists[1]=team_exists[2]=team_exists[3]=false;
	
	
	generate();	
	
}		



//------------------------------------------------------------------------------------------------------
// Function:	CMatchInfo::teamID
// Purpose:	 resolves a team name to its ID
// Input:	teamname - the team name
// Output:	int the ID of the team
//------------------------------------------------------------------------------------------------------
int CMatchInfo::teamID(string teamname)
{
	for (int i=0;i<MAX_TEAMS;i++)
		if (stricmp(teamname.c_str(),teamnames[i].c_str())==0)
			return i;
		return -1;
}


//------------------------------------------------------------------------------------------------------
// Function:	CMatchInfo::getTimeOn
// Purpose:	 returns how long the specified player has been playing
// Input:	pid - the player being queried
// Output:	time_t the time he/she played
//------------------------------------------------------------------------------------------------------
time_t CMatchInfo::getTimeOn(PID pid)
{
	CPlayer& p=players[pid];
	
	if (p.logofftime==0)
		p.logofftime=logclosetime;
	
	time_t timeon=p.logofftime-p.logontime;
	
	return timeon;
}
