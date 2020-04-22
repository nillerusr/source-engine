//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "MatchResults.h"

void CMatchResults::init()
{
	memset(teams,0,sizeof(team)*MAX_TEAMS);
	memset(loserString,0,STRLEN);
	memset(winnerString,0,STRLEN);
	valid=false;
	draw=false;
	numTeams=0;
}

void CMatchResults::generate()
{
	CEventListIterator it=g_pMatchInfo->eventList()->end();
	--it;
	
	for (it=g_pMatchInfo->eventList()->begin();it!=g_pMatchInfo->eventList()->end();++it)
	{
		
		if ((*it)->getType()==CLogEvent::MATCH_RESULTS_MARKER)
			valid=true;
		
		else if ((*it)->getType()==CLogEvent::MATCH_DRAW)
		{
			draw=true;
		}
		
		else if ((*it)->getType()==CLogEvent::MATCH_VICTOR)
		{
			//winning teams are recited first, then losing teams.  so start in "winning" mode
			bool fWinMode=true;
			
			char eventText[200];
			strcpy(eventText,(*it)->getFullMessage());
			
			char seps[]   = " \n\t";
			char *token;
			
			token = strtok( eventText, seps );
			while( token != NULL )
			{
				if (stricmp(token,"defeated")==0)
					fWinMode=false;
				
				else if (token[0]=='\"')
				{
					//found a team name
					//depending on win/lose mode, assign that team appropriately
					token++; //advance past the first quote
					char* quote2=strchr(token,'\"');
					*quote2='\0';	//null out the second quote;
					
					//get team ID
					int tID=g_pMatchInfo->teamID(token);
					teams[tID].fWinner=fWinMode;
					
					


				}
					//Get next token
				token = strtok( NULL, seps );

			}
		}

		else if ((*it)->getType()==CLogEvent::MATCH_TEAM_RESULTS)
		{
			int team=g_pMatchInfo->teamID((*it)->getArgument(0)->getStringValue());
			teams[team].valid=true;
			teams[team].numplayers=(*it)->getArgument(1)->getFloatValue();
			teams[team].frags=(*it)->getArgument(2)->getFloatValue();
			teams[team].unacc_frags=(*it)->getArgument(3)->getFloatValue();
			teams[team].score=(*it)->getArgument(4)->getFloatValue();

			for (int i=0;i<MAX_TEAMS;i++)
			{
				teams[team].allies[i]= (i==team);	//initially set team to be allied with itself.
			}
			//get allies
			i=5;
			CLogEventArgument const * pArg=(*it)->getArgument(i++);
			while(pArg)
			{
				int ally=pArg->getFloatValue()-1;
				teams[team].allies[ally]=true;
				teams[ally].allies[team]=true;  //one sided alliances don't exist.
				pArg=(*it)->getArgument(i++);
			}
		}
	}

}


char* CMatchResults::getWinnerTeamsString()
{
	bool firstWinner=true;
	for (int i=0;i<MAX_TEAMS;i++)
	{
		if (teams[i].valid && teams[i].fWinner)
		{
			if (!firstWinner)
				strcat(winnerString," and ");
			strcat(winnerString,g_pMatchInfo->teamName(i).c_str());
			firstWinner=false;
		}
	}
	return winnerString;
}

int CMatchResults::getWinnerTeamScore()
{
	if (draw)
		return teams[0].score;
	
	for (int i=0;i<MAX_TEAMS;i++)	
		if (teams[i].valid && teams[i].fWinner)
			return teams[i].score;
		return 0;
}

void CMatchResults::calcRealWinners()
{
	//first find the highest score.
	int maxScoreTeam=0;
	for (int i=0;i<MAX_TEAMS;i++)
	{
		teams[i].fWinner=false;
		if (teams[i].score > teams[maxScoreTeam].score)
			maxScoreTeam=i;
	}

	//mark that team as a winner, then mark all their allies as winners
	teams[maxScoreTeam].fWinner=true;
	for (int j=0;j<MAX_TEAMS;j++)
	{
		if (teams[maxScoreTeam].allies[j])
			teams[j].fWinner=true;
	}
}

char* CMatchResults::getLoserTeamsString()
{
	bool firstLoser=true;
	
	
	for (int i=0;i<MAX_TEAMS;i++)
	{
		if (teams[i].valid && !teams[i].fWinner)
		{
			if (!firstLoser)
				strcat(loserString," and ");
			strcat(loserString,g_pMatchInfo->teamName(i).c_str());
			firstLoser=false;
		}
	}
	return loserString;
}

int CMatchResults::getLoserTeamScore()
{
	if (draw)
		return teams[0].score;
	for (int i=0;i<MAX_TEAMS;i++)	
		if (teams[i].valid && !teams[i].fWinner)
			return teams[i].score;
		return 0;
}

bool CMatchResults::Outnumbered(int WinnerOrLoser)
{
	int losers=0;
	int winners=0;
	for (int i=0;i<MAX_TEAMS;i++)
	{
		if (teams[i].fWinner)
			winners+=teams[i].numplayers;
		else
			losers+=teams[i].numplayers;
	}
	
	if (WinnerOrLoser == WINNER)
		return losers > winners;
	else
		return losers < winners;
}

int CMatchResults::numWinningTeams()
{
	int num=0;
	for (int i=0;i<MAX_TEAMS;i++)
	{
		if (teams[i].fWinner) num++;
	}
	return num;
}
void CMatchResults::writeHTML(CHTMLFile& html)
{
	calcRealWinners(); //deal with logging bug in DLL
	html.write("<div class=headline>\n");
	
	if (!valid)
		html.write("No winner has been determined.");
	else if (!draw)
	{
		bool fOutnumbered=false;
		bool winPlural=numWinningTeams()==1;
		
		html.write("%s %s! They scored %li points to %s's %li\n",getWinnerTeamsString(),winPlural?"wins":"win",getWinnerTeamScore(),getLoserTeamsString(),getLoserTeamScore());
	}
	else
		html.write("The match ends in a draw! <br> All teams scored %li \n",getWinnerTeamScore());
	
	
	html.write("</div>");
	
}

