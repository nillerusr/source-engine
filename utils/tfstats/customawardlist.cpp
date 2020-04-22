//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of CCustomAwardList
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "CustomAwardList.h"
#include "memdbg.h"

//------------------------------------------------------------------------------------------------------
// Function:	CCustomAwardList::readCustomAwards
// Purpose:	Factory method to read from a file and return a list of custom awards
// Input:	mapname - the name of the map determines the rule file to read the awards from
//				pmi - a pointer to the Match Info which will be passed to each custom award
// Output:	CCustomAwardList*
//------------------------------------------------------------------------------------------------------
CCustomAwardList* CCustomAwardList::readCustomAwards(string mapname)
{
	char filename[255];

	g_pApp->os->chdir(g_pApp->ruleDirectory.c_str());
	sprintf(filename,"tfc.%s.rul",mapname.c_str());
	
	CTextFile ctf1(filename);
	CTextFile ctf2("tfc.rul");

	if (!ctf1.isValid() && ctf2.isValid())
	{
		if (stricmp(filename,"tfc..rul")==0)
			g_pApp->warning("Could not find mapname in the log file, map-specific custom rules will not be used");
		else
			g_pApp->warning("Could not find %s, map-specific custom rules will not be used",filename);
	}
	if (!ctf2.isValid() && ctf1.isValid())
	{
		g_pApp->warning("tfc.rul could not be found.  Only map-specific rules will be used");
	}
	if (!ctf2.isValid() && !ctf1.isValid())
	{
		g_pApp->warning("Neither tfc.rul nor %s could be found. No custom rules will be used");
		return NULL;
	}
	

	CCustomAwardList* newList=new TRACKED CCustomAwardList;
	bool foundAward=false;

	CCustomAward* pcca=CCustomAward::readCustomAward(ctf1);
	while (pcca)
	{
		foundAward=true;
 		newList->theList.push_back(pcca);
		pcca=CCustomAward::readCustomAward(ctf1);
	} 
	
	pcca=CCustomAward::readCustomAward(ctf2);
	while (pcca)
	{
		foundAward=true;
 		newList->theList.push_back(pcca);
		pcca=CCustomAward::readCustomAward(ctf2);
	} 
	
	if (!foundAward)
	{
		delete newList;
		g_pApp->warning("Could not find any custom rules in either tfc.rul or %s. No custom rules will be used.\n",filename);
		newList=NULL;
	}

	return newList;
}
	