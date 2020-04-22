//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Implementation of CEventList
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include <stdlib.h>
#include "TFStatsApplication.h"
#include "EventList.h"
#include "memdbg.h"

#pragma warning (disable : 4786)
//------------------------------------------------------------------------------------------------------
// Function:	CEventList::readEventList
// Purpose:	 reads and returns a CEventList from a logfile
// Input:	filename -  the logfile to read from
// Output:	CEventList*
//------------------------------------------------------------------------------------------------------
CEventList* CEventList::readEventList(const char* filename)
{
	
	//	ifstream ifs(filename);
	CEventList* plogfile=new TRACKED CEventList();
	if (!plogfile)
	{
		printf("TFStats ran out of memory!\n");
		return NULL;
	}
	
	
	FILE* f=fopen(filename,"rt");
	if (!f)
		g_pApp->fatalError("Error opening %s, please make sure that the file exists and is not being accessed by other processes",filename);

	while (!feof(f))
	{
		CLogEvent* curr=NULL;
		curr=new TRACKED CLogEvent(f);
		
		if (!curr->isValid())
		{
			delete curr;
			break;//eof reached
		}
		plogfile->insert(plogfile->end(),curr);
	}		
	fclose(f);
#ifndef WIN32
		chmod(filename,PERMIT);
#endif

	return plogfile;
	
}

