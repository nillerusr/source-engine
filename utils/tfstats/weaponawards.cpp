//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of the Weapon Awards
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "WeaponAwards.h"

//------------------------------------------------------------------------------------------------------
// Function:	CWeaponAward::getWinner
// Purpose:	all weapon awards use this to generate their winners
//------------------------------------------------------------------------------------------------------
void CWeaponAward::getWinner()
{
	CEventListIterator it;
	
	for (it=g_pMatchInfo->eventList()->begin(); it != g_pMatchInfo->eventList()->end(); ++it)
	{
		if ((*it)->getType()==CLogEvent::FRAG)
		{
			if (strcmp((*it)->getArgument(2)->getStringValue(),killtype)==0)
			{
				PID pid=(*it)->getArgument(0)->asPlayerGetPID();
				accum[pid]++;
				winnerID=pid;
				fNoWinner=false;
			}
		}
	}
	
	map<PID,int>::iterator acc_it;

	for (acc_it=accum.begin();acc_it!=accum.end();++acc_it)
	{
		int currID=(*acc_it).first;
		if (accum[currID]>accum[winnerID])
			winnerID=currID;
	}
}


//------------------------------------------------------------------------------------------------------
// Function:	CAssaultCannonAward::noWinner
// Purpose:	 writes out html indicating that no one won this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CAssaultCannonAward::noWinner(CHTMLFile& html)
{
	html.write("No one was killed with the Assault Cannon during this match.");
}

//------------------------------------------------------------------------------------------------------
// Function:	CAssaultCannonAward::extendedinfo
// Purpose:	writes out html displaying extra information about the winning of this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CAssaultCannonAward::extendedinfo(CHTMLFile& html)
{
	if (accum[winnerID]==1)
		html.write("%s got 1 kill with the Assault Cannon!",winnerName.c_str());
	else
		html.write("%s got %li kills with the Assault Cannon!",winnerName.c_str(),accum[winnerID]);
}

//------------------------------------------------------------------------------------------------------
// Function:	CFlamethrowerAward::noWinner
// Purpose: writes out html indicating that no one won this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CFlamethrowerAward::noWinner(CHTMLFile& html)
{
	html.write("No one got torched during this match.");
}

//------------------------------------------------------------------------------------------------------
// Function:	CFlamethrowerAward::extendedinfo
// Purpose:	writes out html displaying extra information about the winning of this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CFlamethrowerAward::extendedinfo(CHTMLFile& html)
{
	if (accum[winnerID]==1)
		html.write("%s torched 1 person!",winnerName.c_str());
	else
		html.write("%s torched %li people!",winnerName.c_str(),accum[winnerID]);
}

//------------------------------------------------------------------------------------------------------
// Function:	CKnifeAward::noWinner
// Purpose:	 writes out html indicating that no one won this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CKnifeAward::noWinner(CHTMLFile& html)
{
	html.write("No one got knifed during this match.");
}

//------------------------------------------------------------------------------------------------------
// Function:	CKnifeAward::extendedinfo
// Purpose:	writes out html displaying extra information about the winning of this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CKnifeAward::extendedinfo(CHTMLFile& html)
{
	if (accum[winnerID]==1)
		html.write("%s backstabbed 1 person!",winnerName.c_str());
	else
		html.write("%s backstabbed %li people!",winnerName.c_str(),accum[winnerID]);
}

//------------------------------------------------------------------------------------------------------
// Function:	CRocketryAward::noWinner
// Purpose:	 writes out html indicating that no one won this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CRocketryAward::noWinner(CHTMLFile& html)
{
	html.write("No one was killed by a rocket during this match.");
}

//------------------------------------------------------------------------------------------------------
// Function:	CRocketryAward::extendedinfo
// Purpose:	
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CRocketryAward::extendedinfo(CHTMLFile& html)
{
	if (accum[winnerID]==1)
		html.write("%s rocketed 1 person to oblivion!",winnerName.c_str());
	else
		html.write("%s rocketed %li people to oblivion!",winnerName.c_str(),accum[winnerID]);
}

//------------------------------------------------------------------------------------------------------
// Function:	CGrenadierAward::noWinner
// Purpose:	 writes out html indicating that no one won this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CGrenadierAward::noWinner(CHTMLFile& html)
{
	html.write("No one was killed by a grenade during this match.");
}

//------------------------------------------------------------------------------------------------------
// Function:	CGrenadierAward::extendedinfo
// Purpose:	writes out html displaying extra information about the winning of this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CGrenadierAward::extendedinfo(CHTMLFile& html)
{
	if (accum[winnerID]==1)
		html.write("%s killed 1 person with a grenade!",winnerName.c_str());
	else
		html.write("%s got %li people with grenades!",winnerName.c_str(),accum[winnerID]);
}

//------------------------------------------------------------------------------------------------------
// Function:	CDemolitionsAward::noWinner
// Purpose:	 writes out html indicating that no one won this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CDemolitionsAward::noWinner(CHTMLFile& html)
{
	html.write("No one fell victim to a Detpack this match.");
}

//------------------------------------------------------------------------------------------------------
// Function:	CDemolitionsAward::extendedinfo
// Purpose:	writes out html displaying extra information about the winning of this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CDemolitionsAward::extendedinfo(CHTMLFile& html)
{
	if (accum[winnerID]==1)
		html.write("%s smeared 1 person with a detpack!",winnerName.c_str());
	else
		html.write("%s smeared %li people with detpacks!",winnerName.c_str(),accum[winnerID]);
}

//------------------------------------------------------------------------------------------------------
// Function:	CBiologicalWarfareAward::noWinner
// Purpose:	 writes out html indicating that no one won this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CBiologicalWarfareAward::noWinner(CHTMLFile& html)
{
	html.write("No one died of infection this match.");
}

//------------------------------------------------------------------------------------------------------
// Function:	CBiologicalWarfareAward::extendedinfo
// Purpose:	writes out html displaying extra information about the winning of this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CBiologicalWarfareAward::extendedinfo(CHTMLFile& html)
{
	if (accum[winnerID]==1)
		html.write("%s infected 1 person.",winnerName.c_str());
	else
		html.write("%li people succumbed to %s's disease.",accum[winnerID],winnerName.c_str());
}

//------------------------------------------------------------------------------------------------------
// Function:	CBestSentryAward::noWinner
// Purpose:	 writes out html indicating that no one won this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CBestSentryAward::noWinner(CHTMLFile& html)
{
	html.write("No sentries killed anyone this match.");
}

//------------------------------------------------------------------------------------------------------
// Function:	CBestSentryAward::extendedinfo
// Purpose:	writes out html displaying extra information about the winning of this award
// Input:	html - the html file to output to
//------------------------------------------------------------------------------------------------------
void CBestSentryAward::extendedinfo(CHTMLFile& html)
{
	if (accum[winnerID]==1)
		html.write("%s got 1 person with a sentry gun!",winnerName.c_str());
	else
		html.write("%li people were killed by %s's well placed sentries.",accum[winnerID],winnerName.c_str());
}

