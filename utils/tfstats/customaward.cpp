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
// Purpose:  Implementation of CCustomAward
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#include <string.h>
#include "TFStatsApplication.h"
#include "CustomAward.h"
#include "TextFile.h"
#include "memdbg.h"


//------------------------------------------------------------------------------------------------------
// Function:	CCustomAward::extendedinfo
// Purpose:	writes extra info about the award winner, like their score or something.
// The extra info string is defined by the user in the configuration file like the
//rest of a custom award's properties.
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CCustomAward::extendedinfo(CHTMLFile& html)
{
	if (extraInfoMsg.empty())
		return;
	
	char str[500];
	char outputstring[2000]={0};
	
	strcpy(str,extraInfoMsg.c_str());
	
	char delims[]={" \n\t"};
	char* temp=NULL;

	temp=strtok(str,delims);
	while(temp!=NULL)
	{
		char word[500];

		if (strnicmp(temp,"%player",strlen("%player"))==0)
		{
			char* more=&temp[strlen("%player")];
			sprintf(word,"%s%s",winnerName.c_str(),more);
		}
		else if (strnicmp(temp,"%winner",strlen("%winner"))==0)
		{
			char* more=&temp[strlen("%winner")];
			sprintf(word,"%s%s",winnerName.c_str(),more);
		}
		else if (strnicmp(temp,"%score",strlen("%score"))==0)
		{
			char* more=&temp[strlen("%score")];
			if (!namemode)
				sprintf(word,"%li%s",plrscores[winnerID],more);
			else
				sprintf(word,"%li%s",stringscores[winnerName],more);
		}
		else if (strnicmp(temp,"%number",strlen("%number"))==0)
		{
			//right now this is just the score
			char* more=&temp[strlen("%number")];
			if (!namemode)
				sprintf(word,"%li%s",plrnums[winnerID],more);
			else
				sprintf(word,"%li%s",stringscores[winnerName],more);
		}
		else
			strcpy(word,temp);

		strcat(outputstring," ");
		strcat(outputstring,word);
		
		temp=strtok(NULL,delims);
	}

	html.write(outputstring);
}


//------------------------------------------------------------------------------------------------------
// Function:	CCustomAward::noWinner
// Purpose:	 writes some html saying that no one won this award. The noWinnerMsg
// is defined by the user in the configuration file like all other custom 
// award properties
// Input:	html - the html file to write to
//------------------------------------------------------------------------------------------------------
void CCustomAward::noWinner(CHTMLFile& html)
{
	if (noWinnerMsg.empty())
		return;

	html.write(noWinnerMsg.c_str());}


//------------------------------------------------------------------------------------------------------
// Function:	CCustomAward::readCustomAward
// Purpose:	 Factory method to read an award from a config file and return an 
// instance of the CCustomAward class
// Input:	f - the configuration file to read from
//				g_pMatchInfo - a pointer to a matchinfo object to give to the new award
// Output:	CCustomAward*
//------------------------------------------------------------------------------------------------------
CCustomAward* CCustomAward::readCustomAward(CTextFile& f)
{
	
	const char* token=f.getToken();
	while (token)
	{
		
		if (!stricmp(token,"Award"))
			break;
		else if (!stricmp(token,"{"))
			f.discardBlock();
		
		token=f.getToken();
	}
	if (!token)
		return NULL;

	f.discard("{");
	token=f.getToken();
	
	CCustomAward* pCustAward=new TRACKED CCustomAward(g_pMatchInfo);
	while (token)	
	{
		if (stricmp(token,"trigger")==0)
		{
			
			CCustomAwardTrigger* ptrig=CCustomAwardTrigger::readTrigger(f);
			pCustAward->triggers.push_back(ptrig);
			
		}
		else if (stricmp(token,"extraInfo")==0)
		{
			f.discard("=");
			pCustAward->extraInfoMsg=f.readString();
			f.discard(";");
		}
		else if (stricmp(token,"noWinnerMessage")==0)
		{
			f.discard("=");
			pCustAward->noWinnerMsg=f.readString();
			f.discard(";");
		}
		else if (stricmp(token,"name")==0)
		{
			f.discard("=");
			pCustAward->awardName=f.readString();
			f.discard(";");
		}
		else if (stricmp(token,"}")==0)
		{
			break;
		}
		else
			g_pApp->fatalError("Unrecognized Award property name while parsing %s:  \"%s\" is not a property of an Award!",f.fileName().c_str(),token);

		token = f.getToken();
	}

	
	return pCustAward;
}


//------------------------------------------------------------------------------------------------------
// Function:	CCustomAward::getWinner
// Purpose:	generates the winner of this custom award.
//------------------------------------------------------------------------------------------------------
void CCustomAward::getWinner()
{
	fNoWinner=true;
	
	CEventListIterator it;
	for (it=g_pMatchInfo->eventList()->begin();it!=g_pMatchInfo->eventList()->end();it++)
	{
		list<CCustomAwardTrigger*>::iterator tli;
		for (tli=triggers.begin();tli!=triggers.end();++tli)
		{
			if ((*tli)->matches(*it))
			{
				//increase the players count by X score.
				//scan for best at the end
				
				//different triggers have the player name/id placed differently. :(
				//if this returns -1, store based on name. (this way we can give awards to things other than player names
				//while remaining in this award/trigger hierarchy structure)
				PID ID =(*tli)->plrIDFromEvent(*it);
				if (ID==-1)
				{
					string ws=(*tli)->getTrackString(*it);
					stringscores[ws]+=(*tli)->plrValue;
					stringnums[ws]++;
					fNoWinner=false;
					namemode=true;
				}
				else
				{
					plrscores[ID]+=(*tli)->plrValue;
					plrnums[ID]++;
					fNoWinner=false;
					namemode=false;
				}
			}
		}
	}

	if (fNoWinner)
		return;


	if (!namemode)
	{
		//now scan and find highest score.
		map<PID,int>::iterator scores_it;
		scores_it=plrscores.begin();
		winnerID=(*scores_it).first;
		int winnerScore=(*scores_it).second;

		for (scores_it=plrscores.begin();scores_it!=plrscores.end();++scores_it)
		{
			int ID=(*scores_it).first;
			int score=(*scores_it).second;
			if (score > winnerScore)
			{
				winnerScore=score;
				winnerID=ID;
			}
		}
	}
	else
	{
		//now scan and find highest score.
		map<string,int>::iterator scores_it;
		scores_it=stringscores.begin();
		winnerID=-1;
		winnerName=(*scores_it).first;
		int winnerScore=(*scores_it).second;

		for (scores_it=stringscores.begin();scores_it!=stringscores.end();++scores_it)
		{
			string name=(*scores_it).first;
			int score=(*scores_it).second;
			if (score > winnerScore)
			{
				winnerScore=score;
				winnerName=name;
			}
		}
	}
}
